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
#include <cstdint>
#include <vector>
#include <tuple>
#include <functional>
#include <string>
#include <iostream>

#include <framework/timer_module.h>

#include "bluetooth_address.h"
#include "data_element.h"

namespace bluetooth
{

enum class signaling_code : uint8_t
{
    l2cap_command_reject_rsp = 0x01,
    l2cap_connection_req = 0x02,
    l2cap_connection_rsp = 0x03,
    l2cap_configuration_req = 0x04,
    l2cap_configuration_rsp = 0x05,
    l2cap_disconnection_req = 0x06,
    l2cap_disconnection_rsp = 0x07,
    l2cap_echo_req = 0x08,
    l2cap_echo_rsp = 0x09,
    l2cap_information_req = 0x0A,
    l2cap_information_rsp = 0x0B,
    l2cap_connection_parameter_update_req = 0x12,
    l2cap_connection_parameter_update_rsp = 0x13,
    l2cap_le_credit_based_connection_req = 0x14,
    l2cap_le_credit_based_connection_rsp = 0x15,
    l2cap_flow_control_credit_ind = 0x16,
    l2cap_credit_based_connection_req = 0x17,
    l2cap_credit_based_connection_rsp = 0x18,
    l2cap_credit_based_reconfigure_req = 0x19,
    l2cap_credit_based_reconfigure_rsp = 0x1A
};

/**
 * This command reject reason code is for l2cap_command_reject_rsp
 * See core spec, vol 3, part A, section 4.1
 */
enum class command_reject_reason_code : uint16_t
{
    command_not_understood = 0x0000,
    signaling_mtu_exceeded = 0x0001,
    invalid_cid_in_request = 0x0002
};

enum class connection_req_result : uint16_t
{
    connection_success = 0x0000,
    connection_pending = 0x0001,
    connection_refused_not_support = 0x0002,
    connection_refused_security = 0x0003,
    connection_refused_no_resource = 0x0004,
    connection_refused_source_id_invalid = 0x0006,
    connection_refused_source_id_already_used = 0x0007
};

enum class connection_req_refused_status : uint16_t
{
    refused_no_more_info = 0x0000,
    refused_waiting_authentication = 0x0001,
    refused_waiting_authorization = 0x0002
};

enum class acl_type : uint8_t
{
    invalid_type,
    br_edr_acl,
    le_acl
};

enum class channel_config_option_type : uint8_t
{
    mtu = 0x01,
    flush_timeout = 0x02,
    qos = 0x03,
    retransmission_flow_control = 0x04,
    fcs = 0x05,
    extended_flow = 0x06,
    extended_window_size = 0x07
};

enum class retransmission_flow_mode_type : uint8_t
{
    base = 0x00,
    retransmission = 0x01,
    flow_control = 0x02,
    enhanced_retransmission = 0x03,
    streaming = 0x04
};

enum class qos_type : uint8_t
{
    no_traffic = 0x00,
    best_effort = 0x01,
    guaranteed = 0x02
};

enum class channel_config_result : uint16_t
{
    success = 0x00,
    unacceptable_parameters_failed = 0x01,
    rejected_failed = 0x02,
    unknown_options_failed = 0x03,
    pending = 0x04,
    flow_spec_rejected = 0x05
};

enum class l2cap_channel_state_type : uint8_t
{
    close_state = 0x00,
    wait_connect = 0x01,
    wait_config = 0x02,
    wait_config_req_rsp = 0x03,
    wait_config_rsp = 0x04,
    open = 0x05,
    wait_config_req = 0x06,
    wait_connect_rsp = 0x07,
    wait_send_config = 0x08,
    wait_disconnect = 0x09
};

enum class l2cap_command_reject_reason : uint16_t
{
    unknown_command = 0x00, // No reason data
    signaling_mtu_overflow = 0x01, // Reason data is the max signaling MTU
    invalid_cid = 0x02  // Reason data is local cid and remote cid
};

enum class l2cap_channel_information_type : uint16_t
{
    invalid_information = 0x0000,
    connectionless_mtu = 0x0001,
    extended_features_supported = 0x0002,
    fixed_channel_supported = 0x0003,
};

enum class l2cap_channel_close_reason : uint8_t
{
    no_reason = 0x00,
    page_timeout = 0x01, // Locak try to make acl connection but page timeout
};

struct channel_qos_config
{
    qos_type m_qos_type; // for QoS option
    uint32_t m_token_rate; // for QoS option
    uint32_t m_token_bucket_size; // for QoS option
    uint32_t m_peak_bandwidth; // for QoS option
    uint32_t m_latency; // for QoS option
    uint32_t m_delay_variation; // for QoS option
};

struct channel_flow_control_retransmission_config
{
    retransmission_flow_mode_type m_mode;
    uint8_t m_tx_windows_size;
    uint8_t m_max_transmit;
    uint16_t m_retransmission_timeout;
    uint16_t m_monitor_timeout;
    uint16_t m_max_pdu_size;
};

struct channel_ext_flow_config
{
    uint8_t m_identifier;
    qos_type m_qos_type;
    uint16_t m_max_sdu_size;
    uint32_t m_sdu_inter_arrival_time;
    uint32_t m_access_latency;
    uint32_t m_flush_timeout;
};

struct channel_config_option
{
    channel_config_option_type m_type = channel_config_option_type::mtu;
    union
    {
        uint16_t m_mtu;              // for mtu config option
        uint16_t m_flush_timeout;    // for flush timeout option

        channel_qos_config m_qos;

        channel_flow_control_retransmission_config m_flow_control_retransmission;

        uint8_t m_fcs;

        channel_ext_flow_config m_ext_flow;

        uint16_t m_ext_window_size;

    } m_option;

};

struct l2cap_connection
{
    uint16_t m_connection_handle = 0x00;
    uint16_t m_local_channel_id = 0x00;
    uint16_t m_remote_channel_id = 0x00;
    uint16_t m_psm_value = 0x00;
    bool m_local_inited = false;
};

struct channel_connection_request
{
    bluetooth_address m_remote_device;
    uint16_t m_psm = 0;
    std::chrono::steady_clock::time_point m_request_created_time;
};

class signaling_channel_packet
{

public:

    signaling_code m_signaling_code = signaling_code::l2cap_command_reject_rsp;
    uint8_t m_identifier = 0x00;
    uint16_t m_length = 0x00;

    signaling_channel_packet()
    {
        m_create_time = framework::timer_module::get_system_booting_time();
    }

    virtual ~signaling_channel_packet();

    /**
     * Return how long this packet will require
     */
    virtual uint16_t get_payload_length()
    {
        return 0;
    }

    /**
     * Parse the signal packet from the raw buffer.
     * [in] a_buffer the buffer pointer
     * [in] a_size the buffer available size
     * [return] how long buffer already parsed
     */
    virtual uint16_t parse_from_raw( uint8_t const* a_buffer, uint16_t a_size )
    {
        return 0;
    }

    /**
     * Fill all the message element into the buffer.
     * [in] a_buffer the buffer pointer
     * [in] a_zie the buffer available size
     * [return] how long buffer already used
     */
    virtual uint16_t fill_to_raw( uint8_t* a_buffer, uint16_t a_size )
    {
        return 0;
    }

    void set_receiver( bluetooth_address const& a_device )
    {
        m_receiver = a_device;
    }

    bluetooth_address const& get_receiver()const
    {
        return m_receiver;
    }

    void set_sender( bluetooth_address const& a_device )
    {
        m_sender = a_device;
    }

    bluetooth_address const& get_sender()const
    {
        return m_sender;
    }

    int64_t get_create_time()const
    {
        return m_create_time;
    }

protected:

    uint16_t parse_header( uint8_t const* a_buffer, uint16_t a_size );

    uint16_t fill_header_to_raw( uint8_t* a_buffer, uint16_t a_size );

private:

    bluetooth_address m_receiver; // which device will receiv this signaling packet.
    bluetooth_address m_sender;   // which device sent this signaling packet.
    int64_t           m_create_time; // the time point when this signaling packet created
};

class command_reject : public signaling_channel_packet
{

public:

    command_reject();

    uint16_t get_payload_length()override;

    uint16_t parse_from_raw( uint8_t const* a_buffer, uint16_t a_size )override;

    uint16_t fill_to_raw( uint8_t* a_buffer, uint16_t a_size )override;

    /**
     * Set rejected reason is unknown command
     */
    void set_unknown_command();

    /**
     * Set rejected reason is signaling MTU exceeded.
     * [in] a_expected_mtu local signaling mtu in fact
     */
    void set_signaling_mtx_exceeded( uint16_t a_expected_mtu );

    /**
     * Set rejected reason is invalid CID in request.
     * [in] a_expected_mtu local signaling mtu in fact
     */
    void set_invalid_cid( uint16_t a_local_cid = 0x0000, uint16_t a_remote_cid = 0x0000);

    l2cap_command_reject_reason m_reject_reason = l2cap_command_reject_reason::invalid_cid;
    std::vector<uint8_t> m_reject_data;
};

/**
 * l2cap channel connection request
 */
class connection_request : public signaling_channel_packet
{

public:

    connection_request();

    uint16_t m_psm_value = 0x00;
    uint16_t m_source_cid = 0x00;/*if local device sent this packet, then it is local cid; otherwise it is remote cid*/
    uint16_t m_acl_handle = 0x00;
};

/**
* channel connect response
*/
class l2cap_connect_response : public signaling_channel_packet
{

public:

    l2cap_connect_response();
    uint16_t m_destionation_cid = 0x00; /*The cid belongs to whoever generates and sends this signaling packet.*/
    uint16_t m_source_cid = 0x00;/*The cid does not belong to whoever generates and sends this signaling packet;
                                   it belongs to whoever receives this signaling packet.*/
    connection_req_refused_status status = connection_req_refused_status::refused_no_more_info; // valid only if result is pending
    connection_req_result result = connection_req_result::connection_success;
};

/**
 * channel configuration request
 */
class l2cap_config_request : public signaling_channel_packet
{

public:

    l2cap_config_request();

    std::vector<channel_config_option> m_options;
    uint16_t m_source_cid = 0x0000; /*The cid belongs to whoever generates and sends this signaling packet.
                                     Note: we won't wrap this field into l2cap sigaling packet, just use this
                                     to clearfy who send this packet*/
    uint16_t m_destionation_cid = 0x0000; /*It belongs to whoever receives this signaling packet.*/
    uint16_t m_acl_handle = 0x00;
    bool m_continue_flag = 0x00;
    bool m_remote_edr_ext_flow_support = false; // Indicate that remote device support extended flow option
    bool m_is_truncted = false;
    std::vector<uint8_t> m_unkown_option_types;
};

/**
 * channel configuration response
 */
class l2cap_config_response : public signaling_channel_packet
{

public:

    l2cap_config_response();

    std::vector<channel_config_option> m_options;
    uint16_t m_source_cid = 0x0000; /*It belongs to whoever receives this signaling packet.*/
    uint16_t m_acl_handle = 0x00;
    bool m_continue_flag = 0x00;
    channel_config_result m_result = channel_config_result::rejected_failed;
    std::vector<uint8_t> m_unkown_option_types;
};

/**
 * channel disconnect request
 */
class l2cap_disconnect_request : public signaling_channel_packet
{

public:

    l2cap_disconnect_request();
    uint16_t m_connection_handle = 0x00;
    uint16_t m_destination_cid = 0x00; /*The cid belongs to whoever receives this signaling packet.*/
    uint16_t m_source_cid = 0x00;      /*The cid belongs to whoever generates and sends this signaling packet.*/
};

/**
 * channel disconnect response
 */
class l2cap_disconnect_response : public signaling_channel_packet
{

public:

    l2cap_disconnect_response();
    uint16_t m_destination_cid = 0x00;
    uint16_t m_source_cid = 0x00;
};

class l2cap_echo_request : public signaling_channel_packet
{

public:

    l2cap_echo_request();
    std::vector<uint8_t> m_echo_data;
};

class l2cap_echo_response : public signaling_channel_packet
{

public:

    l2cap_echo_response();
    std::vector<uint8_t> m_echo_data;
};

class l2cap_information_request : public signaling_channel_packet
{

public:

    l2cap_information_request();
    l2cap_channel_information_type m_infor_type = l2cap_channel_information_type::extended_features_supported;
};

class l2cap_information_response : public signaling_channel_packet
{

public:

    l2cap_information_response();
    l2cap_channel_information_type m_infor_type = l2cap_channel_information_type::extended_features_supported;
    uint16_t m_result_code;
    uint8_t m_information_data[10];
};

struct l2cap_config_local_channel_request
{
    std::vector<channel_config_option> m_options;
    uint16_t m_acl_handle = 0x00;
    uint16_t m_remote_cid = 0x00; // Help to find the channel state machine entity
};

struct l2cap_callbacks
{
    /**
     * Which module to handle the callback. If there is no specified module, it can be empty.
     */
    std::string m_handle_module;

    /**
     * Indicate that thare is a coming connection request.
     * After receive this callback, the upper layer must decide whether to accept the
     * connection request or not in s_upper_layer_confirm_time_out( 10 seconds ).
     * Send a accept_channel_connection_req task to accept this connection request.
     */
    std::function<void(std::shared_ptr<connection_request>)> m_coming_connection_callback;

    /**
     * Indicate that there is a coming channel config request.
     * The parameter indicates that remote device requested configuration options.
     */
    std::function<void( std::shared_ptr<l2cap_config_request> )> m_coming_config_callback;

    /**
     * Indicate that there is a coming channel config response.
     * The parameter indicates that remote device's reponse for requested configuration options.
     * Upper layer can ignore this callback
     */
    std::function<void( std::shared_ptr<l2cap_config_response> )> m_coming_config_rsp_callback;

    /**
     * Indicate that the channel connection state has been changed.
     * Parameters: remote address, local cid, remote cid, and connection state.
     * After state changed to opened, then upper layer can send SDU data.
     *
     * @note Upon receiving the wait_config state notification, upper layer shall immediately
     * send a configuration request to the L2CAP layer.
     */
    std::function<void( bluetooth_address, uint16_t, uint16_t, l2cap_channel_state_type, l2cap_channel_close_reason )> m_channel_state_changed_callback;

    /**
     * Indicate that new sdu packet received
     */
    std::function<void( std::shared_ptr<hci_data> )> m_channel_sdu_callback;
};

/**
 * Retrieve the local channel id from the acl data
 */
uint16_t retrieve_local_cid( std::shared_ptr<hci_data> const& a_acl_data );

/// @brief Check BR/EDR L2CAP PSM encoding rule
/// @note Not applicable to BLE COC PSM
inline bool is_br_edr_psm_valid( uint16_t psm )
{
    uint8_t octet_low = psm & 0xFF;
    uint8_t octet_high = ( psm >> 8 ) & 0xFF;
    return ( ( octet_low & 0x01 ) == 1 ) && ( ( octet_high & 0x01 ) == 0 );
}

/**
 * Parse the channel configuration options from raw hci data.
 * a_buffer: the channel configuration starting buffer address.
 * a_size: the channel configuration size.
 * return: the parsed config options and unknown option types.
 */
std::tuple< std::vector<channel_config_option>, std::vector<uint8_t>, bool >
    parse_channel_config( uint8_t const* a_buffer, uint16_t a_size );

std::ostream& operator<<( std::ostream& a_os, signaling_code a_signaling );

std::ostream& operator<<( std::ostream& a_os, l2cap_channel_state_type a_state );

std::ostream& operator<<(std::ostream& a_os, channel_config_result a_state);

std::ostream& operator<<(std::ostream& a_os, acl_type a_state);

std::ostream& operator<<( std::ostream& a_os, l2cap_channel_information_type a_type );

}

