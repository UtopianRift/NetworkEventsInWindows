// MIT License
// ansi_ui/include/ansi_ui/console_config.hpp
#pragma once

#include <cstdint>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace ansi_ui
{

    // RAII helper that configures the console for UTF-8 and ANSI/VT sequences
    // on construction and restores the previous state on destruction.
    class ConsoleConfig
    {
    public:
        ConsoleConfig()
        {
#if defined(_WIN32)
            // Cache handles
            hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);
            hIn_ = GetStdHandle(STD_INPUT_HANDLE);

            // Save original code pages
            origOutCp_ = GetConsoleOutputCP();
            origInCp_ = GetConsoleCP();

            // Try to switch to UTF-8 code page
            if (origOutCp_ != CP_UTF8)
            {
                if (SetConsoleOutputCP(CP_UTF8)) changedOutCp_ = true;
            }
            if (origInCp_ != CP_UTF8)
            {
                if (SetConsoleCP(CP_UTF8)) changedInCp_ = true;
            }

            // Save modes
            if (hOut_ && hOut_ != INVALID_HANDLE_VALUE && GetConsoleMode(hOut_, &origOutMode_))
            {
                DWORD desired = origOutMode_ | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                // DISABLE_NEWLINE_AUTO_RETURN can be helpful but may fail on older Windows
                DWORD withNoAutoReturn = desired | DISABLE_NEWLINE_AUTO_RETURN;
                if (!SetConsoleMode(hOut_, withNoAutoReturn))
                {
                    // Fallback: try without DISABLE_NEWLINE_AUTO_RETURN
                    if (SetConsoleMode(hOut_, desired))
                    {
                        changedOutMode_ = (desired != origOutMode_);
                    }
                }
                else
                {
                    changedOutMode_ = (withNoAutoReturn != origOutMode_);
                }
            }
            if (hIn_ && hIn_ != INVALID_HANDLE_VALUE && GetConsoleMode(hIn_, &origInMode_))
            {
                DWORD desired = origInMode_ | ENABLE_VIRTUAL_TERMINAL_INPUT;
                if (SetConsoleMode(hIn_, desired))
                {
                    changedInMode_ = (desired != origInMode_);
                }
            }
#else
            // On POSIX terminals UTF-8 and ANSI escapes are typically the default.
            // Intentionally do nothing here to avoid changing global locale.
#endif
        }

        ~ConsoleConfig()
        {
#if defined(_WIN32)
            // Restore modes
            if (changedOutMode_ && hOut_ && hOut_ != INVALID_HANDLE_VALUE)
            {
                SetConsoleMode(hOut_, origOutMode_);
            }
            if (changedInMode_ && hIn_ && hIn_ != INVALID_HANDLE_VALUE)
            {
                SetConsoleMode(hIn_, origInMode_);
            }
            // Restore code pages
            if (changedOutCp_)
            {
                SetConsoleOutputCP(origOutCp_);
            }
            if (changedInCp_)
            {
                SetConsoleCP(origInCp_);
            }
#endif
        }

        ConsoleConfig(const ConsoleConfig&) = delete;
        ConsoleConfig& operator=(const ConsoleConfig&) = delete;

    private:
#if defined(_WIN32)
        HANDLE hOut_{ INVALID_HANDLE_VALUE };
        HANDLE hIn_{ INVALID_HANDLE_VALUE };
        UINT   origOutCp_{ 0 };
        UINT   origInCp_{ 0 };
        DWORD  origOutMode_{ 0 };
        DWORD  origInMode_{ 0 };
        bool   changedOutCp_{ false };
        bool   changedInCp_{ false };
        bool   changedOutMode_{ false };
        bool   changedInMode_{ false };
#endif
    };

} // namespace ansi_ui
