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

#include "hci_defs.h"

#include <memory>
#include <cstdint>
#include <vector>

namespace bluetooth
{

constexpr uint16_t s_l2cap_offset = 4;
constexpr uint16_t s_l2cap_signaling_offset = s_l2cap_offset + 4;

enum class element_type : uint8_t
{
    uart_type_hci_data = 0,

};

enum class uart_hci_type : uint8_t
{
    invalid_type = 0x00,
    command_type = 0x01,
    acl_type = 0x02,
    sco_type = 0x03,
    event_type = 0x04,
    iso_data_type = 0x05
};

struct hci_data
{
    uart_hci_type m_type = uart_hci_type::invalid_type;
    bool m_from_controller = true;
    std::vector<uint8_t> m_buffer;
};

hci_command get_command_from_hci( std::vector<uint8_t> const& a_hci );

hci_command get_command_from_hci_response( std::vector<uint8_t> const& a_hci );

/**
 * Try to get acl handle from the HCI packet.
 * Will return the acl handle in a_acl_handle only when |a_hci| is an ACL packet.
 */
bool get_acl_handle_from_hci
    (
    std::shared_ptr<hci_data> const& a_hci,
    uint16_t& a_acl_handle
    );

}

