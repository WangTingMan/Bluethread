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

#include "acl_connections_db.h"

#include "framework/log_util.h"

namespace bluetooth
{

acl_connections_db::acl_connections_db()
{
    set_name( s_acl_connections_db_name );
}

bool acl_connections_db::edr_acl_connected( bluetooth_address const& a_address )
{
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    for( auto& ele : m_acls )
    {
        if( ele->m_remote_device == a_address )
        {
            return true;
        }
    }

    return false;
}

std::tuple<uint16_t, bool> acl_connections_db::get_handle( bluetooth_address const& a_address )
{
    uint16_t handle = 0x00;
    bool has = false;
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    for( auto& ele : m_acls )
    {
        if( ele->m_remote_device == a_address )
        {
            handle = ele->m_connection_handle;
            has = true;
            break;
        }
    }
    return { handle,has };
}

std::tuple<bluetooth_address, bool> acl_connections_db::get_address( uint16_t a_handle )
{
    bluetooth_address address;
    bool has = false;
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    for( auto& ele : m_acls )
    {
        if( ele->m_connection_handle == a_handle )
        {
            address = ele->m_remote_device;
            has = true;
            break;
        }
    }
    return { address,has };
}

std::tuple<acl_type, bool> acl_connections_db::get_type( uint16_t a_handle )
{
    acl_type type = acl_type::invalid_type;
    bool has = false;
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    for( auto& ele : m_acls )
    {
        if( ele->m_connection_handle == a_handle )
        {
            type = ele->m_acl_type;
            has = true;
            break;
        }
    }
    return { type,has };
}

void acl_connections_db::add_acl_connection( acl_connection a_acl )
{
    bool found = false;
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    for( auto& ele : m_acls )
    {
        if( ele->m_acl_type == a_acl.m_acl_type &&
            ele->m_remote_device == a_acl.m_remote_device )
        {
            *ele = a_acl;
            found = true;
            break;
        }
    }

    if( !found )
    {
        m_acls.push_back( std::make_shared<acl_connection>( a_acl ) );
    }
}

void acl_connections_db::remove_connection( uint16_t a_handle )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    for( auto it = m_acls.begin(); it != m_acls.end(); )
    {
        auto& acl = *it;
        if( acl->m_connection_handle == a_handle )
        {
            it = m_acls.erase( it );
        }
        else
        {
            ++it;
        }
    }
}

void acl_connections_db::update_acl
    (
    bluetooth_address a_remote_device,
    uint16_t a_handle,
    acl_type a_type,
    bool a_encypted
    )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    bool found = false;
    std::shared_ptr<acl_connection> acl;
    for( auto it = m_acls.begin(); it != m_acls.end(); ++it )
    {
        acl = *it;
        if( acl->m_remote_device == a_remote_device &&
            acl->m_acl_type == a_type )
        {
            found = true;
            break;
        }
    }

    if( found )
    {
        acl->m_connection_handle = a_handle;
        acl->m_encrypted = a_encypted;
    }
    else
    {
        acl_connection acl;
        acl.m_acl_type = a_type;
        acl.m_connection_handle = a_handle;
        acl.m_encrypted = a_encypted;
        acl.m_local_inited = false;
        acl.m_remote_device = a_remote_device;
        m_acls.push_back( std::make_shared<acl_connection>( acl ) );
    }
}

void acl_connections_db::update_acl
    (
    uint16_t a_handle,
    uint16_t a_timeout
    )
{
    bool found = false;
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    for( auto & ele : m_acls )
    {
        if( ele->m_connection_handle == a_handle )
        {
            ele->m_supervision_timeout = a_timeout;
            found = true;
            return;
        }
    }

    if( !found )
    {
        LogUtilError() << "No acl connection for handle: " << a_handle;
    }
}

}

