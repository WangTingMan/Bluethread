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
#include "endian_convert.h"

#include "l2cap_channel_statemachine.h"
#include "l2cap_internal_task.h"
#include "../l2cap_module.h"

#include "../../common/acl_connections_db.h"
#include "../../hci/hci_module.h"

#include "framework/log_util.h"
#include "framework/executable_task.h"
#include "framework/framework_manager.h"
#include "framework/timer_module.h"

static constexpr std::chrono::milliseconds s_upper_layer_confirm_time_out( 10000 );

namespace bluetooth
{

using namespace framework;

l2cap_channel_base_state::l2cap_channel_base_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id )
    : abstract_state( a_sm, a_state_id )
    , m_channel_statemachine( a_sm )
{

}

void l2cap_channel_base_state::transition_to_state( l2cap_channel_state_type a_state )
{
    l2cap_channel_state_type previous_state = static_cast<l2cap_channel_state_type>( get_id() );

    LogUtilInfo() << "local cid: " << m_channel_statemachine.m_local_channel_id
        << ", remote cid: " << m_channel_statemachine.m_remote_channel_id
        << ", state changed from <" << previous_state << "> to <"
        << a_state << ">";

    uint16_t connection_handle = get_statemachine().m_connection_handle;
    uint16_t local_cid = get_statemachine().m_local_channel_id;
    uint16_t remote_cid = get_statemachine().m_remote_channel_id;

    transition_to( static_cast<uint32_t>( a_state ) );

    auto& callbacks = get_statemachine().m_callbacks;
    if( callbacks.m_channel_state_changed_callback )
    {
        auto acl_db = framework_manager::get_instance().get_info_manager()
            .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
        auto [address, has] = acl_db->get_address( connection_handle );
        /**
         * Notify upper layer the channel is closed.
         */
        if( !callbacks.m_handle_module.empty() )
        {
            std::shared_ptr<executable_task> task;
            task = std::make_shared<executable_task>();
            task->set_fun( std::bind( callbacks.m_channel_state_changed_callback,
                                      address,
                                      local_cid,
                                      remote_cid,
                                      a_state,
                                      l2cap_channel_close_reason::no_reason ),
                            callbacks.m_handle_module
                         );
            task->set_source_module( l2cap_module::s_l2cap_module_name );
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        }
        else
        {
            callbacks.m_channel_state_changed_callback( address, local_cid,
                remote_cid, a_state, l2cap_channel_close_reason::no_reason );
        }
    }

}

l2cap_channel_close_state::l2cap_channel_close_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_close_state::handle_event( uint32_t event, void* p_data )
{
    return false;
}

void l2cap_channel_close_state::on_enter()
{
    int previous_state =  get_statemachine().previous_state_id();
    if( previous_state != static_cast<uint32_t>( l2cap_channel_state_type::close_state ) &&
        state_machine::invalid_state != previous_state )
    {
        LogUtilInfo() << "Channel state machine enter close state. local cid: "
            << get_statemachine().m_local_channel_id
            << ", remote cid: " << get_statemachine().m_remote_channel_id
            << ", previous state: " << static_cast<l2cap_channel_state_type>( previous_state );

        std::shared_ptr<l2cap_task_remove_channel_from_cache> tsk;
        tsk = std::make_shared<l2cap_task_remove_channel_from_cache>();
        tsk->m_acl_handle = get_statemachine().m_signaling_channel->get_acl_handle();
        tsk->m_local_cid = get_statemachine().m_local_channel_id;
        tsk->m_acl_type = get_statemachine().m_signaling_channel->get_acl_type();
        tsk->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );

        get_statemachine().clear();
    }
}

void l2cap_channel_close_state::on_exit()
{
}

bool l2cap_channel_close_state::handle_event
    ( 
    std::shared_ptr<state_machine::abstract_event> const& a_event 
    )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast<l2cap_channel_event>( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_connection_req:
            handle_signaling_request( std::static_pointer_cast<connection_request>
                ( event_->m_channel_pkt ) );
            break;
        case signaling_code::l2cap_configuration_req:
            handle_signaling_request( std::static_pointer_cast<l2cap_config_request>
                ( event_->m_channel_pkt ) );
            break;
        case signaling_code::l2cap_disconnection_req:
            handle_signaling_request( std::static_pointer_cast<l2cap_disconnect_request>
                ( event_->m_channel_pkt ) );
            break;
        default:
            LogUtilError() << "signaling code ignored.";
            break;
        }
        break;
    case l2cap_channel_event::event_type::accept_connection_request:
        accept_connection_request( event_ );
        break;
    case l2cap_channel_event::event_type::open_channel_request:
        send_connection_request_to_remote();
        break;
    case l2cap_channel_event::event_type::channel_sdu_pkt_from_upper:
        /*We ignore this event silently.*/
        break;
    default:
        LogUtilDebug() << "Ignore this event in close state." << static_cast<uint16_t>
            ( event_->m_type );
        break;
    }

    return true;
}

void l2cap_channel_close_state::handle_signaling_request
    (
    std::shared_ptr<connection_request> a_requst
    )
{
    auto& callbacks = get_statemachine().m_callbacks;
    if( !callbacks.m_coming_connection_callback )
    {
        LogUtilError() << "No connection callback set. reject coming connection request.";
        get_statemachine().m_signaling_channel->send_connection_response
            (
            a_requst->m_identifier,
            0x00,
            a_requst->m_source_cid,
            connection_req_result::connection_refused_not_support,
            connection_req_refused_status::refused_no_more_info
            );
        return;
    }

    get_statemachine().m_signaling_channel->send_connection_response
        ( a_requst->m_identifier, 0x00, a_requst->m_source_cid,
          connection_req_result::connection_pending,
          connection_req_refused_status::refused_no_more_info
        );

    if( !callbacks.m_handle_module.empty() )
    {
        std::shared_ptr<executable_task> task;
        task = std::make_shared<executable_task>();
        task->set_fun( std::bind( callbacks.m_coming_connection_callback, a_requst ),
            callbacks.m_handle_module );
        task->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( task,
            framework::source_here );
    }
    else
    {
        callbacks.m_coming_connection_callback( a_requst );
    }

    auto state = get_statemachine().find_state( static_cast<uint32_t>(
        l2cap_channel_state_type::wait_connect ) );
    auto wait_state = std::static_pointer_cast<l2cap_channel_wait_connect_state>( state );
    wait_state->set_original_request( a_requst );

    transition_to_state( l2cap_channel_state_type::wait_connect );

}

void l2cap_channel_close_state::handle_signaling_request( std::shared_ptr<l2cap_config_request> a_requst )
{
    /**
    * Channel is in Close state when receiving Configuration Request.
    * Reject the command with Invalid CID reason.
    */
    uint8_t buffer[4] = { 0 };
    write_le16( buffer, a_requst->m_destionation_cid );
    write_le16( buffer, a_requst->m_source_cid );
    get_statemachine().m_signaling_channel->send_reject_rsp
        (
        a_requst->m_identifier,
        l2cap_command_reject_reason::invalid_cid,
        buffer,
        sizeof( buffer )
        );
}

void l2cap_channel_close_state::handle_signaling_request( std::shared_ptr<l2cap_disconnect_request> a_requst )
{
    LogUtilInfo() << "reply disconnect request with close state.";
    get_statemachine().m_signaling_channel->send_disconnect_response
        (
        a_requst->m_identifier,
        a_requst->m_destination_cid,
        a_requst->m_source_cid
        );
}

void l2cap_channel_close_state::accept_connection_request
    (
    std::shared_ptr<l2cap_channel_event> const& a_event
    )
{
    if( !( a_event->m_channel_pkt ) )
    {
        LogUtilError() << "Not set channel request";
        return;
    }

    std::shared_ptr<connection_request> connect_;
    connect_ = std::static_pointer_cast< connection_request >( a_event->m_channel_pkt );
    get_statemachine().m_signaling_channel->send_connection_response
        (
        connect_->m_identifier, get_statemachine().m_local_channel_id,
        connect_->m_source_cid, connection_req_result::connection_success,
        connection_req_refused_status::refused_no_more_info
        );

    transition_to_state( l2cap_channel_state_type::wait_config );
}

void l2cap_channel_close_state::send_connection_request_to_remote()
{
    get_statemachine().m_signaling_channel->send_connection_request
        (
        get_statemachine().m_psm_value,
        get_statemachine().m_local_channel_id
        );

    LogUtilInfo() << "Send channel connection request for psm: " << get_statemachine().m_psm_value
        << ", local channel id: " << get_statemachine().m_local_channel_id;

    transition_to_state( l2cap_channel_state_type::wait_connect_rsp );
}

l2cap_channel_wait_connect_state::l2cap_channel_wait_connect_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_connect_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_connect_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::accept_connection_request:
        accept_connection_request( event_ );
        break;
    case l2cap_channel_event::event_type::reject_connection_request:
        reject_connection_request( event_ );
        break;
    default:
        LogUtilDebug() << "Ignore this event in close state." << static_cast< uint16_t >( event_->m_type );
        break;
    }

    return true;
}

void l2cap_channel_wait_connect_state::on_enter()
{
    auto mod = framework_manager::get_instance().get_module_manager().get_module
        ( timer_module::s_timer_module_name );
    auto timer_mod = std::static_pointer_cast< timer_module >( mod );

    if( m_waiting_authorize_timer != 0x00 )
    {
        timer_mod->undregister_timer( m_waiting_authorize_timer );
        m_waiting_authorize_timer = 0x00;
    }

    using namespace std::placeholders;
    std::function<void( uint32_t, std::string )> callback =
        std::bind( &l2cap_channel_wait_connect_state::handle_timer_expired, this, _1, _2 );
    m_waiting_authorize_timer = timer_mod->register_once_timer
        (
        callback,
        s_upper_layer_confirm_time_out
        );
}

void l2cap_channel_wait_connect_state::on_exit()
{
    auto mod = framework_manager::get_instance().get_module_manager().get_module
    ( timer_module::s_timer_module_name );
    auto timer_mod = std::static_pointer_cast< timer_module >( mod );
    if( m_waiting_authorize_timer != 0x00 )
    {
        timer_mod->undregister_timer( m_waiting_authorize_timer );
        m_waiting_authorize_timer = 0x00;
    }
}

void l2cap_channel_wait_connect_state::set_original_request( std::shared_ptr<connection_request> a_requst )
{
    m_original_request = a_requst;
}

void l2cap_channel_wait_connect_state::handle_timer_expired( uint32_t a_timer_id, std::string a_name )
{
    LogUtilError() << "Waiting upper layer to accept or reject connection request timeout. Reject this connection request.";
    get_statemachine().m_signaling_channel->send_connection_response
            ( m_original_request->m_identifier, 0x00, m_original_request->m_source_cid,
            connection_req_result::connection_refused_no_resource, connection_req_refused_status::refused_no_more_info );

    transition_to_state( l2cap_channel_state_type::close_state );
}

void l2cap_channel_wait_connect_state::accept_connection_request( std::shared_ptr<l2cap_channel_event> const& a_event )
{
    if( !( a_event->m_channel_pkt ) )
    {
        LogUtilError() << "Not set channel request";
        return;
    }

    std::shared_ptr<connection_request> connect_;
    connect_ = std::static_pointer_cast< connection_request >( a_event->m_channel_pkt );
    get_statemachine().m_signaling_channel->send_connection_response
        (
        connect_->m_identifier, get_statemachine().m_local_channel_id,
        connect_->m_source_cid, connection_req_result::connection_success,
        connection_req_refused_status::refused_no_more_info
        );

    transition_to_state( l2cap_channel_state_type::wait_config );
}

void l2cap_channel_wait_connect_state::reject_connection_request( std::shared_ptr<l2cap_channel_event> const& a_event )
{
    if( !( a_event->m_channel_pkt ) )
    {
        LogUtilError() << "Not set channel request";
        return;
    }

    connection_req_result reason = a_event->m_reason;
    if( connection_req_result::connection_success == reason ||
        connection_req_result::connection_pending == reason )
    {
        reason = connection_req_result::connection_refused_no_resource;
        LogUtilError() << "Reject connection but with accept reason.";
    }

    std::shared_ptr<connection_request> connect_;
    connect_ = std::static_pointer_cast< connection_request >( a_event->m_channel_pkt );
    get_statemachine().m_signaling_channel->send_connection_response
        (
        connect_->m_identifier, get_statemachine().m_local_channel_id,
        connect_->m_source_cid, a_event->m_reason,
        connection_req_refused_status::refused_no_more_info
        );

    transition_to_state( l2cap_channel_state_type::close_state );
}

l2cap_channel_wait_config_state::l2cap_channel_wait_config_state( l2cap_channel_statemachine& a_sm, uint32_t a_state_id )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_config_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_config_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::request_configure_local:
        if( get_statemachine().m_local_config_options.empty() )
        {
            LogUtilError() << "No channel configuration to set";
            return false;
        }
        get_statemachine().m_signaling_channel->send_config_request
            (
            get_statemachine().m_remote_channel_id,
            get_statemachine().m_local_config_options
            );
        transition_to_state( l2cap_channel_state_type::wait_config_req_rsp );
        break;
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        handle_signaling_packet( event_->m_channel_pkt );
        break;
    case l2cap_channel_event::event_type::accept_config_request:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_req:
            accept_coming_config_request( std::static_pointer_cast< l2cap_config_request >( event_->m_channel_pkt ) );
            break;
        default:
            LogUtilError() << "signaling code ignored: " << event_->m_channel_pkt->m_signaling_code;
            break;
        }
        break;
    default:
        LogUtilDebug() << "Ignore channel event type: " << static_cast< uint16_t >( event_->m_type );
        break;
    }
    return true;
}

void l2cap_channel_wait_config_state::on_enter()
{
}

void l2cap_channel_wait_config_state::on_exit()
{

}

void l2cap_channel_wait_config_state::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    // Stay in this state and wait for upper layer's reponse for this config request from remote device.

    auto& callbacks = get_statemachine().m_callbacks;
    if( !callbacks.m_coming_config_callback )
    {
        LogUtilError() << "Not set configuration callback.";
        return;
    }

    a_request->m_source_cid = get_statemachine().m_remote_channel_id;

    if( !callbacks.m_handle_module.empty() )
    {
        std::shared_ptr<executable_task> task;
        task = std::make_shared<executable_task>();
        task->set_fun( std::bind( callbacks.m_coming_config_callback, a_request ), callbacks.m_handle_module );
        task->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
    else
    {
        callbacks.m_coming_config_callback( a_request );
    }

}

void l2cap_channel_wait_config_state::handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt )
{
    if( !a_channel_pkt )
    {
        LogUtilError() << "Not set signaling packet.";
        return;
    }

    switch( a_channel_pkt->m_signaling_code )
    {
    case signaling_code::l2cap_configuration_req:
        handle_config_request( std::static_pointer_cast<l2cap_config_request>( a_channel_pkt ) );
        break;
    default:
        LogUtilDebug() << "Ignore channel signaling: " << a_channel_pkt->m_signaling_code;
        break;
    }
}

void l2cap_channel_wait_config_state::accept_coming_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    uint8_t continue_ = a_request->m_continue_flag ? 0x01 : 0x00;
    get_statemachine().m_signaling_channel->send_config_response
        (
        a_request->m_identifier,
        get_statemachine().m_remote_channel_id,
        continue_,
        channel_config_result::success,
        a_request->m_options
        );

    transition_to_state( l2cap_channel_state_type::wait_send_config );
}

l2cap_channel_wait_config_req_rsp_state::l2cap_channel_wait_config_req_rsp_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_config_req_rsp_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_config_req_rsp_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_req:
            handle_config_request( std::static_pointer_cast<l2cap_config_request>( event_->m_channel_pkt ) );
            break;
        case signaling_code::l2cap_configuration_rsp:
            handle_config_response( std::static_pointer_cast<l2cap_config_response>( event_->m_channel_pkt ) );
            break;
        case signaling_code::l2cap_disconnection_req:
            break;
        default:
            LogUtilError() << "signaling code ignored: " << event_->m_channel_pkt->m_signaling_code;
            break;
        }
        break;
    case l2cap_channel_event::event_type::accept_config_request:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_req:
            accept_coming_config_request( std::static_pointer_cast< l2cap_config_request >( event_->m_channel_pkt ) );
            break;
        default:
            LogUtilError() << "signaling code ignored: " << event_->m_channel_pkt->m_signaling_code;
            break;
        }
        break;
    default:
        LogUtilDebug() << "Ignore channel event type: " << static_cast< uint16_t >( event_->m_type );
        break;
    }
    return true;
}

void l2cap_channel_wait_config_req_rsp_state::on_enter()
{

}

void l2cap_channel_wait_config_req_rsp_state::on_exit()
{

}

void l2cap_channel_wait_config_req_rsp_state::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    auto& callbacks = get_statemachine().m_callbacks;
    if( !callbacks.m_coming_config_callback )
    {
        LogUtilError() << "Not set configuration callback.";
        return;
    }

    a_request->m_source_cid = get_statemachine().m_remote_channel_id;

    bool need_reject = false;
    if( a_request->m_continue_flag &&
        a_request->m_remote_edr_ext_flow_support &&
        get_statemachine().m_signaling_channel->get_acl_type() == acl_type::br_edr_acl &&
        l2cap_signaling::s_extended_flow_specification_edr_support != 0x00 )
    {
        /*
         * Bluetooth Core Specification Vol3 PartA:
         * The Extended Flow Specification is an L2CAP entity capability, not a per-channel negotiated attribute.
         * If both local and remote L2CAP entities support this extension,
         * the Continuation flag in all L2CAP_CONFIGURATION_REQ and L2CAP_CONFIGURATION_RSP packets
         * shall be set to 0. The configuration option fragmentation mechanism must not be used.
         * A packet with Continuation flag set to 1 is treated as invalid configuration parameter.
         * Respond with CONFIGURATION_RSP to reject this configuration negotiation.
         */
        need_reject = true;
        LogUtilError() << "Remote device should not use continue flag when support ext-flow";
    }

    if( need_reject || a_request->m_is_truncted )
    {
        LogUtilError() << "reject remote device's configuration request.";
        get_statemachine().m_signaling_channel->send_config_response
            ( a_request->m_identifier, get_statemachine().m_remote_channel_id, 0x00,
            channel_config_result::unacceptable_parameters_failed, a_request->m_options );
        return;
    }

    if( a_request->m_continue_flag )
    {
        /**
         * Continuation flag set: more configuration fragments will follow.
         * According to L2CAP spec, every CONFIGURATION_REQ must be responded.
         * Reply with Success and empty options now; full validation and negotiation
         * will be performed after receiving the final fragment (continue_flag = 0).
         */
        LogUtilInfo() << "cache continue configuration options,"
            " we will complete negotiation after receiving final fragment.";
        get_statemachine().cache_continue_config_options( a_request->m_options );
        std::vector<channel_config_option> empty_config;
        get_statemachine().m_signaling_channel->send_config_response
            (
            a_request->m_identifier,
            get_statemachine().m_remote_channel_id,
            0x00,
            channel_config_result::success,
            empty_config
            );
        return;
    }

    if( !get_statemachine().m_cached_incoming_continue_configs.empty() )
    {
        get_statemachine().cache_continue_config_options( a_request->m_options );
        a_request->m_options = std::move( get_statemachine().m_cached_incoming_continue_configs );
    }

    if( !callbacks.m_handle_module.empty() )
    {
        std::shared_ptr<executable_task> task;
        task = std::make_shared<executable_task>();
        task->set_fun( std::bind( callbacks.m_coming_config_callback, a_request ), callbacks.m_handle_module );
        task->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
    else
    {
        callbacks.m_coming_config_callback( a_request );
    }
}

void l2cap_channel_wait_config_req_rsp_state::handle_config_response( std::shared_ptr<l2cap_config_response> const& a_response )
{
    auto& callbacks = get_statemachine().m_callbacks;

    switch( a_response->m_result )
    {
    case channel_config_result::success:
        if( callbacks.m_coming_config_rsp_callback )
        {
            if( !callbacks.m_handle_module.empty() )
            {
                std::shared_ptr<executable_task> task;
                task = std::make_shared<executable_task>();
                task->set_fun( std::bind( callbacks.m_coming_config_rsp_callback, a_response ), callbacks.m_handle_module );
                task->set_source_module( l2cap_module::s_l2cap_module_name );
                framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            }
            else
            {
                callbacks.m_coming_config_rsp_callback( a_response );
            }
        }
        transition_to_state( l2cap_channel_state_type::wait_config_req );
        break;
    case channel_config_result::pending:
        return;
    default:
        LogUtilInfo() << "config result ignored: " << a_response->m_result;
        break;
    }
}

void l2cap_channel_wait_config_req_rsp_state::accept_coming_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    uint8_t continue_ = a_request->m_continue_flag ? 0x01 : 0x00;
    get_statemachine().m_signaling_channel->send_config_response
        (
        a_request->m_identifier,
        get_statemachine().m_remote_channel_id,
        continue_,
        channel_config_result::success,
        a_request->m_options
        );

    transition_to_state( l2cap_channel_state_type::wait_config_rsp );
}

l2cap_channel_wait_config_req_state::l2cap_channel_wait_config_req_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_config_req_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_config_req_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_req:
            LogUtilInfo() << "Handle confguration reqeust from remote device";
            handle_config_request( std::static_pointer_cast<l2cap_config_request>( event_->m_channel_pkt ) );
            break;
        case signaling_code::l2cap_disconnection_req:
            break;
        default:
            LogUtilError() << "signaling code ignored: " << event_->m_channel_pkt->m_signaling_code;
            break;
        }
        break;
    case l2cap_channel_event::event_type::accept_config_request:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_req:
            accept_coming_config_request( std::static_pointer_cast< l2cap_config_request >( event_->m_channel_pkt ) );
            break;
        default:
            LogUtilError() << "signaling code ignored: " << event_->m_channel_pkt->m_signaling_code;
            break;
        }
        break;
    default:
        LogUtilError() << "Ignore type: " << static_cast<uint32_t>( event_->m_type );
    }

    return true;
}

void l2cap_channel_wait_config_req_state::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    auto& callbacks = get_statemachine().m_callbacks;
    if( !callbacks.m_coming_config_callback )
    {
        LogUtilError() << "Not set configuration callback.";
        return;
    }
    a_request->m_source_cid = get_statemachine().m_remote_channel_id;

    if( !callbacks.m_handle_module.empty() )
    {
        std::shared_ptr<executable_task> task;
        task = std::make_shared<executable_task>();
        task->set_fun( std::bind( callbacks.m_coming_config_callback, a_request ), callbacks.m_handle_module );
        task->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
    else
    {
        callbacks.m_coming_config_callback( a_request );
    }
}

void l2cap_channel_wait_config_req_state::accept_coming_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    uint8_t continue_ = a_request->m_continue_flag ? 0x01 : 0x00;
    get_statemachine().m_signaling_channel->send_config_response
        (
        a_request->m_identifier,
        get_statemachine().m_remote_channel_id,
        continue_,
        channel_config_result::success,
        a_request->m_options
        );

    transition_to_state( l2cap_channel_state_type::open );
}

void l2cap_channel_wait_config_req_state::on_enter()
{

}

void l2cap_channel_wait_config_req_state::on_exit()
{

}

l2cap_channel_wait_config_rsp_state::l2cap_channel_wait_config_rsp_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_config_rsp_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_config_rsp_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        if( !( event_->m_channel_pkt ) )
        {
            LogUtilError() << "Not set channel request";
            return false;
        }

        switch( event_->m_channel_pkt->m_signaling_code )
        {
        case signaling_code::l2cap_configuration_rsp:
            handle_config_response( std::static_pointer_cast<l2cap_config_response>( event_->m_channel_pkt ) );
            break;
        case signaling_code::l2cap_disconnection_req:
            break;
        default:
            LogUtilError() << "signaling code ignored: " << event_->m_channel_pkt->m_signaling_code;
            break;
        }
        break;
    default:
        LogUtilError() << "event type has been ignored, type: " << static_cast< uint16_t >( event_->m_type );
    }
    return true;
}

void l2cap_channel_wait_config_rsp_state::on_enter()
{
    return;
}

void l2cap_channel_wait_config_rsp_state::on_exit()
{

}

void l2cap_channel_wait_config_rsp_state::handle_config_response( std::shared_ptr<l2cap_config_response> const& a_response )
{
    if( a_response->m_result == channel_config_result::success )
    {
        transition_to_state( l2cap_channel_state_type::open );
    }
    else
    {
        LogUtilDebug() << "Remote device rejected local channel config options.";
    }

    auto& callbacks = get_statemachine().m_callbacks;
    if( !callbacks.m_coming_config_rsp_callback )
    {
        return;
    }

    if( !callbacks.m_handle_module.empty() )
    {
        std::shared_ptr<executable_task> task;
        task = std::make_shared<executable_task>();
        task->set_fun( std::bind( callbacks.m_coming_config_rsp_callback, a_response ), callbacks.m_handle_module );
        task->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
    }
    else
    {
        callbacks.m_coming_config_rsp_callback( a_response );
    }
}

l2cap_channel_open_state::l2cap_channel_open_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_open_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_open_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::channel_sdu_pkt_from_controller:
        if( !( event_->m_channel_data ) )
        {
            LogUtilError() << "Not set channel sdu data";
            return false;
        }

        {
            auto& callbacks = get_statemachine().m_callbacks;
            if( !callbacks.m_handle_module.empty() )
            {
                std::shared_ptr<executable_task> task;
                task = std::make_shared<executable_task>();
                task->set_fun( std::bind( callbacks.m_channel_sdu_callback,
                                          event_->m_channel_data ),
                               callbacks.m_handle_module
                             );
                task->set_source_module( l2cap_module::s_l2cap_module_name );
                framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            }
            else
            {
                callbacks.m_channel_sdu_callback( event_->m_channel_data );
            }
        }
        break;
    case l2cap_channel_event::event_type::channel_sdu_pkt_from_upper:
        get_statemachine().send_completed_acl_packet( event_->m_channel_data );
        break;
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        handle_signaling_packet( event_->m_channel_pkt );
        break;
    case l2cap_channel_event::event_type::close_channel_request:
        get_statemachine().send_disconnect_request_to_remote();
        transition_to_state( l2cap_channel_state_type::wait_disconnect );
        break;
    default:
        LogUtilError() << "event type has been ignored, type: " << static_cast< uint16_t >( event_->m_type );
    }

    return true;
}

void l2cap_channel_open_state::on_enter()
{

}

void l2cap_channel_open_state::on_exit()
{

}

void l2cap_channel_open_state::handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt )
{
    switch( a_channel_pkt->m_signaling_code )
    {
    case signaling_code::l2cap_disconnection_req:
        {
            std::shared_ptr<l2cap_disconnect_request> detail_request;
            detail_request = std::static_pointer_cast< l2cap_disconnect_request >( a_channel_pkt );
            get_statemachine().m_signaling_channel->send_disconnect_response( detail_request->m_identifier,
                detail_request->m_destination_cid, detail_request->m_source_cid );
        }
        transition_to_state( l2cap_channel_state_type::close_state );
        break;
    case signaling_code::l2cap_connection_rsp:
    case signaling_code::l2cap_disconnection_rsp:
        /* as core specific, we can ignore these two signaling here */
        break;
    case signaling_code::l2cap_configuration_req:
        /* TODO we need handle this configuration request here */
        break;
    default:
        LogUtilError() << "Received signaling message on open state, ignored signaling: " << a_channel_pkt->m_signaling_code;
        break;
    }
}

l2cap_channel_wait_connect_rsp_state::l2cap_channel_wait_connect_rsp_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_connect_rsp_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_connect_rsp_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        handle_signaling_packet( event_->m_channel_pkt );
        break;
    default:
        LogUtilError() << "Not handle event type: " << static_cast< uint32_t >( event_->m_type );
    }

    return true;
}

void l2cap_channel_wait_connect_rsp_state::on_enter()
{

}

void l2cap_channel_wait_connect_rsp_state::on_exit()
{

}

void l2cap_channel_wait_connect_rsp_state::handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt )
{
    if( !a_channel_pkt )
    {
        LogUtilError() << "Not set signaling packet to handle!";
        return;
    }

    switch( a_channel_pkt->m_signaling_code )
    {
    case signaling_code::l2cap_connection_rsp:
        {
            std::shared_ptr<l2cap_connect_response> detail_request;
            detail_request = std::static_pointer_cast<l2cap_connect_response>( a_channel_pkt );
            switch( detail_request->result )
            {
            case connection_req_result::connection_pending:
                // Keep in current state and wait for the next response
                LogUtilInfo() << "Remote device pending our connection request.";
                return;
            case connection_req_result::connection_success:
                get_statemachine().m_remote_channel_id = detail_request->m_destionation_cid;
                transition_to_state( l2cap_channel_state_type::wait_config );
                break;
            default:
                LogUtilInfo() << "Remote device reject connection request.";
                transition_to_state( l2cap_channel_state_type::close_state );
                break;
            }
        }
        break;
    default:
        LogUtilError() << "Received signaling message on open state, ignored signaling: " << a_channel_pkt->m_signaling_code;
        break;
    }
}

l2cap_channel_wait_send_config_state::l2cap_channel_wait_send_config_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_send_config_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_send_config_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast< l2cap_channel_event >( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    case l2cap_channel_event::event_type::handle_signaling_pkt:
        handle_signaling_packet( event_->m_channel_pkt );
    break;
    default:
        LogUtilError() << "Not handle event type: " << static_cast< uint32_t >( event_->m_type );
    }

    return true;
}

void l2cap_channel_wait_send_config_state::on_enter()
{

}

void l2cap_channel_wait_send_config_state::on_exit()
{

}

void l2cap_channel_wait_send_config_state::handle_signaling_packet
    (
    std::shared_ptr<signaling_channel_packet> const& a_channel_pkt
    )
{
    if( !a_channel_pkt )
    {
        LogUtilError() << "Not set signaling packet to handle!";
        return;
    }

    switch( a_channel_pkt->m_signaling_code )
    {
    case signaling_code::l2cap_disconnection_rsp:
        {
            std::shared_ptr<l2cap_disconnect_response> detail_request =
                std::static_pointer_cast<l2cap_disconnect_response>( a_channel_pkt );
            if( detail_request->m_destination_cid == get_statemachine().m_local_channel_id )
            {
                transition_to_state( l2cap_channel_state_type::close_state );
            }
            else
            {
                LogUtilError() << "Received disconnection response for unknown channel id: "
                    << detail_request->m_destination_cid;
            }
        }
    break;
    default:
        LogUtilError() << "Received signaling message on open state, ignored signaling: "
            << a_channel_pkt->m_signaling_code;
        break;
    }
}

l2cap_channel_wait_disconnect_state::l2cap_channel_wait_disconnect_state
    (
    l2cap_channel_statemachine& a_sm,
    uint32_t a_state_id
    )
    : l2cap_channel_base_state( a_sm, a_state_id )
{

}

bool l2cap_channel_wait_disconnect_state::handle_event( uint32_t event, void* p_data )
{
    return true;
}

bool l2cap_channel_wait_disconnect_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::static_pointer_cast<l2cap_channel_event>( a_event );
    if( !event_ )
    {
        LogUtilError() << "Not state event. ignore this event";
        return false;
    }

    switch( event_->m_type )
    {
    default:
        LogUtilError() << "Not handle event type: " << static_cast<uint32_t>( event_->m_type );
    }

    return true;
}

void l2cap_channel_wait_disconnect_state::on_enter()
{

}

void l2cap_channel_wait_disconnect_state::on_exit()
{

}

void l2cap_channel_wait_disconnect_state::handle_signaling_packet( std::shared_ptr<signaling_channel_packet> const& a_channel_pkt )
{

}

l2cap_channel_statemachine::l2cap_channel_statemachine()
{
    std::shared_ptr<state_machine::abstract_state> state_;
    state_ = std::make_shared<l2cap_channel_close_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::close_state ) );
    add_state( state_ );
    set_initial_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_connect_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_connect ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_config_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_config ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_config_req_rsp_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_config_req_rsp ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_config_rsp_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_config_rsp ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_config_rsp_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_config_rsp ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_open_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::open ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_config_req_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_config_req ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_connect_rsp_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_connect_rsp ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_send_config_state>( *this,
        static_cast< uint32_t >( l2cap_channel_state_type::wait_send_config ) );
    add_state( state_ );

    state_ = std::make_shared<l2cap_channel_wait_disconnect_state>( *this,
        static_cast<uint32_t>( l2cap_channel_state_type::wait_disconnect ) );
    add_state( state_ );
}

void l2cap_channel_statemachine::clear()
{
    m_connection_handle = 0x00;
    m_local_channel_id = 0x00;
    m_remote_channel_id = 0x00;
    m_psm_value = 0x00;
    m_local_inited = false;
}

void l2cap_channel_statemachine::accept_connection_req
    (
    uint16_t a_local_cid,
    std::shared_ptr<connection_request> const& a_request
    )
{
    m_local_channel_id = a_local_cid;

    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_channel_pkt = a_request;
    event_->m_type = l2cap_channel_event::event_type::accept_connection_request;

    handle_event( event_ );
}

void l2cap_channel_statemachine::reject_connection_req
    (
    std::shared_ptr<connection_request> const& a_request,
    connection_req_result a_reason
    )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_channel_pkt = a_request;
    event_->m_type = l2cap_channel_event::event_type::reject_connection_request;
    event_->m_reason = a_reason;

    handle_event( event_ );
}

void l2cap_channel_statemachine::disconnect_channel_req()
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_type = l2cap_channel_event::event_type::close_channel_request;

    handle_event( event_ );
}

void l2cap_channel_statemachine::accept_config_req( std::shared_ptr<l2cap_config_request> const& a_request )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_channel_pkt = a_request;
    event_->m_type = l2cap_channel_event::event_type::accept_config_request;

    handle_event( event_ );
}

void l2cap_channel_statemachine::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_channel_pkt = a_request;
    event_->m_type = l2cap_channel_event::event_type::handle_signaling_pkt;

    handle_event( event_ );
}

void l2cap_channel_statemachine::handle_config_response( std::shared_ptr<l2cap_config_response> const& a_response )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_channel_pkt = a_response;
    event_->m_type = l2cap_channel_event::event_type::handle_signaling_pkt;

    handle_event( event_ );
}

void l2cap_channel_statemachine::handle_disconnect_request( std::shared_ptr<l2cap_disconnect_request> const& a_request )
{
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_channel_pkt = a_request;
    event_->m_type = l2cap_channel_event::event_type::handle_signaling_pkt;

    handle_event( event_ );

    l2cap_channel_state_type state = static_cast<l2cap_channel_state_type>( get_id() );
    if( state != l2cap_channel_state_type::close_state )
    {
        LogUtilDebug() << "After receive disconnect request, need transfer to close state.";
        auto state = std::static_pointer_cast<l2cap_channel_base_state>( find_state( get_id() ) );
        if( state )
        {
            state->transition_to_state( l2cap_channel_state_type::close_state );
        }
        else
        {
            LogUtilError() << "cast to l2cap_channel_base_state failed";
            transition_to( static_cast<uint32_t>( l2cap_channel_state_type::close_state ) );
        }
        m_signaling_channel->send_disconnect_response( a_request->m_identifier,
            a_request->m_destination_cid, a_request->m_source_cid );
    }
}

void l2cap_channel_statemachine::config_local_channel_req( std::shared_ptr<l2cap_config_local_channel_request> const& a_request )
{
    m_local_config_options = std::move( a_request->m_options );
    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_type = l2cap_channel_event::event_type::request_configure_local;

    handle_event( event_ );
}

void l2cap_channel_statemachine::send_upper_sdu( std::shared_ptr<hci_data> const& a_hci_data )
{
    a_hci_data->m_type = uart_hci_type::acl_type;
    a_hci_data->m_from_controller = false;

    std::shared_ptr<l2cap_channel_event> event_;
    event_ = std::make_shared<l2cap_channel_event>();
    event_->m_type = l2cap_channel_event::event_type::channel_sdu_pkt_from_upper;
    event_->m_channel_data = a_hci_data;

    l2cap_header header;
    header.set_acl_handle( m_connection_handle );
    header.set_channel_id( m_remote_channel_id );
    header.set_sdu_length( static_cast<uint16_t>( a_hci_data->m_buffer.size() ) - header.header_size() );
    header.to_raw_buffer( a_hci_data->m_buffer.data(), header.header_size() );

    handle_event( event_ );
}

void l2cap_channel_statemachine::send_completed_acl_packet( std::shared_ptr<hci_data> const& a_hci_data )
{
    std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address> hci_task;
    hci_task = std::make_shared<l2cap_task_send_l2cap_sdu_with_remote_address>();
    hci_task->m_acl_handle = m_connection_handle;
    hci_task->m_local_cid = m_local_channel_id;
    hci_task->m_remote_address = m_signaling_channel->get_remote_address();
    hci_task->m_hci_packet = a_hci_data;

    hci_task->set_target_module( l2cap_module::s_l2cap_module_name );
    hci_task->set_source_module( l2cap_module::s_l2cap_module_name );
    framework::framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

void l2cap_channel_statemachine::send_disconnect_request_to_remote()
{
    m_signaling_channel->send_disconnect_request( m_remote_channel_id, m_local_channel_id );
}

void l2cap_channel_statemachine::cache_continue_config_options( std::vector<channel_config_option> const& a_options )
{
    for( const auto& new_opt : a_options )
    {
        // Search existing cached option with same type
        auto found = std::find_if( m_cached_incoming_continue_configs.begin(), m_cached_incoming_continue_configs.end(),
            [&new_opt]( const channel_config_option& cached )
            {
                return cached.m_type == new_opt.m_type;
            } );

        if( found != m_cached_incoming_continue_configs.end() )
        {
            // Same option type exists, overwrite with latest value
            *found = new_opt;
        }
        else
        {
            // New option type, append to cache
            m_cached_incoming_continue_configs.push_back( new_opt );
        }
    }
}

}

