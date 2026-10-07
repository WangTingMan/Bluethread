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

#include "sdp_connection.h"
#include "sdp_manager.h"

namespace bluetooth
{

void sdp_connection::handle_error_rsp
    (
    sdp_header& _sdp_header,
    std::shared_ptr<sdp_error_response> const& a_error_rsp
    )
{

}

void sdp_connection::handle_service_search_request
    (
    sdp_header& _sdp_header,
    std::shared_ptr<sdp_service_search_request> const& a_error_rsp
    )
{
    if( !a_error_rsp )
    {
        return;
    }

    if( m_incoming_pending_req != sdp_pdu_id::sdp_invalid_pdu )
    {
        m_sdp_manager->send_error_rsp( m_acl_handle, sdp_error_code::reject_with_resource_limited );
        return;
    }

    m_incoming_pending_req = sdp_pdu_id::sdp_service_search_req;


}

}

