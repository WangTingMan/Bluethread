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

#include "sdp_common.h"

namespace bluetooth
{

class sdp_data_element
{

public:

    /**
    * @brief Parse a single SDP DataElement from the input raw buffer
    *
    * @param a_buffer Pointer to the raw input buffer containing SDP DataElement binary data
    * @param a_size Total available byte length of the input buffer
    * @param a_parsed_size [OUT] If parsing succeeds, stores total bytes consumed by this single DataElement
    *        (DE header + length field + payload). If parsing fails, this output value is undefined.
    * @param a_value [OUT] Parsed SDP DataElement result.
    *        If parsing fails, this output value is undefined.
    * @param a_depth Current nesting depth of this DataElement.
    *        Increment this value when recursively parsing child elements inside Sequence / Alternative.
    * @param a_max_depth Maximum allowed nesting depth for SDP DataElement.
    *        Parsing returns false immediately if a_depth >= a_max_depth, to prevent stack overflow from
    *        malicious deeply nested packets.
    *
    * @return bool Return true when single DataElement parsed successfully.
    *         Return false for any error (malformed header, out of bounds, length invalid, nesting depth exceeded etc.)
    *
    * @note This function parses ONLY ONE SDP DataElement, not multiple elements.
    * @note When type is Sequence or Alternative, this function will recursively parse its child elements,
    *       passing a_depth + 1 for nested parsing call.
    * @note Upon return false, all output parameters (a_parsed_size, a_value) are invalid and shall not be used.
    */
    static bool parse_from
        (
        uint8_t const* a_buffer,
        uint32_t a_size,
        uint32_t& a_parsed_size,
        sdp_data_element& a_value,
        uint16_t a_depth = 0,
        uint16_t a_max_depth = 10
        );

    /**
    * @brief Parse a sequence of SDP DataElements from raw buffer
    *
    * @param a_buffer Pointer to input raw buffer containing continuous SDP DataElements
    * @param a_size Total byte length available in a_buffer
    * @param a_max_root_elements Maximum count of root DataElements allowed to parse.
    *        Stop parsing once this number of root elements is reached.
    * @param a_elements Output vector, parsed valid root SDP DataElements will be appended here.
    *        The vector is NOT cleared inside this function; caller is responsible for clearing before invocation.
    *
    * @return bool Return true when all parsed elements are valid and parsing completes successfully.
    *         Return false immediately upon any parsing error (malformed header, out-of-bounds, length mismatch etc.)
    *
    * @note Input buffer holds sequential independent root-level SDP DataElements.
    * @note Nested elements inside Sequence / Alternative are counted as child elements, not root elements.
    */
    static bool parse_elements_from
        (
        uint8_t const* a_buffer,
        uint32_t a_size,
        uint16_t a_max_root_elements,
        std::vector<sdp_data_element>& a_elements,
        uint16_t a_depth = 0,
        uint16_t a_max_depth = 10
        );

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

    void set_url_value( std::u8string const& a_value );

    std::u8string get_url_value()const;

    bool can_as_url()const;

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

    void set_alternative_elements( std::vector<sdp_data_element> a_elements )
    {
        m_elemets = std::move( a_elements );
        m_value_type = sdp_attribute_value_type::alternative_data_element;
        prepare_raw_for_elements();
    }

    void add_alternative_element( sdp_data_element a_element )
    {
        m_value_type = sdp_attribute_value_type::alternative_data_element;
        m_elemets.push_back( a_element );
        prepare_raw_for_elements();
    }

    std::vector<sdp_data_element>const& get_alternative_elements()const
    {
        return m_elemets;
    }

    bool can_as_alternative_elements()const
    {
        return sdp_attribute_value_type::alternative_data_element == m_value_type;
    }

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

std::vector<sdp_data_element> make_language_attribute_list();

}
