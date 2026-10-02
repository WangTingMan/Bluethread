#pragma once

#include "sdp_service_record_db.h"
#include "sdp_protocol.h"
#include "sdp_server.h"
#include "sdp_connection.h"
#include "sdp/sdp_task.h"

#include <l2cap/l2cap_common.h>

namespace bluetooth
{

class sdp_manager
{

public:

    void init();

    void init_db()
    {
        m_local_service.init_db();
    }

    void clear_db()
    {
        m_local_service.clear();
    }

    void handle_sdp_connect_request( std::shared_ptr<connection_request> const& a_request );

    void handle_config_request( std::shared_ptr<l2cap_config_request> const& a_request );

    void config_local_channel
        (
        uint16_t a_acl_handle,
        uint16_t a_remote_cid
        );

    void handle_connection_state_changed
        (
        bluetooth_address a_address,
        uint16_t a_local_cid,
        uint16_t a_remote_cid,
        l2cap_channel_state_type a_state,
        l2cap_channel_close_reason a_reason
        );

    void handle_sdu( std::shared_ptr<hci_data> );

    void handle_service_search_request( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_service_search_attribute_request( std::shared_ptr<hci_data> const& a_hci_data );

    void handle_register_record( std::shared_ptr<sdp_task> const& a_task );

    void handle_service_search( std::shared_ptr<sdp_task> const& a_task );

    /**
     * Handle the service search attribute request from upper layer
     */
    void handle_service_search_attribute_host( std::shared_ptr<sdp_task> const& a_task );

    void send_packet
    (
        std::shared_ptr<sdp_protocol_base> const& a_packet,
        bluetooth_address                           a_remote_address
    );

    void send_error_rsp( sdp_error_code a_code );

    bool verify_received_packer( std::shared_ptr<hci_data> const& a_packet );

    std::shared_ptr<sdp_connection> find_connection( bluetooth_address const& a_address );

    void remove_connection( bluetooth_address const& a_address );

private:

    sdp_server m_local_service;
    sdp_header m_sdp_header;
    std::vector<std::shared_ptr<sdp_connection>> m_connections;
    std::vector<std::shared_ptr<sdp_protocol_base>> m_pending_reqs;

};

}
