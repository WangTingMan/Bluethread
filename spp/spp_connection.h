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
#include "uuid.h"
#include "bluetooth_address.h"
#include "common.h"

#include <cstdint>
#include <vector>

namespace bluetooth
{

struct local_spp_service_info
{
    std::vector<uuid> uuids;
    uint8_t port_number;
    uint32_t sdp_service_recode_handle;
    std::u8string service_name;
};

class spp_connection
{

public:

    spp_connection();

    void set_uuid( std::vector<uuid> a_uuid )
    {
        m_uuid = a_uuid;
    }

    std::vector<uuid>const get_uuid()const
    {
        return m_uuid;
    }

    void set_remote_device( bluetooth_address a_address )
    {
        m_remote_device = a_address;
    }

    bluetooth_address const& get_remote_device()const
    {
        return m_remote_device;
    }

    void set_port( uint8_t a_port )
    {
        m_port = a_port;
    }

    uint8_t get_port()const
    {
        return m_port;
    }

    void set_name( std::u8string a_name )
    {
        m_name = a_name;
    }

    std::u8string const& get_name()const
    {
        return m_name;
    }

    void set_sdp_record_id( uint32_t a_id )
    {
        m_sdp_record_id = a_id;
    }

    uint32_t get_sdp_record_id()const
    {
        return m_sdp_record_id;
    }

    bool local_inited()const
    {
        return m_local_inited;
    }

    void set_local_inited( bool a_local_inited )
    {
        m_local_inited = a_local_inited;
    }

    void set_connection_status( connection_status a_status );

    connection_status get_connection_status()
    {
        return m_connection_status;
    }

private:

    bool m_local_inited = false;
    std::vector<uuid> m_uuid;
    bluetooth_address m_remote_device;
    uint8_t m_port = 0x00;
    std::u8string m_name;
    uint32_t m_sdp_record_id = 0x00;
    connection_status m_connection_status = connection_status::disconnected;
};

}

