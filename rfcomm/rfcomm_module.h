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

#include "internal/rfcomm_multiplexer.h"
#include "rfcomm_common.h"

#include <vector>
#include <shared_mutex>
#include <memory>

namespace bluetooth
{

using rfcomm_port_callback = std::function<std::shared_ptr<rfcomm_port_callback_block>( uint8_t, bool )>;

enum class rfcomm_task_type : uint8_t
{
    invalid_type = 0x00,
    accept_coming_connection_request = 0x01, /** accept coming connection request. parameter address indicate the
                                             * remote device' address, and port indicate which port to accept
                                             */
    reject_coming_connection_request = 0x02, /** accept coming connection request. parameter address indicate the
                                             * remote device' address, and port indicate which port to accept
                                             */
    disconnect_specified_port = 0x03, /** disconnect specified device's connected specified port
                                        * address, port and port_on_local are valid
                                        */
    async_send_spp_data = 0x04, /**
                                * Send spp port data to remote device. address, port, port_on_local and
                                * spp_data are valid
                                */
};

class rfcomm_module : public framework::abstract_module
{

public:

    constexpr static const char* s_rfcomm_module_name = "rfcomm_module";

    class rfcomm_task : public framework::abstract_task
    {

    public:

        rfcomm_task_type m_type = rfcomm_task_type::invalid_type;

        bluetooth_address address;
        uint8_t port;
        bool port_on_local;
        std::shared_ptr<std::vector<uint8_t>> spp_data;
    };

    rfcomm_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

    /**
     * Other module may call this method directly with cross thread
     */
    void register_callback( rfcomm_port_callback_block a_callback );

private:

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

    std::shared_ptr<rfcomm_multipexer> find_multipexer_by_local_cid( uint16_t a_local_cid );

    std::shared_ptr<rfcomm_multipexer> find_multipexer_by_remote( bluetooth_address const& a_address );

    std::shared_ptr<rfcomm_port_callback_block> find_port_callback( uint8_t a_port, bool a_local_inited );

    std::vector<std::shared_ptr<rfcomm_multipexer>> m_working_multipexer;
    std::vector<std::shared_ptr<rfcomm_multipexer>> m_idle_multipexer;

    std::shared_mutex m_mutex;
    std::vector<std::shared_ptr<rfcomm_port_callback_block>> m_port_callbacks;
};

}

