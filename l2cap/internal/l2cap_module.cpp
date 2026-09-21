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

#include "../l2cap_module.h"
#include "l2cap_internal_task.h"

#include "framework/log_util.h"
#include "framework/framework_event.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"

#include "../../hci/hci_module.h"
#include "../../hci/hci_command_maker.h"
#include "../../gap/gap_module.h"

#include "../../common/bluetooth_common_event.h"
#include "../../common/acl_connections_db.h"

#include "acl_manager.h"
#include "endian_convert.h"

#include <utility>
#include <vector>

namespace bluetooth
{

using namespace framework;

l2cap_module::l2cap_module()
{
    set_name( s_l2cap_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
    m_connection_manager = std::make_shared<acl_manager>();
}

void l2cap_module::initialize()
{
    l2cap_task::s_l2cap_task_type_id = framework_manager::get_instance().register_task_type( 1 );

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    if( !acl_db )
    {
        acl_db = std::make_shared<acl_connections_db>();
        framework_manager::get_instance().get_info_manager().register_information( acl_db );
    }

    auto relay_fun = [this]( std::shared_ptr<hci_data> a_event )
    {
        std::shared_ptr<l2cap_task_hci_packet> tsk = std::make_shared<l2cap_task_hci_packet>();
        tsk->m_hci_packet = a_event;
        tsk->set_source_module( s_l2cap_module_name );
        handle_data_from_chipset( tsk );
    };

    auto mod = framework_manager::get_instance().get_module_manager().get_module( hci_module::s_hci_module_name );
    auto hci_mod = static_pointer_cast< hci_module >( mod );

    if( hci_mod )
    {
        hci_mod->register_event_handler( hci_event_type::hci_connection_request, get_name(), source_here, relay_fun );
        hci_mod->register_event_handler( hci_event_type::hci_connection_complete, get_name(), source_here, relay_fun );
        hci_mod->register_event_handler( hci_event_type::hci_link_supervision_timeout_changed, get_name(), source_here, relay_fun );
        hci_mod->register_event_handler( hci_event_type::hci_disconnection_complete, get_name(), source_here, relay_fun );
        hci_mod->register_event_handler( hci_event_type::hci_number_of_completed_packets, get_name(), source_here, relay_fun );
        hci_mod->register_acl_handler( relay_fun, get_name() );
    }
    else
    {
        LogUtilError() << "Cannot find hci module.";
    }

    std::shared_ptr<framework::executable_task> execute_task;
    execute_task = std::make_shared<framework::executable_task>(
        std::bind( &acl_manager::retrieve_controller_info, m_connection_manager ) );
    execute_task->set_target_module( s_l2cap_module_name );
    execute_task->set_source_module( s_l2cap_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( execute_task, framework::source_here );
}

void l2cap_module::deinitialize()
{

}

void l2cap_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    if( uint16_t( a_task->get_task_type() ) != l2cap_task::s_l2cap_task_type_id )
    {
        LogUtilWarning() << "Task type not match: "
            << static_cast<uint16_t>( a_task->get_task_type() )
            << " != "
            << l2cap_task::s_l2cap_task_type_id
            << "from: " << a_task->get_position();
        return;
    }

    std::shared_ptr<l2cap_task> detail_tsk;
    detail_tsk = std::static_pointer_cast< l2cap_task >( a_task );
    if( detail_tsk )
    {
        switch( detail_tsk->m_type )
        {
        case l2cap_task_type::invalid_type:
            LogUtilError() << "Invalid task type received.";
            break;
        case l2cap_task_type::hci_data_from_chip:
            handle_data_from_chipset( detail_tsk );
            break;
        case l2cap_task_type::accept_channel_connection_req:
            handle_accept_channel_connection_req( detail_tsk );
            break;
        case l2cap_task_type::request_config_local_channel:
            handle_config_local_channel( detail_tsk );
            break;
        case l2cap_task_type::accept_channel_config_req:
            handle_accept_config_req( detail_tsk );
            break;
        case l2cap_task_type::send_l2cap_sdu:
            handle_send_upper_sdu( detail_tsk );
            break;
        case l2cap_task_type::reject_channle_connection_req:
            handle_reject_channel_connection_req( detail_tsk );
            break;
        case l2cap_task_type::l2cap_connection_request:
            handle_request_connection_host( detail_tsk );
            break;
        case l2cap_task_type::register_or_deregister_psm:
            handle_register_or_deregister_psm( detail_tsk );
            break;
        case l2cap_task_type::controller_buffer_read_done:
            handle_buffer_size_read_done();
            break;
        case l2cap_task_type::send_l2cap_sdu_with_remote_address:
            handle_send_upper_sdu_with_remote_address( detail_tsk );
            break;
        case l2cap_task_type::remote_invalid_cid_channel_close:
            handle_remote_invalid_cid_channel_close( detail_tsk );
            break;
        case l2cap_task_type::clear_pending_packets:
            m_connection_manager->clear_pending_packets( detail_tsk );
            break;
        case l2cap_task_type::disconnect_channel_request:
            m_connection_manager->disconnect_channel( detail_tsk );
            break;
        case l2cap_task_type::remove_channel_from_cache:
            m_connection_manager->remove_channel_from_cache( detail_tsk );
            break;
        default:
            LogUtilError() << "type ignored." << static_cast< uint16_t >( detail_tsk->m_type );
            break;
        }
    }
    else
    {
        LogUtilWarning() << "Unhandled task.";
    }
}

void l2cap_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    switch( a_event->m_event_type )
    {
    case event_type::power_on:
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case event_type::power_off:
        set_power_status( abstract_module::powering_status::power_off );
        break;
    default:
        break;
    }
}

void l2cap_module::register_callback
    (
    uint16_t        a_psm,
    l2cap_callbacks a_callbacks
    )
{
    auto l2cap_module_ =
        framework::framework_manager::get_instance()
        .get_module_manager().get_module<l2cap_module>(
            l2cap_module::s_l2cap_module_name );
    std::function<void()> fun = std::bind( &l2cap_module::register_callback_internal, l2cap_module_, a_psm, a_callbacks );
    auto task = std::make_shared<framework::executable_task>();
    task->set_fun( fun, l2cap_module::s_l2cap_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void l2cap_module::deregister_callback
    (
    uint16_t a_psm
    )
{
    auto l2cap_module_ =
        framework::framework_manager::get_instance()
        .get_module_manager().get_module<l2cap_module>(
            l2cap_module::s_l2cap_module_name );
    std::function<void()> fun = std::bind( &l2cap_module::deregister_callback_internal, l2cap_module_, a_psm );
    auto task = std::make_shared<framework::executable_task>();
    task->set_fun( fun, l2cap_module::s_l2cap_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void l2cap_module::handle_data_from_chipset( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto hci_packet_task = std::static_pointer_cast<l2cap_task_hci_packet>( a_tsk );
    if( !hci_packet_task )
    {
        LogUtilError() << "Cannot cast a_tsk to l2cap_task_hci_packet.";
        return;
    }

    switch( hci_packet_task->m_hci_packet->m_type )
    {
    case uart_hci_type::event_type:
        handle_hci_event( hci_packet_task );
        break;
    case uart_hci_type::acl_type:
        handle_acl_packet( hci_packet_task );
        break;
    default:
        LogUtilError() << "Wrong hci type data: "
            << static_cast< uint16_t >( hci_packet_task->m_hci_packet->m_type );
        break;
    }
}

void l2cap_module::handle_hci_event( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    std::shared_ptr<hci_data> const& hci_ = a_tsk->m_hci_packet;
    hci_event_type event_ = static_cast<hci_event_type>( hci_->m_buffer[0] );
    std::shared_ptr<hci_data> hci_send;
    switch( event_ )
    {
    case bluetooth::hci_event_type::hci_connection_request:
        hci_send = handle_connection_request( a_tsk );
        break;
    case bluetooth::hci_event_type::hci_connection_complete:
        handle_connection_completed( a_tsk );
        break;
    case bluetooth::hci_event_type::hci_link_supervision_timeout_changed:
        handle_supervision_changed( a_tsk );
        break;
    case bluetooth::hci_event_type::hci_disconnection_complete:
        handle_disconnection_completed( a_tsk );
        break;
    case hci_event_type::hci_number_of_completed_packets:
        handle_number_of_completed_packets( a_tsk );
        break;
    default:
        LogUtilError() << "Unknown event type: " << static_cast< uint16_t >( event_ );
        break;
    }

    if( hci_send )
    {
        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = hci_send;
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( get_name() );
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
    }
}

void l2cap_module::handle_acl_packet( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    m_connection_manager->handle_coming_acl_packet( std::move( a_tsk->m_hci_packet ) );
}

std::shared_ptr<hci_data> l2cap_module::handle_connection_request( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    std::shared_ptr<hci_data> hci_send;
    bluetooth_address address;
    uint8_t requested_type = a_tsk->m_hci_packet->m_buffer[11];
    if( requested_type != 0x01 )
    {
        LogUtilInfo() << " Not ACL connection request, so we ignore this message.";
        return nullptr;
    }

    memcpy( address.address, a_tsk->m_hci_packet->m_buffer.data() + 2, bluetooth_address::s_bluetooth_address_size );
    hci_send = make_accept_connection( address );
    LogUtilInfo() << "Accept the connection request from device: " << address.to_string();

    m_connection_manager->add_new_connection( address, acl_type::br_edr_acl, false );

    return hci_send;
}

void l2cap_module::handle_connection_completed( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    std::vector<uint8_t>const& hci_buffer = a_tsk->m_hci_packet->m_buffer;
    uint8_t status = hci_buffer[2];
    bluetooth_address address;
    uint8_t link_type = hci_buffer[11];
    if( link_type != 0x01 )
    {
        LogUtilInfo() << "Not acl connection message. So ignore this task.";
        return;
    }

    memcpy( address.address, hci_buffer.data() + 5, bluetooth_address::s_bluetooth_address_size );

    uint16_t handle_ = le_to_host16( hci_buffer.data() + 3 );
    if( status == 0x00 )
    {
        bool is_encrypted = hci_buffer[2] == 0x01;
        m_connection_manager->update_new_connection( address, handle_, acl_type::br_edr_acl, is_encrypted );

        std::shared_ptr<bluetooth_common_event> bt_event;
        bt_event = std::make_shared<bluetooth_common_event>();
        bt_event->set_source_module( get_name() );
        bt_event->m_bluetooth_event_type = bluetooth_event_type::edr_acl_connected;
        bt_event->m_remote_device = address;

        framework_manager::get_instance().get_thread_manager().post_task( bt_event, framework::source_here );
    }
    else
    {
        m_connection_manager->remove_pending_connection_request( address );

        std::shared_ptr<bluetooth_common_event> bt_event;
        bt_event = std::make_shared<bluetooth_common_event>();
        bt_event->set_source_module( get_name() );
        bt_event->m_bluetooth_event_type = bluetooth_event_type::edr_acl_disconnected;
        bt_event->m_remote_device = address;
        bt_event->m_acl_disconnected_code = status;

        framework_manager::get_instance().get_thread_manager().post_task( bt_event, framework::source_here );
    }
}

void l2cap_module::handle_disconnection_completed( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    std::vector<uint8_t>const& hci_buffer = a_tsk->m_hci_packet->m_buffer;
    uint8_t status = hci_buffer[2];
    if( status == 0x00 )
    {
        uint16_t handle_ = le_to_host16( hci_buffer.data() + 3 );
        uint8_t reason = hci_buffer[5];
        m_connection_manager->remove_connection( handle_ );
        LogUtilInfo() << "ACL disconnected with reason: " << static_cast< uint16_t >( reason );
    }
    else
    {
        LogUtilError() << "disconnection failed with status: " << static_cast< uint16_t >( status );
    }
}

void l2cap_module::handle_number_of_completed_packets( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    std::vector<uint8_t>const& hci_buffer = a_tsk->m_hci_packet->m_buffer;
    uint8_t total_size = hci_buffer[1];
    if( total_size + 2 > hci_buffer.size() )
    {
        LogUtilError() << "Wrong hci event. Ignore this hci event.";
        return;
    }

    uint8_t handle_num = hci_buffer[2];
    if( handle_num * 4 + 1 > total_size )
    {
        LogUtilError() << "Wrong hci event. Ignore this hci event.";
        return;
    }

    std::vector<std::pair<uint16_t, uint16_t>> packet_compeleted;
    uint8_t const* event_cur = hci_buffer.data() + 3;
    for( uint8_t i = 0; i < handle_num; ++i )
    {
        std::pair<uint16_t, uint16_t> info;
        info.first = le_to_host16( event_cur );
        event_cur += 2;
        packet_compeleted.push_back( info );
    }

    for( uint8_t i = 0; i < handle_num; ++i )
    {
        packet_compeleted[i].second = le_to_host16( event_cur );
        event_cur += 2;
    }

    m_connection_manager->handle_acl_completed_changed( packet_compeleted );
}

void l2cap_module::handle_supervision_changed( std::shared_ptr<l2cap_task_hci_packet> const& a_tsk )
{
    std::vector<uint8_t>const& hci_buffer = a_tsk->m_hci_packet->m_buffer;
    uint16_t handle_ = le_to_host16( hci_buffer.data() + 2 );
    uint16_t time_out = le_to_host16( hci_buffer.data() + 4 );

    m_connection_manager->update_supervision( handle_, time_out );
}

void l2cap_module::handle_accept_channel_connection_req( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_accept_channle_connection_req>( a_tsk );
    std::shared_ptr<connection_request> con_req = detail_task->m_connect_request;
    if( !con_req )
    {
        LogUtilError() << "Empty connection request";
        return;
    }

    LogUtilInfo() << "accept l2cap connection request for device: " << con_req->get_sender()
        << " with psm: " << con_req->m_psm_value
        << " and remote cid: " << con_req->m_source_cid;
    m_connection_manager->accept_connection_req( con_req );
}

void l2cap_module::handle_reject_channel_connection_req( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_reject_channle_connection_req>( a_tsk );
    if( !detail_task->m_connect_request )
    {
        LogUtilError() << "Empty connection request";
        return;
    }

    m_connection_manager->reject_connection_req( detail_task->m_connect_request, detail_task->m_reject_reason );
}

void l2cap_module::handle_accept_config_req( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_accept_channel_config_req>( a_tsk );
    if( !( detail_task->m_config_request ) )
    {
        LogUtilError() << "Empty config request to handle";
        return;
    }

    m_connection_manager->accept_config_req( detail_task->m_config_request );
}

void l2cap_module::handle_config_local_channel( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_request_config_local_channel>( a_tsk );
    if( !detail_task->m_config_local )
    {
        LogUtilError() << "Empty local channel config request";
        return;
    }

    m_connection_manager->config_local_channel_req( detail_task->m_config_local );
}

void l2cap_module::handle_send_upper_sdu( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_send_l2cap_sdu>( a_tsk );
    m_connection_manager->send_upper_sdu( detail_task );
}

void l2cap_module::handle_request_connection_host( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_connection_request>( a_tsk );
    m_connection_manager->handle_request_connection_host( detail_task->m_remote_device, static_cast<uint16_t>( detail_task->m_psm ) );
}

void l2cap_module::handle_register_or_deregister_psm( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_register_or_deregister_psm>( a_tsk );
    if( detail_task->m_to_regitster )
    {
        register_callback_internal( detail_task->m_psm, detail_task->m_callbacks );
    }
    else
    {
        deregister_callback_internal( detail_task->m_psm );
    }
}

void l2cap_module::handle_buffer_size_read_done()
{
    m_connection_manager->handle_buffer_size_read_done();
}

void l2cap_module::handle_send_upper_sdu_with_remote_address( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_send_l2cap_sdu_with_remote_address>(a_tsk);

    m_connection_manager->schedule_outgoing_packet( std::move( detail_task ) );
}

void l2cap_module::handle_remote_invalid_cid_channel_close( std::shared_ptr<l2cap_task> const& a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_remote_invalid_cid_channel_close>( a_tsk );
    m_connection_manager->close_channel_with_invalid_cid( detail_task );
}

void l2cap_module::register_callback_internal
    (
    uint16_t a_psm,
    l2cap_callbacks a_callbacks
    )
{
    LogUtilInfo() << "register psm: " << a_psm << " for " << a_callbacks.m_handle_module;
    m_connection_manager->register_callback( a_psm, a_callbacks );
    LogUtilInfo() << "register psm done.";
}

void l2cap_module::deregister_callback_internal
    (
    uint16_t a_psm
    )
{
    LogUtilInfo() << "deregister psm: " << a_psm;
    m_connection_manager->deregister_callback( a_psm );
}

}

