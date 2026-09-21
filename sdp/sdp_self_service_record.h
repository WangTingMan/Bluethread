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
#include "sdp_common.h"

namespace bluetooth
{

class sdp_self_service_record : public sdp_service_record
{

public:

    sdp_self_service_record();

    void set_version_number_list( std::vector<uint16_t> const& a_versions );

    void set_service_database_state( uint32_t a_state );

private:

    uint32_t m_current_state = 0x01;

};

}

