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


#include "uuid.h"

#include <algorithm>

namespace bluetooth
{

static_assert( sizeof( uuid ) == 16, "uuid must be 16 bytes long!" );

using uuid_128bit = uuid::uuid_128bit;

const uuid uuid::s_empty_uuid = uuid::from_128bit_be( uuid_128bit{ {0x00} } );

namespace
{
constexpr uuid kBase = uuid::from_128bit_be(
    uuid_128bit{ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
                0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb} } );
}  // namespace

uuid::uuid()
{
    memset( uu.data(), 0x00, uu.size() );
}

size_t uuid::shortest_size() const
{
    if( memcmp( uu.data() + s_32bituuid_size, kBase.uu.data() + s_32bituuid_size,
        s_128bituuid_size - s_32bituuid_size ) != 0 )
    {
        return s_128bituuid_size;
    }

    if( uu[0] == 0 && uu[1] == 0 ) return s_16bituuid_size;

    return s_32bituuid_size;
}

bool uuid::is_16bit() const
{
    return shortest_size() == s_16bituuid_size;
}

uint16_t uuid::get_16bit() const { return ( ( ( uint16_t )uu[2] ) << 8 ) + uu[3]; }

uint32_t uuid::get_32bit() const
{
    return ( ( ( uint32_t )uu[0] ) << 24 ) + ( ( ( uint32_t )uu[1] ) << 16 ) +
        ( ( ( uint32_t )uu[2] ) << 8 ) + uu[3];
}

uuid uuid::from_16bit( uint16_t uuid16 )
{
    uuid u = kBase;

    u.uu[2] = ( uint8_t )( ( 0xFF00 & uuid16 ) >> 8 );
    u.uu[3] = ( uint8_t )( 0x00FF & uuid16 );
    return u;
}

uuid uuid::from_32bit( uint32_t uuid32 )
{
    uuid u = kBase;

    u.uu[0] = ( uint8_t )( ( 0xFF000000 & uuid32 ) >> 24 );
    u.uu[1] = ( uint8_t )( ( 0x00FF0000 & uuid32 ) >> 16 );
    u.uu[2] = ( uint8_t )( ( 0x0000FF00 & uuid32 ) >> 8 );
    u.uu[3] = ( uint8_t )( 0x000000FF & uuid32 );
    return u;
}

uuid uuid::from_128bit_be( const uint8_t* uuid )
{
    uuid_128bit tmp;
    memcpy( tmp.data(), uuid, s_128bituuid_size );
    return from_128bit_be( tmp );
}

uuid uuid::from_128bit_le( const uuid_128bit& a_uuid )
{
    uuid u;
    std::reverse_copy( a_uuid.data(), a_uuid.data() + s_128bituuid_size, u.uu.begin() );
    return u;
}

uuid uuid::from_128bit_le( const uint8_t* uuid )
{
    uuid_128bit tmp;
    memcpy( tmp.data(), uuid, s_128bituuid_size );
    return from_128bit_le( tmp );
}

const uuid_128bit uuid::to_128bit_le() const
{
    uuid_128bit le;
    std::reverse_copy( uu.data(), uu.data() + s_128bituuid_size, le.begin() );
    return le;
}

const uuid_128bit& uuid::to_128bit_be() const { return uu; }

bool uuid::empty() const { return *this == s_empty_uuid; }

void uuid::set( const uuid& uuid )
{
    uu = uuid.uu;
}

bool uuid::operator<( const uuid& rhs ) const
{
    return std::lexicographical_compare( uu.begin(), uu.end(), rhs.uu.begin(),
        rhs.uu.end() );
}

bool uuid::operator==( const uuid& rhs ) const { return uu == rhs.uu; }

bool uuid::operator!=( const uuid& rhs ) const { return uu != rhs.uu; }

std::string uuid::to_string() const
{
    char buffer[100] = { 0 };
    snprintf( buffer, sizeof(buffer),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7], uu[8], uu[9],
        uu[10], uu[11], uu[12], uu[13], uu[14], uu[15] );
    return buffer;
}
}  // namespace bluetooth