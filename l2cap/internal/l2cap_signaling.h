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
#include <functional>
#include <memory>
#include <vector>

#include "data_element.h"
#include "../l2cap_common.h"
#include "l2cap_signaling_header.h"
#include "l2cap_ext_features.h"

#include "framework/log_util.h"

namespace bluetooth
{

using sig_pkt_handler = std::function<void( std::shared_ptr<signaling_channel_packet> const& )>;

struct comand_sent_control_block
{
    std::shared_ptr<signaling_channel_packet> m_sent_command;
    uint32_t m_registered_time_out_timer_id = 0;
};

class l2cap_signaling : public std::enable_shared_from_this<l2cap_signaling>
{

public:

    static constexpr uint16_t s_l2cap_edr_signaling_channel = 0x0001;

    static constexpr uint16_t s_l2cap_le_signaling_channel = 0x0005;

    static constexpr uint8_t s_flow_control_mode_support = 0x01;

    static constexpr uint8_t s_retransmission_mode_support = 0x02;

    static constexpr uint8_t s_bi_directional_qos_support = 0x04;

    static constexpr uint8_t s_enhanced_retransmission_mode_support = 0x08;

    static constexpr uint8_t s_streaming_mode_support = 0x10;

    static constexpr uint8_t s_fcs_option_support = 0x20;

    static constexpr uint8_t s_extended_flow_specification_edr_support = 0x40;

    static constexpr uint8_t s_fixed_channels_support = 0x80;

    static constexpr uint8_t s_extended_window_size_support = 0x01;

    static constexpr uint8_t s_unicast_connectionless_data_reception_support = 0x02;

    static constexpr uint8_t s_enhanced_credit_based_flow_control_support = 0x04;

     /**
      * construct a l2cap signaling block.
      * a_handle: the ACL connection handle.
      */
    l2cap_signaling( uint16_t a_handle = 0x00 );

    void set_sig_pkt_handler( sig_pkt_handler a_handler )
    {
        m_sig_pkt_handler = a_handler;
    }

    void set_remote_address( bluetooth_address const& a_remote )
    {
        m_remote_address = a_remote;
    }

    bluetooth_address const& get_remote_address()const
    {
        return m_remote_address;
    }

    /**
     * Handle incoming signaling packet from remote device
     */
    void handle_incoming_signaling( std::shared_ptr<hci_data> const& hci_data );

    void start_query_info()
    {
        query_information( l2cap_channel_information_type::connectionless_mtu );
    }

    void set_acl_handle( uint16_t a_handle )
    {
        m_sig_header.set_acl_handle( a_handle );
    }

    uint16_t get_acl_handle()const
    {
        return m_sig_header.get_acl_handle();
    }

    /**
     * Set the ACL connection's packet boundary flag value.
     */
    void set_packet_boundary( uint8_t a_pb_flag );

    /**
     * Set the ACL connection's broadcast flag
     */
    void set_broadcast_flag( uint8_t a_bc_flag );

    void send_reject_rsp
        (
        uint8_t a_identifier,
        l2cap_command_reject_reason a_reason,
        uint8_t* a_ext_data = nullptr,
        uint16_t a_ext_data_size = 0
        );

    /**
     * Send channel connection request to remote device
     */
    void send_connection_request
        (
        uint16_t a_psm,
        uint16_t a_source_id
        );

    /**
     * Send channel connection response to remote device
     */
    void send_connection_response
        (
        uint8_t a_identifier,
        uint16_t a_dest_cid,
        uint16_t a_src_cid,
        connection_req_result a_result,
        connection_req_refused_status a_refused_status
        );

    /**
     * Send the specified channel's local configuration.
     * Return the identifier used to send the signaling packet.
     */
    uint8_t send_config_request
        (
        uint16_t a_remote_cid,
        std::vector<channel_config_option> const& a_options
        );

    void send_config_response
        (
        uint8_t a_identifier,
        uint16_t a_src_cid,
        uint16_t a_flags,
        channel_config_result a_result,
        std::vector<channel_config_option> const& a_options
        );

    void send_disconnect_request
        (
        uint16_t a_dest_cid,
        uint16_t a_src_cid
        );

    void send_disconnect_response
        (
        uint8_t a_identifider,
        uint16_t a_dest_cid,
        uint16_t a_src_cid
        );

    void send_echo_request
        (
        uint8_t const* a_data = 0,
        uint16_t a_size = 0
        )
    {
        send_echo( signaling_code::l2cap_echo_req, ++m_indentifier, a_data, a_size );
    }

    void send_echo_response
        (
        uint8_t a_identifier,
        uint8_t const* a_data = 0,
        uint16_t a_size = 0
        )
    {
        send_echo( signaling_code::l2cap_echo_rsp, a_identifier, a_data, a_size );
    }

    /**
     * Register a remote device's information requested callback.
     * the first parameter of the callback is the ACL handle, and the second parameter is
     * the requested information type.
     */
    void register_info_requested( std::function<void( uint16_t, l2cap_channel_information_type )> a_callback )
    {
        m_info_callback = a_callback;
    }

    void set_acl_type( acl_type a_type )
    {
        m_acl_type = a_type;
        switch( m_acl_type )
        {
        case acl_type::br_edr_acl:
            m_sig_header.set_channel_id( s_l2cap_edr_signaling_channel );
            break;
        case acl_type::le_acl:
            m_sig_header.set_channel_id( s_l2cap_le_signaling_channel );
            break;
        default:
            LogUtilError() << "Invalid ACL type: " << static_cast<uint8_t>( m_acl_type );
            break;
        }
    }

    acl_type get_acl_type()const
    {
        return m_acl_type;
    }

private:

    bool signaling_length_valid( std::shared_ptr<hci_data> const& hci_data );

    /**
     * Handle information request from remote device
     */
    uint16_t handle_information_request
        (
        uint8_t const* a_raw_sig,
        uint16_t a_size
        );

    /**
     * Handle information response from remote device
     */
    void handle_information_response( std::vector<uint8_t> const& a_raw_hci );

    /**
     * Handle the coming command reject response
     */
    uint16_t handle_command_reject_response
        (
        uint8_t const* a_raw_sig,
        uint16_t a_size
        );

    /**
     * Handle the coming channel connection request
     */
    uint16_t handle_connection_request
        (
        uint8_t const* a_raw_sig,
        uint16_t a_size
        );

    /**
     * Handle the coming channel connection response
     */
    uint16_t handle_connection_response
        (
        uint8_t const* a_raw_sig,
        uint16_t a_size
        );

    /**
     * Handle the coming channel configuration request
     */
    void handle_config_request( std::vector<uint8_t> const& a_raw_hci );

    /**
     * Handle the coming channel configuration response
     */
    void handle_config_response( std::vector<uint8_t> const& a_raw_hci );

    /**
     * Handle the coming channel echo request
     */
    uint16_t handle_echo_request
        (
        uint8_t const* a_raw_sig,
        uint16_t a_size
        );

    /**
     * Handle the coming channel echo request
     */
    void handle_echo_response(std::vector<uint8_t> const& a_raw_hci);

    /**
     * Handle the coming channel disconnect request
     */
    uint16_t handle_disconnect_request( uint8_t const* a_raw_sig, uint16_t a_size );

    /**
     * Handle the incoming connection parameter update request
     * Warning: only used in LE link.
     */
    void handle_connection_parameter_update_request( std::vector<uint8_t> const& a_raw_hci );

    uint16_t handle_unknown_signaling_code( uint8_t const* a_raw_sig, uint16_t a_size );

    /**
     * Send command reject response
     */
    void send_command_reject_response
        (
        uint8_t a_identifier,
        uint16_t a_reason,
        void* a_optional_data,
        uint16_t a_optional_size
        );

    void send_completed_acl_packet( std::vector<uint8_t> a_acl_packet );

    void query_information( l2cap_channel_information_type a_info_type );

    void send_echo
        (
        signaling_code a_code,
        uint8_t a_identifier,
        uint8_t const* a_data,
        uint16_t a_size
        );

    /**
     * We sent a command to remote device, but remote device did not response us in time.
     * We need handle this case.
     */
    void handle_command_wait_rsp_timeout( std::shared_ptr<signaling_channel_packet> a_sent_command );

    void cancel_timer( uint32_t a_timer_id );

    using sig_packtets = std::vector<comand_sent_control_block>;

    signaling_header                m_sig_header;
    uint8_t                         m_indentifier = 0x00;
    uint16_t                        m_remote_connectionless_mtu = 0x0000;
    uint16_t                        m_remote_signaling_mtu = 4096u;
    l2cap_ext_features              m_remote_ext_features;
    sig_pkt_handler                 m_sig_pkt_handler;
    sig_packtets                    m_commands_sent; //!< All the command signaling packets we sent.
                                                     //!< Will delete it once we received correponding response.
    bluetooth_address               m_remote_address;
    acl_type                        m_acl_type = acl_type::invalid_type;
    std::function<void(uint16_t, l2cap_channel_information_type)> m_info_callback;
};

}

