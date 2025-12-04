#pragma once

#include <istream>
#include <iterator>
#include <string>
#include <cstdint>
#include <cstdio>

namespace vega_alpha::util::utf_16
{
    class UTF16
    {
        inline static constexpr bool isHighSurrogate(uint16_t u) noexcept
        {
            return u >= 0xD800 && u <= 0xDBFF;
        }

        inline static constexpr bool isLowSurrogate(uint16_t u) noexcept
        {
            return u >= 0xDC00 && u <= 0xDFFF;
        }

        inline static constexpr uint32_t combineSurrogates(uint16_t high, uint16_t low) noexcept
        {
            // U' = (H - 0xD800) << 10 | (L - 0xDC00) + 0x10000
            return (static_cast<uint32_t>(high - 0xD800) << 10)
                 | (static_cast<uint32_t>(low  - 0xDC00))
                 | 0x10000u;
        }

    public:
        enum class ResultCodes
        {
            SUCCESS,
            INCOMPLETE_PAIR,
            INVALID_HIGH_SURROGATE,
            INVALID_LOW_SURROGATE
        };
        using ResultId = ResultCodes;

        struct Result
        {
            uint32_t codepoint = 0;
            ResultId error_code = ResultCodes::SUCCESS;
        };

        // Reads the next UTF-16 codepoint from a stream of raw bytes containing UTF-16LE data.
        // The iterator should yield bytes; two bytes are consumed per code unit.
        static uint32_t nextCodepoint(std::istream_iterator<char>& it, Result& result)
        {
            // Need at least one 16-bit unit (2 bytes)
            if (it == std::istream_iterator<char>{})
            {
                result.codepoint = 0;
                result.error_code = ResultCodes::SUCCESS;
                return 0;
            }

            // Read first 16-bit unit (UTF-16LE)
            uint8_t b0 = static_cast<uint8_t>(*it++);
            if (it == std::istream_iterator<char>{})
            {
                // Truncated 16-bit unit
                result.codepoint = b0;
                result.error_code = ResultCodes::INCOMPLETE_PAIR;
                return 0;
            }
            uint8_t b1 = static_cast<uint8_t>(*it++);
            uint16_t u1 = static_cast<uint16_t>(b0 | (b1 << 8));

            if (!isHighSurrogate(u1))
            {
                if (isLowSurrogate(u1))
                {
                    // Low surrogate without preceding high surrogate
                    result.codepoint = u1;
                    result.error_code = ResultCodes::INVALID_LOW_SURROGATE;
                    return 0;
                }
                // BMP scalar
                result.codepoint = u1;
                result.error_code = ResultCodes::SUCCESS;
                return result.codepoint;
            }

            // High surrogate: must have following low surrogate
            // Need two more bytes
            if (it == std::istream_iterator<char>{})
            {
                result.codepoint = u1;
                result.error_code = ResultCodes::INCOMPLETE_PAIR;
                return 0;
            }
            uint8_t c0 = static_cast<uint8_t>(*it++);
            if (it == std::istream_iterator<char>{})
            {
                result.codepoint = u1;
                result.error_code = ResultCodes::INCOMPLETE_PAIR;
                return 0;
            }
            uint8_t c1 = static_cast<uint8_t>(*it++);
            uint16_t u2 = static_cast<uint16_t>(c0 | (c1 << 8));

            if (!isLowSurrogate(u2))
            {
                // Invalid continuation after high surrogate
                result.codepoint = (static_cast<uint32_t>(u1) << 16) | u2; // pack for diagnostics
                result.error_code = ResultCodes::INVALID_HIGH_SURROGATE;
                return 0;
            }

            result.codepoint = combineSurrogates(u1, u2);
            result.error_code = ResultCodes::SUCCESS;
            return result.codepoint;
        }

        // Helper to format a 16-bit unit as hex
        static std::string hex16(uint16_t u)
        {
            char buf[7]{}; // "0xFFFF"
            std::snprintf(buf, sizeof(buf), "0x%04X", static_cast<unsigned>(u));
            return std::string(buf);
        }
    };
} // namespace vega_alpha::util::utf_16
