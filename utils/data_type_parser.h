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

#include <vector>

/*
This part defines the basic data types used for Extended Inquiry Response
(EIR), Advertising Data (AD), Scan Response Data (SRD), Additional
Controller Advertising Data (ACAD), and OOB data blocks.
*/
namespace bluetooth
{

/*
All the definitions, ref to https://www.bluetooth.com/specifications/assigned-numbers/
*/
enum class data_type : uint8_t
{
    invlaid = 0x00,
    flags = 0x01,
    incompleted_16bit_uuid_list = 0x02,
    completed_16bit_uuid_list = 0x03,
    incompleted_32bit_uuid_list = 0x04,
    completed_32bit_uuid_list = 0x05,
    incompleted_128bit_uuid_list = 0x06,
    completed_128bit_uuid_list = 0x07,
    shorted_local_name = 0x08,
    completed_local_name = 0x09,
    device_id = 0x10,
    tx_power_level = 0x0A,
    class_of_device = 0x0B,
};

struct data_element_parsed
{
    data_type type = data_type::invlaid;
    uint8_t size = 0;
    uint8_t* buffer = nullptr;
};

std::vector<data_element_parsed> parse_eir( uint8_t* a_buffer, uint16_t a_size );

}

