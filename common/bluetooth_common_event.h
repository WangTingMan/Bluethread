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

#include "../framework/framework_event.h"
#include "bluetooth_address.h"

namespace bluetooth
{

enum class bluetooth_event_type : uint8_t
{
    invalid_type = 0x00,
    edr_acl_connected = 0x01, // m_remote_device indicated which device connected
    pairing_completed = 0x02, // m_remote_device indicated which device paired
    edr_acl_disconnected = 0x03, // indicate that the ACL connection lost( or may connection failed )
};

class bluetooth_common_event : public framework::framework_event
{

public:

    bluetooth_common_event();

    std::shared_ptr<framework::abstract_task> clone()const override
    {
        std::shared_ptr<bluetooth_common_event> task = std::make_shared<bluetooth_common_event>();
        copy_to( task );
        return task;
    }

    void copy_to( std::shared_ptr<bluetooth_common_event> a_tsk )const
    {
        a_tsk->m_bluetooth_event_type = m_bluetooth_event_type;
        a_tsk->m_remote_device = m_remote_device;
        a_tsk->m_acl_disconnected_code = m_acl_disconnected_code;
        framework_event::copy_to( a_tsk );
    }

    static void register_task_type()
    {
        s_bluetooth_common_event_type = framework_event::register_task_type();
    }

    bluetooth_event_type m_bluetooth_event_type = bluetooth_event_type::invalid_type;
    bluetooth_address    m_remote_device;
    uint8_t              m_acl_disconnected_code = 0;
    static uint16_t      s_bluetooth_common_event_type;
};

}

