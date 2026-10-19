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

/**
* Callback block for RFCOMM port events.
* This structure is intended for use by bluetooth profiles (e.g. SPP, HFP),
* and is NOT exposed directly to user applications.
*/
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
    bool is_local_port = false;

    /**
     * Remote peer bluetooth address.
     * Valid only when @ref is_local_port is false, identifies the remote device
     * where this RFCOMM port resides.
     */
    bluetooth_address remote_address;

    /**
     * which module will handle these callback
     */
    std::string handle_module;

    /**
     * Callback invoked when receiving an incoming connection request from peer.
     * Only valid and will be triggered when @ref is_local_port is true:
     * peer device attempts to connect to this local RFCOMM port,
     * upper layer module decides whether to accept this incoming connection.
     * If @ref is_local_port is false (port resides on remote device),
     * this callback will never be called.
     * Parameter: remote peer bluetooth address.
     */
    std::function<void( bluetooth_address )> connection_request_callback;

    /**
     * Callback for RFCOMM port connection status change events.
     * @param bluetooth_address Remote peer address
     * @param connection_status New connection state of this port
     */
    std::function<void( bluetooth_address, connection_status )> connection_changed_callback;

    /**
     * Received remote user data from remote device.
     * @param shared_ptr<hci_data> Raw hci data buffer
     * @param uint32_t Offset to the start of user data inside raw hci data
     * @param uint32_t Total user data size
     * @param bluetooth_address Peer device sending this user data
     */
    std::function<void( std::shared_ptr<hci_data>, uint32_t, uint32_t, bluetooth_address )> data_callback;
};

}

