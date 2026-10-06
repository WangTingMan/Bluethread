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
#include <cstdint>
#include <vector>
#include <list>
#include <tuple>
#include <string>

#include "uuid.h"

namespace bluetooth
{

namespace sdp_universal_attribute_id
{
    /**
     * The definition of `sdp_universal_attribute_id` can be found in Chapter 5
     * of the SDP specification. Refer to section 5.1.5 of the assigned numbers
     * document for specific values.
     */
    constexpr uint16_t service_record_handle = 0x0000;
    constexpr uint16_t service_class_id_list = 0x0001;
    constexpr uint16_t service_record_state = 0x0002;
    constexpr uint16_t service_id = 0x0003;
    constexpr uint16_t protocol_descriptor_list = 0x0004;
    constexpr uint16_t browse_group_list = 0x0005;
    constexpr uint16_t language_base_attribute_id_list = 0x0006;
    constexpr uint16_t service_info_time_to_line = 0x0007;
    constexpr uint16_t service_availability = 0x0008;
    constexpr uint16_t bluetooth_profile_descriptor_list = 0x0009;
    constexpr uint16_t documentation_url = 0x000A;
    constexpr uint16_t client_executable_url = 0x000B;
    constexpr uint16_t icon_url = 0x000C;
    constexpr uint16_t addticional_protocol_descriptor_list = 0x000D;

}

namespace language_code
{
    constexpr uint16_t english = 0x656e; // en
    constexpr uint16_t french = 0x6672;
    constexpr uint16_t german = 0x6465;
    constexpr uint16_t japanese = 0x6A61;
    constexpr uint16_t chinese = 0x7A68; // zh
};

namespace language_base_id
{
    /**
     * Base attribute ID for primary English language textual attributes.
     * According to Bluetooth SDP specification, the first triplet in LanguageBaseAttributeIDList
     * SHALL use 0x0100 as its base attribute ID.
     */
    constexpr uint16_t english = 0x0100;
    /**
     * Base attribute ID for Chinese language textual attributes.
     * Derived offset value, ensuring base+0 / base+1 / base+2 fall within 0x0100~0x01FF range,
     * and do not collide with other attribute IDs inside the same service record.
     */
    constexpr uint16_t chinese = english + 0x10;
}

/**
 * Refers to SDP profile's section 5.1.15 to section 5.1.17, which defines the attribute
 * IDs for service name, service description, and provider name.
 * These values are from Assigned Numbers, section 5.2.
 */
namespace attribute_id_offset_for_string
{
    constexpr uint16_t service_name_offset = 0x0000;
    constexpr uint16_t service_description_offset = 0x0001;
    constexpr uint16_t provider_name_offset = 0x0002;
}

namespace code_page
{
    // see https://www.iana.org/assignments/character-sets/character-sets.xhtml
    constexpr uint16_t utf_8 = 106;
    constexpr uint16_t gbk = 113;
    constexpr uint16_t gb18030 = 114;
};

namespace sdp_self_attribute_id
{
    constexpr uint16_t version_number_list = 0x0200;
    constexpr uint16_t service_database_state = 0x0201;
}

namespace sdp_service_uuid
{
    constexpr uint16_t sdp = 0x0001;
    constexpr uint16_t udp = 0x0002;
    constexpr uint16_t rfcomm = 0x0003;
    constexpr uint16_t tcp = 0x0004;
    constexpr uint16_t tcs_bin = 0x0005;
    constexpr uint16_t tcs_at = 0x0006;
    constexpr uint16_t att = 0x0007;
    constexpr uint16_t obex = 0x0008;
    constexpr uint16_t ip = 0x0009;
    constexpr uint16_t ftp = 0x000A;
    constexpr uint16_t http = 0x000C;
    constexpr uint16_t wsp = 0x000E;
    constexpr uint16_t bnep = 0x000F;
    constexpr uint16_t upnp = 0x0010;
    constexpr uint16_t hidp = 0x0011;
    constexpr uint16_t hardcopy_control_channel = 0x0012;
    constexpr uint16_t hardcopy_data_channel = 0x0014;
    constexpr uint16_t hardcopy_notification = 0x0016;
    constexpr uint16_t avctp = 0x0017;
    constexpr uint16_t avdtp = 0x0019;
    constexpr uint16_t cmtp = 0x001B;
    constexpr uint16_t mcap_control_channel = 0x001E;
    constexpr uint16_t mcap_data_channel = 0x001F;
    constexpr uint16_t l2cap = 0x0100;

    constexpr uint16_t service_discovey_server_service_class_id = 0x1000;
    constexpr uint16_t browse_group_descriptor_service_class_id = 0x1001;
    constexpr uint16_t serial_port = 0x1101;
    constexpr uint16_t pnp_information = 0x1200;
};

enum class sdp_self_service_attribute_id : uint16_t
{
    version_number_list = 0x0200,
    service_database_state = 0x0201
};

enum class sdp_attribute_value_type : uint8_t
{
    null = 0x00,
    unsigned_integer = 0x01,
    signed_integer = 0x02,
    uuid = 0x03,
    string = 0x04,
    boolean_type = 0x05,
    data_elements = 0x06,
    alternative_data_element = 0x07,
    url = 0x08,
};

}

