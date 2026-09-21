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

#include "../l2cap_task.h"
#include "../l2cap_module.h"

namespace bluetooth
{

    uint16_t l2cap_task::s_l2cap_task_type_id = 0u;

    l2cap_task::l2cap_task()
    {
        m_task_type = static_cast<framework::task_type>(s_l2cap_task_type_id);
        set_target_module( l2cap_module::s_l2cap_module_name );
    }

}

