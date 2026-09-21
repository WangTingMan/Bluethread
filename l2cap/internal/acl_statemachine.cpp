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

#include "acl_statemachine.h"
#include "acl_manager.h"
#include "../l2cap_common.h"
#include "common/acl_connections_db.h"
#include "endian_convert.h"

#include "framework/log_util.h"
#include "framework/framework_manager.h"

using namespace framework;

namespace bluetooth
{

    acl_base_state::acl_base_state
        (
        acl_statemachine& a_sm,
        uint32_t a_state_id
        )
        : abstract_state( a_sm, a_state_id )
        , m_acl_statemachine( a_sm )
    {

    }

    void acl_base_state::transition_to_state( connection_status a_state )
    {
        transition_to( static_cast<uint32_t>( a_state ) );
        return;
    }

    bool acl_base_state::handle_event( uint32_t event, void* p_data )
    {
        return true;
    }

    bool acl_base_state::handle_event( std::shared_ptr<state_machine::abstract_event> const& a_event )
    {
        return true;
    }

    bool acl_base_state::handle_event( uint32_t a_event_type, std::vector<void*> a_params )
    {
        return true;
    }

    void acl_base_state::on_enter()
    {

    }

    void acl_base_state::on_exit()
    {

    }

    acl_disconnected_state::acl_disconnected_state
        (
        acl_statemachine& a_sm,
        uint32_t a_state_id
        )
        : acl_base_state(a_sm, a_state_id)
    {

    }

    bool acl_disconnected_state::handle_event( uint32_t a_event_type, std::vector<void*> a_params )
    {
        acl_base_state::acl_event_type event_ = static_cast<acl_base_state::acl_event_type>( a_event_type );
        switch( event_ )
        {
        case acl_base_state::acl_event_type::acl_connection_completed:
            handle_connection_completed( a_params );
            break;
        }
        return true;
    }

    bool acl_disconnected_state::handle_connection_completed( std::vector<void*> a_params )
    {
        if( a_params.size() != 4 )
        {
            LogUtilInfo() << "should have 4 parameters here!";
            return false;
        }

        bluetooth_address* remote_device = reinterpret_cast<bluetooth_address*>( a_params[0] );
        uint16_t* handle = reinterpret_cast<uint16_t*>( a_params[1] );
        acl_type* type = reinterpret_cast<acl_type*>( a_params[2] );
        bool* encypted = reinterpret_cast<bool*>( a_params[3] );

        m_acl_statemachine.set_encrypted( *encypted );
        m_acl_statemachine.set_handle( *handle );

        transition_to_state( connection_status::connected );
        return true;
    }

    acl_connecting_state::acl_connecting_state
        (
        acl_statemachine& a_sm,
        uint32_t a_state_id
        )
        : acl_base_state( a_sm, a_state_id )
    {

    }

    acl_connected_state::acl_connected_state
        (
        acl_statemachine& a_sm,
        uint32_t a_state_id
        )
        : acl_base_state( a_sm, a_state_id )
    {

    }

    void acl_connected_state::on_enter()
    {
        m_acl_statemachine.connection_state_endtered();
    }

    bool acl_connected_state::handle_event(uint32_t a_event_type, std::vector<void*> a_params)
    {
        acl_base_state::acl_event_type event_ = static_cast<acl_base_state::acl_event_type>(a_event_type);
        switch (event_)
        {
        case acl_base_state::acl_event_type::acl_disconnected_with_acl_handle:
            {
                uint16_t* handle = reinterpret_cast<uint16_t*>(a_params[0]);
                LogUtilInfo() << "hanlde: " << std::format("0x{:0>2X}", *handle) << " disconnected";
                m_acl_statemachine.handle_disconnected(*handle);
                transition_to_state(connection_status::disconnected);
            }
            break;
        default:
            break;
        }
        return true;
    }

    acl_disconnecting_state::acl_disconnecting_state
        (
        acl_statemachine& a_sm,
        uint32_t a_state_id
        )
        : acl_base_state( a_sm, a_state_id )
    {

    }

    acl_statemachine::acl_statemachine()
        : m_encrypted( false )
    {
        LogUtilInfo() << "create a acl statemachine.";

        std::shared_ptr<state_machine::abstract_state> state_;
        state_ = std::make_shared<acl_disconnected_state>( *this,
            static_cast<uint32_t>( connection_status::disconnected ) );
        add_state( state_ );
        set_initial_state( state_ );

        state_ = std::make_shared<acl_connecting_state>( *this,
            static_cast<uint32_t>( connection_status::connecting ) );
        add_state( state_ );

        state_ = std::make_shared<acl_connected_state>( *this,
            static_cast<uint32_t>( connection_status::connected ) );
        add_state( state_ );

        state_ = std::make_shared<acl_disconnecting_state>( *this,
            static_cast<uint32_t>( connection_status::disconnecting ) );
        add_state( state_ );

        m_signaling_channel = std::make_shared<l2cap_signaling>();
    }

    void acl_statemachine::handle_request_channel_connection(uint16_t a_psm)
    {
        LogUtilInfo() << "try to create a l2cap channel for PSM: " << a_psm;

        l2cap_callbacks cbs;
        bool callback_found = m_psm_callback_query(a_psm, cbs);
        if (!callback_found)
        {
            LogUtilInfo() << "No upper layer registered callback for psm: " << a_psm;
            return;
        }

        std::shared_ptr<l2cap_channel_statemachine> channel;
        channel = std::make_shared<l2cap_channel_statemachine>();
        channel->start();
        channel->set_signaling_channel(m_signaling_channel);
        channel->set_upper_callbacks(cbs);
        channel->set_acl_handle( m_signaling_channel->get_acl_handle() );
        channel->set_psm(a_psm);
        channel->set_local_cid(m_next_local_cid++);
        m_channel_machines.push_back(channel);

        std::shared_ptr<l2cap_channel_event> event_ = std::make_shared<l2cap_channel_event>();
        event_->m_type = l2cap_channel_event::event_type::open_channel_request;
        channel->handle_event(event_);
    }

    void acl_statemachine::handle_pending_outgoing_connection_request
        (
        std::vector<channel_connection_request> a_pending_connection_request
        )
    {
        if( a_pending_connection_request.empty() )
        {
            return;
        }

        m_pending_outgoing_connection_requests.insert( m_pending_outgoing_connection_requests.end(),
            a_pending_connection_request.begin(), a_pending_connection_request.end());
    }

    void acl_statemachine::handle_coming_acl_packet(std::shared_ptr<hci_data> const& a_hci_data)
    {
        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        std::vector<uint8_t> const& buffer = a_hci_data->m_buffer;
        uint16_t channel_id = le_to_host16(buffer.data() + 6);
        uint8_t handle[2];
        handle[0] = buffer[0];
        handle[1] = buffer[1] & 0x0F;
        uint16_t acl_handle = le_to_host16(handle);
        std::shared_ptr<l2cap_channel_statemachine> dynamic_channel;

        acl_type type = m_signaling_channel->get_acl_type();
        switch( type )
        {
        case acl_type::br_edr_acl:
            switch( channel_id )
            {
            case l2cap_signaling::s_l2cap_edr_signaling_channel:
                m_signaling_channel->handle_incoming_signaling( a_hci_data );
                return;
            }
            break;
        case acl_type::le_acl:
            switch( channel_id )
            {
            case l2cap_signaling::s_l2cap_le_signaling_channel:
                m_signaling_channel->handle_incoming_signaling( a_hci_data );
                return;
            }
            break;
        default:
            LogUtilError( "unknown link type, drop this packet" );
            return;
        }

        /**
        * @note According to Bluetooth Core Specification,
        * the minimum Channel ID for L2CAP dynamic channels is 0x0040.
        */
        if( channel_id >= 0x0040 )
        {
            dynamic_channel = find_channel_state_machine( acl_handle, channel_id );
        }
        else
        {
            LogUtilError() << "unknown channel id: " << std::format( "0x{:0>2X}", channel_id )
                << ", drop this packet";
            return;
        }

        if( dynamic_channel )
        {
            std::shared_ptr<l2cap_channel_event> event_;
            event_ = std::make_shared<l2cap_channel_event>();
            event_->m_type = l2cap_channel_event::event_type::channel_sdu_pkt_from_controller;
            event_->m_channel_data = a_hci_data;
            dynamic_channel->handle_event( event_ );
        }
        else
        {
            LogUtilError() << "No channel statemachine for handle: " << std::format("0x{:0>2X}", acl_handle)
                << ", channel id: " << std::format( "0x{:0>2X}", channel_id );
        }
    }

    void acl_statemachine::accept_connection_req(std::shared_ptr<connection_request> const& a_request)
    {

        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        bool found_sm = false;
        for (auto& channel : m_channel_machines)
        {
            if (channel->get_psm() == a_request->m_psm_value &&
                channel->get_remote_cid() == a_request->m_source_cid)
            {
                ++m_next_local_cid;
                channel->accept_connection_req(m_next_local_cid, a_request);
                found_sm = true;
                break;
            }
        }

        if (!found_sm)
        {
            LogUtilError() << "No channel state machine for handle: " << a_request->m_acl_handle
                << ", remote cid: " << a_request->m_source_cid << ", psm: " << a_request->m_psm_value
                << ", cannot accept this connection request, ignore it.";
            return;
        }
    }

    void acl_statemachine::reject_connection_req
        (
        std::shared_ptr<connection_request> const& a_request,
        connection_req_result a_reason
        )
    {

        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        bool found_sm = false;
        for (auto& channel : m_channel_machines)
        {
            if (channel->get_psm() == a_request->m_psm_value &&
                channel->get_remote_cid() == a_request->m_source_cid)
            {
                channel->reject_connection_req(a_request, a_reason);
                found_sm = true;
                break;
            }
        }

        if (!found_sm)
        {
            LogUtilError() << "No channel state machine for handle: " << a_request->m_acl_handle
                << ", remote cid: " << a_request->m_source_cid;
            return;
        }
    }

    void acl_statemachine::disconnect_channel_req( uint16_t a_local_cid )
    {
        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>( current_state );
        if( con_status != connection_status::connected )
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        for( auto& channel : m_channel_machines )
        {
            if( channel->get_local_cid() == a_local_cid )
            {
                channel->disconnect_channel_req();
                return;
            }
        }
    }

    void acl_statemachine::accept_config_req(std::shared_ptr<l2cap_config_request> const& a_request)
    {

        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        auto channel = find_channel_state_machine(a_request->m_acl_handle, a_request->m_destionation_cid );
        if (!channel)
        {
            LogUtilError() << "No channel state machine for handle: " << a_request->m_acl_handle
                << ", local cid: " << a_request->m_destionation_cid;
            return;
        }

        channel->accept_config_req(a_request);
    }

    void acl_statemachine::config_local_channel_req
        (
        std::shared_ptr<l2cap_config_local_channel_request> const& a_request
        )
    {

        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        std::shared_ptr<l2cap_channel_statemachine> channel;
        uint16_t result = 0x00;
        channel = find_channel_state_machine_by_remote_cid(a_request->m_acl_handle, a_request->m_remote_cid);
        if (!channel)
        {
            LogUtilError() << "Cannot find channel state machine by remote cid: " << a_request->m_remote_cid;
            return;
        }

        channel->config_local_channel_req(a_request);
    }

    void acl_statemachine::send_upper_sdu
        (
        std::shared_ptr<l2cap_task_send_l2cap_sdu> const& a_tsk
        )
    {

        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        auto channel = find_channel_state_machine(a_tsk->m_local_cid);
        if (channel)
        {
            channel->send_upper_sdu(a_tsk->m_hci_packet);
        }
        else
        {
            LogUtilDebug() << "No channel statemachine for local channel: " << a_tsk->m_local_cid;
        }
    }

    void acl_statemachine::close_channel_with_invalid_cid
        (
        std::shared_ptr<l2cap_task_remote_invalid_cid_channel_close> const& a_tsk
        )
    {
        std::shared_ptr<l2cap_channel_statemachine> channel;
        if( a_tsk->m_local_cid != 0x00 &&
            a_tsk->m_remote_cid != 0x00 )
        {
            channel = find_channel_state_machine_by_cids( a_tsk->m_local_cid, a_tsk->m_remote_cid );
        }
        else if( a_tsk->m_local_cid == 0x00 &&
                 a_tsk->m_remote_cid != 0x00 )
        {
            channel = find_channel_state_machine_by_remote_cid( a_tsk->m_acl_handle, a_tsk->m_remote_cid );
        }
        else if( a_tsk->m_local_cid != 0x00 &&
            a_tsk->m_remote_cid == 0x00 )
        {
            channel = find_channel_state_machine( a_tsk->m_local_cid );
        }

        if( channel )
        {
            LogUtilInfo() << "Force transition channel to close_state due to remote INVALID_CID, local cid: "
                << a_tsk->m_local_cid << ", remote cid: " << a_tsk->m_remote_cid;
            channel->transition_to( (uint32_t)l2cap_channel_state_type::close_state );

            for( auto it = m_channel_machines.begin(); it != m_channel_machines.end(); ++it )
            {
                if( *it == channel )
                {
                    /**
                     * We always accept the disconnect request, so we can remove the channel state machine now.
                     */
                    m_channel_machines.erase( it );
                    return;
                }
            }
        }
    }

    void acl_statemachine::remove_channel_from_cache( uint16_t a_local_cid )
    {
        for( auto it = m_channel_machines.begin(); it != m_channel_machines.end(); ++it )
        {
            if( (*it)->get_local_cid() == a_local_cid )
            {
                LogUtilInfo() << "Remove channel state machine for local cid: " << a_local_cid;
                m_channel_machines.erase( it );
                return;
            }
        }
    }

    void acl_statemachine::connection_state_endtered()
    {
        m_signaling_channel->set_sig_pkt_handler( std::bind( &acl_statemachine::handle_signaling_pkt,
            this, std::placeholders::_1 ) );
        m_signaling_channel->register_info_requested( std::bind( &acl_statemachine::handle_channel_infor_requested,
            this, std::placeholders::_1, std::placeholders::_2 ) );
        m_signaling_channel->start_query_info();
    }

    void acl_statemachine::handle_signaling_pkt(std::shared_ptr<signaling_channel_packet> const& a_request)
    {
        int current_state = get_current_state_id();
        connection_status con_status = static_cast<connection_status>(current_state);
        if (con_status != connection_status::connected)
        {
            LogUtilError() << "the acl not connected so cannot handle this signaling packet, device: "
                << m_signaling_channel->get_remote_address();
            return;
        }

        switch (a_request->m_signaling_code)
        {
        case signaling_code::l2cap_connection_req:
            handle_connection_request(std::static_pointer_cast<connection_request>(a_request));
            break;
        case signaling_code::l2cap_configuration_req:
            handle_config_request(std::static_pointer_cast<l2cap_config_request>(a_request));
            break;
        case signaling_code::l2cap_configuration_rsp:
            handle_config_response(std::static_pointer_cast<l2cap_config_response>(a_request));
            break;
        case signaling_code::l2cap_disconnection_req:
            handle_disconnect_request(std::static_pointer_cast<l2cap_disconnect_request>(a_request));
            break;
        case signaling_code::l2cap_connection_rsp:
            handle_connect_response(std::static_pointer_cast<l2cap_connect_response>(a_request));
            break;
        default:
            LogUtilError() << "Not handle request code: " << a_request->m_signaling_code;
            break;
        }
    }

    void acl_statemachine::handle_disconnected(uint16_t a_handle)
    {
        // todo: notify all channel that disconnected
        for (auto& ele : m_channel_machines)
        {
            ele;
        }
    }

    void acl_statemachine::handle_channel_infor_requested
        (
        uint16_t                        a_acl_handle,
        l2cap_channel_information_type  a_info_type
        )
    {
        if (!m_signaling_channel)
        {
            LogUtilError() << "No signaling entity for handle: " << a_acl_handle;
            return;
        }

        if ( m_pending_outgoing_connection_requests.empty())
        {
            LogUtilInfo() << "not handle information request since m_pending_outgoing_connection_requests is empty.";
            return;
        }

        // TODO find out a proper chance to connected next pending request.
        // We need send the connect request after all the information requested from remote device.

        channel_connection_request conn_request = m_pending_outgoing_connection_requests.front();
        m_pending_outgoing_connection_requests.erase(m_pending_outgoing_connection_requests.begin());
        handle_request_channel_connection(conn_request.m_psm);
    }

    void acl_statemachine::handle_connection_request(std::shared_ptr<connection_request> const& a_request)
    {
        std::shared_ptr<l2cap_channel_statemachine> channel;
        connection_req_result result = connection_req_result::connection_success;
        for( auto& ele : m_channel_machines )
        {
            if( a_request->m_acl_handle == ele->get_acl_handle() &&
                a_request->m_psm_value == ele->get_psm() )
            {
                // the coming connection we already connected with the psm.
                LogUtilWarning() << "Already connected to PSM: " << static_cast<uint16_t>( a_request->m_psm_value )
                    << ". Let the upper layer decide whether accept.";
            }

            if( ele->get_remote_cid() == a_request->m_source_cid )
            {
                LogUtilError() << "Source channel id: " << static_cast<uint16_t>( a_request->m_source_cid )
                    << " already used.";
                result = connection_req_result::connection_refused_source_id_already_used;
                break;
            }
        }

        if( !m_signaling_channel )
        {
            LogUtilError() << "No signaling channel for handle: " << a_request->m_acl_handle;
            return;
        }

        if( connection_req_result::connection_success == result &&
            a_request->m_source_cid < 0x0040 )
        {
            result = connection_req_result::connection_refused_source_id_invalid;
        }

        if( connection_req_result::connection_success == result &&
            get_acl_type() == acl_type::le_acl )
        {
            /*
            * @note Reference: Bluetooth Core Specification, L2CAP Chapter 4, Table 4.2
            * LE signaling channel does not support L2CAP_CONNECTION_REQ (0x04).
            * Shall respond with L2CAP_CMD_REJECT with reason Unknown Command.
            */
            LogUtilWarning() << "LE ACL link received L2CAP CONNECTION_REQ, reply unknown command reject";
            m_signaling_channel->send_reject_rsp( a_request->m_identifier,
                l2cap_command_reject_reason::unknown_command, nullptr, 0 );
            return;
        }

        if( connection_req_result::connection_success == result )
        {
            if( !( is_br_edr_psm_valid( a_request->m_psm_value ) ) )
            {
                LogUtilError() << "Invalid BR/EDR PSM 0x" << std::hex << a_request->m_psm_value;
                result = connection_req_result::connection_refused_not_support;
            }
        }

        if( connection_req_result::connection_success != result )
        {
            m_signaling_channel->send_connection_response( a_request->m_identifier, 0x00, a_request->m_source_cid,
                result, connection_req_refused_status::refused_no_more_info );
            return;
        }

        if( !m_psm_callback_query )
        {
            LogUtilError() << "not set callback seeker";
            return;
        }

        l2cap_callbacks cbs;
        bool seeker_found = m_psm_callback_query( a_request->m_psm_value, cbs );
        if( !seeker_found )
        {
            LogUtilInfo() << "No upper layer registered callback for psm: " << a_request->m_psm_value;
            m_signaling_channel->send_connection_response( a_request->m_identifier, 0x00, a_request->m_source_cid,
                connection_req_result::connection_refused_not_support, connection_req_refused_status::refused_no_more_info );
            return;
        }

        channel = std::make_shared<l2cap_channel_statemachine>();
        m_channel_machines.push_back( channel );
        LogUtilInfo() << "Create new l2cap channel, PSM:" << a_request->m_psm_value
            << ", remote_cid:" << a_request->m_source_cid;
        channel->start();
        channel->set_signaling_channel( m_signaling_channel );
        channel->set_upper_callbacks( cbs );
        channel->set_acl_handle( a_request->m_acl_handle );
        channel->set_psm( a_request->m_psm_value );
        channel->set_remote_cid( a_request->m_source_cid );

        std::shared_ptr<l2cap_channel_event> event_ = std::make_shared<l2cap_channel_event>();
        event_->m_channel_pkt = a_request;
        event_->m_type = l2cap_channel_event::event_type::handle_signaling_pkt;
        channel->handle_event(event_);
    }

    void acl_statemachine::handle_config_request(std::shared_ptr<l2cap_config_request> const& a_request)
    {
        auto channel = find_channel_state_machine(a_request->m_acl_handle, a_request->m_destionation_cid );
        if (!channel)
        {
            LogUtilDebug() << "No channel found. handle: " << a_request->m_acl_handle
                << ", local cid: " << a_request->m_destionation_cid;
            m_signaling_channel->send_reject_rsp( a_request->m_identifier,
                l2cap_command_reject_reason::invalid_cid, nullptr, 0 );
            return;
        }

        channel->handle_config_request( a_request );
    }

    void acl_statemachine::handle_config_response(std::shared_ptr<l2cap_config_response> const& a_response)
    {
        auto channel = find_channel_state_machine(a_response->m_acl_handle, a_response->m_source_cid );
        if (!channel)
        {
            LogUtilDebug() << "No channel found. handle: " << a_response->m_acl_handle
                << ", local cid: " << a_response->m_source_cid;
            return;
        }

        channel->handle_config_response(a_response);
    }

    void acl_statemachine::handle_disconnect_request(std::shared_ptr<l2cap_disconnect_request> const& a_request)
    {
        auto channel = find_channel_state_machine(a_request->m_connection_handle, a_request->m_destination_cid );
        uint8_t buffer[4] = { 0 };
        if (!m_signaling_channel)
        {
            LogUtilError() << "No signaling entity for handle: " << a_request->m_connection_handle;
            return;
        }

        if (!channel)
        {
            channel = find_channel_state_machine_by_remote_cid(a_request->m_connection_handle, a_request->m_source_cid );
            if (channel)
            {
                write_le16(buffer, channel->get_local_cid());
                write_le16(buffer, channel->get_remote_cid());
            }
            m_signaling_channel->send_reject_rsp(a_request->m_identifier, l2cap_command_reject_reason::invalid_cid, buffer, 4);
            return;
        }

        if (channel->get_remote_cid() != a_request->m_source_cid)
        {
            write_le16(buffer, channel->get_local_cid());
            write_le16(buffer, channel->get_remote_cid());
            m_signaling_channel->send_reject_rsp(a_request->m_identifier, l2cap_command_reject_reason::invalid_cid, buffer, 4);
            return;
        }

        channel->handle_disconnect_request( a_request );

        std::shared_ptr<l2cap_task_clear_pending_packets> tsk;
        tsk = std::make_shared<l2cap_task_clear_pending_packets>();
        tsk->m_acl_handle = channel->get_acl_handle();
        tsk->m_local_cid = channel->get_local_cid();
        tsk->m_acl_type = get_acl_type();
        tsk->set_source_module( l2cap_module::s_l2cap_module_name );
        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );

        for (auto it = m_channel_machines.begin(); it != m_channel_machines.end(); ++it)
        {
            if (*it == channel)
            {
                /**
                 * We always accept the disconnect request, so we can remove the channel state machine now.
                 */
                m_channel_machines.erase(it);
                return;
            }
        }
    }

    void acl_statemachine::handle_connect_response(std::shared_ptr<l2cap_connect_response> const& a_request)
    {
        std::shared_ptr<l2cap_channel_statemachine> channel;
        channel = find_channel_state_machine(a_request->m_source_cid );
        if (!channel)
        {
            LogUtilError() << "No channel machine for local channel id: " << a_request->m_source_cid;
            return;
        }

        std::shared_ptr<l2cap_channel_event> event_ = std::make_shared<l2cap_channel_event>();
        event_->m_channel_pkt = a_request;
        event_->m_type = l2cap_channel_event::event_type::handle_signaling_pkt;
        channel->handle_event(event_);
    }

    std::shared_ptr<l2cap_channel_statemachine> acl_statemachine::find_channel_state_machine
        (
        uint16_t a_acl_handle,
        uint16_t a_local_cid
        )
    {
        for (auto& ele : m_channel_machines)
        {
            if (ele->get_acl_handle() == a_acl_handle &&
                ele->get_local_cid() == a_local_cid)
            {
                return ele;
            }
        }
        return nullptr;
    }

    std::shared_ptr<l2cap_channel_statemachine> acl_statemachine::find_channel_state_machine_by_remote_cid
        (
        uint16_t a_acl_handle,
        uint16_t a_remote_cid
        )
    {
        for (auto& ele : m_channel_machines)
        {
            if (ele->get_acl_handle() == a_acl_handle &&
                ele->get_remote_cid() == a_remote_cid)
            {
                return ele;
            }
        }
        return nullptr;
    }

    std::shared_ptr<l2cap_channel_statemachine> acl_statemachine::find_channel_state_machine_by_cids
        (
        uint16_t a_local_cid,
        uint16_t a_remote_cid
        )
    {
        for( auto& ele : m_channel_machines )
        {
            if( ele->get_local_cid() == a_local_cid &&
                ele->get_remote_cid() == a_remote_cid )
            {
                return ele;
            }
        }
        return nullptr;
    }

    std::shared_ptr<l2cap_channel_statemachine> acl_statemachine::find_channel_state_machine
        (
        uint16_t a_local_cid
        )
    {
        for (auto& ele : m_channel_machines)
        {
            if (ele->get_local_cid() == a_local_cid)
            {
                return ele;
            }
        }
        return nullptr;
    }

}

