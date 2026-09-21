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

#pragma once
#include <cstdint>

namespace bluetooth
{

class abstract_hci_interface
{

public:

    enum class driver_init_status
    {
        deinitialized = 0,
        initializing = 1,
        initialized = 2,
        deinitalizing = 3
    };

    virtual ~abstract_hci_interface(){}

    /**
     * The chipset driver should implement this function.
     * initialize the chipset or power up the chipset
     * return: true if the initialization work is going on;
     * otherwise false.
     */
    virtual bool intialize() = 0;

    /**
     * The chipset driver should implement this function.
     * deinitialize the chipset or power off the chipset
     * return: true if the deinitialization work is going on;
     * otherwise false.
     */
    virtual bool deintialize() = 0;

    /**
     * The chipset driver should implement this function.
     * send hci data to chipset.
     * a_type: the hci data packet type. see core spec 5.3,
     * vol 4, part A, table 2.1
     * a_data: the buffer pointer
     * a_size: the buffer size
     * return: how many bytes has been sent to chipset
     */
    virtual int send_data_to_controller
        (
        uint8_t a_type,
        uint8_t* a_data,
        uint16_t a_size
        ) = 0;

    /**
     * The chipset driver invoke this function after received
     * completed hci packet from chipset.
     * handle data from contrller.
     * warning: the data shall be completed hci packet.
     */
    void handle_data_from_controller
        (
        uint8_t a_type,
        uint8_t* a_data,
        uint16_t a_size
        );

    /**
     * The chipset driver invoke this function after intialization
     * completed.
     * handle chipset initialziation work completed.
     * The stack will consider that the controller is ready
     * initialized or power up success if the return value is
     * true, and then the stack will work; otherwise the stack
     * may try to initalize controller again.
     * return: true if the initialization work success;
     * otherwise false.
     */
    void initialization_completed( bool a_result );

    /**
     * The chipset driver invoke this function after deintialization
     * completed.
     * handle chipset deinitialziation work completed.
     * The stack will consider that the controller is ready
     * deinitialized or power off success if the return value is
     * true; otherwise the stack will do nothing. That is, the stack
     * will deinitialize the chipset only once time.
     * Some chipset or platform cannot power off the chipset, so
     * we should handle that case.
     * return: true if the deinitialization work success;
     * otherwise false.
     */
    void deinitialization_completed( bool a_result );

    driver_init_status const& get_initial_status()const
    {
        return m_driver_status;
    }

private:

    driver_init_status m_driver_status = driver_init_status::deinitialized;
};

}

