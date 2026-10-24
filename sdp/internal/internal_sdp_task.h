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

#include "sdp/sdp_task.h"
#include "sdp_connection.h"

namespace bluetooth
{

class sdp_task_resume_pending_sdp_tasks : public sdp_task
{

public:

    sdp_task_resume_pending_sdp_tasks()
    {
        m_type = sdp_task_type::resume_pending_sdp_tasks;
    }

    bluetooth_address m_remote_address;
    std::vector<std::shared_ptr<pending_request>> m_pending_tasks;
};

}

