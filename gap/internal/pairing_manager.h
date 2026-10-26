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
#include <string>
#include <memory>
#include <vector>
#include <bitset>
#include <tuple>

#include "data_element.h"
#include "bluetooth_address.h"

namespace bluetooth
{

struct pairing_cb
{
    constexpr static uint16_t s_confirm_value_flag = 0x00;
    constexpr static uint16_t s_name_value_flag = 0x01;

    bluetooth_address m_remote_device;
    uint8_t m_remote_io_cap = 0x00;
    uint8_t m_remote_has_oob = 0x00;
    uint8_t m_remote_mitm_require = 0x00;
    bool m_to_remove = false;

    void reset();

    void set_confirm_value( uint32_t a_confirm_value );

    void set_remote_name( std::u8string a_name );

    std::tuple<bool, std::u8string> get_remote_name();

    std::tuple<bool, uint32_t> get_confirm_value();

private:

    uint32_t m_confirm_value = 0xFFFFFFFF;
    std::u8string m_device_name;
    std::bitset<sizeof( uint16_t ) * 8> m_flags;
};

class pairing_manager
{

public:

    pairing_manager();

    void set_attach_module( std::string const& a_moudle );

    void initialize();

    void set_pairable( bool a_pairable = true );

    void device_name_requested
        (
        bluetooth_address a_address,
        std::u8string a_name
        );

    void accept_ssp_confrim
        (
        bluetooth_address a_address,
        bool a_accept
        );

private:

    void handle_hci_event( std::shared_ptr<hci_data> const& a_hci_event );

    std::shared_ptr<hci_data> handle_io_rsp( std::vector<uint8_t> const& a_event );

    std::shared_ptr<hci_data> handle_io_req( std::vector<uint8_t> const& a_event );

    std::shared_ptr<hci_data> handle_ssp_completed( std::vector<uint8_t> const& a_event );

    std::shared_ptr<hci_data> handle_user_confirm( std::vector<uint8_t> const& a_event );

    std::shared_ptr<hci_data> handle_link_key_notify( std::vector<uint8_t> const& a_event );

    std::shared_ptr<hci_data> handle_link_key_request( std::vector<uint8_t> const& a_event );

    void add_trust_device( bluetooth_address a_address, std::vector<uint8_t> a_link_key, uint8_t a_key_type );

    void notify_pairing_request( bluetooth_address a_address );

    void delete_expired_pairing_cb();

    std::string m_attached_module;
    bool m_pairable = false;
    std::vector<pairing_cb> m_pairing_cbs;
    uint32_t m_timer_id = 0x00;
};

}

