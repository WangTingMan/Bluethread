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

#include "bluetooth_address.h"
#include "class_of_device.h"
#include "uuid.h"

#include <string>
#include <bitset>
#include <cstdint>
#include <tuple>
#include <vector>

namespace bluetooth
{

class remote_device
{

public:

    static constexpr uint8_t s_link_key_size = 16;

    static constexpr uint8_t s_address_flag = 0;
    static constexpr uint8_t s_page_scan_repetition_mode_flag = 1;
    static constexpr uint8_t s_cod_flag = 2;
    static constexpr uint8_t s_clock_offset = 3;
    static constexpr uint8_t s_rssi_flag = 4;
    static constexpr uint8_t s_completed_name_flag = 5;
    static constexpr uint8_t s_link_key_flag = 6;

    /**
     * Check if has the specified field
     */
    bool has_field( uint8_t a_flag )const
    {
        return m_valid_flags.test( a_flag );
    }

    void set_address( bluetooth_address a_address )
    {
        m_address = a_address;
        m_valid_flags.set( s_address_flag );
    }

    std::tuple<bool, bluetooth_address> get_address()const
    {
        return { m_valid_flags.test( s_address_flag ), m_address };
    }

    void set_page_scan_repetition_mode( uint8_t a_page_scan_repetition_mode )
    {
        m_page_scan_repetition_mode = a_page_scan_repetition_mode;
        m_valid_flags.set( s_page_scan_repetition_mode_flag );
    }

    std::tuple<bool, uint8_t> get_page_scan_rp_mode()const
    {
        return { m_valid_flags.test( s_page_scan_repetition_mode_flag ), m_page_scan_repetition_mode };
    }

    void set_clock_offset( uint16_t a_offset )
    {
        m_clock_offset = a_offset;
        m_valid_flags.set( s_clock_offset );
    }

    std::tuple<bool, uint16_t> get_clock_offset()const
    {
        return { m_valid_flags.test( s_clock_offset ), m_clock_offset };
    }

    void set_rssi( int8_t a_rssi )
    {
        m_rssi = a_rssi;
        m_valid_flags.set( s_rssi_flag );
    }

    std::tuple<bool, int8_t> get_rssi()const
    {
        return { m_valid_flags.test( s_rssi_flag ), m_rssi };
    }

    void set_cod( class_of_device a_cod )
    {
        m_cod = a_cod;
        m_valid_flags.set( s_cod_flag );
    }

    std::tuple<bool, class_of_device> get_cod()const
    {
        return { m_valid_flags.test( s_cod_flag ),m_cod };
    }

    void set_uuids( std::vector<uuid> a_uuid )
    {
        m_supported_uuids = a_uuid;
    }

    std::vector<uuid> const& get_uuids()const
    {
        return m_supported_uuids;
    }

    void set_name( std::u8string a_name )
    {
        m_name = std::move( a_name );
        m_valid_flags.set( s_completed_name_flag );
    }

    std::u8string const& get_name()const
    {
        return m_name;
    }

    void set_link_key( uint8_t* a_link_key, uint8_t a_key_type )
    {
        m_link_key.assign( a_link_key, a_link_key + s_link_key_size );
        m_valid_flags.set( s_link_key_flag );
        m_key_type = a_key_type;
    }

    std::tuple<bool, std::vector<uint8_t>> get_link_key()const
    {
        return { m_valid_flags.test( s_link_key_flag ), m_link_key };
    }

private:

    bluetooth_address m_address;
    uint8_t m_page_scan_repetition_mode = 0;
    class_of_device m_cod;
    uint16_t m_clock_offset = 0;
    int8_t m_rssi = 0;
    std::u8string m_name;
    std::vector<uint8_t> m_link_key;
    uint8_t m_key_type;
    std::vector<uuid> m_supported_uuids;

    std::bitset<sizeof( uint64_t ) * 8> m_valid_flags;
};

}

