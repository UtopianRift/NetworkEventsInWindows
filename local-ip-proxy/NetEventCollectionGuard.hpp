#pragma once
#include <Windows.h>
#include <fwptypes.h>
#include <fwpmtypes.h>
#include <fwpmu.h>

#include <iostream>

struct NetEventState
{
    bool engineCollectsNetEvents = false;
    bool weChangedOption = false;
    UINT32 originalValue = 0;
};

static void
restoreNetEventsIfNeeded(
    const NetEventState& state
) noexcept
{
    if( !state.weChangedOption )
    {
        return;
    }

    HANDLE engine = nullptr;
    DWORD status = FwpmEngineOpen0(
        nullptr,
        RPC_C_AUTHN_DEFAULT,
        nullptr,
        nullptr,
        &engine
    );

    if( status != ERROR_SUCCESS )
    {
        std::cerr << "Warning: could not re-open engine to restore net-event option: "
            << status << "\n";

        return;
    }

    FWP_VALUE0 opt{};
    opt.type = FWP_UINT32;
    opt.uint32 = state.originalValue;

    status = FwpmEngineSetOption0(
        engine,
        FWPM_ENGINE_COLLECT_NET_EVENTS,
        &opt
    );

    if( status != ERROR_SUCCESS )
    {
        std::cerr << "Warning: failed to restore net-event collection option: "
            << status << "\n";
    }

    FwpmEngineClose0(engine);
}

struct NetEventCollectionGuard
{
    NetEventState* state;

    explicit NetEventCollectionGuard(NetEventState* s)
        : state(s) {}

    ~NetEventCollectionGuard()
    {
        if (state && state->weChangedOption)
        {
            restoreNetEventsIfNeeded(*state);
        }
    }

    NetEventCollectionGuard(const NetEventCollectionGuard&) = delete;
    NetEventCollectionGuard& operator=(const NetEventCollectionGuard&) = delete;
};
