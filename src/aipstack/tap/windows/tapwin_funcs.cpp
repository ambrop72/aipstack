/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>

#include <windows.h>

#include <aipstack/misc/Assert.h>
#include <aipstack/misc/NonCopyable.h>

#include "tapwin_common.h"
#include "tapwin_funcs.h"

#define TAPWIN32_MAX_REG_SIZE 256

namespace AIpStack {

static bool split_string (std::string const &str, char const *sep,
                          std::size_t num_fields, std::vector<std::string> &out)
{
    AIPSTACK_ASSERT(num_fields > 0);
    AIPSTACK_ASSERT(std::strlen(sep) > 0);
    
    std::size_t str_pos = 0;
    
    for (std::size_t i = 0; i < num_fields - 1; i++) {
        std::size_t sep_pos = str.find(sep, str_pos);
        if (sep_pos == std::string::npos) {
            return false;
        }
        
        out.push_back(str.substr(str_pos, sep_pos - str_pos));
        
        str_pos = sep_pos + 1;
    }
    
    out.push_back(str.substr(str_pos));
    
    return true;
}

bool tapwin_parse_tap_spec (std::string const &name,
                            std::string &out_component_id,
                            std::string &out_human_name)
{
    std::vector<std::string> fields;
    if (!split_string(name, ":", 2, fields)) {
        return false;
    }
    out_component_id = std::move(fields[0]);
    out_human_name = std::move(fields[1]);
    return true;
}

class HkeyWrapper :
    private AIpStack::NonCopyable<HkeyWrapper>
{
private:
    bool m_have;
    HKEY m_hkey;
    
public:
    HkeyWrapper () :
        m_have(false)
    {}
    
    ~HkeyWrapper ()
    {
        if (m_have) {
            AIPSTACK_ASSERT_FORCE(RegCloseKey(m_hkey) == ERROR_SUCCESS);
        }
    }
    
    HKEY get () const
    {
        AIPSTACK_ASSERT(m_have);
        
        return m_hkey;
    }
    
    template<typename Func>
    bool open (Func func)
    {
        AIPSTACK_ASSERT(!m_have);
        
        bool res = func(&m_hkey);
        if (res) {
            m_have = true;
        }
        return res;
    }
    
    bool open (HKEY parent, char const *subkey, DWORD options, REGSAM sam)
    {
        return open([&](HKEY *out) {
            return RegOpenKeyExA(parent, subkey, options, sam, out) == ERROR_SUCCESS;
        });
    }
};

bool tapwin_find_device (std::string const &device_component_id,
                         std::string const &device_name,
                         std::string &out_device_path)
{
    // open adapter key
    // used to find all devices with the given ComponentId
    HkeyWrapper adapter_key;
    if (!adapter_key.open(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ)) {
        return false;
    }
    
    char net_cfg_instance_id[TAPWIN32_MAX_REG_SIZE];
    bool found = false;
    
    DWORD i;
    for (i = 0;; i++) {
        DWORD len;
        DWORD type;
        
        char key_name[TAPWIN32_MAX_REG_SIZE];
        len = sizeof(key_name);
        if (RegEnumKeyExA(adapter_key.get(), i, key_name, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) {
            break;
        }
        
        std::string unit_string = std::string(ADAPTER_KEY) + "\\" + key_name;
        HkeyWrapper unit_key;
        if (!unit_key.open(HKEY_LOCAL_MACHINE, unit_string.c_str(), 0, KEY_READ)) {
            continue;
        }
        
        char component_id[TAPWIN32_MAX_REG_SIZE];
        len = sizeof(component_id);
        if (RegQueryValueExA(unit_key.get(), "ComponentId", NULL,
                             &type, (LPBYTE)component_id, &len) != ERROR_SUCCESS ||
            type != REG_SZ)
        {
            continue;
        }
        
        len = sizeof(net_cfg_instance_id);
        if (RegQueryValueExA(unit_key.get(), "NetCfgInstanceId", NULL,
                             &type, (LPBYTE)net_cfg_instance_id, &len) != ERROR_SUCCESS ||
            type != REG_SZ)
        {
            continue;
        }
        
        // check if ComponentId matches
        if (device_component_id == component_id) {
            // if no name was given, use the first device with the given ComponentId
            if (device_name.empty()) {
                found = true;
                break;
            }
            
            // open connection key
            std::string conn_string = std::string(NETWORK_CONNECTIONS_KEY) +
                "\\" + net_cfg_instance_id + "\\Connection";
            HkeyWrapper conn_key;
            if (!conn_key.open(HKEY_LOCAL_MACHINE, conn_string.c_str(), 0, KEY_READ)) {
                continue;
            }
            
            // read name
            char name[TAPWIN32_MAX_REG_SIZE];
            len = sizeof(name);
            if (RegQueryValueExA(conn_key.get(), "Name", NULL,
                                 &type, (LPBYTE)name, &len) != ERROR_SUCCESS ||
                type != REG_SZ)
            {
                continue;
            }
            
            // check name
            if (device_name == name) {
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        return false;
    }
    
    out_device_path = std::string("\\\\.\\Global\\") + net_cfg_instance_id + ".tap";
    
    return true;
}

}
