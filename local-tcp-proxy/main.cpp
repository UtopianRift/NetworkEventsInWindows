// local-tcp-proxy.cpp
// Simple hardened TCP port forwarder / proxy for Windows (IPv4)
//
// Usage:
//   local-tcp-proxy <listenPort> <targetIP> <targetPort>
//
// Example (Minecraft on same machine):
//   local-tcp-proxy 25566 127.0.0.1 25565

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <cstdlib>
#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <Windows.h>
#include <ws2def.h>
#include <stop_token>

#pragma comment(lib, "ws2_32.lib")

enum class ErrorClass
{
    None,
    NormalRemoteClose,    // recv() returned 0 (not actually an error)
    NetworkOrRemoteIssue, // NAT drop, remote crash, WIFI, firewall, etc.
    LocalProgrammingBug   // misuse of Winsock API or race in our code
};

void
logRawWSAError(
    const char* msg
    )
{
    std::cerr << msg << " (WSAGetLastError = " << WSAGetLastError() << ")\n";
}

ErrorClass
classifyWinsockError(
    int wsaError
    )
{
    switch (wsaError)
    {
        // Common “normal network failure” errors:
        case WSAECONNRESET:   // peer reset or middlebox RST
        case WSAETIMEDOUT:    // retransmits gave up
        case WSAECONNABORTED: // aborted by network or local stack
        case WSAENETRESET:
        case WSAENETDOWN:
        case WSAENETUNREACH:
        case WSAEHOSTUNREACH:
            return ErrorClass::NetworkOrRemoteIssue;

        // Errors that usually mean our own misuse / bug:
        case WSAEINVAL:
        case WSAENOTSOCK:
        case WSAEFAULT:
            return ErrorClass::LocalProgrammingBug;

        default:
            // Unknown / less common codes: treat as network-ish by default.
            return ErrorClass::NetworkOrRemoteIssue;
    }
}

static int
parsePort(
    const char* s,
    const char* what
    )
{
    char* end  = nullptr;
    long value = std::strtol(
        s,
        &end,
        10
        );

    if ((end == s) || (*end != '\0'))
    {
        std::cerr << "Invalid " << what << " '" << s << "' (not a number)\n";

        return -1;
    }

    if ((value < 1) || (value > 65535))
    {
        std::cerr << "Invalid " << what << " '" << s << "' (must be 1..65535)\n";

        return -1;
    }

    return static_cast<int>(value);
}

// Shared connection object
struct Connection
{
    SOCKET client {INVALID_SOCKET};
    SOCKET target {INVALID_SOCKET};

    // Ensure each direction only half-closes once.
    std::atomic<bool> clientSendShutdownDone {false};
    std::atomic<bool> targetSendShutdownDone {false};

    ~Connection()
    {
        // Final cleanup once all shared_ptr owners are gone.
        if (client != INVALID_SOCKET)
        {
            closesocket(client);
        }

        if (target != INVALID_SOCKET)
        {
            closesocket(target);
        }
    }
};

namespace
{

// Helper to handle send() errors and print messages, then return false to break the loop.
bool
handleSendError(
    int sent,
    const char* directionLabel
    )
{
    if (sent == SOCKET_ERROR)
    {
        switch (int err = WSAGetLastError(); classifyWinsockError(err))
        {
            case ErrorClass::NetworkOrRemoteIssue:
                std::cerr << directionLabel
                          << ": network/remote closed or failed during send() "
                          << "(WSA = " << err << ")\n";
                break;
            case ErrorClass::LocalProgrammingBug:
                std::cerr << directionLabel
                          << ": local programming / socket misuse during send() "
                          << "(WSA = " << err << ")\n";
                break;
            default:
                std::cerr << directionLabel
                          << ": unexpected send() error "
                          << "(WSA = " << err << ")\n";
                break;
        }

        return false;
    }

    return true;
}

} // namespace

static void
forward(
    std::stop_token /*stoken*/,
    std::shared_ptr<Connection> conn,
    bool clientToTarget,
    const char* directionLabel
    )
{
    std::string buffer(4096, '\0');

    SOCKET src = clientToTarget ? conn->client : conn->target;
    SOCKET dst = clientToTarget ? conn->target : conn->client;

    // Flag associated with "send side" of dst.
    std::atomic<bool>& shutdownFlag = clientToTarget ? conn->targetSendShutdownDone : conn->clientSendShutdownDone;

    while (true)
    {
        int bytes = recv(
            src,
            &buffer[0],
            static_cast<int>(buffer.size()),
            0
            );

        if (bytes == 0)
        {
            break;
        }
        else if (bytes < 0)
        {
            switch (int err = WSAGetLastError(); classifyWinsockError(err))
            {
                case ErrorClass::NetworkOrRemoteIssue:
                    std::cerr << directionLabel
                              << ": network/remote closed or failed during recv() "
                              << "(WSA = " << err << ")\n";
                    break;
                case ErrorClass::LocalProgrammingBug:
                    std::cerr << directionLabel
                              << ": local programming / socket misuse during recv() "
                              << "(WSA = " << err << ")\n";
                    break;
                default:
                    std::cerr << directionLabel
                              << ": unexpected recv() error "
                              << "(WSA = " << err << ")\n";
                    break;
            }

            break;
        }

        int sentTotal = 0;

        while (sentTotal < bytes)
        {
            int sent = send(
                dst,
                &buffer[0] + sentTotal,
                bytes - sentTotal,
                0
                );

            if (
                !handleSendError(
                    sent,
                    directionLabel
                    )
                )
            {
                goto done;
            }

            sentTotal += sent;
        }
    }

done:
    // Half-close our send side to dst exactly once per direction.
    bool expected = false;

    if (
        shutdownFlag.compare_exchange_strong(
            expected,
            true
            ) && (shutdown(
            dst,
            SD_SEND
            ) == SOCKET_ERROR)
        )
    {
        int err        = WSAGetLastError();
        ErrorClass cls = classifyWinsockError(err);

        // If it's a local bug, complain. If it's network-ish, it was dead anyway.
        if (cls == ErrorClass::LocalProgrammingBug)
        {
            std::cerr << directionLabel << ": shutdown(SD_SEND) local misuse? (WSA = " << err << ")\n";
        }
    }

    // NOTE:
    // We do NOT call closesocket() here.
    // Sockets are closed only in Connection::~Connection(),
    // which runs once both directions' threads have exited
    // and all std::shared_ptr<Connection> owners are gone.
}

int
main(
    int argc,
    char* argv[]
    )
{
    if (argc != 4)
    {
        std::cout << "Usage: local-tcp-proxy <listenPort> <targetIP> <targetPort>\n";

        return 1;
    }

    const char* listenPortStr = argv[1];
    const char* targetIP      = argv[2];
    const char* targetPortStr = argv[3];

    int listenPort = parsePort(
        listenPortStr,
        "listen port"
        );
    int targetPort = parsePort(
        targetPortStr,
        "target port"
        );

    if ((listenPort < 0) || (targetPort < 0))
    {
        return 1;
    }

    WSADATA wsaData {};

    if (
        int wsaResult = WSAStartup(
            MAKEWORD(
                2,
                2
                ),
            &wsaData
            ); (wsaResult != 0)
        )
    {
        std::cerr << "WSAStartup() failed: " << wsaResult << "\n";

        return 1;
    }

    SOCKET listener = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_TCP
        );

    if (listener == INVALID_SOCKET)
    {
        logRawWSAError("socket() failed for listener");
        WSACleanup();

        return 1;
    }

    // Allow quick restart of the program without "address already in use"
    {
        BOOL opt = TRUE;

        if (
            setsockopt(
                listener,
                SOL_SOCKET,
                SO_REUSEADDR,
                reinterpret_cast<const char*>(&opt),
                sizeof(opt)
                ) == SOCKET_ERROR
            )
        {
            logRawWSAError("setsockopt(SO_REUSEADDR) failed");
            // Not fatal; continue anyway.
        }
    }

    sockaddr_in listenAddr {};
    listenAddr.sin_family      = AF_INET;
    listenAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    listenAddr.sin_port        = htons(static_cast<u_short>(listenPort));

    if (
        bind(
            listener,
            (sockaddr*) &listenAddr,
            sizeof(listenAddr)
            ) == SOCKET_ERROR
        )
    {
        logRawWSAError("bind() failed for listener");
        closesocket(listener);
        WSACleanup();

        return 1;
    }

    if (
        listen(
            listener,
            SOMAXCONN
            ) == SOCKET_ERROR
        )
    {
        logRawWSAError("listen() failed on listener");
        closesocket(listener);
        WSACleanup();

        return 1;
    }

    // Pre-build target address template and validate the IP once
    sockaddr_in targetTemplate {};
    targetTemplate.sin_family = AF_INET;
    targetTemplate.sin_port   = htons(static_cast<u_short>(targetPort));

    if (
        int ptonResult = inet_pton(
            AF_INET,
            targetIP,
            &targetTemplate.sin_addr
            ); (ptonResult != 1)
        )
    {
        if (ptonResult == 0)
        {
            std::cerr << "inet_pton(): invalid target IP address string '"
                      << targetIP << "'\n";
        }
        else
        {
            logRawWSAError("inet_pton() failed");
        }

        closesocket(listener);
        WSACleanup();

        return 1;
    }

    std::cout << "local-tcp-proxy listening on port " << listenPort
              << ", forwarding to " << targetIP << ":" << targetPort << "\n";

    while (true)
    {
        SOCKET client = accept(
            listener,
            nullptr,
            nullptr
            );

        if (client == INVALID_SOCKET)
        {
            logRawWSAError("accept() failed");
            continue;
        }

        SOCKET target = socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP
            );

        if (target == INVALID_SOCKET)
        {
            logRawWSAError("socket() failed for target");
            closesocket(client);
            continue;
        }

        sockaddr_in targetAddr = targetTemplate; // copy template

        if (
            connect(
                target,
                (sockaddr*) &targetAddr,
                sizeof(targetAddr)
                ) == SOCKET_ERROR
            )
        {
            logRawWSAError("connect() to target failed");
            closesocket(client);
            closesocket(target);
            continue;
        }

        std::cout << "Connection established: client -> "
                  << targetIP << ":" << targetPort << "\n";

        // Create shared connection object
        auto conn = std::make_shared<Connection>();
        conn->client = client;
        conn->target = target;

        // Launch a new thread to handle the forwarding for this connection
        std::thread(
            [conn] ()
            {
                try
                {
                    std::jthread t1(
                        forward,
                        conn,
                        true,
                        "client->target"
                        );
                    std::jthread t2(
                        forward,
                        conn,
                        false,
                        "target->client"
                        );
                    t1.join();
                    t2.join();
                }
                catch (const std::exception& ex)
                {
                    std::cerr << "Forwarding thread exception: " << ex.what() << "\n";
                }
            }
            ).detach();

        // Main thread continues to accept new connections
    }

    // Not reached in current design, but correct in case you ever add a way to exit.
    closesocket(listener);
    WSACleanup();

    return 0;
}