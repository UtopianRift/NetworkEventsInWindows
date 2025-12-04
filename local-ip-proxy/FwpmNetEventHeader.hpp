
#include "UTF16.hpp"

#include <fwpmtypes.h>

#include <string>
#include <iterator>
#include <sstream>

struct FwpmNetEventHeader : FWPM_NET_EVENT_HEADER3
{
    std::wstring
        getAppPath() const
    {
        if( !appId.data || ( appId.size == 0 ) )
        {
            return L"<unknown>";
        }

        const auto* bytes = static_cast<const uint8_t*>( appId.data );
        const size_t byteCount = appId.size;

        std::wstring out;
        out.reserve(byteCount / 2);

        auto emitBreak = [ &out ]( )
        {
            out.append(L" \uFFFD| "); // " �| "
        };
        auto emitBreakEnd = [ &out ]( )
        {
            out.append(L" |\uFFFD "); // " |� "
        };

        std::string data(reinterpret_cast<const char*>( bytes ), byteCount);
        std::istringstream iss(data);
        std::istream_iterator<char> it(iss);
        std::istream_iterator<char> end;

        while( it != end )
        {
            vega_alpha::util::utf_16::UTF16::Result res{};
            uint32_t cp = vega_alpha::util::utf_16::UTF16::nextCodepoint(
                it,
                res
            );

            using RC = vega_alpha::util::utf_16::UTF16::ResultCodes;

            if( res.error_code == RC::SUCCESS )
            {
                if( cp <= 0xFFFF )
                {
                    out.push_back(static_cast<wchar_t>( cp ));
                }
                else
                {
                    uint32_t v = cp - 0x10000U;
                    auto high = static_cast<uint16_t>( 0xD800U + ( v >> 10 ) );
                    auto low = static_cast<uint16_t>( 0xDC00U + ( v & 0x3FFU ) );
                    out.push_back(static_cast<wchar_t>( high ));
                    out.push_back(static_cast<wchar_t>( low ));
                }

                continue;
            }

            // Error formatting consistent with previous implementation
            switch( res.error_code )
            {
            case RC::INCOMPLETE_PAIR:
                {
                    emitBreak( );
                    wchar_t hex_buf_w[ 7 ]{};
                    std::swprintf(
                        hex_buf_w,
                        7,
                        L"0x%04X",
                        static_cast<unsigned>( res.codepoint & 0xFFFF )
                    );
                    out.append(hex_buf_w);
                    emitBreakEnd( );
                    // Stop after incomplete trailing data
                    it = end;
                    break;
                }
            case RC::INVALID_LOW_SURROGATE:
                {
                    emitBreak( );
                    wchar_t hex_buf_w[ 7 ]{};
                    std::swprintf(
                        hex_buf_w,
                        7,
                        L"0x%04X",
                        static_cast<unsigned>( res.codepoint & 0xFFFF )
                    );
                    out.append(hex_buf_w);
                    emitBreakEnd( );
                    break;
                }
            case RC::INVALID_HIGH_SURROGATE:
                {
                    emitBreak( );
                    auto u1 = static_cast<uint16_t>( ( res.codepoint >> 16 ) & 0xFFFF );
                    auto u2 = static_cast<uint16_t>( res.codepoint & 0xFFFF );
                    wchar_t hexbuf1[ 7 ]{};
                    wchar_t hexbuf2[ 7 ]{};
                    std::swprintf(
                        hexbuf1,
                        7,
                        L"0x%04X",
                        static_cast<unsigned>( u1 )
                    );
                    std::swprintf(
                        hexbuf2,
                        7,
                        L"0x%04X",
                        static_cast<unsigned>( u2 )
                    );
                    out.append(hexbuf1);
                    out.push_back(L' ');
                    out.append(hexbuf2);
                    emitBreakEnd( );
                    break;
                }
            default:
                break;
            }
        }

        if( out.empty( ) )
        {
            return L"<unknown>";
        }

        return out;
    }
};