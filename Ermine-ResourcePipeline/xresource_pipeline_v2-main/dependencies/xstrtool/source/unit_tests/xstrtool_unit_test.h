#ifndef XSTRTOOL_UNIT_TEST_H
#define XSTRTOOL_UNIT_TEST_H

#include "source/xstrtool.h"
#include <cassert>
#include <chrono>
#include <codecvt>
#include <locale>
#include <string>
#include <string_view>
#include <vector>

namespace xstrtool::unit_test
{
    void TestTo()
    {
        // Narrow to wide
        assert(To(std::string_view("hello")) == L"hello");
        assert(To("hello") == L"hello");
        assert(To(std::string_view("")) == L"");

        // Wide to narrow
        assert(To(std::wstring_view(L"hello")) == "hello");
        assert(To(L"hello") == "hello");
        assert(To(std::wstring_view(L"")) == "");
    }

    void TestToLower()
    {
        // Narrow
        assert(ToLowerCopy(std::string_view("HELLO")) == "hello");
        assert(ToLowerCopy("HeLLo") == "hello");
        assert(ToLowerCopy("") == "");
        std::string s = "HeLLo";
        ToLower(s);
        assert(s == "hello");

        // Wide
        assert(ToLowerCopy(std::wstring_view(L"HELLO")) == L"hello");
        assert(ToLowerCopy(L"HeLLo") == L"hello");
        assert(ToLowerCopy(std::wstring_view(L"")) == L"");
        std::wstring ws = L"HeLLo";
        ToLower(ws);
        assert(ws == L"hello");
    }

    void TestToUpper()
    {
        // Narrow
        assert(ToUpperCopy(std::string_view("hello")) == "HELLO");
        assert(ToUpperCopy("HeLLo") == "HELLO");
        assert(ToUpperCopy("") == "");
        std::string s = "HeLLo";
        ToUpper(s);
        assert(s == "HELLO");

        // Wide
        assert(ToUpperCopy(std::wstring_view(L"hello")) == L"HELLO");
        assert(ToUpperCopy(L"HeLLo") == L"HELLO");
        assert(ToUpperCopy(std::wstring_view(L"")) == L"");
        std::wstring ws = L"HeLLo";
        ToUpper(ws);
        assert(ws == L"HELLO");
    }

    void TestCompareI()
    {
        // Narrow
        assert(CompareI(std::string_view("hello"), std::string_view("HELLO")) == 0);
        assert(CompareI("HeLLo", "hello") == 0);
        assert(CompareI("abc", "DEF") < 0);
        assert(CompareI("DEF", "abc") > 0);
        assert(CompareI("", "") == 0);
        assert(CompareI("abc", "") > 0);

        // Wide
        assert(CompareI(std::wstring_view(L"hello"), std::wstring_view(L"HELLO")) == 0);
        assert(CompareI(L"HeLLo", L"hello") == 0);
        assert(CompareI(L"abc", L"DEF") < 0);
        assert(CompareI(L"DEF", L"abc") > 0);
        assert(CompareI(L"", L"") == 0);
        assert(CompareI(L"abc", L"") > 0);
    }

    void TestFindI()
    {
        // Narrow
        assert(findI("Hello World", "WORLD") == 6);
        assert(findI("Hello World", "world") == 6);
        assert(findI("abc", "d") == std::string::npos);
        assert(findI("", "a") == std::string::npos);
        assert(findI("abc", "") == std::string::npos);
        assert(findI("Hello World", "o", 5) == 7);

        // Wide
        assert(findI(L"Hello World", L"WORLD") == 6);
        assert(findI(L"Hello World", L"world") == 6);
        assert(findI(L"abc", L"d") == std::wstring::npos);
        assert(findI(L"", L"a") == std::wstring::npos);
        assert(findI(L"abc", L"") == std::wstring::npos);
        assert(findI(L"Hello World", L"o", 5) == 7);
    }

    void TestStartsWithI()
    {
        // Narrow
        assert(StartsWithI("Hello World", "HELLO"));
        assert(StartsWithI("Hello World", "hello"));
        assert(!StartsWithI("Hello World", "WORLD"));
        assert(StartsWithI("", ""));
        assert(!StartsWithI("abc", "abcd"));

        // Wide
        assert(StartsWithI(L"Hello World", L"HELLO"));
        assert(StartsWithI(L"Hello World", L"hello"));
        assert(!StartsWithI(L"Hello World", L"WORLD"));
        assert(StartsWithI(L"", L""));
        assert(!StartsWithI(L"abc", L"abcd"));
    }

    void TestEndsWithI()
    {
        // Narrow
        assert(EndsWithI("Hello World", "WORLD"));
        assert(EndsWithI("Hello World", "world"));
        assert(!EndsWithI("Hello World", "HELLO"));
        assert(EndsWithI("", ""));
        assert(!EndsWithI("abc", "abcd"));

        // Wide
        assert(EndsWithI(L"Hello World", L"WORLD"));
        assert(EndsWithI(L"Hello World", L"world"));
        assert(!EndsWithI(L"Hello World", L"HELLO"));
        assert(EndsWithI(L"", L""));
        assert(!EndsWithI(L"abc", L"abcd"));
    }

    void TestContainsI()
    {
        // Narrow
        assert(ContainsI("Hello World", "WORLD"));
        assert(ContainsI("Hello World", "lo"));
        assert(!ContainsI("Hello World", "xyz"));
        assert(!ContainsI("", "a"));
        assert(ContainsI("abc", ""));

        // Wide
        assert(ContainsI(L"Hello World", L"WORLD"));
        assert(ContainsI(L"Hello World", L"lo"));
        assert(!ContainsI(L"Hello World", L"xyz"));
        assert(!ContainsI(L"", L"a"));
        assert(ContainsI(L"abc", L""));
    }

    void TestTrim()
    {
        // Narrow
        assert(TrimLeftCopy("  hello  ") == "hello  ");
        assert(TrimRightCopy("  hello  ") == "  hello");
        assert(TrimCopy("  hello  ") == "hello");
        std::string s = "  hello  ";
        Trim(s);
        assert(s == "hello");
        assert(TrimCopy("") == "");
        assert(TrimLeftCopy("\t\r\n") == "");
        assert(TrimRightCopy("\t\r\n") == "");

        // Wide
        assert(TrimLeftCopy(L"  hello  ") == L"hello  ");
        assert(TrimRightCopy(L"  hello  ") == L"  hello");
        assert(TrimCopy(L"  hello  ") == L"hello");
        std::wstring ws = L"  hello  ";
        Trim(ws);
        assert(ws == L"hello");
        assert(TrimCopy(L"") == L"");
        assert(TrimLeftCopy(L"\t\r\n") == L"");
        assert(TrimRightCopy(L"\t\r\n") == L"");
    }

    void TestSplit()
    {
        // Narrow char
        std::vector<std::string_view> v1 = Split("a,b,c", ',');
        assert(v1.size() == 3 && v1[0] == "a" && v1[1] == "b" && v1[2] == "c");
        v1 = Split("", ',');
        assert(v1.size() == 1 && v1[0] == "");
        v1 = Split("a,,b", ',');
        assert(v1.size() == 3 && v1[0] == "a" && v1[1] == "" && v1[2] == "b");

        // Narrow string
        v1 = Split("abc--def--ghi", "--");
        assert(v1.size() == 3 && v1[0] == "abc" && v1[1] == "def" && v1[2] == "ghi");
        v1 = Split("abc", "--");
        assert(v1.size() == 1 && v1[0] == "abc");

        // Wide char
        std::vector<std::wstring_view> wv1 = Split(L"a,b,c", L',');
        assert(wv1.size() == 3 && wv1[0] == L"a" && wv1[1] == L"b" && wv1[2] == L"c");
        wv1 = Split(L"", L',');
        assert(wv1.size() == 1 && wv1[0] == L"");

        // Wide string
        wv1 = Split(L"abc--def--ghi", L"--");
        assert(wv1.size() == 3 && wv1[0] == L"abc" && wv1[1] == L"def" && wv1[2] == L"ghi");
    }

    void TestJoin()
    {
        // Narrow
        std::vector<std::string_view> v1 = { "a", "b", "c" };
        assert(Join(v1, ",") == "a,b,c");
        assert(Join(v1, "--") == "a--b--c");
        assert(Join(std::vector<std::string_view>{}, ",") == "");

        // Wide
        std::vector<std::wstring_view> wv1 = { L"a", L"b", L"c" };
        assert(Join(wv1, L",") == L"a,b,c");
        assert(Join(wv1, L"--") == L"a--b--c");
        assert(Join(std::vector<std::wstring_view>{}, L",") == L"");
    }

    void TestReplace()
    {
        // Narrow
        assert(ReplaceCopy("Hello World", "World", "Earth") == "Hello Earth");
        assert(ReplaceCopy("a,a,a", ",", ";") == "a;a;a");
        assert(ReplaceCopy("", "a", "b") == "");
        std::string s = "Hello World";
        Replace(s, "World", "Earth");
        assert(s == "Hello Earth");

        // Narrow case-insensitive
        assert(ReplaceICopy("Hello WORLD", "world", "Earth") == "Hello Earth");
        s = "Hello WORLD";
        ReplaceI(s, "world", "Earth");
        assert(s == "Hello Earth");

        // Wide
        assert(ReplaceCopy(L"Hello World", L"World", L"Earth") == L"Hello Earth");
        assert(ReplaceCopy(L"a,a,a", L",", L";") == L"a;a;a");
        std::wstring ws = L"Hello World";
        Replace(ws, L"World", L"Earth");
        assert(ws == L"Hello Earth");

        // Wide case-insensitive
        assert(ReplaceICopy(L"Hello WORLD", L"world", L"Earth") == L"Hello Earth");
        ws = L"Hello WORLD";
        ReplaceI(ws, L"world", L"Earth");
        assert(ws == L"Hello Earth");
    }

    void TestPath()
    {
        // Narrow
        assert(PathBaseName("path/to/file.txt") == "file.txt");
        assert(PathDirName("path/to/file.txt") == "path/to");
        assert(PathExtension("file.txt") == ".txt");
        assert(PathWithoutExtension("file.txt") == "file");
        assert(PathJoin("path/to", "file.txt") == "path/to/file.txt");
        assert(PathNormalizeCopy("path//to/../file.txt") == "path/file.txt");
        assert(PathNormalizeCopy("C:\\folder\\..\\file.txt") == "C:/file.txt");
        assert(PathNormalizeCopy("C:/../..") == "C:");
        assert(PathNormalizeCopy("") == ".");
        std::string s = "path//to/../file.txt";
        PathNormalize(s);
        assert(s == "path/file.txt");

        // Wide
        assert(PathBaseName(L"path/to/file.txt") == L"file.txt");
        assert(PathDirName(L"path/to/file.txt") == L"path/to");
        assert(PathExtension(L"file.txt") == L".txt");
        assert(PathWithoutExtension(L"file.txt") == L"file");
        assert(PathJoin(L"path/to", L"file.txt") == L"path/to/file.txt");
        assert(PathNormalizeCopy(L"path//to/../file.txt") == L"path/file.txt");
        assert(PathNormalizeCopy(L"C:\\folder\\..\\file.txt") == L"C:/file.txt");
        assert(PathNormalizeCopy(L"C:/../..") == L"C:");
        assert(PathNormalizeCopy(L"") == L".");
        std::wstring ws = L"path//to/../file.txt";
        PathNormalize(ws);
        assert(ws == L"path/file.txt");
    }

    void TestFormatTime()
    {
        auto now = std::chrono::system_clock::now();
        auto result = FormatTime(now, "%Y");
        assert(!result.empty() && result.size() == 4); // Year like "2025"
    }

    void TestPad()
    {
        // Narrow
        assert(PadLeftCopy("abc", 5) == "  abc");
        assert(PadRightCopy("abc", 5) == "abc  ");
        std::string s = "abc";
        PadLeft(s, 5, '*');
        assert(s == "**abc");
        s = "abc";
        PadRight(s, 5, '*');
        assert(s == "abc**");
    }

    void TestRepeat()
    {
        assert(Repeat("abc", 3) == "abcabcabc");
        assert(Repeat("", 5) == "");
        assert(Repeat("x", 0) == "");
    }

    void TestBase64()
    {
        assert(Base64Encode("Hello") == "SGVsbG8=");
        assert(Base64Decode("SGVsbG8=") == "Hello");
        assert(Base64Encode("") == "");
        assert(Base64Decode("") == "");
    }

    void TestURL()
    {
        assert(URLEncode("Hello World!") == "Hello%20World%21");
        assert(URLDecode("Hello%20World%21") == "Hello World!");
        assert(URLEncode("") == "");
        assert(URLDecode("") == "");
    }

    void TestHash()
    {
        assert(Hash("") == Hash("")); // Same input, same hash
        assert(Hash("abc") != Hash("def")); // Different input, different hash
        assert(CRC32("abc") != CRC32("def"));
        assert(Murmur3("abc") != Murmur3("def"));
        assert(SHA256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        assert(SHA256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    }

    void TestUnicode()
    {
        // ASCII-compatible UTF-8
        std::string utf8 = "hello";
        std::wstring utf16 = UTF8ToUTF16(utf8);
        assert(UTF16ToUTF8(utf16) == utf8);
        assert(utf16 == L"hello");

        // Non-ASCII UTF-8 using hex escapes for Japanese greeting (5 chars)
        utf8 = "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf";
        utf16 = UTF8ToUTF16(utf8);
        assert(UTF16ToUTF8(utf16) == utf8);
        assert(utf16.size() == 5); // 5 characters

        // Empty strings
        assert(UTF8ToUTF16("") == L"");
        assert(UTF16ToUTF8(L"") == "");
    }

    void TestLocale()
    {
        std::wstring ws = L"HELLO";
        ToLowerLocale(ws);
        assert(ws == L"hello");
        ws = L"hello";
        ToUpperLocale(ws);
        assert(ws == L"HELLO");
        assert(ToLowerLocale(L"HELLO") == L"hello");
        assert(ToUpperLocale(L"hello") == L"HELLO");
    }

    void TestLevenshteinDistance()
    {
        assert(LevenshteinDistance("kitten", "sitting") == 3);
        assert(LevenshteinDistance("abc", "") == 3);
        assert(LevenshteinDistance("", "abc") == 3);
        assert(LevenshteinDistance("", "") == 0);
        assert(LevenshteinDistance("abc", "abc") == 0);
    }

    void TestStringToDouble()
    {
        char* end;
        assert(StringToDouble("123.45", &end) == 123.45);
        assert(*end == '\0');
        assert(StringToDouble("-0.123") == -0.123);
        assert(StringToDouble("invalid") == 0.0);
        assert(StringToDouble("") == 0.0);
    }

    void TestRFindI()
    {
        assert(xstrtool::rfindI("Hello World Hello", "hello") == 12);
        assert(xstrtool::rfindI("Hello World Hello", "WORLD") == 6);
        assert(xstrtool::rfindI("abc", "d") == std::string::npos);
        assert(xstrtool::rfindI("", "a") == std::string::npos);
        assert(xstrtool::rfindI("abc", "") == 3);
        assert(xstrtool::rfindI("Hello World", "o", 7) == 7);
        assert(xstrtool::rfindI("Hello World", "o", 3) == std::string::npos);
        assert(xstrtool::rfindI(L"Hello World Hello", L"hello") == 12);
        assert(xstrtool::rfindI(L"Hello World Hello", L"WORLD") == 6);
        assert(xstrtool::rfindI(L"abc", L"d") == std::wstring::npos);
        assert(xstrtool::rfindI(L"", L"a") == std::wstring::npos);
        assert(xstrtool::rfindI(L"abc", L"") == 3);
        assert(xstrtool::rfindI(L"Hello World", L"o", 7) == 7);
    }

    void TestCopyN()
    {
        // Narrow
        assert(CopyN("Hello World", 5) == "Hello");
        assert(CopyN("abc", 5) == "abc");
        assert(CopyN("", 3) == "");
        assert(CopyN(std::string_view("Hello World"), 7) == "Hello W");
        assert(CopyN("abc", 0) == "");

        // Wide
        assert(CopyN(L"Hello World", 5) == L"Hello");
        assert(CopyN(L"abc", 5) == L"abc");
        assert(CopyN(L"", 3) == L"");
        assert(CopyN(std::wstring_view(L"Hello World"), 7) == L"Hello W");
        assert(CopyN(L"abc", 0) == L"");
    }

    void RunAllTests()
    {
        TestTo();
        TestCopyN();
        TestToLower();
        TestToUpper();
        TestCompareI();
        TestFindI();
        TestRFindI();
        TestStartsWithI();
        TestEndsWithI();
        TestContainsI();
        TestTrim();
        TestSplit();
        TestJoin();
        TestReplace();
        TestPath();
        TestFormatTime();
        TestPad();
        TestRepeat();
        TestBase64();
        TestURL();
        TestHash();
        TestUnicode();
        TestLocale();
        TestLevenshteinDistance();
        TestStringToDouble();
    }
}  // namespace xstrtool::test

#endif
