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
#include "bluetooth_address.h"
#include "data_element.h"
#include "l2cap_signaling.h"
#include "acl_statemachine.h"
#include "l2cap_internal_task.h"
#include "../l2cap_common.h"
#include "l2cap_channel_statemachine.h"
#include "l2cap_packet_recombinationer.h"
#include "common/controller.h"

#include <vector>
#include <tuple>

namespace bluetooth
{

class acl_manager
{

public:

    acl_manager();

    void add_new_connection
        (
        bluetooth_address   a_remote_device,
        acl_type            a_type,
        bool                a_local_inited
        );

    void remove_connection( uint16_t a_handle );

    void remove_pending_connection_request( bluetooth_address a_remote_device );

    void update_new_connection
        (
        bluetooth_address a_remote_device,
        uint16_t a_handle,
        acl_type a_type,
        bool a_encypted
        );

    void update_supervision
        (
        uint16_t a_handle,
        uint16_t a_timeout
        );

    std::tuple<acl_type, bool> get_type( uint16_t a_handle );

    void handle_coming_acl_packet( std::shared_ptr<hci_data> a_hci_data );

    /**
     * API: To register callback.
     */
    void register_callback
        (
        uint16_t a_psm,
        l2cap_callbacks a_callbacks
        )
    {
        m_callbacks[a_psm] = a_callbacks;
    }

    /**
     * API: To register callback.
     */
    void deregister_callback
        (
        uint16_t a_psm
        )
    {
        m_callbacks.erase( a_psm );
    }

    l2cap_callbacks get_registered_callback( uint16_t a_psm )
    {
        l2cap_callbacks cb;
        if( m_callbacks.contains( a_psm ) )
        {
            cb = m_callbacks[a_psm];
        }
        return cb;
    }

    /**
     * Accept coming l2cap channel connection request
     */
    void accept_connection_req( std::shared_ptr<connection_request> const& a_request );

    /**
     * Reject coming l2cap channel connection request
     */
    void reject_connection_req
        (
        std::shared_ptr<connection_request> const& a_request,
        connection_req_result a_reason
        );

    void accept_config_req( std::shared_ptr<l2cap_config_request> const& a_request );

    /**
     * Upper layer request to config local channel.
     */
    void config_local_channel_req( std::shared_ptr<l2cap_config_local_channel_request> const& a_request );

    void send_upper_sdu
        (
        std::shared_ptr<l2cap_task_send_l2cap_sdu> const& a_tsk
        );

    void handle_acl_completed_changed( std::vector<std::pair<uint16_t, uint16_t>> );

    /**
     * Handle the channel connection request from upper layer
     */
    void handle_request_connection_host
        (
        bluetooth_address a_remote_device,
        uint16_t a_psm
        );

    bool retrieve_controller_info();

    void handle_buffer_size_read_done();

    void schedule_outgoing_packet
        (
        std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address> a_completed_l2cap_pkt
        );

    void close_channel_with_invalid_cid( std::shared_ptr<l2cap_task_remote_invalid_cid_channel_close> const& a_tsk );

    void clear_pending_packets( std::shared_ptr<l2cap_task> a_tsk );

    void disconnect_channel( std::shared_ptr<l2cap_task> a_tsk );

    void remove_channel_from_cache( std::shared_ptr<l2cap_task> a_tsk );

private:

    bool l2cap_size_check( std::shared_ptr<hci_data> const& a_hci_data );

    void initialize_channel_machine( std::shared_ptr<l2cap_channel_statemachine> const& a_channel );

    bool query_upper_layer_callbacks(uint16_t a_psm, l2cap_callbacks& a_callbacks)
    {
        for (auto& ele : m_callbacks)
        {
            if (ele.first == a_psm)
            {
                a_callbacks = ele.second;
                return true;
            }
        }
        return false;
    }

    std::vector<channel_connection_request> retrieve_all_pending_connection_request(bluetooth_address a_remote_device);

    void send_next_outgoing_packet();

    void handle_completed_coming_acl_packet( std::shared_ptr<hci_data> const& a_hci_data );

    /**
     * First-level cache of outgoing L2CAP channel connection requests
     * initiated by the local upper layer, which cannot be sent yet because
     * no ACL connection to the remote device exists. When the ACL link is
     * established, the corresponding acl_statemachine takes over all
     * requests matching that remote address in one shot (via
     * retrieve_all_pending_outgoing_connection_requests) and processes
     * them; this container is then relieved of those requests.
     */
    std::vector<channel_connection_request> m_pending_outgoing_connection_requests;

    std::vector<std::shared_ptr<acl_statemachine>> m_acl_state_machines; // one acl state machine is one ACL connection.
    std::vector<std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address>> m_outgoing_l2cap_pkts; // to store the outgoing l2cap packets which are waiting for sending.
    std::map<uint16_t, l2cap_callbacks> m_callbacks;    // to store upper layers registered callbacks. the key is PSM value.
    uint16_t m_br_edr_acl_credit = 1;      // ACL credit
    std::shared_ptr<controller> m_controller = nullptr;
    l2cap_packet_recombinationer m_packet_recombinationer;
};

}

