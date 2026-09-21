/*
 * Bluethread - Self-developed dual-mode Bluetooth protocol stack
 * Copyright (C) 2026 Wang Fei.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License v3.0 for more details.
 *
 * Commercial closed-source licenses are available upon request.
 */

#include "hci_module.h"
#include "hci_snoop_module.h"
#include "hci_module_control_block.h"
#include "hci_command_maker.h"
#include "hci_interface_factory.h"
#include "l2cap/l2cap_module.h"

#include "framework/log_util.h"
#include "framework/timer_module.h"
#include "framework/framework_manager.h"
#include "framework/module_manager.h"
#include "framework/executable_task.h"
#include "framework/framework_event.h"
#include "../include/hci_defs.h"
#include "../include/data_element.h"
#include "../include/endian_convert.h"

#include <vector>

constexpr static uint32_t s_hci_initialized_time_out = 5000;

namespace bluetooth
{

using namespace framework;

uint16_t hci_module::hci_module_task::s_hci_module_task_type_id = 0u;

/*constexpr*/ static std::vector<uint16_t> s_hci_initialization_sequence{ 
    mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::reset ),
    mk_hci_cmd( hci_cmd_gp::information, hci_cmd_op::read_buffer_size )
};

hci_module::hci_module()
{
    set_name( s_hci_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
    m_hci_control_block = std::make_shared<hci_module_control_block>();

    register_event_handler( hci_event_type::hci_command_complete, "", source_here,
        std::bind( &hci_module::handle_command_completed_event, this, std::placeholders::_1 ) );
    register_event_handler( hci_event_type::hci_command_status, "", source_here,
        std::bind( &hci_module::handle_command_status_event, this, std::placeholders::_1 ) );
}

void hci_module::initialize()
{
    hci_module_task::s_hci_module_task_type_id = framework_manager::get_instance().register_task_type();

    if( m_hci_control_block->m_hci_interface == nullptr )
    {
        hci_interface_factory factory;
        m_hci_control_block->m_hci_interface = factory.create_hci_interface();
    }
}

void hci_module::deinitialize()
{
    std::lock_guard<std::recursive_mutex> locker( m_mutex );
    m_pending_task.clear();
}

void hci_module::handle_task( std::shared_ptr<abstract_task> a_task )
{

    if( a_task->get_task_type() != task_type( hci_module::hci_module_task::s_hci_module_task_type_id ) )
    {
        LogUtilError() << "Task type is not hci_module_task. type = "
            << static_cast<uint16_t>(a_task->get_task_type())
            << ". from: " << a_task->get_position();
        return;
    }

    std::shared_ptr<hci_module::hci_module_task> detail_task;
    detail_task = std::static_pointer_cast<hci_module::hci_module_task >( a_task );
    if( detail_task )
    {
        switch( detail_task->m_hci_task_type )
        {
        case hci_task_type::send_hci_data:
            handle_send_hci_data( detail_task );
            break;
        case hci_task_type::driver_initialized_task:
            LogUtilInfo() << "Driver initialization complete. status: "
                << static_cast< int >( detail_task->m_driver_init_status );
            if( detail_task->m_driver_init_status == abstract_hci_interface::driver_init_status::initialized )
            {
                m_hci_control_block->m_chip_initial_status = init_status::initialized;
                handle_chipset_initialized();
                handle_pending_task();
            }
            break;
        case hci_task_type::hci_data_from_controller:
            handle_data_from_controller_task( detail_task );
            break;
        case hci_task_type::driver_deinitialized_task:
            LogUtilWarning() << "We do not power off the chipset right now.";
            set_power_status( abstract_module::powering_status::power_off );
            break;
        default:
            LogUtilError() << "Task type ignored. type = "
                << static_cast< uint16_t >( detail_task->m_hci_task_type );
            break;
        }
    }
    else
    {
        LogUtilError() << "Unknown task.";
    }
}

void hci_module::handle_event( std::shared_ptr<framework_event> a_hci_data )
{
    auto type = a_hci_data->m_event_type;
    std::shared_ptr<hci_module::hci_module_task> hci_task;
    switch( type )
    {
    case bluetooth::event_type::invlaid_type:
        break;
    case bluetooth::event_type::power_on:
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case bluetooth::event_type::power_off:
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::driver_deinitialized_task;
        break;
    case bluetooth::event_type::power_status_changed:
        break;
    default:
        break;
    }

    if( hci_task )
    {
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( get_name() );
        hci_task->set_position( source_here );
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
    }
}

uint16_t hci_module::register_event_handler
    (
    hci_event_type a_type,
    source_position a_postion,
    std::function<void( std::shared_ptr<hci_data> )> a_handler
    )
{
    return register_event_handler( a_type, "", a_postion, a_handler );
}

uint16_t hci_module::register_event_handler
    (
    hci_event_type a_type,
    std::string a_target_module,
    source_position a_postion,
    std::function<void( std::shared_ptr<hci_data> )> a_handler
    )
{
    event_callback_cb cb;
    cb.m_handler = a_handler;
    cb.m_target_module = a_target_module;
    cb.m_position = a_postion;

    std::lock_guard<std::recursive_mutex > locker( m_mutex );
    ++m_event_handler_count;
    cb.m_callback_id = m_event_handler_count;
    m_event_handlers[a_type].push_back( cb );
    return cb.m_callback_id;
}

void hci_module::register_acl_handler
    (
    std::function<void( std::shared_ptr<hci_data> )> a_handler,
    std::string a_module_name
    )
{
    std::lock_guard<std::recursive_mutex > locker( m_mutex );
    m_acl_handler = a_handler;
    m_acl_handler_module = a_module_name;
}

void hci_module::handle_send_hci_data( std::shared_ptr<hci_module::hci_module_task> a_task )
{
    bool pending_task = false;
    switch(m_hci_control_block->m_chip_initial_status )
    {
    case init_status::deinitialized:
        LogUtilInfo() << "controller driver not ready. we need initial the driver first.";
        initialize_chip();
        pending_task = true;
        break;
    case init_status::initializing:
        LogUtilInfo() << "controller initialization work is on. ignore this task.";
        pending_task = true;
        break;
    case init_status::initialized:
        break;
    case init_status::deinitalizing:
        LogUtilInfo() << "controller deinitialation work is on. ignore this task.";
        pending_task = true;
        break;
    default:
        break;
    }

    if( m_hci_control_block->m_command_credit < 1 )
    {
        pending_task = true;
        LogUtilInfo() << "We're waiting for command credit. So pending this command";
    }

    if( pending_task )
    {
        std::lock_guard<std::recursive_mutex> locker( m_mutex );
        m_hci_control_block->m_pending_data_to_chip.push_back( a_task );
    }
    else
    {
        std::shared_ptr<hci_data> _hci_data = a_task->m_hci_data;

        std::shared_ptr<hci_snoop_module::hci_snoop_write_task> task;
        task = std::make_shared<hci_snoop_module::hci_snoop_write_task>();
        task->m_hci_data = _hci_data;
        task->set_target_module( hci_snoop_module::s_hci_snoop_module_name );
        task->set_source_module( get_name() );
        task->set_position( source_here );

        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

        switch( _hci_data->m_type )
        {
        case uart_hci_type::command_type:
            m_hci_control_block->m_command_credit--;
            m_hci_control_block->m_waiting_reponse_cmds.push_back( a_task );
            break;
        case uart_hci_type::acl_type:
            if( a_task->get_source_module() != l2cap_module::s_l2cap_module_name )
            {
                LogUtilFatal() << "ACL packet is not from l2cap module. source: " << a_task->get_source_module()
                    << ", task from: " << a_task->get_position();
            }
            break;
        default:
            break;
        }

        m_hci_control_block->m_hci_interface->send_data_to_controller( static_cast< uint8_t >( _hci_data->m_type )
            , _hci_data->m_buffer.data(), _hci_data->m_buffer.size() );
    }
}

void hci_module::handle_data_from_controller_task( std::shared_ptr<hci_module_task> a_task )
{
    std::shared_ptr<hci_snoop_module::hci_snoop_write_task> task;
    task = std::make_shared<hci_snoop_module::hci_snoop_write_task>();
    task->m_hci_data = a_task->m_hci_data;
    task->set_target_module( hci_snoop_module::s_hci_snoop_module_name );
    task->set_source_module( get_name() );
    task->set_position( source_here );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    auto power_status = framework_manager::get_instance().get_module_manager().get_power_status();

    std::shared_ptr<hci_data> _hci_data = a_task->m_hci_data;
    switch( _hci_data->m_type )
    {
    case uart_hci_type::event_type:
        handle_event( _hci_data );
        break;
    case uart_hci_type::command_type:
        LogUtilError() << "Received command from controller. Ignore this packet.";
        break;
    case uart_hci_type::acl_type:
    {
        if( !acl_valid( _hci_data ) )
        {
            break;
        }

        std::unique_lock<std::recursive_mutex > locker( m_mutex );
        if( m_acl_handler )
        {
            std::shared_ptr<executable_task> tsk;
            tsk = std::make_shared<executable_task>();
            tsk->set_source_module( get_name() );
            task->set_position( source_here );
            tsk->set_fun( std::bind( m_acl_handler, _hci_data ), m_acl_handler_module );
            framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
        }
    }
        break;
    default:
        LogUtilError() << "Unknown HCI packet.";
        break;
    }
}

void hci_module::handle_event( std::shared_ptr<hci_data> a_hci_data )
{
    if( !event_valid( a_hci_data ) )
    {
        LogUtilError() << "Invalid HCI event.";
        return;
    }

    hci_event_type event_type = static_cast<hci_event_type>( a_hci_data->m_buffer[0] );

    std::unique_lock<std::recursive_mutex > locker( m_mutex );
    if( m_event_handlers.contains( event_type ) )
    {
        std::vector<event_callback_cb> cb = m_event_handlers[event_type];
        locker.unlock();
        for( auto& ele : cb )
        {
            if( ele.m_target_module.empty() )
            {
                ele.m_handler( a_hci_data );
            }
            else
            {
                std::shared_ptr<executable_task> tsk;
                tsk = std::make_shared<executable_task>();
                tsk->set_source_module( get_name() );
                tsk->set_position( ele.m_position );
                tsk->set_fun( std::bind( ele.m_handler, a_hci_data ), ele.m_target_module );
                framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
            }
        }
    }
    else
    {
        LogUtilError() << "no event handler! event: " << std::format("0x{:0>2X}", static_cast<uint16_t>(event_type));
    }
}

void hci_module::handle_command_status_event( std::shared_ptr<hci_data> const& a_hci_data )
{
    handle_cmd_credit_update( a_hci_data );
    uint8_t status = a_hci_data->m_buffer[2];
    if( 0x00 != status )
    {
        uint16_t hci_cmd_raw = le_to_host16( a_hci_data->m_buffer.data() + 4 );
        hci_command hci_cmd = static_cast<hci_command>( hci_cmd_raw );

        for( auto it = m_hci_control_block->m_waiting_reponse_cmds.begin();
            it != m_hci_control_block->m_waiting_reponse_cmds.end(); ++it )
        {
            auto& ele = *it;
            auto cmd = get_command_from_hci( ele->m_hci_data->m_buffer );
            if (hci_cmd == cmd &&
                ele->m_hci_data_handler)
            {
                if (ele->m_handler_module_name.empty())
                {
                    ele->m_hci_data_handler( a_hci_data );
                }
                else
                {
                    std::shared_ptr<executable_task> tsk = std::make_shared<executable_task>();
                    std::function<void()> fun = std::bind( ele->m_hci_data_handler, a_hci_data );
                    tsk->set_fun( std::bind( ele->m_hci_data_handler, a_hci_data ), ele->m_handler_module_name );
                    tsk->set_source_module( get_name() );
                    tsk->set_position( source_here );
                    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
                }
                m_hci_control_block->m_waiting_reponse_cmds.erase( it );
                break;
            }
        }
    }
}

void hci_module::handle_cmd_credit_update( std::shared_ptr<hci_data> const& a_hci_data )
{
    uint8_t credit =  m_cmd_credit_reader.read_command_credit( a_hci_data );
    bool to_handle_pending = false;
    m_hci_control_block->m_command_credit = credit;
    if( credit > 0 )
    {
        handle_pending_task();
    }
}

void hci_module::handle_command_completed_event( std::shared_ptr<hci_data> a_hci_data )
{
    uint8_t event_para_size = a_hci_data->m_buffer[1];
    if( a_hci_data->m_buffer.size() < event_para_size + 2 )
    {
        LogUtilError() << "Event parameters size to small! ignore this event.";
        return;
    }

    uint8_t cmd_cnt_availiable = a_hci_data->m_buffer[2];
    uint16_t hci_cmd_raw = le_to_host16( a_hci_data->m_buffer.data() + 3 );
    hci_command hci_cmd = static_cast<hci_command>( hci_cmd_raw );

    m_hci_control_block->m_command_credit = cmd_cnt_availiable;
    for( auto it = m_hci_control_block->m_waiting_reponse_cmds.begin();
        it != m_hci_control_block->m_waiting_reponse_cmds.end(); ++it )
    {
        auto& ele = *it;
        auto cmd = get_command_from_hci( ele->m_hci_data->m_buffer );
        if( hci_cmd == cmd &&
            ele->m_hci_data_handler )
        {
            if( ele->m_handler_module_name.empty() )
            {
                ele->m_hci_data_handler( a_hci_data );
            }
            else
            {
                std::shared_ptr<executable_task> tsk = std::make_shared<executable_task>();
                std::function<void()> fun = std::bind( ele->m_hci_data_handler, a_hci_data );
                tsk->set_fun( std::bind( ele->m_hci_data_handler, a_hci_data ), ele->m_handler_module_name );
                tsk->set_source_module( get_name() );
                tsk->set_position( source_here );
                framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
            }
            m_hci_control_block->m_waiting_reponse_cmds.erase( it );
            break;
        }
    }

    if( m_hci_control_block->m_command_credit > 0 )
    {
        if( !m_hci_control_block->m_pending_data_to_chip.empty() )
        {
            handle_pending_task();
        }
    }
}

bool hci_module::event_valid( std::shared_ptr<hci_data> const& a_hci_data )
{
    if( a_hci_data->m_buffer.size() < 2 )
    {
        /* Then event header is 2 bytes. So the whole event packet must bt at least 2 bytes*/
        LogUtilError() << "Event packet size to small! ignore this event.";
        return false;
    }

    uint8_t total_size = a_hci_data->m_buffer[1];
    if( a_hci_data->m_buffer.size() < total_size + 2 )
    {
        LogUtilError() << "Event length not equals the total buffer.";
        return false;
    }
    else if( a_hci_data->m_buffer.size() > total_size + 2 )
    {
        LogUtilWarning() << "Event length greater then total buffer.";
        return true;
    }

    return true;
}

bool hci_module::acl_valid( std::shared_ptr<hci_data> const& a_hci_data )
{
    if( a_hci_data->m_buffer.size() < 4 )
    {
        /* Then event header is 2 bytes. So the whole event packet must bt at least 2 bytes*/
        LogUtilError() << "ACL packet size to small! ignore this ACL packet.";
        return false;
    }

    uint16_t total_size = le_to_host16( a_hci_data->m_buffer.data() + 2 );
    if( a_hci_data->m_buffer.size() < total_size + 4 )
    {
        LogUtilError() << "ACL length not equals the total buffer.";
        return false;
    }
    else if( a_hci_data->m_buffer.size() > total_size + 4 )
    {
        LogUtilWarning() << "ACL length greater then total buffer.";
        return true;
    }

    return true;
}

void hci_module::initialize_chip()
{
    m_hci_control_block->m_hci_interface->intialize();
    if (0x00 == m_hci_init_timer)
    {
        auto _module = framework_manager::get_instance()
            .get_module_manager().get_module(abstract_module::s_timer_module_name);
        auto _timer_module = std::static_pointer_cast<timer_module>(_module);
        auto timer_cb = [this](uint32_t, std::string)->bool
            {
                LogUtilFatal() << "Cannot initialize bluetooth chipset, please check!";
                return true;
            };
        m_hci_init_timer = _timer_module->register_once_timer(timer_cb,
            std::chrono::milliseconds(s_hci_initialized_time_out));
    }
}

void hci_module::handle_chipset_initialized()
{
    auto _module = framework_manager::get_instance()
        .get_module_manager().get_module(abstract_module::s_timer_module_name);
    auto _timer_module = std::static_pointer_cast<timer_module>(_module);
    _timer_module->undregister_timer(m_hci_init_timer);
    m_hci_init_timer = 0x00;
}

void hci_module::handle_pending_task()
{
    std::unique_lock<std::recursive_mutex> locker( m_mutex );
    if( !m_pending_task.empty() )
    {
        LogUtilInfo() << "Handle the pending task.";
        auto front_ = m_pending_task.front();
        m_pending_task.erase( m_pending_task.begin() );
        locker.unlock();
        handle_task( front_ );
    }

    if( locker.owns_lock() )
    {
        locker.unlock();
    }

    if( !(m_hci_control_block->m_pending_data_to_chip.empty() ) )
    {
        auto task = m_hci_control_block->m_pending_data_to_chip.front();
        m_hci_control_block->m_pending_data_to_chip.pop_front();

        std::shared_ptr<hci_module::hci_module_task> detail_task;
        detail_task = std::static_pointer_cast< hci_module::hci_module_task >( task );
        if( detail_task )
        {
            handle_send_hci_data( detail_task );
        }
        else
        {
            LogUtilError() << "pending hci data sending task not the right type!";
        }
    }
}

}

