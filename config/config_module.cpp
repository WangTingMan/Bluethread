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

#include "config_module.h"
#include "config.h"
#include "bluetooth_address.h"

#include "../common/bluetooth_common_event.h"
#include "common/trusted_devices.h"
#include "../common/remote_device.h"

#include "framework/executable_task.h"
#include "framework/framework_manager.h"
#include "framework/internal/platform.h"
#include "framework/framework_event.h"
#include "framework/log_util.h"
#include "framework/utils.h"

#include <functional>

#define PAIRING_DEVICE_RECORD_FILE_NAME "E:/VCLAB/BluetoothStack/x64/Debug/paired_device.conf"
#define PAIRED_DEVICE_KEY "PairedDevice"
#define DEVICE_NAME_KEY "DeviceName"
#define LINK_KEY "LinkKey"

namespace bluetooth
{

using namespace framework;

uint16_t config_module::config_module_task::s_config_module_task_type_id = 0u;

config_module::config_module()
{
    set_name( s_config_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
}

void config_module::initialize()
{
    config_module_task::s_config_module_task_type_id = framework::framework_manager::get_instance()
        .register_task_type( uint16_t( 1 ) );

    std::shared_ptr<executable_task> tsk;
    tsk = std::make_shared<executable_task>();
    tsk->set_fun( std::bind( &config_module::load_pairing_from_file, this ), get_name() );

    tsk->set_source_module( get_name() );
    tsk->set_target_module( get_name() );

    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void config_module::deinitialize()
{

}

void config_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    if( a_task->get_task_type() != static_cast<framework::task_type>( config_module_task::s_config_module_task_type_id ) )
    {
        return;
    }

    std::shared_ptr<config_module_task> detail_task = std::static_pointer_cast< config_module_task >( a_task );
    switch( detail_task->m_config_task_type )
    {
    case config_task_type::write_trusted_device:
        write_pairing_to_file();
        break;
    default:
        break;
    }
}

void config_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    switch( a_event->m_event_type )
    {
    case event_type::power_on:
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case event_type::power_off:
        set_power_status( abstract_module::powering_status::power_off );
        break;
    case event_type::power_status_changed:
        break;
    case event_type::derived_type:
        if( bluetooth_common_event::s_bluetooth_common_event_type == a_event->m_derived_type )
        {
            handle_bluetoot_event( std::static_pointer_cast<bluetooth_common_event>(a_event) );
        }
        break;
    default:
        break;
    }
}

void config_module::handle_bluetoot_event( std::shared_ptr<bluetooth_common_event> const& a_bt_event )
{
    std::shared_ptr<executable_task> tsk;
    switch( a_bt_event->m_bluetooth_event_type )
    {
    case bluetooth_event_type::pairing_completed:
        LogUtilInfo() << "Device: " << a_bt_event->m_remote_device.to_string() << " pairing completed";
        tsk = std::make_shared<executable_task>();
        tsk->set_fun( std::bind( &config_module::write_pairing_to_file, this ), get_name() );
        break;
    default:
        break;
    }

    if( tsk )
    {
        tsk->set_source_module( get_name() );
        tsk->set_target_module( get_name() );

        framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
    }
}

void config_module::write_pairing_to_file()
{
    config_t config;
    auto trust_devices_ = framework_manager::get_instance().get_info_manager()
        .get_detail_information<trusted_devices>( trusted_devices::s_trusted_devices_name );
    if( !trust_devices_ )
    {
        LogUtilError() << "Schedule error, we need create it first in initialization procedure";
        return;
    }

    std::vector<remote_device> paired_devs;
    paired_devs = trust_devices_->get_paired_devices();
    std::string str;

    for( auto it = paired_devs.begin(); it != paired_devs.end(); ++it )
    {
        section_t section;
        section.name.assign( PAIRED_DEVICE_KEY );
        section.name.push_back( ' ' );
        auto [has_address, address] = it->get_address();
        if( !has_address )
        {
            continue;
        }

        base64_encode( reinterpret_cast<const char*>( address.address ), bluetooth_address::s_bluetooth_address_size, &str );
        section.name.append( str );

        entry_t entry;
        entry.key = DEVICE_NAME_KEY;
        entry.value = convert( it->get_name() );
        section.entries.push_back( entry );

        entry.key = LINK_KEY;
        auto [has_link_key,link_key] = it->get_link_key();
        if( has_link_key )
        {
            base64_encode( reinterpret_cast<const char*>( link_key.data() ), link_key.size(), &str );
            entry.value = str;
            section.entries.push_back( entry );
        }

        config.sections.push_back( section );
    }

    config_save( config, PAIRING_DEVICE_RECORD_FILE_NAME );
}

void config_module::load_pairing_from_file()
{
    std::unique_ptr<config_t> config = config_new( PAIRING_DEVICE_RECORD_FILE_NAME );
    if( !config )
    {
        return;
    }

    std::vector<std::string> splited_string;
    std::vector<char> buffer;
    bluetooth_address address;
    std::u8string device_name;
    std::vector<uint8_t> link_key;
    bool ret = false;
    auto trust_devices_ = framework_manager::get_instance().get_info_manager()
        .get_detail_information<trusted_devices>( trusted_devices::s_trusted_devices_name );
    if( !trust_devices_ )
    {
        LogUtilError() << "Schedule error, we need create it first in initialization procedure";
        return;
    }

    std::list<section_t>& sections = config->sections;
    for( auto it = sections.begin(); it != sections.end(); ++it )
    {

        splited_string = framework::split_string( it->name, ' ' );
        if( splited_string.size() != 2 )
        {
            continue;
        }

        if( splited_string[0] == PAIRED_DEVICE_KEY )
        {
            ret = base64_decode( splited_string[1], buffer );
            if( !ret )
            {
                continue;
            }
        }
        else
        {
            continue;
        }

        if( buffer.size() != bluetooth_address::s_bluetooth_address_size )
        {
            continue;
        }

        memcpy( address.address, buffer.data(), bluetooth_address::s_bluetooth_address_size );

        for( auto itr = it->entries.begin(); itr != it->entries.end(); ++itr )
        {
            if( itr->key == DEVICE_NAME_KEY )
            {
                device_name = convert( trim_string( itr->value ) );
            }
            else if( itr->key == LINK_KEY )
            {
                std::vector<char> link_;
                ret = base64_decode( trim_string( itr->value ), link_ );
                if( !ret || link_.size() != remote_device::s_link_key_size )
                {
                    break;
                }

                const uint8_t* p_link = reinterpret_cast<const uint8_t*>( link_.data() );
                link_key.assign( p_link, p_link + link_.size() );
            }
        }

        if( ret )
        {
            trust_devices_->update( address, link_key, device_name );
        }
    }
}

}

