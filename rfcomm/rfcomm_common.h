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

#include <functional>
#include <string>
#include <cstdint>

#include "bluetooth_address.h"
#include "common.h"
#include "data_element.h"

namespace bluetooth
{

struct rfcomm_port_callback_block
{
    /**
     * Which port number to registered
     */
    uint8_t port_number = 0x00;

    /**
     * True then the port is on local device; otherwise the port is on remote
     * device
     */
    bool local_inited = false;

    /**
     * which module will handle these callback
     */
    std::string handle_module;

    /**
     * received a connection request from remote device
     */
    std::function<void( bluetooth_address )> connection_request_callback;

    /**
     * the connection status changed callback
     */
    std::function<void( bluetooth_address, connection_status )> connection_changed_callback;

    /**
     * Received remote user data from remote device.
     * the first parameter is the raw hci data; the second parameter is the offset to the
     * raw hci data; and the third parameter is the total uaer data size; the forth parameter
     * is the device who sends the user data
     */
    std::function<void( std::shared_ptr<hci_data>, uint32_t, uint32_t, bluetooth_address )> data_callback;
};

}

