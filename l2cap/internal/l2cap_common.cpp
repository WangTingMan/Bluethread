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

#include "../l2cap_common.h"
#include "endian_convert.h"
#include "framework/log_util.h"

#include "../../common/stream_writer.h"

namespace bluetooth
{

template<>
stream_writer& stream_writer::operator<<( signaling_code const& a_value )
{
    return *this << static_cast< uint8_t >( a_value );
}

template<>
stream_writer& stream_writer::operator<<( connection_req_result const& a_value )
{
    return *this << static_cast< uint16_t >( a_value );
}

template<>
stream_writer& stream_writer::operator<<( qos_type const& a_value )
{
    return *this << static_cast< uint8_t >( a_value );
}

template<>
stream_writer& stream_writer::operator<<( connection_req_refused_status const& a_value )
{
    return *this << static_cast< uint16_t >( a_value );
}

template<>
stream_writer& stream_writer::operator<<( channel_config_result const& a_value )
{
    return *this << static_cast< uint16_t >( a_value );
}

template<>
stream_writer& stream_writer::operator<<( l2cap_command_reject_reason const& a_value )
{
    return *this << static_cast< uint16_t >( a_value );
}

template<>
stream_writer& stream_writer::operator<<( channel_config_option const& a_value )
{
    *this << static_cast< uint8_t >( a_value.m_type );
    switch( a_value.m_type )
    {
    case channel_config_option_type::mtu:
        *this << static_cast< uint8_t >( 0x02 ) << a_value.m_option.m_mtu;
        break;
    case channel_config_option_type::flush_timeout:
        *this << static_cast< uint8_t >( 0x02 ) << a_value.m_option.m_flush_timeout;
        break;
    case channel_config_option_type::qos:
        *this << static_cast< uint8_t >( 22 ) << static_cast< uint8_t >( 0x00 )
            << a_value.m_option.m_qos.m_qos_type << a_value.m_option.m_qos.m_token_rate
            << a_value.m_option.m_qos.m_token_bucket_size
            << a_value.m_option.m_qos.m_peak_bandwidth
            << a_value.m_option.m_qos.m_latency
            << a_value.m_option.m_qos.m_delay_variation;
        break;
    case channel_config_option_type::retransmission_flow_control:
        *this << static_cast< uint8_t >( 9 )
            << static_cast< uint8_t >( a_value.m_option.m_flow_control_retransmission.m_mode )
            << a_value.m_option.m_flow_control_retransmission.m_tx_windows_size
            << a_value.m_option.m_flow_control_retransmission.m_max_transmit
            << a_value.m_option.m_flow_control_retransmission.m_retransmission_timeout
            << a_value.m_option.m_flow_control_retransmission.m_monitor_timeout
            << a_value.m_option.m_flow_control_retransmission.m_max_pdu_size;
        break;
    case channel_config_option_type::fcs:
        *this << static_cast< uint8_t >( 1 ) << a_value.m_option.m_fcs;
        break;
    case channel_config_option_type::extended_flow:
        *this << static_cast< uint8_t >( 16 ) << a_value.m_option.m_ext_flow.m_identifier
            << a_value.m_option.m_ext_flow.m_qos_type << a_value.m_option.m_ext_flow.m_max_sdu_size
            << a_value.m_option.m_ext_flow.m_sdu_inter_arrival_time
            << a_value.m_option.m_ext_flow.m_access_latency
            << a_value.m_option.m_ext_flow.m_flush_timeout;
        break;
    case channel_config_option_type::extended_window_size:
        *this << static_cast< uint8_t >( 2 ) << a_value.m_option.m_ext_window_size;
        break;
    default:
        LogUtilError() << "Unkown config option type.";
        break;
    }
    return *this;
}

signaling_channel_packet::~signaling_channel_packet()
{

}

uint16_t signaling_channel_packet::parse_header( uint8_t const* a_buffer, uint16_t a_size )
{
    m_signaling_code = static_cast<signaling_code>( a_buffer[0] );
    m_identifier = a_buffer[1];
    m_length = le_to_host16( a_buffer + 2 );
    /* Header always requires 4 bytes */
    return 4;
}

uint16_t signaling_channel_packet::fill_header_to_raw( uint8_t* a_buffer, uint16_t a_size )
{
    a_buffer[0] = static_cast< uint8_t >( m_signaling_code );
    a_buffer[1] = m_identifier;
    m_length = get_payload_length();
    write_le16( a_buffer + 2, m_length );
    return 4;
}

command_reject::command_reject()
{
    m_signaling_code = signaling_code::l2cap_command_reject_rsp;
}

uint16_t command_reject::get_payload_length()
{
    return sizeof( m_reject_reason ) + m_reject_data.size();
}

uint16_t command_reject::parse_from_raw( uint8_t const* a_buffer, uint16_t a_size )
{
    uint16_t parsed_size = parse_header( a_buffer, a_size );
    uint8_t const* ptr = a_buffer + parsed_size;
    m_reject_reason = static_cast<l2cap_command_reject_reason>( le_to_host16( ptr ) );
    m_reject_data.resize( m_length - 2 );
    memcpy( m_reject_data.data(), ptr + 2, m_length - 2 );
    return parsed_size + m_length;
}

uint16_t command_reject::fill_to_raw( uint8_t* a_buffer, uint16_t a_size )
{
    uint16_t consumed_size = fill_header_to_raw( a_buffer, a_size );
    uint8_t* ptr = a_buffer + consumed_size;
    write_le16( ptr, static_cast< uint16_t >( m_reject_reason ) );
    memcpy( ptr + 2, m_reject_data.data(), m_reject_data.size() );
    return m_length + 4;
}

void command_reject::set_unknown_command()
{
    m_reject_reason = l2cap_command_reject_reason::unknown_command;
    m_reject_data.clear();
    m_length = 2;
}

void command_reject::set_signaling_mtx_exceeded( uint16_t a_expected_mtu )
{
    m_reject_reason = l2cap_command_reject_reason::signaling_mtu_overflow;
    m_reject_data.resize( 2 );
    write_le16( m_reject_data.data(), a_expected_mtu );
    m_length = 4;
}

void command_reject::set_invalid_cid( uint16_t a_local_cid, uint16_t a_remote_cid )
{
    m_reject_reason = l2cap_command_reject_reason::invalid_cid;
    m_reject_data.resize( 4 );
    uint8_t* ptr = m_reject_data.data();
    write_le16( ptr, a_local_cid );
    write_le16( ptr + 2, a_remote_cid );
    m_length = 6;
}

connection_request::connection_request()
{
    m_signaling_code = signaling_code::l2cap_connection_req;
}

l2cap_connect_response::l2cap_connect_response()
{
    m_signaling_code = signaling_code::l2cap_connection_rsp;
}

l2cap_config_request::l2cap_config_request()
{
    m_signaling_code = signaling_code::l2cap_configuration_req;
}

l2cap_config_response::l2cap_config_response()
{
    m_signaling_code = signaling_code::l2cap_configuration_rsp;
}

l2cap_disconnect_request::l2cap_disconnect_request()
{
    m_signaling_code = signaling_code::l2cap_disconnection_req;
}

l2cap_disconnect_response::l2cap_disconnect_response()
{
    m_signaling_code = signaling_code::l2cap_disconnection_rsp;
}

l2cap_echo_request::l2cap_echo_request()
{
    m_signaling_code = signaling_code::l2cap_echo_req;
}

l2cap_echo_response::l2cap_echo_response()
{
    m_signaling_code = signaling_code::l2cap_echo_rsp;
}

l2cap_information_request::l2cap_information_request()
{
    m_signaling_code = signaling_code::l2cap_information_req;
}

l2cap_information_response::l2cap_information_response()
{
    m_signaling_code = signaling_code::l2cap_information_rsp;
}

l2cap_connection_parameter_update_request::l2cap_connection_parameter_update_request()
{
    m_signaling_code = signaling_code::l2cap_connection_parameter_update_req;
}

l2cap_connection_parameter_update_response::l2cap_connection_parameter_update_response()
{
    m_signaling_code = signaling_code::l2cap_connection_parameter_update_rsp;
}

uint16_t retrieve_local_cid( std::shared_ptr<hci_data> const& a_acl_data )
{
    uint16_t local_cid = 0x00;
    if( a_acl_data->m_type == uart_hci_type::acl_type )
    {
        if( a_acl_data->m_buffer.size() >= 8 )
        {
            local_cid = le_to_host16( a_acl_data->m_buffer.data() + 6 );
        }
    }
    return local_cid;
}

std::tuple<std::vector<channel_config_option>, std::vector<uint8_t>, bool>
    parse_channel_config( uint8_t const* a_buffer, uint16_t a_size )
{
    uint16_t position = 0;
    uint16_t size_left = a_size;
    std::vector<channel_config_option> ret;
    std::vector<uint8_t> unknown_option_types;
    bool is_truncated = false;

    while( position < a_size )
    {
        if( position + 2 > a_size )
        {
            LogUtilError() << "Config option truncated, no enough bytes for type+len";
            is_truncated = true;
            break;
        }
        channel_config_option option;
        option.m_type = static_cast<channel_config_option_type>( a_buffer[position] );
        uint8_t option_size = a_buffer[position + 1];
        size_left -= 2; /* we already parsed two bytes. */

        switch( option.m_type )
        {
        case bluetooth::channel_config_option_type::mtu:
            if( size_left < 2 || option_size != 2 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_mtu = le_to_host16( a_buffer + position + 2 );
            ret.push_back( option );
            size_left -= 2;
            break;
        case bluetooth::channel_config_option_type::flush_timeout:
            if( size_left < 2 || option_size != 2 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_flush_timeout = le_to_host16( a_buffer + position + 2 );
            ret.push_back( option );
            size_left -= 2;
            break;
        case bluetooth::channel_config_option_type::qos:
            if( size_left < 22 || option_size != 22 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_qos.m_qos_type = static_cast< qos_type >( a_buffer[position + 3] );
            option.m_option.m_qos.m_token_rate = le_to_host32( a_buffer + position + 4 );
            option.m_option.m_qos.m_token_bucket_size = le_to_host32( a_buffer + position + 8 );
            option.m_option.m_qos.m_peak_bandwidth = le_to_host32( a_buffer + position + 12 );
            option.m_option.m_qos.m_latency = le_to_host32( a_buffer + position + 16 );
            option.m_option.m_qos.m_delay_variation = le_to_host32( a_buffer + position + 20 );
            ret.push_back( option );
            size_left -= 22;
            break;
        case bluetooth::channel_config_option_type::retransmission_flow_control:
            if( size_left < 9 || option_size != 9 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_flow_control_retransmission.m_mode =
                static_cast<retransmission_flow_mode_type>( a_buffer[position + 2] );
            option.m_option.m_flow_control_retransmission.m_tx_windows_size = a_buffer[position + 3];
            option.m_option.m_flow_control_retransmission.m_max_transmit = a_buffer[position + 4];
            option.m_option.m_flow_control_retransmission.m_retransmission_timeout = le_to_host16( a_buffer + position + 5 );
            option.m_option.m_flow_control_retransmission.m_monitor_timeout = le_to_host16( a_buffer + position + 7 );
            option.m_option.m_flow_control_retransmission.m_max_pdu_size = le_to_host16( a_buffer + position + 9 );
            ret.push_back( option );
            size_left -= 9;
            break;
        case bluetooth::channel_config_option_type::fcs:
            if( size_left < 1 || option_size != 1 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_fcs = a_buffer[position + 2];
            ret.push_back( option );
            size_left -= 1;
            break;
        case bluetooth::channel_config_option_type::extended_flow:
            if( size_left < 16 || option_size != 16 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_ext_flow.m_identifier = a_buffer[position + 2];
            option.m_option.m_ext_flow.m_qos_type = static_cast< qos_type >( a_buffer[position + 3] );
            option.m_option.m_ext_flow.m_max_sdu_size = le_to_host16( a_buffer + position + 4 );
            option.m_option.m_ext_flow.m_sdu_inter_arrival_time = le_to_host32( a_buffer + position + 6 );
            option.m_option.m_ext_flow.m_access_latency = le_to_host32( a_buffer + position + 10 );
            option.m_option.m_ext_flow.m_flush_timeout = le_to_host32( a_buffer + position + 14 );
            ret.push_back( option );
            size_left -= 16;
            break;
        case bluetooth::channel_config_option_type::extended_window_size:
            if( size_left < 2 || option_size != 2 )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            option.m_option.m_ext_window_size = le_to_host16( a_buffer + position + 2 );
            ret.push_back( option );
            size_left -= 2;
            break;
        default:
            LogUtilError() << "Unknown option type: " << static_cast< uint16_t >( option.m_type );
            unknown_option_types.push_back( a_buffer[position] );
            if( size_left < option_size )
            {
                LogUtilError() << "Config option truncated, no enough bytes for type+len";
                is_truncated = true;
                break;
            }
            size_left -= option_size;
            break;
        }

        if( is_truncated )
        {
            break;
        }
        position += option_size + 2;
    }
    return { ret, unknown_option_types, is_truncated };
}

std::ostream& operator<<( std::ostream& a_os, signaling_code a_signaling )
{
    switch( a_signaling )
    {
    case bluetooth::signaling_code::l2cap_command_reject_rsp:
        a_os << "l2cap_command_reject_rsp";
        break;
    case bluetooth::signaling_code::l2cap_connection_req:
        a_os << "l2cap_connection_req";
        break;
    case bluetooth::signaling_code::l2cap_connection_rsp:
        a_os << "l2cap_connection_rsp";
        break;
    case bluetooth::signaling_code::l2cap_configuration_req:
        a_os << "l2cap_configuration_req";
        break;
    case bluetooth::signaling_code::l2cap_configuration_rsp:
        a_os << "l2cap_configuration_rsp";
        break;
    case bluetooth::signaling_code::l2cap_disconnection_req:
        a_os << "l2cap_disconnection_req";
        break;
    case bluetooth::signaling_code::l2cap_disconnection_rsp:
        a_os << "l2cap_disconnection_rsp";
        break;
    case bluetooth::signaling_code::l2cap_echo_req:
        a_os << "l2cap_echo_req";
        break;
    case bluetooth::signaling_code::l2cap_echo_rsp:
        a_os << "l2cap_echo_rsp";
        break;
    case bluetooth::signaling_code::l2cap_information_req:
        a_os << "l2cap_information_req";
        break;
    case bluetooth::signaling_code::l2cap_information_rsp:
        a_os << "l2cap_information_rsp";
        break;
    case bluetooth::signaling_code::l2cap_connection_parameter_update_req:
        a_os << "l2cap_connection_parameter_update_req";
        break;
    case bluetooth::signaling_code::l2cap_connection_parameter_update_rsp:
        a_os << "l2cap_connection_parameter_update_rsp";
        break;
    case bluetooth::signaling_code::l2cap_le_credit_based_connection_req:
        a_os << "l2cap_le_credit_based_connection_req";
        break;
    case bluetooth::signaling_code::l2cap_le_credit_based_connection_rsp:
        a_os << "l2cap_le_credit_based_connection_rsp";
        break;
    case bluetooth::signaling_code::l2cap_flow_control_credit_ind:
        a_os << "l2cap_flow_control_credit_ind";
        break;
    case bluetooth::signaling_code::l2cap_credit_based_connection_req:
        a_os << "l2cap_credit_based_connection_req";
        break;
    case bluetooth::signaling_code::l2cap_credit_based_connection_rsp:
        a_os << "l2cap_credit_based_connection_rsp";
        break;
    case bluetooth::signaling_code::l2cap_credit_based_reconfigure_req:
        a_os << "l2cap_credit_based_reconfigure_req";
        break;
    case bluetooth::signaling_code::l2cap_credit_based_reconfigure_rsp:
        a_os << "l2cap_credit_based_reconfigure_rsp";
        break;
    default:
        a_os << "Unknown signaling code: " << static_cast< uint16_t >( a_signaling );
        break;
    }
    return a_os;
}

std::ostream& operator<<( std::ostream& a_os, l2cap_channel_state_type a_state )
{
    switch( a_state )
    {
    case bluetooth::l2cap_channel_state_type::close_state:
        a_os << "close";
        break;
    case bluetooth::l2cap_channel_state_type::wait_connect:
        a_os << "wait connect";
        break;
    case bluetooth::l2cap_channel_state_type::wait_config:
        a_os << "wait config";
        break;
    case bluetooth::l2cap_channel_state_type::wait_config_req_rsp:
        a_os << "wait_config_req_rsp";
        break;
    case bluetooth::l2cap_channel_state_type::wait_config_rsp:
        a_os << "wait_config_rsp";
        break;
    case bluetooth::l2cap_channel_state_type::open:
        a_os << "open";
        break;
    case l2cap_channel_state_type::wait_config_req:
        a_os << "wait_config_req";
        break;
    case l2cap_channel_state_type::wait_connect_rsp:
        a_os << "wait_connect_rsp";
        break;
    case l2cap_channel_state_type::wait_send_config:
        a_os << "wait_send_config";
        break;
    case l2cap_channel_state_type::wait_disconnect:
        a_os << "wait_disconnect";
        break;
    default:
        a_os << "Unknown: " << static_cast< uint16_t >( a_state );
        break;
    }
    return a_os;
}

std::ostream& operator<<( std::ostream& a_os, channel_config_result a_state )
{
    switch( a_state )
    {
    case bluetooth::channel_config_result::success:
        a_os << "success";
        break;
    case bluetooth::channel_config_result::unacceptable_parameters_failed:
        a_os << "unacceptable_parameters_failed";
        break;
    case bluetooth::channel_config_result::rejected_failed:
        a_os << "rejected_failed";
        break;
    case bluetooth::channel_config_result::unknown_options_failed:
        a_os << "unknown_options_failed";
        break;
    case bluetooth::channel_config_result::pending:
        a_os << "pending";
        break;
    case bluetooth::channel_config_result::flow_spec_rejected:
        a_os << "flow_spec_rejected";
        break;
    default:
        a_os << "unknown state: " << static_cast< uint16_t >( a_state );
        break;
    }
    return a_os;
}

std::ostream& operator<<(std::ostream& a_os, acl_type a_state)
{
    switch (a_state)
    {
    case acl_type::br_edr_acl:
        a_os << "br_edr_acl";
        break;
    case acl_type::le_acl:
        a_os << "le_acl";
        break;
    case acl_type::invalid_type:
        a_os << "invalid acl type";
        break;
    default:
        break;
    }

    return a_os;
}

std::ostream& operator<<( std::ostream& a_os, l2cap_channel_information_type a_type )
{
    switch( a_type )
    {
    case bluetooth::l2cap_channel_information_type::connectionless_mtu:
        a_os << "connectionless_mtu";
        break;
    case bluetooth::l2cap_channel_information_type::extended_features_supported:
        a_os << "extended_features_supported";
        break;
    case bluetooth::l2cap_channel_information_type::fixed_channel_supported:
        a_os << "fixed_channel_supported";
        break;
    default:
        break;
    }

    return a_os;
}

}

