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

#include "framework\log_util.h"
#include "framework\framework_event.h"
#include "framework\framework_manager.h"

#include "..\l2cap\l2cap_module.h"
#include "..\l2cap\l2cap_common.h"

#include "..\common\acl_connections_db.h"

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
    std::shared_ptr<rfcomm_module::rfcomm_task> detail_tsk;
    detail_tsk = std::static_pointer_cast<rfcomm_module::rfcomm_task>( a_task );
    if( !detail_tsk )
    {
        LogUtilError() << "Unknown task type";
        return;
    }

    std::shared_ptr<rfcomm_multipexer> mulx;
    switch( detail_tsk->m_type )
    {
    case rfcomm_task_type::accept_coming_connection_request:
        mulx = find_multipexer_by_remote( detail_tsk->address );
        if( mulx )
        {
            mulx->accept_coming_connection_request( detail_tsk->port );
        }
        else
        {
            LogUtilError() << "No multipexer for address: " << detail_tsk->address.to_string();
        }
        break;
    case rfcomm_task_type::reject_coming_connection_request:
        mulx = find_multipexer_by_remote( detail_tsk->address );
        if( mulx )
        {
            mulx->accept_coming_connection_request( detail_tsk->port, false );
        }
        else
        {
            LogUtilError() << "No multipexer for address: " << detail_tsk->address.to_string();
        }
        break;
    case rfcomm_task_type::disconnect_specified_port:
        mulx = find_multipexer_by_remote( detail_tsk->address );
        if( mulx )
        {
            mulx->disconnect( detail_tsk->port, detail_tsk->port_on_local );
        }
        else
        {
            LogUtilError() << "No multipexer for address: " << detail_tsk->address.to_string();
        }
        break;
    case rfcomm_task_type::async_send_spp_data:
        mulx = find_multipexer_by_remote( detail_tsk->address );
        if( mulx )
        {
            mulx->send_port_user_data( detail_tsk->port, detail_tsk->port_on_local, std::move( detail_tsk->spp_data ) );
        }
        else
        {
            LogUtilError() << "No multipexer for address: " << detail_tsk->address.to_string();
        }
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

void rfcomm_module::register_callback( rfcomm_port_callback_block a_callback )
{
    if( 0x00 == a_callback.port_number )
    {
        LogUtilError() << "Cannot register callback for port 0( multipexer port)";
        return;
    }

    std::lock_guard<std::shared_mutex>locker( m_mutex );
    for( auto& ele : m_port_callbacks )
    {
        if( ele->port_number == a_callback.port_number &&
            ele->local_inited == a_callback.local_inited )
        {
            LogUtilError() << "port callback for port: " << static_cast<uint32_t>(a_callback.port_number)
                << std::boolalpha << ", local inited: " << a_callback.local_inited
                << ", already registered";
            return;
        }
    }

    m_port_callbacks.push_back( std::make_shared<rfcomm_port_callback_block>( a_callback ) );
}

void rfcomm_module::handle_connect_request( std::shared_ptr<connection_request> const& a_request )
{
    /* TODO check the connection state first.If we already connected with the remote device,
    * We need reject such connection request.
    */

    auto acl_connections = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has_address] = acl_connections->get_address( a_request->m_acl_handle );
    if( !has_address )
    {
        LogUtilError() << "the acl connection db has not record for acl handle: " << a_request->m_acl_handle;
        return;
    }

    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_remote( address );
    if( mulx )
    {
        LogUtilInfo() << "We already have a rfcomm multupexer for remote device: " << address.to_string()
            << ", reject for this one.";
        std::shared_ptr<l2cap_task_reject_channle_connection_req> task;
        task = std::make_shared<l2cap_task_reject_channle_connection_req>();
        task->set_source_module( get_name() );
        task->m_connect_request = a_request;
        task->m_reject_reason = connection_req_result::connection_refused_no_resource;
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        return;
    }

    // TODO check the encryption state, reject if not encryptied

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
    mulx->set_remote_device( address );
    mulx->start();
    mulx->handle_channel_connection_request( a_request );
    mulx->set_rfcomm_port_callback_finder( std::bind( &rfcomm_module::find_port_callback, this,
        std::placeholders::_1, std::placeholders::_2 ) );
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
        m_working_multipexer.emplace_back( mulx );
        mulx->set_local_cid( a_local_cid );
        mulx->start();
        mulx->set_rfcomm_port_callback_finder( std::bind( &rfcomm_module::find_port_callback, this,
            std::placeholders::_1, std::placeholders::_2 ) );
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
    uint8_t* p_l2cap = a_hci->m_buffer.data();
    uint16_t local_cid = le_to_host16( p_l2cap + 6 );
    std::shared_ptr<rfcomm_multipexer> mulx = find_multipexer_by_local_cid( local_cid );
    if( mulx )
    {
        mulx->handle_sdu( a_hci );
    }
    else
    {
        LogUtilError() << "no such multipexer entity for channel id: " << local_cid;
    }
}

std::shared_ptr<rfcomm_multipexer> rfcomm_module::find_multipexer_by_local_cid( uint16_t a_local_cid )
{
    for( auto& ele : m_working_multipexer )
    {
        if( ele->get_local_cid() == a_local_cid )
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

std::shared_ptr<rfcomm_port_callback_block> rfcomm_module::find_port_callback( uint8_t a_port, bool a_local_inited )
{
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    for( auto& ele : m_port_callbacks )
    {
        if( ele->port_number == a_port &&
            ele->local_inited == a_local_inited )
        {
            return ele;
        }
    }
    return nullptr;
}

}

