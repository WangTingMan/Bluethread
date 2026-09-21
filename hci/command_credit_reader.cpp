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

#include "command_credit_reader.h"
#include "hci_defs.h"

namespace bluetooth
{

uint8_t command_credit_reader::read_command_credit( std::shared_ptr<hci_data> const& a_hci_data )
{
    std::vector<uint8_t> const& buffer = a_hci_data->m_buffer;
    hci_event_type event_ = static_cast< hci_event_type >( buffer[0] );
    uint8_t command_credit = 0x00;
    switch( event_ )
    {
    case hci_event_type::hci_command_status:
        command_credit = buffer[3];
        break;
    case hci_event_type::hci_command_complete:
        command_credit = buffer[2];
        break;
    default:
        break;
    }
    return command_credit;
}


}

