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
#include "..\common\stream_writer.h"

#include <cstdint>
#include <string>

namespace bluetooth
{

enum class l2cap_ext_feature_flag : uint8_t
{
    flow_control_mode = 0x00,
    retransmission_mode = 0x01,
    bi_directional_qos = 0x02,
    enhanced_retransmission_mode = 0x03,
    streaming_mode = 0x04,
    fcs_option = 0x05,
    extended_flow_specification_edr = 0x06,
    fixed_channels = 0x07,
    extended_window_size = 0x08,
    unicast_connectionless_data_reception = 0x09,
    enhanced_credit_based_flow_control = 0x0A
};

class l2cap_ext_features
{

public:

    l2cap_ext_features();

    void set_value( uint8_t const* a_buffer, uint8_t a_size = 4 );

    bool support_feature( l2cap_ext_feature_flag a_feature )const;

    void set_support_feature( l2cap_ext_feature_flag a_feature );

    void clear();

    std::string to_string()const;

private:

    friend class stream_writer;

    uint8_t m_ext_features[4];
};

}

