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

#include "hci_command_maker.h"
#include "endian_convert.h"

namespace bluetooth
{

std::shared_ptr<hci_data> make_no_params_cmd( hci_command a_cmd )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100];
    write_le16( buff, static_cast< uint16_t >( a_cmd ) );
    buff[2] = 0x00;
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 3 );
    return hci;
}

std::shared_ptr<hci_data> make_host_buffer_size
    (
    uint16_t a_host_acl_packet_size,
    uint8_t a_host_sco_packet_size,
    uint16_t a_host_total_acl_packet_size,
    uint16_t a_host_total_sco_packet_size
    )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100];
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_host_buffer_size ) );
    buff[2] = 0x07;
    write_le16( buff + 3, a_host_acl_packet_size );
    buff[5] = a_host_sco_packet_size;
    write_le16( buff + 6, a_host_total_acl_packet_size );
    write_le16( buff + 8, a_host_total_sco_packet_size );
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 10 );
    return hci;
}

std::shared_ptr<hci_data> make_write_le_host_support( bool a_host_support_le )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100];
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_write_le_host_support ) );
    buff[2] = 0x02;
    buff[3] = a_host_support_le ? 0x01 : 0x00;
    buff[4] = 0x00;
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 5 );
    return hci;
}

std::shared_ptr<hci_data> make_le_set_event_mask( std::shared_ptr<controller> const& a_local_controller )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_le_set_event_mask ) );
    buff[2] = 0x08;
    uint8_t* event_buf = buff + 3;
    event_buf[0] = 0x1F;
    if( a_local_controller->support_ll_feature( le_ll_feature::ll_connection_params_req ) )
    {
        /* Allow the LE Remote Connection Parameter Request Event */
        event_buf[0] |= 0x20;
    }

    if( a_local_controller->support_ll_feature( le_ll_feature::ll_DATA_LENGTH_EXT ) )
    {
        /* Allow the LE Data Length Change Event */
        event_buf[0] |= 0x40;
    }

    if( a_local_controller->controller_support_cmd( hci_command::hci_le_read_local_p_256_public_key ) )
    {
        /* Allow the LE Read Local P-256 Public Key Complete Event */
        event_buf[0] |= 0x80;
    }

    if( a_local_controller->controller_support_cmd( hci_command::hci_le_le_generate_dhkey_v1 ) ||
        a_local_controller->controller_support_cmd( hci_command::hci_le_le_generate_dhkey_v2 ) )
    {
        /* Allow the LE Generate DHKey Complete Event */
        event_buf[1] |= 0x01;
    }

    if( a_local_controller->support_ll_feature( le_ll_feature::ll_LINK_LAYER_PRIVACY ) )
    {
        /* Allow the LE Enhanced Connection Complete event and the
         * LE Direct Advertising Report Event
         */
        event_buf[1] |= 0x06;
    }

    if( a_local_controller->controller_support_cmd( hci_command::hci_le_le_extended_create_connection ) )
    {
        /* LE Extended Create Connection Command is supported so enable
         * the Enhanced Connection Complete event
         */
        event_buf[1] |= 0x02;
    }

    if( a_local_controller->support_ll_feature( le_ll_feature::ll_2M_PHY ) ||
        a_local_controller->support_ll_feature( le_ll_feature::ll_CODED_PHY ) )
    {
        /* Allow the LE PHY update Complete Event */
        event_buf[1] |= 0x08;
    }

    if( a_local_controller->support_ll_feature( le_ll_feature::ll_EXT_ADVERT ) )
    {
        /* Allow the LE Extended Advertising Report Event */
        event_buf[1] |= 0x10;

        /* Allow the extended advertising Set terminated event and the
         * LE Scan Request Received Event */
        event_buf[2] |= 0x06;
    }

    if( a_local_controller->support_ll_feature( le_ll_feature::ll_PERIODIC_ADVERT ) )
    {
        /* Allow the LE Periodic Adverising Sync Established Event, Periodic Advertising
         * report event, and the periodic adveritsing sync loss event. */
        event_buf[1] |= 0xE0;
    }

    if( a_local_controller->support_ll_feature( le_ll_feature::ll_CHANNEL_SEL_ALGORITHM_2 ) )
    {
        /* Allow the LE Channel Selection Algorithm Event. */
        event_buf[2] |= 0x08;
    }

    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 11 );
    return hci;
}

std::shared_ptr<hci_data> make_cmd( hci_command a_cmd, uint8_t a_para )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( a_cmd ) );
    buff[2] = 0x01;
    buff[3] = a_para;
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 4 );
    return hci;
}

std::shared_ptr<hci_data> make_cmd( hci_command a_cmd, uint16_t a_para )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( a_cmd ) );
    buff[2] = 0x02;
    write_le16( buff + 3, a_para );
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 5 );
    return hci;
}

std::shared_ptr<hci_data> make_accept_connection( bluetooth_address a_address )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_accept_connection_request ) );
    buff[2] = 0x07;
    memcpy( buff + 3, a_address.address, bluetooth_address::s_bluetooth_address_size );
    buff[9] = 0x01; // We do not care which role after connected right now.
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 10 );
    return hci;
}

std::shared_ptr<hci_data> make_scan_mode( bool a_discovery, bool a_connectable )
{
    uint8_t value = 0x00;
    if( a_discovery && a_connectable )
    {
        value = 0x03;
    }
    else if( a_discovery && !a_connectable )
    {
        value = 0x01;
    }
    else if( !a_discovery && a_connectable )
    {
        value = 0x02;
    }

    std::shared_ptr<hci_data> hci;
    hci = make_cmd( hci_command::hci_write_scan_enable, value );
    return hci;
}

std::shared_ptr<hci_data> make_set_local_name( std::u8string const& a_name )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.resize( 248 + 1 + 2 );
    uint8_t* buff = hci->m_buffer.data();
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_write_local_name ) );
    buff[2] = 248;
    memcpy( buff + 3, a_name.c_str(), a_name.size() < 248 ? a_name.size() : 247 );
    return hci;
}

std::shared_ptr<hci_data> make_inquiry_event_filter()
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_set_event_filter ) );
    buff[2] = 0x02;
    buff[3] = 0x01;
    buff[4] = 0x00;
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 5 );
    return hci;
}

std::shared_ptr<hci_data> make_inquiry( uint8_t a_timeout )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_inquiry ) );
    buff[2] = 0x05;
    buff[3] = 0x33;
    buff[4] = 0x8B;
    buff[5] = 0x9E;
    buff[6] = a_timeout;
    buff[7] = 0x00;
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 8 );
    return hci;
}

std::shared_ptr<hci_data> make_event_mask()
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_set_event_mask ) );
    buff[2] = 0x08;
    buff[3] = 0xFF;
    buff[4] = 0xFF;
    buff[5] = 0xFF;
    buff[6] = 0xFF;
    buff[7] = 0xFF;
    buff[8] = 0xFF;
    buff[9] = 0xFF;
    buff[10] = 0x3F;
    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 11 );
    return hci;
}

std::shared_ptr<hci_data> make_read_remote_name
    (
    bluetooth_address a_address,
    uint8_t a_page_scan_rsp_mode,
    uint16_t a_clock_offset
    )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );
    uint8_t buff[100] = { 0 };
    write_le16( buff, static_cast< uint16_t >( hci_command::hci_remote_name_request ) );

    buff[2] = 0x0A;
    memcpy( buff + 3, a_address.address, bluetooth_address::s_bluetooth_address_size );

    buff[9] = a_page_scan_rsp_mode;
    buff[10] = 0x00;

    write_le16( buff + 11, a_clock_offset );

    if( a_clock_offset != 0 )
    {
        buff[12] |= 0x80;
    }

    hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 13 );

    return hci;
}

std::shared_ptr<hci_data> make_link_key_reply
    (
    bluetooth_address a_address,
    std::vector<uint8_t>const& a_link_key
    )
{
    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );

    uint8_t buff[100] = { 0 };
    if( a_link_key.empty() )
    {
        write_le16( buff, static_cast< uint16_t >( hci_command::hci_link_key_request_negative_reply ) );
        buff[2] = 0x06;
        memcpy( buff + 3, a_address.address, bluetooth_address::s_bluetooth_address_size );
        hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 9 );
    }
    else
    {
        write_le16( buff, static_cast< uint16_t >( hci_command::hci_link_key_request_reply ) );
        buff[2] = 0x16;
        memcpy( buff + 3, a_address.address, bluetooth_address::s_bluetooth_address_size );
        memcpy( buff + 9, a_link_key.data(), a_link_key.size() );
        hci->m_buffer.insert( hci->m_buffer.end(), buff, buff + 0x19 );
    }

    return hci;
}

hci_command extract_cmd( std::shared_ptr<hci_data> const& a_hci )
{
    if( a_hci->m_type != uart_hci_type::command_type )
    {
        return hci_command::hci_invalid;
    }

    uint16_t cmd = be_to_host16( a_hci->m_buffer.data() );
    return static_cast< hci_command >( cmd );
}

}

