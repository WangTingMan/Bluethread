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
#include "..\framework\abstract_module.h"
#include "..\framework\lendable_element.h"
#include "..\framework\abstract_task.h"
#include "..\common\bluetooth_common_event.h"

#include "uuid.h"
#include "spp_connection.h"
#include "data_element.h"
#include "bluetooth_address.h"

#include <functional>
#include <vector>
#include <memory>

namespace bluetooth
{

enum class spp_task_type : uint8_t
{
    invalid_type = 0x00,
    register_new_spp_service = 0x01, /** To create a new spp service in local device.
                                      * m_name: the service name, encoded in utf-8
                                      * m_uuids: the uuid to used to register. empty then
                                      * will use spp uuid. but spp uuid only canbe use once
                                      * time.
                                      * m_registered_callback: after registered, then invoke
                                      * this callback
                                      */
    disconnect_specified_address = 0x02, /* disconnect all connected ports with specified remote device
                                         * address is valid
                                         */
    disconnect_specified_port = 0x03, /** disconnect specified device's connected specified port
                                        * address, port and port_on_local are valid
                                        */
    connect_default_spp = 0x04, /**
                                * Connect to the default SPP uuid port. Only m_address is valid
                                */
    async_send_spp_data = 0x05, /**
                                * Send spp port data to remote device. m_address, m_port, m_port_on_local and
                                * m_spp_data are valid
                                */
};

class spp_module : public framework::abstract_module
{

public:

    constexpr static const char* s_spp_module_name = "spp_module";

    class spp_task : public framework::abstract_task
    {

    public:

        spp_task()
        {
            set_target_module( spp_module::s_spp_module_name );
            m_task_type = static_cast<framework::task_type>(s_spp_task_type_id);
        }

        spp_task_type m_type = spp_task_type::invalid_type;

        std::u8string m_name;
        std::vector<uuid> m_uuids;
        std::function<void( uint8_t )> m_registered_callback;

        bluetooth_address m_address;
        uint8_t m_port = 0x00;
        bool m_port_on_local = false;
        std::shared_ptr<std::vector<uint8_t>> m_spp_data; // Only spp user data without any protocol header

        static uint16_t s_spp_task_type_id;
    };

    spp_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_service_record_registered
        (
        uint32_t a_record_handle,
        std::u8string a_name,
        std::function<void( uint8_t )> a_registered_callback,
        uint8_t a_port,
        std::vector<uuid> a_uuids
        );

    void handle_connection_request
        (
        bluetooth_address a_address,
        uint8_t a_port
        );

    void handle_connection_changed
        (
        bluetooth_address a_address,
        connection_status a_status,
        uint8_t a_port,
        bool a_local_inited
        );

    void handle_bluetoot_event( std::shared_ptr<bluetooth_common_event> const& a_bt_event );

    void handle_sdu
        (
        std::shared_ptr<hci_data> a_raw,
        uint32_t a_offset,
        uint32_t a_size,
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_local_inited
        );

    void disconnect
        (
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_port_on_local
        );

    void disconnect
        (
        bluetooth_address a_address
        );

    void connect
        (
        bluetooth_address a_address
        );

    void send_port_data
        (
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_local_inited,
        std::shared_ptr<std::vector<uint8_t>> a_spp_data
        );

    /**
     * Create a new spp service. a_registered_callback will invoked with the rfcomm's port
     */
    void create_new_spp
        (
        std::u8string a_name,
        std::vector<uuid> a_uuids,
        std::function<void( uint8_t )> a_registered_callback
        );

    std::shared_ptr<spp_connection> find
        (
        bluetooth_address a_address,
        uint8_t a_port,
        bool a_local_inited
        );

    std::shared_ptr<spp_connection> find
        (
        bluetooth_address a_address,
        std::vector<uuid> a_uuid,
        bool a_local_inited
        );

    uint32_t m_spp_base_record_id = 0x00;
    uint8_t m_next_port_number = 0x00;
    std::vector<local_spp_service_info> m_local_spp_services;

    std::vector<std::shared_ptr<spp_connection>> m_connections;
};

}

