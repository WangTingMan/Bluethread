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
#include "../l2cap_common.h"
#include "l2cap_signaling.h"

namespace bluetooth
{

class l2cap_channel_statemachine;

class l2cap_channel_event : public state_machine::abstract_event
{

public:

    enum class event_type : uint8_t
    {
        invalid = 0x00,
        handle_signaling_pkt = 0x01,        // Handle signaling message from remote device
        accept_connection_request = 0x02,   // Let state machine to accpet the connection request
        request_configure_local = 0x03,     // Let state machine send local channel configuration options.
                                            // The detail options are m_local_channel_config_request
        accept_config_request = 0x04,       // Upper layer request accept coming config request.
        channel_sdu_pkt_from_controller = 0x05, // The upper layer data packet. Only m_channel_data is valid
        channel_sdu_pkt_from_upper = 0x06,  // The upper layer data packet. Only m_channel_data is valid. We need to send out
        reject_connection_request = 0x07,   // The upper layer rejected coming connection request. m_channel_pkt can be cast to
                                            // connection_request and m_reason indicates why rejected
        open_channel_request = 0x08,        // Let channle machine make a connection request to remote device
        close_channel_request = 0x09,       // Let channel machine disconnect the channel
    };

    event_type m_type = event_type::invalid;
    std::shared_ptr<signaling_channel_packet> m_channel_pkt;
    std::shared_ptr<hci_data> m_channel_data;
    connection_req_result m_reason = connection_req_result::connection_refused_security;
    std::shared_ptr<l2cap_config_local_channel_request> m_local_channel_config_request;
};

class l2cap_channel_base_state : public state_machine::abstract_state
{

public:

    l2cap_channel_base_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    l2cap_channel_statemachine& get_statemachine()
    {
        return m_channel_statemachine;
    }

    void transition_to_state( l2cap_channel_state_type a_state );

private:

    l2cap_channel_statemachine& m_channel_statemachine;
};

class l2cap_channel_close_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_close_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

    void on_enter()override;

    void on_exit()override;

    void handle_signaling_request( std::shared_ptr<connection_request> a_requst );

    void handle_signaling_request( std::shared_ptr<l2cap_config_request> a_requst );

    void handle_signaling_request( std::shared_ptr<l2cap_disconnect_request> a_requst );

    void accept_connection_request( std::shared_ptr<l2cap_channel_event> const& a_event );

    void send_connection_request_to_remote();
};

class l2cap_channel_wait_connect_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_connect_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

    void set_original_request( std::shared_ptr<connection_request> a_requst );

private:

    void handle_timer_expired( uint32_t a_timer_id, std::string a_name );

    void accept_connection_request( std::shared_ptr<l2cap_channel_event> const& a_event );

    void reject_connection_request( std::shared_ptr<l2cap_channel_event> const& a_event );

    uint32_t m_waiting_authorize_timer = 0x00;
    std::shared_ptr<connection_request> m_original_request;
};

class l2cap_channel_wait_config_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_config_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void accept_coming_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt );
};

class l2cap_channel_wait_config_req_rsp_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_config_req_rsp_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void handle_config_response( std::shared_ptr<l2cap_config_response> const& a_response );

    void accept_coming_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

};

class l2cap_channel_wait_config_req_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_config_req_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_response );

    void accept_coming_config_request( std::shared_ptr<l2cap_config_request> const& a_request );
};

class l2cap_channel_wait_config_rsp_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_config_rsp_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_config_response( std::shared_ptr<l2cap_config_response> const& a_response );
};

class l2cap_channel_open_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_open_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt );

};

class l2cap_channel_wait_connect_rsp_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_connect_rsp_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt );

};

class l2cap_channel_wait_send_config_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_send_config_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt );

};

class l2cap_channel_wait_disconnect_state : public l2cap_channel_base_state
{

public:

    l2cap_channel_wait_disconnect_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt );

};

class l2cap_channel_statemachine : public state_machine
{

    friend class l2cap_channel_close_state;
    friend class l2cap_channel_wait_connect_state;
    friend class l2cap_channel_wait_config_state;
    friend class l2cap_channel_wait_config_req_rsp_state;
    friend class l2cap_channel_wait_config_rsp_state;
    friend class l2cap_channel_open_state;
    friend class l2cap_channel_wait_config_req_state;
    friend class l2cap_channel_base_state;
    friend class l2cap_channel_wait_connect_rsp_state;
    friend class l2cap_channel_wait_send_config_state;
    friend class l2cap_channel_wait_disconnect_state;

public:

    l2cap_channel_statemachine();

    uint16_t const& get_psm()const
    {
        return m_psm_value;
    }

    void set_psm( uint16_t a_psm )
    {
        m_psm_value = a_psm;
    }

    uint16_t const& get_acl_handle()const
    {
        return m_connection_handle;
    }

    void set_acl_handle( uint16_t a_handle )
    {
        m_connection_handle = a_handle;
    }

    uint16_t const& get_remote_cid()const
    {
        return m_remote_channel_id;
    }

    void set_remote_cid( uint16_t a_remote_cid )
    {
        m_remote_channel_id = a_remote_cid;
    }

    void set_local_cid( uint16_t a_local_cid )
    {
        m_local_channel_id = a_local_cid;
    }

    uint16_t get_local_cid()const
    {
        return m_local_channel_id;
    }

    void set_signaling_channel( std::shared_ptr<l2cap_signaling> a_sig )
    {
        m_signaling_channel = std::move( a_sig );
    }

    void set_upper_callbacks( l2cap_callbacks a_callbacks )
    {
        m_callbacks = a_callbacks;
    }

    /**
     * Accept the connection request from remote device
     */
    void accept_connection_req( uint16_t a_local_cid, std::shared_ptr<connection_request> const& a_request );

    /**
    * Accept the connection request from remote device
    */
    void reject_connection_req
        (
        std::shared_ptr<connection_request> const& a_request,
        connection_req_result a_reason
        );

    /**
     * Disconnect the channel from remote device. This is local upper layer request.
     */
    void disconnect_channel_req();

    /**
     * Accept the config request from remote device
     */
    void accept_config_req( std::shared_ptr<l2cap_config_request> const& a_request );

    /**
     * Handle the channel configuration request from remote device
     */
    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    /**
     * Handle the channel configuration reponse from remote device
     */
    void handle_config_response( std::shared_ptr<l2cap_config_response> const& a_request );
 
    /**
     * Handle the channel disconnect request from remote device
     */
    void handle_disconnect_request( std::shared_ptr<l2cap_disconnect_request> const& a_request );

    /**
     * Handle configure local channle id. This request is from upper layer.
     */
    void config_local_channel_req( std::shared_ptr<l2cap_config_local_channel_request> const& a_request );

    /**
     * Send upper layer sdu to remote device
     */
    void send_upper_sdu( std::shared_ptr<hci_data> const& a_hci_data );

private:

    void set_default_config();

    void clear();

    void send_completed_acl_packet( std::shared_ptr<hci_data> const& a_hci_data );

    void send_disconnect_request_to_remote();

    /**
     * @brief Merge parsed config options from fragmented L2CAP CONFIGURATION_REQ into cached list.
     * If an option of the same type already exists, overwrite with the newly received value.
     * This function is used for continuation flag segmented configuration request.
     *
     * @param a_options Parsed options from current CONFIGURATION_REQ fragment
     */
    void cache_continue_config_options( std::vector<channel_config_option> const& a_options );

    /**
     * @brief Internal state machine handler for L2CAP Configuration Request
     * @warning Do NOT call this function directly from outside the state machine.
     * It will only be invoked when the channel has transitioned into a valid state
     * ready to process incoming ConfigRequest.
     */
    void handle_config_request_internal( std::shared_ptr<l2cap_config_request> const& a_request );

    /**
     * @brief Process incoming Retransmission and Flow Control(FCR) configuration option from peer in L2CAP CONFIG_REQ
     *
     * Handle L2CAP Flow Control & Retransmission(FCR) option negotiation received from remote peer.
     * This function evaluates the requested link mode and associated parameters(MPS, Rx Window, MaxTransmit etc.)
     * carried in a_fcr_option.
     *
     * @param[in]  a_fcr_option  The raw FCR configuration option received from peer (requested parameters)
     * @param[out] a_suggestion  If the request is rejected, this output argument holds our preferred FCR
     *                           option values to propose back to peer in CONFIG_RSP.
     *                           When request is accepted, the content of a_suggestion is unused.
     *
     * @note Negotiation logic: compare requested link mode against local capability.
     *         - ERTM / Streaming mode requires validation of MPS, receive window and max retransmission count.
     *         - If peer requests a higher link mode than we support, fill our supported mode into a_suggestion and return false.
     *         - If requested mode is fully supported and parameters are valid, return true to accept.
     * @note This function only handles FCR option, does not directly construct CONFIG_RSP packet.
     *       The caller is responsible for assembling response packet and setting configuration result code.
     */
    channel_config_result process_incoming_fcr_request
        (
        channel_config_option const& a_fcr_option,
        channel_config_option&       a_suggestion
        );

    /**
     * @brief Internal state machine handler for L2CAP Configuration Request
     * @warning Do NOT call this function directly from outside the state machine.
     * It will only be invoked when the channel has transitioned into a valid state
     * ready to process outgoing ConfigRequest.
     */
    void config_local_channel_req_internal
        (
        std::shared_ptr<l2cap_config_local_channel_request> const& a_request,
        l2cap_channel_base_state *a_current_state
        );

    void merge_config_options( std::vector<channel_config_option> options, bool a_is_remote );

    uint16_t m_connection_handle = 0x00;
    uint16_t m_local_channel_id = 0x00;
    uint16_t m_remote_channel_id = 0x00;
    uint16_t m_psm_value = 0x00;

    /**
     * @brief Preferred receive-side flush timeout (unit: 0.625ms per count)
     *
     * Upper layer protocols may configure this value.
     * This parameter is advertised to the remote peer as the recommended flush timeout
     * that the peer shall use when transmitting packets on this L2CAP channel.
     *
     * Example for AVDTP:
     * If local AVDTP sink delay is 475 ms, convert to L2CAP flush timeout count:
     * m_prefered_rx_flush_timeout = 475 * 1000 / 625 = 760.
     * Each count represents 0.625 millisecond.
     *
     * @note This is only a recommendation sent in L2CAP CONFIG_RSP;
     * the remote sender ultimately decides its actual flush timeout setting.
     */
    uint16_t m_prefered_rx_flush_timeout = 0xFFFF;

    bool m_local_inited = false;
    l2cap_callbacks m_callbacks;
    l2cap_channel_mode m_prefered_mode = l2cap_channel_mode::basic_mode;
    l2cap_channel_mode m_channel_mode = l2cap_channel_mode::basic_mode;
    std::shared_ptr<l2cap_signaling> m_signaling_channel;

    std::vector<channel_config_option> m_remote_configs;
    std::vector<channel_config_option> m_local_configs;

    /**
     * @brief Caches partial configuration options from L2CAP CONFIGURATION_REQ with continuation flag set.
     *
     * When receiving fragmented config requests (continuation flag = 1), store parsed options temporarily.
     * After all fragments are received (continuation flag = 0), process all cached options together.
     * And need clear this container if we already completed configuration.
     */
    std::vector<channel_config_option> m_cached_incoming_continue_configs;
};

}

