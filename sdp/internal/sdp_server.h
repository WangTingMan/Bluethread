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
#include "sdp_service_record_db.h"
#include "sdp_protocol.h"

#include <functional>

namespace bluetooth
{

class sdp_server
{

public:

    sdp_server();

    void init_db();

    void handle_service_search_attribute_request
        (
        std::shared_ptr<sdp_service_search_attribute_req> const& a_request
        );

    void set_send_packet_fun( sdp_packet_send_type a_fun )
    {
        m_packet_send = a_fun;
    }

    uint32_t register_record( std::shared_ptr<sdp_service_record> a_record );

    void clear()
    {
        m_service_record_db.clear();
        m_next_record_id = 0x00;
    }

private:

    uint32_t m_next_record_id = 0x00;
    sdp_service_record_db m_service_record_db;
    sdp_packet_send_type m_packet_send;
};

}
