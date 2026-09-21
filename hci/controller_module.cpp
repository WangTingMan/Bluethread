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

#include "controller_module.h"
#include "hci_command_maker.h"
#include "hci_module.h"
#include "endian_convert.h"
#include "bluetooth_address.h"
#include "l2cap/l2cap_module.h"
#include "../common/controller.h"

#include "framework/log_util.h"
#include "framework/framework_manager.h"
#include "framework/thread_manager.h"
#include "framework/framework_event.h"
#include "framework/executable_task.h"

#include <future>

static constexpr uint16_t s_voive_setting = 0x0060;
static constexpr uint16_t host_acl_packet_size = 1021u;
static constexpr uint8_t host_sco_packet_size = 120u;
static constexpr uint16_t host_total_acl_packet_size = 50u;
static constexpr uint16_t host_total_sco_packet_size = 10u;
static constexpr uint16_t default_page_timeout = 0x3000; // unit: 0.625ms

namespace bluetooth
{

using namespace framework;

controller_module::controller_module()
{
    set_name( s_controller_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
    set_power_status( abstract_module::powering_status::power_off );
}

void controller_module::initialize()
{
    set_power_status( abstract_module::powering_status::power_off );
    auto con = framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    if( !con )
    {
        con = std::make_shared<controller>();
        framework_manager::get_instance().get_info_manager().register_information( con );
    }
}

void controller_module::deinitialize()
{

}

void controller_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    std::shared_ptr<controller_module_task> task;
    task = std::static_pointer_cast<controller_module_task>( a_task );
    if( task )
    {
        switch( task->m_task_type )
        {
        case controller_task_type::power_on:
            handle_power_on( task );
            break;
        case controller_task_type::power_off:
            handle_power_off( task );
            break;
        case controller_task_type::hci_cmd_response:
            handle_response_sequence( task );
            break;
        default:
            LogUtilError() << "task ignored: " << static_cast< uint16_t >( task->m_task_type );
            break;
        }
    }
}

void controller_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    auto& name = a_event->get_target_module();
    auto& event_type = a_event->m_event_type;
    if( get_name() == name )
    {
        // here not sequence scheduled so we need pay attention to it
        std::shared_ptr<controller_module_task> task;
        switch( event_type )
        {
        case event_type::power_on:
            task = std::make_shared<controller_module_task>();
            task->set_target_module( get_name() );
            task->set_source_module( get_name() );
            task->m_task_type = controller_task_type::power_on;
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            break;
        case event_type::power_off:
            task = std::make_shared<controller_module_task>();
            task->set_target_module( get_name() );
            task->set_source_module( get_name() );
            task->m_task_type = controller_task_type::power_off;
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            break;
        default:
            break;
        }
    }
    else
    {
        LogUtilWarning() << "event ignored.";
    }
}

void controller_module::handle_power_on( std::shared_ptr<controller_module_task> a_task )
{
    if( get_power_status() != powering_status::power_off )
    {
        LogUtilError() << "We need power on task at power off status.";
        return;
    }

    std::shared_ptr<hci_module::hci_module_task> hci_task;
    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    hci_task->m_hci_data = make_no_params_cmd( hci_command::hci_reset );
    hci_task->m_hci_data_handler = std::bind( 
        &controller_module::handle_response, this, std::placeholders::_1 );
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( get_name() );

    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

void controller_module::handle_power_off( std::shared_ptr<controller_module_task> const& a_task )
{
    LogUtilWarning() << "Controller module ignore the power off task, just set to powered off";
    set_power_status( abstract_module::powering_status::power_off );
}

void controller_module::handle_response_sequence( std::shared_ptr<controller_module_task> a_task )
{
    std::shared_ptr<hci_data> res = a_task->m_hci_response;
    std::shared_ptr<hci_data> hci_send;
    auto cmd = get_command_from_hci_response( res->m_buffer );
    uint8_t* p_buffer = res->m_buffer.data();
    uint8_t ret_status = 0x00;
    uint8_t total_size = 0x00;
    auto con = framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    switch( cmd )
    {
    case bluetooth::hci_command::hci_reset:
        hci_send = make_no_params_cmd( hci_command::hci_read_local_supported_commands );
        break;
    case hci_command::hci_read_local_supported_commands:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        if( 0x00 == ret_status )
        {
            con->update_support_cmds( std::vector<uint8_t>( p_buffer + 6, p_buffer + res->m_buffer.size() ) );
            hci_send = make_no_params_cmd( hci_command::hci_read_local_supported_features );
            if( con->controller_support_cmd( hci_command::hci_write_voice_setting ) )
            {
                hci_send = make_write_voice_setting( s_voive_setting );
            }
            else
            {
                hci_send = make_no_params_cmd( hci_command::hci_read_voice_setting );
            }
        }
        else
        {
            LogUtilError() << "controller return error. status: " << ret_status;
        }
        break;
    case hci_command::hci_write_voice_setting:
        hci_send = make_no_params_cmd( hci_command::hci_read_voice_setting );
        break;
    case hci_command::hci_read_voice_setting:
        hci_send = make_no_params_cmd( hci_command::hci_read_buffer_size );
        break;
    case hci_command::hci_read_buffer_size:
        con->update_buffer_size( le_to_host16( p_buffer + 6 ), p_buffer[8],
            le_to_host16( p_buffer + 9 ), le_to_host16( p_buffer + 11 ) );
        hci_send = make_host_buffer_size( host_acl_packet_size, host_sco_packet_size,
            host_total_acl_packet_size, host_total_sco_packet_size );
        {
            std::shared_ptr<l2cap_task> _task;
            _task = std::make_shared<l2cap_task>();
            _task->set_target_module( l2cap_module::s_l2cap_module_name );
            _task->set_source_module( s_controller_module_name );
            _task->m_type = l2cap_task_type::controller_buffer_read_done;
            framework_manager::get_instance().get_thread_manager().post_task
                ( _task, framework::source_here );
        }
        break;
    case hci_command::hci_host_buffer_size:
        hci_send = make_no_params_cmd( hci_command::hci_read_local_version_information );
        break;
    case hci_command::hci_read_local_version_information:
        con->update_version( p_buffer[6], p_buffer[9] );
        hci_send = make_no_params_cmd( hci_command::hci_read_bd_addr );
        break;
    case hci_command::hci_read_bd_addr:
        {
            bluetooth_address local_address;
            memcpy( local_address.address, p_buffer + 6, 6 );
            con->update_address( local_address );
        }
        hci_send = make_no_params_cmd( hci_command::hci_read_local_supported_features );
        break;
    case hci_command::hci_read_local_supported_features:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        if( 0x00 == ret_status )
        {
            con->update_lmp_features( std::vector<uint8_t>( p_buffer + 6, p_buffer + res->m_buffer.size() ) );
            hci_send = make_read_local_extended_features( 0x01 );
        }
        else
        {
            LogUtilError() << "controller return error. status: " << ret_status;
        }
        break;
    case hci_command::hci_read_local_extended_features:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        hci_send = handle_ext_features_response(con, res->m_buffer );
        break;
    case hci_command::hci_write_le_host_support:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        hci_send = con->controller_support_cmd( hci_command::hci_le_read_buffer_size_v2 ) ?
            make_no_params_cmd( hci_command::hci_le_read_buffer_size_v2 ) :
            make_no_params_cmd( hci_command::hci_le_read_buffer_size_v1 );
        break;
    case hci_command::hci_le_read_buffer_size_v2:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        con->update_le_buffer_size( le_to_host16( p_buffer + 6 ), p_buffer[7],
            le_to_host16( p_buffer + 8 ), p_buffer[10] );
        hci_send = make_no_params_cmd( hci_command::hci_le_read_local_supported_features );
        break;
    case hci_command::hci_le_read_buffer_size_v1:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        con->update_le_buffer_size( le_to_host16( p_buffer + 6 ), p_buffer[7] );
        hci_send = make_no_params_cmd( hci_command::hci_le_read_local_supported_features );
        break;
    case hci_command::hci_le_read_local_supported_features:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        if( 0x00 == ret_status )
        {
            std::vector<uint8_t> le_ll_features;
            le_ll_features.insert( le_ll_features.end(), p_buffer + 6, p_buffer + res->m_buffer.size() );
            con->update_le_features( le_ll_features );
            hci_send = make_no_params_cmd( hci_command::hci_le_read_filter_accept_list_size );
        }
        else
        {
            LogUtilError() << "read le features: error code: " << ret_status;
        }
        break;
    case hci_command::hci_le_read_filter_accept_list_size:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        con->update_le_white_list_size( p_buffer[6] );
        hci_send = make_le_set_event_mask( con );
        break;
    case hci_command::hci_le_set_event_mask:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        hci_send = make_no_params_cmd( hci_command::hci_le_le_read_supported_states );
        break;
    case hci_command::hci_le_le_read_supported_states:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        con->update_le_states( std::vector<uint8_t>( p_buffer + 6, p_buffer + res->m_buffer.size() ) );
        hci_send = make_page_timeout_cmd( default_page_timeout );
        break;
    case hci_command::hci_write_page_timeout:
        ret_status = p_buffer[5];
        total_size = p_buffer[1];
        hci_send = make_scan_mode( false, false );
        break;
    case hci_command::hci_write_scan_enable:
        hci_send = make_greed_inquiry_mode( con );
        break;
    case hci_command::hci_write_inquiry_mode:
        hci_send = make_set_local_name( con->get_local_name() );
        break;
    case hci_command::hci_write_local_name:
        hci_send = make_event_mask();
        break;
    case hci_command::hci_set_event_mask:
        LogUtilInfo() << "Initialization completed.";
        set_power_status( abstract_module::powering_status::power_on );
        break;
    default:
        break;
    }

    if( hci_send )
    {
        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = hci_send;
        hci_task->m_hci_data_handler = std::bind(
            &controller_module::handle_response, this, std::placeholders::_1 );
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( get_name() );
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
    }
}

std::shared_ptr<hci_data> controller_module::handle_ext_features_response
    (
    std::shared_ptr<controller> const& a_con,
    std::vector<uint8_t> const& a_hci_buffer
    )
{
    const uint8_t* p_buffer = a_hci_buffer.data();
    uint8_t max_page = p_buffer[7];
    uint8_t current_page = p_buffer[6];
    std::shared_ptr<hci_data> hci_send;
    bool ext_feature_completed = false;
    if( current_page == 0x01 )
    {
        a_con->update_lmp_ext_features( current_page, std::vector<uint8_t>( p_buffer + 7,
            p_buffer + a_hci_buffer.size() ) );
        if( max_page == 0x02 )
        {
            return make_read_local_extended_features( 0x02 );
        }
        else
        {
            ext_feature_completed = true;
        }
    }
    else if( current_page == 0x02 )
    {
        a_con->update_lmp_ext_features( current_page, std::vector<uint8_t>( p_buffer + 7,
            p_buffer + a_hci_buffer.size() ) );
        ext_feature_completed = true;
    }
    else
    {
        LogUtilError() << "we reached too many pages.";
        ext_feature_completed = true;
    }

    if( ext_feature_completed )
    {
        if( a_con->support_lmp_feature( local_features::feature_le_supported ) )
        {
            return make_write_le_host_support( true );
        }
    }
    return make_no_params_cmd( hci_command::hci_read_inquiry_mode );
}

std::shared_ptr<hci_data> controller_module::make_greed_inquiry_mode
    (
    std::shared_ptr<controller> const& a_con
    )
{
    uint8_t value = 0x00;
    bool supoort_eir = a_con->support_lmp_feature( local_features::feature_extended_inquiry );
    if( supoort_eir )
    {
        value = 0x02;
    }
    else
    {
        bool support_rssi = a_con->support_lmp_feature( local_features::feature_rssi_with_inquiry_results );
        if( support_rssi )
        {
            value = 0x01;
        }
    }

    return make_cmd( hci_command::hci_write_inquiry_mode, value );
}

void controller_module::handle_response( std::shared_ptr<hci_data> a_res )
{
    std::shared_ptr<controller_module_task> task;
    task = std::make_shared<controller_module_task>();
    task->set_source_module( get_name() );
    task->set_target_module( get_name() );
    task->m_task_type = controller_task_type::hci_cmd_response;
    task->m_hci_response = std::move( a_res );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

}

