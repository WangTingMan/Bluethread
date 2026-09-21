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

#include "framework/abstract_task.h"

#include "data_element.h"
#include "l2cap_common.h"

namespace bluetooth
{

    enum class l2cap_task_type : uint8_t
    {
        invalid_type                    = 0x00,
        hci_data_from_chip              = 0x01,
        accept_channel_connection_req   = 0x02, /* Let l2cap accept coming connection request. */
        request_config_local_channel    = 0x03, /*Request l2cap module to config the channel's local config.*/
        accept_channel_config_req       = 0x04, /*Let l2cap accept the coming configure request.*/
        send_l2cap_sdu                  = 0x05, /** send upper layer service data unit to remote device.
                                                * Upper layer should fill the data field. L2cap module will fill
                                                * the l2cap header related data field.*/
        reject_channle_connection_req   = 0x06, /*Let l2cap reject coming connection request.*/
        l2cap_connection_request        = 0x07, /* Request l2cap module to make a channel connection to remote device*/
        register_or_deregister_psm      = 0x08, /*upper layer uses this task to ask l2cap to register or cancel a psm.*/
        controller_buffer_read_done     = 0x09, // <! already read controller buffer size.
        send_l2cap_sdu_with_remote_address = 0x0A, // send upper layer service data unit to remote device with remote address.
        remote_invalid_cid_channel_close = 0x0B, /* Remote rejected command with INVALID_CID, trigger local channel close */
        clear_pending_packets           = 0x0C, /* Clear all pending packets in the channel's pending queue */
        disconnect_channel_request      = 0x0D, /* Request l2cap module to disconnect a channel with local cid */
        remove_channel_from_cache       = 0x0E, /* Remove the channel from l2cap module's channel cache */
    };

    class l2cap_task : public framework::abstract_task
    {

    public:

        l2cap_task();

        l2cap_task_type m_type = l2cap_task_type::invalid_type;
        static uint16_t s_l2cap_task_type_id;
    };

    class l2cap_task_hci_packet : public l2cap_task
    {

    public:

        l2cap_task_hci_packet()
        {
            m_type = l2cap_task_type::hci_data_from_chip;
        }

        std::shared_ptr<hci_data> m_hci_packet;
    };

    class l2cap_task_accept_channle_connection_req : public l2cap_task
    {

    public:

        l2cap_task_accept_channle_connection_req()
        {
            m_type = l2cap_task_type::accept_channel_connection_req;
        }

        std::shared_ptr<connection_request> m_connect_request;

    };

    class l2cap_task_request_config_local_channel : public l2cap_task
    {

    public:

        l2cap_task_request_config_local_channel()
        {
            m_type = l2cap_task_type::request_config_local_channel;
        }

        std::shared_ptr<l2cap_config_local_channel_request> m_config_local;

    };

    class l2cap_task_accept_channel_config_req : public l2cap_task
    {

    public:

        l2cap_task_accept_channel_config_req()
        {
            m_type = l2cap_task_type::accept_channel_config_req;
        }

        std::shared_ptr<l2cap_config_request> m_config_request;

    };

    class l2cap_task_send_l2cap_sdu : public l2cap_task
    {

    public:

        l2cap_task_send_l2cap_sdu()
        {
            m_type = l2cap_task_type::send_l2cap_sdu;
        }

        std::shared_ptr<hci_data> m_hci_packet;
        uint16_t m_local_cid = 0x00;
        bluetooth_address m_remote_address;
    };

    class l2cap_task_reject_channle_connection_req : public l2cap_task
    {

    public:

        l2cap_task_reject_channle_connection_req()
        {
            m_type = l2cap_task_type::reject_channle_connection_req;
        }

        std::shared_ptr<connection_request> m_connect_request;
        connection_req_result m_reject_reason = connection_req_result::connection_success;
    };

    class l2cap_task_connection_request : public l2cap_task
    {

    public:

        l2cap_task_connection_request()
        {
            m_type = l2cap_task_type::l2cap_connection_request;
        }

        bluetooth_address m_remote_device;
        defined_l2cap_psm m_psm = defined_l2cap_psm::invalid;
    };

    class l2cap_task_register_or_deregister_psm : public l2cap_task
    {

    public:

        l2cap_task_register_or_deregister_psm()
        {
            m_type = l2cap_task_type::register_or_deregister_psm;
        }

        uint16_t        m_psm = 0;             // which PSM to operate.
        l2cap_callbacks m_callbacks;
        bool            m_to_regitster = true; // true to register a PSM; false to deregister a PSM
    };

    class l2cap_task_send_l2cap_sdu_with_remote_address : public l2cap_task
    {

    public:

        l2cap_task_send_l2cap_sdu_with_remote_address()
        {
            m_type = l2cap_task_type::send_l2cap_sdu_with_remote_address;
        }

        std::shared_ptr<hci_data> m_hci_packet;
        uint16_t m_local_cid = 0x00;
        uint16_t m_acl_handle = 0x00;
        bluetooth_address m_remote_address;
    };

    class l2cap_task_disconnect_channel_request : public l2cap_task
    {

    public:

        l2cap_task_disconnect_channel_request()
        {
            m_type = l2cap_task_type::disconnect_channel_request;
        }

        uint16_t m_local_cid = 0x00;
        uint16_t m_acl_handle = 0x00;
        bluetooth_address m_remote_address;
    };
}
