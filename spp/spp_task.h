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

#include "bluetooth_address.h"
#include "uuid.h"
#include "sdp/sdp_common.h"
#include "sdp/sdp_data_element.h"

namespace bluetooth
{

enum class spp_task_type : uint8_t
{
    invalid_type = 0x00,
    spp_register_new_service = 0x01, /** To create a new spp service in local device.
                                        * m_name: the service name, encoded in utf-8
                                        * m_uuids: the uuid to used to register. empty then
                                        * will use spp uuid. but spp uuid only canbe use once
                                        * time.
                                        * m_registered_callback: after registered, then invoke
                                        * this callback
                                        */
    spp_disconnect_specified_address = 0x02, /* disconnect all connected ports with specified remote device
                                            * address is valid
                                            */
    spp_disconnect_specified_port = 0x03, /** disconnect specified device's connected specified port
                                        * address, port and port_on_local are valid
                                        */
    spp_connect_default_spp = 0x04, /**
                                * Connect to the default SPP uuid port. Only m_address is valid
                                */
    spp_async_send_spp_data = 0x05, /**
                                * Send spp port data to remote device. m_address, m_port, m_port_on_local and
                                * m_spp_data are valid
                                */
    spp_sdp_service_attribute_search_result = 0x06,  /**
                                                        * the SDP service searching result for RFCOMM.
                                                        */
};

class spp_task : public framework::abstract_task
{

    friend class spp_module;

public:

    spp_task();

    spp_task_type get_spp_task_type()const
    {
        return m_type;
    }

protected:

    spp_task_type m_type = spp_task_type::invalid_type;
    static uint16_t s_spp_task_type_id;
};

class spp_register_new_service_task : public spp_task
{

public:

    spp_register_new_service_task();

    std::u8string m_name;
    std::vector<uuid> m_uuids;
    std::function<void( uint8_t )> m_registered_callback;
};

class spp_disconnect_specified_address_task : public spp_task
{

public:

    spp_disconnect_specified_address_task();

    bluetooth_address m_address;
};

class spp_disconnect_specified_port_task : public spp_task
{

public:

    spp_disconnect_specified_port_task();

    bluetooth_address m_address;
    uint8_t m_port = 0x00;
    bool is_local_inited_port_connection = false;
};

class spp_connect_default_spp_task : public spp_task
{

public:

    spp_connect_default_spp_task();

    bluetooth_address m_address;
};

class spp_async_send_spp_data_task : public spp_task
{

public:

    spp_async_send_spp_data_task();

    bluetooth_address m_address;
    uint8_t m_port = 0x00;
    bool m_port_on_local = false;
    std::shared_ptr<std::vector<uint8_t>> m_spp_data; // Only spp user data without any protocol header
};

class spp_sdp_service_attribute_search_result_task : public spp_task
{

public:

    spp_sdp_service_attribute_search_result_task();

    sdp_search_result result = sdp_search_result::sdp_search_failed;
    bluetooth_address address;
    std::vector<uuid> uuids;
    std::vector<sdp_data_element> service_record_list;
    std::shared_ptr<spp_connection> spp_conn;
};


}

