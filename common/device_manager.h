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
#include "..\framework\abstract_info.h"

#include "remote_device.h"

#include <chrono>
#include <vector>
#include <shared_mutex>

namespace bluetooth
{

struct device_found
{
    remote_device m_device;
    std::chrono::steady_clock::time_point m_found_time;
};

class device_manager : public framework::abstract_information
{

public:

    constexpr static const char* s_device_manager_name = "device_manager";

    device_manager();

    /**
     * New device found.
     */
    void new_device_found
        (
        bluetooth_address const &a_address,
        uint8_t a_page_scan_repetition_mode,
        class_of_device a_cod,
        uint16_t a_clock_offset,
        int8_t a_rssi
        );

    /**
     * Update a_address' device name. return true if device name changed
     */
    bool update_davice_name
        (
        bluetooth_address const& a_address,
        std::u8string a_name
        );

    std::tuple<bool, std::u8string> get_device_name( bluetooth_address const& a_address );

    void update_eir_uuid
        (
        bluetooth_address const& a_address,
        std::vector<uuid> a_uuids
        );

    void notify_device_found( bluetooth_address const& a_address );

    void delete_oldest_found_device();

    std::shared_ptr<remote_device> take_found_device( bluetooth_address const& a_address );

private:

    std::shared_mutex m_mutex;
    std::vector<std::shared_ptr<device_found>> m_discovered_devices;
};

}

