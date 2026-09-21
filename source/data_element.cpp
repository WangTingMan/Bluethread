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

#include "data_element.h"
#include "endian_convert.h"

namespace bluetooth
{

hci_command get_command_from_hci( std::vector<uint8_t> const& a_hci )
{
    const uint8_t* le_command = a_hci.data();
    return static_cast< hci_command >( le_to_host16( le_command ) );
}

hci_command get_command_from_hci_response( std::vector<uint8_t> const& a_hci )
{
    hci_command cmd;
    hci_event_type event_type = static_cast< hci_event_type >( a_hci[0] );
    switch( event_type )
    {
    case bluetooth::hci_event_type::hci_command_complete:
        cmd = static_cast< hci_command >( le_to_host16( a_hci.data() + 3 ) );
        break;
    case bluetooth::hci_event_type::hci_command_status:
        cmd = static_cast<hci_command>( le_to_host16( a_hci.data() + 4 ) );
        break;
    default:
        break;
    }
    return cmd;
}

bool get_acl_handle_from_hci
    (
    std::shared_ptr<hci_data> const& a_hci,
    uint16_t& a_acl_handle
    )
{
    if (a_hci->m_type == uart_hci_type::acl_type)
    {
        std::vector<uint8_t> const& buffer = a_hci->m_buffer;
        uint8_t handle[2];
        handle[0] = buffer[0];
        handle[1] = buffer[1] & 0x0F;
        a_acl_handle = le_to_host16(handle);
        return true;
    }

    return false;
}

}

