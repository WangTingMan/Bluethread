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

#include "did_service_record.h"
#include "../framework\internal/platform.h"

namespace bluetooth
{

constexpr uint16_t specification_id_value = 0x0103;
constexpr uint16_t vendor_id_value = 0x00E0;
constexpr uint16_t product_id_value = 0x1020;
constexpr uint16_t version_value = 0x0436;

did_service_record::did_service_record()
{
    std::vector<uuid> uuids;
    uuids.push_back( uuid::from_16bit( sdp_service_uuid::pnp_information ) );
    set_service_class_id_list( uuids );

    auto attribute = find_attribute( did_attribute_id::specification_id );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( did_attribute_id::specification_id );
    }
    attribute->get_value().set_uint16_value( specification_id_value );

    attribute = find_attribute( did_attribute_id::vendor_id );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( did_attribute_id::vendor_id );
    }
    attribute->get_value().set_uint16_value( vendor_id_value );

    attribute = find_attribute( did_attribute_id::product_id );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( did_attribute_id::product_id );
    }
    attribute->get_value().set_uint16_value( product_id_value );

    attribute = find_attribute( did_attribute_id::version );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( did_attribute_id::version );
    }
    attribute->get_value().set_uint16_value( version_value );

    attribute = find_attribute( did_attribute_id::primary_record );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( did_attribute_id::primary_record );
    }
    attribute->get_value().set_boolean();

    attribute = find_attribute( did_attribute_id::vendor_id_source );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( did_attribute_id::vendor_id_source );
    }
    attribute->get_value().set_uint16_value( 0x0001 );

    attribute = find_attribute( sdp_universal_attribute_id::language_base_attribute_id_list );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::language_base_attribute_id_list );
    }
    attribute->get_value().set_elements( make_language_attribute_list() );

    attribute = find_attribute( sdp_universal_attribute_id::provider_name_offset + language_base_id::english );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::provider_name_offset + language_base_id::english );
    }
    attribute->get_value().set_string_value( framework::convert( "Wang Fei" ) );

    attribute = find_attribute( sdp_universal_attribute_id::provider_name_offset + language_base_id::chinese );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_universal_attribute_id::provider_name_offset + language_base_id::chinese );
    }
    attribute->get_value().set_string_value( framework::convert( "Íõ·É" ) );
}

}

