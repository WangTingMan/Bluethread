#include "sdp/sdp_data_element.h"

#include <cassert>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "BluetoothStack.lib")
#pragma comment(lib, "framework.lib")
#endif

using namespace bluetooth;

void sample_spp_service_record_list_test();
void attribute_check( uint16_t a_attribute_id, sdp_data_element a_attribute_value );
void protocol_descriptor_list_check( sdp_data_element a_attribute_value );

int main()
{
    sample_spp_service_record_list_test();
    return 0;
}

void sample_spp_service_record_list_test()
{
    bool status = false;
    std::vector<uint8_t> raw_data
        {
        0x35, 0x51, 0x36, 0x00, 0x4E, 0x09, 0x00, 0x00, 0x0A, 0x00, 0x01,
        0x00, 0x18, 0x09, 0x00, 0x01, 0x35, 0x03, 0x19, 0x11, 0x01, 0x09,
        0x00, 0x04, 0x35, 0x0C, 0x35, 0x03, 0x19, 0x01, 0x00, 0x35, 0x05,
        0x19, 0x00, 0x03, 0x08, 0x0A, 0x09, 0x00, 0x05, 0x35, 0x03, 0x19,
        0x10, 0x02, 0x09, 0x00, 0x09, 0x35, 0x08, 0x35, 0x06, 0x19, 0x11,
        0x01, 0x09, 0x01, 0x02, 0x09, 0x01, 0x00, 0x25, 0x13, 0x42, 0x6C,
        0x75, 0x65, 0x53, 0x50, 0x50, 0x20, 0x66, 0x6F, 0x72, 0x20, 0x41,
        0x6E, 0x64, 0x72, 0x6F, 0x69, 0x64
        };
    uint8_t const* p_buffer = raw_data.data();
    uint32_t attribute_list_size = raw_data.size();
    sdp_data_element attribute_list_value;
    uint32_t pared_size_for_list = 0;
    status = sdp_data_element::parse_from( p_buffer, attribute_list_size,
        pared_size_for_list, attribute_list_value );
    assert( status == true );
    assert( pared_size_for_list == attribute_list_size );

    status = attribute_list_value.can_as_elements();
    assert( status == true );

    std::vector<sdp_data_element> service_record_list = attribute_list_value.get_elements();
    assert( service_record_list.size() == 1 );

    std::vector<sdp_data_element> service_record_attributes_list;
    uint16_t attribute_id = 0;
    sdp_data_element attribute_value;
    for( int i = 0; i < service_record_list.size(); ++i )
    {
        auto& service_record = service_record_list[i];
        status = service_record_list[i].can_as_elements();
        assert( status == true );
        service_record_attributes_list = service_record_list[i].get_elements();

        for( auto j = 0; j < service_record_attributes_list.size(); j = j + 2)
        {
            status = service_record_attributes_list[j].can_as_uint16();
            assert( status == true );
            attribute_id = service_record_attributes_list[j].get_uint16_value();

            uint16_t next = j + 1;
            if( next >= service_record_attributes_list.size() )
            {
                break;
            }

            attribute_check(attribute_id, service_record_attributes_list[next] );
        }
    }

}

void attribute_check( uint16_t a_attribute_id, sdp_data_element a_attribute_value )
{
    bool status = false;
    switch( a_attribute_id )
    {
    case sdp_universal_attribute_id::service_record_handle:
        {
            uint32_t handle = 0;
            status = a_attribute_value.can_as_uint32();
            assert( status == true );

            handle = a_attribute_value.get_uint32_value();
            std::cout << "service handle: " << handle << std::endl;
        }
        break;
    case sdp_universal_attribute_id::service_class_id_list:
        break;
    case sdp_universal_attribute_id::protocol_descriptor_list:
        protocol_descriptor_list_check( a_attribute_value );
        break;
    default:
        break;
    }
}

void protocol_descriptor_list_check( sdp_data_element a_attribute_value )
{
    bool status = false;
    status = a_attribute_value.can_as_alternative_elements();
    if( status )
    {
        /* there are two or more protocol stacks */
        std::vector<sdp_data_element> protocol_stacks;
        protocol_stacks = a_attribute_value.get_alternative_elements();
        for( int i = 0; i < protocol_stacks.size(); ++i )
        {
            protocol_descriptor_list_check( protocol_stacks[i] );
        }
        return;
    }

    status = a_attribute_value.can_as_elements();
    assert( status == true );

    std::vector<sdp_data_element> protocol_descriptors;
    protocol_descriptors = a_attribute_value.get_elements();

    std::vector<sdp_data_element> protocol_descriptor;
    for( int i = 0; i < protocol_descriptors.size(); ++i )
    {
        status = protocol_descriptors[i].can_as_elements();
        assert( status == true );

        protocol_descriptor = protocol_descriptors[i].get_elements();
        if( protocol_descriptor.empty() )
        {
            continue;
        }

        status = protocol_descriptor[0].can_as_uuid();
        assert( status == true );

        auto protocol_uuid = protocol_descriptor[0].get_uuid();
        sdp_data_element data_element;
        for( int z = 1; z < protocol_descriptor.size(); ++z )
        {
            data_element = protocol_descriptor[z];
        }

    }
}
