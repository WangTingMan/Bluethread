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
#include "framework/framework_manager.h"
#include "bluetooth_address.h"
#include "bluetooth.h"

#include <shared_mutex>

namespace bluetooth
{

class stack_manager
{

public:

    stack_manager();

    static stack_manager& get_instance();

    void enable_bt();

    void search_device();

    void cancel_search_edr_device();

    void accept_ssp_confirm( bluetooth_address a_address, bool a_accept );

    void set_callback( stack_callback a_callback );

    stack_callback get_stack_callback()const
    {
        std::shared_lock<std::shared_mutex> locker( m_mutex );
        return m_callback;
    }

private:

    void init();

    mutable std::shared_mutex m_mutex;
    stack_callback m_callback;
};

}

