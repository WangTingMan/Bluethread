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

#include "l2cap_signaling_header.h"
#include "../../common/stream_writer.h"

namespace bluetooth
{

void signaling_header::set_sdu_length( uint16_t a_length )
{
    l2cap_header::set_sdu_length( a_length + 4 );
    m_length_in_signaling = a_length;
}

void signaling_header::to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )
{
    uint8_t* p_buffer = a_buffer + l2cap_header::header_size();
    l2cap_header::to_raw_buffer( a_buffer, a_size );

    stream_writer writer( p_buffer, a_size - l2cap_header::header_size() );
    writer << m_signaling_code << m_identifer << m_length_in_signaling;
}

uint16_t signaling_header::header_size()const
{
    return 4 + l2cap_header::header_size();
}

}

