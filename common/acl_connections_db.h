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
#include "../framework/abstract_info.h"
#include "../l2cap/l2cap_common.h"

#include "bluetooth_address.h"

#include <shared_mutex>
#include <vector>
#include <tuple>

namespace bluetooth
{

struct acl_connection
{
    bluetooth_address m_remote_device;
    bool m_local_inited = false;
    uint16_t m_connection_handle = 0x00;
    bool m_encrypted = false;
    uint16_t m_supervision_timeout = 0x00;
    acl_type m_acl_type = acl_type::br_edr_acl;
};

class acl_connections_db : public framework::abstract_information
{

public:

    constexpr static const char* s_acl_connections_db_name = "acl_connections_db_name";

    acl_connections_db();

    bool edr_acl_connected( bluetooth_address const& a_address );

    std::tuple<uint16_t,bool> get_handle( bluetooth_address const& a_address );

    std::tuple<bluetooth_address, bool> get_address( uint16_t a_handle );

    std::tuple<acl_type, bool> get_type( uint16_t a_handle );

    void add_acl_connection( acl_connection a_acl );

    void remove_connection( uint16_t a_handle );

    /**
     * Update the acl connection, add new one if not exist
     */
    void update_acl
        (
        bluetooth_address a_remote_device,
        uint16_t a_handle,
        acl_type a_type,
        bool a_encypted
        );

    void update_acl
        (
        uint16_t a_handle,
        uint16_t a_timeout
        );

private:

    std::shared_mutex m_mutex;
    std::vector<std::shared_ptr<acl_connection>> m_acls;
};

}
