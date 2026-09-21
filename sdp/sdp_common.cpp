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

#include "sdp_common.h"
#include "endian_convert.h"

#include "framework\log_util.h"

namespace bluetooth
{

bool parse_attribute_value_header
    (
    uint8_t* a_buffer,
    uint32_t a_size,
    sdp_attribute_value_type& a_type,
    uint32_t& a_data_size,
    uint8_t*& a_data
    )
{
    if( a_size < 1 )
    {
        return false;
    }

    a_type = static_cast< sdp_attribute_value_type >( a_buffer[0] >> 3 );
    uint8_t size_index = a_buffer[0] & 0x07;
    a_data_size = 0;
    a_data = nullptr;

    switch( size_index )
    {
    case 0:
        if( a_type == sdp_attribute_value_type::null )
        {
            a_data_size = 0;
        }
        else
        {
            a_data_size = 1;
            a_data = a_buffer + 1;
        }
        break;
    case 1:
        a_data_size = 2;
        a_data = a_buffer + 1;
        break;
    case 2:
        a_data_size = 4;
        a_data = a_buffer + 1;
        break;
    case 3:
        a_data_size = 8;
        a_data = a_buffer + 1;
        break;
    case 4:
        a_data_size = 16;
        a_data = a_buffer + 1;
        break;
    case 5:
        if( a_size < 2 )
        {
            return false;
        }
        else
        {
            a_data_size = a_buffer[1];
            a_data = a_buffer + 2;
        }
        break;
    case 6:
        if( a_size < 3 )
        {
            return false;
        }
        else
        {
            a_data_size = be_to_host16( a_buffer + 1 );
            a_data = a_buffer + 3;
        }
        break;
    case 7:
        if( a_size < 5 )
        {
            return false;
        }
        else
        {
            a_data_size = be_to_host32( a_buffer + 1 );
            a_data = a_buffer + 6;
        }
        break;
    default:
        return false;
    }
    return true;
}

bool parse_elements_from( uint8_t* a_buffer, uint32_t a_size, std::vector<sdp_data_element>& a_elements )
{
    bool ret = false;
    uint8_t* cur = a_buffer;
    uint32_t size_left = a_size;
    while( cur < a_buffer + a_size )
    {
        sdp_attribute_value_type type;
        uint32_t data_size;
        uint8_t* data = nullptr;
        ret = parse_attribute_value_header( cur, size_left, type, data_size, data );
        if( !ret )
        {
            return ret;
        }

        auto [attribute, parse_ret] = sdp_data_element::parse_from
            ( cur, data_size + static_cast<uint32_t>( data - cur ) );
        if( !parse_ret )
        {
            ret = parse_ret;
            break;
        }
        else
        {
            a_elements.push_back( attribute );
        }

        cur += data_size + ( data - cur );
    }

    ret = true;
    return ret;
}

bool sdp_data_element::recognite_data_element( uint8_t* a_buffer, uint32_t a_size, uint32_t& a_invalid_size )
{
    bool ret = false;
    sdp_attribute_value_type type = sdp_attribute_value_type::null;
    uint32_t data_size = 0;
    uint8_t* data = nullptr;
    a_invalid_size = 0;

    ret = parse_attribute_value_header( a_buffer, a_size, type, data_size, data );

    if( ret )
    {
        a_invalid_size = ( data - a_buffer ) + data_size;
        if( a_invalid_size > a_size )
        {
            ret = false;
            a_invalid_size = 0;
        }
    }
    return ret;
}

std::tuple<sdp_data_element, bool> sdp_data_element::parse_from( uint8_t* a_buffer, uint32_t a_size )
{
    sdp_data_element value;
    bool ret = false;
    if( 0 == a_size )
    {
        return { std::move( value ),ret };
    }

    sdp_attribute_value_type type = static_cast< sdp_attribute_value_type >( a_buffer[0] >> 3 );
    uint8_t size_index = a_buffer[0] & 0x07;

    uint64_t data_size = 0;
    std::u8string temp_string;
    char8_t const* p_string_buffer = nullptr;
    uint8_t* p_parsed_buffer = nullptr;

    switch( type )
    {
    case bluetooth::sdp_attribute_value_type::null:
        value.set_null_value();
        ret = ( 0 == size_index );
        break;
    case bluetooth::sdp_attribute_value_type::unsigned_integer:
        switch( size_index )
        {
        case 0:
            if( a_size != 2 )
            {
                return { std::move( value ), false };
            }
            value.set_uint8_value( a_buffer[1] );
            break;
        case 1:
            if( a_size != 3 )
            {
                return { std::move( value ), false };
            }
            value.set_uint16_value( be_to_host16( a_buffer + 1 ) );
            break;
        case 2:
            if( a_size != 5 )
            {
                return { std::move( value ), false };
            }
            value.set_uint32_value( be_to_host32( a_buffer + 1 ) );
            break;
        case 3:
            if( a_size != 9 )
            {
                return { std::move( value ), false };
            }
            LogUtilError() << "No implementation for 64 bit unsigned integer";
        case 4:
            if( a_size != 17 )
            {
                return { std::move( value ), false };
            }
            LogUtilError() << "No implementation for 128 bit unsigned integer";
        default:
            return { std::move( value ), false };
        }
        break;
    case bluetooth::sdp_attribute_value_type::signed_integer:
        switch( size_index )
        {
        case 0:
            if( a_size != 2 )
            {
                return { std::move( value ), false };
            }
            // TODO
            LogUtilError() << "No implementation for signed integer";
            break;
        case 1:
            if( a_size != 3 )
            {
                return { std::move( value ), false };
            }
            LogUtilError() << "No implementation for signed integer";
            break;
        case 2:
            if( a_size != 5 )
            {
                return { std::move( value ), false };
            }
            LogUtilError() << "No implementation for signed integer";
            break;
        case 3:
            if( a_size != 9 )
            {
                return { std::move( value ), false };
            }
            LogUtilError() << "No implementation for signed integer";
        case 4:
            if( a_size != 17 )
            {
                return { std::move( value ), false };
            }
            LogUtilError() << "No implementation for signed integer";
        default:
            return { std::move( value ), false };
        }
        break;
    case bluetooth::sdp_attribute_value_type::uuid:
        switch( size_index )
        {
        case 1:
            if( a_size != 3 )
            {
                return { std::move( value ), false };
            }
            value.set_uuid( uuid::from_16bit( be_to_host16( a_buffer + 1 ) ) );
            break;
        case 2:
            if( a_size != 5 )
            {
                return { std::move( value ), false };
            }
            value.set_uuid( uuid::from_32bit( be_to_host32( a_buffer + 1 ) ) );
            break;
        case 4:
            if( a_size != 17 )
            {
                return { std::move( value ), false };
            }
            value.set_uuid( uuid::from_128bit_be( a_buffer + 1 ) );
            break;
        default:
            return { std::move( value ), false };
        }
        break;
    case bluetooth::sdp_attribute_value_type::string:
        switch( size_index )
        {
        case 5:
            if( a_size < 2 )
            {
                return { std::move( value ), false };
            }
            p_string_buffer = reinterpret_cast< const char8_t* >( a_buffer ) + 2;
            data_size = a_buffer[1];
            if( a_size < 2 + data_size )
            {
                return { std::move( value ), false };
            }
            temp_string.assign( p_string_buffer, p_string_buffer + data_size );
            if( 0x00 != temp_string.back() )
            {
                temp_string.push_back( 0x00 );
            }
            value.set_string_value( temp_string );
            break;
        case 6:
            if( a_size < 3 )
            {
                return { std::move( value ), false };
            }
            p_string_buffer = reinterpret_cast< const char8_t* >( a_buffer ) + 3;
            data_size = be_to_host16( a_buffer + 1 );
            if( a_size < 3 + data_size )
            {
                return { std::move( value ), false };
            }
            temp_string.assign( p_string_buffer, p_string_buffer + data_size );
            if( 0x00 != temp_string.back() )
            {
                temp_string.push_back( 0x00 );
            }
            value.set_string_value( temp_string );
            break;
        case 7:
            if( a_size < 5 )
            {
                return { std::move( value ), false };
            }
            p_string_buffer = reinterpret_cast< const char8_t* >( a_buffer ) + 5;
            data_size = be_to_host32( a_buffer + 1 );
            if( a_size < 5 + data_size )
            {
                return { std::move( value ), false };
            }
            temp_string.assign( p_string_buffer, p_string_buffer + data_size );
            if( 0x00 != temp_string.back() )
            {
                temp_string.push_back( 0x00 );
            }
            value.set_string_value( temp_string );
            break;
        default:
            return { std::move( value ), false };
        }
        break;
    case bluetooth::sdp_attribute_value_type::boolean_type:
        switch( size_index )
        {
        case 0:
            if( a_size != 2 )
            {
                return { std::move( value ), false };
            }
            value.set_boolean( a_buffer[1] != 0x00 );
            break;
        default:
            return { std::move( value ), false };
        }
        break;
    case bluetooth::sdp_attribute_value_type::data_elements:
        switch( size_index )
        {
        case 5:
            if( a_size < 2 )
            {
                return { std::move( value ), false };
            }
            data_size = a_buffer[1];
            if( a_size < 2 + data_size )
            {
                return { std::move( value ), false };
            }
            else
            {
                std::vector<sdp_data_element> elements;
                ret = parse_elements_from( a_buffer + 2, static_cast<uint32_t>( data_size - 2 ), elements );
                value.set_elements( elements );
            }
            break;
        case 6:
            if( a_size < 3 )
            {
                return { std::move( value ), false };
            }
            data_size = be_to_host16( a_buffer + 1 );
            if( a_size < 3 + data_size )
            {
                return { std::move( value ), false };
            }
            else
            {
                std::vector<sdp_data_element> elements;
                ret = parse_elements_from( a_buffer + 3, static_cast<uint32_t>( data_size - 3 ), elements );
                value.set_elements( elements );
            }
            break;
        case 7:
            LogUtilError() << "Not support such more data";
            break;
        default:
            return { std::move( value ), false };
        }
        break;
    case bluetooth::sdp_attribute_value_type::alternative_data_element:
        LogUtilError() << "how to parse this type?";
        return { std::move( value ), false };
    case bluetooth::sdp_attribute_value_type::url:
        LogUtilError() << "how to parse this type?";
        return { std::move( value ), false };
    default:
        break;
    }

    ret = true;
    return { std::move( value ),ret };
}

void sdp_data_element::set_null_value()
{
    uint8_t buffer[10];
    m_value_type = sdp_attribute_value_type::null;
    m_size_index = 0;
    buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
    m_buffer.assign( buffer, buffer + 1 );
}

void sdp_data_element::set_uint32_value( uint32_t a_value )
{
    m_value_type = sdp_attribute_value_type::unsigned_integer;
    m_size_index = 2;
    uint8_t buffer[10];

    buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
    write_be32( buffer + 1, a_value );
    m_buffer.assign( buffer, buffer + 5 );
}

uint32_t sdp_data_element::get_uint32_value()const
{
    if( can_as_uint32() )
    {
        return be_to_host32( m_buffer.data() + 1 );
    }

    LogUtilError() << "cannot convert to uint32 value!";
    return 0;
}

bool sdp_data_element::can_as_uint32()const
{
    if( m_value_type == sdp_attribute_value_type::unsigned_integer &&
        m_size_index == 2 )
    {
        return true;
    }
    return false;
}

void sdp_data_element::set_uint8_value( uint8_t a_value )
{
    m_value_type = sdp_attribute_value_type::unsigned_integer;
    m_size_index = 0;
    uint8_t buffer[10];

    buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
    buffer[1] = a_value;
    m_buffer.assign( buffer, buffer + 2 );
}

uint8_t sdp_data_element::get_uint8_value()const
{
    if( can_as_uint8() )
    {
        return m_buffer[1];
    }
    return 0;
}

bool sdp_data_element::can_as_uint8()const
{
    if( m_value_type == sdp_attribute_value_type::unsigned_integer &&
        m_size_index == 0 )
    {
        return true;
    }

    return false;
}

void sdp_data_element::set_string_value( std::u8string const& a_value )
{
    m_value_type = sdp_attribute_value_type::string;
    size_t data_size = a_value.size();
    uint8_t buffer[10] = { 0 };
    if( 0x00 != a_value.back() )
    {
        data_size += 1;
    }

    m_buffer.clear();
    if( data_size < 0xFF )
    {
        m_size_index = 5;
        buffer[0] = ( static_cast<uint8_t>( m_value_type ) << 3 ) + m_size_index;
        buffer[1] = static_cast<uint8_t>( data_size );
        m_buffer.push_back( buffer[0] );
        m_buffer.push_back( buffer[1] );
        m_buffer.insert( m_buffer.end(), a_value.begin(), a_value.end() );
        if( 0x00 != m_buffer.back() )
        {
            m_buffer.push_back( 0x00 );
        }
    }
    else if( data_size >= 0xFF && data_size < 0xFFFF )
    {
        m_size_index = 6;
        buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
        write_be16( buffer + 1, static_cast< uint16_t>( data_size ) );
        m_buffer.push_back( buffer[0] );
        m_buffer.push_back( buffer[1] );
        m_buffer.push_back( buffer[2] );
        m_buffer.insert( m_buffer.end(), a_value.begin(), a_value.end() );
        if( 0x00 != m_buffer.back() )
        {
            m_buffer.push_back( 0x00 );
        }
    }
    else if( data_size >= 0xFFFF && data_size < 0xFFFFFFFF )
    {
        m_size_index = 7;
        buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
        write_be32( buffer + 1, static_cast< uint32_t >( data_size ) );
        m_buffer.insert( m_buffer.end(), buffer, buffer + 5 );
        m_buffer.insert( m_buffer.end(), a_value.begin(), a_value.end() );
        if( 0x00 != m_buffer.back() )
        {
            m_buffer.push_back( 0x00 );
        }
    }
    else
    {
        LogUtilError() << "to long string";
    }
}

std::u8string sdp_data_element::get_string_value()const
{
    std::u8string str;
    if( !can_as_string() )
    {
        return str;
    }

    switch( m_size_index )
    {
    case 5:
        str.assign( reinterpret_cast<const char8_t*>( m_buffer.data() ) + 1 );
        break;
    case 6:
        str.assign( reinterpret_cast< const char8_t* >( m_buffer.data() ) + 3 );
        break;
    case 7:
        str.assign( reinterpret_cast< const char8_t* >( m_buffer.data() ) + 9 );
        break;
    default:
        break;
    }
    return str;
}

bool sdp_data_element::can_as_string()const
{
    bool size_ok = false;
    size_ok = ( ( m_size_index == 5 ) || ( m_size_index == 6 ) || ( m_size_index == 7 ) );
    if( ( m_value_type == sdp_attribute_value_type::boolean_type ) && size_ok )
    {
        return true;
    }
    return false;
}

void sdp_data_element::set_uint16_value( uint16_t a_value )
{
    m_value_type = sdp_attribute_value_type::unsigned_integer;
    m_size_index = 1;
    uint8_t buffer[10];

    buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
    write_be16( buffer + 1, a_value );
    m_buffer.assign( buffer, buffer + 3 );
}

uint16_t sdp_data_element::get_uint16_value()const
{
    if( can_as_uint32() )
    {
        return be_to_host16( m_buffer.data() + 1 );
    }

    LogUtilError() << "cannot convert to uint16 value!";
    return 0;
}

bool sdp_data_element::can_as_uint16()const
{
    if( m_value_type == sdp_attribute_value_type::unsigned_integer &&
        m_size_index == 1 )
    {
        return true;
    }

    return false;
}

void sdp_data_element::set_uuid( uuid const& a_uuid )
{
    uint8_t buffer[20] = { 0 };
    size_t size = a_uuid.shortest_size();
    m_value_type = sdp_attribute_value_type::uuid;
    switch (size)
    {
    case 2:
        write_be16( buffer + 1, a_uuid.get_16bit() );
        m_size_index = 1;
        break;
    case 4:
        write_be32( buffer + 1, a_uuid.get_32bit() );
        m_size_index = 2;
        break;
    case 16:
        memcpy( buffer + 1, a_uuid.uu.data(), uuid::s_128bituuid_size );
        m_size_index = 4;
        break;
    default:
        break;
    }

    buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
    m_buffer.assign( buffer, buffer + size + 1 );
}

bool sdp_data_element::can_as_uuid()const
{
    bool size_ok = false;
    size_ok = ( ( m_size_index == 4 ) || ( m_size_index == 2 ) || ( m_size_index == 1 ) );
    if( ( m_value_type == sdp_attribute_value_type::uuid ) && size_ok )
    {
        return true;
    }
    return false;
}

void sdp_data_element::set_elements( std::vector<sdp_data_element> a_elements )
{
    m_value_type = sdp_attribute_value_type::data_elements;
    m_elemets = a_elements;
    prepare_raw_for_elements();
}

void sdp_data_element::add_element( sdp_data_element a_element )
{
    m_value_type = sdp_attribute_value_type::data_elements;
    m_elemets.push_back( a_element );
    prepare_raw_for_elements();
}

std::vector<sdp_data_element>const& sdp_data_element::get_elements()const
{
    return m_elemets;
}

bool sdp_data_element::can_as_elements()const
{
    return sdp_attribute_value_type::data_elements == m_value_type;
}

std::vector<uuid> sdp_data_element::get_uuid_from_elements()
{
    std::vector<uuid> uuids;
    if( can_as_uuid() )
    {
        uuids.push_back( get_uuid() );
        return uuids;
    }

    if( can_as_elements() )
    {
        for( auto& ele : m_elemets )
        {
            if( ele.can_as_uuid() )
            {
                uuids.push_back( ele.get_uuid() );
                continue;
            }

            if( ele.can_as_elements() )
            {
                std::vector<uuid> uuids_temp;
                uuids_temp = ele.get_uuid_from_elements();
                uuids.insert( uuids.end(), uuids_temp.begin(), uuids_temp.end() );
            }
        }
        return uuids;
    }
    return uuids;
}

void sdp_data_element::set_boolean( bool a_value )
{
    m_value_type = sdp_attribute_value_type::boolean_type;
    m_size_index = 0;
    uint8_t buffer[20] = { 0 };
    buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
    buffer[1] = ( a_value ? 0x01 : 0x00 );
    m_buffer.assign( buffer, buffer + 2 );
}

bool sdp_data_element::get_boolean()const
{
    if( can_as_boolean() )
    {
        return 0x00 != m_buffer[1];
    }

    LogUtilError() << "Cannot convert to boolean value.";
    return false;
}

bool sdp_data_element::can_as_boolean()const
{
    bool size_ok = false;
    size_ok = ( m_size_index == 0 );
    if( ( m_value_type == sdp_attribute_value_type::boolean_type ) && size_ok )
    {
        return true;
    }
    return false;
}

std::vector<uint8_t> const& sdp_data_element::get_raw_buffer()const
{
    return m_buffer;
}

void sdp_data_element::prepare_raw_for_elements()
{
    if( m_value_type != sdp_attribute_value_type::data_elements )
    {
        LogUtilError() << "Cannot prepare raw data for not data elements.";
        return;
    }

    m_buffer.clear();

    std::vector<uint8_t> elements_buffer;
    for( auto& ele : m_elemets )
    {
        auto& ele_raw_buffer = ele.get_raw_buffer();
        elements_buffer.insert( elements_buffer.end(), ele_raw_buffer.begin(), ele_raw_buffer.end() );
    }

    uint8_t buffer[5] = { 0 };
    uint8_t buffer_used = 0;
    size_t ele_size = elements_buffer.size();
    if( ele_size <= 0xFF )
    {
        m_size_index = 5;
        buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
        buffer[1] = static_cast< uint8_t >( ele_size );
        buffer_used = 2;
    }
    else if( ele_size <= 0xFFFF && ele_size > 0xFF )
    {
        m_size_index = 6;
        buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
        write_be16( buffer + 1, static_cast< uint16_t >( ele_size ) );
        buffer_used = 3;
    }
    else if( ele_size <= 0xFFFFFFFF && ele_size > 0xFFFF )
    {
        m_size_index = 6;
        buffer[0] = ( static_cast< uint8_t >( m_value_type ) << 3 ) + m_size_index;
        write_be32( buffer + 1, static_cast< uint32_t >( ele_size ) );
        buffer_used = 5;
    }
    else
    {
        LogUtilError() << "Too many bytes to write in elements' raw buffer";
        return;
    }

    m_buffer.insert( m_buffer.end(), buffer, buffer + buffer_used );
    m_buffer.insert( m_buffer.end(), elements_buffer.begin(), elements_buffer.end() );
}

uuid sdp_data_element::get_uuid()const
{
    uuid uuid_;
    if( !can_as_uuid() )
    {
        return uuid_;
    }

    switch( m_size_index )
    {
    case 1:
        uuid_ = uuid::from_16bit( be_to_host16( m_buffer.data() + 1 ) );
        break;
    case 2:
        uuid_ = uuid::from_16bit( be_to_host32( m_buffer.data() + 1 ) );
        break;
    case 4:
        memcpy( uuid_.uu.data(), m_buffer.data() + 1, uuid::s_128bituuid_size );
        break;
    default:
        break;
    }
    return uuid_;
}

uint32_t sdp_service_record::get_service_handle()
{
    uint32_t handle = 0x00;
    for( auto& ele : m_attributes )
    {
        if( ele.get_attribute_id() == sdp_universal_attribute_id::service_record_handle )
        {
            handle = ele.get_value().get_uint32_value();
            break;
        }
    }

    return handle;
}

void sdp_service_record::set_service_handle( uint32_t a_handle )
{
    auto attribute = find_attribute( sdp_universal_attribute_id::service_record_handle );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::service_record_handle );
    }

    attribute->get_value().set_uint32_value( a_handle );
}

void sdp_service_record::set_service_class_id_list( std::vector<uuid> const& a_uuids )
{
    auto attribute = find_attribute( sdp_universal_attribute_id::service_class_id_list );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::service_class_id_list );
    }

    auto& attribute_value = attribute->get_value();
    sdp_data_element uuid_attri;
    for( auto& ele : a_uuids )
    {
        uuid_attri.set_uuid( ele );
        attribute_value.add_element( uuid_attri );
    }
}

void sdp_service_record::set_protocol_descriptor_list( std::vector<protocol_descriptor> a_list )
{
    sdp_attribute attri;
    attri.set_attribute_id( sdp_universal_attribute_id::protocol_descriptor_list );

    std::vector<sdp_data_element> protocol_list;
    for( auto& ele : a_list )
    {
        sdp_data_element protocol;
        std::vector<sdp_data_element> protocol_descs;

        sdp_data_element uuid_attri;
        uuid_attri.set_uuid( ele.m_uuid );
        protocol_descs.push_back( uuid_attri );

        protocol_descs.insert( protocol_descs.end(), ele.m_parameters.begin(), ele.m_parameters.end() );

        protocol.set_elements( protocol_descs );

        protocol_list.push_back( protocol );
    }

    attri.get_value().set_elements( protocol_list );

    m_attributes.push_back( attri );
}

void sdp_service_record::set_service_record_state( uint32_t a_state )
{
    auto attribute = find_attribute( sdp_universal_attribute_id::service_record_state );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::service_record_state );
    }

    auto& attribute_value = attribute->get_value();
    attribute_value.set_uint32_value( a_state );
}

void sdp_service_record::set_bluetooth_profile_descriptor_list( std::vector<sdp_data_element> a_descriptor_list )
{
    auto attribute = find_attribute( sdp_universal_attribute_id::bluetooth_profile_descriptor_list );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::bluetooth_profile_descriptor_list );
    }

    auto& attribute_value = attribute->get_value();
    attribute_value.set_elements( std::move( a_descriptor_list ) );
}

bool sdp_service_record::is_matching_uuids( std::vector<uuid> const& a_uuids )
{
    bool ret = false;
    std::vector<uuid> contain_uuids;
    for( auto& ele : m_attributes )
    {
        sdp_data_element& value = ele.get_value();
        if( value.can_as_uuid() )
        {
            contain_uuids.push_back( value.get_uuid() );
            continue;
        }

        if( value.can_as_elements() )
        {
            std::vector<uuid> uuids;
            uuids = value.get_uuid_from_elements();
            contain_uuids.insert( contain_uuids.end(), uuids.begin(), uuids.end() );
            continue;
        }
    }

    for( auto& goal_uuid : a_uuids )
    {
        bool has = false;
        for( auto& local_uuid : contain_uuids )
        {
            if( goal_uuid == local_uuid )
            {
                has = true;
                break;
            }
        }

        if( !has )
        {
            ret = false;
            return ret;
        }
    }

    ret = true;
    return ret;
}

void sdp_service_record::set_default_language()
{
    auto attribute = find_attribute( sdp_universal_attribute_id::language_base_attribute_id_list );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::language_base_attribute_id_list );
    }
    attribute->get_value().set_elements( make_language_attribute_list() );
}

void sdp_service_record::set_attribute
    (
    uint16_t a_attribute_id,
    sdp_data_element a_attribute_value
    )
{
    auto attribute = find_attribute( a_attribute_id );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( a_attribute_id );
    }
    attribute->get_value() = a_attribute_value;
}

sdp_attribute* sdp_service_record::find_attribute( uint16_t a_attribute_id )
{
    for( auto& ele : m_attributes )
    {
        if( ele.get_attribute_id() == a_attribute_id )
        {
            return &ele;
        }
    }
    return nullptr;
}

void sdp_service_record::sort_attribute_by_id()
{
    m_attributes.sort( []( sdp_attribute const& a_left, sdp_attribute const& a_right )
    {
        return a_left.get_attribute_id() < a_right.get_attribute_id();
    } );
}

std::vector<sdp_data_element> make_language_attribute_list()
{
    std::vector<sdp_data_element> lang_list;

    sdp_data_element temp;
    temp.set_uint16_value( language_code::english );
    lang_list.push_back( temp );
    temp.set_uint16_value( code_page::utf_8 );
    lang_list.push_back( temp );
    temp.set_uint16_value( language_base_id::english );
    lang_list.push_back( temp );

    temp.set_uint16_value( language_code::chinese );
    lang_list.push_back( temp );
    temp.set_uint16_value( code_page::utf_8 );
    lang_list.push_back( temp );
    temp.set_uint16_value( language_base_id::chinese );
    lang_list.push_back( temp );

    return lang_list;
}

}

