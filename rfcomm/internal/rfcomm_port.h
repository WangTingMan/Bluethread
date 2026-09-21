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
#include <functional>

#include "data_element.h"
#include "bluetooth_address.h"
#include "../rfcomm_protocol.h"
#include "../rfcomm_common.h"

#include "..\common\state_machine.h"

namespace bluetooth
{

class rfcomm_port;

enum class rfcomm_port_state_type : uint8_t
{
    idle_state = 0x00,
    wait_sabm_state = 0x01,
    wait_pn_rsp_state = 0x02,
    modem_config_state = 0x03,
    sabm_wait_ua_state = 0x04,
    open = 0x05,
    disc_wait_ua_state = 0x06
};

enum class rfcomm_port_event_type : uint8_t
{
    invalid = 0x00,
    multipexer_message_type = 0x01, // m_mulx_msg is valid
    controlling_message = 0x02, // SABM, DM, DISC, UH frame. m_rfcomm_header is valid
    send_multipexer_meesage_type = 0x03, // send m_mulx_msg to remote device
    port_uih_type = 0x04, // the receive UIH data on the port. m_rfcomm_header and m_raw_data are valid
    accept_connection_request = 0x05,
    reject_connection_request = 0x06,
    disconnect_port = 0x07, // upper layer request disconnet current port
};

std::ostream& operator<<( std::ostream& a_os, rfcomm_port_state_type a_state );

struct modem_status
{
    bool m_flow_control_on = false; // True means cannot receive any more data frames.
    bool m_ready_communicated = false; // True means ready to send data frames
    bool m_ready_received = false; // True means ready to receive data frames
    bool m_incoming_call = false;
    bool m_data_valid = false; // True means the valid data is sending.
};

class rfcomm_port_event : public state_machine::abstract_event
{

public:

    rfcomm_port_event_type m_type = rfcomm_port_event_type::invalid;
    std::shared_ptr<multipexer_message> m_mulx_msg;
    rfcomm_header* m_rfcomm_header = nullptr;
    std::shared_ptr<hci_data> m_raw_data;
};

class rfcomm_port_base_state : public state_machine::abstract_state
{

public:

    rfcomm_port_base_state( rfcomm_port& a_sm, rfcomm_port_state_type a_type );

    rfcomm_port& get_port()const
    {
        return m_rfcomm_port;
    }

    void transition_to_state( rfcomm_port_state_type a_type );

private:

    rfcomm_port& m_rfcomm_port;

};

class rfcomm_port_idle_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_idle_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::idle_state )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_connection_request( std::shared_ptr<multipexer_pn_message> const& a_pn );

    void send_connection_request( std::shared_ptr<multipexer_pn_message> const& a_pn );

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port_wait_sabm_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_wait_sabm_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::wait_sabm_state )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port_wait_pn_rsp_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_wait_pn_rsp_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::wait_pn_rsp_state )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port_modem_config_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_modem_config_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::modem_config_state )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void handle_modem_status_message( std::shared_ptr<multipexer_msc_message> const& a_msc );

    void send_local_modem_status();

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port_sabm_wait_ua_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_sabm_wait_ua_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::sabm_wait_ua_state )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port_disc_wait_ua_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_disc_wait_ua_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::disc_wait_ua_state )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port_open_state : public rfcomm_port_base_state
{

public:

    rfcomm_port_open_state( rfcomm_port& a_sm )
        : rfcomm_port_base_state( a_sm, rfcomm_port_state_type::open )
    {

    }

    bool handle_event( uint32_t event, void* p_data )override;

    bool handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )override;

protected:

    void on_enter()override;

    void on_exit()override;
};

class rfcomm_port : public state_machine
{
    friend class rfcomm_port_idle_state;
    friend class rfcomm_port_wait_sabm_state;
    friend class rfcomm_port_modem_config_state;
    friend class rfcomm_port_wait_pn_rsp_state;
    friend class rfcomm_port_open_state;
    friend class rfcomm_port_sabm_wait_ua_state;
    friend class rfcomm_port_disc_wait_ua_state;
    friend class rfcomm_port_base_state;

public:

    rfcomm_port();

    bool local_inited()const
    {
        return m_local_inited;
    }

    void set_local_inited( bool a_local_inited = true )
    {
        m_local_inited = a_local_inited;
    }

    bluetooth_address const& get_remote_device()const
    {
        return m_remote_device;
    }

    void set_remote_device( bluetooth_address a_address )
    {
        m_remote_device = a_address;
    }

    void set_dlci( uint8_t a_dlci )
    {
        m_dlci = a_dlci;
    }

    uint8_t get_dlci()const
    {
        return m_dlci;
    }

    uint8_t get_port()const
    {
        return m_dlci >> 1;
    }

    /**
     * The multipexer we base is disconnected, so we need transfer to idle directly.
     */
    void handle_multipexer_disconnect();

    /**
     * To disconect this port connection
     */
    void disconnect();

    /**
     * Send port user data to remote device
     * a_user_data only contain user data without any header
     */
    void send_user_data( std::shared_ptr<std::vector<uint8_t>> a_user_data );

    /**
     * accept coming port connection request.
     * To send UA as response for SABM
     */
    void handle_accept_connection_request( bool a_accept = true );

    void handle_connection_request( std::shared_ptr<multipexer_pn_message> const& a_pn );

    void handle_modem_status_message( std::shared_ptr<multipexer_msc_message> const& a_msc );

    void set_control_sender( std::function<void( std::shared_ptr<multipexer_message> )> a_sender )
    {
        m_multi_sender = std::move( a_sender );
    }

    void set_port_sender( std::function<void( rfcomm_header, uint8_t*, uint16_t )> a_sender )
    {
        m_port_sender = a_sender;
    }

    void handle_controlling( rfcomm_header& a_header );

    void handle_received_uih_data
        (
        std::shared_ptr<hci_data> const& a_hci_data,
        rfcomm_header& a_header
        );

    void set_port_callback( std::shared_ptr<rfcomm_port_callback_block> a_callback )
    {
        m_port_callback = a_callback;
    }

    void clear();

    bool is_working()const
    {
        return get_id() != static_cast<int>( rfcomm_port_state_type::idle_state );
    }

private:

    void handle_received_uih_data_internal
        (
        std::shared_ptr<hci_data> const& a_hci_data,
        rfcomm_header& a_header
        );

    /**
     * Force send local credit value to remote device
     */
    void force_send_local_credit();

    /**
     * accept coming port connection request.
     * To send UA as response for SABM
     */
    void accept_connection_request( bool a_accept );

    std::function<void( std::shared_ptr<multipexer_message> )> m_multi_sender; /* Send UIH command on channel 0
                                                                                  Like PN, Test, FCon, FCoff, MSC, NSC for
                                                                                  current port
                                                                               */

    std::function<void( rfcomm_header, uint8_t*, uint16_t )> m_port_sender; /* Send SABM, UA, DM, DISC, UIH on current port.
                                                                            *  uint8_t and uint16_t parameters only valid for
                                                                            * UIH frame.
                                                                            */

    bool m_local_inited = false;
    uint8_t m_dlci = 0x00;
    bluetooth_address m_remote_device;
    bool m_remote_credit_supported = false;
    uint16_t m_remote_max_frame_size = 0x00;
    uint16_t m_remote_credit_value = 0x00;
    uint16_t m_local_credit_value = 0x07;
    modem_status m_remote_modem_status;
    modem_status m_local_modem_status;
    bool m_local_modem_status_configed = false;
    bool m_remote_modem_status_configed = false;
    std::shared_ptr<rfcomm_port_callback_block> m_port_callback;
};

}

