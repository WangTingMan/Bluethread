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
#include "data_element.h"

namespace bluetooth
{

enum class gap_task_type : uint8_t
{
    invalid_type = 0x00,
    gap_base_settings,
    hci_cmd_response, // m_hci_response and m_hci_cmd are valid
    visibility_setting, // m_connectable and m_discoverable are valid
    change_local_name, // m_localname is valid
    hci_event,
    ssp_pairing_confirm_result,
    make_edr_acl_connection, // To make one BR/EDR ACL connection
};

enum class gap_base_settings_type : uint8_t
{
    invalid = 0,
    power_on,
    power_off,
    edr_inquiry,
    cancel_edr_inquiry,
    to_discoverable,  // no other parameters need
    to_nondiscoverable,// no other parameters need
    to_connectable,    // no other parameters need
    to_disconnectable, // no other parameters need
    read_visibility,   // read visiblity settings from chip
    to_unpairable,     // To unpairable mode. Stack will reject coming pairing request automatically
    to_pairable,       // To pairabe mode. Stack will router coming pairing request
};

class gap_module_task : public framework::abstract_task
{

public:

    gap_module_task();

    gap_task_type get_gap_module_task_type()const
    {
        return m_gap_task_type;
    }

protected:

    gap_task_type m_gap_task_type = gap_task_type::invalid_type;

public:

    static uint16_t s_gap_module_task_type_id;
};

class gap_base_settings_task : public gap_module_task
{

public:

    gap_base_settings_task
        (
        gap_base_settings_type a_base_setting = gap_base_settings_type::invalid
        )
        : m_base_setting( a_base_setting )
    {
        m_gap_task_type = gap_task_type::gap_base_settings;
    }

    gap_base_settings_type m_base_setting = gap_base_settings_type::invalid;
};

class gap_tsk_hci_cmd_response : public gap_module_task
{

public:

    gap_tsk_hci_cmd_response()
    {
        m_gap_task_type = gap_task_type::hci_cmd_response;
    }

    std::shared_ptr<hci_data> m_hci_response;
};

class gap_tsk_visibility_setting : public gap_module_task
{

public:

    gap_tsk_visibility_setting()
    {
        m_gap_task_type = gap_task_type::visibility_setting;
    }

    bool m_connectable = false;
    bool m_discoverable = false;
};

class gap_tsk_change_local_name : public gap_module_task
{

public:

    gap_tsk_change_local_name()
    {
        m_gap_task_type = gap_task_type::change_local_name;
    }

    std::u8string m_localname;
    std::function<void()> m_local_name_changed_done_cb;
};

class gap_tsk_make_edr_acl_connection : public gap_module_task
{

public:

    gap_tsk_make_edr_acl_connection()
    {
        m_gap_task_type = gap_task_type::make_edr_acl_connection;
    }

    bluetooth_address m_address;
};

class gap_tsk_ssp_pairing_confirm_result : public gap_module_task
{

public:

    gap_tsk_ssp_pairing_confirm_result()
    {
        m_gap_task_type = gap_task_type::ssp_pairing_confirm_result;
    }

    bluetooth_address m_address;
    bool m_accept = false;
};

class gap_tsk_hci_event : public gap_module_task
{

public:

    gap_tsk_hci_event()
    {
        m_gap_task_type = gap_task_type::hci_event;
    }

    std::shared_ptr<hci_data> m_hci_response;
};

}

