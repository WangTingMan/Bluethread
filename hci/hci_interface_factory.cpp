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

#include "hci_interface_factory.h"

#include "framework/log_util.h"
#include "framework/framework_manager.h"
#include "framework/thread_manager.h"
#include "hci_module.h"

#if __has_include("windows/win_bluetooth.h")
#include "windows/win_bluetooth.h"
#define HAS_WINDOWS_DRIVER
#endif

#ifdef HAS_WINDOWS_DRIVER
void driver_logger( hci_log_message a_log_msg )
{
    framework::log_level level = framework::log_level::info;
    switch( a_log_msg.priority )
    {
    case HCI_LOG_INFO_LEVEL:
        level = framework::log_level::info;
        break;
    case HCI_LOG_DEBUG_LEVEL:
        level = framework::log_level::debug;
        break;
    case HCI_LOG_ERROR_LEVEL:
        level = framework::log_level::error;
        break;
    case HCI_LOG_VERBOSE_LEVEL:
        level = framework::log_level::verbose;
        break;
    case HCI_LOG_WARN_LEVEL:
        level = framework::log_level::warning;
        break;
    case HCI_LOG_FATAL_LEVEL:
        level = framework::log_level::fatal;
        break;
    }

    framework::util_logger( a_log_msg.file, a_log_msg.line, level ).logging() << a_log_msg.message;
}
#endif

std::shared_ptr<bluetooth::abstract_hci_interface> s_interface_;

class standard_win_bluetooth_usb_driver : public bluetooth::abstract_hci_interface
{

public:

    standard_win_bluetooth_usb_driver()
    {
#ifdef HAS_WINDOWS_DRIVER
        memset( &m_hci_cb, 0x00, sizeof( bluetooth_hci_callback_t ) );
        m_hci_cb.chipset_initialization_completed = []( int a_result )
            {
                s_interface_->initialization_completed( a_result == 0 );
            };

        m_hci_cb.handle_data_from_controller = []( uint8_t a_type, uint8_t* a_data, uint16_t a_size )
            {
                s_interface_->handle_data_from_controller( a_type, a_data, a_size );
            };

        m_hci_cb.log_handler = []( hci_log_message* a_log_msg )
            {
                driver_logger( *a_log_msg );
            };
#else
        LogUtilError( "No windows bluetooth driver!" );
#endif
    }

    virtual bool intialize() override
    {
#ifdef HAS_WINDOWS_DRIVER
        if( m_hci_interface )
        {
            return true;
        }

        m_hci_interface = get_bluetooth_hci_interface();
        m_hci_interface->register_callback( &m_hci_cb );
        m_hci_interface->initialize();

        return true;
#else
        LogUtilError( "No windows bluetooth driver!" );
        return false;
#endif
    }

    virtual bool deintialize() override
    {
        int status = 0;
#ifdef HAS_WINDOWS_DRIVER
        status = m_hci_interface->deintialize();
#endif
        return status == 0;
    }

    virtual int send_data_to_controller
        (
        uint8_t a_type,
        uint8_t* a_data,
        uint16_t a_size
        ) override
    {
        int ret = 0;
#ifdef HAS_WINDOWS_DRIVER
        ret = m_hci_interface->send_data_to_controller( a_type, a_data, a_size );
#endif
        return ret;
    }

private:

#ifdef HAS_WINDOWS_DRIVER
    bluetooth_hci_interface_t* m_hci_interface = nullptr;
    bluetooth_hci_callback_t m_hci_cb;
#endif
};

namespace bluetooth
{

std::shared_ptr<abstract_hci_interface> hci_interface_factory::create_hci_interface()
{
    std::shared_ptr<abstract_hci_interface> interface_;
    interface_ = std::make_shared<standard_win_bluetooth_usb_driver>();
    s_interface_ = interface_;
    return interface_;
}

}
