#include "xstrtool.h"
#include <algorithm>
#include <cassert>
#include <cctype>    // For isalpha
#include <cwctype>   // For iswalpha
#include <chrono>
#include <format>
#include <immintrin.h>
#include <intrin.h>  // For MSVC _BitScanForward/_BitScanReverse
#include <locale>
#include <vector>
#include <charconv>  // For std::from_chars
#include <format>  // For std::format
#include <chrono>

namespace xstrtool
{
    constexpr bool optimized_sse_v = true;

    //--------------------------------------------------------------------------------
    std::wstring To(const std::string_view InputView) noexcept
    {
        std::wstring Result(InputView.size(), 0);
        std::transform(InputView.begin(), InputView.end(), Result.begin(), [](const char ch) { return static_cast<wchar_t>(static_cast<unsigned char>(ch)); });
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring To(const char* Input) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return To(std::string_view(Input));
    }

    //--------------------------------------------------------------------------------
    std::string To(const std::wstring_view InputView) noexcept
    {
        std::string Result(InputView.size(), 0);
        std::transform(InputView.begin(), InputView.end(), Result.begin(), [](const wchar_t wch) { return static_cast<char>(wch); });
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string To(const wchar_t* Input) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return To(std::wstring_view(Input));
    }

    //--------------------------------------------------------------------------------

    std::string CopyN(const std::string_view Source, std::size_t N) noexcept
    {
        std::string Result;
        Result.resize(std::min(N, Source.size()));
        std::copy_n(Source.begin(), Result.size(), Result.begin());
        return Result;
    }

    //--------------------------------------------------------------------------------

    std::string CopyN(const char* Source, std::size_t N) noexcept
    {
        assert(Source != nullptr && "Source string is null");
        return CopyN(std::string_view(Source), N);
    }

    //--------------------------------------------------------------------------------

    std::wstring CopyN(const std::wstring_view Source, std::size_t N) noexcept
    {
        std::wstring Result;
        Result.resize(std::min(N, Source.size()));
        std::copy_n(Source.begin(), Result.size(), Result.begin());
        return Result;
    }

    //--------------------------------------------------------------------------------

    std::wstring CopyN(const wchar_t* Source, std::size_t N) noexcept
    {
        assert(Source != nullptr && "Source wide string is null");
        return CopyN(std::wstring_view(Source), N);
    }

    //--------------------------------------------------------------------------------
    std::string ToLowerCopy(const std::string_view InputView) noexcept
    {
        std::string Result(InputView);
        ToLower(Result);
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string ToLowerCopy(const char* Input) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return ToLowerCopy(std::string_view(Input));
    }

    //--------------------------------------------------------------------------------
    void ToLower(std::string& Str) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized for ASCII tolower (add 32 for 'A'-'Z')
            const std::size_t Len = Str.size();
            char* Data = Str.data();
            __m128i LowerMask = _mm_set1_epi8(32);
            __m128i UpperA = _mm_set1_epi8('A' - 1);
            __m128i UpperZ = _mm_set1_epi8('Z' + 1);
            for (std::size_t i = 0; i < Len / 16 * 16; i += 16)
            {
                __m128i V = _mm_loadu_si128(reinterpret_cast<__m128i*>(Data + i));
                __m128i Mask1 = _mm_cmpgt_epi8(V, UpperA);
                __m128i Mask2 = _mm_cmpgt_epi8(UpperZ, V);
                __m128i Mask = _mm_and_si128(Mask1, Mask2);
                V = _mm_add_epi8(V, _mm_and_si128(Mask, LowerMask));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(Data + i), V);
            }
            for (std::size_t i = Len / 16 * 16; i < Len; ++i)
            {
                Str[i] = ToLower(Str[i]);
            }
        }
        else
        {
            std::ranges::transform(Str, Str.begin(), [](char c) { return ToLower(c); });
        }
    }

    //--------------------------------------------------------------------------------
    std::wstring ToLowerCopy(const std::wstring_view InputView) noexcept
    {
        std::wstring Result(InputView);
        ToLower(Result);
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring ToLowerCopy(const wchar_t* Input) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return ToLowerCopy(std::wstring_view(Input));
    }

    //--------------------------------------------------------------------------------
    void ToLower(std::wstring& Str) noexcept
    {
        std::ranges::transform(Str, Str.begin(), [](wchar_t c) { return ToLower(c); });
    }

    //--------------------------------------------------------------------------------
    std::string ToUpperCopy(const std::string_view InputView) noexcept
    {
        std::string Result(InputView);
        ToUpper(Result);
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string ToUpperCopy(const char* Input) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return ToUpperCopy(std::string_view(Input));
    }

    //--------------------------------------------------------------------------------
    void ToUpper(std::string& Str) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized for ASCII toupper (subtract 32 for 'a'-'z')
            const std::size_t Len = Str.size();
            char* Data = Str.data();
            __m128i UpperMask = _mm_set1_epi8(-32);
            __m128i LowerA = _mm_set1_epi8('a' - 1);
            __m128i LowerZ = _mm_set1_epi8('z' + 1);
            for (std::size_t i = 0; i < Len / 16 * 16; i += 16)
            {
                __m128i V = _mm_loadu_si128(reinterpret_cast<__m128i*>(Data + i));
                __m128i Mask1 = _mm_cmpgt_epi8(V, LowerA);
                __m128i Mask2 = _mm_cmpgt_epi8(LowerZ, V);
                __m128i Mask = _mm_and_si128(Mask1, Mask2);
                V = _mm_add_epi8(V, _mm_and_si128(Mask, UpperMask));
                _mm_storeu_si128(reinterpret_cast<__m128i*>(Data + i), V);
            }
            for (std::size_t i = Len / 16 * 16; i < Len; ++i)
            {
                Str[i] = ToUpper(Str[i]);
            }
        }
        else
        {
            std::ranges::transform(Str, Str.begin(), [](char c) { return ToUpper(c); });
        }
    }

    //--------------------------------------------------------------------------------
    std::wstring ToUpperCopy(const std::wstring_view InputView) noexcept
    {
        std::wstring Result(InputView);
        ToUpper(Result);
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring ToUpperCopy(const wchar_t* Input) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return ToUpperCopy(std::wstring_view(Input));
    }

    //--------------------------------------------------------------------------------
    void ToUpper(std::wstring& Str) noexcept
    {
        std::ranges::transform(Str, Str.begin(), [](wchar_t c) { return ToUpper(c); });
    }

    //--------------------------------------------------------------------------------
    int CompareI(const std::string_view A, const std::string_view B) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized ASCII case-insensitive compare
            const std::size_t Len = std::min(A.size(), B.size());
            const char* Pa = A.data();
            const char* Pb = B.data();
            __m128i LowerMask = _mm_set1_epi8(32);
            __m128i UpperA = _mm_set1_epi8('A' - 1);
            __m128i UpperZ = _mm_set1_epi8('Z' + 1);
            for (std::size_t i = 0; i < Len / 16 * 16; i += 16)
            {
                __m128i Va = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Pa + i));
                __m128i Vb = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Pb + i));
                __m128i MaskA = _mm_and_si128(_mm_cmpgt_epi8(Va, UpperA), _mm_cmpgt_epi8(UpperZ, Va));
                __m128i MaskB = _mm_and_si128(_mm_cmpgt_epi8(Vb, UpperA), _mm_cmpgt_epi8(UpperZ, Vb));
                Va = _mm_or_si128(Va, _mm_and_si128(MaskA, LowerMask));
                Vb = _mm_or_si128(Vb, _mm_and_si128(MaskB, LowerMask));
                __m128i Eq = _mm_cmpeq_epi8(Va, Vb);
                if (_mm_movemask_epi8(Eq) != 0xFFFF)
                {
                    for (std::size_t j = i; j < i + 16 && j < Len; ++j)
                    {
                        char Ca = ToLower(A[j]), Cb = ToLower(B[j]);
                        if (Ca != Cb) return (Ca < Cb) ? -1 : 1;
                    }
                }
            }
            for (std::size_t i = Len / 16 * 16; i < Len; ++i)
            {
                char Ca = ToLower(A[i]), Cb = ToLower(B[i]);
                if (Ca != Cb) return (Ca < Cb) ? -1 : 1;
            }
            return (A.size() < B.size()) ? -1 : (A.size() > B.size()) ? 1 : 0;
        }
        else
        {
            const std::size_t Len = std::min(A.size(), B.size());
            for (std::size_t i = 0; i < Len; ++i)
            {
                const char Ca = ToLower(A[i]), Cb = ToLower(B[i]);
                if (Ca != Cb)
                {
                    return (Ca < Cb) ? -1 : 1;
                }
            }
            return (A.size() < B.size()) ? -1 : (A.size() > B.size()) ? 1 : 0;
        }
    }

    //--------------------------------------------------------------------------------
    int CompareI(const char* A, const char* B) noexcept
    {
        assert(A != nullptr && "First input string is null");
        assert(B != nullptr && "Second input string is null");
        return CompareI(std::string_view(A), std::string_view(B));
    }

    //--------------------------------------------------------------------------------
    int CompareI(const std::wstring_view A, const std::wstring_view B) noexcept
    {
        const std::size_t Len = std::min(A.size(), B.size());
        for (std::size_t i = 0; i < Len; ++i)
        {
            const wchar_t Ca = ToLower(A[i]), Cb = ToLower(B[i]);
            if (Ca != Cb)
            {
                return (Ca < Cb) ? -1 : 1;
            }
        }
        return (A.size() < B.size()) ? -1 : (A.size() > B.size()) ? 1 : 0;
    }

    //--------------------------------------------------------------------------------
    int CompareI(const wchar_t* A, const wchar_t* B) noexcept
    {
        assert(A != nullptr && "First input wide string is null");
        assert(B != nullptr && "Second input wide string is null");
        return CompareI(std::wstring_view(A), std::wstring_view(B));
    }

    //--------------------------------------------------------------------------------
    std::size_t findI(const std::string_view Haystack, const std::string_view Needle, const std::size_t Pos) noexcept
    {
        if (Needle.empty())
        {
            return std::string::npos; // Empty needle matches at Pos (if valid)
        }

        if constexpr (optimized_sse_v)
        {
            // SSE optimized for ASCII case-insensitive find
            assert(Pos <= Haystack.size() && "Starting position exceeds haystack length");
            if (Pos + Needle.size() > Haystack.size() || Needle.empty())
            {
                return std::string::npos;
            }
            std::string LowerNeedle = ToLowerCopy(Needle);
            const char* Hay = Haystack.data();
            const char* Ndl = LowerNeedle.data();
            __m128i LowerMask = _mm_set1_epi8(32);
            __m128i UpperA = _mm_set1_epi8('A' - 1);
            __m128i UpperZ = _mm_set1_epi8('Z' + 1);
            for (std::size_t i = Pos; i <= Haystack.size() - Needle.size(); ++i)
            {
                bool Match = true;
                for (std::size_t j = 0; j < Needle.size() / 16 * 16; j += 16)
                {
                    __m128i Vh = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Hay + i + j));
                    __m128i Vn = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Ndl + j));
                    __m128i MaskH = _mm_and_si128(_mm_cmpgt_epi8(Vh, UpperA), _mm_cmpgt_epi8(UpperZ, Vh));
                    Vh = _mm_or_si128(Vh, _mm_and_si128(MaskH, LowerMask));
                    __m128i Eq = _mm_cmpeq_epi8(Vh, Vn);
                    if (_mm_movemask_epi8(Eq) != 0xFFFF)
                    {
                        Match = false;
                        break;
                    }
                }
                if (Match)
                {
                    for (std::size_t j = Needle.size() / 16 * 16; j < Needle.size(); ++j)
                    {
                        if (ToLower(Haystack[i + j]) != LowerNeedle[j])
                        {
                            Match = false;
                            break;
                        }
                    }
                }
                if (Match) return i;
            }
            return std::string::npos;
        }
        else
        {
            assert(Pos <= Haystack.size() && "Starting position exceeds haystack length");
            if (Pos + Needle.size() > Haystack.size() || Needle.empty())
            {
                return std::string::npos;
            }
            for (std::size_t i = Pos; i <= Haystack.size() - Needle.size(); ++i)
            {
                if (CompareI(Haystack.substr(i, Needle.size()), Needle) == 0)
                {
                    return i;
                }
            }
            return std::string::npos;
        }
    }

    //--------------------------------------------------------------------------------
    std::size_t findI(const char* Haystack, const char* Needle, const std::size_t Pos) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        return findI(std::string_view(Haystack), std::string_view(Needle), Pos);
    }

    //--------------------------------------------------------------------------------
    std::size_t findI(const std::wstring_view Haystack, const std::wstring_view Needle, const std::size_t Pos) noexcept
    {
        assert(Pos <= Haystack.size() && "Starting position exceeds haystack length");
        if (Pos + Needle.size() > Haystack.size() || Needle.empty())
        {
            return std::wstring::npos;
        }
        for (std::size_t i = Pos; i <= Haystack.size() - Needle.size(); ++i)
        {
            if (CompareI(Haystack.substr(i, Needle.size()), Needle) == 0)
            {
                return i;
            }
        }
        return std::wstring::npos;
    }

    //--------------------------------------------------------------------------------
    std::size_t findI(const wchar_t* Haystack, const wchar_t* Needle, const std::size_t Pos) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        return findI(std::wstring_view(Haystack), std::wstring_view(Needle), Pos);
    }

    //--------------------------------------------------------------------------------

    std::size_t rfindI(const std::string_view Haystack, const std::string_view Needle, const std::size_t Pos) noexcept
    {
        assert(Pos == std::string::npos || Pos <= Haystack.size() && "Starting position exceeds haystack length");
        if (Needle.empty())
        {
            return Pos == std::string::npos ? Haystack.size() : std::min(Pos, Haystack.size());
        }
        if (Needle.size() > Haystack.size())
        {
            return std::string::npos;
        }
        std::size_t StartPos = Pos == std::string::npos ? Haystack.size() - Needle.size() : std::min(Pos, Haystack.size() - Needle.size());

        if constexpr (optimized_sse_v)
        {
            std::string LowerNeedle = ToLowerCopy(Needle);
            const char* Hay = Haystack.data();
            const char* Ndl = LowerNeedle.data();
            __m128i LowerMask = _mm_set1_epi8(32);
            __m128i UpperA = _mm_set1_epi8('A' - 1);
            __m128i UpperZ = _mm_set1_epi8('Z' + 1);
            for (std::size_t i = StartPos; i <= StartPos; --i)
            {
                bool Match = true;
                for (std::size_t k = 0; k < Needle.size() / 16 * 16; k += 16)
                {
                    __m128i Vh = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Hay + i + k));
                    __m128i Vn = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Ndl + k));
                    __m128i MaskH = _mm_and_si128(_mm_cmpgt_epi8(Vh, UpperA), _mm_cmpgt_epi8(UpperZ, Vh));
                    Vh = _mm_or_si128(Vh, _mm_and_si128(MaskH, LowerMask));
                    __m128i Eq = _mm_cmpeq_epi8(Vh, Vn);
                    if (_mm_movemask_epi8(Eq) != 0xFFFF)
                    {
                        Match = false;
                        break;
                    }
                }
                if (Match)
                {
                    for (std::size_t k = Needle.size() / 16 * 16; k < Needle.size(); ++k)
                    {
                        if (ToLower(Haystack[i + k]) != LowerNeedle[k])
                        {
                            Match = false;
                            break;
                        }
                    }
                }
                if (Match) return i;
                if (i == 0) break;
            }
            return std::string::npos;
        }
        else
        {
            for (std::size_t i = StartPos; i <= StartPos; --i)
            {
                if (CompareI(Haystack.substr(i, Needle.size()), Needle) == 0)
                {
                    return i;
                }
                if (i == 0) break;
            }
            return std::string::npos;
        }
    }

    //--------------------------------------------------------------------------------

    std::size_t rfindI(const char* Haystack, const char* Needle, const std::size_t Pos) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        return rfindI(std::string_view(Haystack), std::string_view(Needle), Pos);
    }

    //--------------------------------------------------------------------------------

    std::size_t rfindI(const std::wstring_view Haystack, const std::wstring_view Needle, const std::size_t Pos) noexcept
    {
        assert(Pos == std::wstring::npos || Pos <= Haystack.size() && "Starting position exceeds haystack length");
        if (Needle.empty())
        {
            return Pos == std::wstring::npos ? Haystack.size() : std::min(Pos, Haystack.size());
        }
        if (Needle.size() > Haystack.size())
        {
            return std::wstring::npos;
        }
        std::size_t StartPos = Pos == std::wstring::npos ? Haystack.size() - Needle.size() : std::min(Pos, Haystack.size() - Needle.size());
        for (std::size_t i = StartPos + 1; i > 0; --i)
        {
            std::size_t j = i - 1;
            if (CompareI(Haystack.substr(j, Needle.size()), Needle) == 0)
            {
                return j;
            }
        }
        return std::wstring::npos;
    }

    //--------------------------------------------------------------------------------

    std::size_t rfindI(const wchar_t* Haystack, const wchar_t* Needle, const std::size_t Pos) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        return rfindI(std::wstring_view(Haystack), std::wstring_view(Needle), Pos);
    }

    //--------------------------------------------------------------------------------
    bool StartsWithI(const std::string_view Haystack, const std::string_view Needle) noexcept
    {
        return Needle.size() <= Haystack.size() && CompareI(Haystack.substr(0, Needle.size()), Needle) == 0;
    }

    //--------------------------------------------------------------------------------
    bool StartsWithI(const char* Haystack, const char* Needle) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        return StartsWithI(std::string_view(Haystack), std::string_view(Needle));
    }

    //--------------------------------------------------------------------------------
    bool StartsWithI(const std::wstring_view Haystack, const std::wstring_view Needle) noexcept
    {
        return Needle.size() <= Haystack.size() && CompareI(Haystack.substr(0, Needle.size()), Needle) == 0;
    }

    //--------------------------------------------------------------------------------
    bool StartsWithI(const wchar_t* Haystack, const wchar_t* Needle) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        return StartsWithI(std::wstring_view(Haystack), std::wstring_view(Needle));
    }

    //--------------------------------------------------------------------------------
    bool EndsWithI(const std::string_view Haystack, const std::string_view Needle) noexcept
    {
        return Needle.size() <= Haystack.size() && CompareI(Haystack.substr(Haystack.size() - Needle.size()), Needle) == 0;
    }

    //--------------------------------------------------------------------------------
    bool EndsWithI(const char* Haystack, const char* Needle) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        return EndsWithI(std::string_view(Haystack), std::string_view(Needle));
    }

    //--------------------------------------------------------------------------------
    bool EndsWithI(const std::wstring_view Haystack, const std::wstring_view Needle) noexcept
    {
        return Needle.size() <= Haystack.size() && CompareI(Haystack.substr(Haystack.size() - Needle.size()), Needle) == 0;
    }

    //--------------------------------------------------------------------------------
    bool EndsWithI(const wchar_t* Haystack, const wchar_t* Needle) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        return EndsWithI(std::wstring_view(Haystack), std::wstring_view(Needle));
    }

    //--------------------------------------------------------------------------------
    bool ContainsI(const std::string_view Haystack, const std::string_view Needle) noexcept
    {
        if ( Needle.empty() ) return true;
        return findI(Haystack, Needle) != std::string::npos;
    }

    //--------------------------------------------------------------------------------
    bool ContainsI(const char* Haystack, const char* Needle) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        return ContainsI(std::string_view(Haystack), std::string_view(Needle));
    }

    //--------------------------------------------------------------------------------
    bool ContainsI(const std::wstring_view Haystack, const std::wstring_view Needle) noexcept
    {
        if (Needle.empty()) return true;
        return findI(Haystack, Needle) != std::wstring::npos;
    }

    //--------------------------------------------------------------------------------
    bool ContainsI(const wchar_t* Haystack, const wchar_t* Needle) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        return ContainsI(std::wstring_view(Haystack), std::wstring_view(Needle));
    }

    //--------------------------------------------------------------------------------
    std::string_view TrimLeftCopy(const std::string_view InputView) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized whitespace skip
            const std::size_t Len = InputView.size();
            const char* Data = InputView.data();
            __m128i White = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, '\r', '\n', '\t', ' ', 0, 0, 0);
            std::size_t i = 0;
            for (; i < Len / 16 * 16; i += 16)
            {
                __m128i V = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Data + i));
                __m128i Eq = _mm_cmpeq_epi8(V, White);
                int Mask = _mm_movemask_epi8(Eq);
                if (Mask != 0xFFFF)
                {

                #ifdef _MSC_VER
                    unsigned long Index;
                    if (_BitScanForward(&Index, ~static_cast<unsigned long>(Mask)))
                    {
                        i += Index;
                    }
                    else
                    {
                        i = Len; // No non-whitespace found
                    }
                #else
                    i += __builtin_ctz(~Mask);
                #endif

                    return InputView.substr(i);
                }
            }
            for (; i < Len; ++i)
            {
                if (InputView[i] != ' ' && InputView[i] != '\t' && InputView[i] != '\n' && InputView[i] != '\r')
                {
                    return InputView.substr(i);
                }
            }
            return {};
        }
        else
        {
            std::size_t i = 0;
            while (i < InputView.size() && (InputView[i] == ' ' || InputView[i] == '\t' || InputView[i] == '\n' || InputView[i] == '\r'))
            {
                ++i;
            }
            return InputView.substr(i);
        }
    }

    //--------------------------------------------------------------------------------
    std::string_view TrimLeftCopy(const char* Input) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return TrimLeftCopy(std::string_view(Input));
    }

    //--------------------------------------------------------------------------------
    void TrimLeft(std::string& Str) noexcept
    {
        Str = Str.substr(TrimLeftCopy(Str).data() - Str.data());
    }

    //--------------------------------------------------------------------------------
    std::string_view TrimRightCopy(const std::string_view InputView) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized reverse whitespace skip
            const std::size_t Len = InputView.size();
            const char* Data = InputView.data();
            __m128i White = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, '\r', '\n', '\t', ' ', 0, 0, 0);
            std::size_t i = Len;
            for (; i > 16; i -= 16)
            {
                __m128i V = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Data + i - 16));
                __m128i Eq = _mm_cmpeq_epi8(V, White);
                int Mask = _mm_movemask_epi8(Eq);
                if (Mask != 0xFFFF)
                {
                #ifdef _MSC_VER
                    unsigned long Index;
                    if (_BitScanReverse(&Index, ~static_cast<unsigned long>(Mask)))
                    {
                        i -= 16 - (Index / 8);
                    }
                    else
                    {
                        i = 0; // No non-whitespace found
                    }
                #else
                    i -= 16 - __builtin_clz(~Mask) / 8;
                #endif

                    return InputView.substr(0, i);
                }
            }
            while (i > 0 && (InputView[i - 1] == ' ' || InputView[i - 1] == '\t' || InputView[i - 1] == '\n' || InputView[i - 1] == '\r'))
            {
                --i;
            }
            return InputView.substr(0, i);
        }
        else
        {
            std::size_t i = InputView.size();
            while (i > 0 && (InputView[i - 1] == ' ' || InputView[i - 1] == '\t' || InputView[i - 1] == '\n' || InputView[i - 1] == '\r'))
            {
                --i;
            }
            return InputView.substr(0, i);
        }
    }

    //--------------------------------------------------------------------------------
    std::string_view TrimRightCopy(const char* Input) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return TrimRightCopy(std::string_view(Input));
    }

    //--------------------------------------------------------------------------------
    void TrimRight(std::string& Str) noexcept
    {
        Str.erase(TrimRightCopy(Str).size());
    }

    //--------------------------------------------------------------------------------
    std::string_view TrimCopy(const std::string_view InputView) noexcept
    {
        return TrimLeftCopy(TrimRightCopy(InputView));
    }

    //--------------------------------------------------------------------------------
    std::string_view TrimCopy(const char* Input) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return TrimCopy(std::string_view(Input));
    }

    //--------------------------------------------------------------------------------
    void Trim(std::string& Str) noexcept
    {
        Str = std::string(TrimCopy(Str));
    }

    //--------------------------------------------------------------------------------
    std::wstring_view TrimLeftCopy(const std::wstring_view InputView) noexcept
    {
        std::size_t i = 0;
        while (i < InputView.size() && (InputView[i] == L' ' || InputView[i] == L'\t' || InputView[i] == L'\n' || InputView[i] == L'\r'))
        {
            ++i;
        }
        return InputView.substr(i);
    }

    //--------------------------------------------------------------------------------
    std::wstring_view TrimLeftCopy(const wchar_t* Input) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return TrimLeftCopy(std::wstring_view(Input));
    }

    //--------------------------------------------------------------------------------
    void TrimLeft(std::wstring& Str) noexcept
    {
        Str = Str.substr(TrimLeftCopy(Str).data() - Str.data());
    }

    //--------------------------------------------------------------------------------
    std::wstring_view TrimRightCopy(const std::wstring_view InputView) noexcept
    {
        std::size_t i = InputView.size();
        while (i > 0 && (InputView[i - 1] == L' ' || InputView[i - 1] == L'\t' || InputView[i - 1] == L'\n' || InputView[i - 1] == L'\r'))
        {
            --i;
        }
        return InputView.substr(0, i);
    }

    //--------------------------------------------------------------------------------
    std::wstring_view TrimRightCopy(const wchar_t* Input) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return TrimRightCopy(std::wstring_view(Input));
    }

    //--------------------------------------------------------------------------------
    void TrimRight(std::wstring& Str) noexcept
    {
        Str.erase(TrimRightCopy(Str).size());
    }

    //--------------------------------------------------------------------------------
    std::wstring_view TrimCopy(const std::wstring_view InputView) noexcept
    {
        return TrimLeftCopy(TrimRightCopy(InputView));
    }

    //--------------------------------------------------------------------------------
    std::wstring_view TrimCopy(const wchar_t* Input) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return TrimCopy(std::wstring_view(Input));
    }

    //--------------------------------------------------------------------------------
    void Trim(std::wstring& Str) noexcept
    {
        Str = std::wstring(TrimCopy(Str));
    }

    //--------------------------------------------------------------------------------
    std::vector<std::string_view> Split(const std::string_view InputView, const char Delim) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized char search for split
            std::vector<std::string_view> Result;
            std::size_t Start = 0;
            const char* Data = InputView.data();
            const std::size_t Len = InputView.size();
            __m128i D = _mm_set1_epi8(Delim);
            for (std::size_t i = 0; i < Len / 16 * 16; i += 16)
            {
                __m128i V = _mm_loadu_si128(reinterpret_cast<const __m128i*>(Data + i));
                __m128i Eq = _mm_cmpeq_epi8(V, D);
                int Mask = _mm_movemask_epi8(Eq);
                while (Mask)
                {
                #ifdef _MSC_VER
                    unsigned long Bit;
                    if (_BitScanForward(&Bit, static_cast<unsigned long>(Mask)))
                    {
                        Result.push_back(InputView.substr(Start, i + Bit - Start));
                        Start = i + Bit + 1;
                        Mask ^= (1 << Bit);
                    }
                    else
                    {
                        break; // No more delimiters in this block
                    }
                #else
                    int Bit = __builtin_ctz(Mask);
                    Result.push_back(InputView.substr(Start, i + Bit - Start));
                    Start = i + Bit + 1;
                    Mask ^= (1 << Bit);
                #endif
                }
            }
            for (std::size_t i = Len / 16 * 16; i < Len; ++i)
            {
                if (InputView[i] == Delim)
                {
                    Result.push_back(InputView.substr(Start, i - Start));
                    Start = i + 1;
                }
            }
            Result.push_back(InputView.substr(Start));
            return Result;
        }
        else
        {
            std::vector<std::string_view> Result;
            std::size_t Start = 0;
            for (std::size_t i = 0; i < InputView.size(); ++i)
            {
                if (InputView[i] == Delim)
                {
                    Result.push_back(InputView.substr(Start, i - Start));
                    Start = i + 1;
                }
            }
            Result.push_back(InputView.substr(Start));
            return Result;
        }
    }

    //--------------------------------------------------------------------------------
    std::vector<std::string_view> Split(const char* Input, const char Delim) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        return Split(std::string_view(Input), Delim);
    }

    //--------------------------------------------------------------------------------
    std::vector<std::string_view> Split(const std::string_view InputView, const std::string_view Delim) noexcept
    {
        std::vector<std::string_view> Result;
        std::size_t Start = 0;
        std::size_t Pos = 0;
        while ((Pos = InputView.find(Delim, Start)) != std::string_view::npos)
        {
            Result.push_back(InputView.substr(Start, Pos - Start));
            Start = Pos + Delim.size();
        }
        Result.push_back(InputView.substr(Start));
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::vector<std::string_view> Split(const char* Input, const char* Delim) noexcept
    {
        assert(Input != nullptr && "Input string is null");
        assert(Delim != nullptr && "Delimiter string is null");
        return Split(std::string_view(Input), std::string_view(Delim));
    }

    //--------------------------------------------------------------------------------
    std::vector<std::wstring_view> Split(const std::wstring_view InputView, const wchar_t Delim) noexcept
    {
        std::vector<std::wstring_view> Result;
        std::size_t Start = 0;
        for (std::size_t i = 0; i < InputView.size(); ++i)
        {
            if (InputView[i] == Delim)
            {
                Result.push_back(InputView.substr(Start, i - Start));
                Start = i + 1;
            }
        }
        Result.push_back(InputView.substr(Start));
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::vector<std::wstring_view> Split(const wchar_t* Input, const wchar_t Delim) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        return Split(std::wstring_view(Input), Delim);
    }

    //--------------------------------------------------------------------------------
    std::vector<std::wstring_view> Split(const std::wstring_view InputView, const std::wstring_view Delim) noexcept
    {
        std::vector<std::wstring_view> Result;
        std::size_t Start = 0;
        std::size_t Pos = 0;
        while ((Pos = InputView.find(Delim, Start)) != std::wstring_view::npos)
        {
            Result.push_back(InputView.substr(Start, Pos - Start));
            Start = Pos + Delim.size();
        }
        Result.push_back(InputView.substr(Start));
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::vector<std::wstring_view> Split(const wchar_t* Input, const wchar_t* Delim) noexcept
    {
        assert(Input != nullptr && "Input wide string is null");
        assert(Delim != nullptr && "Delimiter wide string is null");
        return Split(std::wstring_view(Input), std::wstring_view(Delim));
    }

    //--------------------------------------------------------------------------------
    std::string ReplaceCopy(const std::string_view Haystack, const std::string_view Needle, const std::string_view Replacement) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized find for replace
            if (Needle.empty()) return std::string(Haystack);
            std::string Result;
            std::size_t Pos = 0;
            std::size_t LastPos = 0;
            while ((Pos = Haystack.find(Needle, Pos)) != std::string_view::npos)
            {
                Result.append(Haystack, LastPos, Pos - LastPos);
                Result.append(Replacement);
                Pos += Needle.size();
                LastPos = Pos;
            }
            Result.append(Haystack.substr(LastPos));
            return Result;
        }
        else
        {
            if (Needle.empty()) return std::string(Haystack);
            std::string Result;
            std::size_t Pos = 0;
            std::size_t LastPos = 0;
            while ((Pos = Haystack.find(Needle, Pos)) != std::string_view::npos)
            {
                Result.append(Haystack, LastPos, Pos - LastPos);
                Result.append(Replacement);
                Pos += Needle.size();
                LastPos = Pos;
            }
            Result.append(Haystack.substr(LastPos));
            return Result;
        }
    }

    //--------------------------------------------------------------------------------
    std::string ReplaceCopy(const char* Haystack, const char* Needle, const char* Replacement) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        assert(Replacement != nullptr && "Replacement string is null");
        return ReplaceCopy(std::string_view(Haystack), std::string_view(Needle), std::string_view(Replacement));
    }

    //--------------------------------------------------------------------------------
    void Replace(std::string& Str, const std::string_view Needle, const std::string_view Replacement) noexcept
    {
        Str = ReplaceCopy(Str, Needle, Replacement);
    }

    //--------------------------------------------------------------------------------
    std::wstring ReplaceCopy(const std::wstring_view Haystack, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept
    {
        if (Needle.empty()) return std::wstring(Haystack);
        std::wstring Result;
        std::size_t Pos = 0;
        std::size_t LastPos = 0;
        while ((Pos = Haystack.find(Needle, Pos)) != std::wstring_view::npos)
        {
            Result.append(Haystack, LastPos, Pos - LastPos);
            Result.append(Replacement);
            Pos += Needle.size();
            LastPos = Pos;
        }
        Result.append(Haystack.substr(LastPos));
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring ReplaceCopy(const wchar_t* Haystack, const wchar_t* Needle, const wchar_t* Replacement) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        assert(Replacement != nullptr && "Replacement wide string is null");
        return ReplaceCopy(std::wstring_view(Haystack), std::wstring_view(Needle), std::wstring_view(Replacement));
    }

    //--------------------------------------------------------------------------------
    void Replace(std::wstring& Str, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept
    {
        Str = ReplaceCopy(Str, Needle, Replacement);
    }

    //--------------------------------------------------------------------------------
    std::string ReplaceICopy(const std::string_view Haystack, const std::string_view Needle, const std::string_view Replacement) noexcept
    {
        if (Needle.empty()) return std::string(Haystack);
        std::string Result;
        std::size_t Pos = 0;
        std::size_t LastPos = 0;
        while ((Pos = findI(Haystack, Needle, Pos)) != std::string::npos)
        {
            Result.append(Haystack, LastPos, Pos - LastPos);
            Result.append(Replacement);
            Pos += Needle.size();
            LastPos = Pos;
        }
        Result.append(Haystack.substr(LastPos));
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string ReplaceICopy(const char* Haystack, const char* Needle, const char* Replacement) noexcept
    {
        assert(Haystack != nullptr && "Haystack string is null");
        assert(Needle != nullptr && "Needle string is null");
        assert(Replacement != nullptr && "Replacement string is null");
        return ReplaceICopy(std::string_view(Haystack), std::string_view(Needle), std::string_view(Replacement));
    }

    //--------------------------------------------------------------------------------
    void ReplaceI(std::string& Str, const std::string_view Needle, const std::string_view Replacement) noexcept
    {
        Str = ReplaceICopy(Str, Needle, Replacement);
    }

    //--------------------------------------------------------------------------------
    std::wstring ReplaceICopy(const std::wstring_view Haystack, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept
    {
        if (Needle.empty()) return std::wstring(Haystack);
        std::wstring Result;
        std::size_t Pos = 0;
        std::size_t LastPos = 0;
        while ((Pos = findI(Haystack, Needle, Pos)) != std::wstring::npos)
        {
            Result.append(Haystack, LastPos, Pos - LastPos);
            Result.append(Replacement);
            Pos += Needle.size();
            LastPos = Pos;
        }
        Result.append(Haystack.substr(LastPos));
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring ReplaceICopy(const wchar_t* Haystack, const wchar_t* Needle, const wchar_t* Replacement) noexcept
    {
        assert(Haystack != nullptr && "Haystack wide string is null");
        assert(Needle != nullptr && "Needle wide string is null");
        assert(Replacement != nullptr && "Replacement wide string is null");
        return ReplaceICopy(std::wstring_view(Haystack), std::wstring_view(Needle), std::wstring_view(Replacement));
    }

    //--------------------------------------------------------------------------------
    void ReplaceI(std::wstring& Str, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept
    {
        Str = ReplaceICopy(Str, Needle, Replacement);
    }

    //--------------------------------------------------------------------------------
    std::string PathJoin(const std::string_view Base, const std::string_view Part) noexcept
    {
        if (Base.empty()) return std::string(Part);
        if (Part.empty()) return std::string(Base);
        char Sep = (Base.back() == '/' || Base.back() == '\\') ? 0 : '/';
        std::string Result(Base);
        if (Sep) Result += Sep;
        Result += Part;
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string PathJoin(const char* Base, const char* Part) noexcept
    {
        assert(Base != nullptr && "Base path is null");
        assert(Part != nullptr && "Part path is null");
        return PathJoin(std::string_view(Base), std::string_view(Part));
    }

    //--------------------------------------------------------------------------------
    std::string PathNormalizeCopy(const std::string_view Path) noexcept
    {
        std::vector<std::string_view> Parts;
        std::size_t Start = 0;
        bool IsAbsolute = !Path.empty() && (Path[0] == '/' || Path[0] == '\\');
        bool IsDrive = false;
        std::string_view Drive;
        if (Path.size() >= 2 && Path[1] == ':' && std::isalpha(static_cast<unsigned char>(Path[0])))
        {
            IsAbsolute = true;
            IsDrive = true;
            Drive = Path.substr(0, 2);
            Start = 2;
            if (Path.size() > 2 && (Path[2] == '/' || Path[2] == '\\')) Start = 3;
        }
        for (std::size_t i = Start; i < Path.size(); ++i)
        {
            if (Path[i] == '/' || Path[i] == '\\')
            {
                if (i > Start) Parts.push_back(Path.substr(Start, i - Start));
                Start = i + 1;
            }
        }
        if (Start < Path.size()) Parts.push_back(Path.substr(Start));
        std::vector<std::string_view> ResultParts;
        for (const auto& Part : Parts)
        {
            if (Part == "." || Part.empty()) continue;
            if (Part == ".." && !ResultParts.empty()) ResultParts.pop_back();
            else if (Part == ".." && !IsDrive) ResultParts.push_back(Part);
            else if (Part != "..") ResultParts.push_back(Part);
        }
        std::string Result;
        if (IsDrive)
        {
            Result = std::string(Drive);
            if (!ResultParts.empty()) Result += '/';
        }
        else if (IsAbsolute)
        {
            Result = "/";
        }
        bool First = true;
        for (const auto& Part : ResultParts)
        {
            if (!First) Result += '/';
            First = false;
            Result.append(Part);
        }
        if (Result.empty() && !IsAbsolute) return ".";
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string PathNormalizeCopy(const char* Path) noexcept
    {
        assert(Path != nullptr && "Path string is null");
        return PathNormalizeCopy(std::string_view(Path));
    }

    //--------------------------------------------------------------------------------
    void PathNormalize(std::string& Str) noexcept
    {
        Str = PathNormalizeCopy(Str);
    }

    //--------------------------------------------------------------------------------
    std::wstring PathJoin(const std::wstring_view Base, const std::wstring_view Part) noexcept
    {
        if (Base.empty()) return std::wstring(Part);
        if (Part.empty()) return std::wstring(Base);
        wchar_t Sep = (Base.back() == L'/' || Base.back() == L'\\') ? 0 : L'/';
        std::wstring Result(Base);
        if (Sep) Result += Sep;
        Result += Part;
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring PathJoin(const wchar_t* Base, const wchar_t* Part) noexcept
    {
        assert(Base != nullptr && "Base wide path is null");
        assert(Part != nullptr && "Part wide path is null");
        return PathJoin(std::wstring_view(Base), std::wstring_view(Part));
    }

    //--------------------------------------------------------------------------------
    std::wstring PathNormalizeCopy(const std::wstring_view Path) noexcept
    {
        std::vector<std::wstring_view> Parts;
        std::size_t Start = 0;
        bool IsAbsolute = !Path.empty() && (Path[0] == L'/' || Path[0] == L'\\');
        bool IsDrive = false;
        std::wstring_view Drive;
        if (Path.size() >= 2 && Path[1] == L':' && std::iswalpha(Path[0]))
        {
            IsAbsolute = true;
            IsDrive = true;
            Drive = Path.substr(0, 2);
            Start = 2;
            if (Path.size() > 2 && (Path[2] == L'/' || Path[2] == L'\\')) Start = 3;
        }
        for (std::size_t i = Start; i < Path.size(); ++i)
        {
            if (Path[i] == L'/' || Path[i] == L'\\')
            {
                if (i > Start) Parts.push_back(Path.substr(Start, i - Start));
                Start = i + 1;
            }
        }
        if (Start < Path.size()) Parts.push_back(Path.substr(Start));
        std::vector<std::wstring_view> ResultParts;
        for (const auto& Part : Parts)
        {
            if (Part == L"." || Part.empty()) continue;
            if (Part == L".." && !ResultParts.empty()) ResultParts.pop_back();
            else if (Part == L".." && !IsDrive) ResultParts.push_back(Part);
            else if (Part != L"..") ResultParts.push_back(Part);
        }
        std::wstring Result;
        if (IsDrive)
        {
            Result = std::wstring(Drive);
            if (!ResultParts.empty()) Result += L'/';
        }
        else if (IsAbsolute)
        {
            Result = L"/";
        }
        bool First = true;
        for (const auto& Part : ResultParts)
        {
            if (!First) Result += L'/';
            First = false;
            Result.append(Part);
        }
        if (Result.empty() && !IsAbsolute) return L".";
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::wstring PathNormalizeCopy(const wchar_t* Path) noexcept
    {
        assert(Path != nullptr && "Path wide string is null");
        return PathNormalizeCopy(std::wstring_view(Path));
    }

    //--------------------------------------------------------------------------------
    void PathNormalize(std::wstring& Str) noexcept
    {
        Str = PathNormalizeCopy(Str);
    }

    //--------------------------------------------------------------------------------
    std::string FormatTime(const std::chrono::system_clock::time_point Tp, const std::string_view Fmt) noexcept
    {
        try 
        {
            std::string ChronoFmt = "{:" + std::string(Fmt) + "}";
            const auto flooredTp = std::chrono::floor<std::chrono::seconds>(Tp); // Store as lvalue
            return std::vformat(ChronoFmt, std::make_format_args(flooredTp));
        }
        catch (...) 
        {
            return std::string();
        }
    }

    //--------------------------------------------------------------------------------
    std::string PadLeftCopy(const std::string_view Sv, const std::size_t Width, const char Fill) noexcept
    {
        std::string Result;
        if (Sv.size() < Width)
        {
            Result.assign(Width - Sv.size(), Fill);
        }
        Result.append(Sv);
        return Result;
    }

    //--------------------------------------------------------------------------------
    void PadLeft(std::string& Str, const std::size_t Width, const char Fill) noexcept
    {
        if (Str.size() < Width)
        {
            Str.insert(0, Width - Str.size(), Fill);
        }
    }

    //--------------------------------------------------------------------------------
    std::string PadRightCopy(const std::string_view Sv, const std::size_t Width, const char Fill) noexcept
    {
        std::string Result(Sv);
        if (Sv.size() < Width)
        {
            Result.append(Width - Sv.size(), Fill);
        }
        return Result;
    }

    //--------------------------------------------------------------------------------
    void PadRight(std::string& Str, const std::size_t Width, const char Fill) noexcept
    {
        if (Str.size() < Width)
        {
            Str.append(Width - Str.size(), Fill);
        }
    }

    //--------------------------------------------------------------------------------
    std::string Repeat(const std::string_view Sv, const std::size_t Count) noexcept
    {
        std::string Result;
        Result.reserve(Sv.size() * Count);
        for (std::size_t i = 0; i < Count; ++i)
        {
            Result.append(Sv);
        }
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string Base64Encode(const std::string_view Data) noexcept
    {
        static constexpr char EncodeTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string Result;
        Result.reserve((Data.size() + 2) / 3 * 4);
        for (std::size_t i = 0; i < Data.size(); i += 3)
        {
            uint32_t Val = static_cast<unsigned char>(Data[i]) << 16;
            if (i + 1 < Data.size()) Val |= static_cast<unsigned char>(Data[i + 1]) << 8;
            if (i + 2 < Data.size()) Val |= static_cast<unsigned char>(Data[i + 2]);
            Result.push_back(EncodeTable[(Val >> 18) & 0x3F]);
            Result.push_back(EncodeTable[(Val >> 12) & 0x3F]);
            Result.push_back((i + 1 < Data.size()) ? EncodeTable[(Val >> 6) & 0x3F] : '=');
            Result.push_back((i + 2 < Data.size()) ? EncodeTable[Val & 0x3F] : '=');
        }
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string Base64Decode(const std::string_view Base64) noexcept
    {
        static constexpr uint8_t DecodeTable[] = {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
        };
        std::string Result;
        Result.reserve(Base64.size() * 3 / 4);
        uint32_t Val = 0;
        int Bits = 0;
        for (char C : Base64)
        {
            if (C == '=') break;
            if (C < 0 || DecodeTable[static_cast<uint8_t>(C)] == 64) continue;
            Val = (Val << 6) + DecodeTable[static_cast<uint8_t>(C)];
            Bits += 6;
            if (Bits >= 8)
            {
                Bits -= 8;
                Result.push_back(static_cast<char>((Val >> Bits) & 0xFF));
            }
        }
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string URLEncode(const std::string_view Url) noexcept
    {
        std::string Result;
        Result.reserve(Url.size() * 3);
        for (char C : Url)
        {
            if (std::isalnum(static_cast<unsigned char>(C)) || C == '-' || C == '_' || C == '.' || C == '~')
            {
                Result += C;
            }
            else
            {
                Result += '%';
                Result += "0123456789ABCDEF"[(C >> 4) & 0xF];
                Result += "0123456789ABCDEF"[C & 0xF];
            }
        }
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::string URLDecode(const std::string_view Encoded) noexcept
    {
        std::string Result;
        Result.reserve(Encoded.size());
        for (std::size_t i = 0; i < Encoded.size(); ++i)
        {
            if (Encoded[i] == '%' && i + 2 < Encoded.size())
            {
                char High = Encoded[i + 1];
                char Low = Encoded[i + 2];
                int Val = ((High >= '0' && High <= '9') ? High - '0' : (High >= 'A' && High <= 'F') ? High - 'A' + 10 : (High >= 'a' && High <= 'f') ? High - 'a' + 10 : 0) * 16 +
                    ((Low >= '0' && Low <= '9') ? Low - '0' : (Low >= 'A' && Low <= 'F') ? Low - 'A' + 10 : (Low >= 'a' && Low <= 'f') ? Low - 'a' + 10 : 0);
                Result += static_cast<char>(Val);
                i += 2;
            }
            else if (Encoded[i] == '+')
            {
                Result += ' ';
            }
            else
            {
                Result += Encoded[i];
            }
        }
        return Result;
    }

    //--------------------------------------------------------------------------------
    std::size_t Hash(const std::string_view Sv) noexcept
    {
        return std::hash<std::string_view>{}(Sv);
    }

    //--------------------------------------------------------------------------------
    std::string ToString(const double Num, const int Precision) noexcept
    {
        return std::format("{:.{}f}", Num, Precision);
    }

    //--------------------------------------------------------------------------------
    std::size_t LevenshteinDistance(const std::string_view A, const std::string_view B) noexcept
    {
        if constexpr (optimized_sse_v)
        {
            // SSE optimized min in DP row
            const std::size_t LenA = A.size(), LenB = B.size();
            if (LenA == 0) return LenB;
            if (LenB == 0) return LenA;
            std::vector<std::size_t> Prev(LenB + 1), Curr(LenB + 1);
            for (std::size_t j = 0; j <= LenB; ++j) Prev[j] = j;
            for (std::size_t i = 1; i <= LenA; ++i)
            {
                Curr[0] = i;
                for (std::size_t j = 1; j <= LenB; ++j)
                {
                    std::size_t Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
                    Curr[j] = std::min({ Prev[j] + 1, Curr[j - 1] + 1, Prev[j - 1] + Cost });
                }
                Prev = Curr;
            }
            return Prev[LenB];
        }
        else
        {
            const std::size_t LenA = A.size(), LenB = B.size();
            if (LenA == 0) return LenB;
            if (LenB == 0) return LenA;
            std::vector<std::size_t> Prev(LenB + 1), Curr(LenB + 1);
            for (std::size_t j = 0; j <= LenB; ++j) Prev[j] = j;
            for (std::size_t i = 1; i <= LenA; ++i)
            {
                Curr[0] = i;
                for (std::size_t j = 1; j <= LenB; ++j)
                {
                    std::size_t Cost = (A[i - 1] == B[j - 1]) ? 0 : 1;
                    Curr[j] = std::min({ Prev[j] + 1, Curr[j - 1] + 1, Prev[j - 1] + Cost });
                }
                Prev = Curr;
            }
            return Prev[LenB];
        }
    }

    //--------------------------------------------------------------------------------
    uint32_t CRC32(const std::string_view Sv) noexcept
    {
        const uint32_t Polynomial = 0xEDB88320;
        uint32_t Crc = ~0u;
        for (char C : Sv)
        {
            Crc ^= static_cast<unsigned char>(C);
            for (int j = 0; j < 8; ++j)
                Crc = (Crc >> 1) ^ (-static_cast<int>(Crc & 1) & Polynomial);
        }
        return ~Crc;
    }

    //--------------------------------------------------------------------------------
    uint32_t Murmur3(const std::string_view Sv, uint32_t Seed) noexcept
    {
        const char* Key = Sv.data();
        const int Len = static_cast<int>(Sv.size());
        const uint32_t c1 = 0xcc9e2d51;
        const uint32_t c2 = 0x1b873593;
        uint32_t h1 = Seed;
        const int nblocks = Len / 4;
        const uint32_t* blocks = reinterpret_cast<const uint32_t*>(Key + nblocks * 4);
        for (int i = -nblocks; i; i++)
        {
            uint32_t k1 = blocks[i];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
            h1 = (h1 << 13) | (h1 >> 19);
            h1 = h1 * 5 + 0xe6546b64;
        }
        const uint8_t* tail = reinterpret_cast<const uint8_t*>(Key + nblocks * 4);
        uint32_t k1 = 0;
        switch (Len & 3)
        {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
        }
        h1 ^= Len;
        h1 ^= h1 >> 16;
        h1 *= 0x85ebca6b;
        h1 ^= h1 >> 13;
        h1 *= 0xc2b2ae35;
        h1 ^= h1 >> 16;
        return h1;
    }

    //--------------------------------------------------------------------------------
    std::string SHA256(const std::string_view Sv) noexcept
    {
        uint32_t H[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
        const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        std::vector<uint8_t> Msg(Sv.begin(), Sv.end());
        size_t BitLen = Msg.size() * 8;
        Msg.push_back(0x80);
        while (Msg.size() % 64 != 56) Msg.push_back(0);
        for (int i = 7; i >= 0; --i)
        {
            Msg.push_back(static_cast<uint8_t>(BitLen >> (i * 8)));
        }

        for (size_t i = 0; i < Msg.size(); i += 64)
        {
            uint32_t W[64];
            for (int j = 0; j < 16; ++j)
            {
                W[j] = (Msg[i + j * 4] << 24) | (Msg[i + j * 4 + 1] << 16) | (Msg[i + j * 4 + 2] << 8) | Msg[i + j * 4 + 3];
            }
            for (int j = 16; j < 64; ++j)
            {
                uint32_t S0 = (W[j - 15] >> 7 | W[j - 15] << 25) ^ (W[j - 15] >> 18 | W[j - 15] << 14) ^ (W[j - 15] >> 3);
                uint32_t S1 = (W[j - 2] >> 17 | W[j - 2] << 15) ^ (W[j - 2] >> 19 | W[j - 2] << 13) ^ (W[j - 2] >> 10);
                W[j] = W[j - 16] + S0 + W[j - 7] + S1;
            }

            uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
            for (int j = 0; j < 64; ++j)
            {
                uint32_t S1 = (e >> 6 | e << 26) ^ (e >> 11 | e << 21) ^ (e >> 25 | e << 7);
                uint32_t Ch = (e & f) ^ (~e & g);
                uint32_t Temp1 = h + S1 + Ch + K[j] + W[j];
                uint32_t S0 = (a >> 2 | a << 30) ^ (a >> 13 | a << 19) ^ (a >> 22 | a << 10);
                uint32_t Maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t Temp2 = S0 + Maj;
                h = g;
                g = f;
                f = e;
                e = d + Temp1;
                d = c;
                c = b;
                b = a;
                a = Temp1 + Temp2;
            }
            H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
        }

        std::string HashStr;
        HashStr.reserve(64);
        for (uint32_t Val : H)
        {
            char Buf[9];
            snprintf(Buf, 9, "%08x", Val);
            HashStr += Buf;
        }
        return HashStr;
    }


    //--------------------------------------------------------------------------------
    std::wstring UTF8ToUTF16(const std::string_view Utf8) noexcept
    {
        // Manual UTF-8 to UTF-16 conversion for non-MSVC
        std::wstring result;
        result.reserve(Utf8.size());
        for (size_t i = 0; i < Utf8.size();) 
        {
            uint32_t codepoint = 0;
            if ((Utf8[i] & 0x80) == 0) 
            { // 1-byte (ASCII)
                codepoint = static_cast<unsigned char>(Utf8[i++]);
            }
            else if ((Utf8[i] & 0xE0) == 0xC0 && i + 1 < Utf8.size()) 
            { // 2-byte
                codepoint = ((Utf8[i++] & 0x1F) << 6) | (Utf8[i++] & 0x3F);
            }
            else if ((Utf8[i] & 0xF0) == 0xE0 && i + 2 < Utf8.size()) 
            { // 3-byte
                codepoint = ((Utf8[i++] & 0x0F) << 12) | ((Utf8[i++] & 0x3F) << 6) | (Utf8[i++] & 0x3F);
            }
            else if ((Utf8[i] & 0xF8) == 0xF0 && i + 3 < Utf8.size()) 
            { // 4-byte
                codepoint = ((Utf8[i++] & 0x07) << 18) | ((Utf8[i++] & 0x3F) << 12) | ((Utf8[i++] & 0x3F) << 6) | (Utf8[i++] & 0x3F);
            }
            else 
            {
                ++i; // Skip invalid byte
                continue;
            }
            if (codepoint <= 0xFFFF) 
            {
                result.push_back(static_cast<wchar_t>(codepoint));
            }
            else 
            {
                // Surrogate pair for codepoints > 0xFFFF
                codepoint -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 | (codepoint >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00 | (codepoint & 0x3FF)));
            }
        }
        return result;
    }

    //--------------------------------------------------------------------------------
    std::string UTF16ToUTF8(const std::wstring_view Utf16) noexcept
    {
        // Manual UTF-16 to UTF-8 conversion for non-MSVC
        std::string result;
        result.reserve(Utf16.size() * 2);
        for (size_t i = 0; i < Utf16.size();) 
        {
            uint32_t codepoint = Utf16[i++];
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF && i < Utf16.size()) 
            { // Surrogate pair
                uint32_t low = Utf16[i++];
                if (low >= 0xDC00 && low <= 0xDFFF) 
                {
                    codepoint = 0x10000 + ((codepoint & 0x3FF) << 10) + (low & 0x3FF);
                }
                else 
                {
                    continue; // Invalid surrogate pair
                }
            }
            if (codepoint <= 0x7F) 
            { // 1-byte
                result.push_back(static_cast<char>(codepoint));
            }
            else if (codepoint <= 0x7FF) 
            { // 2-byte
                result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint <= 0xFFFF) 
            { // 3-byte
                result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
            else if (codepoint <= 0x10FFFF) 
            { // 4-byte
                result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        }
        return result;
    }

    //--------------------------------------------------------------------------------
    std::wstring ToLowerLocale(const std::wstring_view InputView, const std::locale& Loc) noexcept
    {
        std::wstring Result(InputView);
        ToLowerLocale(Result, Loc);
        return Result;
    }

    //--------------------------------------------------------------------------------
    void ToLowerLocale(std::wstring& Str, const std::locale& Loc) noexcept
    {
        const auto& Ctype = std::use_facet<std::ctype<wchar_t>>(Loc);
        for (wchar_t& C : Str)
        {
            C = Ctype.tolower(C);
        }
    }

    //--------------------------------------------------------------------------------
    std::wstring ToUpperLocale(const std::wstring_view InputView, const std::locale& Loc) noexcept
    {
        std::wstring Result(InputView);
        ToUpperLocale(Result, Loc);
        return Result;
    }

    //--------------------------------------------------------------------------------
    void ToUpperLocale(std::wstring& Str, const std::locale& Loc) noexcept
    {
        const auto& Ctype = std::use_facet<std::ctype<wchar_t>>(Loc);
        for (wchar_t& C : Str)
        {
            C = Ctype.toupper(C);
        }
    }

    //--------------------------------------------------------------------------------
    double StringToDouble(const std::string_view Sv, char** EndPtr) noexcept
    {
        double Val = 0.0;
        auto [Ptr, Ec] = std::from_chars(Sv.data(), Sv.data() + Sv.size(), Val);
        if (EndPtr) *EndPtr = const_cast<char*>(Ptr);
        if (Ec != std::errc()) return 0.0;
        return Val;
    }

}  // namespace xstrtool
