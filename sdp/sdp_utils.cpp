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

#include "sdp/sdp_utils.h"

namespace bluetooth
{

uint16_t get_max_version_number_for_uuid
    (
    std::vector<sdp_data_element>const& a_profile_descriptor_list,
    uuid a_uuid
    )
{
    uint16_t version = 0;
    bool status = false;
    for( int z = 0; z < a_profile_descriptor_list.size(); z++ )
    {
        sdp_data_element const& profile_descriptor = a_profile_descriptor_list[z];
        status = profile_descriptor.can_as_elements();
        if( !status )
        {
            continue;
        }

        std::vector<sdp_data_element>const& descriptors = profile_descriptor.get_elements();
        for( int i = 0; i < descriptors.size(); i = i + 2 )
        {
            uint16_t this_version = 0;
            status = descriptors[i].can_as_uuid();
            if( !status )
            {
                continue;
            }

            uuid this_uuid = descriptors[i].get_uuid();
            if( this_uuid != a_uuid )
            {
                continue;
            }

            int next = i + 1;
            if( next >= descriptors.size() )
            {
                break;
            }
            auto const& next_ele = descriptors[next];
            status = next_ele.can_as_uint16();
            if( !status )
            {
                continue;
            }
            this_version = next_ele.get_uint16_value();
            version = this_version > version ? this_version : version;
        }
    }
    return version;
}

}

