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
#include "sdp_data_element.h"

#include <cstdint>

namespace bluetooth
{

class sdp_attribute
{

public:

    uint16_t get_attribute_id()const
    {
        return m_attribute_id;
    }

    void set_attribute_id( uint16_t a_id )
    {
        m_attribute_id = a_id;
    }

    sdp_data_element& get_value()
    {
        return m_value;
    }

    void clear()
    {
        m_value.clear();
    }

    sdp_attribute() = default;

    sdp_attribute( sdp_attribute&& a_right ) noexcept
    {
        stolen_from( std::move( a_right ) );
    }

    sdp_attribute( sdp_attribute const& a_right )
    {
        copy_from( a_right );
    }

    sdp_attribute& operator=( sdp_attribute&& a_right ) noexcept
    {
        stolen_from( std::move( a_right ) );
        return *this;
    }

    sdp_attribute& operator=( sdp_attribute const& a_right )
    {
        copy_from( a_right );
        return *this;
    }

private:

    void stolen_from( sdp_attribute&& a_right )noexcept
    {
        m_attribute_id = a_right.m_attribute_id;
        m_value = std::move( a_right.m_value );
    }

    void copy_from( sdp_attribute const& a_right )
    {
        m_attribute_id = a_right.m_attribute_id;
        m_value = a_right.m_value;
    }

    uint16_t m_attribute_id = 0x00;
    sdp_data_element m_value;
};

}

