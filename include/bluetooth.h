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
#include "global_config.h"
#include "bluetooth_address.h"
#include "common.h"

#include <string>
#include <functional>

typedef void ( *stack_callback )( bluetooth::service_type a_type, bluetooth::function_type a_function, void* a_paras );

enum
#ifdef _cplusplus
    class
#endif
    profile_type
#ifdef _cplusplus
    : uint8_t
#endif
{
    serial_port_profile = 0x01,
};

struct BLUETOOTH_EXPORT bluetooth_interface
{

    std::uint32_t size = sizeof( bluetooth_interface );

    void (*enable)();

    void (*disable)();

    void (*set_local_name)( std::u8string a_name );

    std::u8string ( *get_local_name )();

    void ( *set_visiblility )( bool a_discoverable, bool a_connectable );

    void ( *set_dicoverable )( bool a_discoverable );

    void ( *set_connectable )( bool a_connectable );

    void ( *set_pairable )( bool a_pairable );

    void ( *get_visiblility )( bool* a_discoverable, bool* a_connectable, bool* a_pairable );

    void ( *search_edr_device )();

    void ( *cancel_search_edr_device )( );

    void ( *accept_ssp_confirm )( bluetooth_address a_address, bool a_accept );

    void ( *set_callback )( stack_callback a_callback );

    /** Get Bluetooth profile interface */
    void* ( *get_profile_interface )( profile_type a_profile );
};

struct BLUETOOTH_EXPORT serial_port_interface
{
    /**
     * Register( listen ) a new SPP port in local spp service. Then remote device canbe connect to the new
     * port. return 0 if register failed otherwise the registered port in rfcomm
     */
    uint8_t (*register_service)( bluetooth::serial_port_register_parameters parameter );

    /**
     * Connect to specified SPP uuid service
     */
    void (*connect_uuid)( bluetooth_address a_address, char* uuids, uint8_t uuid_count );

    /**
     * Connect to specifed SPP port with specified remote device
     */
    void (*connect_port)( bluetooth_address a_address, uint8_t port );

    /**
     * Connect to the default SPP uuid port
     */
    void (*connect)( bluetooth_address a_address );

    void (*accept_coming_connection)( bluetooth_address a_address, uint8_t port );

    void (*reject_coming_connection)( bluetooth_address a_address, uint8_t port );

    /**
     * Disconnect a connected port.
     * a_address indicate which device will be disconnected
     * a_port indicate which port use will be disconnected
     * a_port_on_local indicate the port is on local device( true ) or remote device( false )
     */
    void ( *disconnect_port )( bluetooth_address a_address, uint8_t a_port, bool a_port_on_local );

    /**
     * Send port data to remote device
     */
    void ( *send )( bluetooth_address a_address, uint8_t a_port, bool a_port_on_local, uint8_t* a_buffer, uint32_t a_size );

    /**
     * Disconnect all connected port with specified remote device
     */
    void ( *disconnect )( bluetooth_address a_address );
};

BLUETOOTH_EXPORT bluetooth_interface* get_bt_interface();
