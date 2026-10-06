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

#include "did_service_record.h"
#include "../framework/internal/platform.h"

namespace bluetooth
{

/**
 * DID profile version 1.3
 */
constexpr uint16_t specification_id_value = 0x0103;


constexpr uint16_t vendor_id_value = 0x00E0;
constexpr uint16_t product_id_value = 0x1020;
constexpr uint16_t version_value = 0x0436;

did_service_record::did_service_record()
{
    std::vector<uuid> uuids;
    uuids.push_back( uuid::from_16bit( sdp_service_uuid::pnp_information ) );
    set_service_class_id_list( uuids );

    /**
     * To set the DID version number for this DID service record, the specification ID attribute (0x0200)
     * is set to 0x0103, which indicates that this service record conforms to the Bluetooth Device ID
     * Profile version 1.3.
     */
    auto attribute = find_or_create_attribute( did_attribute_id::specification_id );
    attribute->get_value().set_uint16_value( specification_id_value );

    /**
     * To set the vendor ID, product ID, and version number for this DID service record,
     * the following attributes are set:
     */
    set_vendor_id( vendor_id_value );
    set_product_id( product_id_value );
    set_product_version( version_value );

    /**
     * To indicate that this DID service record is the primary record for the device,
     * the primary record attribute (0x0204) is set to TRUE.
     */
    attribute = find_or_create_attribute( did_attribute_id::primary_record );
    attribute->get_value().set_boolean();

    /**
     * To indicate that the vendor ID is assigned by the Bluetooth SIG,
     * the vendor ID source attribute (0x0205) is set to 0x0001.
     */
    attribute = find_or_create_attribute( did_attribute_id::vendor_id_source );
    attribute->get_value().set_uint16_value( 0x0001 );

    attribute = find_or_create_attribute( sdp_universal_attribute_id::language_base_attribute_id_list );
    attribute->get_value().set_elements( make_language_attribute_list() );

    uint16_t english_provider_name_attribute_id = language_base_id::english +
        attribute_id_offset_for_string::provider_name_offset;
    attribute = find_or_create_attribute( english_provider_name_attribute_id );
    attribute->get_value().set_string_value( framework::convert( "Wang Fei" ) );

    uint16_t descriptor_provider_name_attribute_id = language_base_id::english +
        attribute_id_offset_for_string::service_description_offset;
    attribute = find_or_create_attribute( descriptor_provider_name_attribute_id );
    std::string descriptor = "This is an example describing the DID service, demonstrating how"
        " to add service descriptions within a service. Clients may read the value of"
        " this attribute to obtain the description of the service.";
    attribute->get_value().set_string_value( framework::convert( descriptor ) );

    uint16_t chinese_provider_name_attribute_id = language_base_id::chinese +
        attribute_id_offset_for_string::provider_name_offset;
    attribute = find_or_create_attribute( chinese_provider_name_attribute_id );
    attribute->get_value().set_string_value( framework::convert( "Íõ·É" ) );
}

}

