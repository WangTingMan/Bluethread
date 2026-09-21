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

namespace bluetooth
{

enum class hci_cmd_gp : uint8_t
{
    link_control = 0x01,
    link_policy = 0x02,
    controller_baseband = 0x03,
    information = 0x04,
    status = 0x05,
    testing = 0x06,
    le_controller = 0x08
};

enum class hci_cmd_op : uint16_t
{
    // link control
    inquiry = 0x0001,
    inquiry_cancel = 0x0002,
    periodic_inquiry_mode = 0x0003,
    exit_periodic_inquiry_mode = 0x0004,
    create_connection = 0x0005,
    disconnect = 0x0006,
    create_connection_cancel = 0x0008,
    accept_connection_request = 0x0009,
    reject_connection_request = 0x000A,
    link_key_request_reply = 0x000B,
    link_key_request_negative_reply = 0x000C,
    pin_code_request_reply = 0x000D,
    pin_code_request_negative_reply = 0x000E,
    change_connection_packet_type = 0x000F,
    authentication_requested = 0x0011,
    set_connection_encryption = 0x0013,
    change_connection_link_key = 0x0015,
    master_link_key = 0x0017,
    remote_name_request = 0x0019,
    remote_name_request_cancel = 0x001A,
    read_remote_features = 0x001B,
    read_remote_extended_features = 0x001C,
    read_remote_version_information = 0x001D,
    read_remote_clock_offset = 0x001F,
    read_lmp_handle = 0x0020,
    setup_synchronous_connection = 0x0028,
    accept_synchronous_connection = 0x0029,
    reject_synchronous_connection = 0x002A,
    io_capability_request_reply = 0x002B,
    user_confirmation_request_reply = 0x002C,
    user_confirmation_request_negative_reply = 0x002D,
    user_passkey_request_reply = 0x002E,
    user_passkey_request_negative_reply = 0x002F,
    remote_oob_data_request_reply = 0x0030,
    remote_oob_data_request_negative_reply = 0x0033,
    io_capability_request_negative_reply = 0x0034,
    enhanced_setup_synchronous_connection = 0x003D,
    enhanced_accept_synchronous_connection_request = 0x003E,
    truncated_page = 0x003F,
    truncated_page_cancel = 0x0040,
    set_connectionless_peripheral_broadcast = 0x0041,
    set_connectionless_peripheral_broadcast_receive = 0x0042,
    start_synchronization_train = 0x0043,
    receive_synchronization_train = 0x0044,
    remote_oob_extended_data_request_reply = 0x0045,

    //link policy
    hold_mode = 0x0001,
    sniff_mode = 0x0002,
    exit_sniff_mode = 0x0003,
    qos_setup = 0x0007,
    role_discovery = 0x0009,
    switch_role = 0x000B,
    read_link_policy_settings = 0x000C,
    write_link_policy_settings = 0x000D,
    read_default_link_policy_settings = 0x000E,
    write_default_link_policy_settings = 0x000F,
    flow_specification = 0x0010,
    sniff_subrating = 0x00011,

    //controller and baseband
    set_event_mask = 0x0001,
    reset = 0x0003,
    set_event_filter = 0x0005,
    flush = 0x0008,
    read_pin_type = 0x0009,
    write_pin_type = 0x000A,
    read_stored_link_key = 0x000D,
    write_stored_link_key = 0x0011,
    delete_stored_link_key = 0x0012,
    write_local_name = 0x0013,
    read_local_name = 0x0014,
    read_connection_accept_timeout = 0x0015,
    write_connection_accept_timeout = 0x0016,
    read_page_timeout = 0x0017,
    write_page_timeout = 0x0018,
    read_scan_enable = 0x0019,
    write_scan_enable = 0x001A,
    read_page_scan_activity = 0x001B,
    write_page_scan_activity = 0x001C,
    read_inquiry_scan_activity = 0x001D,
    write_inquiry_scan_activity = 0x001E,
    read_authentication_enable = 0x001F,
    write_authentication_enable = 0x0020,
    read_class_of_device = 0x0023,
    write_class_of_device = 0x0024,
    read_voice_setting = 0x0025,
    write_voice_setting = 0x0026,
    read_automatic_flush_timeout = 0x0027,
    write_automatic_flush_timeout = 0x0028,
    read_num_broadcast_retransmissions = 0x0029,
    write_num_broadcast_retransmissions = 0x002A,
    read_hold_mode_ativity = 0x002B,
    write_hold_mode_activity = 0x002C,
    read_transmit_power_level = 0x002D,
    read_synchronous_flow_control_enable = 0x002E,
    write_synchronous_flow_control_enable = 0x002F,
    set_controller_to_host_flow_control = 0x0031,
    host_buffer_size = 0x0033,
    host_number_of_completed_packets = 0x0035,
    read_link_supervision_timeout = 0x0036,
    write_link_supervision_timeout = 0x0037,
    read_number_of_wupported_iac = 0x0038,
    read_current_iac_lap = 0x0039,
    write_current_iac_lap = 0x003A,
    set_afh_host_channel_classification = 0x003F,
    read_inquiry_scan_type = 0x0042,
    write_inquiry_scan_type = 0x0043,
    read_inquiry_mode = 0x0044,
    write_inquiry_mode = 0x0045,
    read_page_scan_type = 0x0046,
    write_page_scan_type = 0x0047,
    read_afh_channel_assessment_mode = 0x0048,
    write_afh_channel_assessment_mode = 0x0049,
    read_extended_inquiry_response = 0x0051,
    write_extended_inquiry_response = 0x0052,
    refresh_encryption_key = 0x0053,
    read_simple_pairing_mode = 0x0055,
    write_simple_pairing_mode = 0x0056,
    read_local_oob_data = 0x0057,
    read_inquiry_response_transmit_power_level = 0x0058,
    write_inquiry_transmit_power_level = 0x0059,
    send_keypress_notification = 0x0060,
    Read_Default_Erroneous_Data_Reporting = 0x005A,
    write_default_erroneous_data_reporting = 0x005B,
    enhanced_flush = 0x005F,
    set_event_mask_page_2 = 0x0063,
    read_flow_control_mode = 0x0066,
    write_flow_control_mode = 0x0067,
    read_enhanced_transmit_power_level = 0x0068,
    read_le_host_support = 0x006C,
    write_le_host_support = 0x006D,

    //information parameters
    read_local_version_information = 0x0001,
    read_local_supported_commands = 0x0002,
    read_local_supported_features = 0x0003,
    read_local_extended_features = 0x0004,
    read_buffer_size = 0x0005,
    read_bd_addr = 0x0009,

    // LE commands
    le_set_event_mask = 0x0001,
    le_read_buffer_size_v1 = 0x0002,
    le_read_local_supported_features = 0x0003,
    le_read_filter_accept_list_size = 0x000F,
    le_le_read_supported_states = 0x001C,
    le_read_local_p_256_public_key = 0x0025,
    le_le_generate_dhkey_v1 = 0x0026,
    le_le_extended_create_connection = 0x0043,
    le_le_generate_dhkey_v2 = 0x005E,
    le_read_buffer_size_v2 = 0x0060,
};

enum class hci_event_type : uint8_t
{
    hci_inquiry_complete = 0x01,
    hci_inquiry_result = 0x02,
    hci_connection_complete = 0x03,
    hci_connection_request = 0x04,
    hci_disconnection_complete = 0x05,
    hci_remote_name_request_complete = 0x07,
    hci_command_complete = 0x0E,
    hci_command_status = 0x0F,
    hci_number_of_completed_packets = 0x13,
    hci_link_key_request = 0x17,
    hci_link_key_notification = 0x18,
    hci_inquiry_result_with_rssi = 0x22,
    hci_extended_inquiry_result = 0x2F,
    hci_io_capability_request = 0x31,
    hci_io_capability_response = 0x32,
    hci_user_confirmation_request = 0x33,
    hci_simple_pairing_complete = 0x36,
    hci_link_supervision_timeout_changed = 0x38
};

constexpr uint16_t mk_hci_cmd
    (
    uint8_t a_group,
    uint16_t a_command
    )
{
    uint16_t ret = 0x0;
    ret = ( a_group << 10 ) | a_command;
    return ret;
}

constexpr uint16_t mk_hci_cmd
    (
    hci_cmd_gp a_group,
    hci_cmd_op a_command
    )
{
    return mk_hci_cmd( static_cast< uint8_t >( a_group ), static_cast< uint16_t >( a_command ) );
}

enum class hci_command : uint16_t
{
    hci_invalid = 0x00,

    hci_inquiry = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::inquiry ),
    hci_inquiry_cancel = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::inquiry_cancel ),
    hci_create_connection = mk_hci_cmd(hci_cmd_gp::link_control, hci_cmd_op::create_connection ),
    hci_accept_connection_request = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::accept_connection_request ),
    hci_link_key_request_reply = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::link_key_request_reply ),
    hci_link_key_request_negative_reply = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::link_key_request_negative_reply ),
    hci_remote_name_request = mk_hci_cmd(hci_cmd_gp::link_control, hci_cmd_op::remote_name_request ),
    hci_io_capability_request_reply = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::io_capability_request_reply ),
    hci_user_confirmation_request_reply = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::user_confirmation_request_reply ),
    hci_user_confirmation_request_negative_reply = mk_hci_cmd(hci_cmd_gp::link_control, hci_cmd_op::user_confirmation_request_negative_reply ),
    hci_io_capability_request_negative_reply = mk_hci_cmd( hci_cmd_gp::link_control, hci_cmd_op::io_capability_request_negative_reply ),

    hci_set_event_mask = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::set_event_mask ),
    hci_reset = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::reset ),
    hci_set_event_filter = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::set_event_filter ),
    hci_write_page_timeout = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::write_page_timeout ),
    hci_write_local_name = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::write_local_name ),
    hci_read_scan_enable = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::read_scan_enable ),
    hci_write_scan_enable = mk_hci_cmd(hci_cmd_gp::controller_baseband, hci_cmd_op::write_scan_enable ),
    hci_write_voice_setting = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::write_voice_setting ),
    hci_read_voice_setting = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::read_voice_setting ),
    hci_host_buffer_size = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::host_buffer_size ),
    hci_read_inquiry_mode = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::read_inquiry_mode ),
    hci_write_inquiry_mode = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::write_inquiry_mode ),
    hci_write_simple_pairing_mode = mk_hci_cmd(hci_cmd_gp::controller_baseband, hci_cmd_op::write_simple_pairing_mode ),
    hci_write_le_host_support = mk_hci_cmd( hci_cmd_gp::controller_baseband, hci_cmd_op::write_le_host_support ),

    hci_read_buffer_size = mk_hci_cmd( hci_cmd_gp::information, hci_cmd_op::read_buffer_size ),
    hci_read_local_version_information = mk_hci_cmd( hci_cmd_gp::information,
        hci_cmd_op::read_local_version_information ),
    hci_read_bd_addr = mk_hci_cmd( hci_cmd_gp::information, hci_cmd_op::read_bd_addr ),
    hci_read_local_supported_commands = mk_hci_cmd( hci_cmd_gp::information,
        hci_cmd_op::read_local_supported_commands ),
    hci_read_local_supported_features = mk_hci_cmd( hci_cmd_gp::information,
        hci_cmd_op::read_local_supported_features ),
    hci_read_local_extended_features = mk_hci_cmd( hci_cmd_gp::information,
        hci_cmd_op::read_local_extended_features ),

    hci_le_set_event_mask = mk_hci_cmd( hci_cmd_gp::le_controller, hci_cmd_op::le_set_event_mask),
    hci_le_read_buffer_size_v1 = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_read_buffer_size_v1 ),
    hci_le_read_buffer_size_v2 = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_read_buffer_size_v2 ),
    hci_le_read_local_supported_features = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_read_local_supported_features ),
    hci_le_le_read_supported_states = mk_hci_cmd(hci_cmd_gp::le_controller, hci_cmd_op::le_le_read_supported_states ),
    hci_le_read_local_p_256_public_key = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_read_local_p_256_public_key ),
    hci_le_le_generate_dhkey_v1 = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_le_generate_dhkey_v1 ),
    hci_le_le_generate_dhkey_v2 = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_le_generate_dhkey_v2 ),
    hci_le_le_extended_create_connection = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_le_extended_create_connection ),
    hci_le_read_filter_accept_list_size = mk_hci_cmd( hci_cmd_gp::le_controller,
        hci_cmd_op::le_read_filter_accept_list_size ),

};

// see core spec vol6, part b, link layer
enum class le_ll_feature : uint16_t
{
    ll_encryption = 0,   /* LE Encryption (4.0) */
    ll_connection_params_req = 1,   /* Connection Parameters Request Procedure (4.1) */
    ll_extended_reject_ind = 2,   /* Extended Reject Indication (4.1) */
    ll_SLAVE_INITIATED_FEATURE_EXCH = 3,   /* Slave-Initiated Features Exchange (4.1) */
    ll_PING = 4,   /* LE Ping (4.1) */
    ll_DATA_LENGTH_EXT = 5,   /* LE Data Packet Length Extension (4.2) */
    ll_LINK_LAYER_PRIVACY = 6,   /* LE Link Layer Privacy (4.2) */
    ll_EXT_SCANNER_FILTER_POLICY = 7,   /* Extended Scanner Filter Policies (4.2) */
    ll_2M_PHY = 8,   /* LE 2Mbps PHY (5.0) */
    ll_STABLE_MOD_INDEX_TX = 9,   /* Stable Modulation Index - transmitter (5.0) */
    ll_STABLE_MOD_INDEX_RX = 10,   /* Stable Modulation Index - receiver (5.0) */
    ll_CODED_PHY = 11,   /* LE Coded PHY (5.0) */
    ll_EXT_ADVERT = 12,   /* LE Extended Advertising (5.0) */
    ll_PERIODIC_ADVERT = 13,   /* LE Periodic Advertising (5.0) */
    ll_CHANNEL_SEL_ALGORITHM_2 = 14,   /* Channel Selection Algorithm #2 (5.0) */
    ll_POWER_CLASS_1 = 15,   /* LE Power Class 1 (5.0) */
    ll_MIN_NUM_USED_CHAN_PROC = 16,   /* Minum Number of Used Channels Procedure (5.0) */
    ll_connection_cte_req = 17,
    ll_connection_cte_res = 18,
    ll_connectionless_cte_trasmitter = 19,
    ll_connectionless_cte_receiver = 20,
    ll_aod = 21,
    ll_aoa = 22,
    ll_receiving_cte = 23,
    ll_periodic_adv_sync_sender = 24,
    ll_periodic_adv_sync_receiver = 25,
    ll_sleep_clock_accuracy_update = 26,
    ll_remote_public_key_validation = 27,/* Remote Public Key Validation (Erratum 10734) */
    ll_connected_iso_central = 28,
    ll_connected_iso_peripheral = 29,
    ll_iso_braodcaster = 30,
    ll_sync_receiver = 31,
    ll_connected_iso_host_support = 32,
    ll_power_control_req = 33,
    ll_power_control_res = 34,
    ll_path_loss_monitor = 35,
    ll_periodic_adv_adi = 36,
    ll_connection_subrate = 37,
    ll_connection_subrate_host = 38,
    ll_chennal_classification = 39
};

enum class local_features : uint8_t
{
    feature_3_slot_packets = 0,
    feature_5_slot_packets = 1,
    feature_encryption = 2,
    feature_slot_offset = 3,
    feature_timing_accuracy = 4,
    feature_role_switch = 5,
    feature_hold_mode = 6,
    feature_sniff_mode = 7,

    feature_park_state = 8,
    feature_power_control_req = 9,
    feature_cqddr = 10,
    feature_sco_link = 11,
    feature_hv2_packets = 12,
    feature_hv3_packets = 13,
    feature_ulaw_sco_data = 14,
    feature_alaw_sco_data = 15,

    feature_cvsd_sco_data = 16,
    feature_paging_param_neg = 17,
    feature_power_control = 18,
    feature_transparent_sco_data = 19,
    feature_flow_control_lag_1 = 20,
    feature_flow_control_lag_2 = 21,
    feature_flow_control_lag_3 = 22,
    feature_broadcast_encryption = 23,

    feature_edr_2mbs = 25,
    feature_edr_3mbs = 26,
    feature_enhanced_inquiry_scan = 27,
    feature_interlaced_inquiry_scan = 28,
    feature_interlaced_page_scan = 29,
    feature_rssi_with_inquiry_results = 30,
    feature_esco_ev3_packets = 31,

    feature_esco_ev4_packets = 32,
    feature_esco_ev5_packets = 33,
    feature_afh_capable_slave = 35,
    feature_afh_classification_slave = 36,
    feature_br_edr_not_supported = 37,
    feature_le_supported = 38,
    feature_edr_3_slots = 39,

    feature_edr_5_slots = 40,
    feature_sniff_subrating = 41,
    feature_pause_encryption = 42,
    feature_afh_capable_master = 43,
    feature_afh_classification_master = 44,
    feature_edr_esco_2mbs = 45,
    feature_edr_esco_3mbs = 46,
    feature_edr_esco_3_slots = 47,

    feature_extended_inquiry = 48,
    feature_simul_le_bredr_same_dev = 49,

    feature_secure_simple_pairing = 51,
    feature_encapsulated_pdu = 52,
    feature_erroneous_data_reporting = 53,
    feature_non_flushable_pbf = 54,

    feature_link_superv_timeout_event = 56,
    feature_inq_rsp_tx_power_lvl = 57,
    feature_extended_features = 63,

    /* Group: Extended Feature Mask Page 1 (Local/Remote Supported Features) */
    feature_host_secure_simple_pairing = 64,
    feature_host_le_supported = 65,
    feature_host_simul_le_bredr_same_dev = 66,
    feature_host_secure_conn_support = 67
};

enum class le_states_combinations : uint8_t
{
    non_con_non_scan_undirected_adv_state = 0x00, /* Non-connectable and Non-Scannable Undirected Adertising State*/
    scan_undirected_adv_state = 0x01, /* Scannable Undirected Advertising State*/
    con_scan_undirected_adv_state = 0x02, /*Connectable and scannable undirected advertising state*/
    high_duty_con_directed_adve_state = 0x03, /* High duty cycle connectable directed advertising state*/
    passive_scan_state = 0x04, /* Passive Scanning State*/
    active_scan_state = 0x05, /* active scanning state*/
    init_state = 0x06, /* initiating state */
    con_peripheral_state = 0x07, /* connection state peripheral role */

};

enum class defined_l2cap_psm : uint16_t
{
    invalid = 0x00,
    sdp = 0x01,
    rfcomm = 0x03,
    hid_control = 0x011,
    hid_interrupt = 0x013,
    avctp = 0x017,
    avdtp = 0x19,
    avctp_browsing = 0x01B,
    att = 0x1F,
    ots = 0x25,
    eatt = 0x27
};

}

