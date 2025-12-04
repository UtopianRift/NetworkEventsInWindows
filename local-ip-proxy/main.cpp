#include "SocketAddress.hpp"
#include "Event.hpp"
#include "NetEventCollectionGuard.hpp"
#include "WinSockSession.hpp"
#include "FwpmNetEventHeader.hpp"
#include "FwpmEngine.hpp"
#include "FwpmTransaction.hpp"
#include "FwpValue.hpp"

#include <fwpmu.h>
#include <fwptypes.h>
#include <ws2def.h>
#include <ipsectypes.h>
#include <fwpmtypes.h>

#include <Windows.h>

#include <iostream>
#include <string>
#include <string_view>
#include <cctype>
#include <ios>
#include <utility>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdint>
#include <list>
#include <condition_variable>
#include <stop_token>
#include <exception>
#include <memory>

#pragma comment(lib, "fwpuclnt.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "Ws2_32.lib")

struct RunConfig
{
    bool interactive = true;
};

static std::size_t
str_hash(
    const std::wstring& str
    )
{
    size_t h = 1469598103934665603ULL;

    for (std::wstring::value_type c : str)
    {
        h ^= static_cast<size_t>(c);
        h *= 1099511628211ULL;
    }

    return h;
}

static std::size_t
str_hash(
    const std::string& str
    )
{
    size_t h = 1469598103934665603ULL;

    for (std::string::value_type c : str)
    {
        h ^= static_cast<size_t>(c);
        h *= 1099511628211ULL;
    }

    return h;
}

struct EventKeyHasher
{
    size_t
    operator()(
        const EventKey& k
        ) const noexcept
    {
        // Simple hash combine
        size_t h = 1469598103934665603ULL;
        auto mix = [&] (auto v)
            {
                h ^= static_cast<size_t>(v);
                h *= 1099511628211ULL;
            };
        mix(str_hash(k.localSocket));
        mix(str_hash(k.remoteSocket));
        mix(k.protocol);
        mix(k.layerId);
        mix(std::to_underlying(k.type));
        mix(std::to_underlying(k.direction));
        mix(k.filterId);
        mix(str_hash(k.appName));

        return h;
    }
};

struct EventStats
{
    uint64_t count       = 0;
    uint64_t lastPrinted = 0;
};

struct Aggregator
{
    std::unordered_map<EventKey, EventStats, EventKeyHasher> map;
    std::mutex mtx;
};

static bool
promptYesNo(
    const std::string_view& prompt,
    char defaultAnswer
    )
{
    while (true)
    {
        std::cout << prompt;
        std::string line;

        if (
            !std::getline(
                std::cin,
                line
                )
            )
        {
            return false;
        }

        if (line.empty())
        {
            line = std::string(
                1,
                defaultAnswer
                );
        }

        auto c = static_cast<char>(::tolower(static_cast<unsigned char>(line[0])));

        if (c == 'y')
        {
            return true;
        }

        if (c == 'n')
        {
            return false;
        }
    }
}

static char
promptChoice(
    std::string_view prompt,
    std::string_view valid,
    char defaultAnswer
    )
{
    while (true)
    {
        std::cout << prompt;
        std::string line;

        if (
            !std::getline(
                std::cin,
                line
                )
            )
        {
            return defaultAnswer;
        }

        if (line.empty())
        {
            return defaultAnswer;
        }

        auto c = static_cast<char>(::tolower(static_cast<unsigned char>(line[0])));

        if (valid.contains(c))
        {
            return c;
        }
    }
}

static bool
probeAndNegotiateNetEvents(
    const RunConfig& config,
    NetEventState& state
    )
{
    try
    {
        auto globalEngine = FwpmEngine::acquireGlobal( );
    }
    catch( const std::exception& e )
    {
        std::cerr << "Failed to open the global FWPM Engine: " << e.what() << "\n";

        if (!config.interactive)
        {
            return false;
        }

        std::cout << "Unable to query net-event collection state. Continue anyway (events may not be available)?\n";

        if (
            !promptYesNo(
                "[Y]es / [N]o [Y]: ",
                'y'
                )
            )
        {
            return false;
        }

        state.engineCollectsNetEvents = false;
        state.weChangedOption         = false;

        return true;
    }

    FWP_VALUE0* pOpt = nullptr;

    DWORD status = FwpmEngineGetOption0(
        globalEngine,
        FWPM_ENGINE_COLLECT_NET_EVENTS,
        &pOpt
        );
    if ((status != ERROR_SUCCESS) || !pOpt || (pOpt->type != FWP_UINT32))
    {
        std::cerr << "Failed to read a global engine option: " << status << "\n";

        if (pOpt)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&pOpt));
        }

        if (!config.interactive)
        {
            return false;
        }

        std::cout << "Unable to determine net-event collection state. Continue anyway (events may be missing)?\n";

        if (
            !promptYesNo(
                "[Y]es / [N]o [Y]: ",
                'y'
                )
            )
        {
            return false;
        }

        state.engineCollectsNetEvents = false;
        state.weChangedOption         = false;

        return true;
    }

    state.engineCollectsNetEvents = (pOpt->uint32 != 0);
    state.originalValue           = pOpt->uint32;

    FwpmFreeMemory0(reinterpret_cast<void**>(&pOpt));

    if (state.engineCollectsNetEvents)
    {
        std::cout << "Net-event collection is already ENABLED.\n";
        state.weChangedOption = false;
        return true;
    }

    if (!config.interactive)
    {
        std::cerr
            << "ERROR: Net-event collection is DISABLED and --no-prompt was specified.\n"
            << "       Enable it manually or run without --no-prompt.\n";
        return false;
    }

    std::cout << "Net-event collection is DISABLED.\n"
              << "  [E]nable it now (system-wide, persistent)\n"
              << "  [C]ontinue without it (events may be missing)\n"
              << "  [Q]uit\n";

    char choice = promptChoice(
        "Choice [E/C/Q]: ",
        "ecq",
        'e'
        );

    if ((choice == 'q') || (choice == 'Q'))
    {
        return false;
    }

    if ((choice == 'c') || (choice == 'C'))
    {
        std::cout << "Continuing with net-event collection disabled.\n";
        state.weChangedOption = false;
        return true;
    }

    FWP_VALUE0 opt {};
    opt.type   = FWP_UINT32;
    opt.uint32 = 1;

    status = FwpmEngineSetOption0(
        engine,
        FWPM_ENGINE_COLLECT_NET_EVENTS,
        &opt
        );

    if (status != ERROR_SUCCESS)
    {
        std::cerr << "FwpmEngineSetOption0(COLLECT_NET_EVENTS=1) failed: " << status << "\n";
        return false;
    }

    std::cout << "Net-event collection enabled.\n";
    state.engineCollectsNetEvents = true;
    state.weChangedOption         = (state.originalValue == 0);

    return true;
}

static void CALLBACK
netEventCallback(
    void* context,
    const FWPM_NET_EVENT5* event
    )
{
    if (!event || !context)
    {
        return;
    }

    auto* agg       = static_cast<Aggregator*>(context);
    const auto& hdr = FwpmNetEventHeader {event->header};

    UINT32 layerId     = 0;
    UINT64 filterId    = 0;
    EventDirection dir = EventDirection::Unknown;
    EventType type     = EventType::Other;

    switch (event->type)
    {
        using enum EventType;
        case FWPM_NET_EVENT_TYPE_CLASSIFY_DROP:
            type = Drop;

            if (event->classifyDrop)
            {
                layerId  = event->classifyDrop->layerId;
                filterId = event->classifyDrop->filterId;

                switch (event->classifyDrop->msFwpDirection)
                {
                    using enum EventDirection;
                    case FWP_DIRECTION_INBOUND:
                        dir = Inbound;
                        break;

                    case FWP_DIRECTION_OUTBOUND:
                        dir = Outbound;
                        break;

                    default:
                        dir = Unknown;
                }
            }

            break;

        case FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW:
            type = Allow;

            if (event->classifyAllow)
            {
                layerId  = event->classifyAllow->layerId;
                filterId = event->classifyAllow->filterId;

                switch (event->classifyAllow->msFwpDirection)
                {
                    using enum EventDirection;
                    case FWP_DIRECTION_INBOUND:
                        dir = Inbound;
                        break;

                    case FWP_DIRECTION_OUTBOUND:
                        dir = Outbound;
                        break;

                    default:
                        dir = Unknown;
                }
            }

            break;

        default:
            type = Other;
            break;
    }

    std::string localSocket = "N/A";

    std::string remoteSocket = "N/A";

    std::wstring appPath = hdr.getAppPath();

    auto ipProtocol = (IPPROTO) hdr.ipProtocol;

    auto recordEvent = [&] ()
        {
            std::lock_guard lock(agg->mtx);

            auto [it, inserted] = agg->map.try_emplace(
                {localSocket, remoteSocket, ipProtocol, layerId, type, dir, filterId, appPath},
                EventStats{}
                );

            auto& stats = it->second;
            ++stats.count;
        };

    if (hdr.ipVersion == FWP_IP_VERSION_V4)
    {
        localSocket  = to_string({hdr.localAddrV4, hdr.localPort});
        remoteSocket = to_string({hdr.remoteAddrV4, hdr.remotePort});
    }

    if (hdr.ipVersion == FWP_IP_VERSION_V6)
    {
         localSocket = to_string({ hdr.localAddrV6, hdr.localPort });
         remoteSocket = to_string({ hdr.remoteAddrV6, hdr.remotePort });
    }

    recordEvent();
}

static void
doPrint(
    Aggregator* agg
    )
{
    std::vector<std::pair<EventKey, uint64_t>> changed;

    {
        std::lock_guard lock(agg->mtx);

        for (auto& [k, v] : agg->map)
        {
            if (v.count != v.lastPrinted)
            {
                changed.emplace_back(
                    k,
                    v.count
                    );
                v.lastPrinted = v.count;
            }
        }
    }

    if (changed.empty())
    {
        return;
    }

    for (const auto& [k, total] : changed)
    {
        std::cout << to_string(k);
        std::cout << "  (x" << total << ") ";
        std::cout.flush();

        std::wcout << k.appName;

        std::wcout.flush();

        std::cout << "\n";
    }

    std::cout.flush();
}

static void
RunPrinter(
    std::stop_token st,
    Aggregator* agg,
    std::chrono::seconds interval
    )
{
    std::mutex waitMtx;
    std::condition_variable_any cv;

    while (!st.stop_requested())
    {
        doPrint(agg);
        std::unique_lock ul(waitMtx);
        cv.wait_for(
            ul,
            st,
            interval,
            [] ()
            {
                return false;
            }
            );
    }
}

static RunConfig
parseCommandLine(
    const std::vector<std::string>& args
    )
{
    RunConfig cfg {};

    for (const auto& arg : args)
    {
        if (arg == "--no-prompt")
        {
            cfg.interactive = false;
        }
    }

    return cfg;
}

static const GUID g_SUBLAYER_GUID =
{
    0x6F606926,
    0x41DF,
    0x48E5,
    {0x8B, 0xB9, 0xE5, 0xCD, 0x15, 0x8A, 0xEA, 0xEA}
};

int
main(
    int argc,
    char** argv
    )
{

    Aggregator aggregator;
    try
    {
        std::vector<std::string> args;

        if (argc > 1)
        {
            args.reserve(static_cast<size_t>(argc - 1));

            for (int i = 1; i < argc; ++i)
            {
                args.emplace_back(argv[i]);
            }
        }

        RunConfig cfg = parseCommandLine(args);

        WinSockSession wsa;

        NetEventState netState {};
        if (
            !probeAndNegotiateNetEvents(
                cfg,
                netState
                )
            )
        {
            return 1;
        }

        NetEventCollectionGuard netEventsGuard(&netState);

        FwpmEngine tempEngine = FwpmEngine::acquireTemporary(L"Temporary FWPM Session");

        {
            FwpmTransaction txn = tempEngine.beginTransaction( );

            std::wstring sublayerName = L"IP Proxy Sublayer";
            std::wstring filterName = L"Inbound transport allow log";

            FWPM_SUBLAYER0 sublayer = {};
            sublayer.subLayerKey = g_SUBLAYER_GUID;
            sublayer.displayData.name = sublayerName.data( );
            sublayer.flags = 0;
            sublayer.weight = static_cast<UINT16>( 0x0100 );

            txn.addSubLayer(sublayer);

            FWPM_FILTER0 filter = {};
            filter.displayData.name = filterName.data( );
            filter.layerKey = FWPM_LAYER_INBOUND_TRANSPORT_V4;
            filter.subLayerKey = g_SUBLAYER_GUID;
            filter.action.type = FWP_ACTION_PERMIT;
            filter.weight.type = FWP_EMPTY;

            [[maybe_unused]]
            UINT64 filterId = txn.addFilter(filter);

            txn.commit( );
        }

        {
            HANDLE subscriptionHandle = nullptr;
            FWPM_NET_EVENT_SUBSCRIPTION0 sub = {};
            FWPM_NET_EVENT_ENUM_TEMPLATE0 tmpl = {};
            sub.enumTemplate = &tmpl;

            DWORD status = FwpmNetEventSubscribe4(
                tempEngine,
                &sub,
                netEventCallback,
                &aggregator,
                &subscriptionHandle
            );
            if( status != ERROR_SUCCESS )
            {
                std::cerr << "FwpmNetEventSubscribe4 failed: " << status << "\n";
                FwpmEngineClose0(tempEngine);

                return 1;
            }

            std::jthread printerThread(RunPrinter, &aggregator, std::chrono::seconds(10));

            std::wcout << L"WFP controller active. Press Enter to exit...\n";
            std::string line;
            std::getline(
                std::cin,
                line
                );

            printerThread.request_stop();
            printerThread.join();

            FwpmNetEventUnsubscribe0(tempEngine, subscriptionHandle);
        }

    }
    catch( const std::exception& e )
    {
        std::cerr << "The program terminated." << e.what( );
    }

    doPrint(&aggregator);

    return 0;
}