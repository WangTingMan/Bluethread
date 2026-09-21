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
#include "../common/protocol_headers.h"
#include <iostream>
#include <memory>

namespace bluetooth
{

enum class rfcomm_frame_type : uint8_t
{
    sabm = 0x2F, // Set Asynchronous balanced mode
    ua = 0x63,   // unnumbered acknowledgement
    dm = 0x0F,   // disconnected mode
    disc = 0x43, // disconnect
    uih = 0xEF,  // unnmbered information with header check
    ui = 0x03    // unnmbered information
};

enum class multipexer_message_type : uint8_t
{
    pn = 0x80 >> 2, // Parameter Negotiation
    test = 0x20 >> 2, // test
    fc_on = 0xA0 >> 2, // flow control on
    fc_off = 0x60 >> 2, // flow control off
    msc = 0xE0 >> 2, // Modem status Command
    nsc = 0x10 >> 2, // non support command
    rpn = 0x90 >> 2, // Remote Port Negotiation
    rls= 0x50 >> 2, // remote line status
};

std::ostream& operator<<( std::ostream& a_os, rfcomm_frame_type a_type );

std::ostream& operator<<( std::ostream& a_os, multipexer_message_type a_type );

uint8_t rfcomm_calc_fcs( uint8_t* p, uint16_t len );

bool rfcomm_check_fcs( uint8_t* p, uint16_t len, uint8_t received_fcs );

/**
 * For UIH frame, if P/F bit equals 1, then the header contains address, control,
 * length indicator( 1 or 2 bytes ), and credits field; if P/F bit equals 0, then
 * the header contains address, control, length indicator( 1 or 2 bytes).
 * For other frame( SABM, DISC, UA and DM ),the header contains address, control,
 * length indicator( 1 or 2 bytes ).
 */
class rfcomm_header : public l2cap_header
{

public:

    uint8_t get_min_header_size();

    void set_channel_number( uint8_t a_ch_num );

    uint8_t get_channel_number();

    /**
     * We should set the CR value as following table for SABM, UA, DM and DISC:
     * ------------------------------------------------------------------
     * |          | Initiator -->  Responder     |  a_cr_value = true   |
     * |  Command |-----------------------------------------------------|
     * |          | Responder -->  Initiator     |  a_cr_value = false  |
     * ------------------------------------------------------------------
     * |          | Initiator -->  Responder     |  a_cr_value = false  |
     * | Response |------------------------------------------------------
     * |          | Responder -->  Initiator     |  a_cr_value = true   |
     * ------------------------------------------------------------------
     */
    void set_cr( bool a_cr_value );

    /**
     * Get Command/Response bit value
     */
    bool cr_value_in_header()const;

    void set_frame_type( rfcomm_frame_type a_type );

    rfcomm_frame_type get_frame_type()const;

    /**
     * Return if the P/F bit set or not
     */
    bool poll_final_set()const;

    void set_poll_final( bool a_pf );

    /**
     * Here is the parameter length. See sdp specification
     */
    void set_sdu_length( uint16_t a_length )override;

    uint16_t get_information_length();

    void to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )override;

    uint16_t header_size()const override;

    void parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )override;

    uint8_t get_credit_increase_value();

    void set_credit_increase_value( uint8_t a_value );

    uint8_t get_port()const
    {
        return m_address_field >> 3;
    }

    void set_dlci( uint8_t a_dlci )
    {
        a_dlci <<= 2;
        m_address_field &= 0x03;
        m_address_field |= a_dlci;
        set_ea_bit();
    }

    uint8_t get_dlci()const
    {
        return m_address_field >> 2;
    }

private:

    void set_ea_bit()
    {
        m_address_field |= 0x01;
    }

    uint8_t m_address_field = 0x00;
    uint8_t m_control_field = 0x00;
    uint8_t m_length[2] = { 0 };
    uint8_t m_credit_increase_value = 0x00;
};

class multipexer_message
{

public:

    virtual ~multipexer_message();

    static std::shared_ptr<multipexer_message> parse_from( uint8_t* a_buffer, uint16_t a_size );

    virtual uint16_t get_raw_buffer_size() = 0;

    virtual void to_buffer( uint8_t* a_buffer, uint16_t a_size );

    multipexer_message_type get_type()const
    {
        return m_type;
    }

    uint16_t get_length()const
    {
        return m_message_length;
    }

    void set_is_command( bool a_is_command )
    {
        m_is_command = a_is_command;
    }

    bool is_command()const
    {
        return m_is_command;
    }

protected:

    virtual bool parse_detail( uint8_t* a_buffer, uint32_t a_size ) = 0;

    multipexer_message_type m_type = multipexer_message_type::pn;
    uint16_t m_message_length = 0x00;
    bool m_is_command = false;
};

class multipexer_pn_message : public multipexer_message
{

public:

    multipexer_pn_message();

    uint16_t get_raw_buffer_size()override;

    void to_buffer( uint8_t* a_buffer, uint16_t a_size )override;

    void set_dlci( uint8_t a_dlci )
    {
        m_dlci = a_dlci;
    }

    uint8_t get_dlci()const
    {
        return m_dlci;
    }

    uint8_t get_port();

    void set_support_credit( bool a_support )
    {
        m_credit_supported = a_support;
    }

    bool support_credit()const
    {
        return m_credit_supported;
    }

    uint8_t priority()const
    {
        return m_priority;
    }

    void set_priority( uint8_t a_priority )
    {
        m_priority = a_priority;
    }

    void set_max_frame_size( uint16_t a_size )
    {
        m_max_frame_size = a_size;
    }

    uint16_t max_frame_size()const
    {
        return m_max_frame_size;
    }

    uint8_t credit_init_value()const
    {
        return m_credit_init_value;
    }

    void set_credit_init_value( uint8_t a_credit )
    {
        m_credit_init_value = a_credit;
    }

protected:

    bool parse_detail( uint8_t* a_buffer, uint32_t a_size )override;

private:

    uint8_t m_dlci = 0x00;
    bool m_credit_supported = false;
    uint8_t m_priority = 0x00;
    uint16_t m_max_frame_size = 0x00;
    uint8_t m_credit_init_value = 0x00;
};

// rfcomm multipexer UIH Modem Status Command message
class multipexer_msc_message : public multipexer_message
{

public:

    multipexer_msc_message();

    uint16_t get_raw_buffer_size() override;

    void to_buffer( uint8_t* a_buffer, uint16_t a_size )override;

    uint8_t get_dlci()const
    {
        return m_dlci;
    }

    void set_dlci( uint8_t a_dlci )
    {
        m_dlci = a_dlci;
    }

    uint8_t get_port()const
    {
        return m_dlci >> 1;
    }

    void set_port( uint8_t a_port )
    {
        m_dlci &= 0x07;
        m_dlci |= ( a_port << 3 );
        m_dlci |= 0x03;
    }

    bool flow_control_on()const
    {
        return m_flow_control_on;
    }

    void set_flow_control_on( bool a_flow_control_on )
    {
        m_flow_control_on = a_flow_control_on;
    }

    bool ready_communicated()const
    {
        return m_ready_communicated;
    }

    void set_ready_communicated( bool a_communicated )
    {
        m_ready_communicated = a_communicated;
    }

    bool ready_received()const
    {
        return m_ready_received;
    }

    void set_ready_received( bool a_ready_received )
    {
        m_ready_received = a_ready_received;
    }

    bool incoming_call()const
    {
        return m_incoming_call;
    }

    void set_incoming_call( bool a_incoming_call )
    {
        m_incoming_call = a_incoming_call;
    }

    bool data_valid()const
    {
        return m_data_valid;
    }

    void set_data_valid( bool a_data_valid )
    {
        m_data_valid = a_data_valid;
    }

protected:

    bool parse_detail( uint8_t* a_buffer, uint32_t a_size )override;

private:

    uint8_t m_dlci = 0x00;
    bool m_flow_control_on = false; // True means cannot receive any more data frames.
    bool m_ready_communicated = false; // True means ready to send data frames
    bool m_ready_received = false; // True means ready to receive data frames
    bool m_incoming_call = false;
    bool m_data_valid = false; // True means the valid data is sending.
};

}

