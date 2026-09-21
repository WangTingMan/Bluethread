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

#include "l2cap_packet_recombinationer.h"
#include "endian_convert.h"
#include "data_element.h"

#include "framework/log_util.h"

namespace
{
    constexpr uint16_t s_acl_header_size = 4u;
    constexpr uint16_t s_l2cap_header_size = 4u;
    constexpr uint16_t s_acl_and_l2cap_header_size = s_acl_header_size + s_l2cap_header_size;
}

namespace bluetooth
{

enum class broadcast_flag : uint8_t
{
    point_to_point = 0,
    br_edr_broadcast = 0b01
};

l2cap_packet_recombinationer::l2cap_packet_recombinationer()
{
    m_complete_l2cap_packet_reporter = std::bind( &l2cap_packet_recombinationer::default_l2cap_packet_reporter,
        this, std::placeholders::_1 );
}

void l2cap_packet_recombinationer::handle_incoming_hci_packet( std::shared_ptr<hci_data> const& a_hci_data )
{
    if( !a_hci_data->m_from_controller )
    {
        LogUtilError() << "Not from controller's acl packet!";
        return;
    }

    if( !l2cap_hci_packet_check( a_hci_data ) )
    {
        LogUtilError() << "packet checking failed.";
        // TODO: what if we need to do something?
        return;
    }

    std::vector<uint8_t> const& buffer = a_hci_data->m_buffer;
    uint8_t handle[2];
    handle[0] = buffer[0];
    handle[1] = buffer[1] & 0x0F;
    uint16_t acl_handle = le_to_host16( handle );

    boundary_flag boundary( static_cast<boundary_flag>( ( buffer[1] >> 4 ) & 0x03 ) );
    broadcast_flag broadcast( static_cast<broadcast_flag>( ( buffer[1] >> 6 ) & 0x03 ) );

    // TODO: BR/EDR broadcast ACL packet is not supported for now.
    // If broadcast packet received, drop it directly.
    if( broadcast != broadcast_flag::point_to_point )
    {
        LogUtilWarning() << "BR/EDR broadcast ACL packet received, skip processing, handle:" << acl_handle;
        return;
    }

    /**
    * We require the hci packet's minimal size is 8 bytes:
    * 2 bytes used for ACL handle; 2 bytes used for ACL HCI payload size;
    * 2 bytes used for L2CAP payload size; 2 bytes used for channel ID size
    */
    constexpr uint16_t min_size_for_contain_l2cap_header = 8u;
    constexpr uint16_t min_size_for_continue = 5u;
    uint16_t min_size = min_size_for_contain_l2cap_header;

    switch( boundary )
    {
    case boundary_flag::automatically_flushable_start:
        min_size = min_size_for_contain_l2cap_header;
        if( a_hci_data->m_buffer.size() >= min_size )
        {
            handle_incoming_hci_packet_start( acl_handle, a_hci_data );
        }
        else
        {
            LogUtilError() << "Not enough data!";
            clear_for_handle( acl_handle );
            return;
        }
        break;
    case boundary_flag::nonautomatically_flushable_start:
        LogUtilError() << "Controller to Host packet cannot use nonautomatically_flushable_start, drop packet.";
        clear_for_handle( acl_handle );
        return;
    case boundary_flag::continuing_fragment:
        min_size = min_size_for_continue;
        if( a_hci_data->m_buffer.size() >= min_size )
        {
            handle_incoming_hci_packet_continue( acl_handle, a_hci_data );
        }
        else
        {
            LogUtilError() << "Not enough data!";
            clear_for_handle( acl_handle );
            return;
        }
        break;
    case boundary_flag::previously_used:
        LogUtilError() << "Not supported in latest spec.";
        clear_for_handle( acl_handle );
        return;
    default:
        LogUtilFatal() << "Should not go here";
    }
}

bool l2cap_packet_recombinationer::l2cap_hci_packet_check( std::shared_ptr<hci_data> const& a_hci_data )
{
    if( !a_hci_data )
    {
        LogUtilError() << "Empty hci data!";
        return false;
    }

    if( uart_hci_type::acl_type != a_hci_data->m_type )
    {
        LogUtilError() << "Not acl packet!";
        return false;
    }

    uint8_t const* p_packet = a_hci_data->m_buffer.data();
    p_packet += 2;
    uint16_t total_length = le_to_host16( p_packet );
    if( total_length + s_acl_header_size != a_hci_data->m_buffer.size() )
    {
        LogUtilError() << "total length cannot match buffer size!";
        return false;
    }

    return true;
}

void l2cap_packet_recombinationer::handle_incoming_hci_packet_start
    (
    uint16_t a_handle,
    std::shared_ptr<hci_data> const& a_hci_data
    )
{
    uint8_t const* p_packet = a_hci_data->m_buffer.data();
    p_packet += s_acl_header_size;

    int32_t l2cap_pdu_length = le_to_host16( p_packet );
    int32_t received_l2cap_bytes = static_cast<int32_t>( a_hci_data->m_buffer.size() ) - s_acl_and_l2cap_header_size;
    if( received_l2cap_bytes < 0 )
    {
        LogUtilError() << "Not enough data!";
        return;
    }

    if( l2cap_pdu_length <= received_l2cap_bytes )
    {
        m_complete_l2cap_packet_reporter( a_hci_data );
        return;
    }

    if( static_cast<uint32_t>( l2cap_pdu_length ) > s_max_reassembly_bytes_per_handle )
    {
        LogUtilWarning() << "l2cap pdu size " << l2cap_pdu_length
            << " exceeds per handle limit " << s_max_reassembly_bytes_per_handle
            << ", drop packet, handle: " << a_handle;
        clear_for_handle( a_handle );
        return;
    }

    if( m_total_buffered_bytes + static_cast<size_t>( l2cap_pdu_length ) > s_max_total_reassembly_bytes )
    {
        if( m_total_buffered_bytes + static_cast<size_t>( l2cap_pdu_length ) > s_max_total_reassembly_bytes )
        {
            LogUtilWarning() << "Total recombination buffer exceeds limit " << s_max_total_reassembly_bytes
                << ", drop packet, handle: " << a_handle;
            return;
        }
    }

    if( m_fragment_block.size() >= s_max_active_blocks )
    {
        LogUtilWarning() << "Too many active recombination blocks, drop packet, handle: " << a_handle;
        return;
    }

    /**
     * A new start fragment replaces any stale fragments of the same handle,
     * clear the stale block before pushing the new one.
     */
    clear_for_handle( a_handle );

    l2cap_recombination_block block;
    block.acl_handle = a_handle;
    block.pdu_size_left = l2cap_pdu_length - received_l2cap_bytes;
    block.total_pdu_size = l2cap_pdu_length;
    block.bytes_received = received_l2cap_bytes;

    l2cap_packet_fragment fragment;
    fragment.boundary = boundary_flag::automatically_flushable_start;
    fragment.fragment = a_hci_data;

    block.fragments.push_back( fragment );

    m_total_buffered_bytes += static_cast<size_t>( received_l2cap_bytes );

    m_fragment_block.push_back( block );
}

void l2cap_packet_recombinationer::handle_incoming_hci_packet_continue
    (
    uint16_t a_handle,
    std::shared_ptr<hci_data> const& a_hci_data
    )
{
    auto it = m_fragment_block.begin();
    for( ; it != m_fragment_block.end(); ++it )
    {
        if( it->acl_handle == a_handle )
        {
            break;
        }
    }

    if( it == m_fragment_block.end() )
    {
        LogUtilError() << "We do not have a start fragment, so ignore this continue packet fragment";
        return;
    }

    if( it->fragments.size() >= s_max_fragments_per_handle )
    {
        LogUtilWarning() << "Too many fragments for handle " << a_handle << ", drop recombination block.";
        release_block_bytes( *it );
        m_fragment_block.erase( it );
        return;
    }

    uint8_t const* p_packet = a_hci_data->m_buffer.data();

    /**
     * ACL header: 2 bytes handle/flags + 2 bytes total data length.
     * A continue fragment contains no L2CAP header, all its payload
     * belongs to the L2CAP PDU being recombined.
     */
    int32_t fragment_payload_size = le_to_host16( p_packet + 2 );
    if( fragment_payload_size <= 0 || fragment_payload_size > it->pdu_size_left )
    {
        LogUtilError() << "Invalid continue fragment size " << fragment_payload_size
            << ", pdu size left: " << it->pdu_size_left << ", drop recombination block, handle: " << a_handle;
        release_block_bytes( *it );
        m_fragment_block.erase( it );
        return;
    }

    it->pdu_size_left -= fragment_payload_size;
    it->bytes_received += fragment_payload_size;

    l2cap_packet_fragment fragment;
    fragment.boundary = boundary_flag::continuing_fragment;
    fragment.fragment = a_hci_data;
    it->fragments.push_back( fragment );

    m_total_buffered_bytes += static_cast<size_t>( fragment_payload_size );

    if( it->pdu_size_left <= 0 )
    {
        recombinate_and_report( it );
        release_block_bytes( *it );
        it = m_fragment_block.erase( it );
    }

    // we need wait for more fragment...
}

void l2cap_packet_recombinationer::recombinate_and_report( std::vector<l2cap_recombination_block>::iterator a_block )
{
    std::shared_ptr<hci_data> hci_data_report = std::make_shared<hci_data>();
    hci_data_report->m_from_controller = true;
    hci_data_report->m_type = uart_hci_type::acl_type;
    hci_data_report->m_buffer.reserve( a_block->total_pdu_size + 10 );

    bool is_first_fragment = true;
    for( auto it = a_block->fragments.begin(); it != a_block->fragments.end(); ++it )
    {
        if( is_first_fragment )
        {
            is_first_fragment = false;

            hci_data_report->m_buffer.insert( hci_data_report->m_buffer.end(),
                it->fragment->m_buffer.begin(), it->fragment->m_buffer.end() );
            continue;
        }

        uint8_t const* p_packet = it->fragment->m_buffer.data();
        uint8_t const* p_payload = p_packet + s_acl_header_size;
        uint16_t payload_size = le_to_host16( p_packet + 2 );

        hci_data_report->m_buffer.insert( hci_data_report->m_buffer.end(), p_payload,
            p_payload + payload_size );
    }

    m_complete_l2cap_packet_reporter( hci_data_report );
}

void l2cap_packet_recombinationer::default_l2cap_packet_reporter( std::shared_ptr<hci_data> )
{
    LogUtilError() << "Please invoke set_complete_l2cap_packet_reporter to set a reporter";
}

}
