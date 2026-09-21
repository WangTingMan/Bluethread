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

#include "rfcomm_protocol.h"

#include "framework/log_util.h"
#include "endian_convert.h"

#include <bitset>

#define CREDIT_FLAG_VALUE_IN_REQUEST 0xF
#define CREDIT_FLAG_VALUE_IN_RESPONSE 0xE

namespace bluetooth
{

/* CRC table from TS 7.10 specification */
const uint8_t rfcomm_crctable[] = {
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19, 0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
};

uint8_t rfcomm_calc_fcs( uint8_t* p, uint16_t len )
{
    uint8_t fcs = 0xFF;

    while( len-- )
    {
        fcs = rfcomm_crctable[fcs ^ *p++];
    }

    /* Ones compliment */
    return ( 0xFF - fcs );
}

bool rfcomm_check_fcs( uint8_t* p, uint16_t len, uint8_t received_fcs )
{
    uint8_t fcs = 0xFF;

    while( len-- )
    {
        fcs = rfcomm_crctable[fcs ^ *p++];
    }

    /* Ones compliment */
    fcs = rfcomm_crctable[fcs ^ received_fcs];

    /*0xCF is the reversed order of 11110011.*/
    return ( fcs == 0xCF );
}

std::ostream& operator<<( std::ostream& a_os, rfcomm_frame_type a_type )
{
    switch( a_type )
    {
    case bluetooth::rfcomm_frame_type::sabm:
        a_os << "asbm";
        break;
    case bluetooth::rfcomm_frame_type::ua:
        a_os << "ua";
        break;
    case bluetooth::rfcomm_frame_type::dm:
        a_os << "dm";
        break;
    case bluetooth::rfcomm_frame_type::disc:
        a_os << "disc";
        break;
    case bluetooth::rfcomm_frame_type::uih:
        a_os << "uih";
        break;
    case bluetooth::rfcomm_frame_type::ui:
        a_os << "ui";
        break;
    default:
        a_os << "unknown";
        break;
    }
    return a_os;
}

std::ostream& operator<<( std::ostream& a_os, multipexer_message_type a_type )
{
    switch( a_type )
    {
    case bluetooth::multipexer_message_type::pn:
        a_os << "pn";
        break;
    case bluetooth::multipexer_message_type::test:
        a_os << "test";
        break;
    case bluetooth::multipexer_message_type::fc_on:
        a_os << "fc_on";
        break;
    case bluetooth::multipexer_message_type::fc_off:
        a_os << "fc_off";
        break;
    case bluetooth::multipexer_message_type::msc:
        a_os << "msc";
        break;
    case bluetooth::multipexer_message_type::nsc:
        a_os << "nsc";
        break;
    case bluetooth::multipexer_message_type::rpn:
        a_os << "rpn";
        break;
    case bluetooth::multipexer_message_type::rls:
        a_os << "rls";
        break;
    default:
        a_os << "unknown: " << static_cast< uint16_t >( a_type );
        break;
    }
    return a_os;
}

uint8_t rfcomm_header::get_min_header_size()
{
    return l2cap_header::header_size() + 3;
}

void rfcomm_header::set_channel_number( uint8_t a_ch_num )
{
    m_address_field |= a_ch_num << 4;
    set_ea_bit();
}

uint8_t rfcomm_header::get_channel_number()
{
    return m_address_field >> 3;
}

void rfcomm_header::set_cr( bool a_cr_value )
{
    if( a_cr_value )
    {
        m_address_field |= 0x02;
    }
    else
    {
        m_address_field &= 0xFD;
    }
    set_ea_bit();
}

bool rfcomm_header::cr_value_in_header()const
{
    return ( m_address_field & 0x02 ) == 0x02;
}

void rfcomm_header::set_frame_type( rfcomm_frame_type a_type )
{
    bool pf = poll_final_set();
    m_control_field = static_cast<uint8_t>( a_type );
    set_poll_final( pf );
}

rfcomm_frame_type rfcomm_header::get_frame_type()const
{
    uint8_t temp = m_control_field & 0xEF;
    return static_cast<rfcomm_frame_type>( temp );
}

bool rfcomm_header::poll_final_set()const
{
    return ( m_control_field & 0x10 ) > 0x00;
}

void rfcomm_header::set_poll_final( bool a_pf )
{
    if( a_pf )
    {
        m_control_field |= 0x10;
    }
    else
    {
        m_control_field &= 0xEF;
    }
}

void rfcomm_header::set_sdu_length( uint16_t a_length )
{
    uint16_t rfcomm_pdu_length = 3 + a_length;

    if( a_length < 0x7F )
    {
        rfcomm_pdu_length += 1;
        m_length[0] = static_cast<uint8_t>( a_length ) << 1;
        m_length[0] |= 0x01;
    }
    else
    {
        write_le16( m_length, a_length );
        m_length[1] <<= 1;
        bool msb_in_first = ( ( m_length[0] & 0x80 ) == 0x80 );
        if( msb_in_first )
        {
            m_length[1] |= 0x01;
        }
        m_length[0] <<= 1;
        m_length[0] &= 0xFE;
        rfcomm_pdu_length += 2;
    }

    l2cap_header::set_sdu_length( rfcomm_pdu_length );
}

uint16_t rfcomm_header::get_information_length()
{
    bool length_ext = false;
    length_ext = ( 0x00 == ( m_length[0] & 0x01 ) );
    if( !length_ext )
    {
        return m_length[0] >> 1;
    }

    uint16_t length = le_to_host16( m_length );
    length >>= 1;
    return length;
}

uint16_t rfcomm_header::header_size()const
{
    bool length_ext = false;
    length_ext = ( 0x00 == ( m_length[0] & 0x01 ) );
    uint8_t length_size = ( length_ext ? 4 : 3 );
    uint16_t ret = length_size + l2cap_header::header_size();

    if( rfcomm_frame_type::uih != get_frame_type() )
    {
        // Only UIH frame may have credit increase value
        return ret;
    }

    if( poll_final_set() &&
        ( 0x00 != get_dlci() ) )
    {
        // only user data in UIH may has the credit filed. So here we add one credit byte size
        return ret + 1;
    }

    return ret;
}

void rfcomm_header::parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )
{
    l2cap_header::parse_from_raw_data( a_buffer, a_size );

    uint8_t* p_offset = a_buffer + l2cap_header::header_size();
    m_address_field = p_offset[0];
    m_control_field = p_offset[1];
    m_length[0] = p_offset[2];
    m_length[1] = p_offset[3];

    if( rfcomm_frame_type::uih == get_frame_type() &&
        poll_final_set() )
    {
        // Only UIH frame may have credit increase value
        bool length_ext = false;
        length_ext = ( 0x00 == ( m_length[0] & 0x01 ) );
        if( length_ext )
        {
            m_credit_increase_value = p_offset[4];
        }
        else
        {
            m_credit_increase_value = p_offset[3];
        }
    }
}

uint8_t rfcomm_header::get_credit_increase_value()
{
    if( rfcomm_frame_type::uih != get_frame_type() )
    {
        // Only UIH frame may have credit increase value
        return 0;
    }

    if( poll_final_set() )
    {
        return m_credit_increase_value;
    }

    return 0;
}

void rfcomm_header::set_credit_increase_value( uint8_t a_value )
{
    if( rfcomm_frame_type::uih != get_frame_type() )
    {
        LogUtilError() << "UIH frame may have credit increase value";
        return;
    }

    if( 0x00 == a_value )
    {
        set_poll_final( false );
        return;
    }

    set_poll_final( true );
    m_credit_increase_value = a_value;
}

void rfcomm_header::to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )
{
    if( header_size() > a_size )
    {
        LogUtilError() << "buffer size to small.";
        return;
    }

    uint8_t* p_buffer = a_buffer + l2cap_header::header_size();
    l2cap_header::to_raw_buffer( a_buffer, a_size );

    p_buffer[0] = m_address_field | 0x01;
    p_buffer[1] = m_control_field;

    bool length_ext = false;
    length_ext = ( 0x00 == ( m_length[0] & 0x01 ) );
    uint8_t* p_credit_pos = nullptr;
    if( !length_ext )
    {
        p_buffer[2] = m_length[0];
        p_credit_pos = p_buffer + 3;
    }
    else
    {
        p_buffer[2] = m_length[0];
        p_buffer[3] = m_length[1];
        p_credit_pos = p_buffer + 4;
    }

    if( rfcomm_frame_type::uih == get_frame_type() &&
        poll_final_set() )
    {
        // For UIH frame if the P/F bit set, then we need append the credit increase value
        *p_credit_pos = m_credit_increase_value;
    }
}

std::shared_ptr<multipexer_message> make( multipexer_message_type a_type )
{
    switch( a_type )
    {
    case bluetooth::multipexer_message_type::pn:
        return std::make_shared<multipexer_pn_message>();
    case bluetooth::multipexer_message_type::msc:
        return std::make_shared<multipexer_msc_message>();
    default:
        LogUtilError() << "To implementation type: " << a_type;
        break;
    }
    return nullptr;
}

multipexer_message::~multipexer_message()
{

}

std::shared_ptr<multipexer_message> multipexer_message::parse_from( uint8_t* a_buffer, uint16_t a_size )
{
    std::shared_ptr<multipexer_message> msg;
    if( a_size < 2 )
    {
        LogUtilError() << "buffer size not enough";
        return msg;
    }

    bool type_ea = ( 0x00 == ( a_buffer[0] & 0x01 ) );
    if( type_ea )
    {
        LogUtilError() << "The type field only has one byte.";
        return msg;
    }

    bool is_command = ( 0x02 == ( a_buffer[0] & 0x02 ) );
    multipexer_message_type type = static_cast<multipexer_message_type>( ( a_buffer[0] & 0xFC ) >> 2 );

    bool length_ea = ( 0x00 == ( a_buffer[1] & 0x01 ) );
    if( length_ea )
    {
        LogUtilError() << "Currently we only support one byte length field";
        return msg;
    }

    uint16_t message_length = a_buffer[1] >> 1;
    if( message_length + 2 > a_size )
    {
        LogUtilError() << "buffer size not enough";
        return msg;
    }

    msg = make( type );
    msg->m_is_command = is_command;
    msg->m_type = type;
    msg->m_message_length = message_length;

    if( msg )
    {
        bool ret = msg->parse_detail( a_buffer + 2, msg->m_message_length );
        if( !ret )
        {
            return nullptr;
        }
    }
    return msg;
}

void multipexer_message::to_buffer( uint8_t* a_buffer, uint16_t a_size )
{

}

multipexer_pn_message::multipexer_pn_message()
{
    m_type = multipexer_message_type::pn;
}

uint16_t multipexer_pn_message::get_raw_buffer_size()
{
    return 10;
}

void multipexer_pn_message::to_buffer( uint8_t* a_buffer, uint16_t a_size )
{
    if( a_size < 10 )
    {
        LogUtilError() << "need more memory to write.";
        return;
    }

    a_buffer[0] = static_cast< uint8_t >( multipexer_message_type::pn ) << 2;
    a_buffer[0] |= 0x01; // set EA bit
    if( m_is_command )
    {
        a_buffer[0] |= 0x02; // set bit 2, C/R bit
    }
    else
    {
        a_buffer[0] &= 0xFD; // clear bit 2, C/R bit
    }

    a_buffer[1] = 0x08 << 1; // PN message's length is 8.
    a_buffer[1] |= 0x01; // set EA bit

    a_buffer[2] = m_dlci & 0x3F;
    a_buffer[3] = CREDIT_FLAG_VALUE_IN_RESPONSE << 4;
    a_buffer[4] = 0x00;
    a_buffer[5] = 0x00;
    write_le16( a_buffer + 6, m_max_frame_size );
    a_buffer[8] = 0x00;
    a_buffer[9] = m_credit_init_value;

    if( m_credit_init_value > 0x07 )
    {
        LogUtilError() << "Credit init value must be less than 7";
        a_buffer[9] = 0x7;
    }
}

bool multipexer_pn_message::parse_detail( uint8_t* a_buffer, uint32_t a_size )
{
    if( a_size < 8 )
    {
        LogUtilError() << "buffer size not enough";
        return false;
    }

    m_dlci = a_buffer[0] & 0x3F;
    uint8_t cl_field = a_buffer[1] >> 4;
    if( m_is_command )
    {
        m_credit_supported = ( cl_field == CREDIT_FLAG_VALUE_IN_REQUEST );
    }
    else
    {
        m_credit_supported = ( cl_field == CREDIT_FLAG_VALUE_IN_RESPONSE );
    }

    m_priority = a_buffer[2] & 0x3F;
    m_max_frame_size = le_to_host16( a_buffer + 4 );
    m_credit_init_value = a_buffer[7] & 0x7;
    return true;
}

uint8_t multipexer_pn_message::get_port()
{
    return m_dlci >> 1;
}

multipexer_msc_message::multipexer_msc_message()
{
    m_type = multipexer_message_type::msc;
}

uint16_t multipexer_msc_message::get_raw_buffer_size()
{
    return 4;
}

void multipexer_msc_message::to_buffer( uint8_t* a_buffer, uint16_t a_size )
{
    if( a_size < 4 )
    {
        LogUtilError() << "need more memory to write.";
        return;
    }

    a_buffer[0] = static_cast< uint8_t >( multipexer_message_type::msc ) << 2;
    a_buffer[0] |= 0x01; // set EA bit
    if( m_is_command )
    {
        a_buffer[0] |= 0x02; // set bit 2, C/R bit
    }
    else
    {
        a_buffer[0] &= 0xFD; // clear bit 2, C/R bit
    }

    a_buffer[1] = 0x02 << 1; // PN message's length is 8.
    a_buffer[1] |= 0x01; // set EA bit

    a_buffer[2] = m_dlci << 2;
    a_buffer[2] |= 0x03;

    std::bitset<8> signals_;
    signals_.set( 0, true );
    signals_.set( 1, m_flow_control_on );
    signals_.set( 2, m_ready_communicated );
    signals_.set( 3, m_ready_received );
    signals_.set( 6, m_incoming_call );
    signals_.set( 7, m_data_valid );
    a_buffer[3] = static_cast<uint8_t>( signals_.to_ulong() );
}

bool multipexer_msc_message::parse_detail( uint8_t* a_buffer, uint32_t a_size )
{
    if( a_size < 2 )
    {
        LogUtilError() << "MSC message contains DLCI and V.24 signals at least";
        return false;
    }

    bool ea_in_dlci = ( ( a_buffer[0] & 0x01 ) == 0x01 );
    if( !ea_in_dlci )
    {
        LogUtilError() << "The EA bit in DLCI byte must be 1";
        return false;
    }

    m_dlci = a_buffer[0] >> 2;

    std::bitset<8> signals_( a_buffer[1] );
    m_flow_control_on = signals_.test( 1 );
    m_ready_communicated = signals_.test( 2 );
    m_ready_received = signals_.test( 3 );
    m_incoming_call = signals_.test( 6 );
    m_data_valid = signals_.test( 7 );
    return true;
}

}

