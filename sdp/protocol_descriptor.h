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
#include "sdp_data_element.h"
#include "uuid.h"

#include <vector>

namespace bluetooth
{

/**
* @brief Protocol descriptor entry for SDP ProtocolDescriptorList attribute.
*
* Refer to Bluetooth SDP specification section 5.1.5
* 
* Each protocol descriptor describes one layer in the protocol stack for a service.
* Consists of a protocol UUID and optional additional protocol parameters.
* Multiple entries form the ProtocolDescriptorList sequence (Attribute ID 0x0004).
*/
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

    /**
     * @brief UUID identifying the protocol, e.g. L2CAP, RFCOMM, HFP.
     */
    uuid m_uuid;

    /**
     * @brief Optional protocol parameters following the protocol UUID.
     * Represented as SDP data elements, may be empty if no extra parameters required.
     */
    std::vector<sdp_data_element> m_parameters;
};

}
