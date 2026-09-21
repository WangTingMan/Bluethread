/******************************************************************************
 *
 *  Copyright 2017 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "config.h"
#include "framework/log_util.h"

#include "uuid.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <filesystem>
#include <sstream>
#include <type_traits>
#include <fstream>
#include <string>

#include <base\files\file_util.h>

 // Empty definition; this type is aliased to list_node_t.
struct config_section_iter_t {};

static bool config_parse( std::fstream& a_file, config_t* config );

template <typename T,
    class = typename std::enable_if<std::is_same<
    config_t, typename std::remove_const<T>::type>::value>>
    static auto section_find( T& config, const std::string& section )
{
    return std::find_if(
        config.sections.begin(), config.sections.end(),
        [&section]( const section_t& sec ) { return sec.name == section; } );
}

static const entry_t* entry_find( const config_t& config,
    const std::string& section,
    const std::string& key )
{
    auto sec = section_find( config, section );
    if( sec == config.sections.end() ) return nullptr;

    for( const entry_t& entry : sec->entries )
    {
        if( entry.key == key ) return &entry;
    }

    return nullptr;
}

std::unique_ptr<config_t> config_new_empty( void )
{
    return std::make_unique<config_t>();
}

std::unique_ptr<config_t> config_new( const char* filename )
{
    if( !filename )
    {
        LogUtilError() << "empty file name";
        return nullptr;
    }

    std::unique_ptr<config_t> config = config_new_empty();

    std::fstream file;
    file.open( filename, std::ios::in );
    if( !file.is_open() )
    {
        LogUtilError() << __func__ << ": unable to open file '" << filename;
        return nullptr;
    }

    if( !config_parse( file, config.get() ) )
    {
        config.reset();
    }

    return config;
}

std::string checksum_read( const char* filename )
{
    std::filesystem::path file_path( filename );
    bool existed = std::filesystem::exists( file_path );
    if( !existed )
    {
        LogUtilError() << __func__ << ": unable to locate file '" << filename << "'";
        return "";
    }
    std::string encrypted_hash;
    base::FilePath path( filename );
    if( !base::ReadFileToString( path, &encrypted_hash ) )
    {
        LogUtilError() << __func__ << ": unable to read file '" << filename << "'";
    }
    return encrypted_hash;
}

std::unique_ptr<config_t> config_new_clone( const config_t& src )
{
    std::unique_ptr<config_t> ret = config_new_empty();

    for( const section_t& sec : src.sections )
    {
        for( const entry_t& entry : sec.entries )
        {
            config_set_string( ret.get(), sec.name, entry.key, entry.value );
        }
    }

    return ret;
}

bool config_has_section( const config_t& config, const std::string& section )
{
    return ( section_find( config, section ) != config.sections.end() );
}

bool config_has_key( const config_t& config, const std::string& section,
    const std::string& key )
{
    return ( entry_find( config, section, key ) != nullptr );
}

int config_get_int( const config_t& config, const std::string& section,
    const std::string& key, int def_value )
{
    const entry_t* entry = entry_find( config, section, key );
    if( !entry ) return def_value;

    char* endptr;
    int ret = strtol( entry->value.c_str(), &endptr, 0 );
    return ( *endptr == '\0' ) ? ret : def_value;
}

uint64_t config_get_uint64( const config_t& config, const std::string& section,
    const std::string& key, uint64_t def_value )
{
    const entry_t* entry = entry_find( config, section, key );
    if( !entry ) return def_value;

    char* endptr;
    uint64_t ret = strtoull( entry->value.c_str(), &endptr, 0 );
    return ( *endptr == '\0' ) ? ret : def_value;
}

bool config_get_bool( const config_t& config, const std::string& section,
    const std::string& key, bool def_value )
{
    const entry_t* entry = entry_find( config, section, key );
    if( !entry ) return def_value;

    if( entry->value == "true" ) return true;
    if( entry->value == "false" ) return false;

    return def_value;
}

const std::string* config_get_string( const config_t& config,
    const std::string& section,
    const std::string& key,
    const std::string* def_value )
{
    const entry_t* entry = entry_find( config, section, key );
    if( !entry ) return def_value;

    return &entry->value;
}

void config_set_int( config_t* config, const std::string& section,
    const std::string& key, int value )
{
    config_set_string( config, section, key, std::to_string( value ) );
}

void config_set_uint64( config_t* config, const std::string& section,
    const std::string& key, uint64_t value )
{
    config_set_string( config, section, key, std::to_string( value ) );
}

void config_set_bool( config_t* config, const std::string& section,
    const std::string& key, bool value )
{
    config_set_string( config, section, key, value ? "true" : "false" );
}

void config_set_string( config_t* config, const std::string& section,
    const std::string& key, const std::string& value )
{
    if( !config )
    {
        LogUtilError() << "config null";
        return;
    }

    auto sec = section_find( *config, section );
    if( sec == config->sections.end() )
    {
        section_t sec_;
        sec_.name = section;
        config->sections.emplace_back( sec_ );
        sec = std::prev( config->sections.end() );
    }

    std::string value_no_newline;
    size_t newline_position = value.find( '\n' );
    if( newline_position != std::string::npos )
    {
        value_no_newline = value.substr( 0, newline_position );
    }
    else
    {
        value_no_newline = value;
    }

    for( entry_t& entry : sec->entries )
    {
        if( entry.key == key )
        {
            entry.value = value_no_newline;
            return;
        }
    }
    entry_t ent_;
    ent_.key = key;
    ent_.value = value_no_newline;
    sec->entries.emplace_back( ent_ );
}

bool config_remove_section( config_t* config, const std::string& section )
{
    CHECK( config );

    auto sec = section_find( *config, section );
    if( sec == config->sections.end() ) return false;

    config->sections.erase( sec );
    return true;
}

bool config_remove_key( config_t* config, const std::string& section,
    const std::string& key )
{
    CHECK( config );
    auto sec = section_find( *config, section );
    if( sec == config->sections.end() ) return false;

    for( auto entry = sec->entries.begin(); entry != sec->entries.end();
        ++entry )
    {
        if( entry->key == key )
        {
            sec->entries.erase( entry );
            return true;
        }
    }

    return false;
}

bool config_save( const config_t& config, const std::string& filename )
{
    CHECK( !filename.empty() );

    // Steps to ensure content of config file gets to disk:
    //
    // 1) Open and write to temp file (e.g. bt_config.conf.new).
    // 2) Flush the stream buffer to the temp file.
    // 3) Sync the temp file to disk with fsync().
    // 4) Rename temp file to actual config file (e.g. bt_config.conf).
    //    This ensures atomic update.
    // 5) Sync directory that has the conf file with fsync().
    //    This ensures directory entries are up-to-date.
    int dir_fd = -1;
    std::fstream file_stream;
    std::stringstream serialized;

    // Build temp config file based on config file (e.g. bt_config.conf.new).
    const std::string temp_filename = filename + ".new";

    // Extract directory from file path (e.g. /data/misc/bluedroid).
    const std::string directoryname = base::FilePath( filename ).DirName().AsUTF8Unsafe();
    if( directoryname.empty() )
    {
        LogUtilError() << __func__ << ": error extracting directory from '" << filename;
        goto error;
    }

    file_stream.open( temp_filename, std::ios::out | std::ios::trunc );
    if( !file_stream.is_open() )
    {
        LogUtilError() << __func__ << ": unable to write to file '" << temp_filename;
        goto error;
    }

    for( const section_t& section : config.sections )
    {
        serialized << "[" << section.name << "]" << std::endl;

        for( const entry_t& entry : section.entries )
            serialized << entry.key << " = " << entry.value << std::endl;

        serialized << std::endl;
    }

    file_stream << serialized.str();
    if( !file_stream.good() )
    {
        LogUtilError() << __func__ << ": unable to write to file '" << temp_filename;
        goto error;
    }

    file_stream.flush();
    file_stream.close();

    // Rename written temp file to the actual config file.
    remove( filename.c_str() );
    if( rename( temp_filename.c_str(), filename.c_str() ) == -1 )
    {
        LogUtilError() << __func__ << ": unable to commit file '" << filename;
        goto error;
    }
    return true;

error:
    // This indicates there is a write issue.  Unlink as partial data is not
    // acceptable.
    return false;
}

bool checksum_save( const std::string& checksum, const std::string& filename )
{
    return false;
}

static bool IsSpace( int c )
{
    if( c >= -1 && c <= 255 )
    {
        return isspace( c );
    }
    return false;
}

static char* trim( char* str )
{
    while( IsSpace( static_cast< unsigned char >( *str ) ) ) ++str;

    if( !*str ) return str;

    char* end_str = str + strlen( str ) - 1;

    while( end_str > str && IsSpace( *end_str ) ) --end_str;

    end_str[1] = '\0';
    return str;
}

static bool config_parse( std::fstream& a_file, config_t* config )
{
    int line_num = 0;
    char line[1024];
    char section[1024];
    strcpy_s( section, CONFIG_DEFAULT_SECTION );
    std::string temp_str;

    while( a_file.good() )
    {
        temp_str.clear();
        std::getline( a_file, temp_str );
        temp_str.push_back( '\n' );
        temp_str.push_back( 0x00 );
        memcpy( line, temp_str.c_str(), temp_str.size() );

        char* line_ptr = trim( line );
        ++line_num;

        // Skip blank and comment lines.
        if( *line_ptr == '\0' || *line_ptr == '#' ) continue;

        if( *line_ptr == '[' )
        {
            size_t len = strlen( line_ptr );
            if( line_ptr[len - 1] != ']' )
            {
                VLOG( 1 ) << __func__ << ": unterminated section name on line "
                    << line_num;
                return false;
            }
            strncpy_s( section, line_ptr + 1, len - 2 );
            section[len - 2] = '\0';
        }
        else
        {
            char* split = strchr( line_ptr, '=' );
            if( !split )
            {
                VLOG( 1 ) << __func__ << ": no key/value separator found on line "
                    << line_num;
                return false;
            }

            *split = '\0';
            config_set_string( config, section, trim( line_ptr ), trim( split + 1 ) );
        }
    }
    return true;
}

bool config_set_bin( config_t* config, const std::string& section, const std::string& key,
    const uint8_t* value, size_t length )
{
    const char* lookup = "0123456789abcdef";

    if( !value )
    {
        LogUtilError() << "no data to write";
        return false;
    }

    size_t max_value = ( ( size_t )-1 );
    if( ( ( max_value - 1 ) / 2 ) < length )
    {
        LogUtilError() << __func__ << ": length too long";
        return false;
    }

    std::vector<char> buffer;
    buffer.resize( length * 2 + 1 );
    char* str = buffer.data();

    for( size_t i = 0; i < length; ++i )
    {
        str[( i * 2 ) + 0] = lookup[( value[i] >> 4 ) & 0x0F];
        str[( i * 2 ) + 1] = lookup[value[i] & 0x0F];
    }


    config_set_string( config, section, key, str );
    return true;
}

bool config_get_bin( config_t* config, const std::string& section, const std::string& key,
    uint8_t* value, size_t* length )
{
    const std::string* value_str;
    const std::string* value_str_from_config =
        config_get_string( *config, section, key, NULL );

    if( !value_str_from_config )
    {
        LogUtilError() << __func__ << ": cannot find string for section " << section
            << ", key " << key;
        return false;
    }


    value_str = value_str_from_config;

    size_t value_len = value_str->length();
    if( ( value_len % 2 ) != 0 || *length < ( value_len / 2 ) )
    {
        LOG( WARNING ) << ": value size not divisible by 2, size is " << value_len;
        return false;
    }

    for( size_t i = 0; i < value_len; ++i )
    {
        if( !isxdigit( value_str->c_str()[i] ) )
        {
            LOG( WARNING ) << ": value is not hex digit";
            return false;
        }
    }

    const char* ptr = value_str->c_str();
    for( *length = 0; *ptr; ptr += 2, *length += 1 )
    {
        sscanf_s( ptr, "%02hhx", &value[*length] );
    }


    config_set_string( config, section, key, value_str->c_str() );

    return true;
}

/*******************************************************************************
 *
 * Function         btif_split_uuids_string
 *
 * Description      Internal helper function to split the string of UUIDs
 *                  read from the NVRAM to an array
 *
 * Returns          Number of UUIDs parsed from the supplied string
 *
 ******************************************************************************/
size_t btif_split_uuids_string( const char* str, bluetooth::uuid* p_uuid,
    size_t max_uuids )
{
    CHECK( str );
    CHECK( p_uuid );

    size_t num_uuids = 0;
    while( str && num_uuids < max_uuids )
    {
    }

    return num_uuids;
}
