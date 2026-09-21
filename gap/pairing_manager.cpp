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

#include "pairing_manager.h"
#include "framework\log_util.h"
#include "framework\framework_manager.h"
#include "framework\executable_task.h"
#include "framework\general_seq_task_runner_module.h"
#include "framework\timer_module.h"
#include "common\bluetooth_common_event.h"
#include "stack\stack_manager.h"

#include "..\hci\hci_module.h"
#include "..\hci\hci_command_maker.h"
#include "..\common\device_manager.h"
#include "..\common\trusted_devices.h"

#include "bluetooth_address.h"
#include "endian_convert.h"
#include "hci_defs.h"

namespace bluetooth
{

using namespace framework;

void pairing_cb::reset()
{
    m_confirm_value = 0xFFFFFFFF;
    m_remote_device = bluetooth_address::s_empty_address;
    m_remote_io_cap = 0x00;
    m_remote_has_oob = 0x00;
    m_remote_mitm_require = 0x00;
    m_flags.reset();
}

void pairing_cb::set_confirm_value( uint32_t a_confirm_value )
{
    m_confirm_value = a_confirm_value;
    m_flags.set( s_confirm_value_flag );
}

void pairing_cb::set_remote_name( std::u8string a_name )
{
    m_device_name = std::move( a_name );
    m_flags.set( s_name_value_flag );
}

std::tuple<bool, std::u8string> pairing_cb::get_remote_name()
{
    return { m_flags.test( s_name_value_flag ),m_device_name };
}

std::tuple<bool, uint32_t> pairing_cb::get_confirm_value()
{
    return { m_flags.test( s_confirm_value_flag ),m_confirm_value };
}

pairing_manager::pairing_manager()
{

}

void pairing_manager::set_attach_module( std::string const& a_moudle )
{
    m_attached_module = a_moudle;
}

void pairing_manager::initialize()
{
    auto fun = std::bind( &pairing_manager::handle_hci_event, this, std::placeholders::_1 );
    auto mod = framework_manager::get_instance().get_module_manager().get_module( hci_module::s_hci_module_name );
    auto hci_mod = static_pointer_cast< hci_module >( mod );

    if( m_attached_module.empty() )
    {
        LogUtilError() << "Please set a atteched module first.";
        return;
    }

    hci_mod->register_event_handler( hci_event_type::hci_io_capability_response,
        m_attached_module, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_io_capability_request,
        m_attached_module, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_simple_pairing_complete,
        m_attached_module, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_user_confirmation_request,
        m_attached_module, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_link_key_notification,
        m_attached_module, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_link_key_request,
        m_attached_module, source_here, fun );
}

void pairing_manager::set_pairable( bool a_pairable )
{
    LogUtilInfo() << "Set to " << ( a_pairable ? "pairable" : "unpairable" );

    m_pairable = a_pairable;
    std::function<void()> fun;
    fun = [a_pairable]()mutable
    {
        stack_manager::get_instance().get_stack_callback()( service_type::not_specified,
            function_type::pairable_changed, reinterpret_cast< void* >( &a_pairable ) );
    };

    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
    task->set_source_module( m_attached_module );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void pairing_manager::device_name_requested
    (
    bluetooth_address a_address,
    std::u8string a_name
    )
{
    auto it = m_pairing_cbs.begin();
    bool found = false;
    for( ; it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == a_address )
        {
            it->set_remote_name( a_name );
            found = true;
            break;
        }
    }

    if( found )
    {
        notify_pairing_request( a_address );
    }
}

void pairing_manager::accept_ssp_confrim
    (
    bluetooth_address a_address,
    bool a_accept
    )
{
    bool found = false;
    for( auto it = m_pairing_cbs.begin(); it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == a_address )
        {
            found = true;
            break;
        }
    }

    if( !found )
    {
        LogUtilInfo() << "No pairing process about device " << a_address.to_string();
        return;
    }

    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100];
    hci_command cmd_send = a_accept ? hci_command::hci_user_confirmation_request_reply
        : hci_command::hci_user_confirmation_request_negative_reply;
    write_le16( buff, static_cast< uint16_t >( cmd_send ) );
    buff[2] = 6;
    memcpy( buff + 3, a_address.address, bluetooth_address::s_bluetooth_address_size );

    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 10 );

    std::shared_ptr<hci_module::hci_module_task> hci_task;
    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    hci_task->m_hci_data = hci;
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( m_attached_module );
    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

void pairing_manager::handle_hci_event( std::shared_ptr<hci_data> const& a_hci_event )
{
    hci_event_type event_ = static_cast< hci_event_type >( a_hci_event->m_buffer[0] );
    std::shared_ptr<hci_data> hci_send;
    switch( event_ )
    {
    case hci_event_type::hci_io_capability_response:
        hci_send = handle_io_rsp( a_hci_event->m_buffer );
        break;
    case hci_event_type::hci_io_capability_request:
        hci_send = handle_io_req( a_hci_event->m_buffer );
        break;
    case hci_event_type::hci_simple_pairing_complete:
        hci_send = handle_ssp_completed( a_hci_event->m_buffer );
        break;
    case hci_event_type::hci_user_confirmation_request:
        hci_send = handle_user_confirm( a_hci_event->m_buffer );
        break;
    case hci_event_type::hci_link_key_notification:
        hci_send = handle_link_key_notify( a_hci_event->m_buffer );
        break;
    case hci_event_type::hci_link_key_request:
        hci_send = handle_link_key_request( a_hci_event->m_buffer );
        break;
    default:
        break;
    }

    if( hci_send )
    {
        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = hci_send;
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( m_attached_module );
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
    }
}

std::shared_ptr<hci_data> pairing_manager::handle_io_rsp( std::vector<uint8_t> const& a_event )
{
    std::shared_ptr<hci_data> hci_send;
    uint8_t total_size = a_event[1];
    bluetooth_address address;
    memcpy( address.address, a_event.data() + 2, bluetooth_address::s_bluetooth_address_size );
    uint8_t io_cap = a_event[8];
    uint8_t has_oob = a_event[9];
    uint8_t auth_require = a_event[10];

    auto it = m_pairing_cbs.begin();
    for( ; it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == address )
        {
            it->m_remote_has_oob = has_oob;
            it->m_remote_io_cap = io_cap;
            it->m_remote_mitm_require = auth_require;
            break;
        }
    }

    if( it == m_pairing_cbs.end() )
    {
        pairing_cb cb;
        cb.m_remote_has_oob = has_oob;
        cb.m_remote_io_cap = io_cap;
        cb.m_remote_mitm_require = auth_require;
        cb.m_remote_device = address;
        m_pairing_cbs.push_back( cb );
    }

    return hci_send;
}

std::shared_ptr<hci_data> pairing_manager::handle_io_req( std::vector<uint8_t> const& a_event )
{
    std::shared_ptr<hci_data> hci_send;
    bluetooth_address address;
    memcpy( address.address, a_event.data() + 2, bluetooth_address::s_bluetooth_address_size );
    auto it = m_pairing_cbs.begin();
    for( ; it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == address )
        {
            break;
        }
    }

    if( it == m_pairing_cbs.end() )
    {
        pairing_cb cb;
        cb.m_remote_device = address;
        m_pairing_cbs.push_back( cb );
    }

    hci_send = std::make_shared<hci_data>();
    hci_send->m_from_controller = false;
    hci_send->m_type = uart_hci_type::command_type;

    uint8_t buffer[100];

    if( m_pairable )
    {
        write_le16( buffer, static_cast< uint16_t >( hci_command::hci_io_capability_request_reply ) );
        buffer[2] = 9;

        memcpy( buffer + 3, address.address, bluetooth_address::s_bluetooth_address_size );
        buffer[9] = 0x01;
        buffer[10] = 0x00;
        buffer[11] = 0x05;
        hci_send->m_buffer.insert( hci_send->m_buffer.begin(), buffer, buffer + 12 );

        LogUtilInfo() << "Response the io capability request";
    }
    else
    {
        write_le16( buffer, static_cast< uint16_t >( hci_command::hci_io_capability_request_negative_reply ) );
        buffer[2] = 7;

        memcpy( buffer + 3, address.address, bluetooth_address::s_bluetooth_address_size );
        buffer[9] = 0x44;
        hci_send->m_buffer.insert( hci_send->m_buffer.begin(), buffer, buffer + 10 );

        LogUtilInfo() << "Nagative response the io capability request: unpairable.";
    }

    return hci_send;
}

std::shared_ptr<hci_data> pairing_manager::handle_ssp_completed( std::vector<uint8_t> const& a_event )
{
    std::shared_ptr<hci_data> hci_send;
    uint8_t status = a_event[2];
    bluetooth_address address;
    memcpy( address.address, a_event.data() + 3, bluetooth_address::s_bluetooth_address_size );

    LogUtilInfo() << "Pairing with " << address.to_string() << " completed with status "
        << static_cast< uint16_t >( status );

    if( 0x00 == status )
    {
        // TODO pairing success
    }
    else
    {
        // TODO pairing failed
    }

    for( auto it = m_pairing_cbs.begin(); it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == address )
        {
            it->m_to_remove = true;
        }
    }

    auto mod = framework_manager::get_instance().get_module_manager().get_module( timer_module::s_timer_module_name );
    auto timer_module_ = std::static_pointer_cast<timer_module>( mod );
    m_timer_id = timer_module_->register_once_timer
        (
        [this]( uint32_t a_id, std::string a_name )
        {
            delete_expired_pairing_cb();
        },
        std::chrono::milliseconds(2000),
        "",
        m_attached_module
        );
    return hci_send;
}

std::shared_ptr<hci_data> pairing_manager::handle_user_confirm( std::vector<uint8_t> const& a_event )
{
    std::shared_ptr<hci_data> hci_send;
    bluetooth_address address;
    memcpy( address.address, a_event.data() + 2, bluetooth_address::s_bluetooth_address_size );
    uint32_t number_value = 0x00;
    number_value = le_to_host32( a_event.data() + 8 );

    LogUtilInfo() << "SSP pairing user confirm from: " << address.to_string()
        << " confirm value: " << number_value;

    bool found = true;
    for( auto it = m_pairing_cbs.begin(); it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == address )
        {
            it->set_confirm_value( number_value );
            auto dev_manager = framework_manager::get_instance().get_info_manager()
                .get_detail_information<device_manager>( device_manager::s_device_manager_name );
            auto [has_name, device_name] = dev_manager->get_device_name( address );
            if( has_name )
            {
                it->set_remote_name( device_name );
            }
            found = true;
            break;
        }
    }

    if( true )
    {
        notify_pairing_request( address );
    }
    return hci_send;
}

std::shared_ptr<hci_data> pairing_manager::handle_link_key_notify( std::vector<uint8_t> const& a_event )
{
    uint8_t const* p_hci_event = a_event.data();
    uint8_t length = p_hci_event[1];
    if( a_event.size() < length + 2 )
    {
        LogUtilError() << "wrong event size. ignore this hci event";
        return nullptr;
    }

    bluetooth_address address;
    memcpy( address.address, p_hci_event + 2, bluetooth_address::s_bluetooth_address_size );

    std::vector<uint8_t> link_key;
    uint8_t const* p_link_key = p_hci_event + 2 + bluetooth_address::s_bluetooth_address_size;
    link_key.assign( p_link_key, p_link_key + remote_device::s_link_key_size );

    uint8_t const* p_type = p_link_key + remote_device::s_link_key_size;
    uint8_t link_key_type = p_type[0];

    add_trust_device( address, std::move( link_key ), link_key_type );

    std::shared_ptr<bluetooth_common_event> bt_event;
    bt_event = std::make_shared<bluetooth_common_event>();
    bt_event->set_source_module( m_attached_module );
    bt_event->m_bluetooth_event_type = bluetooth_event_type::pairing_completed;
    bt_event->m_remote_device = address;

    framework_manager::get_instance().get_thread_manager().post_task( bt_event, framework::source_here );

    return nullptr;
}

std::shared_ptr<hci_data> pairing_manager::handle_link_key_request( std::vector<uint8_t> const& a_event )
{
    std::shared_ptr<hci_data> hci_;
    uint8_t const* p_hci_event = a_event.data();
    uint8_t length = p_hci_event[1];
    if( a_event.size() < length + 2 )
    {
        LogUtilError() << "wrong event size. ignore this hci event";
        return nullptr;
    }

    bluetooth_address address;
    memcpy( address.address, p_hci_event + 2, bluetooth_address::s_bluetooth_address_size );
    auto trust_devices_ = framework_manager::get_instance().get_info_manager()
        .get_detail_information<trusted_devices>( trusted_devices::s_trusted_devices_name );
    std::vector<uint8_t> link_key;
    bool has_link_key = trust_devices_->get_link_key( address, link_key );
    if( !has_link_key )
    {
        link_key.clear();
    }

    hci_ = make_link_key_reply( address, link_key );

    return hci_;
}

void pairing_manager::add_trust_device
    ( 
    bluetooth_address a_address,
    std::vector<uint8_t> a_link_key,
    uint8_t a_key_type
    )
{
    auto trust_devices_ = framework_manager::get_instance().get_info_manager()
        .get_detail_information<trusted_devices>( trusted_devices::s_trusted_devices_name );

    auto dev_manager = framework_manager::get_instance().get_info_manager()
        .get_detail_information<device_manager>( device_manager::s_device_manager_name );

    auto device = dev_manager->take_found_device( a_address );
    if( !device )
    {
        device = std::make_shared<remote_device>();
    }

    for( auto it = m_pairing_cbs.begin(); it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == a_address )
        {
            auto [has_name, name] = it->get_remote_name();
            if( has_name )
            {
                device->set_name( name );
            }
        }
    }

    device->set_link_key( a_link_key.data(), a_key_type );

    trust_devices_->add_trusted_device( *device );
}

void pairing_manager::notify_pairing_request( bluetooth_address a_address )
{
    std::function<void()> fun;
    for( auto it = m_pairing_cbs.begin(); it != m_pairing_cbs.end(); ++it )
    {
        if( it->m_remote_device == a_address )
        {
            auto [has_name, device_name] = it->get_remote_name();
            auto [has_confirm, passkey] = it->get_confirm_value();
            if( !has_name )
            {
                LogUtilInfo() << "No name, waiting for name request result";
                return;
            }

            if( !has_confirm )
            {
                LogUtilInfo() << "No passkey, waiting for passkey";
                return;
            }

            fun = [a_address,device_name,passkey]()mutable
            {
                ssp_pairing_confirm_request_data ssp_confirm;
                ssp_confirm.address = reinterpret_cast<void*>( a_address.address );
                ssp_confirm.confirm_number = passkey;
                ssp_confirm.remote_device_name = device_name.data();
                ssp_confirm.name_size = device_name.size();

                stack_manager::get_instance().get_stack_callback()( service_type::not_specified,
                    function_type::ss_pairing_confirm_request, &ssp_confirm );
            };
            break;
        }
    }

    if( fun )
    {
        std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
        task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
        task->set_source_module( m_attached_module );
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
}

void pairing_manager::delete_expired_pairing_cb()
{
    for( auto it = m_pairing_cbs.begin(); it != m_pairing_cbs.end(); )
    {
        if( it->m_to_remove )
        {
            it = m_pairing_cbs.erase( it );
            continue;
        }
        else
        {
            ++it;
        }
    }

    auto mod = framework_manager::get_instance().get_module_manager().get_module( timer_module::s_timer_module_name );
    auto timer_module_ = std::static_pointer_cast< timer_module >( mod );
    timer_module_->undregister_timer( m_timer_id );
    m_timer_id = 0x00;
}

}

