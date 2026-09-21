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

#include <stdint.h>
#include <array>
#include <string>
#include <iostream>

namespace bluetooth
{

// This class is representing Bluetooth UUIDs across whole stack.
// Here are some general endianness rules:
// 1. UUID is internally kept as as Big Endian.
// 2. Bytes representing UUID coming from upper layers are Big
//    Endian.
// 3. Bytes representing UUID coming from lower layer, HCI packets, are Little
//    Endian.
// 4. UUID in storage is always string.
class uuid final
{
public:

    static constexpr size_t s_128bituuid_size = 16;
    static constexpr size_t s_32bituuid_size = 4;
    static constexpr size_t s_16bituuid_size = 2;

    static constexpr size_t s_128bit_string_size = 36;

    static const uuid s_empty_uuid;  // 00000000-0000-0000-0000-000000000000

    using uuid_128bit = std::array<uint8_t, s_128bituuid_size>;

    uuid();

    // Returns the shortest possible representation of this UUID in bytes. Either
    // s_16bituuid_size, s_32bituuid_size, or s_128bituuid_size
    size_t shortest_size() const;

    // Returns true if this UUID can be represented as 16 bit.
    bool is_16bit() const;

    // Returns 16 bit Little Endian representation of this UUID. Use
    // shortest_size() or is_16bit() before using this method.
    uint16_t get_16bit() const;

    // Returns 32 bit Little Endian representation of this UUID. Use
    // shortest_size() before using this method.
    uint32_t get_32bit() const;

    // Converts 16bit Little Endian representation of UUID to UUID
    static uuid from_16bit( uint16_t uuid16bit );

    // Converts 32bit Little Endian representation of UUID to UUID
    static uuid from_32bit( uint32_t uuid32bit );

    // Converts 128 bit Big Endian array representing UUID to UUID.
    static constexpr uuid from_128bit_be( const uuid_128bit& a_uuid )
    {
        uuid u( a_uuid );
        return u;
    }

    // Converts 128 bit Big Endian array representing UUID to UUID. |uuid| points
    // to beginning of array.
    static uuid from_128bit_be( const uint8_t* uuid );

    // Converts 128 bit Little Endian array representing UUID to UUID.
    static uuid from_128bit_le( const uuid_128bit& uuid );

    // Converts 128 bit Little Endian array representing UUID to UUID. |uuid|
    // points to beginning of array.
    static uuid from_128bit_le( const uint8_t* uuid );

    // Returns 128 bit Little Endian representation of this UUID
    const uuid_128bit to_128bit_le() const;

    // Returns 128 bit Big Endian representation of this UUID
    const uuid_128bit& to_128bit_be() const;

    // Returns string representing this UUID in
    // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx format, lowercase.
    std::string to_string() const;

    // Returns true if this UUID is equal to kEmpty
    bool empty() const;

    // Update UUID with new value
    void set( const uuid& uuid );

    bool operator<( const uuid& rhs ) const;
    bool operator==( const uuid& rhs ) const;
    bool operator!=( const uuid& rhs ) const;

    constexpr uuid( const uuid_128bit& val ) : uu{ val } {};

public:
    // Network-byte-ordered ID (Big Endian).
    uuid_128bit uu;
};
}  // namespace bluetooth

inline std::ostream& operator<<( std::ostream& os, const bluetooth::uuid& a )
{
    os << a.to_string();
    return os;
}

// Custom std::hash specialization so that bluetooth::UUID can be used as a key
// in std::unordered_map.
namespace std
{

template <>
struct hash<bluetooth::uuid>
{
    std::size_t operator()( const bluetooth::uuid& key ) const
    {
        const auto& uuid_bytes = key.to_128bit_be();
        std::hash<std::string> hash_fn;
        return hash_fn( std::string( reinterpret_cast< const char* >( uuid_bytes.data() ),
            uuid_bytes.size() ) );
    }
};

}  // namespace std
