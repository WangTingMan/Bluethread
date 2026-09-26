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

#include "l2cap_signaling.h"
#include "endian_convert.h"
#include "data_element.h"
#include "l2cap_internal_task.h"
#include "../l2cap_module.h"

#include "common/controller.h"
#include "../../hci/hci_module.h"
#include "../../common/stream_writer.h"
#include "../../common/acl_connections_db.h"

#include "framework/log_util.h"
#include "framework/framework_manager.h"
#include "framework/timer_module.h"

#include <memory>

/* The signaling packet contains( see core specfication doc ):
* ------------------------------------------------------------------------------
* | ACL header | L2CAP header | signaling header | siganling service data unit |
* ------------------------------------------------------------------------------
* * ACL header:
* ------------------------------------------
* | ACL handle( 2bytes ) | Length( 2bytes )|
* ------------------------------------------
* * L2CAP header:
* ------------------------------------------
* | Length( 2bytes ) | Channel ID( 2bytes )|
* ------------------------------------------
* * Siganling packet header:
* ------------------------------------------------------------
* | Code( 1bytes ) | Indentifier( 1bytes )| Length( 2bytes ) |
* ------------------------------------------------------------
*/

namespace
{
    constexpr uint16_t signaling_header_size = 0x0004;
    constexpr uint16_t l2cap_header_size = 0x0004;
    constexpr std::chrono::seconds s_command_response_timeout{ 10 };
    constexpr uint16_t s_connectionless_mtu = 2048;
}

namespace bluetooth
{

l2cap_signaling::l2cap_signaling( uint16_t a_handle )
{
    m_sig_header.set_acl_handle( a_handle );
    m_sig_header.set_packet_boundary( 0x00 );
    m_sig_header.set_broadcast_flag( 0x00 );
}

l2cap_signaling::~l2cap_signaling()
{
    for( auto& cmd : m_commands_sent )
    {
        cancel_timer( cmd.m_registered_time_out_timer_id );
    }
    m_commands_sent.clear();
}

void l2cap_signaling::set_packet_boundary( uint8_t a_pb_flag )
{
    m_sig_header.set_packet_boundary( a_pb_flag );
}

void l2cap_signaling::set_broadcast_flag( uint8_t a_bc_flag )
{
    m_sig_header.set_broadcast_flag( a_bc_flag );
}

void l2cap_signaling::send_reject_rsp
    (
    uint8_t a_identifier,
    l2cap_command_reject_reason a_reason,
    uint8_t* a_ext_data,
    uint16_t a_ext_data_size
    )
{
    m_sig_header.set_identifier( a_identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_command_reject_rsp );
    m_sig_header.set_sdu_length( 2 + a_ext_data_size );

    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_reason;
    writer.write_buffer( a_ext_data, a_ext_data_size );

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
}

void l2cap_signaling::send_connection_request
    (
    uint16_t a_psm,
    uint16_t a_source_id
    )
{
    uint8_t signaling_identifier = get_identifier();
    m_sig_header.set_identifier( signaling_identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_connection_req );
    m_sig_header.set_sdu_length( 4 );

    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_psm << a_source_id;

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );

    auto command = std::make_shared<connection_request>();
    command->set_receiver( m_remote_address );
    command->m_psm_value = a_psm;
    command->m_source_cid = a_source_id;
    command->m_acl_handle = m_sig_header.get_acl_handle();
    command->m_signaling_code = signaling_code::l2cap_connection_req;
    command->m_identifier = signaling_identifier;
    queue_signaling_request( command );
}

void l2cap_signaling::send_connection_response
    (
    uint8_t a_identifier,
    uint16_t a_dest_cid,
    uint16_t a_src_cid,
    connection_req_result a_result,
    connection_req_refused_status a_refused_status
    )
{
    m_sig_header.set_identifier( a_identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_connection_rsp );
    m_sig_header.set_sdu_length( 8 );
    if( a_result != connection_req_result::connection_pending )
    {
        a_refused_status = connection_req_refused_status::refused_no_more_info;
    }

    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_dest_cid << a_src_cid << a_result << a_refused_status;

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
}

uint8_t l2cap_signaling::send_config_request
    (
    uint16_t a_remote_cid,
    std::vector<channel_config_option> const& a_options
    )
{
    if( a_options.empty() )
    {
        LogUtilError() << "No configuration options need to send.";
        return 0x00;
    }

    uint8_t identifier = get_identifier();
    m_sig_header.set_identifier( identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_configuration_req );

    uint8_t write_buffer[256] = { 0 };
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_remote_cid << static_cast< uint16_t >( 0x00 ); // We do not support continue flag right now.
    for( auto& ele : a_options )
    {
        writer << ele;
    }

    m_sig_header.set_sdu_length( writer.wrote_size() );

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    std::shared_ptr<l2cap_config_request> request_sent;
    request_sent = std::make_shared<l2cap_config_request>();
    request_sent->m_options = a_options;
    request_sent->m_identifier = identifier;
    request_sent->m_destionation_cid = a_remote_cid;
    queue_signaling_request( request_sent );

    // TODO: Handle remote ConfigReject with MTU exceeded reason.
    // If peer rejects configuration request because our requested MTU exceeds peer capability,
    // implement MTU renegotiation logic to pick a smaller compatible MTU.
    // Defer this feature to later milestone.
    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
    return identifier;
}

void l2cap_signaling::send_config_response
    (
    uint8_t a_identifier,
    uint16_t a_src_cid,
    uint16_t a_flags,
    channel_config_result a_result,
    std::vector<channel_config_option> const& a_options
    )
{
    m_sig_header.set_identifier( a_identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_configuration_rsp );

    uint8_t write_buffer[1024];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_src_cid << a_flags << a_result;
    for( auto& ele : a_options )
    {
        writer << ele;
    }

    m_sig_header.set_sdu_length( writer.wrote_size() );

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
}

void l2cap_signaling::send_disconnect_request
    (
    uint16_t a_dest_cid,
    uint16_t a_src_cid
    )
{
    uint8_t identifier = get_identifier();
    m_sig_header.set_identifier( identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_disconnection_req );
    m_sig_header.set_sdu_length( 4 );

    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_dest_cid << a_src_cid;

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    auto request = std::make_shared<l2cap_disconnect_request>();
    request->m_identifier = identifier;
    request->m_destination_cid = a_dest_cid;
    request->m_source_cid = a_src_cid;
    request->m_connection_handle = m_sig_header.get_acl_handle();
    request->set_sender( m_remote_address );
    queue_signaling_request( request );

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );

    /**
     * Since we are going to disconnect the channel, we need clear all pending packets in the channel's queue.
     */
    std::shared_ptr<l2cap_task_clear_pending_packets> tsk;
    tsk = std::make_shared<l2cap_task_clear_pending_packets>();
    tsk->m_acl_handle = get_acl_handle();
    tsk->m_local_cid = a_src_cid;
    tsk->m_acl_type = get_acl_type();
    tsk->set_source_module( l2cap_module::s_l2cap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void l2cap_signaling::send_disconnect_response
    (
    uint8_t a_identifider,
    uint16_t a_dest_cid,
    uint16_t a_src_cid
    )
{
    m_sig_header.set_identifier( a_identifider );
    m_sig_header.set_signaling_code( signaling_code::l2cap_disconnection_rsp);
    m_sig_header.set_sdu_length( 4 );

    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << a_dest_cid << a_src_cid;

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
}

void l2cap_signaling::send_echo
    (
    signaling_code a_code,
    uint8_t a_identifier,
    uint8_t const* a_data,
    uint16_t a_size
    )
{
    if( a_size > 70 )
    {
        LogUtilError() << "Max echo data is 70 bytes";
        return;
    }
    m_sig_header.set_identifier( a_identifier );
    m_sig_header.set_signaling_code( a_code );
    m_sig_header.set_sdu_length( a_size );

    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer.write_buffer( a_data, a_size );

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();

    send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
}

void l2cap_signaling::handle_command_wait_rsp_timeout( uint8_t a_identifier )
{
    LogUtilError() << "Remote device did not send response for command identifier: "
        << (uint32_t)a_identifier;

    // TODO: we need re-send this command again or disconnect the ACL connection.
}

void l2cap_signaling::cancel_timer( uint32_t a_timer_id )
{
    auto _module = framework::framework_manager::get_instance()
        .get_module_manager().get_module( framework::abstract_module::s_timer_module_name );
    auto _timer_module = std::static_pointer_cast<framework::timer_module>(_module);
    _timer_module->undregister_timer( a_timer_id );
}

void l2cap_signaling::queue_signaling_request( std::shared_ptr<signaling_channel_packet> a_request )
{
    if( a_request->m_identifier == 0 )
    {
        LogUtilError() << "identifier is zero, ignore this register timer";
        return;
    }

    for( auto& ele : m_commands_sent )
    {
        if( ele.m_sent_command->m_identifier == a_request->m_identifier )
        {
            LogUtilError() << "register agian with same identifier!";
            return;
        }
    }

    uint8_t identifier = a_request->m_identifier;
    command_sent_control_block cmd_cb;
    cmd_cb.m_sent_command = a_request;
    auto _module = framework::framework_manager::get_instance()
        .get_module_manager().get_module( framework::abstract_module::s_timer_module_name );
    auto _timer_module = std::static_pointer_cast<framework::timer_module>( _module );
    auto thiz = shared_from_this();
    auto timer_id = _timer_module->register_once_timer
        (
            [thiz, identifier]( uint32_t /*a_id*/, std::string /*a_name*/ )
            {
                thiz->handle_command_wait_rsp_timeout( identifier );
            },
            std::chrono::milliseconds( s_command_response_timeout ),
            "",
            l2cap_module::s_l2cap_module_name
        );
    cmd_cb.m_registered_time_out_timer_id = timer_id;
    m_commands_sent.push_back( std::move( cmd_cb ) );
}

void l2cap_signaling::query_information( l2cap_channel_information_type a_info_type )
{
    if( a_info_type > l2cap_channel_information_type::fixed_channel_supported ||
        a_info_type == static_cast<l2cap_channel_information_type>( 0x0000 ) )
    {
        LogUtilError() << "Currently we do not support info type: " << static_cast<uint16_t>( a_info_type );
        return;
    }

    uint8_t write_buffer[100];
    stream_writer writer( write_buffer, 100 );
    uint16_t buffer_size = 0;

    uint8_t identifier = get_identifier();
    m_sig_header.set_identifier( identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_information_req );

    m_sig_header.set_sdu_length( sizeof( a_info_type ) );
    writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
    writer << static_cast<uint16_t>( a_info_type );

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();
    if( buffer_size > 0 )
    {
        LogUtilInfo() << "query informatin type: " << a_info_type;
        send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );

        auto request = std::make_shared<l2cap_information_request>();
        request->m_identifier = identifier;
        request->m_infor_type = a_info_type;
        queue_signaling_request( request );
    }
}

void l2cap_signaling::handle_incoming_signaling( std::shared_ptr<hci_data> const& a_hci_data )
{
    if( !signaling_length_valid( a_hci_data ) )
    {
        return;
    }

    std::vector<uint8_t> const& raw_hci = a_hci_data->m_buffer;
    uint8_t handle[2];
    handle[0] = raw_hci[0];
    handle[1] = raw_hci[1] & 0x0F;
    uint16_t handle_ = m_sig_header.get_acl_handle();

    uint8_t const* raw_sig_ptr = raw_hci.data() + s_l2cap_signaling_offset;
    uint32_t available_size = raw_hci.size() - s_l2cap_signaling_offset;
    uint32_t parsed_size = 0;

    /* An L2CAP packet may contain multiple commands */
    while( available_size > 0 )
    {
        if( available_size < 1 )
        {
            LogUtilError( "signaling packet two small, we need signaling code first" );
            return;
        }

        signaling_code code = static_cast<signaling_code>( raw_sig_ptr[0] );
        switch( code )
        {
        case bluetooth::signaling_code::l2cap_command_reject_rsp:
            parsed_size = handle_command_reject_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_connection_req:
            parsed_size = handle_connection_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_connection_rsp:
            parsed_size = handle_connection_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_configuration_req:
            parsed_size = handle_config_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_configuration_rsp:
            parsed_size = handle_config_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_disconnection_req:
            parsed_size = handle_disconnect_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_disconnection_rsp:
            parsed_size = handle_disconnect_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_echo_req:
            parsed_size = handle_echo_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_echo_rsp:
            parsed_size = handle_echo_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_information_req:
            parsed_size = handle_information_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_information_rsp:
            parsed_size = handle_information_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_connection_parameter_update_req:
            parsed_size = handle_connection_parameter_update_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_connection_parameter_update_rsp:
            parsed_size = handle_connection_parameter_update_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_le_credit_based_connection_req:
            parsed_size = handle_le_credit_based_connection_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_le_credit_based_connection_rsp:
            parsed_size = handle_le_credit_based_connection_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_flow_control_credit_ind:
            parsed_size = handle_le_flow_control_credit_ind( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_credit_based_connection_req:
            parsed_size = handle_credit_based_connection_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_credit_based_connection_rsp:
            parsed_size = handle_credit_based_connection_response( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_credit_based_reconfigure_req:
            parsed_size = handle_credit_based_reconfig_request( raw_sig_ptr, available_size );
            break;
        case bluetooth::signaling_code::l2cap_credit_based_reconfigure_rsp:
            parsed_size = handle_credit_based_reconfig_response( raw_sig_ptr, available_size );
            break;
        default:
            parsed_size = handle_unknown_signaling_code( raw_sig_ptr, available_size );
            break;
        }

        raw_sig_ptr += parsed_size;
        available_size -= parsed_size;

        if( parsed_size == 0 )
        {
            LogUtilError() << "parsed_size is zero, breaking to avoid infinite loop";
            break;
        }

        if( parsed_size > available_size )
        {
            LogUtilError() << "parsed_size exceeds available_size, corrupt packet";
            break;
        }

        if( get_acl_type() == acl_type::le_acl )
        {
            /* one signaling rqeust/response in one signaling packet on LE signaling channel.
             *  So we need to break here
            */
            break;
        }
    }
}

bool l2cap_signaling::signaling_length_valid( std::shared_ptr<hci_data> const& hci_data )
{
    uint16_t signaling_data_size = le_to_host16( hci_data->m_buffer.data() + s_l2cap_signaling_offset + 2);
    uint32_t expected_size = signaling_data_size + s_l2cap_signaling_offset + 4;
    if( expected_size == hci_data->m_buffer.size() )
    {
        return true;
    }
    else if( expected_size < hci_data->m_buffer.size() )
    {
        LogUtilWarning() << "l2cap packet size greater than expected size";
        return true;
    }
    else
    {
        LogUtilError() << "l2cap packet size smaller than expected size";
        return false;
    }

    return true;
}

bool l2cap_signaling::parse_signaling_header
    (
    uint8_t const* a_raw_sig,
    uint16_t& a_size_left,
    uint16_t& a_size_parsed,
    uint8_t& a_identifier,
    uint16_t& a_signal_data_length
    )
{
    a_identifier = 0;
    a_signal_data_length = 0;

    if( a_size_left < 2u )
    {
        LogUtilWarning() << "SignalingHeader parse fail: buffer too small, cannot read Identifier";
        return false;
    }
    a_identifier = a_raw_sig[1];
    a_size_left -= 2u;
    a_size_parsed += 2u;

    if( a_size_left < 2u )
    {
        LogUtilWarning() << "SignalingHeader parse fail: buffer too small for signal data length";
        return false;
    }
    a_signal_data_length = le_to_host16( a_raw_sig + 2 );
    a_size_left -= 2u;
    a_size_parsed += 2u;

    return true;
}

bool l2cap_signaling::cancel_timer_for_command( uint8_t a_identifier )
{
    if( a_identifier == 0x00 )
    {
        return false;
    }

    bool found_pending_cmd = false;
    for( auto it = m_commands_sent.begin(); it != m_commands_sent.end(); ++it )
    {
        auto& ele = *it;
        if( ele.m_sent_command->m_identifier == a_identifier )
        {
            cancel_timer( ele.m_registered_time_out_timer_id );
            m_commands_sent.erase( it );
            found_pending_cmd = true;
            break;
        }
    }
    return found_pending_cmd;
}

uint16_t l2cap_signaling::handle_information_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;
    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "InformationRequest parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;
    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "InformationRequest parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    constexpr uint16_t REQUIRED_PAYLOAD_LEN = 2;
    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;
    if( data_size != REQUIRED_PAYLOAD_LEN ||
        size_left < REQUIRED_PAYLOAD_LEN )
    {
        LogUtilWarning() << "InformationRequest parse fail: invalid length not 2";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        */
        size_parsed = a_size;
        return size_parsed;
    }

    uint16_t info_type = le_to_host16( a_raw_sig + 4 );
    size_parsed += REQUIRED_PAYLOAD_LEN;
    size_left -= REQUIRED_PAYLOAD_LEN;

    uint16_t result_code = 0x0000;
    uint8_t write_buffer[100];
    uint16_t buffer_size = 0;
    stream_writer writer;

    m_sig_header.set_identifier( identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_information_rsp );

    l2cap_channel_information_type info_type_enum = static_cast<l2cap_channel_information_type>( info_type );
    switch( info_type_enum )
    {
    case l2cap_channel_information_type::connectionless_mtu:
        m_sig_header.set_sdu_length( sizeof( info_type ) + sizeof( result_code ) + sizeof( s_connectionless_mtu ) );
        writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
        writer << info_type << result_code << s_connectionless_mtu;
        break;
    case l2cap_channel_information_type::extended_features_supported:
        m_sig_header.set_sdu_length( sizeof( info_type ) + sizeof( result_code ) + 4 );
        writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
        writer << info_type << result_code;
        {
            std::vector<uint8_t> ext_mask{ 0xFF, 0x07, 0x00, 0x00 };
            ext_mask[0] = s_flow_control_mode_support | s_retransmission_mode_support | s_bi_directional_qos_support |
                s_enhanced_retransmission_mode_support | s_streaming_mode_support | s_fcs_option_support |
                s_extended_flow_specification_edr_support | s_fixed_channels_support;
            ext_mask[1] = s_extended_window_size_support | s_unicast_connectionless_data_reception_support |
                s_enhanced_credit_based_flow_control_support;
            writer << ext_mask;
        }
        break;
    case l2cap_channel_information_type::fixed_channel_supported:
        m_sig_header.set_sdu_length( sizeof( info_type ) + sizeof( result_code ) + 8 );
        writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
        writer << info_type << result_code << std::vector<uint8_t>( { 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } );
        break;
    default:
        LogUtilError() << "Wrong info type requested. So we send notify response packet.";
        m_sig_header.set_sdu_length( sizeof( info_type ) + sizeof( result_code ) );
        writer.set_buffer( write_buffer + m_sig_header.header_size(), sizeof( write_buffer ) - m_sig_header.header_size() );
        result_code = 0x01; // 0x01 means not supported
        writer << info_type << result_code;
        break;
    }

    buffer_size += writer.wrote_size();
    m_sig_header.to_raw_buffer( write_buffer, sizeof( write_buffer ) - buffer_size );
    buffer_size += m_sig_header.header_size();
    if( buffer_size > 0 )
    {
        send_completed_acl_packet( std::vector<uint8_t>( write_buffer, write_buffer + buffer_size ) );
    }
    else
    {
        LogUtilError( "should not reach here!" );
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_information_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;

    if( m_acl_type == acl_type::le_acl )
    {
        LogUtilError() << "LE ACL connection: INFORMATION_RESPONSE is not supported.";
        size_parsed = a_size;
        size_left = 0;
        return size_parsed;
    }

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "InformationResponse parse fail: buffer too small,"
            " cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }
    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;
    cancel_timer_for_command( identifier );

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "InformationResponse parse fail: buffer too small,"
            " cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        // NOTE: INFORMATION_RESPONSE is Response, MUST NOT send Command Reject
        return size_parsed;
    }

    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    constexpr uint16_t INFO_BASE_PAYLOAD = 4;
    if( data_size < INFO_BASE_PAYLOAD || size_left < data_size )
    {
        LogUtilWarning() << "InformationResponse parse fail: payload insufficient, data_size="
            << data_size;
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        * NOTE: Response frame, no Command Reject reply.
        */
        size_parsed = a_size;
        return size_parsed;
    }

    const uint8_t* p_payload = a_raw_sig + 4;
    uint16_t info_type_ori = le_to_host16( p_payload );
    uint16_t result_code = le_to_host16( p_payload + 2 );
    const uint8_t* p_info_data = p_payload + INFO_BASE_PAYLOAD;
    uint16_t info_data_len = data_size - INFO_BASE_PAYLOAD;
    l2cap_channel_information_type info_type = static_cast<l2cap_channel_information_type>
        ( info_type_ori );

    if( result_code != 0x00 )
    {
        LogUtilError() << "remote device return error code " << result_code
            << " when query information type: " << info_type_ori;
        switch( info_type )
        {
        case l2cap_channel_information_type::connectionless_mtu:
            m_remote_connectionless_mtu = 0x0000;
            query_information( l2cap_channel_information_type::extended_features_supported );
            break;
        case l2cap_channel_information_type::extended_features_supported:
            m_remote_ext_features.clear();
            query_information( l2cap_channel_information_type::fixed_channel_supported );
            break;
        case l2cap_channel_information_type::fixed_channel_supported:
            break;
        default:
            break;
        }
    }
    else
    {
        switch( info_type )
        {
        case l2cap_channel_information_type::connectionless_mtu:
            if( info_data_len >= 2 )
            {
                m_remote_connectionless_mtu = le_to_host16( p_info_data );
            }
            query_information( l2cap_channel_information_type::extended_features_supported );
            break;
        case l2cap_channel_information_type::extended_features_supported:
            m_remote_ext_features.set_value( p_info_data );
            LogUtilInfo() << m_remote_ext_features.to_string();
            query_information( l2cap_channel_information_type::fixed_channel_supported );
            break;
        case l2cap_channel_information_type::fixed_channel_supported:
            break;
        default:
            LogUtilInfo() << "InformationResponse: unknown/reserved info_type: " << info_type_ori
                << ", info data len: " << info_data_len;
            break;
        }
    }

    if( m_info_callback )
    {
        m_info_callback( get_acl_handle(), info_type );
    }

    size_parsed += data_size;
    size_left -= data_size;
    return size_parsed;
}

uint16_t l2cap_signaling::handle_command_reject_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "CommandReject parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;
    cancel_timer_for_command( identifier );

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "CommandReject parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        // NOTE: COMMAND_REJECT is Response, MUST NOT send Command Reject
        return size_parsed;
    }

    constexpr uint16_t BASE_PAYLOAD_LEN = 2;
    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    if( data_size < BASE_PAYLOAD_LEN || size_left < data_size )
    {
        LogUtilWarning() << "CommandReject parse fail: payload too small, min is 2 bytes";
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        * NOTE: Response frame, no Command Reject reply.
        */
        size_parsed = a_size;
        return size_parsed;
    }

    uint16_t reason = le_to_host16( a_raw_sig + 4 );
    command_reject_reason_code reason_code = static_cast<command_reject_reason_code>( reason );
    uint16_t remain_payload_len = data_size - BASE_PAYLOAD_LEN;
    const uint8_t* p_remain_payload = a_raw_sig + 6;

    bool payload_valid = true;
    switch( reason_code )
    {
    case command_reject_reason_code::command_not_understood:
        if( remain_payload_len != 0 )
        {
            LogUtilWarning() << "CommandReject: command_not_understood has extra payload, len="
                << remain_payload_len;
            payload_valid = false;
        }
        break;
    case command_reject_reason_code::signaling_mtu_exceeded:
        if( remain_payload_len != 2 )
        {
            LogUtilWarning() << "CommandReject: signaling_mtu_exceeded expect remain payload=2";
            payload_valid = false;
        }
        break;
    case command_reject_reason_code::invalid_cid_in_request:
        if( remain_payload_len != 4 )
        {
            LogUtilWarning() << "CommandReject: invalid_cid_in_request expect remain payload=4";
            payload_valid = false;
        }
        break;
    default:
        LogUtilInfo() << "CommandReject: unknown/reserved reason code: " << reason
            << ", remain payload length: " << remain_payload_len;
        payload_valid = true;
        break;
    }

    if( !payload_valid )
    {
        size_parsed = a_size;
        return size_parsed;
    }

    switch( reason_code )
    {
    case command_reject_reason_code::command_not_understood:
        LogUtilInfo() << "Remote device cannot understand command identified by " << identifier;
        break;
    case command_reject_reason_code::signaling_mtu_exceeded:
        {
            uint16_t remote_signaling_mtu = le_to_host16( p_remain_payload );
            LogUtilInfo() << "Remote device's signaling channel's mtu is " << remote_signaling_mtu;
            m_remote_signaling_mtu = remote_signaling_mtu;
            // TODO: send command with proper size to remote device again.
        }
        break;
    case command_reject_reason_code::invalid_cid_in_request:
        {
            uint16_t local_cid = le_to_host16( p_remain_payload );
            uint16_t remote_cid = le_to_host16( p_remain_payload + 2 );
            LogUtilInfo() << "invalid cid in request. local: " << local_cid << ", remote: " << remote_cid;
            auto tsk = std::make_shared<l2cap_task_remote_invalid_cid_channel_close>();
            tsk->m_acl_handle = get_acl_handle();
            tsk->m_local_cid = local_cid;
            tsk->m_remote_cid = remote_cid;
            tsk->m_acl_type = m_acl_type;

            framework::framework_manager::get_instance().get_thread_manager().post_task
                ( tsk, framework::source_here );
        }
        break;
    default:
        LogUtilError() << "Currently we do not support such reason code: " << reason;
        auto reject_rsp = std::make_shared<command_reject>();
        reject_rsp->m_identifier = identifier;
        reject_rsp->m_reject_reason = static_cast<l2cap_command_reject_reason>( reason );
        reject_rsp->m_reject_data.assign( p_remain_payload, p_remain_payload + remain_payload_len );
        if( m_sig_pkt_handler )
        {
            auto the_controller = framework::framework_manager::get_instance().get_info_manager()
                .get_detail_information<controller>( controller::s_information_name );
            reject_rsp->set_receiver( the_controller->get_address() );
            reject_rsp->set_sender( m_remote_address );
            m_sig_pkt_handler( reject_rsp );
        }
        else
        {
            LogUtilError() << "No signaling handler for unknown command reject reason";
        }
        break;
    }

    size_parsed += data_size;
    size_left -= data_size;
    return size_parsed;
}

uint16_t l2cap_signaling::handle_connection_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    std::shared_ptr<connection_request> request;
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "ConnectionRequest parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "ConnectionRequest parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;

        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    /* L2CAP CONNECTION_REQ payload : PSM(2) + Source CID(2), fixed 4 octets payload */
    constexpr uint16_t REQUIRED_PAYLOAD_LEN = 4;
    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 bytes data length field.*/
    if( data_size != REQUIRED_PAYLOAD_LEN ||
        size_left < 4 )
    {
        LogUtilWarning() << "ConnectionRequest parse fail: invalid length not 4";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );

        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        */
        size_parsed = a_size;
        return size_parsed;
    }
    size_parsed += 2;

    uint16_t psm = le_to_host16( a_raw_sig + 4 );
    uint16_t source_cid = le_to_host16( a_raw_sig + 6 );
    size_parsed += 4;

    request = std::make_shared<connection_request>();
    request->m_identifier = identifier;
    request->m_acl_handle = m_sig_header.get_acl_handle();
    request->m_psm_value = psm;
    request->m_source_cid = source_cid;

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    request->set_receiver( the_controller->get_address() );
    request->set_sender( m_remote_address );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( request );
    }
    else
    {
        LogUtilError() << "No signaling request handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_connection_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;
    if( m_acl_type == acl_type::le_acl )
    {
        LogUtilError() << "LE ACL connection response is not supported.";
        size_parsed = a_size;
        size_left = 0;
        return size_parsed;
    }

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "ConnectionResponse parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;
    cancel_timer_for_command( identifier );

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "ConnectionResponse parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        // NOTE: CONNECTION_RSP is Response, MUST NOT send Command Reject
        return size_parsed;
    }

    constexpr uint16_t MIN_PAYLOAD_LEN = 8;
    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    if( data_size < MIN_PAYLOAD_LEN || size_left < data_size )
    {
        LogUtilWarning() << "ConnectionResponse parse fail: payload too small, min is 8 bytes";
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        * NOTE: Response frame, no Command Reject reply.
        */
        size_parsed = a_size;
        return size_parsed;
    }


    uint16_t dest_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t src_cid = le_to_host16( a_raw_sig + 6 );
    connection_req_result result = static_cast<connection_req_result>( le_to_host16( a_raw_sig + 8 ) );
    connection_req_refused_status status = connection_req_refused_status::refused_no_more_info;

    if( result == connection_req_result::connection_pending )
    {
        if( data_size >= 10 )
        {
            status = static_cast<connection_req_refused_status>( le_to_host16( a_raw_sig + 10 ) );
        }
        else
        {
            LogUtilError() << "Remote device let the connection request as pending but no status returned.";
        }
    }

    std::shared_ptr<l2cap_connect_response> rsp = std::make_shared<l2cap_connect_response>();
    rsp->m_identifier = identifier;
    rsp->m_source_cid = src_cid;
    rsp->m_destionation_cid = dest_cid;
    rsp->result = result;
    rsp->status = status;
    rsp->set_sender( m_remote_address );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    rsp->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( rsp );
    }
    else
    {
        LogUtilError() << "No signaling response handler!";
    }

    size_parsed += data_size;
    size_left -= data_size;
    return size_parsed;
}

uint16_t l2cap_signaling::handle_config_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t signal_data_length = 0u;

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2u )
    {
        LogUtilWarning() << "ConfigRequest parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2u;
    size_parsed += 2u;

    if( m_acl_type == acl_type::le_acl )
    {
        LogUtilError() << "LE ACL config request is not supported.";
        send_reject_rsp( identifier,
            l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        size_parsed = a_size;
        return size_parsed;
    }

    if( size_left < 2u )
    {
        LogUtilWarning() << "ConfigRequest parse fail: buffer too small for signal data length";
        size_parsed = a_size;
        send_reject_rsp( identifier,
            l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }
    signal_data_length = le_to_host16( a_raw_sig + 2 );
    size_left -= 2u;
    size_parsed += 2u;

    if( signal_data_length < 4 )
    {
        LogUtilWarning() << "ConfigRequest parse fail: signal data length too small, minimum require 4 bytes";
        size_parsed = a_size;
        send_reject_rsp( identifier,
            l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    uint16_t dest_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t flags = le_to_host16( a_raw_sig + 6 );
    bool continue_flag = 0x01 == ( flags & 0x01 );
    size_left -= 4u;
    size_parsed += 4u;

    bool remote_edr_ext_flow_support = m_remote_ext_features.support_feature(
        l2cap_ext_feature_flag::extended_flow_specification_edr );
    if( continue_flag && remote_edr_ext_flow_support )
    {
        LogUtilError() << "Remote device should not use continue flag when support ext-flow";
    }

    uint16_t config_option_len = signal_data_length - 4u;
    if( size_left < config_option_len )
    {
        LogUtilWarning() << "ConfigRequest parse fail: buffer too small for config options";
        size_parsed = a_size;
        send_reject_rsp( identifier,
            l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    uint8_t const* p_config = a_raw_sig + 8u;
    auto [options, unknown_types, is_truncated] = parse_channel_config( p_config, config_option_len );
    size_left -= config_option_len;
    size_parsed += config_option_len;

    if( is_truncated )
    {
        LogUtilError() << "ConfigRequest: remote send truncated configuration options";
    }

    std::shared_ptr<l2cap_config_request> request;
    request = std::make_shared<l2cap_config_request>();
    request->m_destionation_cid = dest_cid;
    request->m_identifier = identifier;
    request->m_options.swap( options );
    request->m_unkown_option_types.swap( unknown_types );
    request->m_acl_handle = get_acl_handle();
    request->m_continue_flag = continue_flag;
    request->set_sender( m_remote_address );
    request->m_is_truncted = is_truncated;
    request->m_remote_edr_ext_flow_support = remote_edr_ext_flow_support;
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    request->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( request );
    }
    else
    {
        LogUtilError() << "No signaling request handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_config_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t signal_data_length = 0u;

    if( m_acl_type == acl_type::le_acl )
    {
        LogUtilError() << "LE ACL config response is not supported.";
        size_parsed = a_size;
        return size_parsed;
    }

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2u )
    {
        LogUtilWarning() << "ConfigResponse parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2u;
    size_parsed += 2u;
    cancel_timer_for_command( identifier );

    if( size_left < 2u )
    {
        LogUtilWarning() << "ConfigResponse parse fail: buffer too small for signal data length";
        size_parsed = a_size;
        return size_parsed;
    }
    signal_data_length = le_to_host16( a_raw_sig + 2 );
    size_left -= 2u;
    size_parsed += 2u;

    //LocalCID(2)+Flag(2)+Result(2) = 6 bytes
    if( signal_data_length < 6u )
    {
        LogUtilWarning() << "ConfigResponse parse fail: signal data length too small, minimum require 6 bytes";
        size_parsed = a_size;
        return size_parsed;
    }

    if( size_left < 6u )
    {
        LogUtilWarning() << "ConfigResponse parse fail: buffer too small for local CID + flag + result";
        size_parsed = a_size;
        return size_parsed;
    }
    uint16_t local_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t flag = le_to_host16( a_raw_sig + 6 );
    uint16_t result = le_to_host16( a_raw_sig + 8 );
    size_left -= 6u;
    size_parsed += 6u;

    uint16_t config_option_len = signal_data_length - 6u;
    if( size_left < config_option_len )
    {
        LogUtilWarning() << "ConfigResponse parse fail: buffer too small for config options";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t const* p_config = a_raw_sig + 10u;
    auto [options, unknown_types, is_truncated] = parse_channel_config( p_config, config_option_len );
    size_left -= config_option_len;
    size_parsed += config_option_len;

    if( is_truncated )
    {
        LogUtilError() << "ConfigResponse: remote send truncated configuration options";
    }

    std::shared_ptr<l2cap_config_response> request;
    request = std::make_shared<l2cap_config_response>();
    request->m_acl_handle = get_acl_handle();
    request->m_continue_flag = flag;
    request->m_identifier = identifier;
    request->m_source_cid = local_cid;
    request->m_options = options;
    request->m_result = static_cast<channel_config_result>( result );
    request->m_unkown_option_types = unknown_types;
    request->set_sender( m_remote_address );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    request->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( request );
    }
    else
    {
        LogUtilError() << "No signaling response handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_disconnect_request( uint8_t const* a_raw_sig, uint16_t a_size )
{
    std::shared_ptr<l2cap_disconnect_request> request;
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "DisconnectRequest parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }
    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "DisconnectRequest parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    constexpr uint16_t REQUIRED_PAYLOAD_LEN = 4;
    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    if( data_size != REQUIRED_PAYLOAD_LEN ||
        size_left < 4 )
    {
        LogUtilWarning() << "DisconnectRequest parse fail: invalid length not 4";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        */
        size_parsed = a_size;
        return size_parsed;
    }

    uint16_t destination_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t source_cid = le_to_host16( a_raw_sig + 6 );
    size_parsed += 4;
    size_left -= 4;

    request = std::make_shared<l2cap_disconnect_request>();
    request->m_identifier = identifier;
    request->m_destination_cid = destination_cid;
    request->m_source_cid = source_cid;
    request->m_connection_handle = m_sig_header.get_acl_handle();
    request->set_sender( m_remote_address );

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    request->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( request );
    }
    else
    {
        LogUtilError() << "No signaling request handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_disconnect_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t signal_data_length = 0u;

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2u )
    {
        LogUtilWarning() << "DisconnectResponse parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }
    uint8_t identifier = a_raw_sig[1];
    size_left -= 2u;
    size_parsed += 2u;
    cancel_timer_for_command( identifier );

    if( size_left < 2u )
    {
        LogUtilWarning() << "DisconnectResponse parse fail: buffer too small for signal data length";
        size_parsed = a_size;
        return size_parsed;
    }
    signal_data_length = le_to_host16( a_raw_sig + 2 );
    size_left -= 2u;
    size_parsed += 2u;

    // DISCONNECT_RSP payload: Source CID(2) + Dest CID(2), min 4 bytes
    if( signal_data_length < 4u )
    {
        LogUtilWarning() << "DisconnectResponse parse fail: signal data length too small, minimum require 4 bytes";
        size_parsed = a_size;
        return size_parsed;
    }

    if( size_left < 4u )
    {
        LogUtilWarning() << "DisconnectResponse parse fail: buffer too small for source CID + dest CID";
        size_parsed = a_size;
        return size_parsed;
    }
    uint16_t source_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t dest_cid = le_to_host16( a_raw_sig + 6 );
    size_left -= 4u;
    size_parsed += 4u;

    auto request = std::make_shared<l2cap_disconnect_response>();
    request->m_identifier = identifier;
    request->m_source_cid = source_cid;
    request->m_destination_cid = dest_cid;
    request->set_sender( m_remote_address );

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    request->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( request );
    }
    else
    {
        LogUtilError() << "No signaling response handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_connection_parameter_update_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "ConnParamUpdateReq parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }
    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;

    if( m_acl_type != acl_type::le_acl )
    {
        LogUtilWarning() << "ConnParamUpdateReq: Not LE ACL link, reject this command";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        size_parsed = a_size;
        size_left = a_size;
        return size_parsed;
    }

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "ConnParamUpdateReq parse fail: buffer too small, cannot read data length";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        // NOTE: This is Request, but frame truncated, cannot fetch identifier to send reject
        return size_parsed;
    }

    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    constexpr uint16_t PARAM_UPDATE_PAYLOAD_FIXED = 8u;
    if( data_size != PARAM_UPDATE_PAYLOAD_FIXED || size_left < data_size )
    {
        LogUtilWarning() << "ConnParamUpdateReq parse fail: payload size invalid, data_size=" << data_size;
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        */
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    const uint8_t* p_payload = a_raw_sig + 4;
    auto param_req = std::make_shared<l2cap_connection_parameter_update_request>();
    param_req->m_min_interval = le_to_host16( p_payload );
    param_req->m_max_interval = le_to_host16( p_payload + 2 );
    param_req->m_latency = le_to_host16( p_payload + 4 );
    param_req->m_timerout_timeout = le_to_host16( p_payload + 6 );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    param_req->set_receiver( the_controller->get_address() );
    param_req->set_sender( m_remote_address );

    LogUtilInfo() << "Received ConnParamUpdateReq: min=" << param_req->m_min_interval
        << ", max=" << param_req->m_max_interval
        << ", latency=" << param_req->m_latency
        << ", timeout=" << param_req->m_timerout_timeout;

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( param_req );
    }
    else
    {
        LogUtilError() << "No signaling handler for connection parameter update request";
    }

    size_parsed += data_size;
    size_left -= data_size;
    return size_parsed;
}

uint16_t l2cap_signaling::handle_connection_parameter_update_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_length = 0u;

    if( m_acl_type != acl_type::le_acl )
    {
        LogUtilError() << "ConnectionParameterUpdateResponse only support LE ACL connection.";
        size_parsed = a_size;
        return size_parsed;
    }

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_length ) )
    {
        size_parsed = a_size;
        cancel_timer_for_command( identifier );
        return size_parsed;
    }

    cancel_timer_for_command( identifier );
    if( signal_data_length < 2u )
    {
        LogUtilWarning() << "ConnParamUpdateRsp parse fail: signal data length too small, minimum require 2 bytes";
        size_parsed = a_size;
        return size_parsed;
    }

    if( size_left < 2u )
    {
        LogUtilWarning() << "ConnParamUpdateRsp parse fail: buffer too small for result field";
        size_parsed = a_size;
        return size_parsed;
    }

    uint16_t result_code = le_to_host16( a_raw_sig + 4 );
    size_left -= 2u;
    size_parsed += 2u;

    auto rsp = std::make_shared<l2cap_connection_parameter_update_response>();
    rsp->m_identifier = identifier;
    rsp->m_result = static_cast<conn_param_update_result>( result_code );
    rsp->set_sender( m_remote_address );

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    rsp->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( rsp );
    }
    else
    {
        LogUtilError() << "No signaling response handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_le_credit_based_connection_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_length = 0u;

    if( m_acl_type != acl_type::le_acl )
    {
        LogUtilError() << "LeCreditBasedConnectionReq only support LE ACL connection.";
        size_parsed = a_size;
        return size_parsed;
    }

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_length ) )
    {
        size_parsed = a_size;
        if( identifier != 0u )
        {
            send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        }
        return size_parsed;
    }

    //SPSM(2)+SrcCID(2)+MTU(2)+MPS(2)+InitialCredits(2)
    if( signal_data_length < 10u )
    {
        LogUtilWarning() << "LeCreditBasedConnectionReq parse fail: signal data length too small,"
            " minimum require 10 bytes";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    if( size_left < 10u )
    {
        LogUtilWarning() << "LeCreditBasedConnectionReq parse fail: buffer too small for request payload";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    uint16_t spsm = le_to_host16( a_raw_sig + 4 );
    uint16_t src_cid = le_to_host16( a_raw_sig + 6 );
    uint16_t mtu = le_to_host16( a_raw_sig + 8 );
    uint16_t mps = le_to_host16( a_raw_sig + 10 );
    uint16_t init_credits = le_to_host16( a_raw_sig + 12 );
    size_left -= 10u;
    size_parsed += 10u;

    auto req = std::make_shared<l2cap_le_credit_based_connection_request>();
    req->m_identifier = identifier;
    req->m_mtu = mtu;
    req->m_mps = mps;
    req->m_spsm = spsm;
    req->m_initial_credits = init_credits;
    req->set_sender( m_remote_address );

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    req->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( req );
    }
    else
    {
        LogUtilError() << "No signaling request handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_le_credit_based_connection_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_length = 0u;

    if( m_acl_type != acl_type::le_acl )
    {
        LogUtilError() << "LeCreditBasedConnectionRsp only support LE ACL connection.";
        size_parsed = a_size;
        return size_parsed;
    }

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_length ) )
    {
        size_parsed = a_size;
        // Response packet: parse fail, do NOT send command reject
        cancel_timer_for_command( identifier );
        return size_parsed;
    }

    cancel_timer_for_command( identifier );
    // DestCID(2) + MTU(2) + MPS(2) + InitialCredits(2) + Result(2) = 10 bytes
    if( signal_data_length < 10u )
    {
        LogUtilWarning() << "LeCreditBasedConnectionRsp parse fail: signal data length too small,"
            " minimum require 10 bytes";
        size_parsed = a_size;
        return size_parsed;
    }

    if( size_left < 10u )
    {
        LogUtilWarning() << "LeCreditBasedConnectionRsp parse fail: buffer too small for response payload";
        size_parsed = a_size;
        return size_parsed;
    }

    uint16_t dest_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t mtu = le_to_host16( a_raw_sig + 6 );
    uint16_t mps = le_to_host16( a_raw_sig + 8 );
    uint16_t init_credits = le_to_host16( a_raw_sig + 10 );
    uint16_t result_code = le_to_host16( a_raw_sig + 12 );

    size_left -= 10u;
    size_parsed += 10u;

    auto rsp = std::make_shared<l2cap_le_credit_based_connection_response>();
    rsp->m_identifier = identifier;
    rsp->m_mtu = mtu;
    rsp->m_mps = mps;
    rsp->m_initial_credits = init_credits;
    rsp->m_result = static_cast<le_credit_conn_result>( result_code );
    rsp->set_sender( m_remote_address );

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    rsp->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( rsp );
    }
    else
    {
        LogUtilError() << "No signaling response handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_le_flow_control_credit_ind
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_length = 0u;

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_length ) )
    {
        size_parsed = a_size;
        return size_parsed;
    }

    // DestCID(2) + AdditionalCredits(2) = 4 bytes
    if( signal_data_length < 4u )
    {
        LogUtilWarning() << "LeFlowControlCreditInd parse fail: signal data length too small,"
            " minimum require 4 bytes";
        size_parsed = a_size;
        return size_parsed;
    }

    if( size_left < 4u )
    {
        LogUtilWarning() << "LeFlowControlCreditInd parse fail: buffer too small for payload";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    uint16_t dest_cid = le_to_host16( a_raw_sig + 4 );
    uint16_t add_credits = le_to_host16( a_raw_sig + 6 );
    size_left -= 4u;
    size_parsed += 4u;

    auto ind = std::make_shared<l2cap_le_flow_control_credit_indication>();
    ind->m_identifier = identifier;
    ind->m_remote_cid = dest_cid;
    ind->m_additional_credits = add_credits;
    ind->set_sender( m_remote_address );

    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    ind->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( ind );
    }
    else
    {
        LogUtilError() << "No signaling indication handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_credit_based_connection_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_size = 0u;
    uint32_t signal_data_length = 0u;

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_size ) )
    {
        size_parsed = a_size;
        if( identifier != 0u )
        {
            send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        }
        return size_parsed;
    }
    signal_data_length = signal_data_size;

    /*
     * Enhanced Credit Based Connection Request (Code=0x17)
     * Payload: SPSM(2) + MTU(2) + MPS(2) + InitialCredits(2) + SourceCID[]
     * Fixed part total 8 octets, SourceCID count derived from remaining payload bytes
     */
    const uint16_t fixed_len = 8u;
    if( signal_data_length < fixed_len )
    {
        LogUtilWarning() << "CreditBasedConnectionReq parse fail: signal data length too small,"
            " minimum require " << fixed_len << " bytes";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }
    if( size_left < fixed_len )
    {
        LogUtilWarning() << "CreditBasedConnectionReq parse fail: buffer too small for fixed payload";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    // Parse fixed fields strictly follow spec table order
    uint16_t spsm = le_to_host16( a_raw_sig + 4 );
    uint16_t mtu = le_to_host16( a_raw_sig + 6 );
    uint16_t mps = le_to_host16( a_raw_sig + 8 );
    uint16_t init_credits = le_to_host16( a_raw_sig + 10 );

    uint32_t source_cid_count = ( signal_data_length - fixed_len ) / 2u;
    uint32_t var_payload_len = source_cid_count * 2u;
    constexpr uint16_t max_cid_cnt = 5u;

    if( var_payload_len > UINT16_MAX )
    {
        LogUtilWarning() << "CreditBasedConnectionReq parse fail: var payload length exceed uint16 max";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    if( signal_data_length < fixed_len + var_payload_len )
    {
        LogUtilWarning() << "CreditBasedConnectionReq parse fail: signal data length insufficient for cid array";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    if( size_left < static_cast<uint16_t>( fixed_len + var_payload_len ) )
    {
        LogUtilWarning() << "CreditBasedConnectionReq parse fail: buffer too small for cid array";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    if( source_cid_count > max_cid_cnt )
    {
        LogUtilWarning() << "CreditBasedConnectionReq parse fail: remote cid count("
            << source_cid_count << ") exceeds max limit " << max_cid_cnt;
        size_parsed += fixed_len + static_cast<uint16_t>( var_payload_len );
        if( size_parsed > a_size )
        {
            size_parsed = a_size;
        }
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    size_left -= fixed_len;
    size_parsed += fixed_len;

    auto req = std::make_shared<l2cap_credit_based_connection_request>();
    req->m_identifier = identifier;
    req->m_spsm = spsm;
    req->m_mtu = mtu;
    req->m_mps = mps;
    req->m_initial_credits = init_credits;
    req->m_remote_cid_count = static_cast<uint16_t>( source_cid_count );;

    for( uint32_t i = 0; i < source_cid_count; i++ )
    {
        req->m_remote_cid[i] = le_to_host16( a_raw_sig + 4 + fixed_len + i * 2u );
    }

    size_left -= static_cast<uint16_t>( var_payload_len );
    size_parsed += static_cast<uint16_t>( var_payload_len );

    req->set_sender( m_remote_address );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    req->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( req );
    }
    else
    {
        LogUtilError() << "No signaling request handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_credit_based_connection_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_size = 0u;
    uint32_t signal_data_length = 0u;
    bool canceled = false;

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_size ) )
    {
        size_parsed = a_size;
        if( identifier != 0u )
        {
            canceled = cancel_timer_for_command( identifier );
        }
        return size_parsed;
    }
    signal_data_length = signal_data_size;
    cancel_timer_for_command( identifier );

    /*
     * Enhanced Credit Based Connection Response (Code=0x18)
     * Payload: MTU(2) + MPS(2) + InitialCredits(2) + Result(2) + DestinationCID[]
     * Fixed part total 8 octets
     */
    const uint16_t fixed_len = 8u;
    if( signal_data_length < fixed_len )
    {
        LogUtilWarning() << "CreditBasedConnectionRsp parse fail: signal data length too small, minimum require " << fixed_len << " bytes";
        size_parsed = a_size;
        return size_parsed;
    }
    if( size_left < fixed_len )
    {
        LogUtilWarning() << "CreditBasedConnectionRsp parse fail: buffer too small for fixed payload";
        size_parsed = a_size;
        return size_parsed;
    }

    // Parse fixed fields strictly follow spec table order
    uint16_t mtu = le_to_host16( a_raw_sig + 4 );
    uint16_t mps = le_to_host16( a_raw_sig + 6 );
    uint16_t init_credits = le_to_host16( a_raw_sig + 8 );
    uint16_t raw_result = le_to_host16( a_raw_sig + 10 );

    // Number of Destination CID = (signal_data_length - fixed_len) / 2
    uint32_t dest_cid_count = ( signal_data_length - fixed_len ) / 2u;
    uint32_t var_payload_len = dest_cid_count * 2u;
    constexpr uint16_t max_dest_cid_cnt = 5u;

    if( var_payload_len > UINT16_MAX )
    {
        LogUtilWarning() << "CreditBasedConnectionRsp parse fail: var payload length exceed uint16 max";
        size_parsed = a_size;
        return size_parsed;
    }
    if( signal_data_length < fixed_len + var_payload_len )
    {
        LogUtilWarning() << "CreditBasedConnectionRsp parse fail: signal data length insufficient for destination CID array";
        size_parsed = a_size;
        return size_parsed;
    }
    if( size_left < static_cast<uint16_t>( fixed_len + var_payload_len ) )
    {
        LogUtilWarning() << "CreditBasedConnectionRsp parse fail: buffer too small for destination CID array";
        size_parsed = a_size;
        return size_parsed;
    }
    if( dest_cid_count > max_dest_cid_cnt )
    {
        LogUtilWarning() << "CreditBasedConnectionRsp parse fail: destination cid count("
            << dest_cid_count << ") exceeds max limit " << max_dest_cid_cnt;
        size_parsed += fixed_len + static_cast<uint16_t>( var_payload_len );
        if( size_parsed > a_size )
        {
            size_parsed = a_size;
        }
        return size_parsed;
    }

    size_left -= fixed_len;
    size_parsed += fixed_len;

    auto rsp = std::make_shared<l2cap_credit_based_connection_response>();
    rsp->m_identifier = identifier;
    rsp->m_mtu = mtu;
    rsp->m_mps = mps;
    rsp->m_initial_credits = init_credits;
    rsp->m_result = static_cast<l2cap_credit_conn_result_code>( raw_result );
    rsp->m_local_cid_count = static_cast<uint16_t>( dest_cid_count );

    for( uint32_t i = 0; i < dest_cid_count; i++ )
    {
        rsp->m_local_cid[i] = le_to_host16( a_raw_sig + 4 + fixed_len + i * 2u );
    }

    size_left -= static_cast<uint16_t>( var_payload_len );
    size_parsed += static_cast<uint16_t>( var_payload_len );

    // Deliver parsed packet to upper handler
    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( rsp );
    }
    else
    {
        LogUtilError() << "CreditBasedConnectionRsp: No signaling packet handler!";
    }

    return size_parsed;
}

uint16_t l2cap_signaling::handle_credit_based_reconfig_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_size = 0u;
    uint32_t signal_data_length = 0u;

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_size ) )
    {
        size_parsed = a_size;
        if( identifier != 0u )
        {
            send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        }
        return size_parsed;
    }
    signal_data_length = signal_data_size;

    /*
     * Credit Based Reconfig Request (Code=0x19)
     * Payload: MTU(2) + MPS(2) + DestinationCID[]
     * Fixed part total 4 octets, DestinationCID count derived from remaining payload bytes
     */
    const uint16_t fixed_len = 4u;
    if( signal_data_length < fixed_len )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_REQ parse fail: signal data length too small,"
            " minimum require " << fixed_len << " bytes";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }
    if( size_left < fixed_len )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_REQ parse fail: buffer too small for fixed payload";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    // Parse fixed fields strictly follow spec table order
    uint16_t mtu = le_to_host16( a_raw_sig + 4 );
    uint16_t mps = le_to_host16( a_raw_sig + 6 );

    uint32_t dest_cid_count = ( signal_data_length - fixed_len ) / 2u;
    uint32_t var_payload_len = dest_cid_count * 2u;
    constexpr uint16_t max_cid_cnt = 5u;

    if( var_payload_len > UINT16_MAX )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_REQ parse fail: var payload length exceed uint16 max";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }
    if( signal_data_length < fixed_len + var_payload_len )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_REQ parse fail: signal data length insufficient for destination cid array";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }
    if( size_left < static_cast<uint16_t>( fixed_len + var_payload_len ) )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_REQ parse fail: buffer too small for destination cid array";
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }
    if( dest_cid_count > max_cid_cnt )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_REQ parse fail: destination cid count("
            << dest_cid_count << ") exceeds max limit " << max_cid_cnt;
        size_parsed += fixed_len + static_cast<uint16_t>( var_payload_len );
        if( size_parsed > a_size )
        {
            size_parsed = a_size;
        }
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    size_left -= fixed_len;
    size_parsed += fixed_len;

    auto req = std::make_shared<l2cap_credit_based_reconfig_request>();
    req->m_identifier = identifier;
    req->m_mtu = mtu;
    req->m_mps = mps;
    req->m_remote_cid_count = static_cast<uint16_t>( dest_cid_count );

    for( uint32_t i = 0; i < dest_cid_count; i++ )
    {
        req->m_remote_cid[i] = le_to_host16( a_raw_sig + 4 + fixed_len + i * 2u );
    }

    size_left -= static_cast<uint16_t>( var_payload_len );
    size_parsed += static_cast<uint16_t>( var_payload_len );

    req->set_sender( m_remote_address );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    req->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( req );
    }
    else
    {
        LogUtilError() << "CreditBasedReconfigReq: No signaling request handler!";
    }
    return size_parsed;
}

uint16_t l2cap_signaling::handle_credit_based_reconfig_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint8_t identifier = 0;
    uint16_t signal_data_size = 0u;
    uint32_t signal_data_length = 0u;

    if( !parse_signaling_header( a_raw_sig, size_left, size_parsed, identifier, signal_data_size ) )
    {
        size_parsed = a_size;
        cancel_timer_for_command( identifier );
        return size_parsed;
    }

    signal_data_length = signal_data_size;
    cancel_timer_for_command( identifier );

    /*
     * Credit Based Reconfig Response (Code=0x1A)
     * Payload: Result(2 octets)
     * Fixed part total 2 octets, no variable length array
     */
    const uint16_t fixed_len = 2u;
    if( signal_data_length < fixed_len )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_RSP parse fail: signal data length too small,"
            " minimum require " << fixed_len << " bytes";
        size_parsed = a_size;
        return size_parsed;
    }
    if( size_left < fixed_len )
    {
        LogUtilWarning() << "L2CAP_CREDIT_BASED_RECONFIGURE_RSP parse fail: buffer too small for fixed payload";
        size_parsed = a_size;
        return size_parsed;
    }

    // Parse Result field (little-endian)
    uint16_t raw_result = le_to_host16( a_raw_sig + 4 );
    l2cap_reconfig_result_code result_code;
    result_code = static_cast<l2cap_reconfig_result_code>( raw_result );

    size_left -= fixed_len;
    size_parsed += fixed_len;

    auto rsp = std::make_shared<l2cap_credit_based_reconfig_response>();
    rsp->m_identifier = identifier;
    rsp->m_result = result_code;

    rsp->set_sender( m_remote_address );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    rsp->set_receiver( the_controller->get_address() );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( rsp );
    }
    else
    {
        LogUtilError() << "L2CAP_CREDIT_BASED_RECONFIGURE_RSP: No signaling response handler!";
    }
    return size_parsed;
}

uint16_t l2cap_signaling::handle_unknown_signaling_code( uint8_t const* a_raw_sig, uint16_t a_size )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;
    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "DisconnectRequest parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t sig_code = a_raw_sig[0];
    uint8_t identifier = a_raw_sig[1];
    size_parsed += 2;
    size_left -= 2;

    LogUtilWarning() << "Received unknown L2CAP signaling code: " << static_cast<uint32_t>( sig_code );

    if( size_left < 2 )
    {
        LogUtilWarning() << "Unknown signaling code parse fail: buffer too small, cannot read data length";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        size_parsed = a_size;
        return size_parsed;
    }

    data_size = le_to_host16( a_raw_sig + 2 );
    size_parsed += 2;
    size_left -= 2;
    if( size_left < data_size )
    {
        LogUtilWarning() << "Unknown signaling code parse fail: payload buffer insufficient";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        size_parsed = a_size;

        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        */
        return size_parsed;
    }
    size_parsed += data_size;
    size_left -= data_size;

    send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );

    return size_parsed;
}

uint16_t l2cap_signaling::handle_echo_request
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;
    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "EchoRequest parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "EchoRequest parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        return size_parsed;
    }

    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    if( size_left < data_size )
    {
        LogUtilWarning() << "EchoRequest parse fail: payload buffer insufficient";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        */
        size_parsed = a_size;
        return size_parsed;
    }

    if( get_acl_type() == acl_type::le_acl )
    {
        LogUtilError() << "LE ACL connection: ECHO_REQUEST is not supported.";
        send_reject_rsp( identifier, l2cap_command_reject_reason::unknown_command, nullptr, 0 );
        size_parsed = a_size;
        size_left = 0;
        return size_parsed;
    }

    m_sig_header.set_identifier( identifier );
    m_sig_header.set_signaling_code( signaling_code::l2cap_echo_rsp );
    m_sig_header.set_sdu_length( data_size );

    std::vector<uint8_t> buffer;
    buffer.resize( m_sig_header.header_size() + data_size );
    m_sig_header.to_raw_buffer( buffer.data(), buffer.size() );
    memcpy( buffer.data() + m_sig_header.header_size(), a_raw_sig + 4, data_size );
    send_completed_acl_packet( std::move( buffer ) );

    size_parsed += data_size;
    size_left -= data_size;
    return size_parsed;
}

uint16_t l2cap_signaling::handle_echo_response
    (
    uint8_t const* a_raw_sig,
    uint16_t a_size
    )
{
    LogUtilInfo() << "received echo response from remote device.";

    uint16_t size_parsed = 0u;
    uint16_t size_left = a_size;
    uint16_t data_size = 0u;

    if( m_acl_type == acl_type::le_acl )
    {
        LogUtilError() << "LE ACL connection response is not supported.";
        size_parsed = a_size;
        size_left = 0;
        return size_parsed;
    }

    /*
     * At least 2 octets required to read Code and Identifier from signaling header.
     * Buffer may contain subsequent signaling commands after current command.
     */
    if( size_left < 2 )
    {
        LogUtilWarning() << "EchoResponse parse fail: buffer too small, cannot read Identifier";
        size_parsed = a_size;
        return size_parsed;
    }

    uint8_t identifier = a_raw_sig[1];
    size_left -= 2; /*Consumed 1 octet for Code field and 1 octet for Identifier field.*/
    size_parsed += 2;

    if( size_left < 2 )
    {
        /* we need parse the length */
        LogUtilWarning() << "EchoResponse parse fail: buffer too small, cannot read data length";
        /*Cannot get data_size, unknown command boundary. Discard all remaining buffer.*/
        size_parsed = a_size;
        // NOTE: ECHO_RESPONSE is Response, MUST NOT send Command Reject
        return size_parsed;
    }

    data_size = le_to_host16( a_raw_sig + 2 );
    size_left -= 2; /* Consumed 2 octets for Length field.*/
    size_parsed += 2;

    if( size_left < data_size )
    {
        LogUtilWarning() << "EchoResponse parse fail: payload buffer insufficient, data_size=" << data_size;
        /*
        * Remote filled incorrect data_size value, cannot trust this length field.
        * Command boundary is unreliable, cannot safely skip to next command.
        * Consume all remaining buffer and stop further parsing in this PDU.
        * NOTE: Response frame, no Command Reject reply.
        */
        size_parsed = a_size;
        return size_parsed;
    }

    const uint8_t* p_echo_data = a_raw_sig + 4;
    LogUtilInfo() << "received echo response from remote device, identifier: " << identifier
        << ", echo data len: " << data_size;

    auto echo_rsp = std::make_shared<l2cap_echo_response>();
    echo_rsp->m_identifier = identifier;
    echo_rsp->m_echo_data.assign( p_echo_data, p_echo_data + data_size );
    auto the_controller = framework::framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    echo_rsp->set_receiver( the_controller->get_address() );
    echo_rsp->set_sender( m_remote_address );

    if( m_sig_pkt_handler )
    {
        m_sig_pkt_handler( echo_rsp );
    }
    else
    {
        LogUtilError() << "No signaling handler for echo response";
    }

    for( auto it = m_commands_sent.begin(); it != m_commands_sent.end(); ++it )
    {
        auto& ele = *it;
        if( ele.m_sent_command->m_identifier == identifier )
        {
            cancel_timer( ele.m_registered_time_out_timer_id );
            m_commands_sent.erase( it );
            break;
        }
    }

    size_parsed += data_size;
    size_left -= data_size;
    return size_parsed;
}

void l2cap_signaling::send_completed_acl_packet( std::vector<uint8_t> a_acl_packet )
{
    std::shared_ptr<hci_data> hci_send = std::make_shared<hci_data>();
    hci_send->m_buffer = std::move( a_acl_packet );
    hci_send->m_type = uart_hci_type::acl_type;
    hci_send->m_from_controller = false;

    std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address> hci_task;
    hci_task = std::make_shared<l2cap_task_send_l2cap_sdu_with_remote_address>();
    hci_task->m_acl_handle = m_sig_header.get_acl_handle();
    hci_task->m_local_cid = m_sig_header.get_channel_id();
    hci_task->m_remote_address = m_remote_address;
    hci_task->m_hci_packet = std::move( hci_send );

    hci_task->set_target_module( l2cap_module::s_l2cap_module_name );
    hci_task->set_source_module( l2cap_module::s_l2cap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

}

