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

#include "rfcomm_module.h"

#include "framework/executable_task.h"
#include "framework/framework_event.h"
#include "framework/framework_manager.h"
#include "framework/log_util.h"
#include "framework/utils.h"

#include "../l2cap/l2cap_module.h"
#include "../l2cap/l2cap_common.h"
#include "sdp/sdp_task.h"
#include "internal/internal_rfcomm_task.h"

#include "../common/acl_connections_db.h"

#include "endian_convert.h"

namespace bluetooth
{
using namespace framework;

rfcomm_module::rfcomm_module()
{
    set_name( s_rfcomm_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
}

void rfcomm_module::initialize()
{
    rfcomm_task::s_rfcomm_task_type_id = framework_manager::get_instance().register_task_type( 1 );

    auto mod = framework_manager::get_instance().get_module_manager().get_module
        ( l2cap_module::s_l2cap_module_name );
    auto l2cap_mod = static_pointer_cast< l2cap_module >( mod );

    l2cap_callbacks l2cap_cbs;
    l2cap_cbs.m_handle_module = get_name();
    l2cap_cbs.m_coming_connection_callback =
        std::bind( &rfcomm_module::handle_connect_request, this, std::placeholders::_1 );
    l2cap_cbs.m_coming_config_callback =
        std::bind( &rfcomm_module::handle_config_request, this, std::placeholders::_1 );
    l2cap_cbs.m_channel_state_changed_callback =
        std::bind( &rfcomm_module::handle_connection_state_changed, this, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5 );
    l2cap_cbs.m_channel_sdu_callback = std::bind( &rfcomm_module::handle_sdu, this, std::placeholders::_1 );
    l2cap_cbs.m_coming_config_rsp_callback = std::bind( &rfcomm_module::handle_config_response, this, std::placeholders::_1 );

    l2cap_mod->register_callback( static_cast< uint16_t >( defined_l2cap_psm::rfcomm ), l2cap_cbs );
}

void rfcomm_module::deinitialize()
{

}

void rfcomm_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    if( uint16_t( a_task->get_task_type() ) != rfcomm_task::s_rfcomm_task_type_id )
    {
        LogUtilWarning() << "Task type not match: "
            << static_cast<uint16_t>( a_task->get_task_type() )
            << " != "
            << rfcomm_task::s_rfcomm_task_type_id
            << "from: " << a_task->get_position();
        return;
    }

    std::shared_ptr<rfcomm_task> detail_tsk;
    detail_tsk = std::static_pointer_cast<rfcomm_task>( a_task );
    if( !detail_tsk )
    {
        LogUtilError() << "Unknown task type";
        return;
    }

    std::shared_ptr<rfcomm_multipexer> mulx;
    switch( detail_tsk->m_type )
    {
    case rfcomm_task_type::accept_coming_connection_request:
        {
            auto rfc_detail_tsk = std::static_pointer_cast<rfcomm_task_accept_coming_connection_request>( detail_tsk );
            mulx = find_multipexer_by_remote( rfc_detail_tsk->address );
            if( mulx )
            {
                mulx->do_accept_coming_connection_request( rfc_detail_tsk->port );
            }
            else
            {
                LogUtilError() << "No multipexer for address: " << rfc_detail_tsk->address.to_string();
            }
        }
        break;
    case rfcomm_task_type::reject_coming_connection_request:
        {
            auto rfc_detail_tsk = std::static_pointer_cast<rfcomm_task_reject_coming_connection_request>( detail_tsk );
            mulx = find_multipexer_by_remote( rfc_detail_tsk->address );
            if( mulx )
            {
                mulx->do_accept_coming_connection_request( rfc_detail_tsk->port, false );
            }
            else
            {
                LogUtilError() << "No multipexer for address: " << rfc_detail_tsk->address.to_string();
            }
        }
        break;
    case rfcomm_task_type::disconnect_specified_port:
        {
            auto rfc_detail_tsk = std::static_pointer_cast<rfcomm_task_disconnect_specified_port>( detail_tsk );
            mulx = find_multipexer_by_remote( rfc_detail_tsk->address );
            if( mulx )
            {
                mulx->disconnect_port( rfc_detail_tsk->port, rfc_detail_tsk->is_local_inited_port_connection );
            }
            else
            {
                LogUtilError() << "No multipexer for address: " << rfc_detail_tsk->address.to_string();
            }
        }
        break;
    case rfcomm_task_type::async_send_spp_data:
        {
            auto rfc_detail_tsk = std::static_pointer_cast<rfcomm_task_async_send_spp_data>( detail_tsk );
            mulx = find_multipexer_by_remote( rfc_detail_tsk->address );
            if( mulx )
            {
                mulx->send_port_user_data
                    (
                    rfc_detail_tsk->port,
                    rfc_detail_tsk->port_local_inited,
                    std::move( rfc_detail_tsk->spp_data )
                    );
            }
            else
            {
                LogUtilError() << "No multipexer for address: " << rfc_detail_tsk->address.to_string();
            }
        }
        break;
    case rfcomm_task_type::connect_specified_port:
        {
            connect_specific_port( std::static_pointer_cast<rfcomm_task_connect_port>( detail_tsk ) );
        }
        break;
    case rfcomm_task_type::remove_specific_port:
        {
            auto rfc_detail_tsk = std::static_pointer_cast<rfcomm_task_remove_specific_port>( detail_tsk );
            mulx = find_multipexer_by_remote( rfc_detail_tsk->address );
            if( mulx )
            {
                mulx->remove_port_connection( rfc_detail_tsk->port, rfc_detail_tsk->local_inited );
                if( mulx->get_worked_ports_count() == 0u )
                {
                    auto mod = framework_manager::get_instance().get_module_manager()
                        .get_module( timer_module::s_timer_module_name );
                    auto timer_module_ = std::static_pointer_cast<timer_module>( mod );
                    std::function<void()> _expire_callback = std::bind(
                        &rfcomm_module::shutdown_mux_l2cap_if_idle, this, mulx );
                    uint32_t random_delay = framework::get_real_random( 1, 10 );
                    timer_module_->register_once_timer
                        (
                        _expire_callback,
                        std::chrono::seconds( random_delay ),
                        "rfc conflicts timer",
                        rfcomm_module::s_rfcomm_module_name
                        );
                }
            }
            else
            {
                LogUtilDebug() << "No multipexer for address: " << rfc_detail_tsk->address.to_string();
            }
        }
        break;
    case rfcomm_task_type::port_callback_operation:
        do_port_callback_operation( std::static_pointer_cast<rfcomm_task_port_callback_operation>( detail_tsk ) );
        break;
    default:
        LogUtilError() << "Task type ignored: " << static_cast< uint16_t >( detail_tsk->m_type );
        break;
    }
}

void rfcomm_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    switch( a_event->m_event_type )
    {
    case event_type::power_on:
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case event_type::power_off:
        set_power_status( abstract_module::powering_status::power_off );
        break;
    case event_type::power_status_changed:
        break;
    default:
        break;
    }
}

bool rfcomm_module::register_callback( rfcomm_port_callback_block a_callback, uint8_t& a_port_used )
{
    LogUtilDebug() << "register port callback for port: " << (uint16_t)a_callback.port_number
        << ( a_callback.is_local_port ? " local port" : " remote port" );
    a_port_used = 0;
    uint8_t port_used = a_callback.port_number;
    if( 0x00 == a_callback.port_number || port_register_as_local( a_callback.port_number ) )
    {
        LogUtilDebug() << "allocate a new port for this";
        port_used = get_next_available_port_number();
        if( port_used == 0x00 )
        {
            LogUtilDebug() << "allocate a new port faild";
            return false;
        }
    }

    for( auto& ele : m_port_callbacks )
    {
        if( ele->port_number == port_used )
        {
            if( ele->is_local_port && a_callback.is_local_port )
            {
                LogUtilError() << "port callback for port: " << static_cast<uint32_t>( port_used )
                    << std::boolalpha << ", local inited: " << a_callback.is_local_port
                    << ", already registered";
                return false;
            }
            else
            {
                if( !ele->is_local_port && !a_callback.is_local_port )
                {
                    if( ele->remote_address == a_callback.remote_address )
                    {
                        LogUtilError() << "port callback for port: " << static_cast<uint32_t>( port_used )
                            << std::boolalpha << ", is local port: " << a_callback.is_local_port << ", remote device: "
                            << a_callback.remote_address.to_string()
                            << ", already registered";
                        return false;
                    }
                }
            }
        }
    }

    a_callback.port_number = port_used;
    m_port_callbacks.push_back( std::make_shared<rfcomm_port_callback_block>( a_callback ) );
    a_port_used = port_used;
    return true;
}

void rfcomm_module::handle_connect_request( std::shared_ptr<connection_request> const& a_request )
{
    auto acl_connections = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has_address] = acl_connections->get_address( a_request->m_acl_handle );
    if( !has_address )
    {
        LogUtilError() << "the acl connection db has not record for acl handle: " << a_request->m_acl_handle;
        return;
    }

    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_acl_handle( a_request->m_acl_handle );
    if( mulx )
    {
        auto current_state = mulx->get_multipexer_state();
        LogUtilInfo() << "We already have a rfcomm multupexer for remote device: " << address.to_string()
            << ", state: " << current_state << ", reject for this one.";
        std::shared_ptr<l2cap_task_reject_channle_connection_req> task;
        task = std::make_shared<l2cap_task_reject_channle_connection_req>();
        task->set_source_module( get_name() );
        task->m_connect_request = a_request;
        task->m_reject_reason = connection_req_result::connection_refused_no_resource;
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

        if( rfcomm_multipexer_state_type::wait_conn_cnf == current_state )
        {
            LogUtilDebug() << "The establishment of the underlying L2CAP channel for RFCOMM may"
                " encounter conflicts, which we need to handle.";
            auto pending_tsks = mulx->get_all_pending_connection_task();

            uint32_t life_timer_id = 0;
            auto mod = framework_manager::get_instance().get_module_manager().get_module
                ( timer_module::s_timer_module_name );
            auto timer_module_ = std::static_pointer_cast<timer_module>( mod );
            uint32_t random_delay = framework::get_real_random( 2, 20 );
            std::function<void()> _expire_callback = std::bind(
                &rfcomm_module::check_connection_conflict, this, pending_tsks, address );
            life_timer_id = timer_module_->register_once_timer
                (
                _expire_callback,
                std::chrono::milliseconds( random_delay ),
                "rfc conflicts timer",
                rfcomm_module::s_rfcomm_module_name
                );
        }
        return;
    }

    LogUtilInfo() << "Allocate a new refcomm multipexer to handle the connection request: " << address.to_string();

    if( !m_idle_multipexer.empty() )
    {
        mulx = m_idle_multipexer.front();
        m_idle_multipexer.erase( m_idle_multipexer.begin() );
    }
    else
    {
        mulx = std::make_shared<rfcomm_multipexer>();
    }

    m_working_multipexer.emplace_back( mulx );
    mulx->start();
    mulx->set_remote_device( address );
    mulx->set_acl_handle( a_request->m_acl_handle );
    mulx->handle_channel_connection_request( a_request );
    mulx->set_rfcomm_port_callback_finder( std::bind( &rfcomm_module::find_port_callback, this,
        std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
}

void rfcomm_module::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    auto acl_connections = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has_address] = acl_connections->get_address( a_request->m_acl_handle );
    if( !has_address )
    {
        LogUtilError() << "the acl connection db has not record for acl handle: " << a_request->m_acl_handle;
        return;
    }

    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_remote( address );
    if( !mulx )
    {
        LogUtilError() << "No multipexer for device: " << address.to_string();
        return;
    }

    mulx->handle_config_request( a_request );

}

void rfcomm_module::handle_config_response( std::shared_ptr<l2cap_config_response> const& a_reponse )
{
    auto acl_connections = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has_address] = acl_connections->get_address( a_reponse->m_acl_handle );
    if( !has_address )
    {
        LogUtilError() << "the acl connection db has not record for acl handle: " << a_reponse->m_acl_handle;
        return;
    }

    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_remote( address );
    if( !mulx )
    {
        LogUtilError() << "no such multipexer entity for remote device: " << address.to_string();
        return;
    }

    mulx->handle_config_response( a_reponse );
}

void rfcomm_module::handle_connection_state_changed
    (
    bluetooth_address a_address,
    uint16_t a_local_cid,
    uint16_t a_remote_cid,
    l2cap_channel_state_type a_state,
    l2cap_channel_close_reason a_reason
    )
{
    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_remote( a_address );
    if( !mulx )
    {
        LogUtilError() << "no such multipexer entity for channel id: " << a_local_cid << ". state: "
            << a_state;
        mulx = std::make_shared<rfcomm_multipexer>();
        mulx->start();
        m_working_multipexer.emplace_back( mulx );
        mulx->set_local_cid( a_local_cid );
        mulx->set_remote_device( a_address );
        mulx->set_rfcomm_port_callback_finder( std::bind( &rfcomm_module::find_port_callback, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
    }

    mulx->set_local_cid( a_local_cid );
    if( mulx->get_acl_handle() == 0 )
    {
        auto acl_db = framework_manager::get_instance().get_info_manager()
            .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
        auto [acl, status] = acl_db->get_handle( a_address );
        mulx->set_acl_handle( acl );
    }

    mulx->handle_connection_state_changed( a_address, a_local_cid, a_remote_cid, a_state );

    if( l2cap_channel_state_type::close_state == a_state )
    {
        mulx->clear();
        m_idle_multipexer.push_back( mulx );
        for( auto it = m_working_multipexer.begin(); it != m_working_multipexer.end(); )
        {
            if( (*it) == mulx )
            {
                it = m_working_multipexer.erase( it );
            }
            else
            {
                ++it;
            }
        }
    }
}

void rfcomm_module::handle_sdu( std::shared_ptr<hci_data> a_hci )
{
    if( a_hci->m_type != uart_hci_type::acl_type )
    {
        LogUtilError( "not ACL type HCI packet" );
        return;
    }

    l2cap_header _l2cap_header;
    if( _l2cap_header.header_size() > a_hci->m_buffer.size() )
    {
        LogUtilError( "Invalid packet" );
        return;
    }
    _l2cap_header.parse_from_raw_data( a_hci->m_buffer.data(), a_hci->m_buffer.size() );

    uint16_t acl_handle = _l2cap_header.get_acl_handle();
    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_acl_handle( acl_handle );
    if( mulx )
    {
        mulx->handle_sdu( a_hci );
    }
    else
    {
        LogUtilDebug() << "no such multipexer entity for ACL handle: " << acl_handle
            << ", try to use address to find.";
        auto acl_connections = framework_manager::get_instance().get_info_manager()
            .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
        auto [address, has_address] = acl_connections->get_address( acl_handle );
        if( !has_address )
        {
            LogUtilError() << "the acl connection db has not record for acl handle: " << acl_handle;
            return;
        }

        std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_remote( address );
        if( !mulx )
        {
            LogUtilError() << "No multipexer for device: " << address.to_string();
            return;
        }
        mulx->set_acl_handle( acl_handle );
        mulx->handle_sdu( a_hci );
    }
}

void rfcomm_module::connect_specific_port( std::shared_ptr<rfcomm_task_connect_port> a_task )
{
    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_remote( a_task->address );
    if( !mulx )
    {
        if( !m_idle_multipexer.empty() )
        {
            mulx = m_idle_multipexer.front();
            m_idle_multipexer.erase( m_idle_multipexer.begin() );
        }
        else
        {
            mulx = std::make_shared<rfcomm_multipexer>();
            mulx->set_rfcomm_port_callback_finder( std::bind( &rfcomm_module::find_port_callback, this,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
        }

        m_working_multipexer.emplace_back( mulx );
        mulx->start();
        mulx->set_local_init_connect();
        mulx->set_remote_device( a_task->address );
        mulx->set_rfcomm_port_callback_finder( std::bind( &rfcomm_module::find_port_callback, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
    }

    if( mulx )
    {
        mulx->connect( a_task );
        return;
    }
    else
    {
        LogUtilDebug() << "cannot create rfcomm multiplexor";
    }

}

void rfcomm_module::do_port_callback_operation( std::shared_ptr<rfcomm_task_port_callback_operation> a_task )
{
    if( a_task->to_register )
    {
        bool result = false;
        uint8_t port_number = 0;
        result = register_callback( a_task->port_callback, port_number );
        if( a_task->register_result_callback )
        {
            std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
            task->set_fun( std::bind( a_task->register_result_callback, port_number ), a_task->callback_module );
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        }
    }
    else
    {
        uint8_t port = a_task->port_callback.port_number;
        bool is_local_port = a_task->port_callback.is_local_port;
        bluetooth_address peer_addr = a_task->port_callback.remote_address;
        bool to_schedule_calback = false;
        bool result = false;
        for( auto it = m_port_callbacks.begin(); it != m_port_callbacks.end(); ++it)
        {
            std::shared_ptr<rfcomm_port_callback_block> ele = *it;
            if( ele->port_number != port )
            {
                continue;
            }

            if( is_local_port )
            {
                if( ele->is_local_port )
                {
                    LogUtilDebug() << "unregister port callback for local port: "
                        << (uint16_t)( port );
                    it = m_port_callbacks.erase( it );
                    to_schedule_calback = true;
                    result = true;
                    break;
                }
                else
                {
                    LogUtilError() << "did not register callback for local port: " << uint32_t( port );
                    break;
                }
            }

            if( ele->remote_address == peer_addr )
            {
                LogUtilDebug() << "unregister port callback for remote port: "
                    << (uint16_t)( port ) << ", on device: " << peer_addr.to_string();
                it = m_port_callbacks.erase( it );
                to_schedule_calback = true;
                result = true;
                break;
            }
        }


        if( to_schedule_calback && a_task->register_result_callback )
        {
            std::shared_ptr<executable_task> task;
            task = std::make_shared<executable_task>();
            task->set_fun( std::bind( a_task->register_result_callback, result ? port : 0 ), a_task->callback_module );
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        }
    }
}

void rfcomm_module::check_connection_conflict
    (
    std::vector<std::shared_ptr<rfcomm_task_connect_port>> a_pending_tsk,
    bluetooth_address a_remote_addr
    )
{
    auto mulx = find_multipexer_by_remote( a_remote_addr );
    if( mulx )
    {
        LogUtilDebug() << "already have a rfcomm multipexser, nothing work to do to fix connection conflict";
        return;
    }

    LogUtilDebug() << "let's fix the connection conflict now";
    for( auto& tsk : a_pending_tsk )
    {
        connect_specific_port( tsk );
    }
}

void rfcomm_module::shutdown_mux_l2cap_if_idle( std::shared_ptr<rfcomm_multipexer> a_mulx )
{
    if( a_mulx->get_worked_ports_count() > 0 )
    {
        return;
    }

    bool found = false;
    for( auto& ele : m_working_multipexer )
    {
        if( ele.get() == a_mulx.get() )
        {
            found = true;
            break;
        }
    }

    if( !found )
    {
        return;
    }

    if( a_mulx->get_current_state_id() == (int)rfcomm_multipexer_state_type::connected )
    {
        LogUtilDebug() << "going to drop this multipexer connection";
        auto event_ = std::make_shared<rfcomm_multipexer_event>();
        event_->m_type = rfcomm_event_type::disconnect;
        a_mulx->handle_event( event_ );
    }
}

std::shared_ptr<rfcomm_multipexer> rfcomm_module::find_multipexer_by_acl_handle
    (
    uint16_t a_acl_handle
    )
{
    for( auto& ele : m_working_multipexer )
    {
        if( a_acl_handle == ele->get_acl_handle() )
        {
            return ele;
        }
    }

    return nullptr;
}

std::shared_ptr<rfcomm_multipexer> rfcomm_module::find_multipexer_by_remote( bluetooth_address const& a_address )
{
    for( auto& ele : m_working_multipexer )
    {
        if( ele->get_remote_device() == a_address )
        {
            return ele;
        }
    }

    return nullptr;
}

std::shared_ptr<rfcomm_port_callback_block> rfcomm_module::find_port_callback
    (
    uint8_t a_port,
    bool a_is_local_port,
    bluetooth_address a_peer_address
    )
{
    for( auto& ele : m_port_callbacks )
    {
        if( ele->port_number != a_port )
        {
            continue;
        }

        if( a_is_local_port )
        {
            if( ele->is_local_port )
            {
                return ele;
            }
            else
            {
                LogUtilError() << "did not register callback for local port: " << uint32_t( a_port );
                return nullptr;
            }
        }

        if( ele->remote_address == a_peer_address )
        {
            return ele;
        }
    }

    LogUtilError() << "no callback registered for port " << uint32_t( a_port )
        << ", address " << a_peer_address.to_string() << ", "
        << ( a_is_local_port ? "local port" : "remote port" );
    return nullptr;
}

uint8_t rfcomm_module::get_next_available_port_number()
{
    uint8_t port = framework::get_real_random( 1, 30 );
    bool found = port_register_as_local( port );
    if( !found )
    {
        return port;
    }

    port = 0;
    for( int i = 1; i < 31; ++i )
    {
        found = port_register_as_local( i );
        if( !found )
        {
            port = i;
            break;
        }
    }

    return port;
}

bool rfcomm_module::port_register_as_local( uint8_t a_port )
{
    for( auto& cb : m_port_callbacks )
    {
        if( !cb->is_local_port )
        {
            continue;
        }

        if( cb->port_number == a_port )
        {
            return true;
        }
    }

    return false;
}

}

