#include <Windows.h>
#include <WinSock2.h>

// RAII guard for WinSock initialization/cleanup
class WinSockSession
{
    bool ok{ false };
    int err{ 0 };

public:

    explicit WinSockSession(
        WORD version = MAKEWORD(2, 2)
    )
    {
        WSADATA data{};
        err = WSAStartup(
            version,
            &data
        );
        ok = ( err == 0 );
    }

    ~WinSockSession( )
    {
        if( ok )
        {
            WSACleanup( );
        }
    }

    bool
        isOk( ) const noexcept
    {
        return ok;
    }

    int
        errorCode( ) const noexcept
    {
        return err;
    }

    explicit
        operator bool( ) const noexcept
    {
        return ok;
    }

    WinSockSession(
        const WinSockSession&
    ) = delete;

    WinSockSession&
        operator=(
            const WinSockSession&
            ) = delete;
};
