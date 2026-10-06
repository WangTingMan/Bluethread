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
#include "sdp/sdp_service_record.h"

namespace bluetooth
{

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


}

