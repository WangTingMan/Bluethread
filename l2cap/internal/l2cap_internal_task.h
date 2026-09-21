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

#include "l2cap/l2cap_task.h"
#include "l2cap/l2cap_module.h"

namespace bluetooth
{

    class l2cap_task_remote_invalid_cid_channel_close : public l2cap_task
    {

    public:

        l2cap_task_remote_invalid_cid_channel_close()
        {
            m_type = l2cap_task_type::remote_invalid_cid_channel_close;
            set_source_module( l2cap_module::s_l2cap_module_name );
        }

        uint16_t    m_local_cid = 0x00;
        uint16_t    m_remote_cid = 0x00;
        uint16_t    m_acl_handle = 0x00;
        acl_type    m_acl_type = acl_type::invalid_type;
    };

    /**
     * @class l2cap_task_clear_pending_packets
     * @brief Task to clear pending unsent packets in channel transmit queue.
     * @details This task is typically triggered when receiving channel disconnection request.
     * It purges packets queued but not yet transmitted for the specified L2CAP channel.
     */
    class l2cap_task_clear_pending_packets : public l2cap_task
    {

    public:

        l2cap_task_clear_pending_packets()
        {
            m_type = l2cap_task_type::clear_pending_packets;
            set_source_module( l2cap_module::s_l2cap_module_name );
        }

        uint16_t    m_local_cid = 0x00;
        uint16_t    m_acl_handle = 0x00;
        acl_type    m_acl_type = acl_type::invalid_type;
    };

    /**
     * @class l2cap_task_remove_channel_from_cache
     */
    class l2cap_task_remove_channel_from_cache : public l2cap_task
    {

    public:

        l2cap_task_remove_channel_from_cache()
        {
            m_type = l2cap_task_type::remove_channel_from_cache;
            set_source_module( l2cap_module::s_l2cap_module_name );
        }

        uint16_t    m_local_cid = 0x00;
        uint16_t    m_acl_handle = 0x00;
        acl_type    m_acl_type = acl_type::invalid_type;
    };
}
