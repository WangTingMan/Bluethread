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

#include "bluetooth_common_event.h"

namespace bluetooth
{

uint16_t bluetooth_common_event::s_bluetooth_common_event_type = 0x00;

bluetooth_common_event::bluetooth_common_event()
{
    m_event_type = framework::event_type::derived_type;
    m_derived_type = s_bluetooth_common_event_type;
}

}
