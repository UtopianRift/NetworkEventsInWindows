#pragma once

#include <istream>
#include <ostream>
#include <iostream>
#include <cinttypes>
#include <iterator>

namespace vega_alpha::util::utf_8
{
    class UTF8
    {
        /**
        * @brief Returns the number of bytes in a codepoint based on its first byte.
        * @param byte This is the first byte of a codepoint.
        * @return The number of bytes to expect in a codepoint, or 0 if the given
        * byte is not a valid first character.
        */
        inline constexpr uint8_t static expectedByteCount(uint8_t byte)
        {
            // Determine the number of bytes in the UTF-8 character
            if((byte & 0x80) == 0)
            {
                // Single-byte character
                return 1;
            }
            else if((byte & 0xE0) == 0xC0)
            {
                // Two-byte character
                return 2;
            }
            else if((byte & 0xF0) == 0xE0)
            {
                // Three-byte character
                return 3;
            }
            else if((byte & 0xF8) == 0xF0)
            {
                // Four-byte character
                return 4;
            }

            return 0;
        }

        /**
         * @brief Recovers from an encounter with an invalid byte by moving the input_iterator
         * over the invalid bytes of the current codepoint.
         * @param codepoint These are the bits for the next codepoint gathered before the
         *                  encounter with the invalid byte.
         * @param it        This is the input_iterator positioned on the invalid byte.
         * @return The bits comprising an invalid codepoint.
         */
        uint32_t static recoverFromError(uint32_t codepoint, std::istream_iterator<char> &it)
        {
            if(it == std::istream_iterator<char>{})
                return codepoint;

            uint8_t byte = static_cast<uint8_t>(*it++);
            codepoint = (codepoint << 6) | (byte & 0x3F);
            while(it != std::istream_iterator<char>{} && ((byte & 0xC0) != 0x80))
            {
                byte = static_cast<uint8_t>(*it++);
                codepoint = (codepoint << 6) | (byte & 0x3F);
            }
            return codepoint;
        }

        inline constexpr bool static isValidContinuationByte(uint8_t byte)
        {
            return (byte & 0xC0) == 0x80;
        }

    public:
        /**
        * @brief List of error codes returned from parsing UTF-8.
        */
        enum class ResultCodes
        {
            // No error
            SUCCESS,
            // The end of the iteration was reached in the middle of a codepoint.
            INCOMPLETE_SEQUENCE,
            // The input_iterator was not positioned on a valid first character of a codepoint.
            INVALID_FIRST_BYTE,
            // A byte expected to be a continuation does not have a valid bit pattern.
            INVALID_CONTINUATION_BYTE
        };

        using ResultId = ResultCodes;

        /**
        * @brief holds information about the state of the UTF-8 parser.
        */
        struct Result
        {
            uint32_t codepoint;
            ResultId error_code;
        };

        /**
         * @brief Returns a UTF-8 codepoint by parsing byte sequences in an input_iterator.
         * @param input_iterator This is the iterator where bytes are accepted.
         * @param result The fields of this object are assigned to the results of the parse.
         */
        uint32_t static nextCodepoint(std::istream_iterator<char> &input_iterator, Result &result)
        {
            // Check for end of iteration
            if(input_iterator == std::istream_iterator<char>{})
            {
                // Reaching the end of the iteration is not an error.
                result.codepoint = 0;
                result.error_code = ResultCodes::SUCCESS;
                return 0;
            }

            uint32_t codepoint = 0;

            // Read the first byte
            uint8_t firstByte = static_cast<uint8_t>(*input_iterator++);

            // Determine the number of bytes in the UTF-8 character
            int remainingBytes = expectedByteCount(firstByte);

            switch(remainingBytes)
            {
            case 1:
                result.codepoint = firstByte;
                result.error_code = ResultCodes::SUCCESS;
                return firstByte;

            case 2:
                codepoint = firstByte & 0x1F; // Last 5 bits
                break;

            case 3:
                codepoint = firstByte & 0x0F; // Last 4 bits
                break;

            case 4:
                codepoint = firstByte & 0x07; // Last 3 bits
                break;

            default:
                // Invalid first byte of a UTF-8 sequence
                result.codepoint = recoverFromError(firstByte, input_iterator);
                result.error_code = ResultCodes::INVALID_FIRST_BYTE;
                return 0;
            }

            // Read the continuation bytes and compute the codepoint
            for(int i = 1; i < remainingBytes; ++i)
            {
                std::cout << remainingBytes << std::endl;

                if(input_iterator == std::istream_iterator<char>{})
                {
                    // Incomplete UTF-8 byte sequence
                    result.codepoint = codepoint;
                    result.error_code = ResultCodes::INCOMPLETE_SEQUENCE;
                    return 0;
                }

                uint8_t byte = static_cast<uint8_t>(*input_iterator++);
                if(!isValidContinuationByte(byte))
                {
                    // Invalid continuation byte.
                    result.codepoint = recoverFromError(codepoint, input_iterator);
                    result.error_code = ResultCodes::INVALID_CONTINUATION_BYTE;
                    return 0;
                }
                // Shift the bits currently in the codepoint left six bits and set the
                // lower six bits of the codepoint to the lower six of the current byte.
                codepoint = (codepoint << 6) | (byte & 0x3F);
            }

            result.error_code = ResultCodes::SUCCESS;
            result.codepoint = codepoint;
            return codepoint;
        }

        static_assert(sizeof(wchar_t) == 2);
    };
} // namespace vega_alpha::util::utf_8