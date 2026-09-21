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

#include "bluetooth.h"
#include "uuid.h"
#include "stack/stack_manager.h"
#include "gap/gap_module.h"
#include "rfcomm/spp_module.h"
#include "rfcomm/rfcomm_module.h"

#include "framework\framework_manager.h"
#include "framework\framework_event.h"
#include "framework\log_util.h"

#include <vector>
#include <string>
#include <future>

bluetooth_interface s_interface;
serial_port_interface s_serial_port_interface;

namespace
{
void init_bt_interface();

void enable_bt();

void disable_bt();

void set_callback( stack_callback a_callback );

void search_device();

void set_discoverable( bool a_discoverable );

void set_connectable( bool a_connectable );

void set_pairable( bool a_pairable );

void set_visiblility( bool a_discoverable, bool a_connectable );

void set_local_name( std::u8string a_name );

void cancel_search_edr_device();

void accept_ssp_confirm( bluetooth_address a_address, bool a_accept );

void* get_profile_interface( profile_type a_profile );

}

namespace serial_port
{

uint8_t register_service( bluetooth::serial_port_register_parameters parameter );

void connect_uuid( bluetooth_address a_address, char* uuids, uint8_t uuid_count );

void connect_port( bluetooth_address a_address, uint8_t port );

void accept_coming_connection( bluetooth_address a_address, uint8_t port );

void reject_coming_connection( bluetooth_address a_address, uint8_t port );

void disconnect_port( bluetooth_address a_address, uint8_t a_port, bool a_port_on_local );

void disconnect( bluetooth_address a_address );

void connect( bluetooth_address a_address );

void send( bluetooth_address a_address, uint8_t a_port, bool a_port_on_local, uint8_t* a_buffer, uint32_t a_size );

}

bluetooth_interface* get_bt_interface()
{
    static bluetooth_interface* s_inter = nullptr;
    if( !s_inter )
    {
        // to do thread safe problem may here
        LogUtilInfo() << "Initialize the interface.";
        init_bt_interface();
        s_inter = &s_interface;
    }
    return s_inter;
}

namespace
{

void init_bt_interface()
{
    memset( &s_interface, 0x00, sizeof( s_interface ) );
    memset( &s_serial_port_interface, 0x00, sizeof( s_serial_port_interface ) );

    s_interface.size = sizeof( s_interface );
    s_interface.enable = &enable_bt;
    s_interface.disable = &disable_bt;
    s_interface.set_callback = &set_callback;
    s_interface.search_edr_device = &search_device;
    s_interface.cancel_search_edr_device = &cancel_search_edr_device;
    s_interface.set_dicoverable = &set_discoverable;
    s_interface.set_connectable = &set_connectable;
    s_interface.set_visiblility = &set_visiblility;
    s_interface.set_local_name = &set_local_name;
    s_interface.set_pairable = &set_pairable;
    s_interface.accept_ssp_confirm = &accept_ssp_confirm;
    s_interface.get_profile_interface = &get_profile_interface;

    s_serial_port_interface.register_service = &serial_port::register_service;
    s_serial_port_interface.connect_uuid = &serial_port::connect_uuid;
    s_serial_port_interface.connect_port = &serial_port::connect_port;
    s_serial_port_interface.accept_coming_connection = &serial_port::accept_coming_connection;
    s_serial_port_interface.reject_coming_connection = &serial_port::reject_coming_connection;
    s_serial_port_interface.disconnect = &serial_port::disconnect;
    s_serial_port_interface.disconnect_port = &serial_port::disconnect_port;
    s_serial_port_interface.connect = &serial_port::connect;
    s_serial_port_interface.send = &serial_port::send;
}

void enable_bt()
{
    bluetooth::stack_manager::get_instance().enable_bt();
}

void set_callback( stack_callback a_callback )
{
    bluetooth::stack_manager::get_instance().set_callback( a_callback );
}

void search_device()
{
    bluetooth::stack_manager::get_instance().search_device();
}

void cancel_search_edr_device()
{
    bluetooth::stack_manager::get_instance().cancel_search_edr_device();
}

void accept_ssp_confirm( bluetooth_address a_address, bool a_accept )
{
    bluetooth::stack_manager::get_instance().accept_ssp_confirm( a_address, a_accept );
}

void* get_profile_interface( profile_type a_profile )
{
    switch( a_profile )
    {
    case serial_port_profile:
        return &s_serial_port_interface;
        break;
    default:
        break;
    }
    return nullptr;
}

void set_discoverable( bool a_discoverable )
{
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    if( a_discoverable )
    {
        task->m_gap_task_type = bluetooth::gap_module::gap_task_type::to_discoverable;
    }
    else
    {
        task->m_gap_task_type = bluetooth::gap_module::gap_task_type::to_nondiscoverable;
    }
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void set_connectable( bool a_connectable )
{
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    if( a_connectable )
    {
        task->m_gap_task_type = bluetooth::gap_module::gap_task_type::to_connectable;
    }
    else
    {
        task->m_gap_task_type = bluetooth::gap_module::gap_task_type::to_disconnectable;
    }
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void set_visiblility( bool a_discoverable, bool a_connectable )
{
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    task->m_connectable = a_connectable;
    task->m_discoverable = a_discoverable;
    task->m_gap_task_type = bluetooth::gap_module::gap_task_type::visibility_setting;
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void set_local_name( std::u8string a_name )
{
    LogUtilInfo() << "Change local name to " << (char*)( a_name.c_str());
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    task->m_localname = a_name;
    task->m_gap_task_type = bluetooth::gap_module::gap_task_type::change_local_name;
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void disable_bt()
{
    std::shared_ptr<framework::framework_event> event_ =
        std::make_shared<framework::framework_event>();
    event_->m_event_type = framework::event_type::power_off;
    framework::framework_manager::get_instance().get_thread_manager().post_task( event_, framework::source_here );
}

void set_pairable( bool a_pairable )
{
    bluetooth::gap_module::gap_task_type type;
    type = a_pairable ? bluetooth::gap_module::gap_task_type::to_pairable :
        bluetooth::gap_module::gap_task_type::to_unpairable;
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    task->m_gap_task_type = type;
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

}

namespace serial_port
{

uint8_t register_service( bluetooth::serial_port_register_parameters parameter )
{
    uint8_t ret = 0;
    std::vector<bluetooth::uuid> uuids;
    char* p_uuid = parameter.uuids;
    for( uint8_t i = 0; i < parameter.uuid_count; ++i )
    {
        bluetooth::uuid uuid_;
        memcpy( uuid_.uu.data(), p_uuid, bluetooth::uuid::s_128bituuid_size );
        p_uuid += bluetooth::uuid::s_128bituuid_size;
        uuids.push_back( uuid_ );
    }

    std::u8string name( reinterpret_cast< std::u8string::value_type const* >( parameter.service_name ) );

    std::shared_ptr<bluetooth::spp_module::spp_task> task;
    task = std::make_shared<bluetooth::spp_module::spp_task>();
    task->m_name = name;
    task->m_type = bluetooth::spp_task_type::register_new_spp_service;
    task->m_uuids = std::move( uuids );

    task->set_target_module( bluetooth::spp_module::s_spp_module_name );

    std::shared_ptr<std::promise<uint8_t>> promise_;
    promise_ = std::make_shared<std::promise<uint8_t>>();
    auto future_ = promise_->get_future();
    auto fun = [promise_]( uint8_t a_port )mutable
    {
        promise_->set_value( a_port );
    };

    task->m_registered_callback = fun;
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    future_.wait();
    ret = future_.get();

    return ret;
}

void connect_uuid( bluetooth_address a_address, char* uuids, uint8_t uuid_count )
{

}

void connect_port( bluetooth_address a_address, uint8_t port )
{

}

void accept_coming_connection( bluetooth_address a_address, uint8_t port )
{
    std::shared_ptr<bluetooth::rfcomm_module::rfcomm_task> task;
    task = std::make_shared<bluetooth::rfcomm_module::rfcomm_task>();
    task->m_type = bluetooth::rfcomm_task_type::accept_coming_connection_request;
    task->port = port;
    task->address = a_address;
    task->set_target_module( bluetooth::rfcomm_module::s_rfcomm_module_name );

    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void reject_coming_connection( bluetooth_address a_address, uint8_t port )
{
    std::shared_ptr<bluetooth::rfcomm_module::rfcomm_task> task;
    task = std::make_shared<bluetooth::rfcomm_module::rfcomm_task>();
    task->m_type = bluetooth::rfcomm_task_type::reject_coming_connection_request;
    task->port = port;
    task->address = a_address;
    task->set_target_module( bluetooth::rfcomm_module::s_rfcomm_module_name );

    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void disconnect_port( bluetooth_address a_address, uint8_t a_port, bool a_port_on_local )
{
    std::shared_ptr<bluetooth::spp_module::spp_task> task;
    task = std::make_shared<bluetooth::spp_module::spp_task>();
    task->m_type = bluetooth::spp_task_type::disconnect_specified_port;
    task->m_port = a_port;
    task->m_address = a_address;
    task->m_port_on_local = a_port_on_local;
    task->set_target_module( bluetooth::spp_module::s_spp_module_name );

    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void disconnect( bluetooth_address a_address )
{
    std::shared_ptr<bluetooth::spp_module::spp_task> task;
    task = std::make_shared<bluetooth::spp_module::spp_task>();
    task->m_type = bluetooth::spp_task_type::disconnect_specified_address;
    task->m_address = a_address;
    task->set_target_module( bluetooth::spp_module::s_spp_module_name );

    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void connect( bluetooth_address a_address )
{
    std::shared_ptr<bluetooth::spp_module::spp_task> task;
    task = std::make_shared<bluetooth::spp_module::spp_task>();
    task->m_type = bluetooth::spp_task_type::connect_default_spp;
    task->m_address = a_address;
    task->set_target_module( bluetooth::spp_module::s_spp_module_name );

    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void send( bluetooth_address a_address, uint8_t a_port, bool a_port_on_local, uint8_t* a_buffer, uint32_t a_size )
{
    std::shared_ptr<bluetooth::spp_module::spp_task> task;
    task = std::make_shared<bluetooth::spp_module::spp_task>();
    task->m_type = bluetooth::spp_task_type::async_send_spp_data;
    task->m_address = a_address;
    task->m_port = a_port;
    task->m_port_on_local = a_port_on_local;
    std::shared_ptr<std::vector<uint8_t>> spp_data = std::make_shared<std::vector<uint8_t>>();
    spp_data->assign( a_buffer, a_buffer + a_size );
    task->m_spp_data = std::move( spp_data );
    task->set_target_module( bluetooth::spp_module::s_spp_module_name );

    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

}