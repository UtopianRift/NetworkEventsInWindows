#pragma once

#include "FwpmLayer.hpp"

#include <fwpmu.h>
#include <fwpmtypes.h>
#include <Windows.h>

#include <stdexcept>
#include <string>
#include <format>
#include <fwptypes.h>

class FwpmRuntimeException : public std::runtime_error
{
public:
    FwpmRuntimeException(const std::string& msg, DWORD code)
        : std::runtime_error(msg), m_code(code)
    {
    }

    using runtime_error::runtime_error;

    DWORD code( ) const noexcept
    {
        return m_code;
    }

private:
    DWORD m_code;
};

class FwpmLogicException : public std::logic_error
{
public:
    using logic_error::logic_error;
};

class FwpmTransaction;

class FwpmEngine
{
public:

    ~FwpmEngine( )
    {
        if( *this )
        {
            FwpmEngineClose0(m_engine);
        }
        m_engine = nullptr;
    }

    FwpmEngine(const FwpmEngine&) = default;
    FwpmEngine& operator=(const FwpmEngine&) = default;

    FwpmEngine(FwpmEngine&& other) noexcept : m_engine(other.m_engine)
    {
        other.m_engine = nullptr;
    }

    FwpmEngine& operator=(FwpmEngine&& other) noexcept
    {
        m_engine = other.m_engine;
        other.m_engine = nullptr;
        return *this;
    }

    FwpmLayer getLayerById(UINT16 layerId)
    {
        FWPM_LAYER0* layer = nullptr;
        if( const DWORD s = FwpmLayerGetById0(m_engine, layerId, &layer); s != ERROR_SUCCESS )
        {
            throw FwpmRuntimeException(errorMsg("FwpmLayerGetById0", s), s);
        }
        return FwpmLayer(layer);
    }

    FwpmTransaction beginTransaction( );

    explicit operator bool( ) const
    {
        return m_engine != nullptr && m_engine != INVALID_HANDLE_VALUE;
    }

    static FwpmEngine acquireGlobal( )
    {
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
            throw FwpmRuntimeException("Construct new FwpmEngine", status);
        }

        return FwpmEngine(engine);
    }

    static FwpmEngine acquireTemporary(const std::wstring& sessionName)
    {
        FWPM_SESSION0 session = {};
        std::wstring tempSessionName{ sessionName };
        session.displayData.name = tempSessionName.data( );
        session.flags = FWPM_SESSION_FLAG_DYNAMIC;

        HANDLE engine = nullptr;
        DWORD status = FwpmEngineOpen0(
            nullptr,
            RPC_C_AUTHN_DEFAULT,
            nullptr,
            &session,
            &engine
        );
        if( status != ERROR_SUCCESS )
        {
            throw FwpmRuntimeException("Construct new FwpmEngine", status);
        }

        return FwpmEngine(engine);
    }

private:
    HANDLE m_engine{};
    std::wstring m_sessionName;

    explicit FwpmEngine(HANDLE engineHandle)
        : m_engine(engineHandle)
    {
        if( !m_engine )
        {
            throw FwpmRuntimeException("FwpmTransaction: null engine handle");
        }
    }

    static std::string errorMsg(const char* api, DWORD code)
    {
        return std::format("{} failed: {}", api, code);
    }

    explicit operator HANDLE( ) const
    {
        return m_engine;
    }
};
