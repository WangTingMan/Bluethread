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

#include "sdp/sdp_common.h"
#include "sdp/sdp_data_element.h"

#include <cstdint>
#include <string>

namespace bluetooth
{

/**
* @brief Retrieve the maximum profile version number for a given UUID from BluetoothProfileDescriptorList
*
* Iterates through the BluetoothProfileDescriptorList data?element container, filters profile descriptors
* matching the target @p a_uuid, and returns the maximum raw 16?bit profile version number among matched entries.
* Handles both Sequence and Alternative container types within the descriptor list.
*
* @param a_profile_descriptor_list Const reference to parsed data?elements of BluetoothProfileDescriptorList attribute.
* @param a_uuid Target profile UUID to match against each profile descriptor entry.
* @return uint16_t Maximum raw 16?bit profile version number. If no matching profile descriptor is found,
*         returns zero as the sentinel value for "not found".
*
* @note This is a pure functional function. Return value depends solely on input parameters.
*       No global or mutable static state is read or modified.
* @note Thread?safe: no access to shared mutable state, safe for concurrent multi?thread invocation.
* @note The returned raw 16?bit version follows Bluetooth specification: high?order octet is major version,
*       low?order octet is minor version. This version is for informational/display purposes only;
*       do not use it for connection?time or runtime profile capability decisions.
*/
uint16_t get_max_version_number_for_uuid
    (
    std::vector<sdp_data_element>const& a_profile_descriptor_list,
    uuid a_uuid
    );

}


