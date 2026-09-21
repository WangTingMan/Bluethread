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

#include "trusted_devices.h"
#include "stack\stack_manager.h"

#include "framework\framework_manager.h"
#include "framework\executable_task.h"
#include "framework\log_util.h"

#include <memory>
#include <vector>

namespace bluetooth
{

trusted_devices::trusted_devices()
{
    set_name( s_trusted_devices_name );
}

void trusted_devices::add_trusted_device( remote_device a_device )
{
    auto [has_addr_in, address_in] = a_device.get_address();
    if( !has_addr_in )
    {
        LogUtilError() << "No device address, no record";
        return;
    }

    std::lock_guard<std::shared_mutex> locker( m_mutex );

    auto it = m_paired_devices.begin();
    for( ; it != m_paired_devices.end(); )
    {
        auto [has_address, address] = it->get_address();
        bool found = false;
        if( has_address && has_addr_in )
        {
            if( address == address_in )
            {
                found = true;
            }
        }

        if( found )
        {
            it = m_paired_devices.erase( it );
        }
        else
        {
            ++it;
        }
    }
    m_paired_devices.push_back( a_device );
}

bool trusted_devices::get_link_key( bluetooth_address const& a_remote_device, std::vector<uint8_t>& a_link_key )
{
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    auto it = m_paired_devices.begin();
    for( ; it != m_paired_devices.end(); ++it )
    {
        auto [has_address, address] = it->get_address();
        if( address == a_remote_device )
        {
            auto [has_link_key, link_key] = it->get_link_key();
            a_link_key = link_key;
            return has_link_key;
        }
    }
    return false;
}

void trusted_devices::remove_trusted( bluetooth_address const& a_remote_device )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    bool removed = false;

    auto it = m_paired_devices.begin();
    for( ; it != m_paired_devices.end(); )
    {
        auto [has_address, address] = it->get_address();
        bool matched = false;
        if( has_address )
        {
            if( address == a_remote_device )
            {
                matched = true;
            }
        }

        if( matched )
        {
            it = m_paired_devices.erase( it );
            removed = true;
        }
        else
        {
            ++it;
        }
    }


}

void trusted_devices::update
    (
    bluetooth_address const& a_remote_device,
    std::vector<uint8_t> a_link_key,
    std::u8string a_name
    )
{
    if( remote_device::s_link_key_size != a_link_key.size() )
    {
        LogUtilError() << "Link size wrong. Ignore";
        return;
    }

    std::lock_guard<std::shared_mutex> locker( m_mutex );
    bool matched = false;
    for( auto it = m_paired_devices.begin(); it != m_paired_devices.end(); ++it )
    {
        auto [has_address, address] = it->get_address();
        if( has_address )
        {
            if( address == a_remote_device )
            {
                matched = true;
                it->set_link_key( a_link_key.data(), remote_device::s_link_key_size );
                if( it->get_name() != a_name )
                {
                    it->set_name( a_name );
                    remote_device& device = *it;
                    notify_paired_device( &device );
                }
            }
        }
    }

    if( !matched )
    {
        remote_device device;
        device.set_address( a_remote_device );
        device.set_name( a_name );
        device.set_link_key( a_link_key.data(), remote_device::s_link_key_size );
        notify_paired_device( &device );
        m_paired_devices.push_back( device );
    }
}

void trusted_devices::update
    (
    bluetooth_address const& a_remote_device,
    std::u8string a_name
    )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    for( auto it = m_paired_devices.begin(); it != m_paired_devices.end(); ++it )
    {
        auto [has_address, address] = it->get_address();
        if( has_address )
        {
            if( address == a_remote_device )
            {
                if( it->get_name() != a_name )
                {
                    it->set_name( a_name );
                    remote_device& device = *it;
                    notify_paired_device( &device );
                }
                return;
            }
        }
    }
}

std::vector<remote_device> trusted_devices::get_paired_devices()
{
    std::shared_lock<std::shared_mutex> locker( m_mutex );
    return m_paired_devices;
}

void trusted_devices::notify_paired_device( remote_device const* a_paired_device )
{
    std::function<void()> fun;
    std::shared_ptr<remote_device> device = std::make_shared<remote_device>();
    *device = *a_paired_device;
    fun = [device]()mutable
    {
        remote_device_info info;
        remote_device_attribute attribute;
        std::vector<remote_device_attribute> attris;

        std::u8string name = device->get_name();
        attribute.buffer = const_cast<void*>( reinterpret_cast<void const*>( name.c_str() ) );
        attribute.type = remote_device_attribute_type::completed_device_name;
        attribute.size = name.size();
        attris.push_back( attribute );

        auto [has_address, address] = device->get_address();
        if( !has_address )
        {
            return;
        }
        attribute.buffer = const_cast< void* >( reinterpret_cast< void const* >( address.address ) );
        attribute.type = remote_device_attribute_type::device_address;
        attribute.size = bluetooth_address::s_bluetooth_address_size;
        attris.push_back( attribute );

        info.attributes = attris.data();
        info.attribute_count = attris.size();

        stack_manager::get_instance().get_stack_callback()( service_type::not_specified,
            function_type::paired_device_info, reinterpret_cast< void* >( &info ) );
    };

    std::shared_ptr<framework::executable_task> task = std::make_shared<framework::executable_task>();
    task->set_fun( fun, framework::abstract_module::s_general_seq_task_runner_module );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

}

