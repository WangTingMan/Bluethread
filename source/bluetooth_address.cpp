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

#include "bluetooth_address.h"

const bluetooth_address bluetooth_address::s_any_address{ {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };
const bluetooth_address bluetooth_address::s_empty_address{ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };

std::string bluetooth_address::to_string() const
{
    char buffer[100];
    snprintf( buffer, 100, "%02X:%02X:%02X:%02X:%02X:%02X", address[0],
        address[1], address[2], address[3], address[4],
        address[5] );
    return buffer;
}

