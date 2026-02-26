#ifndef XSTRTOOL_H
#define XSTRTOOL_H

#include <chrono>
#include <locale>   // For locale-aware case
#include <string>
#include <string_view>
#include <vector>
#include <cassert>

namespace xstrtool
{
    // Makes std::format be compatible with std::string_views and such...
/*
    template<typename T, std::size_t N, typename... T_ARGS>
    inline auto format(const T(&fmt)[N], T_ARGS&&... Args)
    {
        if constexpr (std::is_same_v<T, char>)
        {
            return std::string(std::format(std::basic_format_string<T, T_ARGS...>{fmt}, std::forward<T_ARGS>(Args)...));
        }
        else if constexpr (std::is_same_v<T, wchar_t>)
        {
            return std::wstring(std::format(std::basic_format_string<T, T_ARGS...>{fmt}, std::forward<T_ARGS>(Args)...));
        }
        else
        {
            static_assert(std::is_same_v<T, char> || std::is_same_v<T, wchar_t>, "Unsupported character type for format string");
        }
    }

    template<typename T, std::size_t N, typename... T_ARGS>
    auto format(const T(&Fmt)[N], const T_ARGS&... Args)
    {
        if constexpr (std::is_same_v<T, char>)
        {
            return std::string(std::vformat(Fmt, std::make_format_args(Args...)));
        }
        else if constexpr (std::is_same_v<T, wchar_t>)
        {
            return std::wstring(std::vformat(Fmt, std::make_format_args(Args...)));
        }
        else
        {
            static_assert(std::is_same_v<T, char> || std::is_same_v<T, wchar_t>, "Unsupported character type for format string");
        }
    }
    */

    // Converts a single char to lowercase (English ASCII only).
    // @param Char The character to convert.
    // @return The lowercase equivalent if uppercase letter; otherwise unchanged.
    constexpr char ToLower(const char Char) noexcept
    {
        return (Char >= 'A' && Char <= 'Z') ? static_cast<char>(Char + ('a' - 'A')) : Char;
    }

    // Converts a single wchar_t to lowercase (English ASCII only).
    // @param Char The wide character to convert.
    // @return The lowercase equivalent if uppercase letter; otherwise unchanged.
    constexpr wchar_t ToLower(const wchar_t Char) noexcept
    {
        return (Char >= L'A' && Char <= L'Z') ? static_cast<wchar_t>(Char + (L'a' - L'A')) : Char;
    }

    // Converts a single char to uppercase (English ASCII only).
    // @param Char The character to convert.
    // @return The uppercase equivalent if lowercase letter; otherwise unchanged.
    constexpr char ToUpper(const char Char) noexcept
    {
        return (Char >= 'a' && Char <= 'z') ? static_cast<char>(Char - ('a' - 'A')) : Char;
    }

    // Converts a single wchar_t to uppercase (English ASCII only).
    // @param Char The wide character to convert.
    // @return The uppercase equivalent if lowercase letter; otherwise unchanged.
    constexpr wchar_t ToUpper(const wchar_t Char) noexcept
    {
        return (Char >= L'a' && Char <= L'z') ? static_cast<wchar_t>(Char - (L'a' - L'A')) : Char;
    }

    // Converts narrow string_view to wide string (lossy, assumes ASCII).
    // @param InputView Input narrow string view.
    // @return Converted wide string.
    std::wstring To(const std::string_view InputView) noexcept;

    // Converts null-terminated narrow string to wide string (lossy, assumes ASCII).
    // @param Input Null-terminated narrow string.
    // @return Converted wide string.
    std::wstring To(const char* Input) noexcept;

    // Converts wide string_view to narrow string (lossy if wchar_t > 255).
    // @param InputView Input wide string view.
    // @return Converted narrow string.
    std::string To(const std::wstring_view InputView) noexcept;

    // Converts null-terminated wide string to narrow string (lossy if wchar_t > 255).
    // @param Input Null-terminated wide string.
    // @return Converted narrow string.
    std::string To(const wchar_t* Input) noexcept;

    // Copies n characters from a string view to a new string.
    // @param Source Source string view.
    // @param N Number of characters to copy.
    // @return New string containing up to N characters from Source.
    std::string CopyN(const std::string_view Source, std::size_t N) noexcept;

    // Copies n characters from a null-terminated string to a new string.
    // @param Source Null-terminated source string.
    // @param N Number of characters to copy.
    // @return New string containing up to N characters from Source.
    std::string CopyN(const char* Source, std::size_t N) noexcept;

    // Copies n characters from a wide string view to a new wide string.
    // @param Source Source wide string view.
    // @param N Number of characters to copy.
    // @return New wide string containing up to N characters from Source.
    std::wstring CopyN(const std::wstring_view Source, std::size_t N) noexcept;

    // Copies n characters from a null-terminated wide string to a new wide string.
    // @param Source Null-terminated wide source string.
    // @param N Number of characters to copy.
    // @return New wide string containing up to N characters from Source.
    std::wstring CopyN(const wchar_t* Source, std::size_t N) noexcept;

    // Creates a lowercase copy of the narrow string_view (English ASCII).
    // @param InputView Input string view.
    // @return Lowercase string copy.
    std::string ToLowerCopy(const std::string_view InputView) noexcept;

    // Creates a lowercase copy of the null-terminated narrow string (English ASCII).
    // @param Input Null-terminated narrow string.
    // @return Lowercase string copy.
    std::string ToLowerCopy(const char* Input) noexcept;

    // Converts narrow string to lowercase in-place (English ASCII).
    // @param Str Narrow string to modify.
    void ToLower(std::string& Str) noexcept;

    // Creates a lowercase copy of the wide string_view (English ASCII).
    // @param InputView Input wide string view.
    // @return Lowercase wide string copy.
    std::wstring ToLowerCopy(const std::wstring_view InputView) noexcept;

    // Creates a lowercase copy of the null-terminated wide string (English ASCII).
    // @param Input Null-terminated wide string.
    // @return Lowercase wide string copy.
    std::wstring ToLowerCopy(const wchar_t* Input) noexcept;

    // Converts wide string to lowercase in-place (English ASCII).
    // @param Str Wide string to modify.
    void ToLower(std::wstring& Str) noexcept;

    // Creates an uppercase copy of the narrow string_view (English ASCII).
    // @param InputView Input string view.
    // @return Uppercase string copy.
    std::string ToUpperCopy(const std::string_view InputView) noexcept;

    // Creates an uppercase copy of the null-terminated narrow string (English ASCII).
    // @param Input Null-terminated narrow string.
    // @return Uppercase string copy.
    std::string ToUpperCopy(const char* Input) noexcept;

    // Converts narrow string to uppercase in-place (English ASCII).
    // @param Str Narrow string to modify.
    void ToUpper(std::string& Str) noexcept;

    // Creates an uppercase copy of the wide string_view (English ASCII).
    // @param InputView Input wide string view.
    // @return Uppercase wide string copy.
    std::wstring ToUpperCopy(const std::wstring_view InputView) noexcept;

    // Creates an uppercase copy of the null-terminated wide string (English ASCII).
    // @param Input Null-terminated wide string.
    // @return Uppercase wide string copy.
    std::wstring ToUpperCopy(const wchar_t* Input) noexcept;

    // Converts wide string to uppercase in-place (English ASCII).
    // @param Str Wide string to modify.
    void ToUpper(std::wstring& Str) noexcept;

    // Compares two narrow strings case-insensitively (English ASCII).
    // @param A First string view.
    // @param B Second string view.
    // @return Negative if A < B, positive if A > B, zero if equal (lexicographical).
    int CompareI(const std::string_view A, const std::string_view B) noexcept;

    // Compares two null-terminated narrow strings case-insensitively (English ASCII).
    // @param A First null-terminated string.
    // @param B Second null-terminated string.
    // @return Negative if A < B, positive if A > B, zero if equal (lexicographical).
    int CompareI(const char* A, const char* B) noexcept;

    // Compares two wide strings case-insensitively (English ASCII).
    // @param A First wide string view.
    // @param B Second wide string view.
    // @return Negative if A < B, positive if A > B, zero if equal (lexicographical).
    int CompareI(const std::wstring_view A, const std::wstring_view B) noexcept;

    // Compares two null-terminated wide strings case-insensitively (English ASCII).
    // @param A First null-terminated wide string.
    // @param B Second null-terminated wide string.
    // @return Negative if A < B, positive if A > B, zero if equal (lexicographical).
    int CompareI(const wchar_t* A, const wchar_t* B) noexcept;

    // Finds substring position in narrow haystack case-insensitively (English ASCII).
    // @param Haystack Haystack string view.
    // @param Needle Needle string view to find.
    // @param Pos Starting position (default 0).
    // @return Position if found; std::string::npos otherwise.
    std::size_t findI(const std::string_view Haystack, const std::string_view Needle, const std::size_t Pos = 0) noexcept;

    // Finds substring position in null-terminated narrow haystack case-insensitively (English ASCII).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle string to find.
    // @param Pos Starting position (default 0).
    // @return Position if found; std::string::npos otherwise.
    std::size_t findI(const char* Haystack, const char* Needle, const std::size_t Pos = 0) noexcept;

    // Finds substring position in wide haystack case-insensitively (English ASCII).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle wide string view to find.
    // @param Pos Starting position (default 0).
    // @return Position if found; std::wstring::npos otherwise.
    std::size_t findI(const std::wstring_view Haystack, const std::wstring_view Needle, const std::size_t Pos = 0) noexcept;

    // Finds substring position in null-terminated wide haystack case-insensitively (English ASCII).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle string to find.
    // @param Pos Starting position (default 0).
    // @return Position if found; std::wstring::npos otherwise.
    std::size_t findI(const wchar_t* Haystack, const wchar_t* Needle, const std::size_t Pos = 0) noexcept;

    // Finds last substring position in haystack case-insensitively (English ASCII).
    // @param Haystack Haystack string view.
    // @param Needle Needle string view to find.
    // @param Pos Starting position for reverse search (default std::string::npos, end of Haystack).
    // @return Last position if found; std::string::npos otherwise.
    std::size_t rfindI(const std::string_view Haystack, const std::string_view Needle, std::size_t Pos = std::string::npos) noexcept;

    // Finds last substring position in null-terminated haystack case-insensitively (English ASCII).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle string to find.
    // @param Pos Starting position for reverse search (default std::string::npos, end of Haystack).
    // @return Last position if found; std::string::npos otherwise.
    std::size_t rfindI(const char* Haystack, const char* Needle, std::size_t Pos = std::string::npos) noexcept;

    // Finds last substring position in wide haystack case-insensitively (English ASCII).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle wide string view to find.
    // @param Pos Starting position for reverse search (default std::wstring::npos, end of Haystack).
    // @return Last position if found; std::wstring::npos otherwise.
    std::size_t rfindI(const std::wstring_view Haystack, const std::wstring_view Needle, std::size_t Pos = std::wstring::npos) noexcept;

    // Finds last substring position in null-terminated wide haystack case-insensitively (English ASCII).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle string to find.
    // @param Pos Starting position for reverse search (default std::wstring::npos, end of Haystack).
    // @return Last position if found; std::wstring::npos otherwise.
    std::size_t rfindI(const wchar_t* Haystack, const wchar_t* Needle, std::size_t Pos = std::wstring::npos) noexcept;

    // Checks if narrow haystack starts with needle case-insensitively (English ASCII).
    // @param Haystack Haystack string view.
    // @param Needle Needle string view.
    // @return True if starts with; false otherwise.
    bool StartsWithI(const std::string_view Haystack, const std::string_view Needle) noexcept;

    // Checks if null-terminated narrow haystack starts with needle case-insensitively (English ASCII).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle string.
    // @return True if starts with; false otherwise.
    bool StartsWithI(const char* Haystack, const char* Needle) noexcept;

    // Checks if wide haystack starts with needle case-insensitively (English ASCII).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle wide string view.
    // @return True if starts with; false otherwise.
    bool StartsWithI(const std::wstring_view Haystack, const std::wstring_view Needle) noexcept;

    // Checks if null-terminated wide haystack starts with needle case-insensitively (English ASCII).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle string.
    // @return True if starts with; false otherwise.
    bool StartsWithI(const wchar_t* Haystack, const wchar_t* Needle) noexcept;

    // Checks if narrow haystack ends with needle case-insensitively (English ASCII).
    // @param Haystack Haystack string view.
    // @param Needle Needle string view.
    // @return True if ends with; false otherwise.
    bool EndsWithI(const std::string_view Haystack, const std::string_view Needle) noexcept;

    // Checks if null-terminated narrow haystack ends with needle case-insensitively (English ASCII).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle string.
    // @return True if ends with; false otherwise.
    bool EndsWithI(const char* Haystack, const char* Needle) noexcept;

    // Checks if wide haystack ends with needle case-insensitively (English ASCII).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle wide string view.
    // @return True if ends with; false otherwise.
    bool EndsWithI(const std::wstring_view Haystack, const std::wstring_view Needle) noexcept;

    // Checks if null-terminated wide haystack ends with needle case-insensitively (English ASCII).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle string.
    // @return True if ends with; false otherwise.
    bool EndsWithI(const wchar_t* Haystack, const wchar_t* Needle) noexcept;

    // Checks if narrow haystack contains needle case-insensitively (English ASCII).
    // @param Haystack Haystack string view.
    // @param Needle Needle string view.
    // @return True if contains; false otherwise.
    bool ContainsI(const std::string_view Haystack, const std::string_view Needle) noexcept;

    // Checks if null-terminated narrow haystack contains needle case-insensitively (English ASCII).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle string.
    // @return True if contains; false otherwise.
    bool ContainsI(const char* Haystack, const char* Needle) noexcept;

    // Checks if wide haystack contains needle case-insensitively (English ASCII).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle wide string view.
    // @return True if contains; false otherwise.
    bool ContainsI(const std::wstring_view Haystack, const std::wstring_view Needle) noexcept;

    // Checks if null-terminated wide haystack contains needle case-insensitively (English ASCII).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle string.
    // @return True if contains; false otherwise.
    bool ContainsI(const wchar_t* Haystack, const wchar_t* Needle) noexcept;

    // Trims leading whitespace from narrow string_view (space, tab, newline, carriage return).
    // @param InputView Input string view.
    // @return Trimmed view (no copy).
    std::string_view TrimLeftCopy(const std::string_view InputView) noexcept;

    // Trims leading whitespace from null-terminated narrow string (space, tab, newline, carriage return).
    // @param Input Null-terminated string.
    // @return Trimmed view (no copy).
    std::string_view TrimLeftCopy(const char* Input) noexcept;

    // Trims leading whitespace from narrow string in-place.
    // @param Str Narrow string to modify.
    void TrimLeft(std::string& Str) noexcept;

    // Trims trailing whitespace from narrow string_view.
    // @param InputView Input string view.
    // @return Trimmed view (no copy).
    std::string_view TrimRightCopy(const std::string_view InputView) noexcept;

    // Trims trailing whitespace from null-terminated narrow string.
    // @param Input Null-terminated string.
    // @return Trimmed view (no copy).
    std::string_view TrimRightCopy(const char* Input) noexcept;

    // Trims trailing whitespace from narrow string in-place.
    // @param Str Narrow string to modify.
    void TrimRight(std::string& Str) noexcept;

    // Trims leading and trailing whitespace from narrow string_view.
    // @param InputView Input string view.
    // @return Trimmed view (no copy).
    std::string_view TrimCopy(const std::string_view InputView) noexcept;

    // Trims leading and trailing whitespace from null-terminated narrow string.
    // @param Input Null-terminated string.
    // @return Trimmed view (no copy).
    std::string_view TrimCopy(const char* Input) noexcept;

    // Trims leading and trailing whitespace from narrow string in-place.
    // @param Str Narrow string to modify.
    void Trim(std::string& Str) noexcept;

    // Trims leading whitespace from wide string_view (space, tab, newline, carriage return).
    // @param InputView Input wide string view.
    // @return Trimmed view (no copy).
    std::wstring_view TrimLeftCopy(const std::wstring_view InputView) noexcept;

    // Trims leading whitespace from null-terminated wide string.
    // @param Input Null-terminated wide string.
    // @return Trimmed view (no copy).
    std::wstring_view TrimLeftCopy(const wchar_t* Input) noexcept;

    // Trims leading whitespace from wide string in-place.
    // @param Str Wide string to modify.
    void TrimLeft(std::wstring& Str) noexcept;

    // Trims trailing whitespace from wide string_view.
    // @param InputView Input wide string view.
    // @return Trimmed view (no copy).
    std::wstring_view TrimRightCopy(const std::wstring_view InputView) noexcept;

    // Trims trailing whitespace from null-terminated wide string.
    // @param Input Null-terminated wide string.
    // @return Trimmed view (no copy).
    std::wstring_view TrimRightCopy(const wchar_t* Input) noexcept;

    // Trims trailing whitespace from wide string in-place.
    // @param Str Wide string to modify.
    void TrimRight(std::wstring& Str) noexcept;

    // Trims leading and trailing whitespace from wide string_view.
    // @param InputView Input wide string view.
    // @return Trimmed view (no copy).
    std::wstring_view TrimCopy(const std::wstring_view InputView) noexcept;

    // Trims leading and trailing whitespace from null-terminated wide string.
    // @param Input Null-terminated wide string.
    // @return Trimmed view (no copy).
    std::wstring_view TrimCopy(const wchar_t* Input) noexcept;

    // Trims leading and trailing whitespace from wide string in-place.
    // @param Str Wide string to modify.
    void Trim(std::wstring& Str) noexcept;

    // Splits narrow string_view by char delimiter (includes empty parts only if not consecutive).
    // @param InputView Input string view.
    // @param Delim Delimiter character.
    // @return Vector of string views (no copies).
    std::vector<std::string_view> Split(const std::string_view InputView, const char Delim) noexcept;

    // Splits null-terminated narrow string by char delimiter.
    // @param Input Null-terminated string.
    // @param Delim Delimiter character.
    // @return Vector of string views (no copies).
    std::vector<std::string_view> Split(const char* Input, const char Delim) noexcept;

    // Splits narrow string_view by string delimiter (includes empty parts only if not consecutive).
    // @param InputView Input string view.
    // @param Delim Delimiter string view.
    // @return Vector of string views (no copies).
    std::vector<std::string_view> Split(const std::string_view InputView, const std::string_view Delim) noexcept;

    // Splits null-terminated narrow string by string delimiter.
    // @param Input Null-terminated string.
    // @param Delim Null-terminated delimiter string.
    // @return Vector of string views (no copies).
    std::vector<std::string_view> Split(const char* Input, const char* Delim) noexcept;

    // Splits wide string_view by wchar_t delimiter (includes empty parts only if not consecutive).
    // @param InputView Input wide string view.
    // @param Delim Delimiter wide character.
    // @return Vector of wide string views (no copies).
    std::vector<std::wstring_view> Split(const std::wstring_view InputView, const wchar_t Delim) noexcept;

    // Splits null-terminated wide string by wchar_t delimiter.
    // @param Input Null-terminated wide string.
    // @param Delim Delimiter wide character.
    // @return Vector of wide string views (no copies).
    std::vector<std::wstring_view> Split(const wchar_t* Input, const wchar_t Delim) noexcept;

    // Splits wide string_view by wide string delimiter (includes empty parts only if not consecutive).
    // @param InputView Input wide string view.
    // @param Delim Delimiter wide string view.
    // @return Vector of wide string views (no copies).
    std::vector<std::wstring_view> Split(const std::wstring_view InputView, const std::wstring_view Delim) noexcept;

    // Splits null-terminated wide string by wide string delimiter.
    // @param Input Null-terminated wide string.
    // @param Delim Null-terminated wide delimiter string.
    // @return Vector of wide string views (no copies).
    std::vector<std::wstring_view> Split(const wchar_t* Input, const wchar_t* Delim) noexcept;

    // Joins a range of narrow string-like parts with delimiter.
    // @param T_RANGE Container of string_view compatible types.
    // @param Parts Range to join.
    // @param Delim Delimiter string view.
    // @return Joined string.
    template <typename T_RANGE>
    std::string Join(const T_RANGE& Parts, const std::string_view Delim) noexcept
    {
        std::string Result;
        bool First = true;
        for (const auto& Part : Parts)
        {
            assert(!Part.empty() && "Joining empty part detected");
            if (!First)
            {
                Result.append(Delim);
            }
            First = false;
            Result.append(Part);
        }
        return Result;
    }

    // Joins a range of null-terminated narrow strings with delimiter.
    // @param T_RANGE Container of null-terminated strings.
    // @param Parts Range to join.
    // @param Delim Null-terminated delimiter string.
    // @return Joined string.
    template <typename T_RANGE>
    std::string Join(const T_RANGE& Parts, const char* Delim) noexcept
    {
        assert(Delim != nullptr && "Delimiter string is null");
        return Join(Parts, std::string_view(Delim));
    }

    // Joins a range of wide string-like parts with delimiter.
    // @param T_RANGE Container of wstring_view compatible types.
    // @param Parts Range to join.
    // @param Delim Delimiter wide string view.
    // @return Joined wide string.
    template <typename T_RANGE>
    std::wstring Join(const T_RANGE& Parts, const std::wstring_view Delim) noexcept
    {
        std::wstring Result;
        bool First = true;
        for (const auto& Part : Parts)
        {
            assert(!Part.empty() && "Joining empty part detected");
            if (!First)
            {
                Result.append(Delim);
            }
            First = false;
            Result.append(Part);
        }
        return Result;
    }

    // Joins a range of null-terminated wide strings with delimiter.
    // @param T_RANGE Container of null-terminated wide strings.
    // @param Parts Range to join.
    // @param Delim Null-terminated wide delimiter string.
    // @return Joined wide string.
    template <typename T_RANGE>
    std::wstring Join(const T_RANGE& Parts, const wchar_t* Delim) noexcept
    {
        assert(Delim != nullptr && "Delimiter wide string is null");
        return Join(Parts, std::wstring_view(Delim));
    }

    // Replaces all occurrences of needle with replacement in narrow haystack (case-sensitive).
    // @param Haystack Haystack string view.
    // @param Needle Needle to find.
    // @param Replacement Replacement string view.
    // @return New string with replacements.
    std::string ReplaceCopy(const std::string_view Haystack, const std::string_view Needle, const std::string_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in null-terminated narrow haystack (case-sensitive).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle.
    // @param Replacement Null-terminated replacement.
    // @return New string with replacements.
    std::string ReplaceCopy(const char* Haystack, const char* Needle, const char* Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in narrow string in-place (case-sensitive).
    // @param Str String to modify.
    // @param Needle Needle to find.
    // @param Replacement Replacement string view.
    void Replace(std::string& Str, const std::string_view Needle, const std::string_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in wide haystack (case-sensitive).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle to find.
    // @param Replacement Replacement wide string view.
    // @return New wide string with replacements.
    std::wstring ReplaceCopy(const std::wstring_view Haystack, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in null-terminated wide haystack (case-sensitive).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle.
    // @param Replacement Null-terminated wide replacement.
    // @return New wide string with replacements.
    std::wstring ReplaceCopy(const wchar_t* Haystack, const wchar_t* Needle, const wchar_t* Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in wide string in-place (case-sensitive).
    // @param Str Wide string to modify.
    // @param Needle Needle to find.
    // @param Replacement Replacement wide string view.
    void Replace(std::wstring& Str, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in narrow haystack case-insensitively (English ASCII).
    // @param Haystack Haystack string view.
    // @param Needle Needle to find (case-insensitive).
    // @param Replacement Replacement string view.
    // @return New string with replacements.
    std::string ReplaceICopy(const std::string_view Haystack, const std::string_view Needle, const std::string_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in null-terminated narrow haystack case-insensitively (English ASCII).
    // @param Haystack Null-terminated haystack string.
    // @param Needle Null-terminated needle.
    // @return New string with replacements.
    std::string ReplaceICopy(const char* Haystack, const char* Needle, const char* Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in narrow string in-place case-insensitively (English ASCII).
    // @param Str String to modify.
    // @param Needle Needle to find (case-insensitive).
    // @param Replacement Replacement string view.
    void ReplaceI(std::string& Str, const std::string_view Needle, const std::string_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in wide haystack case-insensitively (English ASCII).
    // @param Haystack Haystack wide string view.
    // @param Needle Needle to find (case-insensitive).
    // @param Replacement Replacement wide string view.
    // @return New wide string with replacements.
    std::wstring ReplaceICopy(const std::wstring_view Haystack, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in null-terminated wide haystack case-insensitively (English ASCII).
    // @param Haystack Null-terminated wide haystack string.
    // @param Needle Null-terminated wide needle.
    // @param Replacement Null-terminated wide replacement.
    // @return New wide string with replacements.
    std::wstring ReplaceICopy(const wchar_t* Haystack, const wchar_t* Needle, const wchar_t* Replacement) noexcept;

    // Replaces all occurrences of needle with replacement in wide string in-place case-insensitively (English ASCII).
    // @param Str Wide string to modify.
    // @param Needle Needle to find (case-insensitive).
    // @param Replacement Replacement wide string view.
    void ReplaceI(std::wstring& Str, const std::wstring_view Needle, const std::wstring_view Replacement) noexcept;

    // Gets const base name (file name with extension) from narrow path view.
    // @param Path Path string view.
    // @return Base name view.
    inline std::string_view PathBaseName(const std::string_view Path) noexcept
    {
        const std::size_t Pos = Path.find_last_of("/\\");
        return (Pos != std::string_view::npos) ? Path.substr(Pos + 1) : Path;
    }

    // Gets the base name from null-terminated narrow path.
    // @param Path Null-terminated path string.
    // @return Base name view.
    inline std::string_view PathBaseName(const char* Path) noexcept
    {
        assert(Path != nullptr && "Path string is null");
        return PathBaseName(std::string_view(Path));
    }

    // Gets the directory name from narrow path view.
    // @param Path Path string view.
    // @return Directory name view (empty if no dir).
    inline std::string_view PathDirName(const std::string_view Path) noexcept
    {
        const std::size_t Pos = Path.find_last_of("/\\");
        return (Pos != std::string_view::npos) ? Path.substr(0, Pos) : std::string_view{};
    }

    // Gets the directory name from null-terminated narrow path.
    // @param Path Null-terminated path string.
    // @return Directory name view.
    inline std::string_view PathDirName(const char* Path) noexcept
    {
        assert(Path != nullptr && "Path string is null");
        return PathDirName(std::string_view(Path));
    }

    // Gets the extension from narrow path view (including dot).
    // @param Path Path string view.
    // @return Extension view (empty if none).
    inline std::string_view PathExtension(const std::string_view Path) noexcept
    {
        const auto LastDot = Path.rfind('.');
        if (LastDot == std::string_view::npos || LastDot == Path.size() - 1)
        {
            return {}; // No extension or dot is last character
        }
        return Path.substr(LastDot);
    }

    // Gets the extension from null-terminated narrow path.
    // @param Path Null-terminated path string.
    // @return Extension view.
    inline std::string_view PathExtension(const char* Path) noexcept
    {
        assert(Path != nullptr && "Path string is null");
        return PathExtension(std::string_view(Path));
    }

    // Gets the path without extension from narrow path view.
    // @param Path Path string view.
    // @return Path without extension view.
    inline std::string_view PathWithoutExtension(const std::string_view Path) noexcept
    {
        const auto LastDot = Path.rfind('.');
        if (LastDot == std::string_view::npos || LastDot == 0)
        {
            return Path; // No extension or dot at start
        }
        return Path.substr(0, LastDot);
    }

    // Gets the path without extension from null-terminated narrow path.
    // @param Path Null-terminated path string.
    // @return Path without extension view.
    inline std::string_view PathWithoutExtension(const char* Path) noexcept
    {
        assert(Path != nullptr && "Path string is null");
        return PathWithoutExtension(std::string_view(Path));
    }

    // Joins two narrow path parts with separator if needed.
    // @param Base Base path view.
    // @param Part Part to append.
    // @return Joined path string.
    std::string PathJoin(const std::string_view Base, const std::string_view Part) noexcept;

    // Joins two null-terminated narrow path parts.
    // @param Base Null-terminated base path.
    // @param Part Null-terminated part.
    // @return Joined path string.
    std::string PathJoin(const char* Base, const char* Part) noexcept;

    // Normalizes narrow path (replaces \ with /, removes duplicate /, resolves . and .., preserves Windows drive letters).
    // @param Path Path view.
    // @return Normalized path string.
    std::string PathNormalizeCopy(const std::string_view Path) noexcept;

    // Normalizes null-terminated narrow path.
    // @param Path Null-terminated path.
    // @return Normalized path string.
    std::string PathNormalizeCopy(const char* Path) noexcept;

    // Normalizes narrow path in-place.
    // @param Str Path string to modify.
    void PathNormalize(std::string& Str) noexcept;

    // Gets the base name (file name with extension) from wide path view.
    // @param Path Wide path string view.
    // @return Base name wide view.
    inline std::wstring_view PathBaseName(const std::wstring_view Path) noexcept
    {
        const std::size_t Pos = Path.find_last_of(L"/\\");
        return (Pos != std::wstring_view::npos) ? Path.substr(Pos + 1) : Path;
    }

    // Gets the base name from null-terminated wide path.
    // @param Path Null-terminated wide path string.
    // @return Base name wide view.
    inline std::wstring_view PathBaseName(const wchar_t* Path) noexcept
    {
        assert(Path != nullptr && "Path wide string is null");
        return PathBaseName(std::wstring_view(Path));
    }

    // Gets the directory name from wide path view.
    // @param Path Wide path string view.
    // @return Directory name wide view (empty if no dir).
    inline std::wstring_view PathDirName(const std::wstring_view Path) noexcept
    {
        const std::size_t Pos = Path.find_last_of(L"/\\");
        return (Pos != std::wstring_view::npos) ? Path.substr(0, Pos) : std::wstring_view{};
    }

    // Gets the directory name from null-terminated wide path.
    // @param Path Null-terminated wide path string.
    // @return Directory name wide view.
    inline std::wstring_view PathDirName(const wchar_t* Path) noexcept
    {
        assert(Path != nullptr && "Path wide string is null");
        return PathDirName(std::wstring_view(Path));
    }

    // Gets the extension from wide path view (including dot).
    // @param Path Wide path string view.
    // @return Extension wide view (empty if none).
    inline std::wstring_view PathExtension(const std::wstring_view Path) noexcept
    {
        const auto LastDot = Path.rfind(L'.');
        if (LastDot == std::wstring_view::npos || LastDot == Path.size() - 1)
        {
            return std::wstring_view(); // No extension or dot is last character
        }
        return Path.substr(LastDot);
    }

    // Gets the extension from null-terminated wide path.
    // @param Path Null-terminated wide path string.
    // @return Extension wide view.
    inline std::wstring_view PathExtension(const wchar_t* Path) noexcept
    {
        assert(Path != nullptr && "Path wide string is null");
        return PathExtension(std::wstring_view(Path));
    }

    // Gets the path without extension from wide path view.
    // @param Path Wide path string view.
    // @return Path without extension wide view.
    inline std::wstring_view PathWithoutExtension(const std::wstring_view Path) noexcept
    {
        const auto LastDot = Path.rfind(L'.');
        if (LastDot == std::wstring_view::npos || LastDot == 0)
        {
            return Path; // No extension or dot at start
        }
        return Path.substr(0, LastDot);
    }

    // Gets the path without extension from null-terminated wide path.
    // @param Path Null-terminated wide path string.
    // @return Path without extension wide view.
    inline std::wstring_view PathWithoutExtension(const wchar_t* Path) noexcept
    {
        assert(Path != nullptr && "Path wide string is null");
        return PathWithoutExtension(std::wstring_view(Path));
    }

    // Joins two wide path parts with separator if needed.
    // @param Base Base wide path view.
    // @param Part Part to append.
    // @return Joined wide path string.
    std::wstring PathJoin(const std::wstring_view Base, const std::wstring_view Part) noexcept;

    // Joins two null-terminated wide path parts.
    // @param Base Null-terminated wide base path.
    // @param Part Null-terminated wide part.
    // @return Joined wide path string.
    std::wstring PathJoin(const wchar_t* Base, const wchar_t* Part) noexcept;

    // Normalizes wide path (replaces \ with /, removes duplicate /, resolves . and .., preserves Windows drive letters).
    // @param Path Wide path view.
    // @return Normalized wide path string.
    std::wstring PathNormalizeCopy(const std::wstring_view Path) noexcept;

    // Normalizes null-terminated wide path.
    // @param Path Null-terminated wide path.
    // @return Normalized wide path string.
    std::wstring PathNormalizeCopy(const wchar_t* Path) noexcept;

    // Normalizes wide path in-place.
    // @param Str Wide path string to modify.
    void PathNormalize(std::wstring& Str) noexcept;

    // Formats a time point to string using std::format.
    // @param Tp Time point.
    // @param Fmt Format string (default "%Y-%m-%d %H:%M:%S").
    // @return Formatted string.
    std::string FormatTime(const std::chrono::system_clock::time_point Tp, const std::string_view Fmt = "%Y-%m-%d %H:%M:%S") noexcept;

    // Pads string left with fill char to width (copy).
    // @param Sv String view.
    // @param Width Target width.
    // @param Fill Fill char (default space).
    // @return Padded string.
    std::string PadLeftCopy(const std::string_view Sv, const std::size_t Width, const char Fill = ' ') noexcept;

    // Pads string left in-place.
    // @param Str String to modify.
    // @param Width Target width.
    // @param Fill Fill char (default space).
    void PadLeft(std::string& Str, const std::size_t Width, const char Fill = ' ') noexcept;

    // Pads string right with fill char to width (copy).
    // @param Sv String view.
    // @param Width Target width.
    // @param Fill Fill char (default space).
    // @return Padded string.
    std::string PadRightCopy(const std::string_view Sv, const std::size_t Width, const char Fill = ' ') noexcept;

    // Pads string right in-place.
    // @param Str String to modify.
    // @param Width Target width.
    // @param Fill Fill char (default space).
    void PadRight(std::string& Str, const std::size_t Width, const char Fill = ' ') noexcept;

    // Repeats string view n times.
    // @param Sv String view to repeat.
    // @param Count Number of repeats.
    // @return Repeated string.
    std::string Repeat(const std::string_view Sv, const std::size_t Count) noexcept;

    // Encodes data to base64 string.
    // @param Data Data view.
    // @return Base64 string.
    std::string Base64Encode(const std::string_view Data) noexcept;

    // Decodes base64 string to data.
    // @param Base64 Base64 string view.
    // @return Decoded string.
    std::string Base64Decode(const std::string_view Base64) noexcept;

    // URL encodes string.
    // @param Url URL view.
    // @return Encoded URL string.
    std::string URLEncode(const std::string_view Url) noexcept;

    // URL decodes string.
    // @param Encoded Encoded URL view.
    // @return Decoded URL string.
    std::string URLDecode(const std::string_view Encoded) noexcept;

    // Hashes string view using std::hash.
    // @param Sv String view.
    // @return Hash value.
    std::size_t Hash(const std::string_view Sv) noexcept;

    // Converts double to string with precision.
    // @param Num Number.
    // @param Precision Decimal places (default 6).
    // @return String representation.
    std::string ToString(const double Num, const int Precision = 6) noexcept;

    // Computes Levenshtein distance between two strings (edit distance).
    // @param A First string view.
    // @param B Second string view.
    // @return Minimum number of edits to transform A to B.
    std::size_t LevenshteinDistance(const std::string_view A, const std::string_view B) noexcept;

    // Computes CRC32 hash of string view.
    // @param Sv String view.
    // @return CRC32 hash value.
    uint32_t CRC32(const std::string_view Sv) noexcept;

    // Computes Murmur3 hash of string view (32-bit).
    // @param Sv String view.
    // @param Seed Seed value (default 0).
    // @return Murmur3 hash value.
    uint32_t Murmur3(const std::string_view Sv, uint32_t Seed = 0) noexcept;

    // Computes SHA256 hash of string view.
    // @param Sv String view.
    // @return Hex string representation of SHA256 hash.
    std::string SHA256(const std::string_view Sv) noexcept;

    // Converts UTF-8 string to UTF-16 (wide string).
    // @param Utf8 UTF-8 string view.
    // @return UTF-16 wide string.
    std::wstring UTF8ToUTF16(const std::string_view Utf8) noexcept;

    // Converts UTF-16 wide string to UTF-8.
    // @param Utf16 UTF-16 wide string view.
    // @return UTF-8 string.
    std::string UTF16ToUTF8(const std::wstring_view Utf16) noexcept;

    // Creates a lowercase copy of the wide string_view using locale.
    // @param InputView Input wide string view.
    // @param Loc Locale for case conversion (default system locale).
    // @return Lowercase wide string copy.
    std::wstring ToLowerLocale(const std::wstring_view InputView, const std::locale& Loc = std::locale("")) noexcept;

    // Converts wide string to lowercase in-place using locale.
    // @param Str Wide string to modify.
    // @param Loc Locale for case conversion (default system locale).
    void ToLowerLocale(std::wstring& Str, const std::locale& Loc = std::locale("")) noexcept;

    // Creates an uppercase copy of the wide string_view using locale.
    // @param InputView Input wide string view.
    // @param Loc Locale for case conversion (default system locale).
    // @return Uppercase wide string copy.
    std::wstring ToUpperLocale(const std::wstring_view InputView, const std::locale& Loc = std::locale("")) noexcept;

    // Converts wide string to uppercase in-place using locale.
    // @param Str Wide string to modify.
    // @param Loc Locale for case conversion (default system locale).
    void ToUpperLocale(std::wstring& Str, const std::locale& Loc = std::locale("")) noexcept;

    // Converts string to double (fast, using std::from_chars).
    // @param Sv String view.
    // @param EndPtr Pointer to end of parsed string (optional).
    // @return Parsed double (0 on failure).
    double StringToDouble(const std::string_view Sv, char** EndPtr = nullptr) noexcept;

}  // namespace xstrtool

#endif
