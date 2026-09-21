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

#include "stream_writer.h"
#include "endian_convert.h"
#include "framework/log_util.h"

namespace bluetooth
{

stream_writer& stream_writer::operator<<( uint32_t a_value )
{
    check_size( sizeof( uint32_t ) );
    write_le32( m_current_cursor, a_value );
    m_current_cursor += sizeof( uint32_t );
    return *this;
}

stream_writer& stream_writer::operator<<( uint16_t a_value )
{
    check_size( sizeof( uint16_t ) );
    write_le16( m_current_cursor, a_value );
    m_current_cursor += sizeof( uint16_t );
    return *this;
}

stream_writer& stream_writer::operator<<( uint8_t a_value )
{
    check_size( sizeof( uint8_t ) );
    *m_current_cursor = a_value;
    m_current_cursor += sizeof( uint8_t );
    return *this;
}

stream_writer& stream_writer::operator<<( bluetooth_address const& a_value )
{
    check_size( bluetooth_address::s_bluetooth_address_size );
    memcpy( m_current_cursor, a_value.address, bluetooth_address::s_bluetooth_address_size );
    m_current_cursor += bluetooth_address::s_bluetooth_address_size;
    return *this;
}

stream_writer& stream_writer::write_buffer( uint8_t const* a_buffer, uint32_t a_size )
{
    check_size( a_size );
    memcpy( m_current_cursor, a_buffer, a_size );
    m_current_cursor += a_size;
    return *this;
}

stream_writer& stream_writer::operator<<( std::vector<uint8_t> a_buffer )
{
    check_size( static_cast<uint32_t>( a_buffer.size() ) );
    memcpy( m_current_cursor, a_buffer.data(), a_buffer.size() );
    m_current_cursor += a_buffer.size();
    return *this;
}

void stream_writer::check_size( uint32_t a_size_to_write )const
{
    if( m_current_cursor + a_size_to_write - m_buffer > m_buffer_size )
    {
        LogUtilFatal() << "Has no more space to write";
    }
}

}

