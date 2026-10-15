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

#include "spp/spp_sdp_utils.h"
#include "sdp/sdp_utils.h"

namespace bluetooth
{

uint8_t retrieve_rfcomm_port( const sdp_data_element& a_attribute_value );

spp_sdp_service_attribute parse_spp_sdp_attrubutes( std::vector<sdp_data_element> const& a_service_records )
{
    using namespace sdp_universal_attribute_id;
    using namespace attribute_id_offset_for_string;

    spp_sdp_service_attribute attrs;
    bool status = false;
    if( a_service_records.empty() )
    {
        return attrs;
    }

    sdp_data_element const& first_service_records = a_service_records[0];
    status = first_service_records.can_as_elements();
    if( !status )
    {
        return attrs;
    }

    std::vector<sdp_data_element>const& service_record_attributes_list = first_service_records.get_elements();
    uint16_t attribute_id = 0;
    constexpr uint16_t default_primary_base_id = 0x0100;

    for( auto j = 0; j < service_record_attributes_list.size(); j = j + 2 )
    {
        status = service_record_attributes_list[j].can_as_uint16();
        if( !status )
        {
            continue;
        }
        attribute_id = service_record_attributes_list[j].get_uint16_value();

        uint16_t next = j + 1;
        if( next >= service_record_attributes_list.size() )
        {
            break;
        }

        auto const& attribute_value = service_record_attributes_list[next];
        switch( attribute_id )
        {
        case service_record_handle:
            {
                status = attribute_value.can_as_uint32();
                if( status )
                {
                    attrs.service_handle = attribute_value.get_uint32_value();
                }
            }
            break;
        case bluetooth_profile_descriptor_list:
            {
                uint16_t version = 0;
                status = attribute_value.can_as_elements();
                if( !status )
                {
                    continue;
                }

                std::vector<sdp_data_element>const& profile_descriptor_list = attribute_value.get_elements();
                version = get_max_version_number_for_uuid( profile_descriptor_list,
                    uuid::from_16bit( sdp_service_uuid::spp ) );
                attrs.has_spp_version = version != 0x0000;
                attrs.version_number = version;
                const uint8_t major = static_cast<uint8_t>( ( version >> 8U ) & 0xFFU );
                const uint8_t minor = static_cast<uint8_t>( version & 0xFFU );
                attrs.version_string = std::to_string( major ) + "." + std::to_string( minor );
            }
            break;
        case protocol_descriptor_list:
            {
                uint8_t port_number = 0;
                port_number = retrieve_rfcomm_port( attribute_value );
                attrs.is_valid = port_number != 0;
                attrs.port = port_number;
            }
            break;
        case default_primary_base_id + service_name_offset:
            {
                status = attribute_value.can_as_string();
                if( !status )
                {
                    continue;
                }

                attrs.has_name = true;
                attrs.service_name = attribute_value.get_string_value();
            }
            break;
        default:
            break;
        }
    }

    return attrs;
}

uint8_t retrieve_rfcomm_port( const sdp_data_element& a_attribute_value )
{
    uint8_t port = 0;
    bool status = false;
    status = a_attribute_value.can_as_alternative_elements();
    if( status )
    {
        /* there are two or more protocol stacks */
        std::vector<sdp_data_element>const& protocol_stacks = a_attribute_value.get_alternative_elements();
        for( int i = 0; i < protocol_stacks.size(); ++i )
        {
            port = retrieve_rfcomm_port( protocol_stacks[i] );
            if( port != 0 )
            {
                return port;
            }
        }
        return port;
    }

    status = a_attribute_value.can_as_elements();
    if( !status )
    {
        return port;
    }

    std::vector<sdp_data_element>const& protocol_descriptors = a_attribute_value.get_elements();
    for( int i = 0; i < protocol_descriptors.size(); ++i )
    {
        status = protocol_descriptors[i].can_as_elements();
        if( !status )
        {
            continue;
        }

        std::vector<sdp_data_element>const& one_protocol_descriptor = protocol_descriptors[i].get_elements();
        for( int k = 0; k < one_protocol_descriptor.size(); k = k + 2)
        {
            status = one_protocol_descriptor[k].can_as_uuid();
            if( !status )
            {
                continue;
            }

            auto thiz_uuid = one_protocol_descriptor[k].get_uuid();
            if( thiz_uuid != uuid::from_16bit( sdp_service_uuid::rfcomm ) )
            {
                continue;
            }

            int next = k + 1;
            sdp_data_element const& parameter = one_protocol_descriptor[next];
            status = parameter.can_as_uint8();
            if( !status )
            {
                continue;
            }

            port = parameter.get_uint8_value();
            break;
        }
    }

    return port;
}

}

