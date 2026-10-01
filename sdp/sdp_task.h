#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

#include "framework/abstract_task.h"
#include "l2cap/l2cap_common.h"
#include "uuid.h"
#include "sdp_common.h"

namespace bluetooth
{

enum class sdp_task_type : uint8_t
{
    invalid_type = 0x00,
    register_service_record = 0x01, // Request SDP module register a new service record into service
                                    // record database.
    service_search_request = 0x02,  // execute service search transaction.
    service_search_attribute = 0x03, // execute service search attribute request
};

class sdp_task : public framework::abstract_task
{

public:

    static uint16_t s_sdp_task_type_id;

    sdp_task_type m_type = sdp_task_type::invalid_type;

    sdp_task();
};

// Request SDP module register a new service record into service
// record database. m_service_record will be registered and then
// after registered, sdp module will invoke m_registered_callback
// to notify registering finished with record handle. The callback
// will scheduled to m_callback_handle_module
class sdp_task_register_service_record : public sdp_task
{

public:

    sdp_task_register_service_record()
    {
        m_type = sdp_task_type::register_service_record;
    }

    std::shared_ptr<sdp_service_record> m_service_record;
    std::function<void( uint32_t )> m_registered_callback;
    std::string m_callback_handle_module;

};

class sdp_task_service_search_request : public sdp_task
{

public:

    sdp_task_service_search_request()
    {
        m_type = sdp_task_type::service_search_request;
    }

    bluetooth_address m_remote_device;
    std::vector<uuid> m_service_uuid;
};

class sdp_task_service_search_attribute : public sdp_task
{

public:

    sdp_task_service_search_attribute()
    {
        m_type = sdp_task_type::service_search_attribute;
    }

    bluetooth_address m_remote_device;
    std::vector<uuid> m_service_uuid;
    std::vector<uint16_t> m_attribute_id_list;
    std::vector<std::pair<uint16_t, uint16_t>> m_requested_id_ranges;
};

}
