#pragma once

#include <WinSock2.h>
#include <Windows.h>
#include <fwptypes.h>

#include <string>
#include <sstream>
#include <utility>
#include <version>
#include <format>
#include <ostream>

inline std::string to_string(const std::pair<UINT32, UINT16>& v4)
{
    const UINT32 addr = v4.first;
    const UINT16 port = v4.second;
    std::ostringstream oss;
    oss << ((addr >> 24) & 0xFF) << '.'
        << ((addr >> 16) & 0xFF) << '.'
        << ((addr >>  8) & 0xFF) << '.'
        << ((addr >>  0) & 0xFF)
        << ':' << port;
    return oss.str();
}

inline std::string to_string(const std::pair<FWP_BYTE_ARRAY16, UINT16>& v6)
{
    const FWP_BYTE_ARRAY16& addr = v6.first;
    const UINT16 port = v6.second;
#if __cpp_lib_format >= 201907L
    // Use std::format if available (C++20)
    std::string result = "[";
    for (int i = 0; i < 16; i += 2)
    {
        if (i > 0) result += ':';
        result += std::format("{:04x}", (static_cast<unsigned>(addr.byteArray16[i]) << 8) | addr.byteArray16[i + 1]);
    }
    result += std::format("]:{}", port);
    return result;
#elif defined(__cpp_lib_print) && __cpp_lib_print >= 202207L
    // Use std::print if available (C++23)
    std::string result = "[";
    for (int i = 0; i < 16; i += 2)
    {
        if (i > 0) result += ':';
        char buf[5];
        std::snprintf(buf, sizeof(buf), "%04x", (static_cast<unsigned>(addr.byteArray16[i]) << 8) | addr.byteArray16[i + 1]);
        result += buf;
    }
    char portbuf[16];
    std::snprintf(portbuf, sizeof(portbuf), "]:%u", port);
    result += portbuf;
    return result;
#else
    // Fallback for pre-C++20: use std::ostringstream, but without manipulators
    std::string result = "[";
    for (int i = 0; i < 16; i += 2)
    {
        if (i > 0) result += ':';
        unsigned value = (static_cast<unsigned>(addr.byteArray16[i]) << 8) | addr.byteArray16[i + 1];
        char buf[5];
        std::snprintf(buf, sizeof(buf), "%04x", value);
        result += buf;
    }
    char portbuf[16];
    std::snprintf(portbuf, sizeof(portbuf), "]:%u", port);
    result += portbuf;
    return result;
#endif
}
