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

#include "rfcomm_multiplexer.h"

#include "endian_convert.h"
#include "../rfcomm_module.h"

#include "../l2cap/l2cap_module.h"
#include "../common/acl_connections_db.h"

#include "framework/log_util.h"
#include "framework/framework_manager.h"
#include "framework/timer_module.h"

#include <functional>

static constexpr uint8_t s_rfcomm_multipexer_channel = 0x00;
static constexpr uint8_t s_fcs_uih_cal_size = 2;
static constexpr uint8_t s_fc_cal_size = 3;
static constexpr uint8_t s_wait_sabm_timeout = 120; // wait sabm timeout is 120 seconds

namespace bluetooth
{
 
using namespace framework;

std::ostream& operator<<( std::ostream& a_os, rfcomm_event_type a_state )
{
    switch( a_state )
    {
    case bluetooth::rfcomm_event_type::start_request:
        a_os << "start_request";
        break;
    case rfcomm_event_type::l2cap_connetion_result:
        a_os << "l2cap_connetion_result";
        break;
    case rfcomm_event_type::l2cap_signaling_message:
        a_os << "l2cap_signaling_message";
        break;
    case rfcomm_event_type::multipexer_controlling:
        a_os << "multipexer_controlling";
        break;
    default:
        a_os << "unknown event( " << static_cast<uint16_t>( a_state ) << " )";
        break;
    }
    return a_os;
}

std::ostream& operator<<( std::ostream& a_os, rfcomm_multipexer_state_type a_type )
{
    switch( a_type )
    {
    case bluetooth::rfcomm_multipexer_state_type::idle:
        a_os << "idle";
        break;
    case bluetooth::rfcomm_multipexer_state_type::wait_conn_cnf:
        a_os << "wait_conn_cnf";
        break;
    case bluetooth::rfcomm_multipexer_state_type::configure:
        a_os << "configure";
        break;
    case bluetooth::rfcomm_multipexer_state_type::sabm_wait_ua:
        a_os << "sabm_wait_ua";
        break;
    case bluetooth::rfcomm_multipexer_state_type::wait_sabm:
        a_os << "wait_sabm";
        break;
    case bluetooth::rfcomm_multipexer_state_type::connected:
        a_os << "connected";
        break;
    case bluetooth::rfcomm_multipexer_state_type::disc_wait_ua:
        a_os << "disc_wait_ua";
        break;
    default:
        a_os << "unknown state";
        break;
    }
    return a_os;
}

rfcomm_state_base::rfcomm_state_base( rfcomm_multipexer& a_sm, rfcomm_multipexer_state_type a_type )
    : state_machine::abstract_state( a_sm, static_cast<uint8_t>( a_type ) )
    , m_rfcomm_mul( a_sm )
{
}

void rfcomm_state_base::transition_to_state( rfcomm_multipexer_state_type a_type )
{
    rfcomm_multipexer_state_type this_state = static_cast< rfcomm_multipexer_state_type >( get_id() );
    transition_to( static_cast<uint32_t>( a_type ) );
    LogUtilInfo() << "rfcomm state machine transition from state <" << this_state << "> to state <"
        << a_type << ">";
}

rfcomm_multipexer_idle::rfcomm_multipexer_idle( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::idle )
{

}

bool rfcomm_multipexer_idle::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_idle::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_multipexer_event> rfc_event;
    rfc_event = std::static_pointer_cast<rfcomm_multipexer_event>( a_event );
    if( !rfc_event )
    {
        LogUtilError() << "Wrong event received";
        return false;
    }

    switch( rfc_event->m_type )
    {
    case rfcomm_event_type::start_request:
        get_multipexer().connect();
        transition_to_state( rfcomm_multipexer_state_type::wait_conn_cnf );
        break;
    case rfcomm_event_type::l2cap_signaling_message:
        switch( rfc_event->m_signaling_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_connection_req:
            handle_connection_request( std::static_pointer_cast< connection_request >( rfc_event->m_signaling_pkt ) );
            break;
        default:
            LogUtilError() << "signaling evnet ignored: " << rfc_event->m_signaling_pkt->m_signaling_code;
        }
        break;
    case rfcomm_event_type::multipexer_controlling:
        handle_controlling( rfc_event );
        break;
    case rfcomm_event_type::l2cap_connetion_result:
        LogUtilInfo() << "Remote address: " << rfc_event->m_address.to_string() << ", local cid: "
            << rfc_event->m_local_cid << ", remote cid: " << rfc_event->m_remote_cid << ", state: " << rfc_event->m_state;
        break;
    default:
        LogUtilError() << "Ignored unknown event: " << rfc_event->m_type;
        break;
    }

    return true;
}

void rfcomm_multipexer_idle::handle_connection_request( std::shared_ptr<connection_request> const& a_request )
{
    if( !a_request )
    {
        LogUtilError() << "empty connection request.";
        return;
    }

    std::shared_ptr<l2cap_task_accept_channle_connection_req> task;
    task = std::make_shared<l2cap_task_accept_channle_connection_req>();
    task->set_source_module( rfcomm_module::s_rfcomm_module_name );
    task->m_connect_request = a_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    LogUtilInfo() << "accept connection request from remote device. handle " << a_request->m_acl_handle
        << ", remote cid: " << a_request->m_source_cid;

    get_multipexer().config_local_channel( a_request->m_acl_handle, a_request->m_source_cid );
    transition_to_state( rfcomm_multipexer_state_type::configure );
}

void rfcomm_multipexer_idle::handle_controlling( std::shared_ptr<rfcomm_multipexer_event>const& a_rfc_event )
{
    rfcomm_header& header = a_rfc_event->m_rfcomm_header;
    switch( header.get_frame_type() )
    {
    case rfcomm_frame_type::disc:
        // TODO send DM frame
        break;
    case rfcomm_frame_type::uih:
        // TODO send DM frame
        break;
    default:
        LogUtilError() << "rfcomm controlling type ignored: " << header.get_frame_type();
    }
}

void rfcomm_multipexer_idle::on_enter()
{

}

void rfcomm_multipexer_idle::on_exit()
{

}

rfcomm_multipexer_wait_conn_cnf::rfcomm_multipexer_wait_conn_cnf( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::wait_conn_cnf )
{

}

bool rfcomm_multipexer_wait_conn_cnf::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_wait_conn_cnf::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_multipexer_event> rfc_event;
    rfc_event = std::static_pointer_cast< rfcomm_multipexer_event >( a_event );
    if( !rfc_event )
    {
        LogUtilError() << "Wrong event received";
        return false;
    }

    switch( rfc_event->m_type )
    {
    case rfcomm_event_type::l2cap_connetion_result:
        handle_connection_changed( rfc_event );
        break;
    default:
        LogUtilError() << "Ignored unknown event: " << rfc_event->m_type;
        break;
    }

    return true;
}

void rfcomm_multipexer_wait_conn_cnf::handle_connection_changed( std::shared_ptr<rfcomm_multipexer_event> const& a_event )
{
    switch( a_event->m_state )
    {
    case l2cap_channel_state_type::close_state:
        // TODO notigy upper layer connection failed.
        transition_to_state( rfcomm_multipexer_state_type::idle );
        break;
    case l2cap_channel_state_type::open:

        break;
    default:
        break;
    }
}

void rfcomm_multipexer_wait_conn_cnf::on_enter()
{

}

void rfcomm_multipexer_wait_conn_cnf::on_exit()
{

}

rfcomm_multipexer_configure::rfcomm_multipexer_configure( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::configure )
{

}

bool rfcomm_multipexer_configure::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_configure::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_multipexer_event> rfc_event;
    rfc_event = std::static_pointer_cast< rfcomm_multipexer_event >( a_event );
    if( !rfc_event )
    {
        LogUtilError() << "Wrong event received";
        return false;
    }

    switch( rfc_event->m_type )
    {
    case rfcomm_event_type::l2cap_signaling_message:
        switch( rfc_event->m_signaling_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_req:
            handle_configure_request( std::static_pointer_cast<l2cap_config_request>( rfc_event->m_signaling_pkt ) );
            break;
        case signaling_code::l2cap_configuration_rsp:
            handle_configure_respose( std::static_pointer_cast<l2cap_config_response>( rfc_event->m_signaling_pkt ) );
            break;
        default:
            LogUtilError() << "signaling evnet ignored: " << rfc_event->m_signaling_pkt->m_signaling_code;
        }
        break;
    case rfcomm_event_type::l2cap_connetion_result:
        handle_connection_sate_changed( rfc_event );
        break;
    default:
        LogUtilError() << "Event type ignored: " << rfc_event->m_type;
    }

    return true;
}

void rfcomm_multipexer_configure::handle_configure_request( std::shared_ptr<l2cap_config_request> const& a_config_request )
{
    /**
    * TODO check the configuration request can be accpeted or not.
    */

    LogUtilInfo() << "received configure request from device: " << get_multipexer().m_remote_address.to_string()
        << ", local wait config rsp flag: " << std::boolalpha << get_multipexer().m_wait_config_rsp_flag;

    std::shared_ptr<l2cap_task_accept_channel_config_req> task;
    task = std::make_shared<l2cap_task_accept_channel_config_req>();
    task->set_source_module( rfcomm_module::s_rfcomm_module_name );
    task->m_config_request = a_config_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    get_multipexer().m_remote_configured = true;

    if( get_multipexer().m_wait_config_rsp_flag == false )
    {
        // we're not waiting for config rsp from remote device.
        if( get_multipexer().m_local_inited )
        {
            // send sabm frame to remote device next step
            get_multipexer().send_sabm_frame();
            transition_to_state( rfcomm_multipexer_state_type::sabm_wait_ua );
        }
        else
        {
            // It's remote device's responsibility to send SABM frame due to remote deivce create the l2cap connection 
            transition_to_state( rfcomm_multipexer_state_type::wait_sabm );
        }
    }
}

void rfcomm_multipexer_configure::handle_configure_respose( std::shared_ptr<l2cap_config_response> const& a_config_response )
{
    LogUtilInfo() << "Received config rsp from deivce: " << get_multipexer().m_remote_address.to_string();

    get_multipexer().m_wait_config_rsp_flag = false;
    if( !get_multipexer().m_remote_configured )
    {
        // we wait for remote device configure the channel
        return;
    }

    // we're not waiting for config rsp from remote device.
    if( get_multipexer().m_local_inited )
    {
        // send sabm frame to remote device next step
        get_multipexer().send_sabm_frame();
        transition_to_state( rfcomm_multipexer_state_type::sabm_wait_ua );
    }
    else
    {
        // It's remote device's responsibility to send SABM frame due to remote deivce create the l2cap connection 
        transition_to_state( rfcomm_multipexer_state_type::wait_sabm );
    }
}

void rfcomm_multipexer_configure::handle_connection_sate_changed( std::shared_ptr<rfcomm_multipexer_event> const& a_event )
{

    LogUtilInfo() << "Connection state changed. Device: " << a_event->m_address.to_string() << ", state: "
        << a_event->m_state;

    if( l2cap_channel_state_type::open == a_event->m_state )
    {
        // we're not waiting for config rsp from remote device.
        if( get_multipexer().m_local_inited )
        {
            // send sabm frame to remote device next step
            get_multipexer().send_sabm_frame();
            transition_to_state( rfcomm_multipexer_state_type::sabm_wait_ua );
        }
        else
        {
            // It's remote device's responsibility to send SABM frame due to remote deivce create the l2cap connection 
            transition_to_state( rfcomm_multipexer_state_type::wait_sabm );
        }
    }
}

void rfcomm_multipexer_configure::on_enter()
{
}

void rfcomm_multipexer_configure::on_exit()
{

}

rfcomm_multipexer_sabm_wait_ua::rfcomm_multipexer_sabm_wait_ua( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::sabm_wait_ua )
{

}

bool rfcomm_multipexer_sabm_wait_ua::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_sabm_wait_ua::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_multipexer_event> rfc_event;
    rfc_event = std::static_pointer_cast< rfcomm_multipexer_event >( a_event );
    if( !rfc_event )
    {
        LogUtilError() << "Wrong event received";
        return false;
    }

    switch( rfc_event->m_type )
    {
    case rfcomm_event_type::l2cap_connetion_result:
        switch( rfc_event->m_state )
        {
        case l2cap_channel_state_type::close_state:
            transition_to_state( rfcomm_multipexer_state_type::idle );
            break;
        default:
            LogUtilError() << "l2cap state ignored: " << rfc_event->m_state;
        }
        break;
    case rfcomm_event_type::multipexer_controlling:
        handle_multipexer_contolling( rfc_event );
        break;
    default:
        LogUtilError() << "Event type ignored: " << rfc_event->m_type;
    }

    return true;
}

void rfcomm_multipexer_sabm_wait_ua::handle_multipexer_contolling( std::shared_ptr<rfcomm_multipexer_event> const& a_event )
{
    rfcomm_header& header = a_event->m_rfcomm_header;
    switch( header.get_frame_type() )
    {
    case rfcomm_frame_type::ua:
        if( !get_multipexer().m_local_inited )
        {
            LogUtilError() << "We're not initor but received UA frame.";
        }
        transition_to_state( rfcomm_multipexer_state_type::connected );
        break;
    case rfcomm_frame_type::dm:
        // Remote device reject our connection request
        transition_to_state( rfcomm_multipexer_state_type::idle );
        break;
    default:
        break;
    }
}

void rfcomm_multipexer_sabm_wait_ua::on_enter()
{

}

void rfcomm_multipexer_sabm_wait_ua::on_exit()
{

}

rfcomm_multipexer_wait_sabm::rfcomm_multipexer_wait_sabm( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::wait_sabm )
{

}

bool rfcomm_multipexer_wait_sabm::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_wait_sabm::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_multipexer_event> rfc_event;
    rfc_event = std::static_pointer_cast< rfcomm_multipexer_event >( a_event );
    if( !rfc_event )
    {
        LogUtilError() << "Wrong event received";
        return false;
    }

    switch( rfc_event->m_type )
    {
    case rfcomm_event_type::l2cap_signaling_message:
        if( !rfc_event->m_signaling_pkt )
        {
            LogUtilError() << "Not set m_signaling_pkt";
            return false;
        }

        switch( rfc_event->m_signaling_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_rsp:
            handle_configure_respose( std::static_pointer_cast<l2cap_config_response>( rfc_event->m_signaling_pkt ) );
            break;
        default:
            LogUtilError() << "signaling evnet ignored: " << rfc_event->m_signaling_pkt->m_signaling_code;
        }
        break;
    case rfcomm_event_type::multipexer_controlling:
        handle_controlling( rfc_event );
        break;
    case rfcomm_event_type::l2cap_connetion_result:
        LogUtilInfo() << "Remote address: " << rfc_event->m_address.to_string() << ", local cid: "
            << rfc_event->m_local_cid << ", remote cid: " << rfc_event->m_remote_cid << ", state: " << rfc_event->m_state;
        break;
    default:
        LogUtilError() << "event type ignored: " << static_cast<uint32_t>( rfc_event->m_type );
        break;
    }
    return true;
}

void rfcomm_multipexer_wait_sabm::handle_controlling( std::shared_ptr<rfcomm_multipexer_event> const& a_rfc_event )
{
    rfcomm_header& header = a_rfc_event->m_rfcomm_header;
    rfcomm_frame_type type = header.get_frame_type();
    switch( type )
    {
    case bluetooth::rfcomm_frame_type::sabm:
        if( header.get_port() == s_rfcomm_multipexer_channel )
        {
            get_multipexer().send_ua_frame();
            transition_to_state( rfcomm_multipexer_state_type::connected );
        }
        else
        {
            LogUtilError() << "multipexer must receive the first SABM";
        }
        break;
    default:
        LogUtilError() << "rfcomm controlling frame ignored: " << type;
        break;
    }
}

void rfcomm_multipexer_wait_sabm::handle_configure_respose( std::shared_ptr<l2cap_config_response> const& a_config_response )
{

    LogUtilInfo() << "Received config rsp from deivce: " << get_multipexer().m_remote_address.to_string();

    get_multipexer().m_wait_config_rsp_flag = false;
}

void rfcomm_multipexer_wait_sabm::on_enter()
{
    auto mod = framework_manager::get_instance().get_module_manager().get_module( timer_module::s_timer_module_name );
    auto timer_module_ = std::static_pointer_cast<timer_module>( mod );
    timer_control_block::timeout_callback timer_callback;
    timer_callback = std::bind( &rfcomm_multipexer_wait_sabm::handle_timer_expire, this, std::placeholders::_1, std::placeholders::_2 );
    m_wait_sabm_timeout_timer = timer_module_->register_once_timer
        (
        timer_callback,
        std::chrono::seconds( s_wait_sabm_timeout ),
        "rfcomm_multipexer_wait_sabm_timerout",
        rfcomm_module::s_rfcomm_module_name
        );
    // TODO here the binded function not thread safe
}

void rfcomm_multipexer_wait_sabm::handle_timer_expire( uint32_t a_timer_id, std::string a_timer_name )
{
    LogUtilError() << "Not SABM received and disconnect the l2cap channel due to we already wait for "
        << s_wait_sabm_timeout << " seconds.";
    // TODO disconnect the L2CAP channel and transfer to idle state.
}

rfcomm_multipexer_wait_sabm::~rfcomm_multipexer_wait_sabm()
{
    auto mod = framework_manager::get_instance().get_module_manager().get_module( timer_module::s_timer_module_name );
    auto timer_module_ = std::static_pointer_cast< timer_module >( mod );
    timer_module_->undregister_timer( m_wait_sabm_timeout_timer );
}

void rfcomm_multipexer_wait_sabm::on_exit()
{
    auto mod = framework_manager::get_instance().get_module_manager().get_module( timer_module::s_timer_module_name );
    auto timer_module_ = std::static_pointer_cast<timer_module>( mod );
    timer_module_->undregister_timer( m_wait_sabm_timeout_timer );
}

rfcomm_multipexer_connected::rfcomm_multipexer_connected( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::connected )
{

}

bool rfcomm_multipexer_connected::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_connected::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_multipexer_event> rfc_event;
    rfc_event = std::static_pointer_cast< rfcomm_multipexer_event >( a_event );
    if( !rfc_event )
    {
        LogUtilError() << "Wrong event received";
        return false;
    }

    switch( rfc_event->m_type )
    {
    case rfcomm_event_type::multipexer_controlling:
        handle_controlling( rfc_event );
        break;
    default:
        LogUtilError() << "event type ignored: " << static_cast< uint32_t >( rfc_event->m_type );
        break;
    }

    return true;
}

void rfcomm_multipexer_connected::handle_controlling( std::shared_ptr<rfcomm_multipexer_event> const& a_rfc_event )
{
    rfcomm_header& header = a_rfc_event->m_rfcomm_header;
    rfcomm_frame_type type = header.get_frame_type();
    switch( type )
    {
    case rfcomm_frame_type::uih:
        get_multipexer().handle_uih_frame( a_rfc_event->m_hci_data, header );
        break;
    case rfcomm_frame_type::sabm:
        if( header.get_port() == s_rfcomm_multipexer_channel )
        {
            get_multipexer().send_ua_frame();
            LogUtilDebug() << "connected state received SABM frame";
        }
        else
        {
            // User channel SABM frame, we need transfer to user port to handle this frame.
            auto acl_db = framework_manager::get_instance().get_info_manager()
                .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
            auto [remote_address, has_address] = acl_db->get_address( header.get_acl_handle() );
            if( !has_address )
            {
                LogUtilError() << "Cannot find acl handle from the acl db. handle: " << header.get_acl_handle();
                return;
            }

            auto port = get_multipexer().find_port( remote_address, header.get_port(), false );
            if( port )
            {
                port->handle_controlling( header );
            }
            else
            {
                LogUtilError() << "Cannot find port. address: " << remote_address.to_string()
                    << ", port: " << static_cast< uint32_t >( header.get_port() );
            }
        }
        break;
    case rfcomm_frame_type::disc:
        if( header.get_port() == s_rfcomm_multipexer_channel )
        {
            LogUtilInfo() << "Since the base multipexer channel has been disconnected,"
                " so we clear all the upper port connections";
            get_multipexer().send_ua_frame();
            for( auto& ele : get_multipexer().m_worked_ports )
            {
                ele->handle_multipexer_disconnect();
            }
            get_multipexer().m_idle_ports.insert( get_multipexer().m_idle_ports.end(),
                get_multipexer().m_worked_ports.begin(), get_multipexer().m_worked_ports.end() );
            get_multipexer().m_worked_ports.clear();
            transition_to_state( rfcomm_multipexer_state_type::idle );
        }
        else
        {
            bool local_inited = !header.cr_value_in_header();
            auto port = get_multipexer().find_port( get_multipexer().m_remote_address, header.get_port(), local_inited );
            if( port )
            {
                port->handle_controlling( header );
            }
            else
            {
                LogUtilError() << "Cannot find port. address: " << get_multipexer().m_remote_address.to_string()
                    << ", port: " << static_cast< uint32_t >( header.get_port() );
            }
            get_multipexer().check_working_port();
        }
        break;
    case rfcomm_frame_type::ua:
        if( header.get_port() == s_rfcomm_multipexer_channel )
        {
            LogUtilError() << "Not handle multipexer UA frame";
        }
        else
        {
            bool local_inited = header.cr_value_in_header();
            auto port = get_multipexer().find_port( get_multipexer().m_remote_address, header.get_port(), local_inited );
            if( port )
            {
                port->handle_controlling( header );
            }
            else
            {
                LogUtilError() << "Cannot find port. address: " << get_multipexer().m_remote_address.to_string()
                    << ", port: " << static_cast< uint32_t >( header.get_port() ) << ( local_inited ? " local inited" : " remote inited" );
            }
            get_multipexer().check_working_port();
        }
        break;
    default:
        LogUtilError() << "rfcomm controlling frame ignored: " << type;
        break;
    }

}

void rfcomm_multipexer_connected::on_enter()
{

}

void rfcomm_multipexer_connected::on_exit()
{

}

rfcomm_multipexer_disc_wait_ua::rfcomm_multipexer_disc_wait_ua( rfcomm_multipexer& a_sm )
    : rfcomm_state_base( a_sm, rfcomm_multipexer_state_type::disc_wait_ua )
{

}

bool rfcomm_multipexer_disc_wait_ua::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_multipexer_disc_wait_ua::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    return true;
}

void rfcomm_multipexer_disc_wait_ua::on_enter()
{

}

void rfcomm_multipexer_disc_wait_ua::on_exit()
{

}

rfcomm_multipexer::rfcomm_multipexer()
{
    std::shared_ptr<state_machine::abstract_state> state_;
    state_ = std::make_shared<rfcomm_multipexer_idle>( *this );
    add_state( state_ );
    set_initial_state( state_ );

    add_state( std::make_shared<rfcomm_multipexer_wait_conn_cnf>( *this ) );
    add_state( std::make_shared<rfcomm_multipexer_configure>( *this ) );
    add_state( std::make_shared<rfcomm_multipexer_sabm_wait_ua>( *this ) );
    add_state( std::make_shared<rfcomm_multipexer_wait_sabm>( *this ) );
    add_state( std::make_shared<rfcomm_multipexer_connected>( *this ) );
    add_state( std::make_shared<rfcomm_multipexer_disc_wait_ua>( *this ) );

    start();
}

uint16_t rfcomm_multipexer::get_local_cid()
{
    return m_local_cid;
}

void rfcomm_multipexer::accept_coming_connection_request
    (
    uint8_t a_port,
    bool a_accept
    )
{
    std::shared_ptr<rfcomm_port> port;
    port = find_port( m_remote_address, a_port, false );
    if( port )
    {
        port->handle_accept_connection_request( a_accept );
    }
    else
    {
        LogUtilError() << "Cannot find port for address: " << m_remote_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port );
    }
}

void rfcomm_multipexer::disconnect( uint8_t a_port, bool a_port_on_local )
{
    std::shared_ptr<rfcomm_port> port;
    port = find_port( m_remote_address, a_port, a_port_on_local );
    if( port )
    {
        port->disconnect();
    }
    else
    {
        LogUtilError() << "Cannot find port for address: " << m_remote_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port );
    }
}

void rfcomm_multipexer::send_port_user_data
    (
    uint8_t a_port,
    bool a_port_on_local,
    std::shared_ptr<std::vector<uint8_t>> a_spp_data
    )
{
    std::shared_ptr<rfcomm_port> port;
    port = find_port( m_remote_address, a_port, a_port_on_local );
    if( port )
    {
        port->send_user_data( a_spp_data );
    }
    else
    {
        LogUtilError() << "Cannot find port for address: " << m_remote_address.to_string()
            << ", port: " << static_cast< uint32_t >( a_port );
    }
}

void rfcomm_multipexer::handle_channel_connection_request( std::shared_ptr<connection_request> const& a_request )
{
    std::shared_ptr<rfcomm_multipexer_event> event_;
    event_ = std::make_shared<rfcomm_multipexer_event>();
    event_->m_signaling_pkt = a_request;
    event_->m_type = rfcomm_event_type::l2cap_signaling_message;

    handle_event( event_ );
}

void rfcomm_multipexer::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    /**
     * TODO check the configuration request can be accpeted or not.
     */

    std::shared_ptr<rfcomm_multipexer_event> event_;
    event_ = std::make_shared<rfcomm_multipexer_event>();
    event_->m_signaling_pkt = a_request;
    event_->m_type = rfcomm_event_type::l2cap_signaling_message;

    handle_event( event_ );
}

void rfcomm_multipexer::handle_config_response( std::shared_ptr<l2cap_config_response> const& a_reponse )
{
    std::shared_ptr<rfcomm_multipexer_event> event_;
    event_ = std::make_shared<rfcomm_multipexer_event>();
    event_->m_signaling_pkt = a_reponse;
    event_->m_type = rfcomm_event_type::l2cap_signaling_message;

    handle_event( event_ );
}

void rfcomm_multipexer::handle_sdu( std::shared_ptr<hci_data> const& a_hci_data )
{
    rfcomm_header rfc_header;
    bool valid = packet_valid( a_hci_data, rfc_header );
    if( !valid )
    {
        return;
    }

    std::shared_ptr<rfcomm_multipexer_event> event_;
    event_ = std::make_shared<rfcomm_multipexer_event>();
    event_->m_rfcomm_header = rfc_header;
    event_->m_type = rfcomm_event_type::multipexer_controlling;
    event_->m_hci_data = a_hci_data;

    handle_event( event_ );
}

void rfcomm_multipexer::handle_connection_state_changed
    (
    bluetooth_address a_address,
    uint16_t a_local_cid,
    uint16_t a_remote_cid,
    l2cap_channel_state_type a_state
    )
{
    m_local_cid = a_local_cid;

    std::shared_ptr<rfcomm_multipexer_event> event_;
    event_ = std::make_shared<rfcomm_multipexer_event>();
    event_->m_address = a_address;
    event_->m_local_cid = a_local_cid;
    event_->m_remote_cid = a_remote_cid;
    event_->m_state = a_state;
    event_->m_type = rfcomm_event_type::l2cap_connetion_result;

    handle_event( event_ );
}

void rfcomm_multipexer::clear()
{
    m_local_cid = 0x00;
    m_remote_mtu = 0x00;
    m_remote_address = bluetooth_address::s_empty_address;
    m_wait_config_rsp_flag = false;
    m_remote_configured = false;
    m_local_inited = false;
}

bool rfcomm_multipexer::packet_valid
    (
    std::shared_ptr<hci_data> const& a_hci_data,
    rfcomm_header& a_header
    )
{
    uint8_t* p_l2cap = a_hci_data->m_buffer.data();

    // for empty information field rfcomm packet, the sdu size is 4.
    if( a_hci_data->m_buffer.size() < a_header.get_min_header_size() + 1 )
    {
        LogUtilError() << "Rfcomm packet size wrong.";
        return false;
    }

    a_header.parse_from_raw_data( p_l2cap, a_hci_data->m_buffer.size() );

    uint16_t info_length = a_header.get_information_length();
    if( a_hci_data->m_buffer.size() < info_length + a_header.header_size() + 1 )
    {
        LogUtilError() << "rfcomm information length size wrong";
        return false;
    }

    uint8_t fcs_value = p_l2cap[a_header.header_size() + info_length];
    uint8_t* p_sdu = p_l2cap + a_header.l2cap_header::header_size();
    bool fcs_check = false;
    switch( a_header.get_frame_type() )
    {
    case rfcomm_frame_type::sabm:
    case rfcomm_frame_type::disc:
    case rfcomm_frame_type::ua:
    case rfcomm_frame_type::dm:
        fcs_check = rfcomm_check_fcs( p_sdu, s_fc_cal_size, fcs_value );
        break;
    case rfcomm_frame_type::uih:
        fcs_check = rfcomm_check_fcs( p_sdu, s_fcs_uih_cal_size, fcs_value );
        break;
    default:
        LogUtilError() << "Unknown rtfcomm frame type";
        return false;
    }

    if( !fcs_check )
    {
        LogUtilError() << "fcs check error.";
        return false;
    }

    return true;
}

void rfcomm_multipexer::handle_uih_frame
    (
    std::shared_ptr<hci_data> const& a_hci_data,
    rfcomm_header& a_header
    )
{
    uint8_t ch = a_header.get_channel_number();
    if( 0x00 == ch )
    {
        std::shared_ptr<multipexer_message> msg;
        uint8_t* p = a_hci_data->m_buffer.data() + a_header.header_size();
        uint16_t msg_size = static_cast<uint16_t>( a_hci_data->m_buffer.size() - a_header.header_size() );
        msg = multipexer_message::parse_from( p, msg_size );
        if( !msg )
        {
            return;
        }

        switch( msg->get_type() )
        {
        case multipexer_message_type::pn:
            handle_multipexer_pn( a_header, std::static_pointer_cast<multipexer_pn_message>( msg ) );
            break;
        case multipexer_message_type::msc:
            handle_multipexer_msc( a_header, std::static_pointer_cast<multipexer_msc_message>( msg ) );
            break;
        default:
            LogUtilError() << "multipexer signaling not handled: " << msg->get_type();
            break;
        }
    }
    else
    {
        /**
         * If remote device is the port connection initor, then the remote device will set this CR bit here.
         */
        bool local_inited = !( a_header.cr_value_in_header() );

        auto acl_db = framework_manager::get_instance().get_info_manager()
            .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
        auto [remote_address, has_address] = acl_db->get_address( a_header.get_acl_handle() );
        if( !has_address )
        {
            LogUtilError() << "Cannot find acl handle from the acl db. handle: " << a_header.get_acl_handle();
            return;
        }

        uint8_t port = a_header.get_port();
        std::shared_ptr<rfcomm_port> p_port = find_port( remote_address, port, local_inited );
        if( p_port )
        {
            p_port->handle_received_uih_data( a_hci_data, a_header );
        }
        else
        {
            LogUtilError() << "No local port entity for address: " << remote_address.to_string() << ", port: "
                << port << ", " << ( local_inited ? "local inited" : "remote inited" );
        }
    }
}

void rfcomm_multipexer::connect()
{
    // TODO make a l2cap channel connection
}

void rfcomm_multipexer::config_local_channel( uint16_t a_acl_handle, uint16_t a_remote_cid )
{
    std::vector<channel_config_option> channel_cfg_options;
    channel_config_option cfg;
    cfg.m_type = channel_config_option_type::mtu;
    cfg.m_option.m_mtu = 1024;
    channel_cfg_options.push_back( cfg );

    std::shared_ptr<l2cap_config_local_channel_request> cfg_request;
    cfg_request = std::make_shared<l2cap_config_local_channel_request>();
    cfg_request->m_options = std::move( channel_cfg_options );
    cfg_request->m_acl_handle = a_acl_handle;
    cfg_request->m_remote_cid = a_remote_cid;

    std::shared_ptr<l2cap_task_request_config_local_channel> task;
    task = std::make_shared<l2cap_task_request_config_local_channel>();
    task->set_source_module( rfcomm_module::s_rfcomm_module_name );
    task->m_config_local = cfg_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    m_wait_config_rsp_flag = true;
}

void rfcomm_multipexer::handle_multipexer_pn
    (
    rfcomm_header const& a_rfc_header,
    std::shared_ptr<multipexer_pn_message> const& a_pn
    )
{
    bool local_inited = !( a_pn->is_command() );
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [remote_address,has_address] = acl_db->get_address( a_rfc_header.get_acl_handle() );
    if( !has_address )
    {
        LogUtilError() << "Cannot find acl handle from the acl db. handle: " << a_rfc_header.get_acl_handle();
        return;
    }

    uint8_t port = a_pn->get_port();

    std::shared_ptr<rfcomm_port> p_port = find_port( remote_address, port, local_inited );
    if( !p_port )
    {
        if( !m_idle_ports.empty() )
        {
            LogUtilInfo() << "Use previously used port";
            p_port = m_idle_ports.front();
            m_idle_ports.erase( m_idle_ports.begin() );
            m_worked_ports.push_back( p_port );
        }
        else
        {
            LogUtilInfo() << "Allocate a new rfcomm_port to handle coming UIH PN";
            p_port = std::make_shared<rfcomm_port>();
            m_worked_ports.emplace_back( p_port );
        }
        p_port->set_dlci( a_pn->get_dlci() );
        p_port->set_local_inited( local_inited );
        p_port->set_remote_device( remote_address );
        p_port->set_control_sender( std::bind( &rfcomm_multipexer::send_multipexer_message,
            this, std::placeholders::_1 ) );
        p_port->set_port_sender( std::bind( &rfcomm_multipexer::send_port_data, this,
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
        auto callback = m_callback_finder( p_port->get_port(), local_inited );
        if( callback )
        {
            p_port->set_port_callback( callback );
        }
        else
        {
            LogUtilError() << "There is no callback register for port: " <<
                static_cast< uint16_t >( p_port->get_port() ) << ", local inited: "
                << std::boolalpha << local_inited;
        }
    }

    p_port->handle_connection_request( a_pn );
}

void rfcomm_multipexer::handle_multipexer_msc
    (
    rfcomm_header const& a_rfc_header,
    std::shared_ptr<multipexer_msc_message> const& a_msc
    )
{
    bool local_inited = !a_rfc_header.cr_value_in_header();
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [remote_address, has_address] = acl_db->get_address( a_rfc_header.get_acl_handle() );
    if( !has_address )
    {
        LogUtilError() << "Cannot find acl handle from the acl db. handle: " << a_rfc_header.get_acl_handle();
        return;
    }

    uint8_t port = a_msc->get_port();

    std::shared_ptr<rfcomm_port> p_port = find_port( remote_address, port, local_inited );
    if( !p_port )
    {
        LogUtilError() << "No port entity for this port connection. address: " << remote_address.to_string()
            << ", port: " << static_cast<uint16_t>( port ) << ", " << ( local_inited ? "local inited" : "remote inited" );
        return;
    }

    p_port->handle_modem_status_message( a_msc );
}

void rfcomm_multipexer::send_sabm_frame()
{
    rfcomm_header header;
    header.set_frame_type( rfcomm_frame_type::sabm );
    header.set_channel_number( s_rfcomm_multipexer_channel );
    header.set_poll_final( true );
    header.set_sdu_length( 0x00 );

    uint8_t buffer[100];
    header.to_raw_buffer( buffer, 100 );

    uint8_t fcs = rfcomm_calc_fcs( buffer + header.l2cap_header::header_size(), 3 );
    std::vector<uint8_t> hci_buffer( buffer, buffer + header.header_size() );
    hci_buffer.push_back( fcs );

    auto hci_ = std::make_shared<hci_data>();
    hci_->m_buffer.swap( hci_buffer );
    hci_->m_from_controller = false;
    hci_->m_type = uart_hci_type::acl_type;

    std::shared_ptr<l2cap_task_send_l2cap_sdu> tsk;
    tsk = std::make_shared<l2cap_task_send_l2cap_sdu>();
    tsk->m_local_cid = m_local_cid;
    tsk->m_hci_packet = hci_;
    tsk->m_remote_address = m_remote_address;

    tsk->set_source_module( rfcomm_module::s_rfcomm_module_name );
    tsk->set_target_module( l2cap_module::s_l2cap_module_name );
    tsk->set_position( source_here );
    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void rfcomm_multipexer::send_ua_frame()
{
    rfcomm_header header;
    header.set_frame_type( rfcomm_frame_type::ua );
    header.set_channel_number( s_rfcomm_multipexer_channel );
    header.set_poll_final( true );
    header.set_sdu_length( 0x00 );
    header.set_cr( !m_local_inited );

    uint8_t buffer[100];
    header.to_raw_buffer( buffer, 100 );
    uint8_t fcs = rfcomm_calc_fcs( buffer + header.l2cap_header::header_size(), s_fc_cal_size );
    std::vector<uint8_t> hci_buffer( buffer, buffer + header.header_size() );
    hci_buffer.push_back( fcs );

    auto hci_ = std::make_shared<hci_data>();
    hci_->m_buffer.swap( hci_buffer );
    hci_->m_from_controller = false;
    hci_->m_type = uart_hci_type::acl_type;

    std::shared_ptr<l2cap_task_send_l2cap_sdu> tsk;
    tsk = std::make_shared<l2cap_task_send_l2cap_sdu>();
    tsk->m_local_cid = m_local_cid;
    tsk->m_hci_packet = std::move(hci_);
    tsk->m_remote_address = m_remote_address;

    tsk->set_source_module( rfcomm_module::s_rfcomm_module_name );
    tsk->set_target_module( l2cap_module::s_l2cap_module_name );
    tsk->set_position( source_here );
    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void rfcomm_multipexer::send_multipexer_message( std::shared_ptr<multipexer_message> a_pn )
{
    rfcomm_header header;
    header.set_frame_type( rfcomm_frame_type::uih );
    header.set_channel_number( s_rfcomm_multipexer_channel );
    header.set_poll_final( true );
    header.set_sdu_length( a_pn->get_raw_buffer_size() );
    header.set_cr( !m_local_inited );

    uint8_t buffer[100];
    header.to_raw_buffer( buffer, 100 );

    a_pn->to_buffer( buffer + header.header_size(), 100 - header.header_size() - 1 );

    uint8_t fcs = rfcomm_calc_fcs( buffer + header.l2cap_header::header_size(), s_fcs_uih_cal_size );
    std::vector<uint8_t> hci_buffer( buffer, buffer + header.header_size() + a_pn->get_raw_buffer_size() );
    hci_buffer.push_back( fcs );

    auto hci_ = std::make_shared<hci_data>();
    hci_->m_buffer.swap( hci_buffer );
    hci_->m_from_controller = false;
    hci_->m_type = uart_hci_type::acl_type;

    std::shared_ptr<l2cap_task_send_l2cap_sdu> tsk;
    tsk = std::make_shared<l2cap_task_send_l2cap_sdu>();
    tsk->m_local_cid = m_local_cid;
    tsk->m_hci_packet = std::move(hci_);
    tsk->m_remote_address = m_remote_address;

    tsk->set_source_module( rfcomm_module::s_rfcomm_module_name );
    tsk->set_target_module( l2cap_module::s_l2cap_module_name );
    tsk->set_position( source_here );
    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void rfcomm_multipexer::send_port_data( rfcomm_header a_header, uint8_t* a_buffer, uint16_t a_size )
{
    uint8_t buffer[100];
    a_header.to_raw_buffer( buffer, 100 );
    uint8_t fcs = 0x00;
    if( a_header.get_frame_type() == rfcomm_frame_type::uih )
    {
        fcs = rfcomm_calc_fcs( buffer + a_header.l2cap_header::header_size(), s_fcs_uih_cal_size );
    }
    else
    {
        fcs = rfcomm_calc_fcs( buffer + a_header.l2cap_header::header_size(), s_fc_cal_size );
    }

    std::vector<uint8_t> hci_buffer( buffer, buffer + a_header.header_size() );
    hci_buffer.insert( hci_buffer.end(), a_buffer, a_buffer + a_size );
    hci_buffer.push_back( fcs );

    auto hci_ = std::make_shared<hci_data>();
    hci_->m_buffer.swap( hci_buffer );
    hci_->m_from_controller = false;
    hci_->m_type = uart_hci_type::acl_type;

    std::shared_ptr<l2cap_task_send_l2cap_sdu> tsk;
    tsk = std::make_shared<l2cap_task_send_l2cap_sdu>();
    tsk->m_local_cid = m_local_cid;
    tsk->m_hci_packet = std::move(hci_);
    tsk->m_remote_address = m_remote_address;

    tsk->set_source_module( rfcomm_module::s_rfcomm_module_name );
    tsk->set_target_module( l2cap_module::s_l2cap_module_name );
    tsk->set_position( source_here );
    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

std::shared_ptr<rfcomm_port> rfcomm_multipexer::find_port
    (
    bluetooth_address const& a_address,
    uint8_t const& a_port,
    bool a_local_inited
    )
{
    for( auto& ele : m_worked_ports )
    {
        if( a_address == ele->get_remote_device() &&
            a_port == ele->get_port() &&
            a_local_inited == ele->local_inited() )
        {
            return ele;
        }
    }

    return nullptr;
}

void rfcomm_multipexer::check_working_port()
{
    for( auto it = m_worked_ports.begin(); it != m_worked_ports.end(); )
    {
        if( !( *it )->is_working() )
        {
            m_idle_ports.push_back( *it );
            it = m_worked_ports.erase( it );
        }
        else
        {
            ++it;
        }
    }
}

}

