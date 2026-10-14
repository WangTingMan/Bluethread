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

#include "spp/spp_module.h"
#include "spp/spp_task.h"

namespace bluetooth
{

uint16_t spp_task::s_spp_task_type_id = 0u;

spp_task::spp_task()
{
    set_target_module( spp_module::s_spp_module_name );
    m_task_type = static_cast<framework::task_type>( s_spp_task_type_id );
}

spp_register_new_service_task::spp_register_new_service_task()
{
    m_type = spp_task_type::spp_register_new_service;
}

spp_disconnect_specified_address_task::spp_disconnect_specified_address_task()
{
    m_type = spp_task_type::spp_disconnect_specified_address;
}

spp_disconnect_specified_port_task::spp_disconnect_specified_port_task()
{
    m_type = spp_task_type::spp_disconnect_specified_port;
}

spp_connect_default_spp_task::spp_connect_default_spp_task()
{
    m_type = spp_task_type::spp_connect_default_spp;
}

spp_async_send_spp_data_task::spp_async_send_spp_data_task()
{
    m_type = spp_task_type::spp_async_send_spp_data;
}

}

