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

#include "../sdp_module.h"
#include "hci_defs.h"
#include "sdp_protocol.h"
#include "endian_convert.h"
#include "../sdp_common.h"
#include "../sdp_task.h"
#include "sdp_manager.h"

#include "framework/log_util.h"
#include "framework/module_manager.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"
#include "framework/framework_event.h"

#include "../common/acl_connections_db.h"
#include "../l2cap/l2cap_module.h"
#include "../l2cap/l2cap_common.h"

#include <functional>

namespace bluetooth
{

using namespace framework;

uint16_t sdp_task::s_sdp_task_type_id = 0u;

sdp_module::sdp_module()
{
    set_name( s_sdp_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
    m_sdp_manager = std::make_shared<sdp_manager>();
}

void sdp_module::initialize()
{
    sdp_task::s_sdp_task_type_id = framework_manager::get_instance().register_task_type();

    auto mod = framework_manager::get_instance().get_module_manager().get_module
        ( l2cap_module::s_l2cap_module_name );
    auto l2cap_mod = static_pointer_cast<l2cap_module>( mod );

    l2cap_callbacks l2cap_cbs;
    l2cap_cbs.m_handle_module = get_name();
    l2cap_cbs.m_coming_connection_callback =
        std::bind( &sdp_manager::handle_sdp_connect_request, m_sdp_manager, std::placeholders::_1 );
    l2cap_cbs.m_coming_config_callback =
        std::bind( &sdp_manager::handle_config_request, m_sdp_manager, std::placeholders::_1 );
    l2cap_cbs.m_channel_state_changed_callback =
        std::bind( &sdp_manager::handle_connection_state, m_sdp_manager, std::placeholders::_1,
            std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5 );
    l2cap_cbs.m_channel_sdu_callback = std::bind( &sdp_manager::handle_sdu, m_sdp_manager, std::placeholders::_1 );

    l2cap_mod->register_callback( static_cast<uint16_t>( defined_l2cap_psm::sdp ), l2cap_cbs );
    m_sdp_manager->init();
}

void sdp_module::deinitialize()
{

}

void sdp_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    if( (uint16_t)a_task->get_task_type() != sdp_task::s_sdp_task_type_id )
    {
        LogUtilError() << "Wrong task type.";
        return;
    }

    std::shared_ptr<sdp_task> detail_task;
    detail_task = std::static_pointer_cast<sdp_task>( a_task );
    if( !detail_task )
    {
        LogUtilError() << "Wrong task type.";
        return;
    }

    switch( detail_task->m_type )
    {
    case sdp_task_type::register_service_record:
        m_sdp_manager->handle_register_record( detail_task );
        break;
    case sdp_task_type::service_search_request:
        m_sdp_manager->handle_service_search( detail_task );
        break;
    case sdp_task_type::service_search_attribute:
        m_sdp_manager->handle_service_search_attribute_host( detail_task );
        break;
    default:
        LogUtilError() << "sdp task type ignored: " << static_cast< uint32_t >( detail_task->m_type );
    }

}

void sdp_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    switch( a_event->m_event_type )
    {
    case event_type::power_on:
        m_sdp_manager->init_db();
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case event_type::power_off:
        set_power_status( abstract_module::powering_status::power_off );
        m_sdp_manager->clear_db();
        break;
    case event_type::power_status_changed:
        break;
    default:
        break;
    }
}

}

