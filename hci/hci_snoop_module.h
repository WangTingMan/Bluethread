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
#include "../framework/abstract_task.h"
#include "../include/data_element.h"

#include <mutex>
#include <fstream>

namespace bluetooth
{

class hci_snoop_module : public framework::abstract_module
{

public:

    constexpr static const char* s_hci_snoop_module_name = "hci_snoop_module";

    class hci_snoop_write_task : public framework::abstract_task
    {

    public:

        hci_snoop_write_task()
        {
            set_task_type( static_cast<framework::task_type>(s_hci_snoop_write_task_type) );
        }

        std::shared_ptr<hci_data> m_hci_data;
        static uint16_t s_hci_snoop_write_task_type;
    };

    hci_snoop_module();

    /**
     * initialize this module self.
     */
    void initialize()override;

    /**
     * deinitialize this module self
     */
    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_write_hci_log_task( std::shared_ptr<hci_data> a_hci_data );

    void open_new_file();

    std::recursive_mutex m_mutex;
    std::ofstream m_logfile;
    uint32_t m_wrote_packets = 0;
};

}

