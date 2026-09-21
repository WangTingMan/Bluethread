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

#include "sdp_server.h"
#include "sdp_self_service_record.h"
#include "did_service_record.h"

#include "framework\log_util.h"

namespace bluetooth
{

sdp_server::sdp_server()
{

}

void sdp_server::init_db()
{
    // We must register the sdp self service record firstly
    uint32_t record_id = register_record( std::make_shared<sdp_self_service_record>() );
    if( 0x00 != record_id )
    {
        LogUtilError() << "SDP self service record's id must be 0x00";
    }

    register_record( std::make_shared<did_service_record>() );

}

void sdp_server::handle_service_search_attribute_request
    (
    std::shared_ptr<sdp_service_search_attribute_req> const& a_request
    )
{
    std::list<std::shared_ptr<sdp_service_record>> record_matched;
    std::vector<sdp_data_element> attribute_list_result;

    record_matched = m_service_record_db.find_matched_uuids_record( a_request->m_matching_uuids );
    for( auto& ele : record_matched )
    {
        sdp_data_element matched_values;
        std::vector<sdp_data_element> matched_details;
        sdp_service_record& record = *ele;
        for( auto& attribute_ele : record )
        {
            uint16_t id = attribute_ele.get_attribute_id();
            bool found = false;
            for( auto& match_id : a_request->m_requested_id_ranges )
            {
                if( id <= match_id.second && id >= match_id.first )
                {
                    sdp_data_element data_element;
                    data_element.set_uint16_value( id );
                    matched_details.push_back( data_element );
                    matched_details.push_back( attribute_ele.get_value() );
                    found = true;
                    break;
                }
            }

            if( found )
            {
                continue;
            }

            for( auto& match_id : a_request->m_matching_ids )
            {
                if( id == match_id )
                {
                    sdp_data_element data_element;
                    data_element.set_uint16_value( id );
                    matched_details.push_back( data_element );
                    matched_details.push_back( attribute_ele.get_value() );
                    found = true;
                    break;
                }
            }
        }

        if( !matched_details.empty() )
        {
            matched_values.set_elements( std::move( matched_details ) );
            attribute_list_result.push_back( std::move( matched_values ) );
        }
    }

    std::shared_ptr<sdp_service_search_attribute_rsp> rsp;
    rsp = std::make_shared<sdp_service_search_attribute_rsp>();
    rsp->m_local_cid = a_request->m_local_cid;
    rsp->m_transaction_id = a_request->m_transaction_id;
    sdp_data_element element;
    element.set_elements( attribute_list_result );
    auto raw = element.get_raw_buffer();
    if( raw.size() > a_request->m_max_return_count )
    {
        LogUtilError() << "Need to split the buffer";
        // TODO Need to split the buffer
        return;
    }
    rsp->m_attribute_list = std::move( raw );
    rsp->m_remote_device = a_request->m_remote_device;

    m_packet_send( rsp, rsp->m_remote_device);
}

uint32_t sdp_server::register_record( std::shared_ptr<sdp_service_record> a_record )
{
    uint32_t handle = m_next_record_id++;

    a_record->set_service_handle( handle );
    a_record->sort_attribute_by_id();
    m_service_record_db.add_service_record( a_record );

    return handle;
}

}

