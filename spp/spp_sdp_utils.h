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
* @brief Parsed SDP attribute set for SPP service record
*
* Holds extracted fields from an SPP SDP service record.
* When @p is_valid is true, the structure content is usable and @p port is valid.
* service-name and version-related string fields are optional. Corresponding boolean flags
* indicate whether those optional fields exist in the original SDP record and have been filled.
* If multiple language-specific service-name entries are present, only the primary-language
* service name is returned; other language variants are discarded.
*/
struct spp_sdp_service_attribute
{
    bool is_valid = false;             ///< Overall validity flag. If true, this struct is usable and port holds valid value
    bool has_name = false;             ///< True if service_name was present and parsed from SDP record
    std::u8string service_name;        ///< Human-readable service name; valid only when has_name == true. Primary-language name is selected when multi-language entries exist
    uint8_t port;                      ///< RFCOMM DLCI port number; valid only when is_valid == true
    bool has_spp_version = false;      ///< True if SPP version information was present and parsed from SDP record
    std::string version_string;        ///< String-formatted SPP version text; valid only when has_spp_version == true
    uint16_t version_number;           ///< Numeric SPP version identifier; valid only when has_spp_version == true
    uint16_t service_handle;           ///< SDP service-handle of the source service record
};

/**
 * @brief Attempts to extract basic SPP service information from SDP service record elements.
 *
 * Parses SPP-related fields from the provided list of SDP service record elements.
 * When an Alternative container is encountered, this function extracts information
 * from the first child element inside the Alternative and ignores remaining candidates.
 *
 * @param a_service_records Input list of SDP service record elements. The caller shall guarantee
 *        that the input conforms to SPP service record requirements. No further validity checks
 *        are performed inside this function.
 * @return spp_sdp_service_attribute Parsed SPP service attribute structure.
 *
 * @note This is a pure functional function. Its return value depends solely on input parameters.
 *       No global or mutable static state is read or modified.
 * @note Thread-safe: no access to shared mutable state, safe for concurrent multi-thread invocation.
 * @note For Alternative elements, only the first sub-element is used for parsing; other alternatives are discarded.
 * @note Input format compliance is the responsibility of the caller. No validation whether the record is truly an SPP record.
 */
spp_sdp_service_attribute parse_spp_sdp_attrubutes( std::vector<sdp_data_element> const& a_service_records );

}

