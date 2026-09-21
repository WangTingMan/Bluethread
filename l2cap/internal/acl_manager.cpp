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

#include "acl_manager.h"
#include "endian_convert.h"
#include "../l2cap_module.h"
#include "hci/hci_module.h"

#include "../../common/acl_connections_db.h"
#include "framework/log_util.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"
#include "../../gap/gap_module.h"

#include <functional>

namespace bluetooth
{

using namespace framework;

acl_manager::acl_manager()
{
    m_packet_recombinationer.set_complete_l2cap_packet_reporter
        (
        std::bind( &acl_manager::handle_completed_coming_acl_packet, this, std::placeholders::_1 )
        );
}

void acl_manager::add_new_connection
    (
    bluetooth_address a_remote_device,
    acl_type          a_type,
    bool              a_local_inited
    )
{
    acl_connection con_cb;
    con_cb.m_local_inited = a_local_inited;
    con_cb.m_remote_device = a_remote_device;
    con_cb.m_acl_type = a_type;

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    acl_db->add_acl_connection( con_cb );
}

void acl_manager::remove_connection( uint16_t a_handle )
{
    m_packet_recombinationer.clear_for_handle( a_handle );

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address,has] = acl_db->get_address( a_handle );
    if( has )
    {
        remove_pending_connection_request( address );
    }
    acl_db->remove_connection( a_handle );

    for (auto it = m_acl_state_machines.begin(); it != m_acl_state_machines.end();)
    {
        std::shared_ptr<acl_statemachine> acl_state_;
        acl_state_ = *it;

        if (acl_state_->get_acl_handle() == a_handle)
        {
            acl_state_->handle_event
                (
                static_cast<uint32_t>(acl_base_state::acl_event_type::acl_disconnected_with_acl_handle),
                a_handle
                );
            m_acl_state_machines.erase(it);
            break;
        }
        else
        {
            ++it;
        }
    }

    uint32_t count = 0;
    for( auto it = m_outgoing_l2cap_pkts.begin(); it != m_outgoing_l2cap_pkts.end(); )
    {
        std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address>& pkt = *it;
        if( pkt->m_acl_handle == a_handle )
        {
            it = m_outgoing_l2cap_pkts.erase( it );
            count++;
        }
        else
        {
            ++it;
        }
    }

    if( count > 0 )
    {
        LogUtilInfo() << "Remove " << count << " outgoing l2cap packets for acl handle " << a_handle;
    }

    if( m_acl_state_machines.empty() )
    {
        LogUtilInfo() << "All ACL connections have been disconnected, reset acl credit to max value.";
        m_br_edr_acl_credit = m_controller->get_acl_total_credit_value();
        m_packet_recombinationer.clear();
    }
}

void acl_manager::remove_pending_connection_request( bluetooth_address a_remote_device )
{
    for( auto it = m_pending_outgoing_connection_requests.begin(); it != m_pending_outgoing_connection_requests.end(); )
    {
        channel_connection_request& con_tsk = *it;
        if( con_tsk.m_remote_device == a_remote_device )
        {
            l2cap_callbacks cb = get_registered_callback( static_cast< uint16_t >( con_tsk.m_psm ) );
            if( !cb.m_handle_module.empty() )
            {
                std::shared_ptr<executable_task> task;
                task = std::make_shared<executable_task>();
                task->set_fun( std::bind( cb.m_channel_state_changed_callback, a_remote_device, 0x00, 0x00,
                    l2cap_channel_state_type::close_state, l2cap_channel_close_reason::page_timeout ),
                    cb.m_handle_module );
                task->set_source_module( l2cap_module::s_l2cap_module_name );
                framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            }
            else
            {
                cb.m_channel_state_changed_callback( a_remote_device, 0x00, 0x00, l2cap_channel_state_type::close_state,
                    l2cap_channel_close_reason::page_timeout );
            }
            it = m_pending_outgoing_connection_requests.erase( it );
        }
        else
        {
            ++it;
        }
    }
}

void acl_manager::update_new_connection
    (
    bluetooth_address a_remote_device,
    uint16_t          a_handle,
    acl_type          a_type,
    bool              a_encypted
    )
{
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    acl_db->update_acl( a_remote_device, a_handle, a_type, a_encypted );

    std::shared_ptr<acl_statemachine> acl_state_;
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_type() == a_type &&
            ele->get_remote_device() == a_remote_device )
        {
            acl_state_ = ele;
            LogUtilInfo() << "We already have a acl state machine for " << a_remote_device;
            break;
        }
    }

    if( !acl_state_ )
    {
        acl_state_ = std::make_shared<acl_statemachine>();
        acl_state_->set_acl_type( a_type );
        acl_state_->set_remote_device( a_remote_device );
        acl_state_->set_l2cap_callback_query(std::bind(&acl_manager::query_upper_layer_callbacks,
            this, std::placeholders::_1, std::placeholders::_2));
        LogUtilInfo() << "Make a new acl statemachine for " << a_remote_device;
    }
    acl_state_->start();
    acl_state_->handle_event( static_cast<uint32_t>( acl_base_state::acl_event_type::acl_connection_completed ),
        a_remote_device, a_handle, a_type, a_encypted );
    acl_state_->handle_pending_outgoing_connection_request( retrieve_all_pending_connection_request( a_remote_device ) );
    m_acl_state_machines.push_back( std::move( acl_state_ ) );
}

std::tuple<acl_type,bool> acl_manager::get_type( uint16_t a_handle )
{
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    return acl_db->get_type( a_handle );
}

void acl_manager::update_supervision
    ( 
    uint16_t a_handle,
    uint16_t a_timeout
    )
{
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    acl_db->update_acl( a_handle, a_timeout );
}

void acl_manager::handle_coming_acl_packet( std::shared_ptr<hci_data> a_hci_data )
{
    m_packet_recombinationer.handle_incoming_hci_packet( std::move( a_hci_data ) );
}

void acl_manager::accept_connection_req( std::shared_ptr<connection_request> const& a_request )
{
    bool found = false;
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_handle() == a_request->m_acl_handle )
        {
            ele->accept_connection_req( a_request );
            found = true;
            return;
        }
    }

    if( !found )
    {
        LogUtilError() << "No state machine for handle: " << a_request->m_acl_handle;
        return;
    }
}

void acl_manager::reject_connection_req
    (
    std::shared_ptr<connection_request> const&  a_request,
    connection_req_result                       a_reason
    )
{
    bool found = false;
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_handle() == a_request->m_acl_handle )
        {
            ele->reject_connection_req( a_request, a_reason );
            found = true;
            return;
        }
    }

    if( !found )
    {
        LogUtilError() << "No state machine for handle: " << a_request->m_acl_handle;
        return;
    }
}

void acl_manager::accept_config_req( std::shared_ptr<l2cap_config_request> const& a_request )
{
    bool found = false;
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_handle() == a_request->m_acl_handle )
        {
            ele->accept_config_req( a_request );
            found = true;
            return;
        }
    }

    if( !found )
    {
        LogUtilError() << "No state machine for handle: " << a_request->m_acl_handle;
        return;
    }
}

void acl_manager::config_local_channel_req( std::shared_ptr<l2cap_config_local_channel_request> const& a_request )
{
    bool found = false;
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_handle() == a_request->m_acl_handle )
        {
            ele->config_local_channel_req(a_request);
            found = true;
            return;
        }
    }

    if( !found )
    {
        LogUtilError() << "No state machine for handle: " << a_request->m_acl_handle;
        return;
    }
}

void acl_manager::send_upper_sdu
    (
    std::shared_ptr<l2cap_task_send_l2cap_sdu> const& a_tsk
    )
{
    bool found = false;
    for( auto& ele : m_acl_state_machines )
    {
        if (ele->get_remote_device() == a_tsk->m_remote_address)
        {
            ele->send_upper_sdu(a_tsk);
            found = true;
            return;
        }
    }

    if( !found )
    {
        LogUtilError() << "No state machine for device: " << a_tsk->m_remote_address;
        return;
    }
}

void acl_manager::handle_acl_completed_changed( std::vector<std::pair<uint16_t, uint16_t>> a_acl_completed )
{
    uint16_t completed_count = 0;
    for( auto& ele : a_acl_completed )
    {
        completed_count += ele.second;
    }

    if( completed_count > 0 )
    {
        m_br_edr_acl_credit += completed_count;
        LogUtilInfo() << "completed count = " << completed_count
            << " current credit = " << m_br_edr_acl_credit;
        send_next_outgoing_packet();
    }
}

void acl_manager::handle_request_connection_host
    (
    bluetooth_address a_remote_device,
    uint16_t a_psm
    )
{
    std::shared_ptr<acl_statemachine> acl_sm;
    for( auto& acl : m_acl_state_machines )
    {
        if( acl->get_remote_device() == a_remote_device )
        {
            acl_sm = acl;
            break;
        }
    }

    if( acl_sm )
    {
        LogUtilInfo() << "There is a ACL handle for device: " << a_remote_device;
        acl_sm->handle_request_channel_connection(a_psm);
    }
    else
    {
        // to do make acl connection and pending the request
        LogUtilInfo() << "No ACL handle for device " << a_remote_device.to_string()
            << ", to make one.";

        std::shared_ptr<gap_module::gap_module_task> tsk;
        tsk = std::make_shared<gap_module::gap_module_task>();
        tsk->m_gap_task_type = gap_module::gap_task_type::make_edr_acl_connection;
        tsk->set_source_module( l2cap_module::s_l2cap_module_name );
        tsk->m_address = a_remote_device;

        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );

        bool found = false;
        for( auto& ele : m_pending_outgoing_connection_requests )
        {
            if( ele.m_psm == a_psm &&
                ele.m_remote_device == a_remote_device )
            {
                LogUtilInfo() << "We already pending connection request for PSM: " << a_psm;
                found = true;
            }
        }

        if( !found )
        {
            channel_connection_request req;
            req.m_psm = a_psm;
            req.m_remote_device = a_remote_device;
            req.m_request_created_time = std::chrono::steady_clock::now();
            m_pending_outgoing_connection_requests.push_back( req );
        }
        return;
    }
}

bool acl_manager::retrieve_controller_info()
{
    bool status = false;

    auto con = framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );

    m_controller = con;
    status = ( con != nullptr );
    return status;
}

void acl_manager::handle_buffer_size_read_done()
{
    if( m_controller == nullptr )
    {
        LogUtilError() << "Controller information not available.";
        return;
    }
    m_br_edr_acl_credit = m_controller->get_acl_total_credit_value();
}

void acl_manager::schedule_outgoing_packet
    (
    std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address> a_completed_l2cap_pkt
    )
{
    m_outgoing_l2cap_pkts.push_back( std::move( a_completed_l2cap_pkt ) );
    send_next_outgoing_packet();
}

void acl_manager::close_channel_with_invalid_cid
    (
    std::shared_ptr<l2cap_task_remote_invalid_cid_channel_close> const& a_tsk
    )
{
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_type() == a_tsk->m_acl_type &&
            ele->get_acl_handle() == a_tsk->m_acl_handle )
        {
            ele->close_channel_with_invalid_cid( a_tsk );
            return;
        }
    }
}

void acl_manager::clear_pending_packets( std::shared_ptr<l2cap_task> a_tsk )
{
    uint16_t count = 0;
    auto detail_task = std::static_pointer_cast<l2cap_task_clear_pending_packets>( a_tsk );
    for( auto it = m_outgoing_l2cap_pkts.begin(); it != m_outgoing_l2cap_pkts.end(); )
    {
        std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address>& pkt = *it;
        if( pkt->m_acl_handle == detail_task->m_acl_handle &&
            pkt->m_local_cid == detail_task->m_local_cid )
        {
            it = m_outgoing_l2cap_pkts.erase( it );
            count++;
        }
        else
        {
            ++it;
        }
    }

    if( count > 0 )
    {
        LogUtilInfo() << "Clear " << count << " pending packets for acl handle "
            << detail_task->m_acl_handle << " local cid " << detail_task->m_local_cid;
    }
}

void acl_manager::disconnect_channel( std::shared_ptr<l2cap_task> a_tsk )
{
    auto detail_task = std::static_pointer_cast<l2cap_task_disconnect_channel_request>( a_tsk );
    std::shared_ptr<acl_statemachine> acl_sm;
    for( auto& acl : m_acl_state_machines )
    {
        if( acl->get_remote_device() == detail_task->m_remote_address ||
            acl->get_acl_handle() == detail_task->m_acl_handle )
        {
            acl_sm = acl;
            break;
        }
    }

    if( acl_sm )
    {
        LogUtilInfo() << "Disconnect channel for device: " << detail_task->m_remote_address
            << " local cid: " << detail_task->m_local_cid;
        acl_sm->disconnect_channel_req( detail_task->m_local_cid );
    }
    else
    {
        LogUtilError() << "No ACL handle for device " << detail_task->m_remote_address.to_string()
            << ", cannot disconnect channel.";
    }
}

void acl_manager::remove_channel_from_cache( std::shared_ptr<l2cap_task> a_tsk )
{
    uint16_t count = 0;
    auto detail_task = std::static_pointer_cast<l2cap_task_remove_channel_from_cache>( a_tsk );
    for( auto& acl : m_acl_state_machines )
    {
        if( acl->get_acl_handle() == detail_task->m_acl_handle &&
            acl->get_acl_type() == detail_task->m_acl_type )
        {
            acl->remove_channel_from_cache( detail_task->m_local_cid );
            break;
        }
    }

    for( auto it = m_outgoing_l2cap_pkts.begin(); it != m_outgoing_l2cap_pkts.end(); )
    {
        std::shared_ptr<l2cap_task_send_l2cap_sdu_with_remote_address>& pkt = *it;
        if( pkt->m_acl_handle == detail_task->m_acl_handle &&
            pkt->m_local_cid == detail_task->m_local_cid )
        {
            it = m_outgoing_l2cap_pkts.erase( it );
            count++;
        }
        else
        {
            ++it;
        }
    }

    if( count > 0 )
    {
        LogUtilInfo() << "Clear " << count << " pending packets for acl handle "
            << detail_task->m_acl_handle << " local cid " << detail_task->m_local_cid;
    }
}

void acl_manager::initialize_channel_machine( std::shared_ptr<l2cap_channel_statemachine> const& a_channel )
{
    a_channel->start();
}

std::vector<channel_connection_request> acl_manager::retrieve_all_pending_connection_request
    (
    bluetooth_address a_remote_device
    )
{
    std::vector<channel_connection_request> connection_requests;
    for( auto it = m_pending_outgoing_connection_requests.begin();
        it != m_pending_outgoing_connection_requests.end(); )
    {
        if( it->m_remote_device == a_remote_device )
        {
            connection_requests.push_back( *it );
            it = m_pending_outgoing_connection_requests.erase( it );
        }
        else
        {
            ++it;
        }
    }
    return connection_requests;
}

bool acl_manager::l2cap_size_check( std::shared_ptr<hci_data> const& a_hci_data )
{
    std::vector<uint8_t> const& buffer = a_hci_data->m_buffer;
    if( buffer.size() < s_l2cap_offset + 2 )
    {
        LogUtilError() << "l2cap buffer too small.";
        return false;
    }

    uint16_t sdu_size = le_to_host16( buffer.data() + 4 );

    uint32_t expected_size = sdu_size + s_l2cap_offset + 4;

    if( buffer.size() == expected_size )
    {
        return true;
    }
    else if( buffer.size() > expected_size )
    {
        LogUtilWarning() << "l2cap packet size is greater than exepected size";
        return true;
    }
    else
    {
        LogUtilError() << "l2cap packet size is smaller than expected size.";
        return false;
    }

    return true;
}

/*
* @todo Future QoS improvement: Implement multi‑priority scheduler with time‑window quota.
*       Current implementation is simple FIFO, risk: high‑volume SPP data may block AVDTP/L2CAP signaling;
*       audio stream may starve SPP traffic.
*       Priority definition:
*          0) L2CAP link signaling (highest, guarantee link alive)
*          1) Profile control signaling(AVDTP/AVRCP)
*          2) Audio media packets
*          3) SPP/RFCOMM ordinary data(lowest)
*       Must use persistent time‑based quota counters across scheduler invocations,
*       avoid starvation caused by credit arriving in multiple small batches.
*/
void acl_manager::send_next_outgoing_packet()
{
    while( true )
    {
        if( m_outgoing_l2cap_pkts.empty() )
        {
            return;
        }

        if( m_br_edr_acl_credit <= 0 )
        {
            LogUtilInfo() << "No more credit to send outgoing packet.";
            return;
        }

        auto tsk = m_outgoing_l2cap_pkts.front();
        m_outgoing_l2cap_pkts.erase( m_outgoing_l2cap_pkts.begin() );

        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = tsk->m_hci_packet;

        if( hci_task->m_hci_data == nullptr )
        {
            LogUtilFatal() << "hci data is null.";
            return;
        }

        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( l2cap_module::s_l2cap_module_name );
        framework::framework_manager::get_instance().get_thread_manager()
            .post_task( hci_task, framework::source_here );
        m_br_edr_acl_credit--;
        LogUtilInfo() << "Send outgoing packet, remaining credit = " << m_br_edr_acl_credit;
    }
}

void acl_manager::handle_completed_coming_acl_packet( std::shared_ptr<hci_data> const& a_hci_data )
{
    std::vector<uint8_t> const& buffer = a_hci_data->m_buffer;
    uint8_t handle[2];
    handle[0] = buffer[0];
    handle[1] = buffer[1] & 0x0F;
    uint16_t acl_handle = le_to_host16( handle );
    auto [type_, has_] = get_type( acl_handle );
    if( !has_ )
    {
        LogUtilError() << "No such acl handle registered.";
        return;
    }

    if( !l2cap_size_check( a_hci_data ) )
    {
        LogUtilError() << "l2cap packet size wrong. Ignore this packet";
        return;
    }

    if( type_ != acl_type::br_edr_acl )
    {
        LogUtilError() << "Not handle not edr acl packet.";
        return;
    }

    bool handled = false;
    for( auto& ele : m_acl_state_machines )
    {
        if( ele->get_acl_handle() == acl_handle )
        {
            ele->handle_coming_acl_packet( a_hci_data );
            handled = true;
        }
    }

    if( !handled )
    {
        LogUtilError() << "no such ACL statemachine to handle received ACL pakcet";
        return;
    }
}

}

