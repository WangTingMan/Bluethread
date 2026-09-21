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

#include "stack_manager.h"
#include "framework/log_util.h"

#include "gap/gap_module.h"
#include "rfcomm/spp_module.h"
#include "rfcomm/rfcomm_module.h"
#include "hci\hci_module.h"
#include "hci\hci_snoop_module.h"
#include "hci\controller_module.h"
#include "l2cap\l2cap_module.h"
#include "gap\gap_module.h"
#include "sdp\sdp_module.h"
#include "rfcomm\spp_module.h"
#include "rfcomm\rfcomm_module.h"
#include "config\config_module.h"

namespace bluetooth
{

using namespace framework;

void s_handle_power_changed( abstract_module::powering_status a_status )
{
    function_type type;
    if( abstract_module::powering_status::power_on == a_status )
    {
        type = function_type::bluetooth_enabled;
    }
    else if( abstract_module::powering_status::power_off == a_status )
    {
        type = function_type::bluetooth_disabled;
    }
    else
    {
        LogUtilInfo() << "Unexpected module manager power status: "
            << static_cast< uint16_t >( a_status );
        return;
    }

    auto cb = stack_manager::get_instance().get_stack_callback();
    if( cb )
    {
        LogUtilInfo() << "To notify bluetooth power status changed to: "
            << static_cast< uint16_t >( type );
        cb( service_type::not_specified, type, nullptr );
    }
}

stack_manager::stack_manager()
    : m_callback( nullptr )
{
    init();
}

stack_manager& stack_manager::get_instance()
{
    static stack_manager instance;
    return instance;
}

std::vector<std::shared_ptr<bluetooth::abstract_module>> module_creater()
{
    std::vector<std::shared_ptr<bluetooth::abstract_module>> _modules;
    _modules.push_back( std::make_shared<bluetooth::hci_module>() );
    _modules.push_back( std::make_shared<bluetooth::hci_snoop_module>() );
    _modules.push_back( std::make_shared<bluetooth::controller_module>() );
    _modules.push_back( std::make_shared<bluetooth::gap_module>() );
    _modules.push_back( std::make_shared<bluetooth::l2cap_module>() );
    _modules.push_back( std::make_shared<bluetooth::sdp_module>() );
    _modules.push_back( std::make_shared<bluetooth::rfcomm_module>() );
    _modules.push_back( std::make_shared<bluetooth::spp_module>() );
    _modules.push_back( std::make_shared<bluetooth::config_module>() );
    LogUtilInfo() << "Make bluetooth modules.";
    return _modules;
}

void stack_manager::enable_bt()
{
    bluetooth::framework_manager::get_instance().run( std::bind( &module_creater ), false );
    bluetooth::framework_manager::get_instance().get_module_manager()
        .register_power_changed_callback( std::bind( &s_handle_power_changed, std::placeholders::_1 ) );
    bluetooth::framework_manager::get_instance().power_up();

}

void stack_manager::search_device()
{
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    task->m_gap_task_type = bluetooth::gap_module::gap_task_type::edr_inquiry;
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    bluetooth::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void stack_manager::cancel_search_edr_device()
{
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    task->m_gap_task_type = bluetooth::gap_module::gap_task_type::cancel_edr_inquiry;
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    bluetooth::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void stack_manager::accept_ssp_confirm( bluetooth_address a_address, bool a_accept )
{
    bluetooth::gap_module::gap_task_type type = a_accept ?
        bluetooth::gap_module::gap_task_type::accept_ssp_confirm :
        bluetooth::gap_module::gap_task_type::reject_ssp_confirm;
    std::shared_ptr<bluetooth::gap_module::gap_module_task> task;
    task = std::make_shared<bluetooth::gap_module::gap_module_task>();
    task->m_gap_task_type = type;
    task->m_address = a_address;
    task->set_target_module( bluetooth::gap_module::s_gap_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void stack_manager::set_callback( stack_callback a_callback )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    m_callback = a_callback;
}

void stack_manager::init()
{
    bluetooth_common_event::register_task_type();
}

}
