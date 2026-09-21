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

#include "endian_convert.h"

namespace bluetooth
{
#ifndef USE_ENDIAN_MACROS
/*---------------------------------------------------------------------------
 *            be_to_host16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 16-bit number from the given buffer. The number
 *            is in Big-Endian format.
 *
 * Return:    16-bit number.
 */
uint16_t be_to_host16( const uint8_t* ptr )
{
    return ( uint16_t )( ( ( uint16_t )*ptr << 8 ) | ( ( uint16_t ) * ( ptr + 1 ) ) );
}

/*---------------------------------------------------------------------------
 *            be_to_host32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 32-bit number from the given buffer. The number
 *            is in Big-Endian format.
 *
 * Return:    32-bit number.
 */
uint32_t be_to_host32( const uint8_t* ptr )
{
    return ( uint32_t )( ( ( uint32_t )*ptr << 24 ) | \
        ( ( uint32_t ) * ( ptr + 1 ) << 16 ) | \
        ( ( uint32_t ) * ( ptr + 2 ) << 8 ) | \
        ( ( uint32_t ) * ( ptr + 3 ) ) );
}

/*---------------------------------------------------------------------------
 *            le_to_host16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 16-bit number from the given buffer. The number
 *            is in Little-Endian format.
 *
 * Return:    16-bit number.
 */
uint16_t le_to_host16( const uint8_t* ptr )
{
    return ( uint16_t )( ( ( uint16_t ) * ( ptr + 1 ) << 8 ) | ( ( uint16_t )*ptr ) );
}

/*---------------------------------------------------------------------------
 *            le_to_host32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve a 32-bit number from the given buffer. The number
 *            is in Little-Endian format.
 *
 * Return:    32-bit number.
 */
uint32_t le_to_host32( const uint8_t* ptr )
{
    return ( uint32_t )( ( ( uint32_t ) * ( ptr + 3 ) << 24 ) | \
        ( ( uint32_t ) * ( ptr + 2 ) << 16 ) | \
        ( ( uint32_t ) * ( ptr + 1 ) << 8 ) | \
        ( ( uint32_t ) * ( ptr ) ) );
}

/*---------------------------------------------------------------------------
 *            write_le16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 16 bit value into a buffer in Little Endian format.
 *
 * Return:    void
 */
void write_le16( uint8_t* buff, uint16_t le_value )
{
    buff[1] = ( uint8_t )( le_value >> 8 );
    buff[0] = ( uint8_t )le_value;
}

/*---------------------------------------------------------------------------
 *            write_le32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 32 bit value into a buffer in Little Endian format.
 *
 * Return:    void
 */
void write_le32( uint8_t* buff, uint32_t le_value )
{
    buff[3] = ( uint8_t )( le_value >> 24 );
    buff[2] = ( uint8_t )( le_value >> 16 );
    buff[1] = ( uint8_t )( le_value >> 8 );
    buff[0] = ( uint8_t )le_value;
}

/*---------------------------------------------------------------------------
 *            write_be16()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 16 bit value into a buffer in Big Endian format.
 *
 * Return:    void
 */
void write_be16( uint8_t* buff, uint16_t be_value )
{
    buff[0] = ( uint8_t )( be_value >> 8 );
    buff[1] = ( uint8_t )be_value;
}

/*---------------------------------------------------------------------------
 *            write_be32()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Store 32 bit value into a buffer in Big Endian format.
 *
 * Return:    void
 */
void write_be32( uint8_t* buff, uint32_t be_value )
{
    buff[0] = ( uint8_t )( be_value >> 24 );
    buff[1] = ( uint8_t )( be_value >> 16 );
    buff[2] = ( uint8_t )( be_value >> 8 );
    buff[3] = ( uint8_t )be_value;
}

void write_be64( uint8_t* buff, uint64_t be_value )
{
    buff[0] = ( uint8_t )( be_value >> 56 );
    buff[1] = ( uint8_t )( be_value >> 48 );
    buff[2] = ( uint8_t )( be_value >> 40 );
    buff[3] = ( uint8_t )( be_value >> 32 );
    buff[4] = ( uint8_t )( be_value >> 24 );
    buff[5] = ( uint8_t )( be_value >> 16 );
    buff[6] = ( uint8_t )( be_value >> 8 );
    buff[7] = ( uint8_t )be_value;
}

#endif /* XA_USE_ENDIAN_MACROS == XA_DISABLED */

}
