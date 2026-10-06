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

#include "sdp_attribute.h"
#include "sdp_data_element.h"
#include "protocol_descriptor.h"

#include <list>

namespace bluetooth
{

class sdp_service_record
{

public:

    virtual ~sdp_service_record() {}

    uint32_t get_service_handle();

    /**
     * @brief Set the service record handle of this service record.
     *
     * ServiceRecordHandle is defined in Bluetooth SDP specification section 5.1.1.
     * Its attribute ID is 0x0000. This 32-bit value uniquely identifies a service record
     * within one SDP server.
     *
     * @warning This function shall only be invoked internally by the SDP module.
     * External profiles MUST NOT modify this value after service registration.
     *
     * @param a_handle 32-bit service record handle value.
     */
    void set_service_handle( uint32_t a_handle );

    /**
     * @brief Set ServiceClassIDList attribute value for the service record.
     *
     * ServiceClassIDList is defined in Bluetooth SDP specification section 5.1.2.
     * Its attribute ID is 0x0001. It is a sequence of UUIDs indicating service classes
     * supported by this service record, ordered from most specific to most generic.
     *
     * @param a_uuids Const reference to vector of UUIDs for constructing ServiceClassIDList.
     */
    void set_service_class_id_list( std::vector<uuid> const& a_uuids );

    /**
     * @brief Set the value of ProtocolDescriptorList attribute for a service record.
     *
     * Refer to Bluetooth SDP specification section 5.1.5 for the introduction of ProtocolDescriptorList.
     * The attribute ID of this attribute is 0x0004, and its value is specified by the parameter a_list.
     *
     * @param a_list List of protocol descriptors used to construct ProtocolDescriptorList attribute value.
     */
    void set_protocol_descriptor_list( std::vector<protocol_descriptor > a_list );

    /**
     * @brief Set ServiceRecordState attribute value for the service record.
     *
     * ServiceRecordState is defined in Bluetooth SDP specification section 5.1.3.
     * Its attribute ID is 0x0002. This 32-bit unsigned integer conveys state information
     * associated with the service record.
     *
     * @param a_state 32-bit state value of the service record.
     */
    void set_service_record_state( uint32_t a_state );

    void set_bluetooth_profile_descriptor_list( std::vector<sdp_data_element> a_descriptor_list );

    /**
     * @brief Set BluetoothProfileDescriptorList attribute value for the service record.
     *
     * BluetoothProfileDescriptorList is defined in Bluetooth SDP specification section 5.1.11.
     * Its attribute ID is 0x0009. It is a sequence of pairs, each containing a profile UUID
     * and a 16-bit profile version number, describing the Bluetooth profiles supported by this service.
     *
     * @param a_descriptor_list List of profile descriptor pairs: (profile UUID, profile version).
     */
    void set_bluetooth_profile_descriptor_list( std::vector<std::pair<uuid, uint16_t>> a_descriptor_list )
    {
        std::vector<sdp_data_element> _descriptor_list;
        sdp_data_element paras;
        for( auto& ele : a_descriptor_list )
        {
            paras.set_uuid( ele.first );
            _descriptor_list.push_back( paras );

            paras.set_uint16_value( ele.second );
            _descriptor_list.push_back( paras );
        }
        set_bluetooth_profile_descriptor_list( std::move( _descriptor_list ) );
    }

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

    sdp_attribute* find_or_create_attribute( uint16_t a_attribute_id )
    {
        auto attribute = find_attribute( a_attribute_id );
        if( !attribute )
        {
            m_attributes.push_back( sdp_attribute() );
            attribute = &( m_attributes.back() );
            attribute->set_attribute_id( a_attribute_id );
        }
        return attribute;
    }

    std::list<sdp_attribute> m_attributes;
};

}
