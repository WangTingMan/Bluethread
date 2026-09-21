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

#pragma once
#include <cstdint>
#include <vector>

#include "bluetooth_address.h"

namespace bluetooth
{

class stream_writer
{

public:

    stream_writer( uint8_t* a_buffer, uint32_t a_size )
    {
        set_buffer( a_buffer, a_size );
    }

    stream_writer(){}

    void set_buffer( uint8_t* a_buffer, uint32_t a_size )
    {
        m_buffer = a_buffer;
        m_buffer_size = a_size;
        m_current_cursor = m_buffer;
    }

    stream_writer& operator<<( uint32_t a_value );

    stream_writer& operator<<( uint16_t a_value );

    stream_writer& operator<<( uint8_t a_value );

    stream_writer& operator<<( bluetooth_address const& a_value );

    template<typename T>
    stream_writer& operator<<( T const& a_value );

    stream_writer& write_buffer( uint8_t const* a_buffer, uint32_t a_size );

    stream_writer& operator<<( std::vector<uint8_t> a_buffer );

    uint16_t wrote_size()const
    {
        return static_cast<uint16_t>( m_current_cursor - m_buffer );
    }

    void check_size( uint32_t a_size_to_write )const;

private:

    uint8_t* m_buffer = nullptr;
    uint32_t m_buffer_size = 0;

    uint8_t* m_current_cursor = nullptr;
};

}

