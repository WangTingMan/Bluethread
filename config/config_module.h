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
#include "../framework/abstract_module.h"
#include "../framework/lendable_element.h"
#include "../framework/abstract_task.h"

#include "../common/bluetooth_common_event.h"

namespace bluetooth
{

class config_module : public framework::abstract_module
{

public:

    constexpr static const char* s_config_module_name = "config_module";

    enum class config_task_type : uint8_t
    {
        invalid_type = 0xFF,
        write_trusted_device = 0x01, // To write trusted devices into config file
    };

    class config_module_task : public framework::abstract_task
    {

    public:

        config_module_task()
        {
            m_task_type = static_cast<framework::task_type>(s_config_module_task_type_id);
        }

        config_task_type m_config_task_type = config_task_type::invalid_type;
        static uint16_t s_config_module_task_type_id;
    };

    config_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_bluetoot_event( std::shared_ptr<bluetooth_common_event> const& a_bt_event );

    void write_pairing_to_file();

    void load_pairing_from_file();

};

}

