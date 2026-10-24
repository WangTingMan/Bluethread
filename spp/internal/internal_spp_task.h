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

#include "spp/spp_task.h"
#include "spp_connection.h"

namespace bluetooth
{

class spp_sdp_service_attribute_search_result_task : public spp_task
{

public:

    spp_sdp_service_attribute_search_result_task();

    sdp_search_result result = sdp_search_result::sdp_search_failed;
    bluetooth_address address;
    std::vector<uuid> uuids;
    std::vector<sdp_data_element> service_record_list;
    std::shared_ptr<spp_connection> spp_conn;
};

}
