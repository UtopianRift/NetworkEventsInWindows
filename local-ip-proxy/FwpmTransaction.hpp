#pragma once

#include "FwpmEngine.hpp"

#include <Windows.h>
#include <fwpmu.h>
#include <string>
#include <format>
#include <fwpmtypes.h>
#include <ipsectypes.h>
#include <utility>

class FwpmTransactionRuntimeException : public FwpmRuntimeException
{
public:
    using FwpmRuntimeException::FwpmRuntimeException;
};

class FwpmTransactionLogicException : public FwpmLogicException
{
public:
    using FwpmLogicException::FwpmLogicException;
};

class FwpmTransactionFinalizedException : public FwpmTransactionLogicException
{
public:
    using FwpmTransactionLogicException::FwpmTransactionLogicException;
};

/**
 * @brief Transaction manager for WFP (FWPM) transactions.
 * Implicitly begins a transaction on construction. Transactions are explicitly committed
 * or aborted (no changes applied) using member functions `void commit(void)` and
 * `void abort(void)` respectively. If not explicitly committed or aborted, then the
 * destructor implicitly aborts the transaction (no changes applied). Once committed or
 * aborted explicitly or implicitly any further mutating or observational calls throw.
 */
class FwpmTransaction
{
public:
    enum class State {
        Aborted, Active, Committed
    };

    FwpmTransaction(const FwpmTransaction&) = delete;
    FwpmTransaction& operator=(const FwpmTransaction&) = delete;

    FwpmTransaction(FwpmTransaction&& other) noexcept : m_engine{ std::move(other.m_engine) }, m_state{ other.m_state }
    {
        other.m_state = State::Aborted;
    }

    FwpmTransaction& operator=(FwpmTransaction&& other) noexcept
    {
        m_engine = std::move(other.m_engine);
        m_state = other.m_state;
        other.m_state = State::Aborted;
        return *this;
    }

    ~FwpmTransaction()
    {
        if (m_state == State::Active)
        {
            FwpmTransactionAbort0(m_engine);
            m_state = State::Aborted;
        }
    }

    State state() const noexcept { return m_state; }

    bool isFinalized() const noexcept { return m_state != State::Active; }

    void commit()
    {
        ensureActive("commit");
        if (const DWORD s = FwpmTransactionCommit0(m_engine); s != ERROR_SUCCESS)
        {
            abort( );
            throw FwpmTransactionRuntimeException(errorMsg("FwpmTransactionCommit0", s), s);
        }
        m_state = State::Committed;
    }

    void abort()
    {
        ensureActive("abort");
        if (const DWORD s = FwpmTransactionAbort0(m_engine); s != ERROR_SUCCESS)
        {
            abort( );
            throw FwpmTransactionRuntimeException(errorMsg("FwpmTransactionAbort0", s), s);
        }
        m_state = State::Aborted;
    }

    void addSubLayer(const FWPM_SUBLAYER0& sublayer)
    {
        ensureActive("addSubLayer");
        const DWORD s = FwpmSubLayerAdd0(m_engine, &sublayer, nullptr);
        if (s != ERROR_SUCCESS && s != FWP_E_ALREADY_EXISTS)
        {
            abort( );
            throw FwpmTransactionRuntimeException(errorMsg("FwpmSubLayerAdd0", s), s);
        }
    }

    UINT64 addFilter(const FWPM_FILTER0& filter)
    {
        ensureActive("addFilter");
        UINT64 id = 0;
        if (const DWORD s = FwpmFilterAdd0(m_engine, &filter, nullptr, &id); s != ERROR_SUCCESS)
        {
            abort( );
            throw FwpmTransactionRuntimeException(errorMsg("FwpmFilterAdd0", s), s);
        }
        return id;
    }

private:
    HANDLE m_engine;

    State  m_state{State::Aborted};

    static std::string errorMsg(const char* api, DWORD code)
    {
        return std::format("{} failed: {}", api, code);
    }

    void ensureActive(const char* what) const
    {
        if (m_state != State::Active)
        {
            throw FwpmTransactionFinalizedException(
                std::string("FwpmTransaction is finalized; cannot ") + what
            );
        }
    }

    friend FwpmEngine;

    explicit FwpmTransaction(const HANDLE& engine) : m_engine{ engine }
    {
        if( !m_engine )
        {
            throw FwpmTransactionLogicException("FwpmTransaction: null engine handle");
        }
        if( const DWORD s = FwpmTransactionBegin0(m_engine, 0); s != ERROR_SUCCESS )
        {
            throw FwpmTransactionRuntimeException(errorMsg("FwpmTransactionBegin0", s), s);
        }
        m_state = State::Active;
    }
};

inline FwpmTransaction FwpmEngine::beginTransaction( )
{
    return FwpmTransaction{m_engine};
}