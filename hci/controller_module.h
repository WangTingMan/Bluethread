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
#include "..\framework\abstract_module.h"
#include "..\framework\lendable_element.h"
#include "..\framework\abstract_task.h"
#include "data_element.h"

namespace bluetooth
{

class controller;

class controller_module : public framework::abstract_module
{

public:

    enum class controller_task_type : uint8_t
    {
        invalid_type = 0x00,
        power_on,
        power_off,
        hci_cmd_response, // m_hci_response and m_hci_cmd are valid
    };

    class controller_module_task : public framework::abstract_task
    {

    public:

        controller_task_type m_task_type = controller_task_type::invalid_type;
        std::shared_ptr<hci_data> m_hci_response;
    };

    constexpr static const char* s_controller_module_name = "controller_module";

    controller_module();

    void initialize()override;

    void deinitialize()override;

    void handle_task( std::shared_ptr<framework::abstract_task> a_task )override;

    void handle_event( std::shared_ptr<framework::framework_event> a_event )override;

private:

    void handle_power_on( std::shared_ptr<controller_module_task> a_task );

    void handle_power_off( std::shared_ptr<controller_module_task> const& a_task );

    void handle_response_sequence( std::shared_ptr<controller_module_task> a_task );

    void handle_response( std::shared_ptr<hci_data> a_res );

    std::shared_ptr<hci_data> handle_ext_features_response
        (
        std::shared_ptr<controller> const& a_con,
        std::vector<uint8_t> const& a_hci_buffer
        );

    std::shared_ptr<hci_data> make_greed_inquiry_mode( std::shared_ptr<controller> const& a_con );
};

}
