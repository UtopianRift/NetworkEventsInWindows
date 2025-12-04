#pragma once

#include <Windows.h>
#include <fwpmu.h>
#include <stdexcept>

// RAII wrapper for FWPM_LAYER0* returned by FWPM APIs.
// Frees memory with FwpmFreeMemory0 on destruction.
class FwpmLayer
{
public:
    FwpmLayer() noexcept = default;
    explicit FwpmLayer(FWPM_LAYER0* p) noexcept : m_ptr(p) {}

    ~FwpmLayer()
    {
        reset();
    }

    FwpmLayer(const FwpmLayer&)            = delete;
    FwpmLayer& operator=(const FwpmLayer&) = delete;

    FwpmLayer(FwpmLayer&& other) noexcept : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    FwpmLayer& operator=(FwpmLayer&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    void reset(FWPM_LAYER0* p = nullptr) noexcept
    {
        if (m_ptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_ptr));
        }
        m_ptr = p;
    }

    FWPM_LAYER0* get() const noexcept { return m_ptr; }
    FWPM_LAYER0& operator*() const
    {
        if (!m_ptr) throw std::logic_error("FwpmLayer: null access");
        return *m_ptr;
    }
    FWPM_LAYER0* operator->() const noexcept { return m_ptr; }
    explicit operator bool() const noexcept { return m_ptr != nullptr; }

private:
    FWPM_LAYER0* m_ptr{};
};
