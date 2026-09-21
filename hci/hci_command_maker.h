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

#include "data_element.h"
#include "hci_defs.h"
#include "bluetooth_address.h"
#include "../common/controller.h"

#include <memory>
#include <string>

namespace bluetooth
{

std::shared_ptr<hci_data> make_no_params_cmd( hci_command a_cmd );

std::shared_ptr<hci_data> make_cmd( hci_command a_cmd, uint8_t a_para );

std::shared_ptr<hci_data> make_cmd( hci_command a_cmd, uint16_t a_para );

std::shared_ptr<hci_data> make_accept_connection( bluetooth_address a_address );

inline std::shared_ptr<hci_data> make_write_voice_setting( uint16_t a_vs )
{
    return make_cmd( hci_command::hci_write_voice_setting, a_vs );
}

std::shared_ptr<hci_data> make_host_buffer_size
    (
    uint16_t a_host_acl_packet_size,
    uint8_t a_host_sco_packet_size,
    uint16_t a_host_total_acl_packet_size,
    uint16_t a_host_total_sco_packet_size
    );

inline std::shared_ptr<hci_data> make_read_local_extended_features( uint8_t a_page )
{
    return make_cmd( hci_command::hci_read_local_extended_features, a_page );
}

std::shared_ptr<hci_data> make_write_le_host_support( bool a_host_support_le );

std::shared_ptr<hci_data> make_le_set_event_mask( std::shared_ptr<controller> const& a_local_controller );

inline std::shared_ptr<hci_data> make_page_timeout_cmd( uint16_t a_page_out )
{
    if( a_page_out < 1 )
    {
        // at least 1
        a_page_out = 1;
    }

    return make_cmd( hci_command::hci_write_page_timeout, a_page_out );
}

std::shared_ptr<hci_data> make_scan_mode( bool a_discovery, bool a_connectable );

std::shared_ptr<hci_data> make_set_local_name( std::u8string const& a_name );

std::shared_ptr<hci_data> make_inquiry_event_filter();

std::shared_ptr<hci_data> make_inquiry( uint8_t a_timeout );

std::shared_ptr<hci_data> make_event_mask();

std::shared_ptr<hci_data> make_read_remote_name
        (
        bluetooth_address a_address,
        uint8_t a_page_scan_rsp_mode,
        uint16_t a_clock_offset
        );

/**
 * if a_link_key empty or size wrong, then will reject link key request;
 * otherwise will accept the link key request
 */
std::shared_ptr<hci_data> make_link_key_reply
    (
    bluetooth_address a_address,
    std::vector<uint8_t>const& a_link_key
    );

hci_command extract_cmd( std::shared_ptr<hci_data> const& a_hci );

}

