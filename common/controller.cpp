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

#include "controller.h"
#include "framework/log_util.h"

#include "framework/internal/platform.h"

#include <unordered_map>

namespace bluetooth
{

/**
 * the hci command position in supported command table.
 * see core spec, vol 4, part e, section 5.27.
 * For example, HCI_Remote_Name_Request command's postion value
 * is 2 * 8 + 3.
 */
namespace
{

static std::unordered_map<hci_command, uint16_t> s_support_cmds_pos;

void initialize_support_cmd_pos();

static const char* s_defult_local_name = "BluetoothDevice";

}

controller::controller()
{
    set_name( s_information_name );
    initialize_support_cmd_pos();
    m_local_name = framework::convert( s_defult_local_name );
}

bool controller::support_lmp_feature( local_features a_feature )const
{
    uint8_t feature = static_cast<uint8_t>( a_feature );
    uint8_t featureByte;
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

    std::shared_lock<std::shared_mutex> locker( m_mutex );
    if( feature < 64 )
    {
        /* Return the local device's feature */
        featureByte = ( feature / 8 );
        return ( 0 != ( m_lmp_support_features[featureByte] & ( power2[feature % 8] ) ) );
    }
    else if( ( feature >= 64 ) && ( feature < 128 ) )
    {
        featureByte = ( ( feature - 64 ) / 8 );
        return ( 0 != ( m_lmp_support_ext_features[0][featureByte] & ( power2[feature % 8] ) ) );
    }
    else
    {
        if( ( feature >= 128 ) && ( feature < 192 ) )
        {
            featureByte = ( ( ( feature - 128 ) / 8 ) % 8 );
            return ( 0 != ( m_lmp_support_ext_features[1][featureByte] & ( power2[feature % 8] ) ) );
        }
        else
        {
            return false;
        }
    }
    return true;
}

bool controller::controller_support_cmd( hci_command a_cmd )const
{
    bool ret = false;
    uint16_t pos = 0x00;
    auto it = s_support_cmds_pos.find( a_cmd );
    if( it == s_support_cmds_pos.end() )
    {
        LogUtilError() << "Please add postion for command " << static_cast< uint16_t >( a_cmd );
        std::abort();
    }
    pos = it->second;
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    uint8_t cmd_byte = ( pos ) / 8;
    uint8_t cmd_pos = ( pos ) % 8;

    std::shared_lock<std::shared_mutex> locker( m_mutex );
    ret = ( 0 != ( m_support_cmds[cmd_byte] & ( power2[cmd_pos] ) ) );
    return ret;
}

bool controller::update_controller_support_cmd( hci_command a_cmd, bool a_supported )
{
    uint16_t pos = 0x00;
    auto it = s_support_cmds_pos.find( a_cmd );
    if( it == s_support_cmds_pos.end() )
    {
        LogUtilError() << "Please add postion for command " << static_cast<uint16_t>( a_cmd );
        std::abort();
        return false;
    }

    pos = it->second;
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    uint8_t cmd_byte = ( pos ) / 8;
    uint8_t cmd_pos = ( pos ) % 8;

    std::lock_guard<std::shared_mutex> locker( m_mutex );
    uint8_t& cmd_byte_detail = m_support_cmds[cmd_byte];
    if( a_supported )
    {
        cmd_byte_detail |= power2[cmd_pos];
    }
    else
    {
        uint8_t revert = ~( power2[cmd_pos] );
        cmd_byte_detail &= revert;
    }

    return true;
}

void controller::set_local_name( std::u8string a_name )
{

}

void controller::update_lmp_ext_features( uint8_t a_page, std::vector<uint8_t> a_support_features )
{
    if( 0x00 == a_page )
    {
        LogUtilError() << "page number must start with 1!";
        return;
    }

    uint8_t page_index = a_page - 1; /* page number start with 1, but the array start with index 0 */
    if( page_index > 1 )
    {
        // currently the spec only support two ext pages.
        LogUtilError() << "parse more exe pages";
        return;
    }

    std::lock_guard<std::shared_mutex> locker( m_mutex );
    if( m_lmp_support_ext_features.size() < 2 )
    {
        m_lmp_support_ext_features.resize( 2 );
    }
    m_lmp_support_ext_features[page_index] = a_support_features;
}

void controller::update_le_buffer_size
    (
    uint16_t a_le_acl_size,
    uint8_t a_le_total_size
    )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    if( a_le_acl_size == 0x0000 )
    {
        m_le_acl_size = m_chip_acl_packet_size;
    }
    else
    {
        m_le_acl_size = a_le_acl_size;
    }

    if( a_le_total_size == 0x0000 )
    {
        m_le_total_size = static_cast<uint8_t>( m_total_acl_packet_size );
    }
    else
    {
        m_le_total_size = a_le_total_size;
    }
}

void controller::update_le_buffer_size
    (
    uint16_t a_le_acl_size,
    uint8_t a_le_total_size,
    uint16_t a_le_iso_size,
    uint8_t a_le_iso_total_size
    )
{
    update_le_buffer_size( a_le_acl_size, a_le_total_size );
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    m_le_iso_size = a_le_iso_size;
    m_le_iso_total_size = a_le_iso_total_size;
}

bool controller::support_ll_feature( le_ll_feature a_feature )const
{
    bool ret = false;
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    ret = ( 0 != ( m_support_le_features[static_cast< uint8_t >( a_feature ) / 8]
        & ( power2[static_cast<uint8_t>( a_feature ) % 8] ) ) );
    return ret;
}

bool controller::support_ll_state( le_states_combinations a_state )const
{
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    uint8_t state = static_cast< uint8_t >( a_state );

    std::shared_lock<std::shared_mutex> locker( m_mutex );
    bool ret = ( 0 != ( m_support_le_states[state / 8] & ( power2[state % 8] ) ) );
    return ret;
}

namespace
{

/**
 * Make the hci command position in supported command list. see core spec,vol 4, part E, section 6.27
 * a_bute_post: the byte number; a_bit_pos: the bit position, start with 0.
 */
constexpr uint16_t make_cmd_pos( uint8_t a_byte_pos, uint8_t a_bit_pos )
{
    return a_byte_pos * 8 + a_bit_pos;
}

/**
 * the hci command position in supported command list. see core spec,vol 4, part E, section 6.27
 */
void initialize_support_cmd_pos()
{
    s_support_cmds_pos[hci_command::hci_le_read_local_p_256_public_key] = make_cmd_pos( 34, 1 );
    s_support_cmds_pos[hci_command::hci_le_le_generate_dhkey_v1] = make_cmd_pos( 34, 2 );
    s_support_cmds_pos[hci_command::hci_le_le_extended_create_connection] = make_cmd_pos( 37, 7 );
    s_support_cmds_pos[hci_command::hci_le_le_generate_dhkey_v2] = make_cmd_pos( 41, 2 );
    s_support_cmds_pos[hci_command::hci_le_read_buffer_size_v2] = make_cmd_pos( 41, 5 );
    s_support_cmds_pos[hci_command::hci_write_voice_setting] = make_cmd_pos( 9, 3 );
    s_support_cmds_pos[hci_command::hci_read_scan_enable] = make_cmd_pos( 7, 6 );
}

}
}

