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
#include "../framework/abstract_info.h"

#include "remote_device.h"

#include <shared_mutex>
#include <string>

namespace bluetooth
{

class trusted_devices : public framework::abstract_information
{

public:

    constexpr static const char* s_trusted_devices_name = "trusted_devices";

    trusted_devices();

    void add_trusted_device( remote_device a_device );

    bool get_link_key( bluetooth_address const& a_remote_device, std::vector<uint8_t>& a_link_key );

    void remove_trusted( bluetooth_address const& a_remote_device );

    /**
     * Update the link key. Create a new record if not has
     */
    void update
        (
        bluetooth_address const& a_remote_device,
        std::vector<uint8_t> a_link_key,
        std::u8string a_name
        );

    /**
     * Update the device name if has
     */
    void update
        (
        bluetooth_address const& a_remote_device,
        std::u8string a_name
        );

    std::vector<remote_device> get_paired_devices();

private:

    void notify_paired_device( remote_device const* a_paired_device );

    std::shared_mutex m_mutex;
    std::vector<remote_device> m_paired_devices;
};

}

