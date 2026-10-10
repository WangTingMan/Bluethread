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
#include "sdp/sdp_task.h"

#include "framework/timer_module.h"

namespace bluetooth
{

class sdp_manager;

struct continuation_control_block
{
    std::vector<uint8_t> buffer;
    uint16_t total_record_handle_count = 0u;
    std::vector<uint32_t> service_record_handles;
};

enum class pending_type : uint8_t
{
    invalid = 0x00,
    sdp_task_pending_type = 0x01,
    sdp_wrapped_task = 0x02,
};

struct pending_request
{
    pending_type m_pending_type = pending_type::invalid;
};

struct sdp_task_pending : public pending_request
{
    sdp_task_pending()
    {
        m_pending_type = pending_type::sdp_task_pending_type;
    }

    std::shared_ptr<sdp_task> m_pending_request;
    std::shared_ptr<sdp_protocol_base> m_protocol_msg;
};

struct wrapped_sdp_task_pending : public pending_request
{
    wrapped_sdp_task_pending()
    {
        m_pending_type = pending_type::sdp_wrapped_task;
    }

    std::function<void()> m_task;
};

/**
 * SDP l2cap connection control block.
 * Only one SDP connection for specified remote device
 */
class sdp_connection
{

public:

    sdp_connection( sdp_manager* a_sdp_manager )
        : m_sdp_manager( a_sdp_manager )
    {

    }

    ~sdp_connection();

    void set_sdp_manager( sdp_manager* a_sdp_manager )
    {
        m_sdp_manager = a_sdp_manager;
    }

    /**
     * Set the connection status.
     * return true if the status has been changed
     */
    bool set_connection_status( connection_status a_connection_status );

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

    void search_service( std::shared_ptr<sdp_task_service_search_request> a_service_search );

    void search_service_by_handle( uint32_t a_handle );

public:

    void handle_error_rsp
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_error_response> const& a_error_rsp
        );

    void handle_service_search_request
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_service_search_request> const& a_ser_searching
        );

    void handle_service_search_response
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_service_search_response> const& a_ser_response
        );

    void handle_service_search_attribute_request
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_service_search_attribute_req> const& a_request
        );

    void handle_service_attribute_request
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_service_attribute_request> const& a_request
        );

    void handle_sdp_service_attribute_response
        (
        sdp_header& _sdp_header,
        std::shared_ptr<sdp_service_attribute_response> const& a_response
        );

public:

    bluetooth_address m_address;
    uint16_t m_local_cid = 0x00;
    uint16_t m_remote_cid = 0x00;
    uint16_t m_acl_handle = 0x00;

private:

    std::shared_ptr<continuation_control_block> exract_continue_buffer( uintptr_t a_pointer )
    {
        std::shared_ptr<continuation_control_block> ccb;
        for( auto it = m_conitues_buffers.begin(); it != m_conitues_buffers.end(); )
        {
            ccb = *it;
            uintptr_t ptr_val = reinterpret_cast<uintptr_t>( ccb->buffer.data() );
            if( a_pointer == ptr_val )
            {
                it = m_conitues_buffers.erase( it );
                break;
            }
            else
            {
                ++it;
            }
        }
        return ccb;
    }

    std::shared_ptr<continuation_control_block> exract_continue_handles( uintptr_t a_pointer )
    {
        std::shared_ptr<continuation_control_block> ccb;
        for( auto it = m_conitues_buffers.begin(); it != m_conitues_buffers.end(); )
        {
            ccb = *it;
            uintptr_t ptr_val = reinterpret_cast<uintptr_t>( ccb->service_record_handles.data() );
            if( a_pointer == ptr_val )
            {
                it = m_conitues_buffers.erase( it );
                break;
            }
            else
            {
                ++it;
            }
        }
        return ccb;
    }

    void process_next_pending_request();

    void process_pending_sdp_task( std::shared_ptr<sdp_task_pending> a_pending_tsk );

    connection_status m_connection_status = connection_status::disconnected;
    int64_t m_connection_status_changed_time = 0;
    uint32_t m_connection_status_watch_timer = 0;

    uint16_t m_next_transaction_id = 0;
    bool m_config_local_req_sent = false; // whether sent local config request to remote device
    bool m_config_local_rsp_received = false; // whether received local config response from remote device
    bool m_config_remote_req_received = false; // whether received remote config request from remote device
    bool m_config_remote_rsp_sent = false; // whether send remote config response to remote device
    sdp_pdu_id m_incoming_pending_req = sdp_pdu_id::sdp_invalid_pdu;
    /* remote side's this l2cap channel's MTU*/
    uint32_t m_remote_mtu = 4800;
    std::vector<std::shared_ptr<continuation_control_block>> m_conitues_buffers;
    std::vector<std::shared_ptr<pending_request>> m_pending_tasks;
    std::shared_ptr<sdp_protocol_base> m_current_pending_request;/*we already sent request pdu to remote side and waiting for response*/
    sdp_manager* m_sdp_manager = nullptr;
};

}

