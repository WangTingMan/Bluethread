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

#include "rfcomm/rfcomm_task.h"
#include "rfcomm/rfcomm_module.h"

namespace bluetooth
{

uint16_t rfcomm_task::s_rfcomm_task_type_id = 0;

rfcomm_task::rfcomm_task()
{
    set_task_type( framework::task_type( s_rfcomm_task_type_id ) );
    set_target_module( rfcomm_module::s_rfcomm_module_name );
}

}

