#include "sdp_manager.h"

#include "framework/log_util.h"
#include "framework/module_manager.h"
#include "framework/framework_manager.h"
#include "framework/executable_task.h"
#include "framework/framework_event.h"

#include "endian_convert.h"

#include "common/acl_connections_db.h"
#include "l2cap/l2cap_module.h"
#include "l2cap/l2cap_common.h"
#include "sdp/sdp_module.h"

namespace bluetooth
{

using namespace framework;

void sdp_manager::init()
{
    m_local_service.set_send_packet_fun( std::bind( &sdp_manager::send_packet, this,
        std::placeholders::_1, std::placeholders::_2 ) );
}

void sdp_manager::handle_sdp_connect_request( std::shared_ptr<connection_request> const& a_request )
{
    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has] = acl_db->get_address( a_request->m_acl_handle );

    if( !has )
    {
        LogUtilError() << "No acl handle: " << a_request->m_acl_handle;
        return;
    }

    bool already_connected = false;
    for( auto& ele : m_connections )
    {
        if( ele->m_address == address &&
            ele->get_connection_status() != connection_status::disconnected
            )
        {
            already_connected = true;
            break;
        }
    }

    if( !already_connected )
    {
        std::shared_ptr<l2cap_task_accept_channle_connection_req> acp_con_task;
        acp_con_task = std::make_shared<l2cap_task_accept_channle_connection_req>();
        acp_con_task->set_source_module( sdp_module::s_sdp_module_name );
        acp_con_task->m_connect_request = a_request;
        auto conn_ = find_connection( address );
        if( !conn_ )
        {
            LogUtilDebug() << "Make one due to no sdp connection control block for device: " << address.to_string();

            std::shared_ptr<sdp_connection> sdp_conn = std::make_shared<sdp_connection>();
            sdp_conn->m_address = address;
            sdp_conn->set_connection_status( connection_status::connecting );
            m_connections.push_back( sdp_conn );
            conn_ = sdp_conn;
        }

        framework_manager::get_instance().get_thread_manager().post_task( acp_con_task, framework::source_here );

        if( !conn_->get_config_local_req_sent() )
        {
            config_local_channel( a_request->m_acl_handle, a_request->m_source_cid );
            conn_->set_config_local_req_sent( true );
        }
    }
    else
    {

        std::shared_ptr<l2cap_task_reject_channle_connection_req> task;
        task = std::make_shared<l2cap_task_reject_channle_connection_req>();
        task->set_source_module( sdp_module::s_sdp_module_name );
        task->m_connect_request = a_request;
        task->m_reject_reason = connection_req_result::connection_refused_no_resource;
        framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
        LogUtilDebug() << "We already connected to device: " << address.to_string();
    }

}

void sdp_manager::handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request )
{
    /**
     * TODO check the configuration request can be accpeted or not.
     */

    std::shared_ptr<l2cap_task_accept_channel_config_req> task;
    task = std::make_shared<l2cap_task_accept_channel_config_req>();
    task->set_source_module( sdp_module::s_sdp_module_name );
    task->m_config_request = a_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    auto [address, has] = acl_db->get_address( a_request->m_acl_handle );

    if( !has )
    {
        LogUtilError() << "No acl handle: " << a_request->m_acl_handle;
        return;
    }

    auto conn_ = find_connection( address );
    if( !conn_ )
    {
        LogUtilError() << "Make one due to no sdp connection control block for device: " << address.to_string();

        std::shared_ptr<sdp_connection> sdp_conn = std::make_shared<sdp_connection>();
        sdp_conn->m_address = address;
        sdp_conn->set_connection_status( connection_status::connecting );
        m_connections.push_back( sdp_conn );
        conn_ = sdp_conn;
    }

    conn_->set_config_remote_req_received( true );
    conn_->set_config_remote_rsp_sent( true );

    if( 0x0000 == conn_->m_remote_cid )
    {
        conn_->m_remote_cid = a_request->m_source_cid;
    }

    if( !conn_->get_config_local_req_sent() )
    {
        config_local_channel( a_request->m_acl_handle, conn_->m_remote_cid );
        conn_->set_config_local_req_sent( true );
    }
}

void sdp_manager::config_local_channel
    (
    uint16_t a_acl_handle,
    uint16_t a_remote_cid
    )
{
    /**
    * TODO: Upper layer shall send L2CAP CONFIGURATION_REQ once channel transitions
    * to wait_config state or other suitable state if configuration has not been performed.
    */
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
    task->set_source_module( sdp_module::s_sdp_module_name );
    task->m_config_local = cfg_request;
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void sdp_manager::handle_connection_state
    (
    bluetooth_address a_address,
    uint16_t a_local_cid,
    uint16_t a_remote_cid,
    l2cap_channel_state_type a_state,
    l2cap_channel_close_reason a_reason
    )
{
    auto sdp_connection_ = find_connection( a_address );
    if( !sdp_connection_ )
    {
        if( a_state != l2cap_channel_state_type::close_state )
        {
            sdp_connection_ = std::make_shared<sdp_connection>();
            m_connections.push_back( sdp_connection_ );
            sdp_connection_->m_address = a_address;
        }
        else
        {
            return;
        }
    }

    sdp_connection_->m_local_cid = a_local_cid;
    sdp_connection_->m_remote_cid = a_remote_cid;
    switch( a_state )
    {
    case bluetooth::l2cap_channel_state_type::close_state:
        sdp_connection_->set_connection_status( connection_status::disconnected );
        remove_connection( a_address );
        break;
    case bluetooth::l2cap_channel_state_type::open:
        sdp_connection_->set_connection_status( connection_status::connected );
        for( auto it = m_pending_reqs.begin(); it != m_pending_reqs.end(); )
        {
            std::shared_ptr<sdp_protocol_base> req = *it;
            if( req->m_remote_device == a_address )
            {
                send_packet( req, a_address );
                it = m_pending_reqs.erase( it );
                break;
            }
            else
            {
                ++it;
            }
        }
        break;
    case l2cap_channel_state_type::wait_config:
        if( !sdp_connection_->get_config_local_req_sent() )
        {
            auto acl_db = framework_manager::get_instance().get_info_manager()
                .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
            auto [acl_handle, has] = acl_db->get_handle( a_address );

            if( has )
            {
                config_local_channel( acl_handle, a_remote_cid );
                sdp_connection_->set_config_local_req_sent( true );
            }
            else
            {
                LogUtilError() << "No acl handle for device: " << a_address.to_string();
            }
        }
        break;
    default:
        sdp_connection_->set_connection_status( connection_status::connecting );
        break;
    }

}

void sdp_manager::handle_sdu( std::shared_ptr<hci_data> a_sdu )
{
    if( !verify_received_packer( a_sdu ) )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    sdp_pdu_id pdu_id = sdp_pdu_id::sdp_error_rsp;
    uint8_t* p_sdp_header = a_sdu->m_buffer.data() + m_sdp_header.l2cap_header::header_size();
    pdu_id = static_cast<sdp_pdu_id>( p_sdp_header[0] );

    switch( pdu_id )
    {
    case bluetooth::sdp_pdu_id::sdp_error_rsp:
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_req:
        handle_service_search_request( a_sdu );
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_rsp:
        break;
    case bluetooth::sdp_pdu_id::sdp_service_attr_req:
        break;
    case bluetooth::sdp_pdu_id::sdp_service_attr_rsp:
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_attr_req:
        handle_service_search_attribute_request( a_sdu );
        break;
    case bluetooth::sdp_pdu_id::sdp_service_search_attr_rsp:
        break;
    default:
        break;
    }
}

void sdp_manager::handle_service_search_request( std::shared_ptr<hci_data> const& a_hci_data )
{
    uint8_t* p_sdp_header = a_hci_data->m_buffer.data() + m_sdp_header.l2cap_header::header_size();
    uint8_t* p_sdu = p_sdp_header + 5;
    uint16_t parameter_size = be_to_host16( p_sdp_header + 3 );
    uint16_t transaction_id = be_to_host16( p_sdp_header + 1 );
    auto [attribute_value, ret] = sdp_data_element::parse_from( p_sdu, parameter_size );
    if( !ret )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }


}

void sdp_manager::handle_service_search_attribute_request( std::shared_ptr<hci_data> const& a_hci_data )
{
    uint8_t* p_sdp_header = a_hci_data->m_buffer.data() + m_sdp_header.l2cap_header::header_size();
    uint8_t* p_sdu = p_sdp_header + 5;
    uint16_t parameter_size = be_to_host16( p_sdp_header + 3 );
    uint16_t transaction_id = be_to_host16( p_sdp_header + 1 );

    uint32_t pattern_size = 0;
    bool ret = sdp_data_element::recognite_data_element( p_sdu, parameter_size, pattern_size );
    if( !ret )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    auto [pattern, parse_ret] = sdp_data_element::parse_from( p_sdu, pattern_size );
    if( !pattern.can_as_elements() )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    auto uuids = pattern.get_uuid_from_elements();
    if( uuids.size() > 12 )
    {
        // This value's minmum value is 12. See sdp specification
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    if( parameter_size < pattern_size + 2 )
    {
        // We need maximum attribute byte count here. But there is no more buffer to use.
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    uint16_t max_attribute_bytes_count = be_to_host16( p_sdu + pattern_size );
    if( max_attribute_bytes_count < 0x0007 )
    {
        // This value's minmum value is 0x0007. See sdp specification
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    uint32_t attribute_id_size = 0;
    ret = sdp_data_element::recognite_data_element( p_sdu + pattern_size + 2, parameter_size - pattern_size - 2, attribute_id_size );
    if( !ret )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    auto [attribute_ids, parse_id_ret] = sdp_data_element::parse_from( p_sdu + pattern_size + 2, attribute_id_size );
    if( !attribute_ids.can_as_elements() )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    if( !attribute_ids.can_as_elements() )
    {
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    std::vector<uint16_t> requested_ids;
    std::vector<std::pair<uint16_t, uint16_t>> requested_id_ranges;
    std::vector<sdp_data_element>const& ids = attribute_ids.get_elements();
    for( auto& ele : ids )
    {
        if( ele.can_as_uint16() )
        {
            requested_ids.push_back( ele.get_uint16_value() );
            continue;
        }

        if( ele.can_as_uint32() )
        {
            uint32_t range_raw = ele.get_uint32_value();
            uint8_t buffer[4] = { 0 };
            write_be32( buffer, range_raw );
            std::pair<uint16_t, uint16_t> range;
            range.first = be_to_host16( buffer );
            range.second = be_to_host16( buffer + 2 );
            requested_id_ranges.push_back( range );
            continue;
        }

        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    if( parameter_size < pattern_size + 2 + attribute_id_size + 1 )
    {
        // We need continuation state here. But there is no more buffer to use.
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    uint8_t continue_size = p_sdu[pattern_size + 2 + attribute_id_size];
    if( parameter_size < pattern_size + 2 + attribute_id_size + 1 + continue_size )
    {
        // We need continuation state here. But there is no more buffer to use.
        send_error_rsp( sdp_error_code::invalid_syntax );
        return;
    }

    std::vector<uint8_t> continue_state;
    continue_state.insert( continue_state.end(), p_sdu + pattern_size + 2 + attribute_id_size + 1,
        p_sdu + pattern_size + 2 + attribute_id_size + 1 + continue_size );

    auto acl_db = framework_manager::get_instance().get_info_manager()
        .get_detail_information<acl_connections_db>( acl_connections_db::s_acl_connections_db_name );
    uint16_t acl_handle = 0;
    get_acl_handle_from_hci( a_hci_data, acl_handle );
    auto [remote_device, has] = acl_db->get_address( acl_handle );

    std::shared_ptr<sdp_service_search_attribute_req> request;
    request = std::make_shared<sdp_service_search_attribute_req>();
    request->m_matching_uuids = uuids;
    request->m_max_return_count = max_attribute_bytes_count;
    request->m_matching_ids = requested_ids;
    request->m_requested_id_ranges = requested_id_ranges;
    request->m_continue_info = continue_state;
    request->m_local_cid = retrieve_local_cid( a_hci_data );
    request->m_remote_device = remote_device;
    m_local_service.handle_service_search_attribute_request( request );
}

void sdp_manager::handle_register_record( std::shared_ptr<sdp_task> const& a_task )
{
    auto detail_tsk = std::static_pointer_cast<sdp_task_register_service_record>( a_task );

    uint32_t handle = m_local_service.register_record( detail_tsk->m_service_record );

    std::shared_ptr<executable_task> tsk;
    tsk = std::make_shared<executable_task>();
    tsk->set_source_module( sdp_module::s_sdp_module_name );
    tsk->set_position( source_here );
    tsk->set_fun( std::bind( detail_tsk->m_registered_callback, handle ), detail_tsk->m_callback_handle_module );
    tsk->set_target_module( detail_tsk->m_callback_handle_module );

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void sdp_manager::handle_service_search( std::shared_ptr<sdp_task> const& a_task )
{

}

void sdp_manager::handle_service_search_attribute_host( std::shared_ptr<sdp_task> const& a_task )
{
    auto detail_tsk = std::static_pointer_cast<sdp_task_service_search_attribute>( a_task );

    if( detail_tsk->m_service_uuid.empty() )
    {
        LogUtilError() << "sdp_service_search_attribute_req requires at least one service uuid";
        return;
    }

    std::shared_ptr<sdp_service_search_attribute_req> request;
    request = std::make_shared<sdp_service_search_attribute_req>();
    request->m_matching_uuids = detail_tsk->m_service_uuid;
    request->m_requested_id_ranges = detail_tsk->m_requested_id_ranges;
    request->m_max_return_count = 0xFFFF;
    request->m_matching_ids = detail_tsk->m_attribute_id_list;
    request->m_remote_device = detail_tsk->m_remote_device;

    auto conn_ = find_connection( detail_tsk->m_remote_device );
    if( conn_ )
    {
        //todo : execute the resut
    }
    else
    {
        std::shared_ptr<l2cap_task_connection_request> tsk;
        tsk = std::make_shared<l2cap_task_connection_request>();
        tsk->m_remote_device = detail_tsk->m_remote_device;
        tsk->m_psm = defined_l2cap_psm::sdp;
        tsk->set_source_module( sdp_module::s_sdp_module_name );
        tsk->set_position( source_here );

        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );

        std::shared_ptr<sdp_connection> sdp_conn = std::make_shared<sdp_connection>();
        sdp_conn->m_address = detail_tsk->m_remote_device;
        sdp_conn->set_connection_status( connection_status::connecting );
        m_connections.push_back( sdp_conn );

        m_pending_reqs.push_back( request );
    }
}

void sdp_manager::send_packet
    (
    std::shared_ptr<sdp_protocol_base> const& a_packet,
    bluetooth_address                         a_remote_address
    )
{
    if( !a_packet )
    {
        LogUtilError() << "empty packet to send.";
        return;
    }

    uint16_t local_cid = 0x00;
    std::shared_ptr<hci_data> hci_packet;
    size_t sdp_sdu_size = 0x00; // the SDP protocal data total length
    uint8_t* p_sdp_sdu = nullptr;
    size_t offset = 0;

    switch( a_packet->m_pdu_id )
    {
    case sdp_pdu_id::sdp_service_search_attr_rsp:
    {
        std::shared_ptr<sdp_service_search_attribute_rsp> rsp;
        rsp = std::static_pointer_cast<sdp_service_search_attribute_rsp>( a_packet );
        local_cid = rsp->m_local_cid;
        hci_packet = std::make_shared<hci_data>();

        sdp_sdu_size = 2 + rsp->m_attribute_list.size() + 1 + rsp->m_continue_info.size();
        size_t hci_total_size = m_sdp_header.header_size() + sdp_sdu_size;
        hci_packet->m_buffer.resize( hci_total_size );

        // fill the sdp sdu field.
        p_sdp_sdu = hci_packet->m_buffer.data() + m_sdp_header.header_size();
        write_be16( p_sdp_sdu, static_cast<uint16_t>( rsp->m_attribute_list.size() ) );
        offset += 2;
        memcpy( p_sdp_sdu + offset, rsp->m_attribute_list.data(), rsp->m_attribute_list.size() );
        offset += rsp->m_attribute_list.size();
        p_sdp_sdu[offset] = static_cast<uint8_t>( rsp->m_continue_info.size() );
        offset += 1;
        memcpy( p_sdp_sdu + offset, rsp->m_continue_info.data(), rsp->m_continue_info.size() );

        m_sdp_header.set_transcation_id( rsp->m_transaction_id );
    }
    break;
    case sdp_pdu_id::sdp_service_search_attr_req:
    {
        std::shared_ptr<sdp_service_search_attribute_req> req;
        req = std::static_pointer_cast<sdp_service_search_attribute_req>( a_packet );
        for( auto& ele : m_connections )
        {
            if( ele->m_address == req->m_remote_device )
            {
                local_cid = ele->m_local_cid;
                break;
            }
        }

        hci_packet = std::make_shared<hci_data>();
        /**
         * todo: need handle request: sdp_pdu_id::sdp_service_search_attr_req.
         */
        LogUtilFatal( "need handle request: sdp_pdu_id::sdp_service_search_attr_req." );
    }
    break;
    default:
        LogUtilError() << "Packet type not handled to sent: " << a_packet->m_pdu_id;
        break;
    }

    if( !hci_packet )
    {
        LogUtilDebug() << "No hci packet to send.";
        return;
    }

    m_sdp_header.set_sdu_length( static_cast<uint16_t>( sdp_sdu_size ) );
    m_sdp_header.set_pdu_id( a_packet->m_pdu_id );
    m_sdp_header.to_raw_buffer( hci_packet->m_buffer.data(), static_cast<uint32_t>( hci_packet->m_buffer.size() ) );

    std::shared_ptr<l2cap_task_send_l2cap_sdu> tsk;
    tsk = std::make_shared<l2cap_task_send_l2cap_sdu>();
    tsk->m_hci_packet = hci_packet;
    tsk->m_local_cid = local_cid;
    tsk->set_source_module( sdp_module::s_sdp_module_name );
    tsk->set_target_module( l2cap_module::s_l2cap_module_name );
    tsk->set_position( source_here );
    tsk->m_remote_address = a_remote_address;

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void sdp_manager::send_error_rsp( sdp_error_code a_code )
{

}

bool sdp_manager::verify_received_packer( std::shared_ptr<hci_data> const& a_packet )
{
    if( a_packet->m_buffer.size() < m_sdp_header.header_size() )
    {
        return false;
    }

    uint16_t parameter_size = be_to_host16( a_packet->m_buffer.data() +
        m_sdp_header.l2cap_header::header_size() + 3 );

    if( a_packet->m_buffer.size() < m_sdp_header.header_size() + parameter_size )
    {
        return false;
    }

    return true;
}

std::shared_ptr<sdp_connection> sdp_manager::find_connection( bluetooth_address const& a_address )
{
    for( auto& ele : m_connections )
    {
        if( ele->m_address == a_address )
        {
            return ele;
        }
    }
    return nullptr;
}

void sdp_manager::remove_connection( bluetooth_address const& a_address )
{
    for( auto it = m_connections.begin(); it != m_connections.end(); ++it )
    {
        if( ( *it )->m_address == a_address )
        {
            m_connections.erase( it );
            break;
        }
    }
}

}
