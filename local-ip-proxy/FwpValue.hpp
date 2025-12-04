#include <fwptypes.h>
#include <fwpmu.h>
#include <Windows.h>

struct FwpValuePtr
{
    ~FwpValuePtr( )
    {
        // Release any resource a FWP_VALUE0 might have
        if ( m_p->type == FWP_BYTE_ARRAY16_TYPE && m_p->byteArray16 != nullptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_p->byteArray16));
        }
        else if ( m_p->type == FWP_BYTE_ARRAY6_TYPE && m_p->byteArray6 != nullptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_p->byteArray6));
        }
        else if ( m_p->type == FWP_BYTE_BLOB_TYPE && m_p->byteBlob != nullptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_p->byteBlob));
        }
        else if ( m_p->type == FWP_SID && m_p->sid != nullptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_p->sid));
        }
        else if ( m_p->type == FWP_SECURITY_DESCRIPTOR_TYPE && m_p->sd != nullptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_p->sd));
        }
        else if ( m_p->type == FWP_UNICODE_STRING_TYPE && m_p->unicodeString != nullptr)
        {
            FwpmFreeMemory0(reinterpret_cast<void**>(&m_p->unicodeString));
        }
        m_p->type = FWP_EMPTY;
        FwpmFreeMemory0(reinterpret_cast<void**>(&m_p));
    }

    FWP_DATA_TYPE type( ) const noexcept
    {
        return m_p->type;
    }

    bool isEmpty( ) const noexcept
    {
        return m_p->type == FWP_EMPTY;
    }

    UINT8 uint8( ) const noexcept
    {
        return m_p->uint8;
    }
    UINT16 uint16( ) const noexcept
    {
        return m_p->uint16;
    }
    UINT32 uint32( ) const noexcept
    {
        return m_p->uint32;
    }
    UINT64* uint64( ) const noexcept
    {
        return m_p->uint64;
    }
    INT8 int8( ) const noexcept
    {
        return m_p->int8;
    }
    INT16 int16( ) const noexcept
    {
        return m_p->int16;
    }
    INT32 int32( ) const noexcept
    {
        return m_p->int32;
    }
    INT64* int64( ) const noexcept
    {
        return m_p->int64;
    }
    float float32( ) const noexcept
    {
        return m_p->float32;
    }
    double* double64( ) const noexcept
    {
        return m_p->double64;
    }
    FWP_BYTE_ARRAY16* byteArray16( ) const noexcept
    {
        return m_p->byteArray16;
    }
    FWP_BYTE_BLOB* byteBlob( ) const noexcept
    {
        return m_p->byteBlob;
    }
    SID* sid( ) const noexcept
    {
        return m_p->sid;
    }
    FWP_BYTE_BLOB* sd( ) const noexcept
    {
        return m_p->sd;
    }
    FWP_TOKEN_INFORMATION* tokenInformation( ) const noexcept
    {
        return m_p->tokenInformation;
    }
    FWP_BYTE_BLOB* tokenAccessInformation( ) const noexcept
    {
        return m_p->tokenAccessInformation;
    }
    LPWSTR unicodeString( ) const noexcept
    {
        return m_p->unicodeString;
    }
    FWP_BYTE_ARRAY6* byteArray6( ) const noexcept
    {
        return m_p->byteArray6;
    }

private:
    FWP_VALUE0* m_p;
};
