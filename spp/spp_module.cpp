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
#include "rfcomm/rfcomm_module.h"
#include "rfcomm/rfcomm_common.h"
#include "stack/stack_manager.h"

#include "framework/log_util.h"
#include "framework/framework_event.h"
#include "framework/internal/platform.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"

#include "sdp/sdp_common.h"
#include "sdp/sdp_module.h"
#include "sdp/sdp_task.h"
#include "spp/spp_task.h"
#include "spp/spp_sdp_utils.h"
#include "internal/internal_spp_task.h"

namespace bluetooth
{

using namespace framework;

spp_module::spp_module()
{
    set_name( s_spp_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
}

void spp_module::initialize()
{
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

    std::shared_ptr<spp_task> _spp_task;
    _spp_task = std::static_pointer_cast<spp_task>( a_task );
    if( !_spp_task )
    {
        LogUtilError() << "Wrong task type.";
        return;
    }

    switch( _spp_task->m_type )
    {
    case spp_task_type::spp_register_new_service:
        {
            auto detail_task = static_pointer_cast<spp_register_new_service_task>( _spp_task );
            create_new_spp( detail_task->m_name, detail_task->m_uuids, detail_task->m_registered_callback );
        }
        break;
    case spp_task_type::spp_disconnect_specified_port:
        {
            auto detail_task = static_pointer_cast<spp_disconnect_specified_port_task>( _spp_task );
            disconnect_port( detail_task->m_address, detail_task->m_port, detail_task->is_local_inited_port_connection );
        }
        break;
    case spp_task_type::spp_disconnect_specified_address:
        {
            auto detail_task = static_pointer_cast<spp_disconnect_specified_address_task>( _spp_task );
            disconnect( detail_task->m_address );
        }
        break;
    case spp_task_type::spp_connect_default_spp:
        {
            auto detail_task = static_pointer_cast<spp_connect_default_spp_task>( _spp_task );
            connect( detail_task->m_address );
        }
        break;
    case spp_task_type::spp_async_send_spp_data:
        {
            auto detail_task = static_pointer_cast<spp_async_send_spp_data_task>( _spp_task );
            send_port_data( detail_task->m_address, detail_task->m_port,
                detail_task->is_local_inited_port_connection, std::move( detail_task->m_spp_data ) );
        }
        break;
    case spp_task_type::spp_sdp_service_attribute_search_result:
        {
            auto detail_task = static_pointer_cast<spp_sdp_service_attribute_search_result_task>( _spp_task );
            handle_spp_sdp_service_attribute_search_result( detail_task );
        }
        break;
    default:
        LogUtilError() << "sdp task type ignored: " << static_cast<uint32_t>( _spp_task->m_type );
    }
}

void spp_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    switch( a_event->m_event_type )
    {
    case event_type::power_on:
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

void spp_module::handle_rfcomm_port_registered
    (
    std::u8string a_name,
    uint8_t a_port,
    std::vector<uuid> a_uuids,
    std::function<void( uint8_t )> a_registered_callback
    )
{
    LogUtilInfo() << "service: " << convert( a_name ) << " rfcomm registered. port: "
        << static_cast< uint32_t >( a_port ) << ", going to register sdp service record";

    uint8_t port_used = a_port;

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
        service_uuids = a_uuids;
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
        attribute_id_offset_for_string::provider_name_offset + language_base_id::english,
        paras
        );

    std::shared_ptr<sdp_task_register_service_record> tsk;
    tsk = std::make_shared<sdp_task_register_service_record>();
    tsk->m_callback_handle_module = get_name();
    tsk->m_registered_callback = [this, a_uuids, port_used, a_registered_callback]( uint32_t a_sdp_service_record_handle )
        {
            bool found = false;
            LogUtilDebug() << "sdp service record registeration done. handle is " << a_sdp_service_record_handle;
            for( auto& ele : m_local_spp_services )
            {
                if( ele.uuids == a_uuids )
                {
                    ele.port_number = port_used;
                    ele.sdp_service_recode_handle = a_sdp_service_record_handle;
                    found = true;
                    break;
                }
            }

            if( found && a_registered_callback )
            {
                LogUtilDebug() << "notify upper layer that the rfcomm port registered";
                a_registered_callback( port_used );
            }
        };
    tsk->set_source_module( get_name() );
    tsk->set_target_module( sdp_module::s_sdp_module_name );
    tsk->m_service_record = spp_base_sdp_record;

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void spp_module::handle_connection_request
    (
    bluetooth_address a_address,
    uint8_t a_port
    )
{
    LogUtilDebug() << "handle connection request from " << a_address.to_string()
        << ", port : " << (uint32_t)a_port;
    std::shared_ptr<rfcomm_task_reject_coming_connection_request> tsk;
    auto connection_ = find( a_address, a_port, false );
    if( connection_ )
    {
        if( connection_->get_connection_status() != connection_status::disconnected )
        {
            LogUtilDebug() << "We already dial with the same remote device and same port. so reject such connection request";
            tsk = std::make_shared<rfcomm_task_reject_coming_connection_request>();
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
    uint8_t           a_port,
    bool              a_local_inited
    )
{
    LogUtilInfo() << "remote " << a_address.to_string() << ", port: " << static_cast< uint32_t >( a_port )
        << ( a_local_inited ? ", local inited" : ", remote inited" ) << ", status: " << a_status;

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

        if( !found )
        {
            LogUtilError() << "Shall not happen: Local port listened without registered uuid.";
        }
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
            status.port_on_local = !a_local_inited;
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

    if( a_local_inited && a_status == connection_status::disconnected )
    {
        LogUtilDebug() << "unregister the callback since already disconnected remote port";
        rfcomm_port_callback_block cb;
        cb.port_number = a_port;
        cb.is_local_port = false;
        cb.remote_address = a_address;

        auto tsk = std::make_shared<rfcomm_task_port_callback_operation>();
        tsk->to_register = false;
        tsk->port_callback = cb;
        tsk->set_source_module( get_name() );
        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
    }
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
            data_recv.port_on_local = !a_local_inited;

            stack_manager::get_instance().get_stack_callback()( service_type::serial_port_profile,
                function_type::serial_port_data_received, reinterpret_cast< void* >( &data_recv ) );
        };

    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
    task->set_source_module( s_spp_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void spp_module::handle_spp_sdp_service_attribute_search_result
    (
    std::shared_ptr<spp_sdp_service_attribute_search_result_task> a_detail_task
    )
{
    bluetooth_address remote_address = a_detail_task->address;
    std::vector<uuid> uuids = a_detail_task->uuids;
    std::vector<sdp_data_element>const& service_records = a_detail_task->service_record_list;
    LogUtilDebug() << "RFCOMM sdp searching result from " << remote_address.to_string();

    auto spp_conn = a_detail_task->spp_conn;
    if( service_records.empty() )
    {
        /**
         * RFCOMM port connect failed since cannot found such port from remote deivce by specificed uuids.
         */
        LogUtilDebug() << "failed: empty service record";
        handle_spp_connection_failed( spp_conn );
        return;
    }

    std::vector<sdp_data_element> attributes;
    bool matched = false;
    matched = service_records[0].matching_uuids( uuids );
    if( !matched )
    {
        /**
         * RFCOMM port connect failed since cannot found such port from remote deivce by specificed uuids.
         */
        LogUtilDebug() << "failed: service record not matched";
        handle_spp_connection_failed( spp_conn );
        return;
    }

    spp_sdp_service_attribute spp_attris;
    spp_attris = parse_spp_sdp_attrubutes( service_records );
    if( !spp_attris.is_valid )
    {
        LogUtilDebug() << "failed: cannot read the rfcomm port from this service record";
        handle_spp_connection_failed( spp_conn );
        return;
    }

    for( auto& con : m_connections )
    {
        auto& con_uuid = con->get_uuid();
        auto& attr_uuid = a_detail_task->uuids;
        if( con_uuid.size() != attr_uuid.size() )
        {
            continue;
        }

        bool same = true;
        for( std::size_t i = 0; i < con_uuid.size(); ++i )
        {
            if( con_uuid[i] != attr_uuid[i] )
            {
                same = false;
                break;
            }
        }

        if( same )
        {
            con->set_port( spp_attris.port );
        }
    }

    rfcomm_port_callback_block cb;
    cb.port_number = spp_attris.port;
    cb.is_local_port = false;
    cb.remote_address = remote_address;
    cb.handle_module = s_spp_module_name;
    cb.connection_changed_callback = std::bind( &spp_module::handle_connection_changed, this, std::placeholders::_1,
        std::placeholders::_2, spp_attris.port, true );
    cb.connection_request_callback = std::bind( &spp_module::handle_connection_request, this, std::placeholders::_1, spp_attris.port );
    cb.data_callback = std::bind( &spp_module::handle_sdu, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4, spp_attris.port, true );

    auto tsk = std::make_shared<rfcomm_task_port_callback_operation>();
    tsk->to_register = true;
    tsk->port_callback = cb;
    tsk->set_source_module( get_name() );
    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );

    /**
     * The callback shall be registered prior to starting connection establishment.
     * Otherwise the connection process may try to look up the callback while it is not yet registered.
     */
    std::shared_ptr<rfcomm_task_connect_port> task;
    task = std::make_shared<rfcomm_task_connect_port>();
    task->port = spp_attris.port;
    task->address = a_detail_task->address;
    bluetooth::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    LogUtilDebug() << "remote rfcomm port number is " << uint32_t( spp_attris.port );
}

void spp_module::handle_spp_connection_failed( std::shared_ptr<spp_connection> a_sdp_conn )
{
    if( !a_sdp_conn )
    {
        return;
    }

    LogUtilDebug() << "spp connection failed: " << a_sdp_conn->get_remote_device().to_string();

    for( auto it = m_connections.begin(); it != m_connections.end(); )
    {
        if( (*it).get() == a_sdp_conn.get() )
        {
            it = m_connections.erase( it );
        }
        else
        {
            ++it;
        }
    }

    std::function<void()> fun;
    fun = [a_sdp_conn]()mutable
        {
            serial_port_connection_status status;
            status.address = (void*)a_sdp_conn->get_remote_device().address;
            status.port_number = a_sdp_conn->get_port();
            status.port_on_local = !a_sdp_conn->local_inited();
            status.status = connection_status::disconnected;
            stack_manager::get_instance().get_stack_callback()( service_type::serial_port_profile,
                function_type::serial_connection_status_changed, reinterpret_cast<void*>( &status ) );
        };

    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, abstract_module::s_general_seq_task_runner_module );
    task->set_source_module( s_spp_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void spp_module::disconnect_port
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
        std::shared_ptr<rfcomm_task_disconnect_specified_port> task;
        task = std::make_shared<rfcomm_task_disconnect_specified_port>();
        task->port = a_port;
        task->address = a_address;
        task->is_local_inited_port_connection = a_port_on_local;
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
            std::shared_ptr<rfcomm_task_disconnect_specified_port> task;
            task = std::make_shared<rfcomm_task_disconnect_specified_port>();
            task->port = ele->get_port();
            task->address = ele->get_remote_device();
            task->is_local_inited_port_connection = ele->local_inited();
            task->set_target_module( rfcomm_module::s_rfcomm_module_name );
            ele->set_connection_status( connection_status::disconnecting );

            bluetooth::framework_manager::get_instance().get_thread_manager()
                .post_task( task, framework::source_here );
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
        m_connections.push_back( spp_conn );
    }

    if( spp_conn->get_connection_status() != connection_status::disconnected )
    {
        LogUtilDebug() << "id: " << spp_conn->get_id() << ", remote device: " << a_address.to_string()
            << ", uuid: " << uuids.front().to_string() << ", connection status: "
            << spp_conn->get_connection_status() << ", reject the connection request";
        return;
    }

    LogUtilInfo() << "Start to connect to defaule spp port with remote device " << a_address.to_string();

    spp_conn->set_remote_device( a_address );
    spp_conn->set_uuid( uuids );
    spp_conn->set_local_inited( true );
    spp_conn->set_connection_status( connection_status::connecting );

    std::shared_ptr<sdp_task_service_search_attribute> tsk;
    tsk = std::make_shared<sdp_task_service_search_attribute>();
    tsk->m_remote_device = a_address;
    tsk->m_service_uuid = uuids;
    tsk->set_source_module( get_name() );
    tsk->m_searching_result_callback = [uuids, a_address, spp_conn]
        ( std::vector<sdp_data_element> a_service_records, sdp_search_result a_result )
        {
            auto tsk = std::make_shared<spp_sdp_service_attribute_search_result_task>();
            tsk->address = a_address;
            tsk->service_record_list = std::move( a_service_records );
            tsk->uuids = uuids;
            tsk->result = a_result;
            tsk->spp_conn = spp_conn;
            framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
        };

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
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

    std::shared_ptr<rfcomm_task_async_send_spp_data> task;
    task = std::make_shared<rfcomm_task_async_send_spp_data>();
    task->port = a_port;
    task->address = a_address;
    task->port_local_inited = a_local_inited;
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
    if( a_uuids.empty() )
    {
        auto spp_uuid = uuid::from_16bit( sdp_service_uuid::serial_port );
        a_uuids.push_back( spp_uuid );
    }

    bool found = false;
    for( auto& ele : m_local_spp_services )
    {
        if( ele.uuids == a_uuids )
        {
            LogUtilError() << "Using already exsited spp service info";
            found = true;
            a_registered_callback( ele.port_number );
            break;
        }
    }

    if( found )
    {
        return;
    }

    local_spp_service_info info;
    info.port_number = 0;
    info.sdp_service_recode_handle = 0;
    info.service_name = a_name;
    info.uuids = a_uuids;
    m_local_spp_services.push_back( info );

    rfcomm_port_callback_block cb;
    cb.port_number = 0;
    cb.is_local_port = true;
    cb.handle_module = s_spp_module_name;
    cb.connection_changed_callback = std::bind( &spp_module::handle_connection_changed,
        this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, false );
    cb.connection_request_callback = std::bind( &spp_module::handle_connection_request,
        this, std::placeholders::_1, std::placeholders::_2 );
    cb.data_callback = std::bind( &spp_module::handle_sdu, this, std::placeholders::_1,
        std::placeholders::_2, std::placeholders::_3,
        std::placeholders::_4, std::placeholders::_5, false );

    auto tsk = std::make_shared<rfcomm_task_port_callback_operation>();
    tsk->to_register = true;
    tsk->port_callback = cb;
    tsk->set_source_module( get_name() );
    tsk->callback_module = get_name();
    tsk->register_result_callback = std::bind( &spp_module::handle_rfcomm_port_registered, this,
        a_name, std::placeholders::_1, a_uuids, a_registered_callback );
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

