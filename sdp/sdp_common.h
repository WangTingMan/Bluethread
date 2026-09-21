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
#include <list>
#include <tuple>
#include <string>

#include "uuid.h"

namespace bluetooth
{

namespace sdp_universal_attribute_id
{
    constexpr uint16_t service_record_handle = 0x0000;
    constexpr uint16_t service_class_id_list = 0x0001;
    constexpr uint16_t service_record_state = 0x0002;
    constexpr uint16_t service_id = 0x0003;
    constexpr uint16_t protocol_descriptor_list = 0x0004;
    constexpr uint16_t addticional_protocol_descriptor_list = 0x000D;
    constexpr uint16_t browse_group_list = 0x0005;
    constexpr uint16_t language_base_attribute_id_list = 0x0006;
    constexpr uint16_t service_info_time_to_line = 0x0007;
    constexpr uint16_t service_availability = 0x0008;
    constexpr uint16_t bluetooth_profile_descriptor_list = 0x0009;
    constexpr uint16_t documentation_url = 0x000A;
    constexpr uint16_t client_executable_url = 0x000B;
    constexpr uint16_t icon_url = 0x000C;

    constexpr uint16_t service_name_offset = 0x0000;
    constexpr uint16_t service_description_offset = 0x0001;
    constexpr uint16_t provider_name_offset = 0x0002;
};

namespace sdp_self_attribute_id
{
    constexpr uint16_t version_number_list = 0x0200;
    constexpr uint16_t service_database_state = 0x0201;
}

namespace did_attribute_id
{
    constexpr uint16_t specification_id = 0x0200;
    constexpr uint16_t vendor_id = 0x0201;
    constexpr uint16_t product_id = 0x0202;
    constexpr uint16_t version = 0x0203;
    constexpr uint16_t primary_record = 0x0204;
    constexpr uint16_t vendor_id_source = 0x0205;
}

namespace sdp_service_uuid
{
    constexpr uint16_t sdp = 0x0001;
    constexpr uint16_t udp = 0x0002;
    constexpr uint16_t rfcomm = 0x0003;
    constexpr uint16_t tcp = 0x0004;
    constexpr uint16_t tcs_bin = 0x0005;
    constexpr uint16_t tcs_at = 0x0006;
    constexpr uint16_t att = 0x0007;
    constexpr uint16_t obex = 0x0008;
    constexpr uint16_t ip = 0x0009;
    constexpr uint16_t ftp = 0x000A;
    constexpr uint16_t http = 0x000C;
    constexpr uint16_t wsp = 0x000E;
    constexpr uint16_t bnep = 0x000F;
    constexpr uint16_t upnp = 0x0010;
    constexpr uint16_t hidp = 0x0011;
    constexpr uint16_t hardcopy_control_channel = 0x0012;
    constexpr uint16_t hardcopy_data_channel = 0x0014;
    constexpr uint16_t hardcopy_notification = 0x0016;
    constexpr uint16_t avctp = 0x0017;
    constexpr uint16_t avdtp = 0x0019;
    constexpr uint16_t cmtp = 0x001B;
    constexpr uint16_t mcap_control_channel = 0x001E;
    constexpr uint16_t mcap_data_channel = 0x001F;
    constexpr uint16_t l2cap = 0x0100;

    constexpr uint16_t service_discovey_server_service_class_id = 0x1000;
    constexpr uint16_t browse_group_descriptor_service_class_id = 0x1001;
    constexpr uint16_t serial_port = 0x1101;
    constexpr uint16_t pnp_information = 0x1200;
};

enum class sdp_self_service_attribute_id : uint16_t
{
    version_number_list = 0x0200,
    service_database_state = 0x0201
};

namespace language_code
{
    constexpr uint16_t english = 0x656e; // en
    constexpr uint16_t french = 0x6672;
    constexpr uint16_t german = 0x6465;
    constexpr uint16_t japanese = 0x6A61;
    constexpr uint16_t chinese = 0x7A68; // zh
};

namespace language_base_id
{
    constexpr uint16_t english = 0x0100;
    constexpr uint16_t chinese = english + 0x10;
}

namespace code_page
{
    // see https://www.iana.org/assignments/character-sets/character-sets.xhtml
    constexpr uint16_t utf_8 = 0x006a;
    constexpr uint16_t gbk = 113;
    constexpr uint16_t gb18030 = 114;
};

enum class sdp_attribute_value_type : uint8_t
{
    null = 0x00,
    unsigned_integer = 0x01,
    signed_integer = 0x02,
    uuid = 0x03,
    string = 0x04,
    boolean_type = 0x05,
    data_elements = 0x06,
    alternative_data_element = 0x07,
    url = 0x08,
};

class sdp_data_element
{

public:

    static std::tuple<sdp_data_element, bool> parse_from( uint8_t* a_buffer, uint32_t a_size );

    /**
     * recognite given buffer if it is data element raw buffer.
     * return true if the buffer can be treated as data element raw buffer otherwise return false
     * a_invalid_size indicates the data element buffer length.
     */
    static bool recognite_data_element( uint8_t* a_buffer, uint32_t a_size, uint32_t& a_invalid_size );

    void set_null_value();

    void set_uint32_value( uint32_t a_value );

    uint32_t get_uint32_value()const;

    bool can_as_uint32()const;

    void set_uint8_value( uint8_t a_value );

    uint8_t get_uint8_value()const;

    bool can_as_uint8()const;

    void set_string_value( std::u8string const& a_value );

    std::u8string get_string_value()const;

    bool can_as_string()const;

    void set_uint16_value( uint16_t a_value );

    uint16_t get_uint16_value()const;

    bool can_as_uint16()const;

    void set_uuid( uuid const& a_uuid );

    uuid get_uuid()const;

    bool can_as_uuid()const;

    void set_elements( std::vector<sdp_data_element> a_elements );

    void add_element( sdp_data_element a_element );

    std::vector<sdp_data_element>const& get_elements()const;

    bool can_as_elements()const;

    /**
     * If the detail type is elements, then use this function to extract all the
     * uuids from these elements.
     */
    std::vector<uuid> get_uuid_from_elements();

    void set_boolean( bool a_value = true );

    bool get_boolean()const;

    bool can_as_boolean()const;

    std::tuple<sdp_attribute_value_type, uint8_t> get_value_type()
    {
        return { m_value_type, m_size_index };
    }

    std::vector<uint8_t> const& get_raw_buffer()const;

    void clear()
    {
        m_elemets.clear();
        m_value_type = sdp_attribute_value_type::null;
    }

    sdp_data_element() = default;

    sdp_data_element( sdp_data_element&& a_right ) noexcept
    {
        stolen_from( std::move( a_right ) );
    }

    sdp_data_element( sdp_data_element const& a_right )
    {
        copy_from( a_right );
    }

    sdp_data_element& operator=( sdp_data_element&& a_right ) noexcept
    {
        stolen_from( std::move( a_right ) );
        return *this;
    }

    sdp_data_element& operator=( sdp_data_element const& a_right )
    {
        copy_from( a_right );
        return *this;
    }

private:

    void stolen_from( sdp_data_element&& a_right )noexcept
    {
        m_size_index = a_right.m_size_index;
        m_buffer.swap( a_right.m_buffer );
        m_value_type = a_right.m_value_type;
        m_elemets.swap( a_right.m_elemets );
    }

    void copy_from( sdp_data_element const& a_right )
    {
        m_size_index = a_right.m_size_index;
        m_buffer = a_right.m_buffer;
        m_value_type = a_right.m_value_type;
        m_elemets = a_right.m_elemets;
    }

    void prepare_raw_for_elements();

    uint8_t m_size_index = 0;
    std::vector<uint8_t> m_buffer;
    sdp_attribute_value_type m_value_type = sdp_attribute_value_type::null;
    std::vector<sdp_data_element> m_elemets; // If the type is elements, then this member can be used.
};

struct protocol_descriptor
{
    void clear()
    {
        m_parameters.clear();
    }

    protocol_descriptor() = default;

    protocol_descriptor( protocol_descriptor&& a_right ) noexcept
    {
        stolen_from( std::move( a_right ) );
    }

    protocol_descriptor( protocol_descriptor const& a_right )
    {
        copy_from( a_right );
    }

    protocol_descriptor& operator=( protocol_descriptor&& a_right ) noexcept
    {
        stolen_from( std::move( a_right ) );
        return *this;
    }

    protocol_descriptor& operator=( protocol_descriptor const& a_right )
    {
        copy_from( a_right );
        return *this;
    }

    void stolen_from( protocol_descriptor&& a_right )noexcept
    {
        m_uuid = a_right.m_uuid;
        m_parameters = std::move( a_right.m_parameters );
    }

    void copy_from( protocol_descriptor const& a_right )
    {
        m_uuid = a_right.m_uuid;
        m_parameters = a_right.m_parameters;
    }

    uuid m_uuid;
    std::vector<sdp_data_element> m_parameters;
};

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

class sdp_service_record
{

public:

    virtual ~sdp_service_record() {}

    uint32_t get_service_handle();

    void set_service_handle( uint32_t a_handle );

    void set_service_class_id_list( std::vector<uuid> const& a_uuids );

    void set_protocol_descriptor_list( std::vector<protocol_descriptor > a_list );

    void set_service_record_state( uint32_t a_state );

    void set_bluetooth_profile_descriptor_list( std::vector<sdp_data_element> a_descriptor_list );

    bool is_matching_uuids( std::vector<uuid> const& a_uuids );

    void set_default_language();

    void set_attribute
        (
        uint16_t a_attribute_id,
        sdp_data_element a_attribute_value
        );

    std::list<sdp_attribute>::iterator begin()
    {
        return m_attributes.begin();
    }

    std::list<sdp_attribute>::iterator end()
    {
        return m_attributes.end();
    }

    /**
     * Within each attribute list, the attributes are listed in ascending order of attribute ID value.
     */
    void sort_attribute_by_id();

protected:

    sdp_attribute* find_attribute( uint16_t a_attribute_id );

    std::list<sdp_attribute> m_attributes;
};

std::vector<sdp_data_element> make_language_attribute_list();

}

