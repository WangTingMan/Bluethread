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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "data_element.h"

namespace bluetooth
{

enum class boundary_flag : uint8_t
{
    nonautomatically_flushable_start = 0b00,
    continuing_fragment = 0b01,
    automatically_flushable_start = 0b10,
    previously_used = 0b11
};

/**
 * @note Recombination layer applies resource protection limits
 * (per handle byte limit, fragment count limit, active block limit,
 * global byte limit and fragment timeout) to avoid memory exhaustion
 * caused by malformed or malicious fragmented packets.
 * Upper layer shall invoke clear_for_handle() when HCI disconnect event occurs.
 */
class l2cap_packet_recombinationer
{

    struct l2cap_packet_fragment
    {
        boundary_flag boundary = boundary_flag::automatically_flushable_start;
        std::shared_ptr<hci_data> fragment;
    };

    struct l2cap_recombination_block
    {
        uint16_t acl_handle = 0x00;
        int32_t total_pdu_size = 0x00;   // the total completed pdu size
        int32_t pdu_size_left = 0x00;    // the continue fragments size we wait
        int32_t bytes_received = 0x00;   // the l2cap payload bytes already buffered
        std::vector<l2cap_packet_fragment> fragments;
    };

public:

    /** Maximum l2cap pdu bytes allowed to buffer for one ACL handle. */
    static constexpr uint32_t s_max_reassembly_bytes_per_handle = 64u * 1024u;
    /** Maximum bytes buffered across all ACL handles. */
    static constexpr size_t s_max_total_reassembly_bytes = 512u * 1024u;
    /** Maximum fragments allowed for one recombination block. */
    static constexpr size_t s_max_fragments_per_handle = 64u;
    /** Maximum concurrent recombination blocks( one block per ACL handle ). */
    static constexpr size_t s_max_active_blocks = 32u;

    l2cap_packet_recombinationer();

    void handle_incoming_hci_packet( std::shared_ptr<hci_data> const& a_hci_data );

    /**
     * clear fragments for specific acl handle.
     * for example the ACL dispeared.
     */
    void clear_for_handle( uint16_t a_handle )
    {
        for( auto it = m_fragment_block.begin(); it != m_fragment_block.end(); )
        {
            if( it->acl_handle == a_handle )
            {
                release_block_bytes( *it );
                it = m_fragment_block.erase( it );
            }
            else
            {
                ++it;
            }
        }
    }

    void clear()
    {
        m_fragment_block.clear();
        m_total_buffered_bytes = 0;
    }

    void set_complete_l2cap_packet_reporter( std::function<void( std::shared_ptr<hci_data> )> a_complete_l2cap_packet_reporter )
    {
        m_complete_l2cap_packet_reporter = a_complete_l2cap_packet_reporter;
    }

private:

    bool l2cap_hci_packet_check( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_incoming_hci_packet_start
        (
        uint16_t a_handle,
        std::shared_ptr<hci_data> const& a_hci_data
        );

    void handle_incoming_hci_packet_continue
        (
        uint16_t a_handle,
        std::shared_ptr<hci_data> const& a_hci_data
        );

    void recombinate_and_report( std::vector<l2cap_recombination_block>::iterator a_block );

    void release_block_bytes( l2cap_recombination_block const& a_block )
    {
        if( static_cast<size_t>( a_block.bytes_received ) <= m_total_buffered_bytes )
        {
            m_total_buffered_bytes -= static_cast<size_t>( a_block.bytes_received );
        }
        else
        {
            m_total_buffered_bytes = 0;
        }
    }

    void default_l2cap_packet_reporter( std::shared_ptr<hci_data> );

    std::vector<l2cap_recombination_block> m_fragment_block;
    std::function<void(std::shared_ptr<hci_data>)> m_complete_l2cap_packet_reporter;
    size_t m_total_buffered_bytes = 0; // total bytes buffered by all recombination blocks
};

}

