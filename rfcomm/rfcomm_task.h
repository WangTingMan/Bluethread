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

#include "framework/abstract_task.h"

#include "bluetooth_address.h"
#include "uuid.h"

namespace bluetooth
{

enum class rfcomm_task_type : uint8_t
{
    invalid_type = 0x00,
    accept_coming_connection_request = 0x01, /* accept coming connection request. */
    reject_coming_connection_request = 0x02, /* accept coming connection request.*/
    disconnect_specified_port = 0x03, /* disconnect specified device's connected specified port*/
    async_send_spp_data = 0x04, /* Send spp port data to remote device.*/
    connect_specified_port = 0x05, /* Connect remote device's port*/
    remove_specific_port = 0x06, /* remove specific rfcomm port connection */
};

class rfcomm_task : public framework::abstract_task
{

    friend class rfcomm_module;

public:

    rfcomm_task();

protected:

    rfcomm_task_type m_type = rfcomm_task_type::invalid_type;
    static uint16_t s_rfcomm_task_type_id;
};

class rfcomm_task_accept_coming_connection_request : public rfcomm_task
{

public:

    rfcomm_task_accept_coming_connection_request()
    {
        m_type = rfcomm_task_type::accept_coming_connection_request;
    }

    bluetooth_address address;
    uint8_t port = 0;
};

class rfcomm_task_reject_coming_connection_request : public rfcomm_task
{

public:

    rfcomm_task_reject_coming_connection_request()
    {
        m_type = rfcomm_task_type::reject_coming_connection_request;
    }

    bluetooth_address address;
    uint8_t port = 0;
};

class rfcomm_task_disconnect_specified_port : public rfcomm_task
{

public:

    rfcomm_task_disconnect_specified_port()
    {
        m_type = rfcomm_task_type::disconnect_specified_port;
    }

    bluetooth_address address;
    uint8_t port = 0;
    bool port_on_local = false;
};

class rfcomm_task_async_send_spp_data : public rfcomm_task
{

public:

    rfcomm_task_async_send_spp_data()
    {
        m_type = rfcomm_task_type::async_send_spp_data;
    }

    bluetooth_address address;
    uint8_t port = 0;
    bool port_on_local = false;
    std::shared_ptr<std::vector<uint8_t>> spp_data;
};

class rfcomm_task_connect_port : public rfcomm_task
{

public:

    rfcomm_task_connect_port()
    {
        m_type = rfcomm_task_type::connect_specified_port;
    }

    bluetooth_address address;
    uint8_t port = 0;
};

}

