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

#include "common.h"

namespace bluetooth
{

std::ostream& operator<<( std::ostream& os, enable_status a_status )
{
    switch( a_status )
    {
    case bluetooth::enable_status::unknown:
        os << "unknown";
        break;
    case bluetooth::enable_status::disabled:
        os << "disabled";
        break;
    case bluetooth::enable_status::enabling:
        os << "enabling";
        break;
    case bluetooth::enable_status::enabled:
        os << "enabled";
        break;
    case bluetooth::enable_status::disabling:
        os << "disabling";
        break;
    default:
        break;
    }
    return os;
}

std::ostream& operator<<( std::ostream& os, connection_status a_status )
{
    switch( a_status )
    {
    case bluetooth::connection_status::disconnected:
        os << "disconnected";
        break;
    case bluetooth::connection_status::connecting:
        os << "connecting";
        break;
    case bluetooth::connection_status::connected:
        os << "connected";
        break;
    case bluetooth::connection_status::disconnecting:
        os << "disconnecting";
        break;
    default:
        break;
    }
    return os;
}

}
