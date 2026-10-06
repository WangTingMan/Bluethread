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

}
