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

#include "framework/log_util.h"

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

uint32_t sdp_server::register_record( std::shared_ptr<sdp_service_record> a_record )
{
    uint32_t handle = m_next_record_id++;

    a_record->set_service_handle( handle );
    a_record->sort_attribute_by_id();
    for( auto& ele : m_local_services )
    {
        if( a_record->get_service_handle() == ele->get_service_handle() )
        {
            LogUtilError() << "Add service handle: " << ele->get_service_handle() << " again.";
            ele = a_record;
            return handle;
        }
    }

    m_local_services.push_back( a_record );

    return handle;
}

std::list<std::shared_ptr<sdp_service_record>> sdp_server::find_matched_uuids_record
    (
    std::vector<uuid> const& a_uuids
    )
{
    std::list<std::shared_ptr<sdp_service_record>> ret;
    for( auto& ele : m_local_services )
    {
        if( ele->is_matching_uuids( a_uuids ) )
        {
            ret.push_back( ele );
        }
    }
    return ret;
}

std::shared_ptr<sdp_service_record> sdp_server::find_service( uint32_t a_service_record_handle )
{
    std::shared_ptr<sdp_service_record> record;
    for( auto& ele : m_local_services )
    {
        if( a_service_record_handle == ele->get_service_handle() )
        {
            record = ele;
            break;
        }
    }
    return record;
}

}

