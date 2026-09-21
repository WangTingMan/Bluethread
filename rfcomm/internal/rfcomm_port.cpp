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

#include "rfcomm_port.h"
#include "../rfcomm_module.h"

#include "framework/log_util.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"

static constexpr bool s_local_support_credit = true;
static constexpr uint8_t s_local_credit_init_value = 0x07;
static constexpr uint16_t s_local_credit_report_threshold = 10;
static constexpr uint8_t s_max_credit_local = 0x0FF;

namespace bluetooth
{

using namespace framework;

std::ostream& operator<<( std::ostream& a_os, rfcomm_port_state_type a_state )
{
    switch( a_state )
    {
    case bluetooth::rfcomm_port_state_type::idle_state:
        a_os << "idle";
        break;
    case bluetooth::rfcomm_port_state_type::wait_sabm_state:
        a_os << "wait_sabm";
        break;
    case bluetooth::rfcomm_port_state_type::wait_pn_rsp_state:
        a_os << "wait_pn_rsp";
        break;
    case bluetooth::rfcomm_port_state_type::modem_config_state:
        a_os << "modem_config";
        break;
    case bluetooth::rfcomm_port_state_type::disc_wait_ua_state:
        a_os << "disc_wait_ua_state";
        break;
    case bluetooth::rfcomm_port_state_type::sabm_wait_ua_state:
        a_os << "sabm_wait_ua_state";
        break;
    case bluetooth::rfcomm_port_state_type::open:
        a_os << "open";
        break;
    default:
        a_os << "Unknown state: " << static_cast< uint16_t >( a_state );
        break;
    }
    return a_os;
}

rfcomm_port_base_state::rfcomm_port_base_state( rfcomm_port& a_sm, rfcomm_port_state_type a_type )
    : state_machine::abstract_state( a_sm, static_cast< uint32_t >( a_type ) )
    , m_rfcomm_port( a_sm )
{
}

void rfcomm_port_base_state::transition_to_state( rfcomm_port_state_type a_type )
{
    rfcomm_port_state_type previous = static_cast< rfcomm_port_state_type >( get_id() );
    if( previous == a_type )
    {
        return;
    }

    LogUtilInfo() << "rfcomm port for device " << m_rfcomm_port.m_remote_device.to_string()
        << ", port: " << static_cast< uint32_t >( m_rfcomm_port.get_port() )
        << ( m_rfcomm_port.m_local_inited ? ", local inited" : ", remote inited" )
        << ", from state <" << previous << "> to <" << a_type << ">";
    transition_to( static_cast< uint32_t >( a_type ) );
}

bool rfcomm_port_idle_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_idle_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_port_event> event_;
    event_ = std::static_pointer_cast<rfcomm_port_event>( a_event );
    if( !event_ )
    {
        LogUtilError() << "Unknown event received in idle state";
        return false;
    }

    switch( event_->m_type )
    {
    case rfcomm_port_event_type::multipexer_message_type:
        if( !event_->m_mulx_msg )
        {
            LogUtilError() << "Not set mulx msg";
            return false;
        }

        switch( event_->m_mulx_msg->get_type() )
        {
        case multipexer_message_type::pn:
            handle_connection_request( std::static_pointer_cast< multipexer_pn_message >( event_->m_mulx_msg ) );
            break;
        default:
            LogUtilError() << "event type not handle: " << event_->m_mulx_msg->get_type();
            break;
        }
        break;
    case rfcomm_port_event_type::send_multipexer_meesage_type:
        if( !event_->m_mulx_msg )
        {
            LogUtilError() << "Not set mulx msg";
            return false;
        }

        switch( event_->m_mulx_msg->get_type() )
        {
        case multipexer_message_type::pn:
            send_connection_request( std::static_pointer_cast<multipexer_pn_message>( event_->m_mulx_msg ) );
            break;
        default:
            LogUtilError() << "event type not handle: " << event_->m_mulx_msg->get_type();
            break;
        }
        break;
    default:
        LogUtilError() << "event type not handle: " << static_cast<uint16_t>( event_->m_type );
        break;
    }

    return true;
}

void rfcomm_port_idle_state::handle_connection_request( std::shared_ptr<multipexer_pn_message> const& a_pn )
{
    get_port().m_remote_credit_supported = a_pn->support_credit();
    get_port().m_remote_max_frame_size = a_pn->max_frame_size();
    get_port().m_remote_credit_value = a_pn->credit_init_value();

    bool is_cmd = a_pn->is_command();
    LogUtilInfo() << "PN is command: " << std::boolalpha << is_cmd
        << ", is local inited: " << get_port().m_local_inited << ", remote support credit: "
        << a_pn->support_credit() << ", remote credit value: " << static_cast<uint16_t>( a_pn->credit_init_value() );

    if( is_cmd && get_port().m_local_inited )
    {
        LogUtilError() << "schedule error. who inited, who send pn cmd";
        return;
    }

    if( !is_cmd && !get_port().m_local_inited )
    {
        LogUtilError() << "schedule error. who inited, who should not send rsp";
        return;
    }

    std::shared_ptr<multipexer_pn_message> pn_rsp;
    get_port().m_local_credit_value = 0x07;
    pn_rsp = std::make_shared<multipexer_pn_message>();
    pn_rsp->set_is_command( false );
    pn_rsp->set_dlci( a_pn->get_dlci() );
    pn_rsp->set_support_credit( s_local_support_credit );
    pn_rsp->set_priority( 0x00 );
    pn_rsp->set_max_frame_size( 996 );
    pn_rsp->set_credit_init_value( get_port().m_local_credit_value );

    if( get_port().m_multi_sender )
    {
        get_port().m_multi_sender( pn_rsp );
    }
    else
    {
        LogUtilError() << "No controlling sender";
        return;
    }

    transition_to_state( rfcomm_port_state_type::wait_sabm_state );
}

void rfcomm_port_idle_state::send_connection_request( std::shared_ptr<multipexer_pn_message> const& a_pn )
{
    if( !a_pn )
    {
        LogUtilError() << "Not set multipexer_pn_message.";
        return;
    }

    if( get_port().m_multi_sender )
    {
        get_port().m_multi_sender( a_pn );
        transition_to_state( rfcomm_port_state_type::wait_pn_rsp_state );
    }
    else
    {
        LogUtilError() << "m_multi_sender null";
    }
}

void rfcomm_port_idle_state::on_enter()
{
    get_port().m_local_credit_value = s_local_credit_init_value;
    if( get_port().previous_state_id() != state_machine::invalid_state )
    {
        auto& callback_cb = get_port().m_port_callback;
        if( callback_cb && callback_cb->data_callback )
        {
            if( !callback_cb->handle_module.empty() )
            {
                std::function<void()> fun;
                fun = std::bind( callback_cb->connection_changed_callback, get_port().m_remote_device,
                    connection_status::disconnected );
                std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
                task->set_fun( fun, callback_cb->handle_module );
                task->set_source_module( rfcomm_module::s_rfcomm_module_name );
                framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            }
            else
            {
                callback_cb->connection_changed_callback( get_port().m_remote_device,
                    connection_status::disconnected );
            }
        }
        get_port().clear();
    }
}

void rfcomm_port_idle_state::on_exit()
{

}

bool rfcomm_port_wait_sabm_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_wait_sabm_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_port_event> event_;
    event_ = std::static_pointer_cast<rfcomm_port_event>( a_event );
    if( !event_ )
    {
        LogUtilError() << "Unknown event received in idle state";
        return false;
    }

    switch( event_->m_type )
    {
    case rfcomm_port_event_type::controlling_message:
        {
            rfcomm_header* header = event_->m_rfcomm_header;
            std::shared_ptr<rfcomm_port_callback_block> callback;
            callback = get_port().m_port_callback;
            switch( header->get_frame_type() )
            {
            case rfcomm_frame_type::sabm:
                get_port().m_local_inited = false;

                if( !callback )
                {
                    get_port().accept_connection_request( false );
                    transition_to_state( rfcomm_port_state_type::idle_state );
                    LogUtilError() << "reject connection request. due to no callback. state port = "
                        << static_cast< uint32_t >( get_port().get_port() ) << std::boolalpha
                        << get_port().m_local_inited;
                    return true;
                }

                if( callback->port_number != get_port().get_port() ||
                    callback->local_inited != false )
                {
                    get_port().accept_connection_request( false );
                    transition_to_state( rfcomm_port_state_type::idle_state );
                    LogUtilError() << "reject connection request. callback: port = " << static_cast< uint32_t >( callback->port_number )
                        << ", local inited = " << std::boolalpha << callback->local_inited
                        << "; state port = " << static_cast< uint32_t >( get_port().get_port() )
                        << ", local inited = " << get_port().m_local_inited;
                    return true;
                }

                if( callback->connection_request_callback )
                {
                    if( !callback->handle_module.empty() )
                    {
                        std::function<void()> fun;
                        fun = std::bind( callback->connection_request_callback, get_port().m_remote_device );

                        std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
                        task->set_fun( fun, callback->handle_module );
                        task->set_source_module( rfcomm_module::s_rfcomm_module_name );
                        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
                    }
                    else
                    {
                        callback->connection_request_callback( get_port().m_remote_device );
                    }
                }
                else
                {
                    get_port().accept_connection_request( false );
                    transition_to_state( rfcomm_port_state_type::idle_state );
                    LogUtilError() << "reject connection request. due to no callback. state port = "
                        << static_cast< uint32_t >( get_port().get_port() ) << std::boolalpha
                        << get_port().m_local_inited;
                }
                break;
            default:
                LogUtilError() << "frame type ignored: " << header->get_frame_type();
                break;
            }
        }
        break; 
    case rfcomm_port_event_type::accept_connection_request:
        get_port().accept_connection_request( true );
        transition_to_state( rfcomm_port_state_type::modem_config_state );
        break;
    case rfcomm_port_event_type::reject_connection_request:
        get_port().accept_connection_request( false );
        transition_to_state( rfcomm_port_state_type::idle_state );
        break;
    default:
        LogUtilError() << "event type not handle: " << static_cast<uint16_t>( event_->m_type );
        break;
    }

    return true;
}

void rfcomm_port_wait_sabm_state::on_enter()
{

}

void rfcomm_port_wait_sabm_state::on_exit()
{

}

bool rfcomm_port_wait_pn_rsp_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_wait_pn_rsp_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    return true;
}

void rfcomm_port_wait_pn_rsp_state::on_enter()
{

}

void rfcomm_port_wait_pn_rsp_state::on_exit()
{

}

bool rfcomm_port_modem_config_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_modem_config_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_port_event> event_;
    event_ = std::static_pointer_cast<rfcomm_port_event>( a_event );
    if( !event_ )
    {
        LogUtilError() << "Unknown event received in idle state";
        return false;
    }

    switch( event_->m_type )
    {
    case rfcomm_port_event_type::multipexer_message_type:
        if( !event_->m_mulx_msg )
        {
            LogUtilError() << "Not set mulx msg";
            return false;
        }

        switch( event_->m_mulx_msg->get_type() )
        {
        case multipexer_message_type::msc:
            handle_modem_status_message( std::static_pointer_cast<multipexer_msc_message>( event_->m_mulx_msg ) );
            break;
        default:
            LogUtilError() << "event type not handle: " << event_->m_mulx_msg->get_type();
            break;
        }
        break;
    case rfcomm_port_event_type::disconnect_port:
        {
            LogUtilInfo() << "Send DISC to remote device to discoonect current port";
            rfcomm_header header_send;
            header_send.set_dlci( get_port().get_dlci() );
            header_send.set_cr( get_port().m_local_inited ? true : false );
            header_send.set_frame_type( rfcomm_frame_type::disc );
            header_send.set_poll_final( true );
            header_send.set_sdu_length( 0x00 );
            get_port().m_port_sender( header_send, nullptr, 0x00 );
            transition_to( static_cast<uint32_t>( rfcomm_port_state_type::disc_wait_ua_state ) );
        }
        break;
    default:
        LogUtilError() << "Event type ignored: " << static_cast<uint32_t>( event_->m_type );
        break;
    }

    return true;
}

void rfcomm_port_modem_config_state::handle_modem_status_message( std::shared_ptr<multipexer_msc_message> const& a_msc )
{
    if( a_msc->is_command() )
    {
        auto& remote = get_port().m_remote_modem_status;
        remote.m_data_valid = a_msc->data_valid();
        remote.m_flow_control_on = a_msc->flow_control_on();
        remote.m_incoming_call = a_msc->incoming_call();
        remote.m_ready_communicated = a_msc->ready_communicated();
        remote.m_ready_received = a_msc->ready_received();

        std::shared_ptr<multipexer_msc_message> rsp;
        rsp = std::make_shared<multipexer_msc_message>();
        rsp->set_is_command( false );
        rsp->set_data_valid( remote.m_data_valid );
        rsp->set_flow_control_on( remote.m_flow_control_on );
        rsp->set_incoming_call( remote.m_incoming_call );
        rsp->set_ready_communicated( remote.m_ready_communicated );
        rsp->set_ready_received( remote.m_ready_received );
        rsp->set_dlci( a_msc->get_dlci() );
        if( get_port().m_multi_sender )
        {
            get_port().m_multi_sender( rsp );
        }
        else
        {
            LogUtilError() << "Not set multipexer UIH command/response sender";
            return;
        }

        get_port().m_remote_modem_status_configed = true;
    }
    else
    {
        get_port().m_local_modem_status_configed = true;
    }

    if( get_port().m_remote_modem_status_configed &&
        get_port().m_local_modem_status_configed )
    {
        transition_to_state( rfcomm_port_state_type::open );
    }
}

void rfcomm_port_modem_config_state::send_local_modem_status()
{
    auto& local = get_port().m_local_modem_status;
    std::shared_ptr<multipexer_msc_message> request;
    request = std::make_shared<multipexer_msc_message>();
    request->set_is_command( true );
    request->set_data_valid( local.m_data_valid );
    request->set_flow_control_on( local.m_flow_control_on );
    request->set_incoming_call( local.m_incoming_call );
    request->set_ready_communicated( local.m_ready_communicated );
    request->set_ready_received( local.m_ready_received );

    request->set_dlci( get_port().get_dlci() );

    if( get_port().m_multi_sender )
    {
        get_port().m_multi_sender( request );
    }
    else
    {
        LogUtilError() << "Not set multipexer UIH command/response sender";
        return;
    }
}

void rfcomm_port_modem_config_state::on_enter()
{
    send_local_modem_status();
}

void rfcomm_port_modem_config_state::on_exit()
{

}

bool rfcomm_port_sabm_wait_ua_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_sabm_wait_ua_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    return true;
}

void rfcomm_port_sabm_wait_ua_state::on_enter()
{

}

void rfcomm_port_sabm_wait_ua_state::on_exit()
{

}

bool rfcomm_port_disc_wait_ua_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_disc_wait_ua_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_port_event> event_;
    event_ = std::static_pointer_cast< rfcomm_port_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Unknown event received in idle state";
        return false;
    }

    switch( event_->m_type )
    {
    case rfcomm_port_event_type::controlling_message:

        if( !event_->m_rfcomm_header )
        {
            LogUtilError() << "No rfcomm header to deal with";
            return false;
        }

        switch( event_->m_rfcomm_header->get_frame_type() )
        {
        case rfcomm_frame_type::ua:
            transition_to_state( rfcomm_port_state_type::idle_state );
            break;
        default:
            LogUtilError() << "Event not handled: " << event_->m_rfcomm_header->get_frame_type();
        }

        break;
    default:
        LogUtilError() << "Event not handled: " << static_cast<uint32_t>( event_->m_type );
    }

    return true;
}

void rfcomm_port_disc_wait_ua_state::on_enter()
{

}

void rfcomm_port_disc_wait_ua_state::on_exit()
{

}

bool rfcomm_port_open_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool rfcomm_port_open_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<rfcomm_port_event> event_;
    event_ = std::static_pointer_cast< rfcomm_port_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Unknown event received in idle state";
        return false;
    }

    switch( event_->m_type )
    {
    case rfcomm_port_event_type::port_uih_type:
        if( nullptr == event_->m_raw_data )
        {
            LogUtilError() << "No raw data set";
            return false;
        }

        if( nullptr == event_->m_rfcomm_header )
        {
            LogUtilError() << "No rfcomm header set";
            return false;
        }

        get_port().handle_received_uih_data_internal( event_->m_raw_data, *( event_->m_rfcomm_header ) );
        break;
    case rfcomm_port_event_type::controlling_message:
        {
            rfcomm_header* header = event_->m_rfcomm_header;
            rfcomm_header header_send;
            bool local_inited = !header->cr_value_in_header();
            switch( header->get_frame_type() )
            {
            case rfcomm_frame_type::disc:
                header_send.set_dlci( header->get_dlci() );
                header_send.set_cr( local_inited ? true : false );
                header_send.set_frame_type( rfcomm_frame_type::ua );
                header_send.set_poll_final( true );
                header_send.set_sdu_length( 0x00 );
                get_port().m_port_sender( header_send, nullptr, 0x00 );
                transition_to_state( rfcomm_port_state_type::idle_state );
                break;
            default:
                LogUtilError() << "rfcomm controlling frame ignored: " << header->get_frame_type();
                break;
            }
        }
        break;
    case rfcomm_port_event_type::disconnect_port:
        {
            LogUtilInfo() << "Send DISC to remote device to discoonect current port";
            rfcomm_header header_send;
            header_send.set_dlci( get_port().get_dlci() );
            header_send.set_cr( get_port().m_local_inited ? true : false );
            header_send.set_frame_type( rfcomm_frame_type::disc );
            header_send.set_poll_final( true );
            header_send.set_sdu_length( 0x00 );
            get_port().m_port_sender( header_send, nullptr, 0x00 );
            transition_to_state( rfcomm_port_state_type::disc_wait_ua_state );
        }
        break;
    default:
        LogUtilError() << "Event type not handled: " << static_cast<uint16_t>( event_->m_type );
        break;
    }

    return true;
}

void rfcomm_port_open_state::on_enter()
{
    auto& callback_cb = get_port().m_port_callback;
    if( callback_cb )
    {
        if( callback_cb->data_callback )
        {
            if( !callback_cb->handle_module.empty() )
            {
                std::function<void()> fun;
                fun = std::bind( callback_cb->connection_changed_callback, get_port().m_remote_device,
                    connection_status::connected );
                std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
                task->set_fun( fun, callback_cb->handle_module );
                task->set_source_module( rfcomm_module::s_rfcomm_module_name );
                framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            }
            else
            {
                callback_cb->connection_changed_callback( get_port().m_remote_device,
                    connection_status::connected );
            }
        }
    }
}

void rfcomm_port_open_state::on_exit()
{

}

rfcomm_port::rfcomm_port()
{
    m_local_modem_status.m_data_valid = true;
    m_local_modem_status.m_flow_control_on = false;
    m_local_modem_status.m_incoming_call = false;
    m_local_modem_status.m_ready_communicated = true;
    m_local_modem_status.m_ready_received = true;

    std::shared_ptr<state_machine::abstract_state> state_;
    state_ = std::make_shared<rfcomm_port_idle_state>( *this );
    add_state( state_ );
    set_initial_state( state_ );

    add_state( std::make_shared<rfcomm_port_open_state>( *this ) );
    add_state( std::make_shared<rfcomm_port_sabm_wait_ua_state>( *this ) );
    add_state( std::make_shared<rfcomm_port_modem_config_state>( *this ) );
    add_state( std::make_shared<rfcomm_port_wait_pn_rsp_state>( *this ) );
    add_state( std::make_shared<rfcomm_port_wait_sabm_state>( *this ) );
    add_state( std::make_shared<rfcomm_port_disc_wait_ua_state>( *this ) );

    start();
}

void rfcomm_port::handle_multipexer_disconnect()
{
    auto state = find_state( get_id() );
    auto rfc_state = std::static_pointer_cast<rfcomm_port_base_state>( state );
    if( rfc_state )
    {
        rfc_state->transition_to_state( rfcomm_port_state_type::idle_state );
    }
    else
    {
        LogUtilError() << "Cannot find state: " << static_cast< uint32_t >( get_id() );
    }
}

void rfcomm_port::disconnect()
{
    std::shared_ptr<rfcomm_port_event> event_ = std::make_shared<rfcomm_port_event>();
    event_->m_type = rfcomm_port_event_type::disconnect_port;

    handle_event( event_ );
}

void rfcomm_port::send_user_data( std::shared_ptr<std::vector<uint8_t>> a_user_data )
{
    rfcomm_port_state_type state_type = static_cast<rfcomm_port_state_type> ( get_id() );
    if( rfcomm_port_state_type::open != state_type )
    {
        LogUtilError() << "Port not open, cannot send user data";
        return;
    }

    rfcomm_header header;
    header.set_dlci( m_dlci );
    header.set_frame_type( rfcomm_frame_type::uih );
    uint8_t credit_increased = s_max_credit_local - static_cast<uint8_t>( m_local_credit_value );
    m_local_credit_value = s_max_credit_local;
    header.set_credit_increase_value( credit_increased );
    header.set_cr( m_local_inited );

    uint8_t* p_buffer = nullptr;
    uint16_t size = 0;
    if( a_user_data )
    {
        p_buffer = a_user_data->data();
        size = a_user_data->size();
    }

    header.set_sdu_length( size );
    if( m_port_sender )
    {
        m_port_sender( header, p_buffer, size );
    }
    else
    {
        LogUtilError() << "m_port_sender equals null";
    }
}

void rfcomm_port::handle_accept_connection_request( bool a_accept )
{
    std::shared_ptr<rfcomm_port_event> event_ = std::make_shared<rfcomm_port_event>();
    event_->m_type = a_accept ? rfcomm_port_event_type::accept_connection_request
        : rfcomm_port_event_type::reject_connection_request;

    handle_event( event_ );
}

void rfcomm_port::handle_connection_request( std::shared_ptr<multipexer_pn_message> const& a_pn )
{
    std::shared_ptr<rfcomm_port_event> event_ = std::make_shared<rfcomm_port_event>();
    event_->m_type = rfcomm_port_event_type::multipexer_message_type;
    event_->m_mulx_msg = a_pn;

    handle_event( event_ );
}

void rfcomm_port::handle_modem_status_message( std::shared_ptr<multipexer_msc_message> const& a_msc )
{
    std::shared_ptr<rfcomm_port_event> event_ = std::make_shared<rfcomm_port_event>();
    event_->m_type = rfcomm_port_event_type::multipexer_message_type;
    event_->m_mulx_msg = a_msc;

    handle_event( event_ );
}

void rfcomm_port::handle_controlling( rfcomm_header& a_header )
{
    std::shared_ptr<rfcomm_port_event> event_ = std::make_shared<rfcomm_port_event>();
    event_->m_type = rfcomm_port_event_type::controlling_message;
    event_->m_rfcomm_header = &a_header;

    handle_event( event_ );
}

void rfcomm_port::handle_received_uih_data
    (
    std::shared_ptr<hci_data> const& a_hci_data,
    rfcomm_header& a_header
    )
{
    std::shared_ptr<rfcomm_port_event> event_ = std::make_shared<rfcomm_port_event>();
    event_->m_type = rfcomm_port_event_type::port_uih_type;
    event_->m_rfcomm_header = &a_header;
    event_->m_raw_data = a_hci_data;

    handle_event( event_ );
}

void rfcomm_port::clear()
{
    m_local_inited = false;
    m_dlci = 0x00;
    m_remote_device = bluetooth_address::s_empty_address;
    m_remote_credit_supported = false;
    m_remote_max_frame_size = 0x00;
    m_remote_credit_value = 0x00;
    m_local_credit_value = 0x00;
    m_local_modem_status_configed = false;
    m_remote_modem_status_configed = false;
}

void rfcomm_port::handle_received_uih_data_internal
    (
    std::shared_ptr<hci_data> const& a_hci_data,
    rfcomm_header& a_header
    )
{
    if( m_remote_credit_supported )
    {
        --m_local_credit_value;
        if( m_local_credit_value < s_local_credit_report_threshold )
        {
            // Since the credit value is less than s_local_credit_report_threshold, we
            // need to update the credit value to remote device
            force_send_local_credit();
        }
    }

    if( m_port_callback )
    {
        if( m_port_callback->data_callback )
        {
            if( !m_port_callback->handle_module.empty() )
            {
                std::function<void()> fun;
                fun = std::bind( m_port_callback->data_callback, a_hci_data, a_header.header_size(),
                    a_header.get_information_length(), m_remote_device );
                std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
                task->set_fun( fun, m_port_callback->handle_module );
                task->set_source_module( rfcomm_module::s_rfcomm_module_name );
                framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            }
            else
            {
                m_port_callback->data_callback( a_hci_data, a_header.header_size(),
                    a_header.get_information_length(), m_remote_device );
            }
        }
        else
        {
            LogUtilError() << "No data callback for address: " << m_remote_device.to_string()
                << ", port: " << static_cast< uint32_t >( get_port() );
        }
    }
    else
    {
        LogUtilError() << "No data callback cb for address: " << m_remote_device.to_string()
            << ", port: " << static_cast< uint32_t >( get_port() );
    }
}

void rfcomm_port::force_send_local_credit()
{
    send_user_data( nullptr );
}

void rfcomm_port::accept_connection_request( bool a_accept )
{
    rfcomm_header header_send;
    if( a_accept )
    {
        header_send.set_frame_type( rfcomm_frame_type::ua );
    }
    else
    {
        header_send.set_frame_type( rfcomm_frame_type::dm );
    }

    header_send.set_dlci( get_dlci() );
    header_send.set_cr( true );
    header_send.set_poll_final( true ); // We need set the F bit to 1 here.
    header_send.set_sdu_length( 0x00 ); // UA frame do not have information to send.

    if( m_port_sender )
    {
        m_port_sender( header_send, nullptr, 0 );
    }
    else
    {
        LogUtilError() << "port sender is null. cannot send UA response";
    }
}

}

