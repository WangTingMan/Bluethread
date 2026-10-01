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

#include "sdp_protocol.h"
#include "endian_convert.h"

#include "framework/log_util.h"

namespace bluetooth
{

std::ostream& operator<<( std::ostream& a_os, sdp_pdu_id a_state )
{
    switch( a_state )
    {
    case bluetooth::sdp_pdu_id::sdp_error_rsp:
        a_os << "sdp_error_rsp";
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_req:
        a_os << "sdp_service_search_req";
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_rsp:
        a_os << "sdp_service_search_rsp";
        break;
    case bluetooth::sdp_pdu_id::sdp_service_attr_req:
        a_os << "sdp_service_attr_req";
        break;
    case bluetooth::sdp_pdu_id::sdp_service_attr_rsp:
        a_os << "sdp_service_attr_rsp";
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_attr_req:
        a_os << "sdp_service_search_attr_req";
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_attr_rsp:
        a_os << "sdp_service_search_attr_rsp";
        break;
    default:
        break;
    }
    return a_os;
}

void sdp_header::set_sdu_length( uint16_t a_length )
{
    l2cap_header::set_sdu_length( a_length + 5 );
    m_parameter_length = a_length;
}

void sdp_header::to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )
{
    uint8_t* p_buffer = a_buffer + l2cap_header::header_size();
    l2cap_header::to_raw_buffer( a_buffer, a_size );

    p_buffer[0] = static_cast< uint8_t >( m_pdu_id );
    write_be16( p_buffer + 1, m_transaction_id );
    write_be16( p_buffer + 3, m_parameter_length );
}

uint16_t sdp_header::header_size()const
{
    return 5 + l2cap_header::header_size();
}

void sdp_header::parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )
{
    l2cap_header::parse_from_raw_data( a_buffer, a_size );
    uint8_t* p_offset = a_buffer + l2cap_header::header_size();
    m_pdu_id = static_cast< sdp_pdu_id >( p_offset[0] );
    m_transaction_id = le_to_host16( p_offset + 1 );
    m_parameter_length = le_to_host16( p_offset + 3 );
}

}

