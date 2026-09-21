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

#include "framework/lendable_element.h"
#include "abstract_hci_interface.h"
#include "hci_module.h"
#include "..\include\data_element.h"
#include "..\include\common.h"

#include <list>

namespace bluetooth
{

class hci_module_control_block : public framework::base_element
{

public:

    ~hci_module_control_block() {}

    std::shared_ptr<abstract_hci_interface> m_hci_interface;
    uint8_t m_command_credit = 1;   // commnads credit
    uint32_t m_hci_response_timer = 0;      // Command response timer
    std::list<std::shared_ptr<hci_module::hci_module_task>> m_pending_data_to_chip; // The data need send to chipset.
    std::list<std::shared_ptr<hci_module::hci_module_task>> m_waiting_reponse_cmds; // The commands has sent to chipset adn waiting for response
    init_status m_chip_initial_status = init_status::deinitialized; // chipset initialization status
};

}

