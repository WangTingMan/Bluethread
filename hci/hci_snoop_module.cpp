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

#include "hci_snoop_module.h"
#include "../include/endian_convert.h" 
#include "framework/log_util.h"
#include "framework/internal/platform.h"
#include "framework/framework_event.h"
#include "framework/framework_manager.h"

#ifndef HCI_LOG_FOLDER
#define HCI_LOG_FOLDER "E:/VCLAB/BluetoothStack/x64/Debug/"
#endif

#ifndef HCI_LOG_FILE_NAME
#define HCI_LOG_FILE_NAME "bt_stack_hci"
#endif

#ifndef HCI_MAX_PACKETS_IN_SINGLE_FILE
#define HCI_MAX_PACKETS_IN_SINGLE_FILE 20000
#endif

#ifndef HCI_MAX_LOG_FILE
#define HCI_MAX_LOG_FILE 5
#endif

#ifdef _MSC_VER
#pragma pack(1)
#endif
typedef struct
{
    uint32_t length_original;
    uint32_t length_captured;
    uint32_t flags;
    uint32_t dropped_packets;
    uint64_t timestamp;
    uint8_t type;
}
#if defined(__GNUC__) || defined(__GNUG__)
__attribute__( ( __packed__ ) ) 
#endif
btsnoop_header_t;
#ifdef _MSC_VER
#pragma pack()
#endif

namespace bluetooth
{

using namespace framework;

uint16_t hci_snoop_module::hci_snoop_write_task::s_hci_snoop_write_task_type = 0u;

hci_snoop_module::hci_snoop_module()
{
    set_name( s_hci_snoop_module_name );
    set_module_type( abstract_module::module_type::execute_task_when_post );
}

void hci_snoop_module::initialize()
{
    hci_snoop_write_task::s_hci_snoop_write_task_type = framework::framework_manager::get_instance()
        .register_task_type( uint16_t( 1 ) );
    set_power_status( abstract_module::powering_status::power_on );
}

/**
 * deinitialize this module self
 */
void hci_snoop_module::deinitialize()
{
    std::lock_guard<std::recursive_mutex> locker( m_mutex );
    if( m_logfile.is_open() )
    {
        m_logfile.flush();
        m_logfile.close();
    }
    m_wrote_packets = 0;
    set_power_status( abstract_module::powering_status::power_off );
}

void hci_snoop_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    auto type = a_task->get_task_type();
    if( static_cast<uint16_t>( type ) != hci_snoop_write_task::s_hci_snoop_write_task_type )
    {
        return;
    }

    std::shared_ptr<hci_snoop_write_task> _hci_data;
    _hci_data = std::static_pointer_cast< hci_snoop_write_task >( a_task );
    if( _hci_data )
    {
        handle_write_hci_log_task( _hci_data->m_hci_data );
    }
    else
    {
        LogUtilDebug() << "ignore task, from " << a_task->get_source_module();
    }
}

void hci_snoop_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    std::shared_ptr<hci_snoop_write_task> task;
    auto& event_type = a_event->m_event_type;
    switch( event_type )
    {
    case event_type::power_on:
        set_power_status( abstract_module::powering_status::power_on );
        break;
    case event_type::power_off:
        deinitialize();
        break;
    case event_type::power_status_changed:
        break;
    case event_type::derived_type:
        break;
    default:
        LogUtilError() << "event ignored: " << static_cast< uint16_t >( event_type );
        break;
    }
}

void hci_snoop_module::handle_write_hci_log_task( std::shared_ptr<hci_data> a_hci_data )
{
    uint8_t* p_buffer = a_hci_data->m_buffer.data();

    uint32_t length_he = 0;
    uint32_t flags = 0;
    switch( a_hci_data->m_type )
    {
    case bluetooth::uart_hci_type::command_type:
        length_he = p_buffer[2] + 4;
        flags = 2;
        break;
    case bluetooth::uart_hci_type::acl_type:
        length_he = ( p_buffer[3] << 8 ) + p_buffer[2] + 5;
        flags = a_hci_data->m_from_controller;
        break;
    case bluetooth::uart_hci_type::sco_type:
        length_he = p_buffer[2] + 4;
        flags = a_hci_data->m_from_controller;
        break;
    case bluetooth::uart_hci_type::event_type:
        length_he = p_buffer[1] + 3;
        flags = 3;
        break;
    case bluetooth::uart_hci_type::iso_data_type:
        break;
    default:
        LogUtilError() << "Error hci type. Ingore this hci packet.";
        return;
    }

    // Epoch in microseconds since 01/01/0000.
    constexpr uint64_t BTSNOOP_EPOCH_DELTA = 0x00dcddb30f2f8000ULL;

    btsnoop_header_t header;
    write_be32( reinterpret_cast<uint8_t*>( &header.length_original ), length_he );
    header.length_captured = header.length_original;
    write_be32( reinterpret_cast< uint8_t* >( &header.flags ), flags );
    header.dropped_packets = 0;
    header.type = static_cast< uint8_t >( a_hci_data->m_type );
    uint64_t time_stamp = get_time_stamp();
    time_stamp += std::chrono::duration_cast< std::chrono::microseconds >(
        std::chrono::hours( 8 ) ).count();
    time_stamp += BTSNOOP_EPOCH_DELTA;
    write_be64( reinterpret_cast< uint8_t* >( &header.timestamp ), time_stamp );

    std::lock_guard<std::recursive_mutex> locker( m_mutex );
    if( !m_logfile.is_open() )
    {
        open_new_file();
    }
    m_logfile.write( reinterpret_cast< char* >( &header ), 24 );
    m_logfile.write( reinterpret_cast< char* >( &header.type ), 1 );
    m_logfile.write( reinterpret_cast< char* >( p_buffer ), length_he - 1 );
    m_logfile.flush();
}

void hci_snoop_module::open_new_file()
{
    std::lock_guard<std::recursive_mutex> locker( m_mutex );
    if( m_logfile.is_open() )
    {
        m_logfile.flush();
        m_logfile.close();
        m_wrote_packets = 0;
    }

    rotate_log( HCI_LOG_FOLDER, HCI_LOG_FILE_NAME, HCI_MAX_LOG_FILE );

    m_logfile.open( HCI_LOG_FOLDER HCI_LOG_FILE_NAME ".log",
        std::ios::binary | std::ios::out | std::ios::trunc );
    m_logfile.write( "btsnoop\0\0\0\0\1\0\0\x3\xea", 16 );
}

}

