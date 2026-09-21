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
#include "..\common\protocol_headers.h"

#include "..\l2cap_common.h"

namespace bluetooth
{

class signaling_header : public l2cap_header
{

public:

    void set_sdu_length( uint16_t a_length )override;

    void to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )override;

    uint16_t header_size()const override;

    void set_signaling_code( signaling_code a_code )
    {
        m_signaling_code = a_code;
    }

    void set_identifier( uint8_t a_identifier )
    {
        m_identifer = a_identifier;
    }

private:

    uint16_t m_length_in_signaling = 0x0000;
    signaling_code m_signaling_code = signaling_code::l2cap_command_reject_rsp;
    uint8_t m_identifer = 0x00;

};

}

