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

namespace bluetooth
{

/*---------------------------------------------------------------------------
 * le_to_host16()
 *
 *     Converts the two octets that are stored the buffer pointed to by
 *     le_value in little endian format to a 16-bit unsigned integer.
 *
 * Parameters:
 *     le_value - Pointer to two octets in little endian format
 *
 * Returns:
 *     uint16_t value
 */
uint16_t le_to_host16( const uint8_t* le_value );

/*---------------------------------------------------------------------------
 * le_to_host32()
 *
 *     Converts the four octets that are stored the buffer pointed to by
 *     le_value in little endian format to a 32-bit unsigned integer.
 *
 * Parameters:
 *     le_value - Pointer to four octets in little endian format
 *
 * Returns:
 *     uint32_t value
 */
uint32_t le_to_host32( const uint8_t* le_value );

/*---------------------------------------------------------------------------
 * be_to_host16()
 *
 *     Converts the two octets that are stored the buffer pointed to by
 *     be_ptr in big endian format to a 16-bit unsigned integer.
 *
 * Parameters:
 *     be_ptr - Pointer to two octets in big endian format
 *
 * Returns:
 *     uint16_t value
 */
uint16_t be_to_host16( const uint8_t* be_ptr );

/*---------------------------------------------------------------------------
 * be_to_host32()
 *
 *     Converts the four octets that are stored the buffer pointed to by
 *     be_ptr in big endian format to a 32-bit unsigned integer.
 *
 * Parameters:
 *     be_ptr - Pointer to two octets in big endian format
 *
 * Returns:
 *     uint32_t value
 */
uint32_t be_to_host32( const uint8_t* be_ptr );

/*---------------------------------------------------------------------------
 * write_le16()
 *
 *     Stores an unsigned 16-bit integer into a buffer in little endian
 *     format.
 *
 * Parameters:
 *     buff - pointer to buffer to receive the bytes
 *     le_value - unsigned 16-bit integer
 *
 * Returns:
 *     void
 */
void write_le16( uint8_t* buff, uint16_t le_value );

/*---------------------------------------------------------------------------
 * write_le32()
 *
 *     Stores an unsigned 32-bit integer into a buffer in little endian
 *     format.
 *
 * Parameters:
 *     buff - pointer to buffer to receive the bytes
 *     le_value - unsigned 32-bit integer
 *
 * Returns:
 *     void
 */
void write_le32( uint8_t* buff, uint32_t le_value );

/*---------------------------------------------------------------------------
 * write_be16()
 *
 *     Stores an unsigned 16-bit integer into a buffer in big endian format.
 *
 * Parameters:
 *     ptr - pointer to buffer to receive the bytes
 *     be_value - unsigned 16-bit integer
 *
 * Returns:
 *     void
 */
void write_be16( uint8_t* ptr, uint16_t be_value );

/*---------------------------------------------------------------------------
 * write_be32()
 *
 *     Stores an unsigned 32-bit integer into a buffer in big endian format.
 *
 * Parameters:
 *     ptr - pointer to buffer to receive the bytes
 *     be_value - unsigned 32-bit integer
 *
 * Returns:
 *     void
 */
void write_be32( uint8_t* ptr, uint32_t be_value );

/*---------------------------------------------------------------------------
 * write_be64()
 *
 *     Stores an unsigned 64-bit integer into a buffer in big endian format.
 *
 * Parameters:
 *     ptr - pointer to buffer to receive the bytes
 *     be_value - unsigned 64-bit integer
 *
 * Returns:
 *     void
 */
void write_be64( uint8_t* ptr, uint64_t be_value );
}

#ifdef USE_ENDIAN_MACROS

/* Little Endian to Host integer format conversion macros */
#define le_to_host16(ptr)  (uint16_t)( ((uint16_t) *((uint8_t*)(ptr)+1) << 8) | \
                                ((uint16_t) *((uint8_t*)(ptr))) )

#define le_to_host32(ptr)  (uint32_t)( ((uint32_t) *((uint8_t*)(ptr)+3) << 24) | \
                                ((uint32_t) *((uint8_t*)(ptr)+2) << 16) | \
                                ((uint32_t) *((uint8_t*)(ptr)+1) << 8)  | \
                                ((uint32_t) *((uint8_t*)(ptr))) )

/* Big Endian to Host integer format conversion macros */
#define be_to_host16(ptr)  (uint16_t)( ((uint16_t) *((uint8_t*)(ptr)) << 8) | \
                                ((uint16_t) *((uint8_t*)(ptr)+1)) )

#define be_to_host32(ptr)  (uint32_t)( ((uint32_t) *((uint8_t*)(ptr)) << 24)   | \
                                ((uint32_t) *((uint8_t*)(ptr)+1) << 16) | \
                                ((uint32_t) *((uint8_t*)(ptr)+2) << 8)  | \
                                ((uint32_t) *((uint8_t*)(ptr)+3)) )

/* Store value into a buffer in Little Endian format */
#define write_le16(buff,num) ( ((buff)[1] = (uint8_t) ((num)>>8)),    \
                              ((buff)[0] = (uint8_t) (num)) )

#define write_le32(buff,num) ( ((buff)[3] = (uint8_t) ((num)>>24)),  \
                              ((buff)[2] = (uint8_t) ((num)>>16)),  \
                              ((buff)[1] = (uint8_t) ((num)>>8)),   \
                              ((buff)[0] = (uint8_t) (num)) )

/* Store value into a buffer in Big Endian format */
#define write_be16(buff,num) ( ((buff)[0] = (uint8_t) ((num)>>8)),    \
                              ((buff)[1] = (uint8_t) (num)) )

#define write_be32(buff,num) ( ((buff)[0] = (uint8_t) ((num)>>24)),  \
                              ((buff)[1] = (uint8_t) ((num)>>16)),  \
                              ((buff)[2] = (uint8_t) ((num)>>8)),   \
                              ((buff)[3] = (uint8_t) (num)) )

#endif /* USE_ENDIAN_MACROS */

