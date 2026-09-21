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

#include "protocol_headers.h"
#include "endian_convert.h"
#include "framework/log_util.h"
#include "stream_writer.h"

namespace bluetooth
{

acl_header::~acl_header()
{

}

void acl_header::make_l2cap_first_field( uint8_t* a_buffer )
{
    write_le16( a_buffer, m_connection_handle );
    uint8_t temp = ( ( m_bc_flag << 2 ) + m_pb_flag ) << 4;
    a_buffer[1] |= temp;
}

void acl_header::set_packet_boundary( uint8_t a_pb_flag )
{
    if( a_pb_flag > 0b11 )
    {
        LogUtilError() << "Wrong packet boundary flag";
        return;
    }
    m_pb_flag = a_pb_flag;
    make_l2cap_first_field( m_first_two_bytes );
}

void acl_header::set_broadcast_flag( uint8_t a_bc_flag )
{
    if( a_bc_flag > 0b11 )
    {
        LogUtilError() << "Wrong broadcast flag";
        return;
    }
    m_bc_flag = a_bc_flag;
    make_l2cap_first_field( m_first_two_bytes );
}

void acl_header::set_sdu_length( uint16_t a_length )
{
    m_length_in_acl = a_length;
}

void acl_header::to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )
{
    stream_writer writer( a_buffer, a_size );
    writer.write_buffer( m_first_two_bytes, sizeof( m_first_two_bytes ) );
    writer << m_length_in_acl;
}

uint16_t acl_header::header_size()const
{
    return 4;
}

void acl_header::parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )
{
    uint8_t handle[2];
    handle[0] = a_buffer[0];
    handle[1] = a_buffer[1] & 0x0F;
    m_connection_handle = le_to_host16( handle );

    uint8_t temp = a_buffer[1] >> 4;
    m_bc_flag = temp >> 2;
    m_pb_flag = temp & 0x03;

    m_length_in_acl = le_to_host16( a_buffer + 2 );
}

void l2cap_header::set_sdu_length( uint16_t a_length )
{
    m_length_in_l2cap = a_length;
    acl_header::set_sdu_length( a_length + 4 );
}

void l2cap_header::to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )
{
    uint8_t* p_buffer = a_buffer + acl_header::header_size();
    acl_header::to_raw_buffer( a_buffer, a_size );

    stream_writer writer( p_buffer, a_size - acl_header::header_size() );
    writer << m_length_in_l2cap << m_channel_id;
}

uint16_t l2cap_header::header_size()const
{
    return 4 + acl_header::header_size();
}

void l2cap_header::parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )
{
    acl_header::parse_from_raw_data( a_buffer, a_size );

    uint8_t* p_offset = a_buffer + acl_header::header_size();
    m_length_in_l2cap = le_to_host16( p_offset );
    m_channel_id = le_to_host16( p_offset + 2 );
}

}

