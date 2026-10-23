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
#include "framework/abstract_task.h"
#include "common/bluetooth_common_event.h"

#include "uuid.h"
#include "internal/spp_connection.h"
#include "data_element.h"
#include "bluetooth_address.h"
#include "spp/spp_task.h"

#include <functional>
#include <vector>
#include <memory>

namespace bluetooth
{

class spp_module : public framework::abstract_module
{

public:

    constexpr static const char* s_spp_module_name = "spp_module";

    spp_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_rfcomm_port_registered
        (
        std::u8string a_name,
        uint8_t a_port,
        std::vector<uuid> a_uuids,
        std::function<void( uint8_t )> a_registered_callback
        );

    void handle_connection_request
        (
        bluetooth_address a_address,
        uint8_t a_port
        );

    void handle_connection_changed
        (
        bluetooth_address a_address,
        connection_status a_status,
        uint8_t a_port,
        bool a_local_inited
        );

    void handle_bluetoot_event( std::shared_ptr<bluetooth_common_event> const& a_bt_event );

    void handle_sdu
        (
        std::shared_ptr<hci_data> a_raw,
        uint32_t a_offset,
        uint32_t a_size,
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_local_inited
        );

    void handle_spp_sdp_service_attribute_search_result
        (
        std::shared_ptr<spp_sdp_service_attribute_search_result_task> a_detail_task
        );

    void handle_spp_connection_failed( std::shared_ptr<spp_connection> a_sdp_conn );

    void disconnect_port
        (
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_port_on_local
        );

    void disconnect
        (
        bluetooth_address a_address
        );

    void connect
        (
        bluetooth_address a_address
        );

    void send_port_data
        (
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_local_inited,
        std::shared_ptr<std::vector<uint8_t>> a_spp_data
        );

    /**
     * Create a new spp service. a_registered_callback will invoked with the rfcomm's port
     */
    void create_new_spp
        (
        std::u8string a_name,
        std::vector<uuid> a_uuids,
        std::function<void( uint8_t )> a_registered_callback
        );

    std::shared_ptr<spp_connection> find
        (
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_local_inited
        );

    std::shared_ptr<spp_connection> find
        (
        bluetooth_address a_address,
        std::vector<uuid> a_uuid,
        bool a_local_inited
        );

    uint32_t m_spp_base_record_id = 0x00;
    std::vector<local_spp_service_info> m_local_spp_services;

    std::vector<std::shared_ptr<spp_connection>> m_connections;
};

}

