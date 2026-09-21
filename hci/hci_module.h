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
#include "abstract_hci_interface.h"
#include "data_element.h"
#include "command_credit_reader.h"
#include "../common/controller.h"

#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include <unordered_map>

namespace bluetooth
{

using hci_data_receive_handler = std::function<void( std::shared_ptr<hci_data> )>;

struct event_callback_cb
{
    std::string m_target_module;
    hci_data_receive_handler m_handler;
    uint16_t m_callback_id = 0;
    framework::source_position m_position;
};

class hci_module_control_block;

class hci_module : public framework::abstract_module
{

public:

    enum class hci_task_type : uint8_t
    {
        invalid_type = 0xFF,
        send_hci_data = 0x01, // m_hci_data, m_hci_data_handler and m_handler_module_name are valid
        driver_initialized_task = 2, // Only m_driver_init_status is valid
        hci_data_from_controller = 3, // received data from controller. Only m_hci_data is valid
        driver_deinitialized_task = 4,
    };

    class hci_module_task : public framework::abstract_task
    {

    public:

        hci_module_task()
        {
            set_task_type( framework::task_type( s_hci_module_task_type_id ) );
        }

        hci_task_type m_hci_task_type = hci_task_type::invalid_type;
        abstract_hci_interface::driver_init_status m_driver_init_status 
            = abstract_hci_interface::driver_init_status::deinitialized;
        std::shared_ptr<hci_data> m_hci_data;
        hci_data_receive_handler m_hci_data_handler; // the data from chip handler
        std::string m_handler_module_name;  // which module to handle the m_hci_data_handler.
                                            // If empty then task runner module will execute the handler.

        static uint16_t s_hci_module_task_type_id;
    };

    constexpr static const char* s_hci_module_name = "hci_module_name";

    hci_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_hci_data )override;

    uint16_t register_event_handler
        (
        hci_event_type a_type,
        framework::source_position a_postion,
        std::function<void( std::shared_ptr<hci_data> )> a_handler
        );

    uint16_t register_event_handler
        (
        hci_event_type a_type,
        std::string a_target_module,
        framework::source_position a_postion,
        std::function<void( std::shared_ptr<hci_data> )> a_handler
        );

    /**
     * Register ACL packet handler. Will dispatch the a_handler to module
     * a_module_name.
     */
    void register_acl_handler
        (
        std::function<void( std::shared_ptr<hci_data> )> a_handler,
        std::string a_module_name
        );

private:

    void handle_send_hci_data( std::shared_ptr<hci_module_task> a_task );

    void handle_data_from_controller_task( std::shared_ptr<hci_module_task> a_task );

    void handle_pending_task();

    void handle_event( std::shared_ptr<hci_data> a_hci_data );

    void handle_command_completed_event( std::shared_ptr<hci_data> a_hci_data );

    void handle_command_status_event( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_cmd_credit_update( std::shared_ptr<hci_data> const& a_hci_data );

    /**
     * Check the hci event packet is valid or not. Check size
     */
    bool event_valid( std::shared_ptr<hci_data> const& a_hci_data );

    /**
     * Check the hci acl packet is valid or not. check size
     */
    bool acl_valid( std::shared_ptr<hci_data> const& a_hci_data );

    void initialize_chip();

    void handle_chipset_initialized();

    std::shared_ptr<hci_module_control_block> m_hci_control_block;
    command_credit_reader m_cmd_credit_reader;

    std::recursive_mutex m_mutex;
    std::vector<std::shared_ptr<framework::abstract_task>> m_pending_task;
    std::unordered_map<hci_event_type, std::vector<event_callback_cb>> m_event_handlers;
    uint16_t m_event_handler_count = 0;
    std::function<void( std::shared_ptr<hci_data> )> m_acl_handler;
    std::string m_acl_handler_module;
    uint32_t m_hci_init_timer = 0x00;
};


}


