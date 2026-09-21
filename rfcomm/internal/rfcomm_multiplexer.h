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
#include <cstdint>
#include <iostream>
#include <vector>

#include "data_element.h" 
#include "../rfcomm_protocol.h"
#include "rfcomm_port.h"
#include "bluetooth_address.h"

#include "../common/state_machine.h"
#include "../l2cap/l2cap_common.h"

namespace bluetooth
{

enum class rfcomm_multipexer_state_type : uint8_t
{
    idle = 0x00,
    wait_conn_cnf = 0x01,
    configure = 0x02,
    sabm_wait_ua = 0x03,
    wait_sabm = 0x04,
    connected = 0x05,
    disc_wait_ua = 0x06
};

enum class rfcomm_event_type : uint8_t
{
    start_request = 0x00, // upper layer wants to start the multipexer
                          // to specified remote device
    l2cap_signaling_message = 0x01, // remote device wants to connect with local rfcomm,
                                     // m_signaling_pkt is valid
    multipexer_controlling = 0x02, // rfcomm multipexer contolling signlaing( one of sabm, disc, dm, ua, and uih ).
                                   // m_rfcomm_header and m_hci_data is valid
    l2cap_connetion_result = 0x03, // received rfcomm l2cap channel connection state changed event from l2cap layer
                                   // valid members: m_address, m_local_cid, m_remote_cid, m_state
};

std::ostream& operator<<( std::ostream& a_os, rfcomm_event_type a_state );

std::ostream& operator<<( std::ostream& a_os, rfcomm_multipexer_state_type a_type );

class rfcomm_multipexer;

class rfcomm_multipexer_event : public state_machine::abstract_event
{

public:

    rfcomm_event_type m_type;
    std::shared_ptr<signaling_channel_packet> m_signaling_pkt;
    rfcomm_header m_rfcomm_header;
    std::shared_ptr<hci_data> m_hci_data;
    bluetooth_address m_address;
    uint16_t m_local_cid;
    uint16_t m_remote_cid;
    l2cap_channel_state_type m_state;
};

class rfcomm_state_base : public state_machine::abstract_state
{

public:

    rfcomm_state_base( rfcomm_multipexer& a_sm, rfcomm_multipexer_state_type a_type );

    rfcomm_multipexer& get_multipexer()const
    {
        return m_rfcomm_mul;
    }

    void transition_to_state( rfcomm_multipexer_state_type a_type );

private:

    rfcomm_multipexer& m_rfcomm_mul;
};

class rfcomm_multipexer_idle : public rfcomm_state_base
{

public:

    rfcomm_multipexer_idle( rfcomm_multipexer& a_sm );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_connection_request( std::shared_ptr<connection_request> const& a_request );

    void handle_controlling( std::shared_ptr<rfcomm_multipexer_event>const& a_rfc_event );

    void on_enter()override;

    void on_exit()override;
};

/**
 * Wait connection confirm, we already send l2cap connection request
 */
class rfcomm_multipexer_wait_conn_cnf : public rfcomm_state_base
{

public:

    rfcomm_multipexer_wait_conn_cnf( rfcomm_multipexer& a_sm );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_connection_changed( std::shared_ptr<rfcomm_multipexer_event> const& a_event );

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_multipexer_configure : public rfcomm_state_base
{

public:

    rfcomm_multipexer_configure( rfcomm_multipexer& a_sm );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_configure_request( std::shared_ptr<l2cap_config_request> const& a_config_request );

    void handle_configure_respose( std::shared_ptr<l2cap_config_response> const& a_config_request );

    void handle_connection_sate_changed( std::shared_ptr<rfcomm_multipexer_event> const& a_event );

    void on_enter()override;

    void on_exit()override;
};

/**
 * We already send SABM command and waiting for UA response
 */
class rfcomm_multipexer_sabm_wait_ua : public rfcomm_state_base
{

public:

    rfcomm_multipexer_sabm_wait_ua( rfcomm_multipexer& a_sm );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_multipexer_contolling( std::shared_ptr<rfcomm_multipexer_event> const& a_event );

    void on_enter()override;

    void on_exit()override;
};

/**
 * We're waiting for SABM command
 */
class rfcomm_multipexer_wait_sabm : public rfcomm_state_base
{

public:

    rfcomm_multipexer_wait_sabm( rfcomm_multipexer& a_sm );

    ~rfcomm_multipexer_wait_sabm();

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_configure_respose( std::shared_ptr<l2cap_config_response> const& a_config_response );

    void handle_controlling( std::shared_ptr<rfcomm_multipexer_event> const& a_rfc_event );

    void on_enter()override;

    void on_exit()override;

private:

    void handle_timer_expire( uint32_t a_timer_id, std::string a_timer_name );

    uint32_t m_wait_sabm_timeout_timer = 0x00;
};

/**
 * The rfcomm multipexer has been connected with remote device
 */
class rfcomm_multipexer_connected : public rfcomm_state_base
{

public:

    rfcomm_multipexer_connected( rfcomm_multipexer& a_sm );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_controlling( std::shared_ptr<rfcomm_multipexer_event> const& a_rfc_event );

    void on_enter()override;

    void on_exit()override;
};

/**
 * We already send DISC command and waiting for UA response
 */
class rfcomm_multipexer_disc_wait_ua : public rfcomm_state_base
{

public:

    rfcomm_multipexer_disc_wait_ua( rfcomm_multipexer& a_sm );

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_multipexer : public state_machine
{
    friend class rfcomm_multipexer_idle;
    friend class rfcomm_multipexer_wait_conn_cnf;
    friend class rfcomm_multipexer_configure;
    friend class rfcomm_multipexer_sabm_wait_ua;
    friend class rfcomm_multipexer_wait_sabm;
    friend class rfcomm_multipexer_connected;
    friend class rfcomm_multipexer_disc_wait_ua;

public:

    rfcomm_multipexer();

    uint16_t get_local_cid();

    void set_local_cid( uint16_t a_local_cid )
    {
        m_local_cid = a_local_cid;
    }

    bluetooth_address const& get_remote_device()const
    {
        return m_remote_address;
    }

    void set_remote_device( bluetooth_address const& a_address )
    {
        m_remote_address = a_address;
    }

    void accept_coming_connection_request
        (
        uint8_t a_port,
        bool a_accept = true
        );

    void disconnect( uint8_t a_port, bool a_port_on_local );

    void send_port_user_data
        (
        uint8_t a_port,
        bool a_port_on_local,
        std::shared_ptr<std::vector<uint8_t>> a_spp_data
        );

    void handle_channel_connection_request( std::shared_ptr<connection_request> const& a_request );

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void handle_config_response( std::shared_ptr<l2cap_config_response> const& a_reponse );

    void clear();

    void handle_sdu( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_connection_state_changed
        (
        bluetooth_address a_address,
        uint16_t a_local_cid,
        uint16_t a_remote_cid,
        l2cap_channel_state_type a_state
        );

    void set_rfcomm_port_callback_finder
        (
        std::function<std::shared_ptr<rfcomm_port_callback_block>( uint8_t, bool )> a_callback
        )
    {
        m_callback_finder = a_callback;
    }

private:

    bool packet_valid
        (
        std::shared_ptr<hci_data> const& a_hci_data,
        rfcomm_header& a_header
        );

    void handle_uih_frame
        (
        std::shared_ptr<hci_data> const& a_hci_data,
        rfcomm_header& a_header
        );

    void connect();

    void config_local_channel( uint16_t a_acl_handle, uint16_t a_remote_cid );

    /**
     * Parameters Negotiation command/response on UIH
     */
    void handle_multipexer_pn
        (
        rfcomm_header const& a_rfc_header,
        std::shared_ptr<multipexer_pn_message> const& a_pn
        );

    /**
     * Modem Status command/response on UIH
     */
    void handle_multipexer_msc
        (
        rfcomm_header const& a_rfc_header,
        std::shared_ptr<multipexer_msc_message> const& a_pn
        );

    void send_sabm_frame();

    void send_ua_frame();

    void send_multipexer_message( std::shared_ptr<multipexer_message> a_pn );

    /**
     * To send user port data information to remote device. For rfcomm port class using
     */
    void send_port_data( rfcomm_header, uint8_t*, uint16_t );

    std::shared_ptr<rfcomm_port> find_port
        (
        bluetooth_address const& a_address,
        uint8_t const& a_port,
        bool a_local_inited
        );

    /**
     * Find out the idle port in the working list and push it into idle list
     */
    void check_working_port();

    uint16_t m_local_cid = 0x00;
    uint16_t m_remote_mtu = 0x00; // remote device's mtu on this rfcomm channel.
    bluetooth_address m_remote_address; // remote device's address
    bool m_wait_config_rsp_flag = false; // true if we wait for config rsp from remote device
    bool m_remote_configured = false;
    bool m_local_inited = false;
    std::vector<std::shared_ptr<rfcomm_port>> m_worked_ports;
    std::vector<std::shared_ptr<rfcomm_port>> m_idle_ports;
    std::function<std::shared_ptr<rfcomm_port_callback_block>( uint8_t, bool )> m_callback_finder;
};

}

