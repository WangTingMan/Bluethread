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
#include "global_config.h"

#include <algorithm>
#include <string>
#include <cstdint>
#include <ostream>

 /** Bluetooth Address */
class BLUETOOTH_EXPORT bluetooth_address final
{
public:

    static constexpr uint16_t s_bluetooth_address_size = 6;

    uint8_t address[s_bluetooth_address_size] = { 0 };

    bluetooth_address() = default;

    bluetooth_address( const uint8_t( &addr )[s_bluetooth_address_size] )
    {
        std::copy( addr, addr + s_bluetooth_address_size, address );
    }

    bool operator<( const bluetooth_address& rhs ) const
    {
        return ( std::memcmp( address, rhs.address, sizeof( address ) ) < 0 );
    }

    bool operator==( const bluetooth_address& rhs ) const
    {
        return ( std::memcmp( address, rhs.address, sizeof( address ) ) == 0 );
    }

    bool operator>( const bluetooth_address& rhs ) const { return ( rhs < *this ); }
    bool operator<=( const bluetooth_address& rhs ) const { return !( *this > rhs ); }
    bool operator>=( const bluetooth_address& rhs ) const { return !( *this < rhs ); }
    bool operator!=( const bluetooth_address& rhs ) const { return !( *this == rhs ); }

    bool empty() const { return *this == s_empty_address; }

    std::string to_string() const;

    // Copies |from| raw Bluetooth address octets to the local object.
    // Returns the number of copied octets - should be always RawAddress::kLength
    void from_bytes( const uint8_t* from )
    {
        std::copy( from, from + s_bluetooth_address_size, address );
    }

    static const bluetooth_address s_empty_address;  // 00:00:00:00:00:00

    static const bluetooth_address s_any_address;    // FF:FF:FF:FF:FF:FF
};

inline std::ostream& operator<<( std::ostream& os, const bluetooth_address& a )
{
    os << a.to_string();
    return os;
}
