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
#include "../framework/abstract_task.h"

#include "../l2cap/l2cap_common.h"

#include "data_element.h"
#include "sdp_common.h"
#include "sdp_service_record_db.h"
#include "sdp_protocol.h"
#include "sdp_server.h"
#include "sdp_connection.h"

#include <functional>

namespace bluetooth
{

class sdp_module : public framework::abstract_module
{

public:

    enum class sdp_task_type : uint8_t
    {
        invalid_type = 0x00,
        register_service_record = 0x01, // Request SDP module register a new service record into service
                                        // record database. m_service_record will be registered and then
                                        // after registered, sdp module will invoke m_registered_callback
                                        // to notify registering finished with record handle. The callback
                                        // will scheduled to m_callback_handle_module
        service_search_request = 0x02, // execute service search transaction.
                                       // valid members: m_remote_device and m_service_uuid
        service_search_attribute = 0x03, // execute service search attribute request
                                         // valid members: m_remote_device, m_service_uuid, m_requested_id_ranges
                                         // and m_attribute_id_list.
                                         // request all attributes if m_attribute_id_list and m_requested_id_ranges is empty
    };

    class sdp_task : public framework::abstract_task
    {

    public:

        sdp_task_type m_type = sdp_task_type::invalid_type;

        sdp_task()
        {
            set_target_module( s_sdp_module_name );
            m_task_type = static_cast<framework::task_type>(s_sdp_task_type_id);
        }

        std::shared_ptr<sdp_service_record> m_service_record;
        std::function<void( uint32_t )> m_registered_callback;
        std::string m_callback_handle_module;

        bluetooth_address m_remote_device;
        std::vector<uuid> m_service_uuid;
        std::vector<uint16_t> m_attribute_id_list;
        std::vector<std::pair<uint16_t, uint16_t>> m_requested_id_ranges;

        static uint16_t s_sdp_task_type_id;
    };

    constexpr static const char* s_sdp_module_name = "sdp_module";

    sdp_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_sdp_connect_request( std::shared_ptr<connection_request> const& a_request );

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void config_local_channel
        (
        uint16_t a_acl_handle,
        uint16_t a_remote_cid
        );

    void handle_connection_state
        (
        bluetooth_address a_address,
        uint16_t a_local_cid,
        uint16_t a_remote_cid,
        l2cap_channel_state_type a_state,
        l2cap_channel_close_reason a_reason
        );

    void handle_sdu( std::shared_ptr<hci_data> );

    void handle_service_search_request( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_service_search_attribute_request( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_register_record( std::shared_ptr<sdp_task> const& a_task );

    void handle_service_search( std::shared_ptr<sdp_task> const& a_task );

    /**
     * Handle the service search attribute request from upper layer
     */
    void handle_service_search_attribute_host( std::shared_ptr<sdp_task> const& a_task );

    void send_packet
        (
        std::shared_ptr<sdp_protocol_base> const&   a_packet,
        bluetooth_address                           a_remote_address
        );

    void send_error_rsp( sdp_error_code a_code );

    bool verify_received_packer( std::shared_ptr<hci_data> const& a_packet );

    std::shared_ptr<sdp_connection> find_connection( bluetooth_address const& a_address );

    void remove_connection( bluetooth_address const& a_address );

    sdp_server m_local_service;
    sdp_header m_sdp_header;
    std::vector<std::shared_ptr<sdp_connection>> m_connections;
    std::vector< std::shared_ptr<sdp_protocol_base>> m_pending_reqs;
};

}
