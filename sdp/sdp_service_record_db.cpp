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

#include "sdp_service_record_db.h"

#include "framework/log_util.h"

namespace bluetooth
{

sdp_service_record_db::sdp_service_record_db()
{
}

std::shared_ptr<sdp_service_record> sdp_service_record_db::add_service_record( uint32_t a_service_record_handle )
{
    std::shared_ptr<sdp_service_record> record = find_service( a_service_record_handle );
    if( record )
    {
        return record;
    }

    std::shared_ptr<sdp_service_record> new_record = std::make_shared<sdp_service_record>();
    new_record->set_service_handle( a_service_record_handle );
    m_local_services.push_back( new_record );
    return new_record;
}

std::shared_ptr<sdp_service_record> sdp_service_record_db::find_service( uint32_t a_service_record_handle )
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

std::list<std::shared_ptr<sdp_service_record>> sdp_service_record_db::find_matched_uuids_record
    ( std::vector<uuid> const& a_uuids )
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

void sdp_service_record_db::add_service_record( std::shared_ptr<sdp_service_record> a_service_record )
{
    for( auto& ele : m_local_services )
    {
        if( a_service_record->get_service_handle() == ele->get_service_handle() )
        {
            LogUtilError() << "Add service handle: " << ele->get_service_handle() << " again.";
            ele = a_service_record;
            return;
        }
    }

    m_local_services.push_back( a_service_record );
}

}

