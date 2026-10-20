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

#include "spp_connection.h"
#include "framework/log_util.h"

namespace
{
    static uint16_t s_next_spp_connection_id = 0;
}

namespace bluetooth
{

spp_connection::spp_connection()
    : m_spp_connection_id( s_next_spp_connection_id++ )
{
}

void spp_connection::set_connection_status( connection_status a_status )
{
    LogUtilInfo() << "spp port id = " << m_spp_connection_id << ", address " << m_remote_device.to_string()
        << ", port " << static_cast< uint32_t >( m_port ) << ", local inited: " << std::boolalpha
        << m_local_inited << ", status changed from " << m_connection_status << " to " << a_status;
    m_connection_status = a_status;
}

}

