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
#include "framework/abstract_module.h"
#include "framework/lendable_element.h"
#include "data_element.h"
#include "l2cap_common.h"
#include "l2cap_task.h"

#include <map>

namespace bluetooth
{

class acl_manager;

class l2cap_module : public framework::abstract_module
{

public:

    constexpr static const char* s_l2cap_module_name = "l2cap_module";

    l2cap_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

    /**
     * API: To register callback.
     */
    void register_callback
        (
        uint16_t a_psm,
        l2cap_callbacks a_callbacks
        );

    /**
     * API: To deregister callback.
     */
    void deregister_callback
        (
        uint16_t a_psm
        );

private:

    void handle_data_from_chipset( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_hci_event( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    void handle_acl_packet( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    std::shared_ptr<hci_data> handle_connection_request( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    void handle_connection_completed( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    void handle_disconnection_completed( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    void handle_number_of_completed_packets( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    void handle_supervision_changed( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk );

    void handle_accept_channel_connection_req( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_reject_channel_connection_req( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_accept_config_req( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_config_local_channel( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_send_upper_sdu( std::shared_ptr<l2cap_task> const& a_tsk );

    /**
     * Handle the channel connection request from upper layer
     */
    void handle_request_connection_host( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_register_or_deregister_psm( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_buffer_size_read_done();

    void handle_send_upper_sdu_with_remote_address( std::shared_ptr<l2cap_task> const& a_tsk );

    void handle_remote_invalid_cid_channel_close( std::shared_ptr<l2cap_task> const& a_tsk );

    /**
     * API: To register callback.
     */
    void register_callback_internal
        (
        uint16_t a_psm,
        l2cap_callbacks a_callbacks
        );

    /**
     * API: To deregister callback.
     */
    void deregister_callback_internal
        (
        uint16_t a_psm
        );

    std::shared_ptr<acl_manager> m_connection_manager;
};


}

