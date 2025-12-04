#pragma once

#include <WinSock2.h>
#include <fwpmu.h>
#include <fwpmtypes.h>
#include <ws2def.h>
#include <fwptypes.h>

#include <string>
#include <cstdint>
#include <sstream>
#include <format>
#include <ostream>
#include <unordered_map>
#include <utility>

enum class EventDirection : std::uint8_t
{
    Unknown  = 0,
    Inbound  = 1,
    Outbound = 2
};

enum class EventType : std::uint8_t
{
    Other = 0,
    Drop  = 1,
    Allow = 2
};

struct EventKey
{
    std::string localSocket;
    std::string remoteSocket;
    IPPROTO protocol;
    std::uint32_t layerId;
    EventType type;
    EventDirection direction;
    std::uint64_t filterId;
    std::wstring appName;

    bool
    operator==(
        const EventKey& o
        ) const noexcept = default;
};

inline std::string
to_string(
    EventType t
    )
{
    switch (t)
    {
        case EventType::Drop: return "DROP";
        case EventType::Allow: return "ALLOW";
        default: return "OTHER";
    }
}

inline std::string
to_string(
    const EventDirection& d
    )
{
    switch (d)
    {
        case EventDirection::Inbound: return "IN";
        case EventDirection::Outbound: return "OUT";
        default: return "UNK";
    }
}

static std::string
layerIdToName(
    UINT32 layerId
    )
{
    static std::unordered_map<UINT32, std::string> cache;

    if (auto it = cache.find(layerId); (it != cache.end()))
    {
        return it->second;
    }

    HANDLE engine = nullptr;
    DWORD status  = FwpmEngineOpen0(
        nullptr,
        RPC_C_AUTHN_DEFAULT,
        nullptr,
        nullptr,
        &engine
        );

    if (status != ERROR_SUCCESS)
    {
        return std::format(
            "LAYER_{}",
            layerId
            );
    }

    FWPM_LAYER0* layer = nullptr;
    status = FwpmLayerGetById0(
        engine,
        layerId,
        &layer
        );

    if ((status != ERROR_SUCCESS) || !layer)
    {
        FwpmEngineClose0(engine);

        return std::format(
            "LAYER_{}",
            layerId
            );
    }

    std::string name;

    if (layer->displayData.name)
    {
        char buf[256] {};
        int n = WideCharToMultiByte(
            CP_UTF8,
            0,
            layer->displayData.name,
            -1,
            buf,
            static_cast<int>(sizeof(buf)),
            nullptr,
            nullptr
            );

        if (n > 0)
        {
            name.assign(buf);
        }
        else
        {
            name = "<unknown-layer>";
        }
    }
    else
    {
        name = std::format(
            "LAYER_{}",
            layerId
            );
    }

    cache.try_emplace(
        layerId,
        name
        );
    FwpmFreeMemory0(reinterpret_cast<void**>(&layer));
    FwpmEngineClose0(engine);

    return name;
}

inline static std::string
to_string(
    const IPPROTO& ipProto
    )
{
    switch (ipProto)
    {
        case IPPROTO_TCP: return "TCP";
        case IPPROTO_UDP: return "UDP";
        case IPPROTO_ICMP: return "ICMP";
        default:
            return std::format(
                "IP{}",
                (UINT8) ipProto
                );
            break;
    }
}

inline std::string
to_string(
    const EventKey& k
    )
{
    std::stringstream out;
    out
        << "[" << to_string(k.type) << "]"
        << "[" << layerIdToName(k.layerId) << "]"
        << "[" << to_string(k.protocol) << "]"
        << "[" << to_string(k.direction) << "] "
        << k.localSocket << " -> " << k.remoteSocket;

    return out.str();
}