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

namespace bluetooth
{

class acl_header
{

public:

    virtual ~acl_header();

    void set_acl_handle( uint16_t a_handle )
    {
        m_connection_handle = a_handle;
        make_l2cap_first_field( m_first_two_bytes );
    }

    uint16_t const& get_acl_handle()const
    {
        return m_connection_handle;
    }

    /**
    * Set the ACL connection's packet boundary flag value.
    */
    void set_packet_boundary( uint8_t a_pb_flag );

    /**
     * Set the ACL connection's broadcast flag
     */
    void set_broadcast_flag( uint8_t a_bc_flag );

    /**
     * set the length field value.
     */
    virtual void set_sdu_length( uint16_t a_length );

    virtual void to_raw_buffer( uint8_t* a_buffer, uint32_t a_size );

    virtual uint16_t header_size()const;

    virtual void parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size );

private:

    void make_l2cap_first_field( uint8_t* a_buffer );

    uint16_t m_connection_handle = 0x0000;
    uint8_t m_pb_flag = 0b00;    // packet boundary flag value
    uint8_t m_bc_flag = 0b00;    // broadcast flag value
    uint8_t m_first_two_bytes[2];// The first two bytes of the ACL packet to send.
    uint16_t m_length_in_acl = 0x0000;
};

class l2cap_header : public acl_header
{

public:

    void set_sdu_length( uint16_t a_length )override;

    void to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )override;

    uint16_t header_size()const override;

    void set_channel_id( uint16_t a_channel_id )
    {
        m_channel_id = a_channel_id;
    }

    uint16_t const& get_channel_id()const
    {
        return m_channel_id;
    }

    void parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )override;

private:

    uint16_t m_length_in_l2cap = 0x0000;
    uint16_t m_channel_id = 0x0000;
};

}

