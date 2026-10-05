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

#include "sdp_manager.h"

#include "framework/log_util.h"
#include "framework/module_manager.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"
#include "framework/framework_event.h"

#include "endian_convert.h"

#include "common/acl_connections_db.h"
#include "l2cap/l2cap_module.h"
#include "l2cap/l2cap_common.h"
#include "sdp/sdp_module.h"

namespace bluetooth
{

using namespace framework;

void sdp_manager::init()
{
    m_local_service.set_send_packet_fun( std::bind( &sdp_manager::send_packet, this,
        std::placeholders::_1, std::placeholders::_2 ) );
}

void sdp_manager::handle_sdp_connect_request( std::shared_ptr<connection_request> const& a_request )
{
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has] = acl_db->get_address( a_request->m_acl_handle );

    if( !has )
    {
        LogUtilError() << "No acl handle: " << a_request->m_acl_handle;
        return;
    }

    bool already_connected = false;
    for( auto& ele : m_connections )
    {
        if( ele->m_address == address &&
            ele->get_connection_status() != connection_status::disconnected
            )
        {
            already_connected = true;
            break;
        }
    }

    if( !already_connected )
    {
        std::shared_ptr<l2cap_task_accept_channle_connection_req> acp_con_task;
        acp_con_task = std::make_shared<l2cap_task_accept_channle_connection_req>();
        acp_con_task->set_source_module( sdp_module::s_sdp_module_name );
        acp_con_task->m_connect_request = a_request;
        auto conn_ = find_connection( address );
        if( !conn_ )
        {
            LogUtilDebug() << "Make one due to no sdp connection control block for device: " << address.to_string();

            std::shared_ptr<sdp_connection> sdp_conn = std::make_shared<sdp_connection>();
            sdp_conn->m_address = address;
            sdp_conn->set_connection_status( connection_status::connecting );
            sdp_conn->set_acl_handle( a_request->m_acl_handle );
            m_connections.push_back( sdp_conn );
            conn_ = sdp_conn;
        }

        framework_manager::get_instance().get_thread_manager().post_task( acp_con_task, framework::source_here );

        if( !conn_->get_config_local_req_sent() )
        {
            config_local_channel( a_request->m_acl_handle, a_request->m_source_cid );
            conn_->set_config_local_req_sent( true );
        }
    }
    else
    {

        std::shared_ptr<l2cap_task_reject_channle_connection_req> task;
        task = std::make_shared<l2cap_task_reject_channle_connection_req>();
        task->set_source_module( sdp_module::s_sdp_module_name );
        task->m_connect_request = a_request;
        task->m_reject_reason = connection_req_result::connection_refused_no_resource;
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        LogUtilDebug() << "We already connected to device: " << address.to_string();
    }

}

void sdp_manager::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    /**
     * TODO check the configuration request can be accpeted or not.
     */

    std::shared_ptr<l2cap_task_accept_channel_config_req> task;
    task = std::make_shared<l2cap_task_accept_channel_config_req>();
    task->set_source_module( sdp_module::s_sdp_module_name );
    task->m_config_request = a_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has] = acl_db->get_address( a_request->m_acl_handle );

    if( !has )
    {
        LogUtilError() << "No acl handle: " << a_request->m_acl_handle;
        return;
    }

    auto conn_ = find_connection( address );
    if( !conn_ )
    {
        LogUtilError() << "Make one due to no sdp connection control block for device: " << address.to_string();

        std::shared_ptr<sdp_connection> sdp_conn = std::make_shared<sdp_connection>();
        sdp_conn->m_address = address;
        sdp_conn->set_connection_status( connection_status::connecting );
        sdp_conn->set_acl_handle( a_request->m_acl_handle );
        m_connections.push_back( sdp_conn );
        conn_ = sdp_conn;
    }

    conn_->set_config_remote_req_received( true );
    conn_->set_config_remote_rsp_sent( true );

    if( 0x0000 == conn_->m_remote_cid )
    {
        conn_->m_remote_cid = a_request->m_source_cid;
    }

    if( !conn_->get_config_local_req_sent() )
    {
        config_local_channel( a_request->m_acl_handle, conn_->m_remote_cid );
        conn_->set_config_local_req_sent( true );
    }
}

void sdp_manager::config_local_channel
    (
    uint16_t a_acl_handle,
    uint16_t a_remote_cid
    )
{
    /**
    * TODO: Upper layer shall send L2CAP CONFIGURATION_REQ once channel transitions
    * to wait_config state or other suitable state if configuration has not been performed.
    */
    std::vector<channel_config_option> channel_cfg_options;
    channel_config_option cfg;
    cfg.m_type = channel_config_option_type::mtu;
    cfg.m_option.m_mtu = 1024;
    channel_cfg_options.push_back( cfg );

    std::shared_ptr<l2cap_config_local_channel_request> cfg_request;
    cfg_request = std::make_shared<l2cap_config_local_channel_request>();
    cfg_request->m_options = std::move( channel_cfg_options );
    cfg_request->m_acl_handle = a_acl_handle;
    cfg_request->m_remote_cid = a_remote_cid;

    std::shared_ptr<l2cap_task_request_config_local_channel> task;
    task = std::make_shared<l2cap_task_request_config_local_channel>();
    task->set_source_module( sdp_module::s_sdp_module_name );
    task->m_config_local = cfg_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void sdp_manager::handle_connection_state_changed
    (
    bluetooth_address a_address,
    uint16_t a_local_cid,
    uint16_t a_remote_cid,
    l2cap_channel_state_type a_state,
    l2cap_channel_close_reason a_reason
    )
{
    auto sdp_connection_ = find_connection( a_address );
    if( !sdp_connection_ )
    {
        if( a_state != l2cap_channel_state_type::close_state )
        {
            auto acl_db = framework_manager::get_instance().get_info_manager()
                .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
            auto [acl_handle, has] = acl_db->get_handle( a_address );
            if( has )
            {
                sdp_connection_ = std::make_shared<sdp_connection>();
                sdp_connection_->m_address = a_address;
                sdp_connection_->set_acl_handle( acl_handle );
                sdp_connection_->set_local_cid( a_local_cid );
                m_connections.push_back( sdp_connection_ );
            }
            else
            {
                LogUtilError() << "No acl handle for device: " << a_address.to_string();
                return;
            }
        }
        else
        {
            return;
        }
    }

    sdp_connection_->set_local_cid( a_local_cid );
    sdp_connection_->set_remote_cid( a_remote_cid );
    if( sdp_connection_->get_acl_handle() == 0x00 )
    {
        auto acl_db = framework_manager::get_instance().get_info_manager()
            .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
        auto [acl_handle, has] = acl_db->get_handle( a_address );
        if( has )
        {
            sdp_connection_->set_acl_handle( acl_handle );
        }
        else
        {
            LogUtilError() << "No acl handle for device: " << a_address.to_string();
            remove_connection( a_address );
            return;
        }
    }

    switch( a_state )
    {
    case bluetooth::l2cap_channel_state_type::close_state:
        sdp_connection_->set_connection_status( connection_status::disconnected );
        remove_connection( a_address );
        break;
    case bluetooth::l2cap_channel_state_type::open:
        sdp_connection_->set_connection_status( connection_status::connected );
        for( auto it = m_pending_reqs.begin(); it != m_pending_reqs.end(); )
        {
            std::shared_ptr<sdp_protocol_base> req = *it;
            if( req->m_remote_device == a_address )
            {
                send_packet( req, a_address );
                it = m_pending_reqs.erase( it );
                break;
            }
            else
            {
                ++it;
            }
        }
        break;
    case l2cap_channel_state_type::wait_config:
        if( !sdp_connection_->get_config_local_req_sent() )
        {
            auto acl_db = framework_manager::get_instance().get_info_manager()
                .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
            auto [acl_handle, has] = acl_db->get_handle( a_address );

            if( has )
            {
                config_local_channel( acl_handle, a_remote_cid );
                sdp_connection_->set_config_local_req_sent( true );
            }
            else
            {
                LogUtilError() << "No acl handle for device: " << a_address.to_string();
            }
        }
        break;
    default:
        sdp_connection_->set_connection_status( connection_status::connecting );
        break;
    }

}

void sdp_manager::handle_sdu( std::shared_ptr<hci_data> a_sdu )
{
    sdp_header _sdp_header;
    if( a_sdu->m_buffer.size() < _sdp_header.l2cap_header::header_size() )
    {
        LogUtilError() << "cannot parsing completed l2cap header, so ignore this packet";
        return;
    }

    l2cap_header _l2cap_header;
    _l2cap_header.parse_from_raw_data( a_sdu->m_buffer.data(), a_sdu->m_buffer.size() );
    auto sdp_con = find_connection( _l2cap_header.get_acl_handle() );
    if( !sdp_con )
    {
        LogUtilError() << "No sdp connection control block for local cid: "
            << _l2cap_header.get_channel_id()
            << " acl handle: " << _l2cap_header.get_acl_handle();
        return;
    }

    if( !verify_received_packet( _sdp_header, a_sdu ) )
    {
        send_error_rsp( _l2cap_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return;
    }

    sdp_pdu_id pdu_id = _sdp_header.get_pdu_id();

    uint8_t* _parameter_buffer = a_sdu->m_buffer.data() + _sdp_header.header_size();
    uint16_t _parameter_size = a_sdu->m_buffer.size() - _sdp_header.header_size();
    switch( pdu_id )
    {
    case bluetooth::sdp_pdu_id::sdp_error_rsp:
        {
            auto error_rsp = parse_error_rsp( _sdp_header, _parameter_buffer, _parameter_size );
            if( error_rsp )
            {
                sdp_con->handle_error_rsp( _sdp_header, error_rsp );
            }
        }
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_req:
        {
            auto req = parse_service_search_request( _sdp_header, _parameter_buffer, _parameter_size );
        }
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_rsp:
        {
            auto rsp = parse_service_search_response( _sdp_header, _parameter_buffer, _parameter_size );
        }
        break;
    case bluetooth::sdp_pdu_id::sdp_service_attr_req:
        {
            auto request = parse_service_attribute_request( _sdp_header, _parameter_buffer, _parameter_size );

        }
        break;
    case bluetooth::sdp_pdu_id::sdp_service_attr_rsp:
        {
            auto response = parse_service_attribute_response( _sdp_header, _parameter_buffer, _parameter_size );
        }
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_attr_req:
        {
            auto request = parse_service_search_attribute_request( _sdp_header, _parameter_buffer, _parameter_size );
            m_local_service.handle_service_search_attribute_request( request );
        }
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_attr_rsp:
        {
            auto response = parse_service_search_attribute_response( _sdp_header, _parameter_buffer, _parameter_size );
        }
        break;
    default:
        send_error_rsp( _l2cap_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        break;
    }
}

std::shared_ptr<sdp_error_rsp> sdp_manager::parse_error_rsp
    (
    sdp_header& a_sdp_header,
    uint8_t* a_parameter_buffer,
    uint16_t a_parameter_size
    )
{
    std::shared_ptr<sdp_error_rsp> error_rsp;
    uint8_t* p_buffer = a_parameter_buffer;
    int32_t size_left = a_parameter_size;

    uint16_t error_code = 0;
    if( size_left < 2 )
    {
        LogUtilError() << "Invalid error rsp packet, size left: " << size_left;
        return error_rsp;
    }
    error_code = be_to_host16( p_buffer );

    error_rsp = std::make_shared<sdp_error_rsp>();
    error_rsp->m_error_code = static_cast<sdp_error_code>( error_code );
    error_rsp->m_transaction_id = a_sdp_header.get_transaction_id();
    error_rsp->m_local_cid = a_sdp_header.get_channel_id();
    error_rsp->m_remote_device = find_connection( a_sdp_header.get_acl_handle() )->m_address;
    return error_rsp;
}

std::shared_ptr<sdp_service_search_request> sdp_manager::parse_service_search_request
    (
    sdp_header& a_sdp_header,
    uint8_t* a_parameter_buffer,
    uint16_t a_parameter_size
    )
{
    bool status = false;
    std::shared_ptr<sdp_service_search_request> request;
    uint16_t parameter_size = a_parameter_size;
    uint16_t transaction_id = a_sdp_header.get_transaction_id();
    uint32_t parsed_size = 0;
    uint8_t* p_buffer = a_parameter_buffer;
    uint32_t size_left = parameter_size;
    sdp_data_element element;

    std::vector<sdp_data_element> uuid_elements;
    std::vector<uuid> uuids;
    status = sdp_data_element::parse_from( p_buffer, size_left, parsed_size, element );
    if( !status )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    if( !element.can_as_elements() )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    uuid_elements = element.get_elements();
    for( auto& ele : uuid_elements )
    {
        if( !ele.can_as_uuid() )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
            return request;
        }
        uuids.push_back( ele.get_uuid() );
    }
    if( uuids.size() < 1 || uuids.size() > 12 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    size_left -= parsed_size;
    p_buffer += parsed_size;

    uint16_t max_return_count = 0;
    if( size_left < 2 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    parsed_size = 2;
    max_return_count = be_to_host16( p_buffer );
    size_left -= parsed_size;
    p_buffer += parsed_size;

    std::vector<uint8_t> continue_state;
    uint16_t continue_state_size = 0;
    if( size_left < 1 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    continue_state_size = p_buffer[0];
    if( continue_state_size > 16 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
        return request;
    }
    parsed_size = 1;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    if( continue_state_size > 0 )
    {
        if( size_left < continue_state_size )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
            return request;
        }
        continue_state.resize( continue_state_size );
        memcpy( continue_state.data(), p_buffer, continue_state_size );
    }

    request = std::make_shared<sdp_service_search_request>();
    request->m_matching_uuids = uuids;
    request->m_max_return_count = max_return_count;
    request->m_continue_info = continue_state;
    request->m_transaction_id = transaction_id;
    request->m_local_cid = a_sdp_header.get_channel_id();
    request->m_remote_device = find_connection( a_sdp_header.get_acl_handle() )->m_address;
    return request;
}

std::shared_ptr<sdp_service_search_response> sdp_manager::parse_service_search_response
    (
    sdp_header& a_sdp_header,
    uint8_t* a_parameter_buffer,
    uint16_t a_parameter_size
    )
{
    std::shared_ptr<sdp_service_search_response> rsp;
    uint8_t* p_buffer = a_parameter_buffer;
    uint32_t size_left = a_parameter_size;
    uint32_t parsed_size = 0;
    uint16_t transaction_id = a_sdp_header.get_transaction_id();

    if( size_left < 2 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return rsp;
    }
    uint16_t total_record_count = be_to_host16( p_buffer );
    parsed_size = 2;
    size_left -= parsed_size;
    p_buffer += parsed_size;

    if( size_left < 2 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return rsp;
    }
    uint16_t return_record_count = be_to_host16( p_buffer );
    parsed_size = 2;
    size_left -= parsed_size;
    p_buffer += parsed_size;

    uint32_t record_list_size = return_record_count;
    std::vector<uint32_t> record_handles;
    if( size_left < record_list_size * sizeof( uint32_t ) )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return rsp;
    }
    for( int i = 0; i < record_list_size; ++i )
    {
        uint32_t record_handle = be_to_host32( p_buffer );
        record_handles.push_back( record_handle );
        parsed_size += sizeof( uint32_t );
        size_left -= sizeof( uint32_t );
        p_buffer += sizeof( uint32_t );
    }

    std::vector<uint8_t> continue_state;
    uint16_t continue_state_size = 0;
    if( size_left < 1 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return rsp;
    }
    continue_state_size = p_buffer[0];
    if( continue_state_size > 16 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
        return rsp;
    }
    parsed_size = 1;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    if( continue_state_size > 0 )
    {
        if( size_left < continue_state_size )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
            return rsp;
        }
        continue_state.resize( continue_state_size );
        memcpy( continue_state.data(), p_buffer, continue_state_size );
    }

    rsp = std::make_shared<sdp_service_search_response>();
    rsp->m_total_record_count = total_record_count;
    rsp->m_return_record_count = return_record_count;
    rsp->m_matched_record_handles = record_handles;
    rsp->m_continue_info = continue_state;
    rsp->m_transaction_id = transaction_id;
    rsp->m_local_cid = a_sdp_header.get_channel_id();
    rsp->m_remote_device = find_connection( a_sdp_header.get_acl_handle() )->m_address;

    return rsp;
}

std::shared_ptr<sdp_service_attribute_request> sdp_manager::parse_service_attribute_request
    (
    sdp_header& a_sdp_header,
    uint8_t* a_parameter_buffer,
    uint16_t a_parameter_size
    )
{
    bool status = false;
    std::shared_ptr<sdp_service_attribute_request> request;
    uint16_t parameter_size = a_parameter_size;
    uint16_t transaction_id = a_sdp_header.get_transaction_id();
    uint32_t parsed_size = 0;
    uint8_t* p_buffer = a_parameter_buffer;
    uint32_t size_left = parameter_size;
    sdp_data_element element;

    if( a_parameter_size < 4 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    uint32_t record_handle = be_to_host32( p_buffer );
    parsed_size = 4;
    p_buffer += parsed_size;
    size_left -= parsed_size;

    uint16_t max_attribute_bytes_count = 0;
    if( size_left < 2 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    max_attribute_bytes_count = be_to_host16( p_buffer );
    if( max_attribute_bytes_count < 7 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    parsed_size = 2;
    p_buffer += parsed_size;
    size_left -= parsed_size;

    std::vector<uint16_t> requested_ids;
    std::vector<std::pair<uint16_t, uint16_t>> requested_id_ranges;
    status = sdp_data_element::parse_from( p_buffer, size_left, parsed_size, element );
    if( !status )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    if( !element.can_as_elements() )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    std::vector<sdp_data_element> id_elements = element.get_elements();
    for( auto& ele : id_elements )
    {
        uint16_t id16 = 0;
        uint32_t id32 = 0;
        if( ele.can_as_uint16() )
        {
            id16 = ele.get_uint16_value();
            requested_ids.push_back( id16 );
            continue;
        }
        if( ele.can_as_uint32() )
        {
            id32 = ele.get_uint32_value();
            uint16_t high16 = id32 >> 16;
            uint16_t low16 = id32 & 0xFFFF;
            requested_id_ranges.push_back( std::make_pair( high16, low16 ) );
        }
    }
    p_buffer += parsed_size;
    size_left -= parsed_size;

    std::vector<uint8_t> continue_state;
    uint16_t continue_state_size = 0;
    if( size_left < 1 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    continue_state_size = p_buffer[0];
    if( continue_state_size > 16 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
        return request;
    }
    parsed_size = 1;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    if( continue_state_size > 0 )
    {
        if( size_left < continue_state_size )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
            return request;
        }
        continue_state.resize( continue_state_size );
        memcpy( continue_state.data(), p_buffer, continue_state_size );
    }

    request = std::make_shared<sdp_service_attribute_request>();
    request->m_service_record_handle = record_handle;
    request->m_max_attribute_count = max_attribute_bytes_count;
    request->m_matching_ids = requested_ids;
    request->m_requested_id_ranges = requested_id_ranges;
    request->m_continue_info = continue_state;
    request->m_transaction_id = transaction_id;
    request->m_local_cid = a_sdp_header.get_channel_id();
    request->m_remote_device = find_connection( a_sdp_header.get_acl_handle() )->m_address;

    return request;
}

std::shared_ptr<sdp_service_attribute_response> sdp_manager::parse_service_attribute_response
    (
    sdp_header& a_sdp_header,
    uint8_t* a_parameter_buffer,
    uint16_t a_parameter_size
    )
{
    std::shared_ptr<sdp_service_attribute_response> response;
    bool status = false;
    uint16_t parameter_size = a_parameter_size;
    uint16_t transaction_id = a_sdp_header.get_transaction_id();
    uint32_t parsed_size = 0;
    uint8_t* p_buffer = a_parameter_buffer;
    uint32_t size_left = parameter_size;

    uint32_t attribute_list_size = 0;
    if( size_left < 2 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return response;
    }
    attribute_list_size = be_to_host16( p_buffer );
    parsed_size = 2;
    p_buffer += parsed_size;
    size_left -= parsed_size;

    if( size_left < attribute_list_size )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return response;
    }
    std::vector<sdp_data_element> attribute_list;
    uint32_t max_elements_size_to_parse = 50;
    status = sdp_data_element::parse_elements_from( p_buffer, size_left, max_elements_size_to_parse, attribute_list );
    if( !status )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return response;
    }
    parsed_size = attribute_list_size;
    p_buffer += parsed_size;
    size_left -= parsed_size;

    std::vector<uint8_t> continue_state;
    uint16_t continue_state_size = 0;
    if( size_left < 1 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return response;
    }
    continue_state_size = p_buffer[0];
    if( continue_state_size > 16 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
        return response;
    }
    parsed_size = 1;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    if( continue_state_size > 0 )
    {
        if( size_left < continue_state_size )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
            return response;
        }
        continue_state.resize( continue_state_size );
        memcpy( continue_state.data(), p_buffer, continue_state_size );
    }

    response = std::make_shared<sdp_service_attribute_response>();
    response->m_attribute_list = attribute_list;
    response->m_continue_info = continue_state;
    response->m_attribute_list_byte_count = attribute_list_size;
    response->m_transaction_id = transaction_id;
    response->m_local_cid = a_sdp_header.get_channel_id();
    response->m_remote_device = find_connection( a_sdp_header.get_acl_handle() )->m_address;

    return response;
}

std::shared_ptr<sdp_service_search_attribute_req> sdp_manager::parse_service_search_attribute_request
    (
    sdp_header& a_sdp_header,
    uint8_t*    a_parameter_buffer,
    uint16_t    a_parameter_size
    )
{
    std::shared_ptr<sdp_service_search_attribute_req> request;
    bool status = false;
    uint8_t* p_buffer = a_parameter_buffer;
    uint16_t parameter_size = a_parameter_size;
    uint16_t transaction_id = a_sdp_header.get_transaction_id();
    uint32_t size_left = parameter_size;

    std::vector<uuid> uuids;

    uint32_t parsed_size = 0;
    sdp_data_element element;
    std::vector<sdp_data_element> uuid_elements;
    status = sdp_data_element::parse_from( p_buffer, size_left, parsed_size, element );
    if( !status )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    if( !element.can_as_elements() )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    uuid_elements = element.get_elements();
    for( auto& ele : uuid_elements )
    {
        if( !ele.can_as_uuid() )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
            return request;
        }
        uuids.push_back( ele.get_uuid() );
    }
    if( uuids.empty() || uuids.size() > 12 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }

    size_left -= parsed_size;
    p_buffer += parsed_size;
    uint16_t max_attribute_bytes_count = 0;
    if( size_left < 2 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    max_attribute_bytes_count = be_to_host16( p_buffer );
    parsed_size = 2;

    std::vector<uint16_t> requested_ids;
    std::vector<std::pair<uint16_t, uint16_t>> requested_id_ranges;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    std::vector<sdp_data_element> id_elements;
    status = sdp_data_element::parse_from( p_buffer, size_left, parsed_size, element );
    if( !status )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    if( !element.can_as_elements() )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    id_elements = element.get_elements();
    for( auto& ele : id_elements )
    {
        uint16_t id16 = 0;
        uint32_t id32 = 0;
        if( ele.can_as_uint16() )
        {
            id16 = ele.get_uint16_value();
            requested_ids.push_back( id16 );
            continue;
        }

        if( ele.can_as_uint32() )
        {
            id32 = ele.get_uint32_value();
            uint16_t high16 = id32 >> 16;
            uint16_t low16 = id32 & 0xFFFF;
            requested_id_ranges.push_back( std::make_pair( high16, low16 ) );
        }
    }

    std::vector<uint8_t> continue_state;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    uint16_t continue_state_size = 0;
    if( size_left < 1 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_syntax );
        return request;
    }
    continue_state_size = p_buffer[0];
    if( continue_state_size > 16 )
    {
        send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
        return request;
    }
    parsed_size = 1;
    size_left -= parsed_size;
    p_buffer += parsed_size;
    if( continue_state_size > 0 )
    {
        if( size_left < continue_state_size )
        {
            send_error_rsp( a_sdp_header.get_acl_handle(), sdp_error_code::invalid_continue_status );
            return request;
        }
        continue_state.resize( continue_state_size );
        memcpy( continue_state.data(), p_buffer, continue_state_size );
    }

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    uint16_t acl_handle = a_sdp_header.get_acl_handle();
    auto [remote_device, has] = acl_db->get_address( acl_handle );

    request = std::make_shared<sdp_service_search_attribute_req>();
    request->m_matching_uuids = uuids;
    request->m_max_return_count = max_attribute_bytes_count;
    request->m_matching_ids = requested_ids;
    request->m_requested_id_ranges = requested_id_ranges;
    request->m_continue_info = continue_state;
    request->m_local_cid = a_sdp_header.get_channel_id();
    request->m_remote_device = remote_device;
    return request;
}

std::shared_ptr<sdp_service_search_attribute_response> sdp_manager::parse_service_search_attribute_response
    (
    sdp_header& a_sdp_header,
    uint8_t* a_parameter_buffer,
    uint16_t a_parameter_size
    )
{
    auto alternative_result = parse_service_attribute_response( a_sdp_header, a_parameter_buffer, a_parameter_size );
    if( !alternative_result )
    {
        return nullptr;
    }

    std::shared_ptr<sdp_service_search_attribute_response> response;
    response = std::make_shared<sdp_service_search_attribute_response>();
    response->m_attribute_list = alternative_result->m_attribute_list;
    response->m_continue_info = alternative_result->m_continue_info;
    response->m_attribute_list_byte_count = alternative_result->m_attribute_list_byte_count;
    response->m_transaction_id = alternative_result->m_transaction_id;
    response->m_local_cid = alternative_result->m_local_cid;
    response->m_remote_device = alternative_result->m_remote_device;

    return response;
}

void sdp_manager::handle_register_record( std::shared_ptr<sdp_task> const& a_task )
{
    auto detail_tsk = std::static_pointer_cast<sdp_task_register_service_record>( a_task );

    uint32_t handle = m_local_service.register_record( detail_tsk->m_service_record );

    std::shared_ptr<executable_task> tsk;
    tsk = std::make_shared<executable_task>();
    tsk->set_source_module( sdp_module::s_sdp_module_name );
    tsk->set_position( source_here );
    tsk->set_fun( std::bind( detail_tsk->m_registered_callback, handle ), detail_tsk->m_callback_handle_module );
    tsk->set_target_module( detail_tsk->m_callback_handle_module );

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void sdp_manager::handle_service_search( std::shared_ptr<sdp_task> const& a_task )
{

}

void sdp_manager::handle_service_search_attribute_host( std::shared_ptr<sdp_task> const& a_task )
{
    auto detail_tsk = std::static_pointer_cast<sdp_task_service_search_attribute>( a_task );

    if( detail_tsk->m_service_uuid.empty() )
    {
        LogUtilError() << "sdp_service_search_attribute_req requires at least one service uuid";
        return;
    }

    std::shared_ptr<sdp_service_search_attribute_req> request;
    request = std::make_shared<sdp_service_search_attribute_req>();
    request->m_matching_uuids = detail_tsk->m_service_uuid;
    request->m_requested_id_ranges = detail_tsk->m_requested_id_ranges;
    request->m_max_return_count = 0xFFFF;
    request->m_matching_ids = detail_tsk->m_attribute_id_list;
    request->m_remote_device = detail_tsk->m_remote_device;

    auto conn_ = find_connection( detail_tsk->m_remote_device );
    if( conn_ )
    {
        //todo : execute the resut
    }
    else
    {
        std::shared_ptr<l2cap_task_connection_request> tsk;
        tsk = std::make_shared<l2cap_task_connection_request>();
        tsk->m_remote_device = detail_tsk->m_remote_device;
        tsk->m_psm = defined_l2cap_psm::sdp;
        tsk->set_source_module( sdp_module::s_sdp_module_name );
        tsk->set_position( source_here );

        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );

        std::shared_ptr<sdp_connection> sdp_conn = std::make_shared<sdp_connection>();
        sdp_conn->m_address = detail_tsk->m_remote_device;
        sdp_conn->set_connection_status( connection_status::connecting );
        m_connections.push_back( sdp_conn );

        m_pending_reqs.push_back( request );
    }
}

void sdp_manager::send_packet
    (
    std::shared_ptr<sdp_protocol_base> const& a_packet,
    bluetooth_address                         a_remote_address
    )
{
    if( !a_packet )
    {
        LogUtilError() << "empty packet to send.";
        return;
    }

    uint16_t local_cid = 0x00;
    std::shared_ptr<hci_data> hci_packet;
    size_t sdp_sdu_size = 0x00; // the SDP protocal data total length
    uint8_t* p_sdp_sdu = nullptr;
    size_t offset = 0;
    sdp_header _sdp_header;

    switch( a_packet->m_pdu_id )
    {
    case sdp_pdu_id::sdp_service_search_attr_rsp:
    {
        std::shared_ptr<sdp_service_search_attribute_rsp> rsp;
        rsp = std::static_pointer_cast<sdp_service_search_attribute_rsp>( a_packet );
        local_cid = rsp->m_local_cid;
        hci_packet = std::make_shared<hci_data>();

        sdp_sdu_size = 2 + rsp->m_attribute_list.size() + 1 + rsp->m_continue_info.size();
        size_t hci_total_size = _sdp_header.header_size() + sdp_sdu_size;
        hci_packet->m_buffer.resize( hci_total_size );

        // fill the sdp sdu field.
        p_sdp_sdu = hci_packet->m_buffer.data() + _sdp_header.header_size();
        write_be16( p_sdp_sdu, static_cast<uint16_t>( rsp->m_attribute_list.size() ) );
        offset += 2;
        memcpy( p_sdp_sdu + offset, rsp->m_attribute_list.data(), rsp->m_attribute_list.size() );
        offset += rsp->m_attribute_list.size();
        p_sdp_sdu[offset] = static_cast<uint8_t>( rsp->m_continue_info.size() );
        offset += 1;
        memcpy( p_sdp_sdu + offset, rsp->m_continue_info.data(), rsp->m_continue_info.size() );

        _sdp_header.set_transcation_id( rsp->m_transaction_id );
    }
    break;
    case sdp_pdu_id::sdp_service_search_attr_req:
    {
        std::shared_ptr<sdp_service_search_attribute_req> req;
        req = std::static_pointer_cast<sdp_service_search_attribute_req>( a_packet );
        for( auto& ele : m_connections )
        {
            if( ele->m_address == req->m_remote_device )
            {
                local_cid = ele->m_local_cid;
                break;
            }
        }

        hci_packet = std::make_shared<hci_data>();
        /**
         * todo: need handle request: sdp_pdu_id::sdp_service_search_attr_req.
         */
        LogUtilFatal( "need handle request: sdp_pdu_id::sdp_service_search_attr_req." );
    }
    break;
    default:
        LogUtilError() << "Packet type not handled to sent: " << a_packet->m_pdu_id;
        break;
    }

    if( !hci_packet )
    {
        LogUtilDebug() << "No hci packet to send.";
        return;
    }

    _sdp_header.set_sdu_length( static_cast<uint16_t>( sdp_sdu_size ) );
    _sdp_header.set_pdu_id( a_packet->m_pdu_id );
    _sdp_header.to_raw_buffer( hci_packet->m_buffer.data(), static_cast<uint32_t>( hci_packet->m_buffer.size() ) );

    std::shared_ptr<l2cap_task_send_l2cap_sdu> tsk;
    tsk = std::make_shared<l2cap_task_send_l2cap_sdu>();
    tsk->m_hci_packet = hci_packet;
    tsk->m_local_cid = local_cid;
    tsk->set_source_module( sdp_module::s_sdp_module_name );
    tsk->set_target_module( l2cap_module::s_l2cap_module_name );
    tsk->set_position( source_here );
    tsk->m_remote_address = a_remote_address;

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void sdp_manager::send_error_rsp( uint16_t a_acl_handle, sdp_error_code a_code )
{

}

bool sdp_manager::verify_received_packet
    (
    sdp_header& a_sdp_header,
    std::shared_ptr<hci_data> const& a_packet
    )
{
    if( a_packet->m_buffer.size() < a_sdp_header.header_size() )
    {
        return false;
    }

    a_sdp_header.parse_from_raw_data( a_packet->m_buffer.data(),
        static_cast<uint32_t>( a_packet->m_buffer.size() ) );

    uint16_t parameter_size = a_sdp_header.get_parameters_length();

    if( a_packet->m_buffer.size() < a_sdp_header.header_size() + parameter_size )
    {
        return false;
    }

    return true;
}

std::shared_ptr<sdp_connection> sdp_manager::find_connection( bluetooth_address const& a_address )
{
    for( auto& ele : m_connections )
    {
        if( ele->m_address == a_address )
        {
            return ele;
        }
    }
    return nullptr;
}

std::shared_ptr<sdp_connection> sdp_manager::find_connection
    (
    uint16_t a_acl_handle
    )
{
    for( auto& ele : m_connections )
    {
        if( ele->match( a_acl_handle ) )
        {
            return ele;
        }
    }
    return nullptr;
}

void sdp_manager::remove_connection( bluetooth_address const& a_address )
{
    for( auto it = m_connections.begin(); it != m_connections.end(); ++it )
    {
        if( ( *it )->m_address == a_address )
        {
            m_connections.erase( it );
            break;
        }
    }
}

}
