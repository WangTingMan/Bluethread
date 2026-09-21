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

#include "data_type_parser.h"

namespace bluetooth
{

std::vector<data_element_parsed> parse_eir( uint8_t* a_buffer, uint16_t a_size )
{
    uint16_t position = 0;
    std::vector<data_element_parsed> ret;
    while( position != a_size )
    {
        uint8_t len = a_buffer[position];

        if( len == 0 )
        {
            break;
        }

        if( position + len >= a_size )
        {
            break;
        }

        data_type type = static_cast< data_type >( a_buffer[position + 1] );

        data_element_parsed element;
        element.type = type;
        element.size = len - 1;
        element.buffer = a_buffer + position + 2;
        ret.push_back( element );

        position += len + 1; /* skip the length of data */
    }

    return ret;
}

}

