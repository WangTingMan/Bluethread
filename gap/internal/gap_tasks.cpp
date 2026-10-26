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
#include "gap/gap_tasks.h"
#include "gap/gap_module.h"


namespace bluetooth
{

uint16_t gap_module_task::s_gap_module_task_type_id = 0;

gap_module_task::gap_module_task()
{
    set_target_module( gap_module::s_gap_module_name );
    m_task_type = static_cast<framework::task_type>( s_gap_module_task_type_id );
}

}

