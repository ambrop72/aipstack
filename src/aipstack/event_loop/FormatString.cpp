/*
 * Copyright (c) 2018 Ambroz Bizjak
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
#include <cstdarg>
#include <cstddef>
#include <string>
#include <stdexcept>
#include <limits>
#include <type_traits>

#include <aipstack/misc/MinMax.h>
#include <aipstack/misc/Pragma.h>
#include <aipstack/event_loop/FormatString.h>

namespace AIpStack {

static std::size_t const FormatSizeHint = 25;

std::string formatString (char const *fmt, ...)
{
    std::size_t fmt_len = std::strlen(fmt);
    if (fmt_len > TypeMax<std::size_t>() - FormatSizeHint) {
        throw std::bad_alloc();
    }
    std::size_t initial_size = fmt_len + FormatSizeHint;
    
    std::string str(initial_size, '\0');

    while (true) {
        if (str.size() > TypeMax<int>()) {
            throw std::bad_alloc();
        }

        std::va_list args;
        va_start(args, fmt);

        AIPSTACK_GCC_DIAGNOSTIC_START("GCC diagnostic ignored \"-Wformat-nonliteral\"")
        auto print_res = std::vsnprintf(&str[0], str.size(), fmt, args);
        AIPSTACK_GCC_DIAGNOSTIC_END
        
        va_end(args);

        if (print_res < 0) {
            throw std::runtime_error("vsnprintf failed");
        }

        auto print_bytes = std::make_unsigned_t<decltype(print_res)>(print_res);

        if (print_bytes < str.size()) {
            str.resize(print_bytes);
            break;
        }

        // Get the common type of print_bytes and size_t, to avoid a warning
        // in the check below.
        using CT = decltype(false ? print_bytes : std::size_t());

        if (CT(print_bytes) > CT(TypeMax<std::size_t>() - 1)) {
            throw std::bad_alloc();
        }

        str.resize(print_bytes + 1);
    }

    return str;
}

}
