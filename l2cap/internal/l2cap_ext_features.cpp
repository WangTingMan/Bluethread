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

#include "l2cap_ext_features.h"

#include <string>

namespace bluetooth
{

l2cap_ext_features::l2cap_ext_features()
{
    memset( m_ext_features, 0x00, sizeof( m_ext_features ) );
}

void l2cap_ext_features::set_value( uint8_t const* a_buffer, uint8_t a_size )
{
    memcpy( m_ext_features, a_buffer, sizeof( m_ext_features ) );
}

bool l2cap_ext_features::support_feature( l2cap_ext_feature_flag a_feature )const
{
    bool ret = false;
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    ret = ( 0 != ( m_ext_features[static_cast< uint8_t >( a_feature ) / 8]
        & ( power2[static_cast< uint8_t >( a_feature ) % 8] ) ) );
    return ret;
}

void l2cap_ext_features::set_support_feature( l2cap_ext_feature_flag a_feature )
{
    uint8_t power2[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
    m_ext_features[static_cast< uint8_t >( a_feature ) / 8] |= power2[static_cast< uint8_t >( a_feature ) % 8];
}

template<>
stream_writer& stream_writer::operator<<( l2cap_ext_features const& a_value )
{
    write_buffer( a_value.m_ext_features, sizeof( a_value.m_ext_features ) );
    return *this;
}

void l2cap_ext_features::clear()
{
    memset( m_ext_features, 0x00, sizeof( m_ext_features ) );
}

std::string l2cap_ext_features::to_string()const
{
    std::string str(" support: " );

    if( support_feature( l2cap_ext_feature_flag::flow_control_mode ) )
    {
        str.append( "flow_control_mode " );
    }

    if( support_feature( l2cap_ext_feature_flag::retransmission_mode ) )
    {
        str.append( "retransmission_mode " );
    }

    if( support_feature( l2cap_ext_feature_flag::bi_directional_qos ) )
    {
        str.append( "bi_directional_qos " );
    }

    if( support_feature( l2cap_ext_feature_flag::enhanced_retransmission_mode ) )
    {
        str.append( "enhanced_retransmission_mode " );
    }

    if( support_feature( l2cap_ext_feature_flag::streaming_mode ) )
    {
        str.append( "streaming_mode " );
    }

    if( support_feature( l2cap_ext_feature_flag::fcs_option ) )
    {
        str.append( "fcs_option " );
    }

    if( support_feature( l2cap_ext_feature_flag::extended_flow_specification_edr ) )
    {
        str.append( "extended_flow_specification_edr " );
    }

    if( support_feature( l2cap_ext_feature_flag::fixed_channels ) )
    {
        str.append( "fixed_channels " );
    }

    if( support_feature( l2cap_ext_feature_flag::extended_window_size ) )
    {
        str.append( "extended_window_size " );
    }

    if( support_feature( l2cap_ext_feature_flag::unicast_connectionless_data_reception ) )
    {
        str.append( "unicast_connectionless_data_reception " );
    }
    return str;
}

}

