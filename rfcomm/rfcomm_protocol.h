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
    sabm =  0x2F, // Set Asynchronous balanced mode
    ua =    0x63, // unnumbered acknowledgement
    dm =    0x0F, // disconnected mode
    disc =  0x43, // disconnect
    uih =   0xEF, // unnmbered information with header check
    ui =    0x03  // unnmbered information
};

enum class multipexer_message_type : uint8_t
{
    pn =        0x80 >> 2, // Parameter Negotiation
    test =      0x20 >> 2, // test
    fc_on =     0xA0 >> 2, // flow control on
    fc_off =    0x60 >> 2, // flow control off
    msc =       0xE0 >> 2, // Modem status Command
    nsc =       0x10 >> 2, // non support command
    rpn =       0x90 >> 2, // Remote Port Negotiation
    rls =       0x50 >> 2, // remote line status
};

std::ostream& operator<<( std::ostream& a_os, rfcomm_frame_type a_type );

std::ostream& operator<<( std::ostream& a_os, multipexer_message_type a_type );

uint8_t rfcomm_calc_fcs( uint8_t* p, uint16_t len );

bool rfcomm_check_fcs( uint8_t* p, uint16_t len, uint8_t received_fcs );

/**
 * @brief Generate RFCOMM DLCI for remote-side server channel(port), conform to TS 27.010
 *
 * RFCOMM DLCI encoding rule:
 * DLCI = (server_channel << 1) | D_bit
 * D-bit definition (per TS 27.010):
 * - D = 1: The target server channel(port) resides on the device that initiated the RFCOMM multiplexer L2CAP connection.
 * - D = 0: The target server channel(port) resides on the device that accepted the RFCOMM multiplexer L2CAP connection.
 *
 * This function is used for creating DLCI to access port located on remote peer device.
 *
 * @param a_port_on_remote Remote RFCOMM server channel number(port), valid range [1,30]
 * @param a_local_is_multiplexer_l2cap_initiator true: local device is the initiator of mux L2CAP connection;
 *                              false: local device is the acceptor of mux L2CAP connection.
 * @return uint8_t constructed DLCI value for remote port.
 * @note Input port out of [1,30] is undefined behaviour, caller should validate before call.
 */
inline uint8_t make_dlci_for_remote_port( uint8_t a_port_on_remote, bool a_local_is_multiplexer_l2cap_initiator )
{
    // Target port is on remote side:
    // If local is mux initiator -> remote is acceptor -> D=0
    // If local is mux acceptor -> remote is initiator -> D=1
    uint8_t d_bit = a_local_is_multiplexer_l2cap_initiator ? 0U : 1U;
    return static_cast<uint8_t>( ( a_port_on_remote << 1 ) | d_bit );
}

/**
 * @brief Generate RFCOMM DLCI for local-side server channel(port), conform to TS 27.010
 *
 * RFCOMM DLCI encoding rule:
 * DLCI = (server_channel << 1) | D_bit
 * D-bit definition (per TS 27.010):
 * - D = 1: The target server channel(port) resides on the device that initiated the RFCOMM multiplexer L2CAP connection.
 * - D = 0: The target server channel(port) resides on the device that accepted the RFCOMM multiplexer L2CAP connection.
 *
 * This function is used for creating DLCI to access port located on local device.
 *
 * @param a_port_on_local Local RFCOMM server channel number(port), valid range [1,30]
 * @param a_local_is_multiplexer_l2cap_initiator true: local device is the initiator of mux L2CAP connection;
 *                              false: local device is the acceptor of mux L2CAP connection.
 * @return uint8_t constructed DLCI value for local port.
 * @note Input port out of [1,30] is undefined behaviour, caller should validate before call.
 */
inline uint8_t make_dlci_for_local_port( uint8_t a_port_on_local, bool a_local_is_multiplexer_l2cap_initiator )
{
    // Target port is on local side:
    // If local is mux initiator -> local is initiator -> D=1
    // If local is mux acceptor -> local is acceptor -> D=0
    uint8_t d_bit = a_local_is_multiplexer_l2cap_initiator ? 1U : 0U;
    return static_cast<uint8_t>( ( a_port_on_local << 1 ) | d_bit );
}

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

    /*Set the P/F bits as specified in the explanatory note at the end of section 10.3.5 of RFCOMM document*/
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

/**
 * @class multiplexer_message
 * @brief RFCOMM multiplexer control message carried over DLCI=0
 *
 * This class represents MCC (Multiplexer Control Command) messages encapsulated
 * inside UIH frames on DLCI=0, which is dedicated for RFCOMM multiplexer control signaling.
 * DLCI=0 is reserved and shall not be used for user data DLC.
 *
 * Typical MCC commands include PN, MSC, RPN, TEST, NSC, FCS etc.
 * These control messages manage DLC creation, parameter negotiation, flow control,
 * link status and other multiplexer-level operations.
 */
class multipexer_message
{

public:

    virtual ~multipexer_message();

    /**
     * @brief Parse RFCOMM MCC multiplexer control message from raw buffer
     * MCC: Multiplexer Control Channel
     * Parses MCC message carried in UIH frame on DLCI=0 control channel.
     * The first byte of a_buffer is the MCC message type identifier,
     * which determines the specific command type: PN, MSC, RPN, TEST, NSC, FCS.
     *
     * @param a_buffer Pointer to raw MCC message payload.
     *                 The first byte is message type field.
     * @param a_size Total byte length of the buffer available for parsing.
     * @return std::shared_ptr<multiplexer_message>
     *         nullptr if parsing fails, buffer invalid or unknown message type.
     * @note a_buffer points to MCC payload (not including RFCOMM frame header).
     *       Caller retains ownership of a_buffer; this function does not take ownership.
     * @note Supported message types: PN, MSC, RPN, TEST, NSC, FCS.
     */
    static std::shared_ptr<multipexer_message> parse_from( uint8_t* a_buffer, uint16_t a_size );

    /**
     * @brief Get total raw buffer size required for RFCOMM MCC control message in UIH frame
     *
     * Return total memory region length for MCC control message on UIH frame.
     * Includes three parts:
     * 1. Type bytes field
     * 2. Length bytes field
     * 3. Value bytes field
     *
     * @return uint16_t total required buffer size (type + length + value)
     * @note Does NOT include UIH frame header / DLCI byte, only MCC message itself.
     */
    virtual uint16_t get_raw_buffer_size()
    {
        return 2;
    }

    /**
     * @brief Serialize RFCOMM MCC control message into provided buffer for UIH frame
     *
     * Writes full MCC message content to the memory pointed by a_buffer.
     * The serialized data consists of three parts:
     * 1. Type byte
     * 2. Length byte(s)
     * 3. Value payload bytes
     *
     * @param a_buffer Destination buffer to store serialized MCC message
     * @param a_size Total available byte count of a_buffer
     * @note Caller must guarantee a_buffer has enough space (use get_raw_buffer_size() to check).
     *       This method writes only MCC message itself; UIH frame header and DLCI byte are NOT included.
     */
    virtual void to_buffer( uint8_t* a_buffer, uint16_t a_size );

    multipexer_message_type get_type()const
    {
        return m_type;
    }

    uint16_t get_length()const
    {
        return m_message_length;
    }

    void set_is_command_in_uih( bool a_is_command )
    {
        m_is_command_in_uih = a_is_command;
    }

    bool is_command_in_uih()const
    {
        return m_is_command_in_uih;
    }

protected:

    /**
     * @brief Parse payload of UIH frame on RFCOMM DLC 0 (multiplexer control channel)
     *
     * Parse the detail value part of RFCOMM MCC message carried by UIH frame on control channel(DLC=0).
     * For example, the 8-byte body of PN message: a_buffer[0] is the first byte which contains DLCI field.
     *
     * @param a_buffer Pointer to start of MCC message content.
     * @param a_size Total byte length available in a_buffer.
     * @return bool true if parse success, false for invalid / malformed data.
     *
     * @note This function only processes MCC message body, does not parse UIH frame header.
     */
    virtual bool parse_detail( uint8_t* a_buffer, uint32_t a_size )
    {
        return true;
    }

    multipexer_message_type m_type = multipexer_message_type::pn;
    uint16_t m_message_length = 0x00;
    /**
     * @brief C/R bit from the first type octet of RFCOMM UIH frame (MCC message header)
     *
     * C/R bit indicates whether this MCC frame is a Command or a Response:
     * - true: Command frame
     * - false: Response frame
     * This bit is extracted from the type byte of MCC message on DLCI=0 UIH frame.
     */
    bool m_is_command_in_uih = false;
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
    /**
     * @brief RFCOMM DLC priority
     * Valid value range is 0 to 63.
     * A smaller numeric value indicates higher priority.
     */
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

// rfcomm multipexer UIH non support command response
class multipexer_nsc_message : public multipexer_message
{

public:

    multipexer_nsc_message();

    void set_command_type_to_reply( multipexer_message_type a_command_type )
    {
        m_command_type_to_reply = a_command_type;
    }

    multipexer_message_type get_command_type_to_reply()const
    {
        return m_command_type_to_reply;
    }

    void set_is_command_to_reply( bool a_is_command = true )
    {
        m_is_command_to_reply = a_is_command;
    }

    uint16_t get_raw_buffer_size() override;

    void to_buffer( uint8_t* a_buffer, uint16_t a_size )override;

protected:

    bool parse_detail( uint8_t* a_buffer, uint32_t a_size )override;

private:

    multipexer_message_type m_command_type_to_reply;
    bool m_is_command_to_reply = true;
};

}

