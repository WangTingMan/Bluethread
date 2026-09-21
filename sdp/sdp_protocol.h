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
#include "sdp_common.h"
#include "bluetooth_address.h"
#include "..\common\protocol_headers.h"

#include <cstdint>
#include <vector>
#include <functional>

namespace bluetooth
{

enum class sdp_pdu_id : uint8_t
{
    sdp_error_rsp = 0x01,
    sdp_service_search_req = 0x02,
    sdp_service_search_rsp = 0x03,
    sdp_service_attr_req = 0x04,
    sdp_service_attr_rsp = 0x05,
    sdp_service_search_attr_req = 0x06,
    sdp_service_search_attr_rsp = 0x07,
};

std::ostream& operator<<( std::ostream& a_os, sdp_pdu_id a_state );

enum class sdp_error_code : uint8_t
{
    reserve_code = 0x00,
    unsupported_sdp_version = 0x01,
    invalid_service_record_handle = 0x02,
    invalid_syntax = 0x03,
    invalid_continue_status = 0x04,
    reject_with_resource_limited = 0x05
};

class sdp_header : public l2cap_header
{

public:

    /**
     * Here is the parameter length. See sdp specification
     */
    void set_sdu_length( uint16_t a_length )override;

    void to_raw_buffer( uint8_t* a_buffer, uint32_t a_size )override;

    uint16_t header_size()const override;

    void set_pdu_id( sdp_pdu_id a_id )
    {
        m_pdu_id = a_id;
    }

    void set_transcation_id( uint16_t a_transaction_id )
    {
        m_transaction_id = a_transaction_id;
    }

    void parse_from_raw_data( uint8_t* a_buffer, uint32_t a_size )override;

private:

    sdp_pdu_id m_pdu_id = sdp_pdu_id::sdp_error_rsp;
    uint16_t m_transaction_id = 0;
    uint16_t m_parameter_length = 0;
};

class sdp_protocol_base
{

public:

    virtual ~sdp_protocol_base(){}

    sdp_pdu_id m_pdu_id = sdp_pdu_id::sdp_error_rsp;
    uint16_t m_transaction_id = 0;
    uint16_t m_parameter_length = 0;
    uint16_t m_local_cid = 0x00;
    bluetooth_address m_remote_device;
};

class sdp_error_rsp : public sdp_protocol_base
{

public:

    sdp_error_rsp()
    {
        m_pdu_id = sdp_pdu_id::sdp_error_rsp;
    }

    sdp_error_code m_error_code = sdp_error_code::reserve_code;
};

class sdp_servbice_search_req : public sdp_protocol_base
{

public:

    sdp_servbice_search_req()
    {
        m_pdu_id = sdp_pdu_id::sdp_service_search_req;
    }

    std::vector<uuid> m_matching_uuids;
    uint16_t m_max_return_count = 0;
    std::vector<uint8_t> m_continue_info;
};

class sdp_service_search_attribute_req : public sdp_protocol_base
{

public:

    sdp_service_search_attribute_req()
    {
        m_pdu_id = sdp_pdu_id::sdp_service_search_attr_req;
    }

    std::vector<uuid> m_matching_uuids;  // the service search pattern
    uint16_t m_max_return_count = 0;     // maximum attribute byte count
    std::vector<uint16_t> m_matching_ids;// the ids to matching
    std::vector<std::pair<uint16_t, uint16_t>> m_requested_id_ranges; // the id ranges to matching
    std::vector<uint8_t> m_continue_info;// continue information
};

class sdp_service_search_attribute_rsp : public sdp_protocol_base
{

public:

    sdp_service_search_attribute_rsp()
    {
        m_pdu_id = sdp_pdu_id::sdp_service_search_attr_rsp;
    }

    std::vector<uint8_t> m_attribute_list;
    std::vector<uint8_t> m_continue_info;
};

using sdp_packet_send_type = std::function< void( std::shared_ptr<sdp_protocol_base> const&, bluetooth_address) >;

}

