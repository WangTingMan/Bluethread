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

#include <cstdint>
#include <memory>

namespace bluetooth
{

class sdp_service_record_db
{

public:

    sdp_service_record_db();

    std::shared_ptr<sdp_service_record> add_service_record( uint32_t a_service_record_handle );

    void add_service_record( std::shared_ptr<sdp_service_record> a_service_record );

    std::shared_ptr<sdp_service_record> find_service( uint32_t a_service_record_handle );

    void clear()
    {
        m_local_services.clear();
    }

    std::list<std::shared_ptr<sdp_service_record>> find_matched_uuids_record( std::vector<uuid> const& a_uuids );

private:

    std::list<std::shared_ptr<sdp_service_record>> m_local_services;
};

}

