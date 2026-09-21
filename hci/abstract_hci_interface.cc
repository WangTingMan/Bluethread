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

#include "abstract_hci_interface.h"
#include "framework/log_util.h"
#include "framework/abstract_task.h"
#include "framework/framework_manager.h"
#include "framework/thread_manager.h"
#include "hci_module.h"
#include "../include/data_element.h"

namespace bluetooth
{

void abstract_hci_interface::handle_data_from_controller
    (
    uint8_t a_type,
    uint8_t* a_data,
    uint16_t a_size
    )
{
    std::shared_ptr<hci_data> _hci_data;
    _hci_data = std::make_shared<hci_data>();
    _hci_data->m_buffer.reserve( a_size );
    _hci_data->m_from_controller = true;
    _hci_data->m_buffer.insert( _hci_data->m_buffer.begin(), a_data, a_data + a_size );
    switch( a_type )
    {
    case 0x01:
        _hci_data->m_type = uart_hci_type::command_type;
        break;
    case 0x02:
        _hci_data->m_type = uart_hci_type::acl_type;
        break;
    case 0x03:
        _hci_data->m_type = uart_hci_type::sco_type;
        break;
    case 0x04:
        _hci_data->m_type = uart_hci_type::event_type;
        break;
    default:
        return;
        break;
    }

    std::shared_ptr<hci_module::hci_module_task> task;
    task = std::make_shared<hci_module::hci_module_task>();
    task->m_hci_task_type = hci_module::hci_task_type::hci_data_from_controller;
    task->set_target_module( hci_module::s_hci_module_name );
    task->set_source_module( hci_module::s_hci_module_name );
    task->m_hci_data = std::move( _hci_data );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void abstract_hci_interface::initialization_completed( bool a_result )
{
    LogUtilInfo() << "Stack initialiation completed. result: " <<
        std::boolalpha << a_result;
    if( a_result )
    {
        m_driver_status = driver_init_status::initialized;
    }
    else
    {
        m_driver_status = driver_init_status::deinitialized;
    }

    std::shared_ptr<hci_module::hci_module_task> task;
    task = std::make_shared<hci_module::hci_module_task>();
    task->m_driver_init_status = m_driver_status;
    task->m_hci_task_type = hci_module::hci_task_type::driver_initialized_task;
    task->set_target_module( hci_module::s_hci_module_name );
    task->set_source_module( hci_module::s_hci_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void abstract_hci_interface::deinitialization_completed( bool a_result )
{
    LogUtilInfo() << "Stack deinitialization_completed completed. result: " <<
        std::boolalpha << a_result;
    if( a_result )
    {
        m_driver_status = driver_init_status::deinitialized;
    }
    else
    {
        LogUtilFatal() << "What should we do next step?";
    }
}

}
