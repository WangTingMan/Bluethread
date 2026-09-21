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
#include "..\framework\abstract_info.h"
#include "hci_defs.h"
#include "bluetooth_address.h"

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace bluetooth
{

class controller : public framework::abstract_information
{

public:

    constexpr static const char* s_information_name = "controller";

    controller();

    /**
     * Check if support specified link manager feature
     */
    bool support_lmp_feature( local_features a_feature )const;

    /**
     * Check if support specified hci command
     */
    bool controller_support_cmd( hci_command a_cmd)const;

    /**
     * For compatibility use. Some controller may declare that it support one hci command,
     * but it will return Unknown HCI Command for that command. So then it is time to invoke
     * this function to set it NOT supported.
     */
    bool update_controller_support_cmd( hci_command a_cmd, bool a_supported = true );

    bool support_ll_feature( le_ll_feature a_feature )const;

    /**
     * Check if support BLE link layer state or state combination
     */
    bool support_ll_state( le_states_combinations a_state )const;

    /*
    * Update the controller supported buffer size
    * Only can be invoked refer to the event returned from chipset
    */
    void update_buffer_size
        (
        uint16_t a_chip_acl_packet_size,
        uint8_t a_chip_sco_packet_size,
        uint16_t a_total_acl_packet_size,
        uint16_t a_total_sco_packet_size
        )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_chip_acl_packet_size = a_chip_acl_packet_size;
        m_chip_sco_packet_size = a_chip_sco_packet_size;
        m_total_acl_packet_size = a_total_acl_packet_size;
        m_total_sco_packet_size = a_total_sco_packet_size;
    }

    /**
     * Update the controller version information
     * Only can be invoked refer to the event returned from chipset
     */
    void update_version
        (
        uint8_t a_hci_version,
        uint8_t a_lmp_verson
        )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_hci_version = a_hci_version;
        m_lmp_verson = a_lmp_verson;
    }

    /**
     * Update local controller's bluetooth address.
     * The stack will not set the local bluetooth address.
     * So need to set the bluetooth address before chipset initialized.
     */
    void update_address( bluetooth_address a_address )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_address = a_address;
    }

    bluetooth_address get_address()const
    {
        std::shared_lock locker( m_mutex );
        return m_address;
    }

    /**
     * update local controller supported hci commands.
     */
    void update_support_cmds( std::vector<uint8_t> a_support_cmds )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_support_cmds = a_support_cmds;
    }

    /**
     * update local link manager supported features
     */
    void update_lmp_features( std::vector<uint8_t> a_support_cmds )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_lmp_support_features = a_support_cmds;
    }

    /**
     * update local link manager supported extented features
     */
    void update_lmp_ext_features( uint8_t a_page, std::vector<uint8_t> a_support_features );

    /**
     * upate local LE link layer supported features
     */
    void update_le_features( std::vector<uint8_t> a_support_le_features )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_support_le_features = a_support_le_features;
    }

    /**
     * update local link layer supported state and combined features
     */
    void update_le_states( std::vector<uint8_t> a_support_le_states )
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_support_le_states = a_support_le_states;
    }

    /**
     * update local LE supported white list size
     */
    void update_le_white_list_size( uint8_t a_size)
    {
        std::lock_guard<std::shared_mutex> locker( m_mutex );
        m_le_white_size = a_size;
    }

    /**
     * update LE buffer size
     */
    void update_le_buffer_size
        (
        uint16_t a_le_acl_size,
        uint8_t a_le_total_size
        );

    /**
     * update LE buffer size( supported 5.2 )
     */
    void update_le_buffer_size
        (
        uint16_t a_le_acl_size,
        uint8_t a_le_total_size,
        uint16_t a_le_iso_size,
        uint8_t a_le_iso_total_size
        );

    /**
     * set local device name
     */
    void set_local_name( std::u8string a_name );

    /**
     * get local device name
     */
    std::u8string const& get_local_name()const
    {
        std::shared_lock<std::shared_mutex> locker( m_mutex );
        return m_local_name;
    }

    uint8_t get_acl_total_credit_value()const
    {
        std::shared_lock<std::shared_mutex> locker( m_mutex );
        return m_total_acl_packet_size;
    }

private:

    mutable std::shared_mutex m_mutex;
    std::u8string m_local_name;

    uint16_t m_chip_acl_packet_size = 0u;
    uint8_t m_chip_sco_packet_size = 0u;
    uint16_t m_total_acl_packet_size = 0u;
    uint16_t m_total_sco_packet_size = 0u;
    uint16_t m_le_acl_size = 0;
    uint8_t m_le_total_size = 0;
    uint16_t m_le_iso_size = 0;
    uint8_t m_le_iso_total_size = 0;

    uint8_t m_hci_version = 0;
    uint8_t m_lmp_verson = 0;

    bluetooth_address m_address;

    std::vector<uint8_t> m_support_cmds;
    std::vector<uint8_t> m_lmp_support_features; /* return by HCI_Read_Local_Supported_Features */
    std::vector<std::vector<uint8_t>> m_lmp_support_ext_features; /* return by HCI_Read_Local_Extended_Features */
    std::vector<uint8_t> m_support_le_features;
    std::vector<uint8_t> m_support_le_states;
    uint8_t m_le_white_size = 0;
};

}

