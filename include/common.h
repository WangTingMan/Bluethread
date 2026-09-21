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
#include <iostream>

namespace bluetooth
{

enum class init_status : uint8_t
{
    deinitialized = 0,
    initializing = 1,
    initialized = 2,
    deinitalizing = 3
};

enum class connection_status : uint8_t
{
    disconnected = 0x00,
    connecting = 0x01,
    connected = 0x02,
    disconnecting = 0x03
};

enum class enable_status : uint8_t
{
    unknown,
    disabled,
    enabling,
    enabled,
    disabling
};

enum class service_type : uint8_t
{
    not_specified = 0x01,
    serial_port_profile = 0x02
};

enum class function_type : uint16_t
{
    /* service_type not specified*/
    bluetooth_enabled,
    bluetooth_disabled,
    searching,
    searching_completed,
    visibility_changed, // a_paras type is visibility
    remote_device_found, // a_paras type is remote_device_info
    pairable_changed, // a_paras type is bool to indicate whether pairable
    ss_pairing_confirm_request,
    paired_device_info, // notify paired device's info, a_paras type is remote_device_info

    /* serial_port_profile */
    serial_connection_status_changed, // a_paras is serial_port_connection_status
    serial_connection_request, /* remote device request connect to local spp port
                               * a_paras is serial_port_connection_request
                               */
    serial_port_data_received, /** new serial port data received from remote device
                                *  a_paras is serial_port_data_received_info
                                */
};

struct visibility
{
    enable_status discoverable;
    enable_status connectable;
};

enum class remote_device_attribute_type
{
    completed_device_name, // utf8 string
    shorted_device_name, // utf8 string
    device_address, // 6 bytes to indicate device's address
    inquiry_rssi, // signed 1 byte to indicate rssi
    device_cod, // three bytes to indicate class of device
};

struct remote_device_attribute
{
    remote_device_attribute_type type;
    void* buffer = nullptr;
    uint16_t size = 0;
};

struct ssp_pairing_confirm_request_data
{
    void* address;
    void* remote_device_name;
    uint32_t name_size;
    uint32_t confirm_number;
};

struct remote_device_info
{
    remote_device_attribute* attributes = nullptr;
    uint16_t attribute_count = 0;
};

struct serial_port_register_parameters
{
    char const* service_name = nullptr; // service name, encoded in utf-8
    char* uuids = nullptr;  // registered uuid. To use spp uuid, then this pointer canbe null
    uint8_t uuid_count = 0; // the uuid count
};

struct serial_port_connection_status
{
    void* address;
    uint8_t port_number;
    bool port_on_local;
    connection_status status;
    char* uuids = nullptr;
    uint8_t uuid_count = 0;
};

struct serial_port_connection_request
{
    void* address;
    uint8_t port_number;
};

struct serial_port_data_received_info
{
    void* address;
    uint8_t port_number;
    bool port_on_local;
    void* p_data;
    uint16_t data_size;
};

std::ostream& operator<<( std::ostream& os, enable_status a_status );

std::ostream& operator<<( std::ostream& os, connection_status a_status );

}

