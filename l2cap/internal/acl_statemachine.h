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
#include "../common/state_machine.h"
#include "../l2cap_task.h"
#include "l2cap_internal_task.h"
#include "common.h"
#include "l2cap_channel_statemachine.h"
#include "l2cap_signaling.h"

#include <memory>
#include <vector>

/**
 * The ACL statemachine which has four state:
 * disconnected, connecting, connected and disconnecting.
 * disconnected --> connecting
 * connecting   --> connected
 * connecting   --> disconnected
 * connected    --> disconnecting
 * connected    --> disconnected
 * disconnecting--> disconnected
 */

namespace bluetooth
{

class acl_statemachine;

class acl_base_state : public state_machine::abstract_state
{

public:

    enum class acl_event_type : uint16_t
    {
        acl_connection_completed             = 0x00,
        acl_disconnected_with_acl_handle     = 0x01
    };

    acl_base_state( acl_statemachine& a_sm, uint32_t a_state_id );

    acl_statemachine& get_statemachine()
    {
        return m_acl_statemachine;
    }

    void transition_to_state( connection_status a_state );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

    bool handle_event( uint32_t a_event_type, std::vector<void*> a_params );

    void on_enter()override;

    void on_exit()override;

protected:

    acl_statemachine& m_acl_statemachine;
};

class acl_disconnected_state : public acl_base_state
{

public:

    acl_disconnected_state( acl_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t a_event_type, std::vector<void*> a_params ) override;

private:

    bool handle_connection_completed( std::vector<void*> a_params );

};

class acl_connecting_state : public acl_base_state
{

public:

    acl_connecting_state( acl_statemachine& a_sm, uint32_t a_state_id );

private:

};

class acl_connected_state : public acl_base_state
{

public:

    acl_connected_state( acl_statemachine& a_sm, uint32_t a_state_id );

    void on_enter()override;

    bool handle_event(uint32_t a_event_type, std::vector<void*> a_params) override;

private:

};

class acl_disconnecting_state : public acl_base_state
{

public:

    acl_disconnecting_state( acl_statemachine& a_sm, uint32_t a_state_id );

private:

};

class acl_statemachine : public state_machine
{

    friend class acl_disconnected_state;
    friend class acl_connecting_state;
    friend class acl_connected_state;
    friend class acl_disconnecting_state;

public:

    acl_statemachine();

    void set_remote_device( bluetooth_address a_remote_device )
    {
        m_signaling_channel->set_remote_address( a_remote_device );
    }

    bluetooth_address const& get_remote_device()const
    {
        return m_signaling_channel->get_remote_address();
    }

    void set_acl_type( acl_type a_type )
    {
        m_signaling_channel->set_acl_type( a_type );
    }

    acl_type get_acl_type()const
    {
        return m_signaling_channel->get_acl_type();
    }

    /**
     * Set the query function used to look up the L2CAP callbacks registered
     * by upper layers for a specific PSM.
     * [in] a_query the query function, takes a PSM value as input and outputs
     *      the registered callbacks (connection, configuration, data, etc.)
     *      through the reference parameter.
     * [return] of the query function: true if callbacks are registered for
     *      the given PSM, false otherwise.
     */
    void set_l2cap_callback_query
        (
        std::function<bool(uint16_t, l2cap_callbacks&)> a_query
        )
    {
        m_psm_callback_query = a_query;
    }

    void handle_request_channel_connection(uint16_t a_psm);

    void handle_pending_outgoing_connection_request(std::vector<channel_connection_request> a_pending_connection_request);

    void handle_coming_acl_packet(std::shared_ptr<hci_data> const& a_hci_data);

    void accept_connection_req(std::shared_ptr<connection_request> const& a_request);

    void reject_connection_req
        (
        std::shared_ptr<connection_request> const& a_request,
        connection_req_result a_reason
        );

    void disconnect_channel_req( uint16_t a_local_cid );

    void accept_config_req(std::shared_ptr<l2cap_config_request> const& a_request);

    void config_local_channel_req(std::shared_ptr<l2cap_config_local_channel_request> const& a_request);

    void send_upper_sdu
        (
        std::shared_ptr<l2cap_task_send_l2cap_sdu> const& a_tsk
        );

    uint16_t get_acl_handle()const
    {
        return m_signaling_channel->get_acl_handle();
    }

    void close_channel_with_invalid_cid
        (
        std::shared_ptr<l2cap_task_remote_invalid_cid_channel_close> const& a_tsk
        );

    void remove_channel_from_cache( uint16_t a_local_cid );

private:

    void set_handle( uint16_t a_handle )
    {
        m_signaling_channel->set_acl_handle( a_handle );
    }

    void set_encrypted( bool a_encrypted )
    {
        m_encrypted = a_encrypted;
    }

    void connection_state_endtered();

    void handle_signaling_pkt(std::shared_ptr<signaling_channel_packet> const& a_request);

    void handle_disconnected(uint16_t a_handle);

    /**
     * Handle the information has been requested from remote device.
     * a_acl_handle: the ACL handle
     * a_info_type: which information type has been requested
     */
    void handle_channel_infor_requested(uint16_t a_acl_handle, l2cap_channel_information_type a_info_type);

    /**
     * Handle coming l2cap channel connection request
     */
    void handle_connection_request(std::shared_ptr<connection_request> const& a_request);

    void handle_config_request(std::shared_ptr<l2cap_config_request> const& a_response);

    void handle_config_response(std::shared_ptr<l2cap_config_response> const& a_request);

    void handle_disconnect_request(std::shared_ptr<l2cap_disconnect_request> const& a_request);

    /**
     * Handle connection response signaling packet from remote device
     */
    void handle_connect_response(std::shared_ptr<l2cap_connect_response> const& a_request);

    std::shared_ptr<l2cap_channel_statemachine> find_channel_state_machine
        (
        uint16_t a_acl_handle,
        uint16_t a_local_cid
        );

    std::shared_ptr<l2cap_channel_statemachine> find_channel_state_machine_by_remote_cid
        (
        uint16_t a_acl_handle,
        uint16_t a_remote_cid
        );

    std::shared_ptr<l2cap_channel_statemachine> find_channel_state_machine_by_cids
        (
        uint16_t a_local_cid,
        uint16_t a_remote_cid
        );

    std::shared_ptr<l2cap_channel_statemachine> find_channel_state_machine
        (
        uint16_t a_local_cid
        );

    uint16_t                                                    m_next_local_cid = 0x0050;
    std::shared_ptr<l2cap_signaling>                            m_signaling_channel;// the signaling fix channel
    std::vector<std::shared_ptr<l2cap_channel_statemachine>>    m_channel_machines; // to store all l2cap channel connections
    bool                                                        m_encrypted;

    /**
     * Outgoing L2CAP channel connection requests initiated by the local
     * upper layer, which cannot be sent yet because the precondition is
     * not satisfied (e.g. no ACL connection to the remote device exists).
     * They are cached here until the ACL link is established, then the
     * L2CAP connection requests are sent to the remote device in order.
     */
    std::vector<channel_connection_request>                     m_pending_outgoing_connection_requests;

    /**
     * Query function used to look up the L2CAP callbacks registered by
     * upper layers for a specific PSM. The query takes a PSM value as input
     * and outputs the registered callbacks (connection, configuration, data,
     * etc.) through the reference parameter; it returns true if callbacks
     * are registered for the given PSM, false otherwise.
     * Injected by acl_manager, which owns the PSM-to-callbacks registry.
     */
    std::function<bool(uint16_t, l2cap_callbacks&)>             m_psm_callback_query;
};

}

