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

#include "device_manager.h"
#include "stack/stack_manager.h"

#include "framework\executable_task.h"
#include "framework\framework_manager.h"
#include "framework\abstract_module.h"
#include "framework\log_util.h"


namespace
{

static constexpr uint8_t s_max_buffered_found_devices = 20;

}

namespace bluetooth
{

device_manager::device_manager()
{
    set_name( s_device_manager_name );
}

void device_manager::new_device_found
    (
    bluetooth_address const& a_address,
    uint8_t a_page_scan_repetition_mode,
    class_of_device a_cod,
    uint16_t a_clock_offset,
    int8_t a_rssi
    )
{
    std::unique_lock<std::shared_mutex> locker( m_mutex );

    auto it = m_discovered_devices.begin();
    for( ; it != m_discovered_devices.end(); ++it )
    {
        remote_device const& device = ( *it )->m_device;
        auto [has_address, address] = device.get_address();
        if( has_address && address == a_address )
        {
            break;;
        }
    }

    if( it == m_discovered_devices.end() )
    {
        std::shared_ptr<device_found> new_device;
        new_device = std::make_shared<device_found>();
        new_device->m_found_time = std::chrono::steady_clock::now();

        new_device->m_device.set_address( a_address );
        new_device->m_device.set_clock_offset( a_clock_offset );
        new_device->m_device.set_cod( a_cod );
        new_device->m_device.set_rssi( a_rssi );
        new_device->m_device.set_page_scan_repetition_mode( a_page_scan_repetition_mode );
        m_discovered_devices.push_back( new_device );
        return;
    }

    remote_device& device = ( *it )->m_device;
    device.set_address( a_address );
    device.set_clock_offset( a_clock_offset );
    device.set_cod( a_cod );
    device.set_rssi( a_rssi );
    device.set_page_scan_repetition_mode( a_page_scan_repetition_mode );

    locker.unlock();
    delete_oldest_found_device();
}

bool device_manager::update_davice_name
    (
    bluetooth_address const& a_address,
    std::u8string a_name
    )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    auto it = m_discovered_devices.begin();
    bool found = false;
    for( ; it != m_discovered_devices.end(); ++it )
    {
        remote_device& device = ( *it )->m_device;
        auto [has_address, address] = device.get_address();
        if( has_address && address == a_address )
        {
            if( a_name != device.get_name() )
            {
                device.set_name( std::move( a_name ) );
                found = true;
                return true;
            }
        }
    }

    if( !found )
    {
        std::shared_ptr<device_found> new_device;
        new_device = std::make_shared<device_found>();
        new_device->m_found_time = std::chrono::steady_clock::now();
        new_device->m_device.set_address( a_address );
        new_device->m_device.set_name( a_name );
        m_discovered_devices.push_back( new_device );
    }

    return false;
}

std::tuple<bool, std::u8string> device_manager::get_device_name( bluetooth_address const& a_address )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    auto it = m_discovered_devices.begin();
    bool found = false;
    std::u8string name;
    for( ; it != m_discovered_devices.end(); ++it )
    {
        remote_device& device = ( *it )->m_device;
        auto [has_address, address] = device.get_address();
        if( has_address && address == a_address )
        {
            name = device.get_name();
            found = true;
        }
    }

    return { found,std::move( name ) };
}

void device_manager::update_eir_uuid
    (
    bluetooth_address const& a_address,
    std::vector<uuid> a_uuids
    )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    auto it = m_discovered_devices.begin();
    for( ; it != m_discovered_devices.end(); ++it )
    {
        remote_device& device = ( *it )->m_device;
        auto [has_address, address] = device.get_address();
        if( has_address && address == a_address )
        {
            device.set_uuids( a_uuids );
            return;
        }
    }
}

void device_manager::notify_device_found( bluetooth_address const& a_address )
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    auto it = m_discovered_devices.begin();
    std::shared_ptr<device_found> device_notify;
    for( ; it != m_discovered_devices.end(); ++it )
    {
        remote_device& _device = ( *it )->m_device;
        auto [has_address, address] = _device.get_address();
        if( has_address && address == a_address )
        {
            device_notify = *it;
            break;
        }
    }

    if( !device_notify )
    {
        return;
    }

    auto fun = [device = device_notify->m_device]()
    {
        std::vector<remote_device_attribute> attributes;
        remote_device_attribute attribute;

        auto [has_address, address] = device.get_address();
        if( has_address )
        {
            attribute.type = remote_device_attribute_type::device_address;
            attribute.buffer = address.address;
            attribute.size = bluetooth_address::s_bluetooth_address_size;
            attributes.push_back( attribute );
        }
        else
        {
            LogUtilInfo() << "no address, no report.";
            return;
        }

        auto name = device.get_name();
        if( !name.empty() )
        {
            attribute.type = remote_device_attribute_type::completed_device_name;
            attribute.buffer = name.data();
            attribute.size = static_cast<uint16_t>( name.size() );
            attributes.push_back( attribute );
        }

        auto [has_cod, cod] = device.get_cod();
        if( has_cod )
        {
            attribute.type = remote_device_attribute_type::device_cod;
            attribute.buffer = &cod;
            attribute.size = class_of_device::s_class_of_device_size;
            attributes.push_back( attribute );
        }

        auto [has_rssi, rssi] = device.get_rssi();
        if( has_rssi )
        {
            attribute.type = remote_device_attribute_type::inquiry_rssi;
            attribute.buffer = &rssi;
            attribute.size = sizeof( rssi );
            attributes.push_back( attribute );
        }

        remote_device_info info;
        info.attributes = attributes.data();
        info.attribute_count = static_cast<uint16_t>( attributes.size() );
        stack_manager::get_instance().get_stack_callback()( service_type::not_specified,
            function_type::remote_device_found, reinterpret_cast< void* >( &info ) );
    };

    std::shared_ptr<framework::executable_task> task = std::make_shared<framework::executable_task>();
    task->set_fun( fun, framework::abstract_module::s_general_seq_task_runner_module );
    framework::framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void device_manager::delete_oldest_found_device()
{
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    if( m_discovered_devices.size() < s_max_buffered_found_devices )
    {
        return;
    }

    std::sort( m_discovered_devices.begin(), m_discovered_devices.end(),
        []( std::shared_ptr<device_found> const& left, std::shared_ptr<device_found> const& right )
        {
            return left->m_found_time < right->m_found_time;
        } );

    auto it = m_discovered_devices.begin();
    std::advance( it, m_discovered_devices.size() - s_max_buffered_found_devices );
    m_discovered_devices.erase( m_discovered_devices.begin(), it );
}

std::shared_ptr<remote_device> device_manager::take_found_device( bluetooth_address const& a_address )
{
    std::shared_ptr<remote_device> device_ret;
    std::lock_guard<std::shared_mutex> locker( m_mutex );
    for( auto it = m_discovered_devices.begin(); it != m_discovered_devices.end(); )
    {
        std::shared_ptr<device_found> device;
        device = *it;
        if( !device )
        {
            it = m_discovered_devices.erase( it );
            continue;
        }

        remote_device& remote = device->m_device;
        auto [found, address] = remote.get_address();
        bool matched = false;
        if( found )
        {
            if( address == a_address )
            {
                matched = true;
            }
        }

        if( matched )
        {
            device_ret = std::make_shared<remote_device>( remote );
            it = m_discovered_devices.erase( it );
            continue;
        }
        else
        {
            ++it;
        }
    }
    return device_ret;
}

}

