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
#include "endian_convert.h"
#include "sdp_connection.h"
#include "sdp_manager.h"

#include <framework/log_util.h>

namespace bluetooth
{

std::vector<uint8_t> make_continuation( void* a_continue_pointer )
{
    uintptr_t ptr_val = reinterpret_cast<uintptr_t>( a_continue_pointer );
    std::vector<uint8_t> buffer( sizeof( uintptr_t ) );
    std::memcpy( buffer.data(), &ptr_val, sizeof( uintptr_t ) );
    return buffer;
}

bool exact_pointer_from_continuation( std::vector<uint8_t>const& a_continue_buffer, uintptr_t& a_pointer )
{
    a_pointer = 0;
    if( a_continue_buffer.empty() )
    {
        return true;
    }

    if( a_continue_buffer.size() != sizeof( uintptr_t ) )
    {
        return false;
    }

    std::memcpy( &a_pointer, a_continue_buffer.data(), sizeof( uintptr_t ) );
    return true;
}

bool sdp_connection::set_connection_status( connection_status a_connection_status )
{
    bool ret = ( m_connection_status != a_connection_status );
    m_connection_status = a_connection_status;
    if( m_connection_status == connection_status::connected )
    {
        process_next_pending_request();
    }
    return ret;
}

void sdp_connection::search_service( std::shared_ptr<sdp_task_service_search_request> a_service_search )
{
    if( m_connection_status != connection_status::connected ||
        m_current_pending_request )
    {
        auto sdp_tsk = std::make_shared<sdp_task_pending>();
        sdp_tsk->m_pending_request = a_service_search;
        m_pending_tasks.push_back( sdp_tsk );
        return;
    }

    std::shared_ptr<sdp_service_search_request> request;
    request = std::make_shared<sdp_service_search_request>();
    request->m_transaction_id = m_next_transaction_id++;
    request->m_remote_device = a_service_search->m_remote_device;
    request->m_matching_uuids = a_service_search->m_service_uuid;
    request->m_local_cid = m_local_cid;
    request->m_max_return_count = 0xFF;
    m_current_pending_request = request;
    m_sdp_manager->send_packet( request, m_address );
}

void sdp_connection::search_service_by_handle( uint32_t a_handle )
{
    if( m_connection_status != connection_status::connected ||
        m_current_pending_request )
    {
        auto sdp_tsk = std::make_shared<wrapped_sdp_task_pending>();
        sdp_tsk->m_task = std::bind( &sdp_connection::search_service_by_handle, this, a_handle );
        m_pending_tasks.push_back( sdp_tsk );
        return;
    }

    std::shared_ptr<sdp_service_attribute_request> request;
    request = std::make_shared<sdp_service_attribute_request>();
    request->m_transaction_id = m_next_transaction_id++;
    request->m_remote_device = m_address;
    request->m_local_cid = m_local_cid;
    request->m_service_record_handle = a_handle;
    request->m_max_attribute_count = 0xFFFF;
    request->m_requested_id_ranges.push_back( { 0x0000,0xFFFF } );
    m_current_pending_request = request;
    m_sdp_manager->send_packet( request, m_address );
}

void sdp_connection::handle_error_rsp
    (
    sdp_header& _sdp_header,
    std::shared_ptr<sdp_error_response> const& a_error_rsp
    )
{
    m_current_pending_request.reset();
    process_next_pending_request();
}

void sdp_connection::handle_service_search_request
    (
    sdp_header& a_sdp_header,
    std::shared_ptr<sdp_service_search_request> const& a_ser_searching
    )
{
    if( !a_ser_searching )
    {
        return;
    }

    if( m_incoming_pending_req != sdp_pdu_id::sdp_invalid_pdu )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::reject_with_resource_limited );
        return;
    }

    uint32_t max_pdu_content = m_remote_mtu - a_sdp_header.header_size() - sizeof( uint16_t ) * 2 - sizeof( void* ) * 2;
    uint32_t max_count_send = std::min<uint32_t>( max_pdu_content / sizeof( uint32_t ), a_ser_searching->m_max_return_count );

    if( a_ser_searching->m_continue_info.size() > 0 )
    {
        uintptr_t pointer = 0;
        bool status = false;
        status = exact_pointer_from_continuation( a_ser_searching->m_continue_info, pointer );
        if( status )
        {
            auto c_buf = exract_continue_handles( pointer );
            if( c_buf )
            {
                std::shared_ptr<sdp_service_search_response> rsp;
                rsp = std::make_shared<sdp_service_search_response>();
                rsp->m_local_cid = a_ser_searching->m_local_cid;
                rsp->m_transaction_id = a_ser_searching->m_transaction_id;
                rsp->m_remote_device = a_ser_searching->m_remote_device;

                if( c_buf->service_record_handles.size() > max_count_send )
                {
                    std::shared_ptr<continuation_control_block> ccb;
                    ccb = std::make_shared<continuation_control_block>();
                    auto it = c_buf->service_record_handles.begin();
                    std::advance( it, max_count_send );
                    ccb->service_record_handles.assign( it, c_buf->service_record_handles.end() );
                    c_buf->service_record_handles.erase( it, c_buf->service_record_handles.end() );
                    uint32_t* p_continue = ccb->service_record_handles.data();
                    ccb->total_record_handle_count = c_buf->total_record_handle_count;
                    m_conitues_buffers.push_back( ccb );
                    rsp->m_continue_info = make_continuation( p_continue );
                }
                rsp->m_matched_record_handles = std::move( c_buf->service_record_handles );
                rsp->m_total_record_count = c_buf->total_record_handle_count;
                m_sdp_manager->send_packet( rsp, m_address );
                m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
            }
            else
            {
                m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_continue_status );
            }
        }
        else
        {
            m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_continue_status );
        }
        m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
        return;
    }

    if( a_ser_searching->m_matching_uuids.empty() )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_syntax );
        return;
    }

    m_incoming_pending_req = sdp_pdu_id::sdp_service_search_req;
    std::list<std::shared_ptr<sdp_service_record>> record_matched;
    std::vector<uint32_t> service_handles;
    record_matched = m_sdp_manager->m_local_service.find_matched_uuids_record( a_ser_searching->m_matching_uuids );
    for( auto& ele : record_matched )
    {
        service_handles.push_back( ele->get_service_handle() );
    }

    std::shared_ptr<sdp_service_search_response> rsp;
    rsp = std::make_shared<sdp_service_search_response>();
    rsp->m_local_cid = a_ser_searching->m_local_cid;
    rsp->m_transaction_id = a_ser_searching->m_transaction_id;
    rsp->m_remote_device = a_ser_searching->m_remote_device;
    rsp->m_total_record_count = service_handles.size();
    if( service_handles.size() > max_count_send )
    {
        std::shared_ptr<continuation_control_block> ccb;
        ccb = std::make_shared<continuation_control_block>();
        auto it = service_handles.begin();
        std::advance( it, max_count_send );
        ccb->service_record_handles.assign( it, service_handles.end() );
        ccb->total_record_handle_count = service_handles.size();
        service_handles.erase( it, service_handles.end() );
        uint32_t* p_continue = ccb->service_record_handles.data();
        m_conitues_buffers.push_back( ccb );
        rsp->m_continue_info = make_continuation( p_continue );
    }

    rsp->m_matched_record_handles = std::move( service_handles );
    m_sdp_manager->send_packet( rsp, m_address );
    m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
}

void sdp_connection::handle_service_search_response
    (
    sdp_header& _sdp_header,
    std::shared_ptr<sdp_service_search_response> const& a_ser_response
    )
{
    m_current_pending_request.reset();
    a_ser_response->m_return_record_count;
    a_ser_response->m_matched_record_handles;
    search_service_by_handle( 0x00 );
    for( auto& handle : a_ser_response->m_matched_record_handles )
    {
        search_service_by_handle( handle );
    }
    process_next_pending_request();
}

void sdp_connection::handle_service_search_attribute_request
    (
    sdp_header& a_sdp_header,
    std::shared_ptr<sdp_service_search_attribute_req> const& a_request
    )
{
    if( !a_request )
    {
        return;
    }

    if( m_incoming_pending_req != sdp_pdu_id::sdp_invalid_pdu )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::reject_with_resource_limited );
        return;
    }

    uint32_t max_pdu_content = m_remote_mtu - a_sdp_header.header_size() - 2 - 1 - sizeof( void* );
    uint32_t max_bytes_to_send = std::min<uint32_t>( max_pdu_content, a_request->m_max_return_count );
    if( a_request->m_continue_info.size() > 0 )
    {
        uintptr_t pointer = 0;
        bool status = false;
        status = exact_pointer_from_continuation( a_request->m_continue_info, pointer );
        if( status )
        {
            auto c_buf = exract_continue_buffer( pointer );
            if( c_buf )
            {
                std::shared_ptr<sdp_service_search_attribute_rsp> rsp;
                rsp = std::make_shared<sdp_service_search_attribute_rsp>();
                rsp->m_local_cid = a_request->m_local_cid;
                rsp->m_transaction_id = a_request->m_transaction_id;
                rsp->m_remote_device = a_request->m_remote_device;

                if( c_buf->buffer.size() > max_bytes_to_send )
                {
                    std::shared_ptr<continuation_control_block> ccb;
                    ccb = std::make_shared<continuation_control_block>();
                    auto it = c_buf->buffer.begin();
                    std::advance( it, max_bytes_to_send );
                    ccb->buffer.assign( it, c_buf->buffer.end() );
                    c_buf->buffer.erase( it, c_buf->buffer.end() );
                    uint8_t* p_continue = ccb->buffer.data();
                    m_conitues_buffers.push_back( ccb );
                    rsp->m_continue_info = make_continuation( p_continue );
                }
                rsp->m_attribute_list = std::move( c_buf->buffer );
                m_sdp_manager->send_packet( rsp, m_address );
                m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
            }
            else
            {
                m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_continue_status );
            }
        }
        else
        {
            m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_continue_status );
        }
        m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
        return;
    }

    if( a_request->m_matching_uuids.empty() )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_syntax );
        m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
        return;
    }

    m_incoming_pending_req = sdp_pdu_id::sdp_service_search_attr_req;
    std::list<std::shared_ptr<sdp_service_record>> record_matched;
    std::vector<sdp_data_element> attribute_list_result;
    record_matched = m_sdp_manager->m_local_service.find_matched_uuids_record( a_request->m_matching_uuids );
    for( auto& ele : record_matched )
    {
        sdp_data_element matched_values;
        std::vector<sdp_data_element> matched_details;
        sdp_service_record& record = *ele;
        for( auto& attribute_ele : record )
        {
            uint16_t id = attribute_ele.get_attribute_id();
            bool found = false;
            for( auto& match_id : a_request->m_requested_id_ranges )
            {
                if( id <= match_id.second && id >= match_id.first )
                {
                    sdp_data_element data_element;
                    data_element.set_uint16_value( id );
                    matched_details.push_back( data_element );
                    matched_details.push_back( attribute_ele.get_value() );
                    found = true;
                    break;
                }
            }

            if( found )
            {
                continue;
            }

            for( auto& match_id : a_request->m_matching_ids )
            {
                if( id == match_id )
                {
                    sdp_data_element data_element;
                    data_element.set_uint16_value( id );
                    matched_details.push_back( data_element );
                    matched_details.push_back( attribute_ele.get_value() );
                    found = true;
                    break;
                }
            }
        }

        if( !matched_details.empty() )
        {
            matched_values.set_elements( std::move( matched_details ) );
            attribute_list_result.push_back( std::move( matched_values ) );
        }
    }

    sdp_data_element element;
    element.set_elements( attribute_list_result );
    auto raw = element.get_raw_buffer();
    std::shared_ptr<sdp_service_search_attribute_rsp> rsp;
    rsp = std::make_shared<sdp_service_search_attribute_rsp>();
    rsp->m_local_cid = a_request->m_local_cid;
    rsp->m_transaction_id = a_request->m_transaction_id;
    rsp->m_remote_device = a_request->m_remote_device;

    if( raw.size() > max_bytes_to_send )
    {
        std::shared_ptr<continuation_control_block> ccb;
        ccb = std::make_shared<continuation_control_block>();
        auto it = raw.begin();
        std::advance( it, max_bytes_to_send );
        ccb->buffer.assign( it, raw.end() );
        raw.erase( it, raw.end() );
        uint8_t* p_continue = ccb->buffer.data();
        m_conitues_buffers.push_back( ccb );
        rsp->m_continue_info = make_continuation(p_continue);
    }
    rsp->m_attribute_list = std::move( raw );

    m_sdp_manager->send_packet( rsp, m_address );
    m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
}

void sdp_connection::handle_service_attribute_request
    (
    sdp_header& a_sdp_header,
    std::shared_ptr<sdp_service_attribute_request> const& a_request
    )
{
    if( !a_request )
    {
        return;
    }

    if( m_incoming_pending_req != sdp_pdu_id::sdp_invalid_pdu )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::reject_with_resource_limited );
        return;
    }

    uint32_t max_pdu_content = m_remote_mtu - a_sdp_header.header_size() - 2 /*AttributeListByteCount*/
        - 1 - sizeof( void* ) /*ContinuationState*/;
    uint32_t max_bytes_to_send = std::min<uint32_t>( max_pdu_content, a_request->m_max_attribute_count );

    if( a_request->m_continue_info.size() > 0 )
    {
        uintptr_t pointer = 0;
        bool status = false;
        status = exact_pointer_from_continuation( a_request->m_continue_info, pointer );
        if( status )
        {
            auto c_buf = exract_continue_buffer( pointer );
            if( c_buf )
            {
                std::shared_ptr<sdp_service_attribute_response> rsp;
                rsp = std::make_shared<sdp_service_attribute_response>();
                rsp->m_local_cid = a_request->m_local_cid;
                rsp->m_transaction_id = a_request->m_transaction_id;
                rsp->m_remote_device = a_request->m_remote_device;

                if( c_buf->buffer.size() > max_bytes_to_send )
                {
                    std::shared_ptr<continuation_control_block> ccb;
                    ccb = std::make_shared<continuation_control_block>();
                    auto it = c_buf->buffer.begin();
                    std::advance( it, max_bytes_to_send );
                    ccb->buffer.assign( it, c_buf->buffer.end() );
                    c_buf->buffer.erase( it, c_buf->buffer.end() );
                    uint8_t* p_continue = ccb->buffer.data();
                    m_conitues_buffers.push_back( ccb );
                    rsp->m_continue_info = make_continuation( p_continue );
                }
                rsp->m_attribute_list_raw_buffer = std::move( c_buf->buffer );
                m_sdp_manager->send_packet( rsp, m_address );
            }
            else
            {
                m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_continue_status );
            }
        }
        else
        {
            m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_continue_status );
        }
        m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
        return;
    }

    auto service_record = m_sdp_manager->m_local_service.find_service( a_request->m_service_record_handle );
    if( !service_record )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::invalid_service_record_handle );
        return;
    }

    sdp_data_element matched_values;
    std::vector<sdp_data_element> matched_details;
    for( auto& attribute_ele : *service_record )
    {
        uint16_t id = attribute_ele.get_attribute_id();
        bool found = false;
        for( auto& match_id : a_request->m_requested_id_ranges )
        {
            if( id <= match_id.second && id >= match_id.first )
            {
                sdp_data_element data_element;
                data_element.set_uint16_value( id );
                matched_details.push_back( data_element );
                matched_details.push_back( attribute_ele.get_value() );
                found = true;
                break;
            }
        }

        if( found )
        {
            continue;
        }

        for( auto& match_id : a_request->m_matching_ids )
        {
            if( id == match_id )
            {
                sdp_data_element data_element;
                data_element.set_uint16_value( id );
                matched_details.push_back( data_element );
                matched_details.push_back( attribute_ele.get_value() );
                found = true;
                break;
            }
        }
    }

    matched_values.set_elements( std::move( matched_details ) );
    auto raw = matched_values.get_raw_buffer();
    std::shared_ptr<sdp_service_attribute_response> rsp;
    rsp = std::make_shared<sdp_service_attribute_response>();
    rsp->m_local_cid = a_request->m_local_cid;
    rsp->m_transaction_id = a_request->m_transaction_id;
    rsp->m_remote_device = a_request->m_remote_device;

    if( raw.size() > max_bytes_to_send )
    {
        std::shared_ptr<continuation_control_block> ccb;
        ccb = std::make_shared<continuation_control_block>();
        auto it = raw.begin();
        std::advance( it, max_bytes_to_send );
        ccb->buffer.assign( it, raw.end() );
        raw.erase( it, raw.end() );
        uint8_t* p_continue = ccb->buffer.data();
        m_conitues_buffers.push_back( ccb );
        rsp->m_continue_info = make_continuation( p_continue );
    }
    rsp->m_attribute_list_raw_buffer = std::move( raw );

    m_sdp_manager->send_packet( rsp, m_address );
    m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
}

void sdp_connection::handle_sdp_service_attribute_response
    (
    sdp_header& _sdp_header,
    std::shared_ptr<sdp_service_attribute_response> const& a_response
    )
{
    m_current_pending_request.reset();
    process_next_pending_request();
}

void sdp_connection::process_next_pending_request()
{
    if( m_pending_tasks.empty() )
    {
        return;
    }

    auto first_pending = m_pending_tasks.front();
    m_pending_tasks.erase( m_pending_tasks.begin() );

    switch( first_pending->m_pending_type )
    {
    case pending_type::sdp_task_pending_type:
        process_pending_sdp_task( std::static_pointer_cast<sdp_task_pending>
            ( first_pending ) );
        break;
    case pending_type::sdp_wrapped_task:
    {
        auto pending_task = std::static_pointer_cast<wrapped_sdp_task_pending>( first_pending );
        pending_task->m_task();
    }
        break;
    default:
        LogUtilError() << "pending task ignored with type: "
            << static_cast<uint32_t>( first_pending->m_pending_type );
        break;
    }

}

void sdp_connection::process_pending_sdp_task( std::shared_ptr<sdp_task_pending> a_pending_tsk )
{
    switch( a_pending_tsk->m_pending_request->m_type )
    {
    case sdp_task_type::service_search_request:
        search_service( std::static_pointer_cast<sdp_task_service_search_request>
            ( a_pending_tsk->m_pending_request ) );
        break;
    default:
        LogUtilError() << "pending task ignored with type: "
            << static_cast<uint32_t>( a_pending_tsk->m_pending_request->m_type );
        break;
    };
}

}

