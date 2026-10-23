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
#include "../framework/abstract_module.h"
#include "../framework/lendable_element.h"
#include "../framework/abstract_task.h"
#include "data_element.h"

#include "../l2cap/l2cap_common.h"
#include "sdp/sdp_data_element.h"

#include "internal/rfcomm_multiplexer.h"
#include "rfcomm_common.h"
#include "rfcomm_task.h"
#include "uuid.h"

#include <vector>
#include <shared_mutex>
#include <memory>

namespace bluetooth
{

using rfcomm_port_callback = std::function<std::shared_ptr<rfcomm_port_callback_block>( uint8_t, bool )>;

class rfcomm_module : public framework::abstract_module
{

public:

    constexpr static const char* s_rfcomm_module_name = "rfcomm_module";

    rfcomm_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    bool register_callback( rfcomm_port_callback_block a_callback, uint8_t& a_port_used );

    void handle_connect_request( std::shared_ptr<connection_request> const& a_request );

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void handle_config_response( std::shared_ptr<l2cap_config_response> const& a_reponse );

    void handle_connection_state_changed
        (
        bluetooth_address a_address,
        uint16_t a_local_cid,
        uint16_t a_remote_cid,
        l2cap_channel_state_type a_state,
        l2cap_channel_close_reason a_reason
        );

    void handle_sdu( std::shared_ptr<hci_data> );

    void connect_specific_port( std::shared_ptr<rfcomm_task_connect_port> a_task );

    void do_port_callback_operation( std::shared_ptr<rfcomm_task_port_callback_operation> a_task );

    void check_connection_conflict
        (
        std::vector<std::shared_ptr<rfcomm_task_connect_port>> a_pending_tsk,
        bluetooth_address a_remote_addr
        );

    void shutdown_mux_l2cap_if_idle( std::shared_ptr<rfcomm_multipexer> a_mulx );

    std::shared_ptr<rfcomm_multipexer> find_multipexer_by_acl_handle
        (
        uint16_t a_acl_handle
        );

    std::shared_ptr<rfcomm_multipexer> find_multipexer_by_remote( bluetooth_address const& a_address );

    /**
     * @brief Lookup registered RFCOMM port callback block.
     *
     * Search callback registry for matching rfcomm_port_callback_block entry.
     * Composite lookup key: port number, local-port flag and peer bluetooth address.
     *
     * @param a_port RFCOMM server channel(port) number
     * @param a_is_local_port true if this is a local listening port; false for remote peer port
     * @param a_peer_address Remote peer Bluetooth address.
     *        When @p a_is_local_port is true: this parameter is ignored during matching.
     *        When @p a_is_local_port is false: must supply valid peer Bluetooth address for lookup.
     * @return shared_ptr to matched callback block; returns empty shared_ptr if no entry found.
     *
     * @note Matching rule:
     *       - a_is_local_port = true: match only by port number, peer address is not used for comparison.
     *       - a_is_local_port = false: match requires port number AND valid peer address both identical.
     */
    std::shared_ptr<rfcomm_port_callback_block> find_port_callback
        (
        uint8_t a_port,
        bool a_is_local_port,
        bluetooth_address a_peer_address
        );

    uint8_t get_next_available_port_number();

    bool port_register_as_local( uint8_t a_port );

    std::vector<std::shared_ptr<rfcomm_multipexer>> m_working_multipexer;
    std::vector<std::shared_ptr<rfcomm_multipexer>> m_idle_multipexer;
    std::vector<std::shared_ptr<rfcomm_port_callback_block>> m_port_callbacks;
};

}

