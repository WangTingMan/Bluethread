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

#include "spp_module.h"
#include "rfcomm_module.h"
#include "rfcomm_common.h"
#include "stack/stack_manager.h"

#include "framework/log_util.h"
#include "framework/framework_event.h"
#include "framework/internal/platform.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"

#include "../sdp/sdp_common.h"
#include "../sdp/sdp_module.h"

static constexpr uint8_t s_spp_port_start = 10;

namespace bluetooth
{

using namespace framework;

uint16_t spp_module::spp_task::s_spp_task_type_id = 0u;

spp_module::spp_module()
{
    set_name( s_spp_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
}

void spp_module::initialize()
{
    m_next_port_number = s_spp_port_start;
    spp_task::s_spp_task_type_id = framework_manager::get_instance().register_task_type( 1 );
}

void spp_module::deinitialize()
{

}

void spp_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    if (uint16_t( a_task->get_task_type() ) != spp_task::s_spp_task_type_id)
    {
        LogUtilWarning() << "Task type not match: "
            << static_cast<uint16_t>(a_task->get_task_type())
            << " != "
            << spp_task::s_spp_task_type_id
            << "from: " << a_task->get_position();
        return;
    }

    std::shared_ptr<spp_task> detail_task;
    detail_task = std::static_pointer_cast<spp_task>( a_task );
    if( !detail_task )
    {
        LogUtilError() << "Wrong task type.";
        return;
    }

    switch( detail_task->m_type )
    {
    case spp_task_type::register_new_spp_service:
        create_new_spp( detail_task->m_name, detail_task->m_uuids, detail_task->m_registered_callback );
        break;
    case spp_task_type::disconnect_specified_port:
        disconnect( detail_task->m_address, detail_task->m_port, detail_task->m_port_on_local );
        break;
    case spp_task_type::disconnect_specified_address:
        disconnect( detail_task->m_address );
        break;
    case spp_task_type::connect_default_spp:
        connect( detail_task->m_address );
        break;
    case spp_task_type::async_send_spp_data:
        send_port_data( detail_task->m_address, detail_task->m_port,
            detail_task->m_port_on_local, std::move( detail_task->m_spp_data ) );
        break;
    default:
        LogUtilError() << "sdp task type ignored: " << static_cast<uint32_t>( detail_task->m_type );
    }
}

void spp_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    switch( a_event->m_event_type )
    {
    case event_type::power_on:
        m_next_port_number = s_spp_port_start;
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case event_type::power_off:
        set_power_status( abstract_module::powering_status::power_off );
        m_local_spp_services.clear();
        break;
    case event_type::power_status_changed:
        break;
    case event_type::derived_type:
        if (bluetooth_common_event::s_bluetooth_common_event_type == a_event->m_derived_type)
        {
            handle_bluetoot_event( std::static_pointer_cast<bluetooth_common_event>(a_event) );
        }
        break;
    default:
        LogUtilError() << "framework event ignored: " << static_cast<uint16_t>( a_event->m_event_type );
        break;
    }
}

void spp_module::handle_service_record_registered
    (
    uint32_t a_record_handle,
    std::u8string a_name,
    std::function<void( uint8_t )> a_registered_callback,
    uint8_t a_port,
    std::vector<uuid> a_uuids
    )
{
    LogUtilInfo() << "service: " << convert( a_name ) << " registered. port: "
        << static_cast< uint32_t >( a_port );

    bool found = false;
    for( auto& ele : m_local_spp_services )
    {
        if( ele.uuids == a_uuids )
        {
            ele.port_number = a_port;
            ele.sdp_service_recode_handle = a_record_handle;
            ele.service_name = a_name;
            LogUtilError() << "Using already exsited spp service info";
            found = true;
            break;
        }
    }

    if( !found )
    {
        local_spp_service_info info;
        info.port_number = a_port;
        info.sdp_service_recode_handle = a_record_handle;
        info.service_name = a_name;
        info.uuids = a_uuids;
        m_local_spp_services.push_back( info );
    }

    a_registered_callback( a_port );
    m_spp_base_record_id = a_record_handle;

    rfcomm_port_callback_block cb;
    cb.port_number = a_port;
    cb.local_inited = false;
    cb.handle_module = s_spp_module_name;
    cb.connection_changed_callback = std::bind( &spp_module::handle_connection_changed, this, std::placeholders::_1,
        std::placeholders::_2, a_port, false );
    cb.connection_request_callback = std::bind( &spp_module::handle_connection_request, this, std::placeholders::_1, a_port );
    cb.data_callback = std::bind( &spp_module::handle_sdu, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4, a_port, false );

    auto mod = framework_manager::get_instance().get_module_manager().get_module( rfcomm_module::s_rfcomm_module_name );
    auto detail_module = std::static_pointer_cast<rfcomm_module> ( mod );
    if( detail_module )
    {
        detail_module->register_callback( cb );
    }
    else
    {
        LogUtilError() << "Cannot register the port callback";
    }
}

void spp_module::handle_connection_request
    (
    bluetooth_address a_address,
    uint8_t a_port
    )
{
    std::shared_ptr<rfcomm_module::rfcomm_task> tsk;
    auto connection_ = find( a_address, a_port, false );
    if( connection_ )
    {
        if( connection_->get_connection_status() != connection_status::disconnected )
        {
            LogUtilDebug() << "We already dial with the same remote device and same port. so reject such connection request";
            tsk = std::make_shared<rfcomm_module::rfcomm_task>();
            tsk->m_type = rfcomm_task_type::reject_coming_connection_request;
        }
    }

    if( tsk )
    {
        tsk->address = a_address;
        tsk->port = a_port;
        tsk->set_target_module( rfcomm_module::s_rfcomm_module_name );
        tsk->set_source_module( get_name() );
        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
    }
    else
    {
        connection_ = std::make_shared<spp_connection>();
        m_connections.push_back( connection_ );
        connection_->set_local_inited( false );
        connection_->set_remote_device( a_address );
        connection_->set_port( a_port );
        connection_->set_connection_status( connection_status::connecting );

        std::function<void()> fun;
        fun = [a_address, a_port]()mutable
            {
                serial_port_connection_request request;
                request.address = reinterpret_cast<void*>( a_address.address );
                request.port_number = a_port;
                stack_manager::get_instance().get_stack_callback()( service_type::serial_port_profile,
                    function_type::serial_connection_request, reinterpret_cast<void*>( &request ) );
            };

        std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
        task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
        task->set_source_module( s_spp_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
}

void spp_module::handle_connection_changed
    (
    bluetooth_address a_address,
    connection_status a_status,
    uint8_t a_port,
    bool a_local_inited
    )
{
    LogUtilInfo() << "remote " << a_address.to_string() << ", port: " << static_cast< uint32_t >( a_port )
        << ", status: " << a_status;

    std::vector<uuid> local_uuid;
    bool found = false;
    if( !a_local_inited )
    {
        for( auto& ele: m_local_spp_services )
        {
            if( ele.port_number == a_port )
            {
                local_uuid = ele.uuids;
                found = true;
                break;
            }
        }
    }

    if( !found )
    {
        LogUtilError() << "Shall not happen: Local port listened without registered uuid.";
    }

    auto connection_ = find( a_address, a_port, a_local_inited );
    if( !connection_ )
    {
        connection_ = std::make_shared<spp_connection>();
        m_connections.push_back( connection_ );
        connection_->set_local_inited( a_local_inited );
        connection_->set_remote_device( a_address );
        connection_->set_port( a_port );
    }

    connection_->set_connection_status( a_status );

    if( a_status == connection_status::disconnected )
    {
        for( auto it = m_connections.begin(); it != m_connections.end(); ++it )
        {
            if( *it == connection_ )
            {
                m_connections.erase( it );
                break;
            }
        }
    }

    std::function<void()> fun;
    fun = [a_address, a_status, a_port, a_local_inited, local_uuid]()mutable
        {
            serial_port_connection_status status;
            status.address = reinterpret_cast<void*>( a_address.address );
            status.port_on_local = a_local_inited;
            status.status = a_status;
            status.port_number = a_port;
            status.uuids = reinterpret_cast<char*>( local_uuid.data() );
            status.uuid_count = local_uuid.size();

            stack_manager::get_instance().get_stack_callback()( service_type::serial_port_profile,
                function_type::serial_connection_status_changed, reinterpret_cast< void* >( &status ) );
        };

    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
    task->set_source_module( s_spp_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void spp_module::handle_bluetoot_event( std::shared_ptr<bluetooth_common_event> const& a_bt_event )
{
    if( !a_bt_event )
    {
        LogUtilError() << "Empty bluetooth common event to handle.";
        return;
    }

    switch( a_bt_event->m_bluetooth_event_type )
    {
    case bluetooth_event_type::edr_acl_disconnected:
        for( auto it = m_connections.begin(); it != m_connections.end(); )
        {
            std::shared_ptr<spp_connection> con = *it;
            if( con->get_remote_device() == a_bt_event->m_remote_device )
            {
                con->set_connection_status( connection_status::disconnected );
                it = m_connections.erase( it );
            }
            else
            {
                ++it;
            }
        }
        break;
    default:
        break;
    }
}

void spp_module::handle_sdu
    (
    std::shared_ptr<hci_data> a_raw,
    uint32_t a_offset,
    uint32_t a_size,
    bluetooth_address a_address,
    uint8_t a_port,
    bool a_local_inited
    )
{
    std::function<void()> fun;
    fun = [a_raw, a_offset, a_size, a_address, a_port, a_local_inited]()mutable
        {
            uint8_t* p_sdu = a_raw->m_buffer.data() + a_offset;
            serial_port_data_received_info data_recv;
            data_recv.address = reinterpret_cast<void*>( a_address.address );
            data_recv.data_size = a_size;
            data_recv.p_data = p_sdu;
            data_recv.port_number = a_port;
            data_recv.port_on_local = a_local_inited;

            stack_manager::get_instance().get_stack_callback()( service_type::serial_port_profile,
                function_type::serial_port_data_received, reinterpret_cast< void* >( &data_recv ) );
        };

    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
    task->set_source_module( s_spp_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void spp_module::disconnect
    (
    bluetooth_address a_address,
    uint8_t a_port,
    bool a_port_on_local
    )
{
    auto conection_ = find( a_address, a_port, a_port_on_local );
    if( !conection_ )
    {
        LogUtilDebug() << "No such spp connection for address: " << a_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port );
        return;
    }

    if( conection_->get_connection_status() == connection_status::connected )
    {
        std::shared_ptr<bluetooth::rfcomm_module::rfcomm_task> task;
        task = std::make_shared<bluetooth::rfcomm_module::rfcomm_task>();
        task->m_type = bluetooth::rfcomm_task_type::disconnect_specified_port;
        task->port = a_port;
        task->address = a_address;
        task->port_on_local = a_port_on_local;
        task->set_target_module( rfcomm_module::s_rfcomm_module_name );
        conection_->set_connection_status( connection_status::disconnecting );

        bluetooth::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
    else
    {
        LogUtilDebug() << "Cannot disconnect the port, address: " << a_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port ) << ", status: " << conection_->get_connection_status();
    }
}

void spp_module::disconnect
    (
    bluetooth_address a_address
    )
{
    for( auto& ele : m_connections )
    {
        if( ele->get_remote_device() != a_address )
        {
            continue;
        }

        if( ele->get_connection_status() == connection_status::connected )
        {
            std::shared_ptr<bluetooth::rfcomm_module::rfcomm_task> task;
            task = std::make_shared<bluetooth::rfcomm_module::rfcomm_task>();
            task->m_type = bluetooth::rfcomm_task_type::disconnect_specified_port;
            task->port = ele->get_port();
            task->address = ele->get_remote_device();
            task->port_on_local = ele->local_inited();
            task->set_target_module( rfcomm_module::s_rfcomm_module_name );
            ele->set_connection_status( connection_status::disconnecting );

            bluetooth::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        }
    }
}

void spp_module::connect
    (
    bluetooth_address a_address
    )
{
    std::vector<uuid> uuids;
    uuids.push_back( uuid::from_16bit( sdp_service_uuid::serial_port ) );

    std::shared_ptr<spp_connection> spp_conn;
    spp_conn = find( a_address, uuids, true );
    if( !spp_conn )
    {
        spp_conn = std::make_shared<spp_connection>();
        spp_conn->set_remote_device( a_address );
        spp_conn->set_uuid( uuids );
        spp_conn->set_local_inited( true );
        m_connections.push_back( spp_conn );
    }

    if( spp_conn->get_connection_status() != connection_status::disconnected )
    {
        LogUtilDebug() << "remote device: " << a_address.to_string()
            << ", uuid: " << uuids.front().to_string() << ", connection status: "
            << spp_conn->get_connection_status() << ", reject the connection request";
        return;
    }

    spp_conn->set_connection_status( connection_status::connecting );

    std::shared_ptr<sdp_module::sdp_task> tsk;
    tsk = std::make_shared<sdp_module::sdp_task>();
    tsk->m_type = sdp_module::sdp_task_type::service_search_attribute;
    tsk->m_remote_device = a_address;
    tsk->m_service_uuid.push_back( uuid::from_16bit( sdp_service_uuid::serial_port ) );
    tsk->set_source_module( get_name() );

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
    LogUtilInfo() << "Connect to defaule spp port with remote device " << a_address.to_string();
}

void spp_module::send_port_data
    (
    bluetooth_address a_address,
    uint8_t a_port,
    bool a_local_inited,
    std::shared_ptr<std::vector<uint8_t>> a_spp_data
    )
{
    std::shared_ptr<spp_connection> spp_conn = find( a_address, a_port, a_local_inited );
    if( !spp_conn )
    {
        LogUtilError() << "No such spp connection. remote device: " << a_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port ) << ( a_local_inited ? " local inited" : " remote inited" );
        return;
    }

    if( spp_conn->get_connection_status() != connection_status::connected )
    {
        LogUtilError() << "Such spp connection not connected. remote device: " << a_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port ) << ( a_local_inited ? " local inited" : " remote inited" )
            << ", connection status:" << spp_conn->get_connection_status();
        return;
    }

    std::shared_ptr<bluetooth::rfcomm_module::rfcomm_task> task;
    task = std::make_shared<bluetooth::rfcomm_module::rfcomm_task>();
    task->m_type = bluetooth::rfcomm_task_type::async_send_spp_data;
    task->port = a_port;
    task->address = a_address;
    task->port_on_local = a_local_inited;
    task->spp_data = std::move( a_spp_data );
    task->set_target_module( rfcomm_module::s_rfcomm_module_name );
    task->set_source_module( get_name() );

    bluetooth::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void spp_module::create_new_spp
    (
    std::u8string a_name,
    std::vector<uuid> a_uuids,
    std::function<void( uint8_t )> a_registered_callback
    )
{
    LogUtilInfo() << "register spp, name: " << convert( a_name );
    uint8_t port_used = m_next_port_number;
    ++m_next_port_number;

    std::shared_ptr<sdp_service_record> spp_base_sdp_record;
    spp_base_sdp_record = std::make_shared<sdp_service_record>();

    std::vector<uuid> service_uuids;
    auto spp_uuid = uuid::from_16bit( sdp_service_uuid::serial_port );
    if( a_uuids.empty() )
    {
        service_uuids.push_back( spp_uuid );
    }
    else
    {
        service_uuids = std::move( a_uuids );
        bool has_spp_uuid = false;
        for( auto& ele : service_uuids )
        {
            if( ele == spp_uuid )
            {
                has_spp_uuid = true;
                break;
            }
        }

        if( !has_spp_uuid )
        {
            service_uuids.push_back( spp_uuid );
        }
    }
    spp_base_sdp_record->set_service_class_id_list( service_uuids );

    std::vector<protocol_descriptor> desc_list;
    protocol_descriptor desc_;
    desc_.m_uuid = uuid::from_16bit( sdp_service_uuid::l2cap );
    desc_list.push_back( desc_ );
    desc_.m_uuid = uuid::from_16bit( sdp_service_uuid::rfcomm );
    sdp_data_element paras;
    paras.set_uint8_value( port_used );
    desc_.m_parameters.push_back( paras );
    desc_list.push_back( desc_ );
    spp_base_sdp_record->set_protocol_descriptor_list( desc_list );

    std::vector<sdp_data_element> profile_desc;
    paras.set_uuid( uuid::from_16bit( sdp_service_uuid::serial_port ) );
    profile_desc.push_back( paras );
    paras.set_uint16_value( 0x0102 );
    profile_desc.push_back( paras );
    spp_base_sdp_record->set_bluetooth_profile_descriptor_list( profile_desc );

    spp_base_sdp_record->set_default_language();

    paras.set_string_value( a_name );
    spp_base_sdp_record->set_attribute
        (
        sdp_universal_attribute_id::provider_name_offset + language_base_id::english,
        paras
        );

    std::shared_ptr<sdp_module::sdp_task> tsk;
    tsk = std::make_shared<sdp_module::sdp_task>();
    tsk->m_type = sdp_module::sdp_task_type::register_service_record;
    tsk->m_callback_handle_module = get_name();
    tsk->m_registered_callback = std::bind( &spp_module::handle_service_record_registered, this, std::placeholders::_1,
        a_name, a_registered_callback, port_used, service_uuids );
    tsk->set_source_module( get_name() );
    tsk->set_target_module( sdp_module::s_sdp_module_name );
    tsk->m_service_record = spp_base_sdp_record;

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

std::shared_ptr<spp_connection> spp_module::find
    (
    bluetooth_address a_address,
    uint8_t a_port,
    bool a_local_inited
    )
{
    for( auto& ele : m_connections )
    {
        if( ele->get_remote_device() == a_address &&
            ele->get_port() == a_port &&
            ele->local_inited() == a_local_inited )
        {
            return ele;
        }
    }
    return nullptr;
}

std::shared_ptr<spp_connection> spp_module::find
    (
    bluetooth_address a_address,
    std::vector<uuid> a_uuid,
    bool a_local_inited
    )
{
    for( auto& ele : m_connections )
    {
        if( ele->get_remote_device() == a_address &&
            ele->get_uuid() == a_uuid &&
            ele->local_inited() == a_local_inited )
        {
            return ele;
        }
    }

    return nullptr;
}

}

