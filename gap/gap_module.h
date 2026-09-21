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
#include "../framework/abstract_module.h"
#include "../framework/lendable_element.h"
#include "../framework/abstract_task.h"
#include "data_element.h"
#include "common.h"
#include "pairing_manager.h"

#include <string>
#include <vector>
#include <chrono>

namespace bluetooth
{

class controller;
class bluetooth_common_event;
struct acl_connection_cb
{
    bluetooth_address m_remote_device;
    connection_status m_status = connection_status::disconnected;
};

class gap_module : public framework::abstract_module
{

public:

    enum class gap_task_type : uint8_t
    {
        invalid_type = 0x00,
        power_on,
        power_off,
        hci_cmd_response, // m_hci_response and m_hci_cmd are valid
        edr_inquiry,      // no other parameters need
        cancel_edr_inquiry,
        to_discoverable,  // no other parameters need
        to_nondiscoverable,// no other parameters need
        to_connectable,    // no other parameters need
        to_disconnectable, // no other parameters need
        visibility_setting, // m_connectable and m_discoverable are valid
        read_visibility,   // read visiblity settings from chip
        change_local_name, // m_localname is valid
        hci_event,
        to_unpairable,     // To unpairable mode. Stack will reject coming pairing request automatically
        to_pairable,       // To pairabe mode. Stack will router coming pairing request
        accept_ssp_confirm,// To accept ssp pairing confirm from device m_address
        reject_ssp_confirm, // To reject ssp pairing confirm from device m_addres
        make_edr_acl_connection, // To make one BR/EDR ACL connection
    };

    class gap_module_task : public framework::abstract_task
    {

    public:

        gap_module_task()
        {
            set_target_module( s_gap_module_name );
            m_task_type = static_cast<framework::task_type>(s_gap_module_task_type_id);
        }

        gap_task_type m_gap_task_type = gap_task_type::invalid_type;
        std::shared_ptr<hci_data> m_hci_response;
        bool m_connectable = false;
        bool m_discoverable = false;
        std::u8string m_localname;
        bluetooth_address m_address;

        static uint16_t s_gap_module_task_type_id;
    };

    constexpr static const char* s_gap_module_name = "gap_module";

    gap_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_power_on( std::shared_ptr<gap_module_task> a_task );

    void handle_power_off( std::shared_ptr<gap_module_task> a_task );

    bool handle_other_module_pwr_changed( std::string a_name );

    void handle_search_task( std::shared_ptr<gap_module_task> a_task );

    void handle_cancel_search( std::shared_ptr<gap_module_task> const& a_task );

    void handle_hci_event( std::shared_ptr<hci_data> const& a_hci_event );

    void handle_bluetooth_event( std::shared_ptr<bluetooth_common_event> const& a_event );

    void handle_cmd_completed_event( std::shared_ptr<hci_data> const& a_hci_event );

    void handle_eir_inquiry_result( std::shared_ptr<hci_data> const& a_hci_event );

    void handle_name_request_completed( std::shared_ptr<hci_data> const& a_hci_event );

    void handle_command_status( std::shared_ptr<hci_data> const& a_hci_event );

    void set_discoverable( bool a_discoverable = true, bool a_execute = false );

    void set_connectable( bool a_connectable = true, bool a_execute = false );

    void set_visiblility( bool a_discoverable, bool a_connectable );

    void set_scan_mode();

    void notify_searching_changed();

    void notify_visiblity_changed();

    void changed_local_name( std::u8string const& a_name );

    void relay_hci_event( std::shared_ptr<hci_data> const& a_hci_event );

    void make_edr_acl( std::shared_ptr<gap_module_task> const& a_detail_task );

    void make_send_hci_task_and_schedule
        (
        std::shared_ptr<hci_data> a_hci_data,
        std::function< void( std::shared_ptr<hci_data>)> a_hci_data_receive_handler
        );

private:

    bool m_searching = false;
    enable_status m_connectable = enable_status::unknown;
    enable_status m_discoverable = enable_status::unknown;
    pairing_manager m_pairing_manager;
    std::vector<acl_connection_cb> m_acl_cb;
};

}
