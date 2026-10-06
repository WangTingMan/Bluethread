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
#include "../sdp_common.h"
#include "sdp/sdp_service_record.h"

namespace bluetooth
{

namespace did_attribute_id
{
    constexpr uint16_t specification_id = 0x0200;
    constexpr uint16_t vendor_id = 0x0201;
    constexpr uint16_t product_id = 0x0202;
    constexpr uint16_t version = 0x0203;
    constexpr uint16_t primary_record = 0x0204;
    constexpr uint16_t vendor_id_source = 0x0205;
}

class did_service_record : public sdp_service_record
{

public:

    did_service_record();


};

}

