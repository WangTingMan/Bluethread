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

#include "sdp_protocol.h"
#include "sdp_server.h"
#include "sdp_connection.h"
#include "sdp/sdp_task.h"

#include <l2cap/l2cap_common.h>

namespace bluetooth
{

class sdp_manager
{

    friend class sdp_connection;

public:

    void init();

    void init_db()
    {
        m_local_service.init_db();
    }

    void clear_db()
    {
        m_local_service.clear();
    }

    void handle_sdp_connect_request( std::shared_ptr<connection_request> const& a_request );

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void config_local_channel
        (
        uint16_t a_acl_handle,
        uint16_t a_remote_cid
        );

    void handle_connection_state_changed
        (
        bluetooth_address a_address,
        uint16_t a_local_cid,
        uint16_t a_remote_cid,
        l2cap_channel_state_type a_state,
        l2cap_channel_close_reason a_reason
        );

    void handle_sdu( std::shared_ptr<hci_data> );

    void register_record( std::shared_ptr<sdp_task> const& a_task );

    void service_search( std::shared_ptr<sdp_task> const& a_task );

    /**
     * Handle the service search attribute request from upper layer
     */
    void handle_service_search_attribute_host( std::shared_ptr<sdp_task> const& a_task );

    void send_packet
        (
        std::shared_ptr<sdp_protocol_base> const& a_packet,
        bluetooth_address                           a_remote_address
        );

    void send_error_rsp( uint16_t a_acl_handle, sdp_error_code a_code );

    std::shared_ptr<sdp_connection> find_connection( bluetooth_address const& a_address );

    std::shared_ptr<sdp_connection> find_connection
        (
        uint16_t a_acl_handle
        );

    void remove_connection( bluetooth_address const& a_address );

    void handle_connection_status_monitor_timeout( sdp_connection* a_connection );

private:

    bool verify_received_packet
        (
        sdp_header& a_sdp_header,
        std::shared_ptr<hci_data> const& a_packet
        );

    std::shared_ptr<sdp_error_response> parse_error_rsp
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    std::shared_ptr<sdp_service_search_request> parse_service_search_request
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    std::shared_ptr<sdp_service_search_response> parse_service_search_response
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    std::shared_ptr<sdp_service_attribute_request> parse_service_attribute_request
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    std::shared_ptr<sdp_service_attribute_response> parse_service_attribute_response
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    std::shared_ptr<sdp_service_search_attribute_req> parse_service_search_attribute_request
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    std::shared_ptr<sdp_service_search_attribute_response> parse_service_search_attribute_response
        (
        sdp_header& a_sdp_header,
        uint8_t* a_parameter_buffer,
        uint16_t a_parameter_size
        );

    sdp_server m_local_service;
    std::vector<std::shared_ptr<sdp_connection>> m_connections;
    std::vector<std::shared_ptr<sdp_protocol_base>> m_pending_reqs;

};

}
