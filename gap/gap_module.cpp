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

#include "gap_module.h"
#include "..\hci\hci_command_maker.h"
#include "..\hci\hci_module.h"
#include "..\hci\controller_module.h"
#include "..\utils\data_type_parser.h"
#include "endian_convert.h"
#include "bluetooth_address.h"
#include "class_of_device.h"
#include "..\common\controller.h"
#include "..\common\device_manager.h"
#include "..\common\bluetooth_common_event.h"
#include "..\common\trusted_devices.h"
#include "stack\stack_manager.h"

#include "framework/log_util.h"
#include "framework\framework_manager.h"
#include "framework\thread_manager.h"
#include "framework\framework_event.h"
#include "framework\executable_task.h"

#include <future>

static constexpr bool s_enable_ssp_if_can = true;

namespace bluetooth
{

using namespace framework;

uint16_t gap_module::gap_module_task::s_gap_module_task_type_id = 0u;

gap_module::gap_module()
{
    set_name( s_gap_module_name );
    set_module_type( abstract_module::module_type::sequence_executing );
    m_pairing_manager.set_attach_module( s_gap_module_name );
}

void gap_module::initialize()
{

    gap_module_task::s_gap_module_task_type_id = framework::framework_manager::get_instance()
        .register_task_type( uint16_t( 1 ) );

    set_power_status( abstract_module::powering_status::power_off );

    auto fun = std::bind( &gap_module::relay_hci_event, this, std::placeholders::_1 );
    auto mod = framework_manager::get_instance().get_module_manager().get_module( hci_module::s_hci_module_name );
    auto hci_mod = static_pointer_cast< hci_module >( mod );
    hci_mod->register_event_handler( hci_event_type::hci_extended_inquiry_result, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_inquiry_complete, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_inquiry_result, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_inquiry_result_with_rssi, source_here, fun );
    hci_mod->register_event_handler( hci_event_type::hci_remote_name_request_complete, source_here, fun );

    m_pairing_manager.initialize();

    auto dev_manager = framework_manager::get_instance().get_info_manager()
        .get_detail_information<device_manager>( device_manager::s_device_manager_name );
    if( !dev_manager )
    {
        dev_manager = std::make_shared<device_manager>();
        framework_manager::get_instance().get_info_manager().register_information( dev_manager );
    }

    auto trust_devices_ = framework_manager::get_instance().get_info_manager()
        .get_detail_information<trusted_devices>( trusted_devices::s_trusted_devices_name );
    if( !trust_devices_ )
    {
        trust_devices_ = std::make_shared<trusted_devices>();
        framework_manager::get_instance().get_info_manager().register_information( trust_devices_ );
    }
}

void gap_module::deinitialize()
{
}

void gap_module::handle_task( std::shared_ptr<abstract_task> a_task )
{
    if (a_task->get_task_type() != static_cast<framework::task_type>(gap_module_task::s_gap_module_task_type_id))
    {
        return;
    }

    std::shared_ptr<gap_module_task> task;
    task = std::static_pointer_cast< gap_module_task >( a_task );
    if( task )
    {
        switch( task->m_gap_task_type)
        {
        case gap_task_type::power_on:
            handle_power_on( task );
            break;
        case gap_task_type::power_off:
            handle_power_off( task );
            break;
        case gap_task_type::hci_cmd_response:
            break;
        case gap_task_type::edr_inquiry:
            handle_search_task( task );
            break;
        case gap_task_type::hci_event:
            handle_hci_event( task->m_hci_response );
            break;
        case gap_task_type::to_connectable:
            set_connectable(true, true );
            break;
        case gap_task_type::to_disconnectable:
            set_connectable( false, true  );
            break;
        case gap_task_type::to_discoverable:
            set_discoverable( true, true );
            break;
        case gap_task_type::to_nondiscoverable:
            set_discoverable( false, true );
            break;
        case gap_task_type::visibility_setting:
            set_visiblility( task->m_discoverable, task->m_connectable );
            break;
        case gap_task_type::change_local_name:
            changed_local_name( task->m_localname );
            break;
        case gap_task_type::cancel_edr_inquiry:
            handle_cancel_search( task );
            break;
        case gap_task_type::to_pairable:
            m_pairing_manager.set_pairable();
            break;
        case gap_task_type::to_unpairable:
            m_pairing_manager.set_pairable( false );
            break;
        case gap_task_type::accept_ssp_confirm:
            m_pairing_manager.accept_ssp_confrim( task->m_address, true );
            break;
        case gap_task_type::reject_ssp_confirm:
            m_pairing_manager.accept_ssp_confrim( task->m_address, false );
            break;
        case gap_task_type::make_edr_acl_connection:
            make_edr_acl( task );
            break;
        default:
            LogUtilError() << "Ignored task type:" << static_cast< uint16_t >( task->m_gap_task_type);
            break;
        }
    }
}

void gap_module::handle_event( std::shared_ptr<framework_event> a_event )
{
    auto& name = a_event->get_target_module();
    auto& event_type = a_event->m_event_type;
    if( get_name() == name )
    {
        // here not sequence scheduled so we need pay attention to it
        std::shared_ptr<gap_module_task> task;
        std::shared_ptr<executable_task> exe_task;
        switch( event_type )
        {
        case event_type::power_on:
            task = std::make_shared<gap_module_task>();
            task->set_target_module( get_name() );
            task->set_source_module( get_name() );
            task->m_gap_task_type = gap_task_type::power_on;
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            break;
        case event_type::power_status_changed:
            exe_task = std::make_shared<executable_task>(
                std::bind( &gap_module::handle_other_module_pwr_changed, this, a_event->m_module_name ) );
            exe_task->set_target_module( get_name() );
            exe_task->set_source_module( get_name() );
            framework_manager::get_instance().get_thread_manager().post_task( exe_task, framework::source_here );
            break;
        case event_type::power_off:
            task = std::make_shared<gap_module_task>();
            task->set_target_module( get_name() );
            task->set_source_module( get_name() );
            task->m_gap_task_type = gap_task_type::power_off;
            framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
            break;
        case event_type::derived_type:
            if (bluetooth_common_event::s_bluetooth_common_event_type == a_event->m_derived_type)
            {
                std::shared_ptr<bluetooth_common_event> event_;
                event_ = std::static_pointer_cast<bluetooth_common_event>(a_event);
                handle_bluetooth_event( event_ );
            }
            break;
        default:
            LogUtilWarning() << "Event ignored.";
            break;
        }
    }
    else
    {
        LogUtilWarning() << "event ignored.";
    }
}

void gap_module::handle_power_on( std::shared_ptr<gap_module_task> a_task )
{
    std::shared_ptr<abstract_module> module_;
    module_ = framework_manager::get_instance().get_module_manager().get_module( controller_module::s_controller_module_name );
    if( !module_ )
    {
        LogUtilError() << "No module: " << controller_module::s_controller_module_name;
        return;
    }

    auto status = module_->get_power_status();
    if( status != powering_status::power_on )
    {
        LogUtilInfo() << "We need wait controller module powered on first.";
        return;
    }

    if( powering_status::power_on != get_power_status() )
    {
        /*
        * Some controller may not support hci_read_scan_enable but it does not declare in response of
        * HCI_Read_Local_Supported_Commands. Even hci_read_scan_enable command is a mandatory command.
        * Such as controller from: Actions (Zhuhai) Technology Co., Limited, it does not support command
        * hci_read_scan_enable. So we test if controller support hci_read_scan_enable or not here.
        */
        std::shared_ptr<hci_data> hci_send;
        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_send = make_no_params_cmd( hci_command::hci_read_scan_enable );
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = hci_send;
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( get_name() );
        hci_task->m_hci_data_handler = std::bind(
            &gap_module::handle_hci_event, this, std::placeholders::_1 );
        hci_task->m_handler_module_name = get_name();
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );

        bool is_enable_ssp = s_enable_ssp_if_can;
        if( is_enable_ssp )
        {
            auto con = framework_manager::get_instance().get_info_manager()
                .get_detail_information<controller>( controller::s_information_name );
            if( con->support_lmp_feature( local_features::feature_secure_simple_pairing ) )
            {
                hci_send = make_cmd( hci_command::hci_write_simple_pairing_mode,
                    static_cast< uint8_t >( 0x01 ) );
                hci_task = std::make_shared<hci_module::hci_module_task>();
                hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
                hci_task->m_hci_data = hci_send;
                hci_task->set_target_module( hci_module::s_hci_module_name );
                hci_task->set_source_module( get_name() );
                hci_task->m_hci_data_handler = std::bind(
                    &gap_module::handle_hci_event, this, std::placeholders::_1 );
                hci_task->m_handler_module_name = get_name();
                framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
            }
        }
        set_power_status( abstract_module::powering_status::power_on );
    }
    else
    {
        LogUtilInfo() << "Gap module already powered on";
    }
}

void gap_module::handle_power_off( std::shared_ptr<gap_module_task> a_task )
{
    LogUtilInfo() << "to power off.";
    m_searching = false;
    m_connectable = enable_status::unknown;
    m_discoverable = enable_status::unknown;

    set_power_status( abstract_module::powering_status::power_off );
}

bool gap_module::handle_other_module_pwr_changed( std::string a_name )
{
    if( a_name == controller_module::s_controller_module_name )
    {
        LogUtilInfo() << "module power status changed: " << a_name;
        std::shared_ptr<gap_module_task> task;
        task = std::make_shared<gap_module_task>();
        task->set_target_module( get_name() );
        task->set_source_module( get_name() );
        task->m_gap_task_type = gap_task_type::power_on;
        handle_power_on( task );
    }
    return false;
}

void gap_module::handle_search_task( std::shared_ptr<gap_module_task> a_task )
{
    if( m_searching )
    {
        // to do notify searching status
        LogUtilInfo() << "already sreaching devices, ignore this task.";
        return;
    }

    if( powering_status::power_on != get_power_status() )
    {
        // to do notify searching status
        LogUtilInfo() << "Not powered on. cannot searching.";
        return;
    }

    m_searching = true;
    notify_searching_changed();
    std::shared_ptr<hci_module::hci_module_task> hci_task;

    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( get_name() );
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    hci_task->m_hci_data = make_inquiry_event_filter();
    LogUtilInfo() << "Send event filter settings";
    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );

    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( get_name() );
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    hci_task->m_hci_data = make_inquiry(0x0F);
    LogUtilInfo() << "start searching.";
    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );

    auto dev_manager = framework_manager::get_instance().get_info_manager()
        .get_detail_information<device_manager>( device_manager::s_device_manager_name );

    dev_manager->delete_oldest_found_device();
}

void gap_module::handle_cancel_search( std::shared_ptr<gap_module_task> const& a_task )
{
    if( !m_searching )
    {
        LogUtilInfo() << "Already not searching devices.";
        notify_searching_changed();
        return;
    }

    std::shared_ptr<hci_module::hci_module_task> hci_task;
    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( get_name() );
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    hci_task->m_hci_data = make_no_params_cmd( hci_command::hci_inquiry_cancel );
    hci_task->m_hci_data_handler = std::bind( &gap_module::handle_hci_event, this, std::placeholders::_1 );
    hci_task->m_handler_module_name = get_name();
    LogUtilInfo() << "cancel searching.";
    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

void gap_module::handle_hci_event( std::shared_ptr<hci_data> const& a_hci_event )
{
    if( a_hci_event->m_type != uart_hci_type::event_type )
    {
        LogUtilError() << "not event hci packet!";
        return;
    }

    hci_event_type event_type = static_cast<hci_event_type>( a_hci_event->m_buffer[0] );
    switch( event_type )
    {
    case hci_event_type::hci_inquiry_complete:
        m_searching = false;
        notify_searching_changed();
        break;
    case hci_event_type::hci_command_complete:
        handle_cmd_completed_event( a_hci_event );
        break;
    case hci_event_type::hci_extended_inquiry_result:
        handle_eir_inquiry_result( a_hci_event );
        break;
    case hci_event_type::hci_remote_name_request_complete:
        handle_name_request_completed( a_hci_event );
        break;
    case hci_event_type::hci_command_status:
        handle_command_status( a_hci_event );
        break;
    default:
        LogUtilError() << "Not handled event: " << static_cast< uint8_t >( event_type );
    }
}

void gap_module::handle_bluetooth_event( std::shared_ptr<bluetooth_common_event> const& a_event )
{
    if( !a_event )
    {
        LogUtilError() << "Wrong event type.";
        return;
    }

    std::shared_ptr<hci_data> hci_send;
    switch( a_event->m_bluetooth_event_type )
    {
    case bluetooth_event_type::edr_acl_connected:
        // We already have a ACL connection so not care the clock offset.
        hci_send = make_read_remote_name( a_event->m_remote_device, 0x01, 0x00 );
        break;
    case bluetooth_event_type::edr_acl_disconnected:
        for( auto it = m_acl_cb.begin(); it != m_acl_cb.end(); )
        {
            if( it->m_remote_device == a_event->m_remote_device )
            {
                it->m_status = connection_status::disconnected;
                it = m_acl_cb.erase( it );
            }
            else
            {
                ++it;
            }
        }
        break;
    default:
        LogUtilWarning() << "Bluetooth event ignored: " << static_cast< uint32_t >( a_event->m_bluetooth_event_type );
        break;
    }

    if( hci_send )
    {
        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( get_name() );
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = hci_send;
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
    }
}

void gap_module::handle_cmd_completed_event( std::shared_ptr<hci_data> const& a_hci_event )
{
    hci_command cmd = static_cast< hci_command >( le_to_host16( a_hci_event->m_buffer.data() + 3 ) );
    std::shared_ptr<hci_data> hci_send;
    switch( cmd )
    {
    case bluetooth::hci_command::hci_read_scan_enable:
        {
            uint8_t status = a_hci_event->m_buffer[5];
            if( 0x00 == status )
            {
                uint8_t ret = a_hci_event->m_buffer[6];
                switch( ret )
                {
                case 0x00:
                    m_connectable = enable_status::disabled;
                    m_discoverable = enable_status::disabled;
                    break;
                case 0x01:
                    m_connectable = enable_status::disabled;
                    m_discoverable = enable_status::enabled;
                    break;
                case 0x02:
                    m_connectable = enable_status::enabled;
                    m_discoverable = enable_status::disabled;
                    break;
                case 0x03:
                    m_connectable = enable_status::enabled;
                    m_discoverable = enable_status::enabled;
                    break;
                default:
                    LogUtilError() << "Unknown return value";
                    break;
                }
            }
            notify_visiblity_changed();
        }
        break;
    case bluetooth::hci_command::hci_write_scan_enable:
        {
            auto con = framework_manager::get_instance().get_info_manager()
                .get_detail_information<controller>( controller::s_information_name );
            if( con->controller_support_cmd( hci_command::hci_read_scan_enable ) )
            {
                hci_send = make_no_params_cmd( hci_command::hci_read_scan_enable );
            }
            else
            {
                uint8_t status = a_hci_event->m_buffer[5];
                if( 0x00 == status )
                {
                    if( m_connectable == enable_status::disabling )
                    {
                        m_connectable = enable_status::disabled;
                    }

                    if( m_connectable == enable_status::enabling )
                    {
                        m_connectable = enable_status::enabled;
                    }

                    if( m_discoverable == enable_status::disabling )
                    {
                        m_discoverable = enable_status::disabled;
                    }

                    if( m_discoverable == enable_status::enabling )
                    {
                        m_discoverable = enable_status::enabled;
                    }

                    notify_visiblity_changed();
                }
            }
        }
        break;
    case hci_command::hci_inquiry_cancel:
    {
        uint8_t status = a_hci_event->m_buffer[5];
        if( 0x00 != status )
        {
            LogUtilError() << "Error when cancel inquiry.";
        }
        m_searching = false;
        notify_searching_changed();
    }
        break;
    case hci_command::hci_write_simple_pairing_mode:
    {
        uint8_t status = a_hci_event->m_buffer[5];
        LogUtilInfo() << "Enable ssp: " << ( 0x00 == status ? "success" : "failed" );
    }
        break;
    case hci_command::hci_write_local_name:
    {
        uint8_t status = a_hci_event->m_buffer[5];
        LogUtilInfo() << "Write local name: " << ( 0x00 == status ? "success" : "failed" );
    }
        break;
    default:
        LogUtilError() << "Not handled cmd: " << static_cast< uint16_t >( cmd );
        break;
    }

    if( hci_send )
    {
        std::shared_ptr<hci_module::hci_module_task> hci_task;
        hci_task = std::make_shared<hci_module::hci_module_task>();
        hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
        hci_task->m_hci_data = hci_send;
        hci_task->m_hci_data_handler = std::bind(
            &gap_module::handle_hci_event, this, std::placeholders::_1 );
        hci_task->m_handler_module_name = get_name();
        hci_task->set_target_module( hci_module::s_hci_module_name );
        hci_task->set_source_module( get_name() );
        framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
    }
}

void gap_module::handle_eir_inquiry_result( std::shared_ptr<hci_data> const& a_hci_event )
{
    uint8_t total_size = a_hci_event->m_buffer[1];
    uint8_t num_res = a_hci_event->m_buffer[2];
    if( num_res != 1 )
    {
        LogUtilError() << "EIR response contains more than one result. Ignore such event.";
        return;
    }

    if( !m_searching )
    {
        LogUtilInfo() << "Ignore this event since not searching.";
        return;
    }
    bluetooth_address address;
    memcpy( address.address, a_hci_event->m_buffer.data() + 3, bluetooth_address::s_bluetooth_address_size );

    uint8_t page_scan_repetition_mode = a_hci_event->m_buffer[9];
    class_of_device cod;
    memcpy( cod.cod, a_hci_event->m_buffer.data() + 11, class_of_device::s_class_of_device_size );

    uint16_t clock_offset = le_to_host16( a_hci_event->m_buffer.data() + 14 );
    int8_t rssi = a_hci_event->m_buffer[16];

    auto dev_manager = framework_manager::get_instance().get_info_manager()
        .get_detail_information<device_manager>( device_manager::s_device_manager_name );

    dev_manager->new_device_found( address, page_scan_repetition_mode, cod, clock_offset, rssi );

    uint8_t* p_eir = a_hci_event->m_buffer.data() + 17;

    auto eir_elements = parse_eir( p_eir, total_size - 17 );
    std::vector<uuid> uuids;
    for( auto& ele : eir_elements )
    {
        switch( ele.type )
        {
        case data_type::completed_local_name:
        {
            std::u8string name( reinterpret_cast< char8_t* >( ele.buffer ), ele.size );
            name.push_back( 0x00 );
            dev_manager->update_davice_name( address, name );
        }
            break;
        case data_type::completed_16bit_uuid_list:
        {
            for( int i = 0; i < ele.size; i += 2 )
            {
                uint16_t uuid16 = le_to_host16( ele.buffer + i );
                uuid uuid_ = uuid::from_16bit( uuid16 );
                uuids.push_back( uuid_ );
            }
        }
            break;
        case data_type::completed_128bit_uuid_list:
        {
            for( int i = 0; i < ele.size; i += uuid::s_128bituuid_size )
            {
                uuid uuid_ = uuid::from_128bit_le( ele.buffer + i );
                if( !uuid_.empty() )
                {
                    uuids.push_back( uuid_ );
                }
            }
        }
            break;
        case data_type::completed_32bit_uuid_list:
            if( ele.size > 0 )
            {
                LogUtilWarning() << "Completed 32bit uuid ignored.";
            }
            break;
        case data_type::tx_power_level:
            break;
        default:
            LogUtilError() << "EIR type ignored: " << static_cast< uint16_t >( ele.type );
            break;
        }
    }
    dev_manager->update_eir_uuid( address, uuids );
    dev_manager->notify_device_found( address );
}

void gap_module::handle_name_request_completed( std::shared_ptr<hci_data> const& a_hci_event )
{
    std::vector<uint8_t> const& hci_data = a_hci_event->m_buffer;

    uint8_t total_size = hci_data[1];
    if( total_size < 250 )
    {
        LogUtilInfo() << "Event size too small. ignore this event";
        return;
    }

    uint8_t status = hci_data[2];
    if( status != 0x00 )
    {
        return;
    }

    bluetooth_address address;
    memcpy( address.address, hci_data.data() + 3, bluetooth_address::s_bluetooth_address_size );

    std::u8string name( reinterpret_cast< const char8_t* >( hci_data.data() ) + 9, total_size - 9 );
    name.push_back( 0x00 );

    auto dev_manager = framework_manager::get_instance().get_info_manager()
        .get_detail_information<device_manager>( device_manager::s_device_manager_name );

    auto trust_devices_ = framework_manager::get_instance().get_info_manager()
        .get_detail_information<trusted_devices>( trusted_devices::s_trusted_devices_name );

    trust_devices_->update( address, name );

    dev_manager->update_davice_name( address, name );
    m_pairing_manager.device_name_requested( address, std::move( name ) );
}

void gap_module::handle_command_status( std::shared_ptr<hci_data> const& a_hci_event )
{
    uint8_t status = a_hci_event->m_buffer[2];
    if( 0x01 == status )
    {
        uint16_t hci_cmd_raw = le_to_host16( a_hci_event->m_buffer.data() + 4 );
        hci_command hci_cmd = static_cast<hci_command>( hci_cmd_raw );

        auto con = framework_manager::get_instance().get_info_manager()
            .get_detail_information<controller>( controller::s_information_name );
        con->update_controller_support_cmd( hci_cmd, false );
    }
}

void gap_module::set_discoverable( bool a_discoverable, bool a_execute )
{
    LogUtilInfo() << "set discoverable. From " << std::boolalpha << m_discoverable
        << " to " << a_discoverable;
    switch( m_discoverable )
    {
    case bluetooth::enable_status::unknown:
        m_discoverable = a_discoverable ? enable_status::enabling : enable_status::disabling;
        break;
    case bluetooth::enable_status::disabled:
        if( a_discoverable )
        {
            m_discoverable = enable_status::enabling;
        }
        break;
    case bluetooth::enable_status::enabling:
        if( !a_discoverable )
        {
            m_discoverable = enable_status::disabling;
        }
        break;
    case bluetooth::enable_status::enabled:
        if( !a_discoverable )
        {
            m_discoverable = enable_status::disabling;
        }
        break;
    case bluetooth::enable_status::disabling:
        if( a_discoverable )
        {
            m_discoverable = enable_status::enabling;
        }
        break;
    default:
        LogUtilError() << "Unknown status.";
        break;
    }

    if( a_execute )
    {
        set_scan_mode();
    }
}

void gap_module::set_connectable( bool a_connectable, bool a_execute )
{
    LogUtilInfo() << "set connectable. From " << std::boolalpha << m_connectable
        << " to " << a_connectable;
    switch( m_connectable )
    {
    case bluetooth::enable_status::unknown:
        m_connectable = a_connectable ? enable_status::enabling : enable_status::disabling;
        break;
    case bluetooth::enable_status::disabled:
        if( a_connectable )
        {
            m_connectable = enable_status::enabling;
        }
        break;
    case bluetooth::enable_status::enabling:
        if( !a_connectable )
        {
            m_connectable = enable_status::disabling;
        }
        break;
    case bluetooth::enable_status::enabled:
        if( !a_connectable )
        {
            m_connectable = enable_status::disabling;
        }
        break;
    case bluetooth::enable_status::disabling:
        if( a_connectable )
        {
            m_connectable = enable_status::enabling;
        }
        break;
    default:
        LogUtilError() << "Unknown status.";
        break;
    }

    if( a_execute )
    {
        set_scan_mode();
    }
}

void gap_module::set_visiblility( bool a_discoverable, bool a_connectable )
{
    set_connectable( a_connectable, false );
    set_discoverable( a_discoverable, false );
    set_scan_mode();
}

void gap_module::changed_local_name( std::u8string const& a_name )
{
    if( powering_status::power_on != get_power_status() )
    {
        LogUtilInfo() << "Not powered on. cannot setting.";
        return;
    }

    auto con = framework_manager::get_instance().get_info_manager()
        .get_detail_information<controller>( controller::s_information_name );
    con->set_local_name( a_name );
    auto hci_send = make_set_local_name( a_name );

    LogUtilInfo() << "Change local name to " << (char*)( a_name.c_str() );
    make_send_hci_task_and_schedule( hci_send, std::bind(
        &gap_module::handle_hci_event, this, std::placeholders::_1 ) );
}

void gap_module::relay_hci_event( std::shared_ptr<hci_data> const& a_hci_event )
{
    std::shared_ptr<gap_module_task> tsk = std::make_shared<gap_module_task>();
    tsk->set_target_module( s_gap_module_name );
    tsk->m_gap_task_type = gap_task_type::hci_event;
    tsk->m_hci_response = a_hci_event;
    tsk->set_source_module( s_gap_module_name );
    framework_manager::get_instance().get_thread_manager().post_task( tsk, framework::source_here );
}

void gap_module::make_edr_acl( std::shared_ptr<gap_module_task> const& a_detail_task )
{
    bluetooth_address& remote_device = a_detail_task->m_address;

    bool found = false;
    for( auto& ele : m_acl_cb )
    {
        if( ele.m_remote_device == remote_device )
        {
            found = true;
            if( ele.m_status != connection_status::disconnected )
            {
                break;
            }
            else
            {
                LogUtilInfo() << "We already page the device: " << remote_device.to_string();
                return;
            }
        }
    }

    if( !found )
    {
        acl_connection_cb cb;
        cb.m_remote_device = remote_device;
        cb.m_status = connection_status::connecting;
        m_acl_cb.push_back( cb );
    }

    std::shared_ptr<hci_data> hci = std::make_shared<hci_data>();
    hci->m_type = uart_hci_type::command_type;
    hci->m_from_controller = false;
    hci->m_buffer.reserve( 50 );

    uint8_t buff[100];
    uint8_t* p = buff;
    write_le16( buff, static_cast<uint16_t>( hci_command::hci_create_connection ) );
    buff[2] = 0x0D;
    p += 3;

    memcpy( p, remote_device.address, bluetooth_address::s_bluetooth_address_size );
    p += bluetooth_address::s_bluetooth_address_size;

    p[0] = 0x18;
    p[1] = 0xCC;
    p += 2;

    p[0] = 0x00; // RPM: r0
    p += 1;

    p[0] = 0x00; // Reserved byte
    p += 1;

    p[0] = 0x00;
    p[1] = 0x00;
    p += 2;

    p[0] = 0x01;
    p += 1;

    hci->m_buffer.insert( hci->m_buffer.end(), buff, p );

    make_send_hci_task_and_schedule( hci, nullptr );
}

void gap_module::make_send_hci_task_and_schedule
    (
    std::shared_ptr<hci_data> a_hci_data,
    std::function<void(std::shared_ptr<hci_data>)> a_hci_data_receive_handler
    )
{
    std::shared_ptr<hci_module::hci_module_task> hci_task;
    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    hci_task->m_hci_data = a_hci_data;
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( get_name() );
    if( a_hci_data_receive_handler )
    {
        hci_task->m_hci_data_handler = a_hci_data_receive_handler;
        hci_task->m_handler_module_name = get_name();
    }
    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

void gap_module::set_scan_mode()
{
    if( powering_status::power_on != get_power_status() )
    {
        LogUtilInfo() << "Not powered on. cannot setting.";
        return;
    }

    std::shared_ptr<hci_module::hci_module_task> hci_task;
    hci_task = std::make_shared<hci_module::hci_module_task>();
    hci_task->m_hci_task_type = hci_module::hci_task_type::send_hci_data;
    bool discoverable = false;
    bool connectable = false;
    switch( m_discoverable )
    {
    case bluetooth::enable_status::unknown:
    case bluetooth::enable_status::disabled:
    case bluetooth::enable_status::disabling:
        discoverable = false;
        break;
    case bluetooth::enable_status::enabling:
    case bluetooth::enable_status::enabled:
        discoverable = true;
        break;
        break;
    default:
        LogUtilError() << "Unknown status.";
        break;
    }

    switch( m_connectable )
    {
    case bluetooth::enable_status::unknown:
    case bluetooth::enable_status::disabled:
    case bluetooth::enable_status::disabling:
        connectable = false;
        break;
    case bluetooth::enable_status::enabling:
    case bluetooth::enable_status::enabled:
        connectable = true;
        break;
        break;
    default:
        LogUtilError() << "Unknown status.";
        break;
    }
    hci_task->m_hci_data = make_scan_mode( discoverable, connectable );
    hci_task->set_target_module( hci_module::s_hci_module_name );
    hci_task->set_source_module( get_name() );
    hci_task->m_hci_data_handler = std::bind(
        &gap_module::handle_hci_event, this, std::placeholders::_1 );
    hci_task->m_handler_module_name = get_name();
    framework_manager::get_instance().get_thread_manager().post_task( hci_task, framework::source_here );
}

void gap_module::notify_searching_changed()
{
    function_type type = m_searching ? function_type::searching : function_type::searching_completed;
    std::function<void()> fun;
    fun = std::bind( stack_manager::get_instance().get_stack_callback(),
        service_type::not_specified, type, nullptr );
    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, s_general_seq_task_runner_module );
    task->set_source_module( get_name() );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

void gap_module::notify_visiblity_changed()
{
    visibility visi;
    visi.connectable = m_connectable;
    visi.discoverable = m_discoverable;
    std::function<void()> fun;
    fun = [visi]()mutable
    {
        stack_manager::get_instance().get_stack_callback()( service_type::not_specified,
            function_type::visibility_changed, reinterpret_cast< void* >( &visi ) );
    };

    std::shared_ptr<executable_task> task = std::make_shared<executable_task>();
    task->set_fun( fun, s_general_seq_task_runner_module );
    task->set_source_module( get_name() );
    framework_manager::get_instance().get_thread_manager().post_task( task, framework::source_here );
}

}

