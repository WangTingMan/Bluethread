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

#include "sdp_self_service_record.h"

namespace bluetooth
{

sdp_self_service_record::sdp_self_service_record()
{
    set_service_handle( 0x00 );

    std::vector<uuid> uuids;
    uuids.push_back( uuid::from_16bit( sdp_service_uuid::service_discovey_server_service_class_id ) );
    set_service_class_id_list( uuids );

    uint16_t origin_version = 0x0100;
    uint16_t latest_version = 0x0101;
    std::vector<uint16_t> versions;
    versions.push_back( origin_version );
    versions.push_back( latest_version );
    set_version_number_list( versions );

    set_service_database_state( m_current_state );
    set_service_record_state( 0x00 );
}

void sdp_self_service_record::set_version_number_list( std::vector<uint16_t> const& a_versions )
{
    auto attribute = find_attribute( sdp_self_attribute_id::version_number_list );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_self_attribute_id::version_number_list );
    }

    attribute->clear();
    auto& attribute_value = attribute->get_value();
    sdp_data_element version_attri;
    for( auto& ele : a_versions )
    {
        version_attri.set_uint16_value( ele );
        attribute_value.add_element( version_attri );
    }
}

void sdp_self_service_record::set_service_database_state( uint32_t a_state )
{
    auto attribute = find_attribute( sdp_self_attribute_id::service_database_state );
    if( !attribute )
    {
        m_attributes.push_back( sdp_attribute() );
        attribute = &( m_attributes.back() );
        attribute->set_attribute_id( sdp_self_attribute_id::service_database_state );
    }

    attribute->get_value().set_uint32_value( a_state );
}

}

