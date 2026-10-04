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
#include "common.h"

#include "sdp_protocol.h"

namespace bluetooth
{

/**
 * SDP l2cap connection control block.
 * Only one SDP connection for specified remote device
 */
class sdp_connection
{

public:

    /**
     * Set the connection status.
     * return true if the status has been changed
     */
    bool set_connection_status( connection_status a_connection_status )
    {
        bool ret = ( m_connection_status != a_connection_status );
        m_connection_status = a_connection_status;
        return ret;
    }

    connection_status const& get_connection_status()const
    {
        return m_connection_status;
    }

    void set_config_local_req_sent( bool a_send )
    {
        m_config_local_req_sent = a_send;
    }

    bool get_config_local_req_sent()const
    {
        return m_config_local_req_sent;
    }

    void set_config_local_rsp_received( bool a_received )
    {
        m_config_local_rsp_received = a_received;
    }

    bool get_config_local_rsp_received()const
    {
        return m_config_local_rsp_received;
    }

    void set_config_remote_req_received( bool a_received )
    {
        m_config_remote_req_received = a_received;
    }

    bool get_config_remote_req_received()const
    {
        return m_config_remote_req_received;
    }

    void set_config_remote_rsp_sent( bool a_send )
    {
        m_config_remote_rsp_sent = a_send;
    }

    bool get_config_remote_rsp_sent()const
    {
        return m_config_remote_rsp_sent;
    }

    void set_acl_handle
        (
        uint16_t a_acl_handle
        )
    {
        m_acl_handle = a_acl_handle;
    }

    uint16_t get_acl_handle()const
    {
        return m_acl_handle;
    }

    void set_local_cid
        (
        uint16_t a_local_cid
        )
    {
        m_local_cid = a_local_cid;
    }

    void set_remote_cid
        (
        uint16_t a_remote_cid
        )
    {
        m_remote_cid = a_remote_cid;
    }

    bool match
        (
        uint16_t a_acl_handle
        ) const
    {
        return ( m_acl_handle == a_acl_handle );
    }

public:

    void handle_error_rsp
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_error_rsp> const& a_error_rsp
        );

public:

    bluetooth_address m_address;
    uint16_t m_local_cid = 0x00;
    uint16_t m_remote_cid = 0x00;
    uint16_t m_acl_handle = 0x00;

private:

    connection_status m_connection_status = connection_status::disconnected;
    bool m_config_local_req_sent = false; // whether sent local config request to remote device
    bool m_config_local_rsp_received = false; // whether received local config response from remote device
    bool m_config_remote_req_received = false; // whether received remote config request from remote device
    bool m_config_remote_rsp_sent = false; // whether send remote config response to remote device
};

}

