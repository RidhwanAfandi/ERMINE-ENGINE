# xstrtool: Turbocharge Your C++ String Manipulation!

Elevate your string and path handling with **xstrtool** – a high-performance, lightweight C++ 20 library 
designed for blazing-fast string operations. Say goodbye to cumbersome string processing and hello to a 
streamlined, modern interface that supports both narrow and wide strings, optimized with SSE for speed. 
With comprehensive string utilities, path normalization, Unicode support, and robust hashing, xstrtool 
is your go-to tool for efficient text processing in any C++ project!

## Key Features

- **Blazing Fast Operations**: SSE-optimized functions  for lightning-fast string processing.
- **Narrow and Wide String Support**: Seamless handling of C++ 20 classes and C
- **Unicode Support**: Convert between UTF-8 and UTF-16 with, plus locale-aware case conversion
- **Path Manipulation**: Normalize paths, handle Windows drive letters, and extract components
- **Robust Hashing**: Multiple algorithms for versatile string hashing.
- **Memory Efficient**: Minimize copies and optimize memory usage.
- **Flexible String Operations**: Comprehensive utilities for case conversion, comparison, searching, trimming... etc
- **C++20 Modern Design**: Uses all modern C++20 features for a safe, and efficient API.
- **MIT License**: Free and open for all your projects.
- **Minimal Dependencies**: Requires only standard C++20 libraries
- **Easy Integration**: Include `xstrtool.h` (and `xstrtool.cpp` for non-inline functions) in your project.
- **Unit Tests & Documentation**: Fully tested with and documented in `xstrtool.h`.

## Dependencies

None

## Getting Started

1. **Add to Your Project**:
   - Include `source/xstrtool.h` and `source/xstrtool.cpp` into your project.

## Code Example

```cpp
#include "source/xstrtool.h"
#include <cassert>

int main() {
    // String case conversion and comparison
    std::string s = "HeLLo";
    xstrtool::ToLower(s);
    assert(s == "hello");
    assert(xstrtool::CompareI("hello", "HELLO") == 0);

    // Path normalization (Windows drive letters)
    std::wstring path = L"C:\\folder\\..\\file.txt";
    auto normalized = xstrtool::PathNormalizeCopy(path);
    assert(normalized == L"C:/file.txt");

    // Unicode conversion
    std::string utf8 = "\xe3\x81\x93\xe3\x82\x93\xe3\x81\xab\xe3\x81\xa1\xe3\x81\xaf"; // Japanese greeting
    std::wstring utf16 = xstrtool::UTF8ToUTF16(utf8);
    assert(xstrtool::UTF16ToUTF8(utf16) == utf8);
    assert(utf16.size() == 5); // 5 characters

    // Hashing
    assert(xstrtool::SHA256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // String splitting and joining
    std::vector<std::string_view> parts = xstrtool::Split("a,b,c", ',');
    assert(xstrtool::Join(parts, ",") == "a,b,c");

    // Number parsing
    char* end;
    assert(xstrtool::StringToDouble("123.45", &end) == 123.45);
    assert(*end == '\0');

    // Levenshtein distance
    assert(xstrtool::LevenshteinDistance("kitten", "sitting") == 3);

    return 0;
}
```

## Comparison with Other Libraries

| Library         | Functionality | Performance | Ease of Use | Dependencies | Score |
|-----------------|---------------|-------------|-------------|--------------|-------|
| **xstrtool**    | 8/10          | 8/10        | 9/10        | 9/10         | **8.5** |
| Boost           | 9/10          | 7/10        | 6/10        | 5/10         | **6.75** |
| Abseil          | 8/10          | 9/10        | 8/10        | 7/10         | **8.0** |
| folly           | 8/10          | 9/10        | 7/10        | 5/10         | **7.25** |
| StrTk           | 7/10          | 9/10        | 7/10        | 9/10         | **8.0** |
| Poco            | 9/10          | 7/10        | 6/10        | 5/10         | **6.75** |
| C++20 std       | 7/10          | 8/10        | 8/10        | 10/10        | **8.25** |

- **xstrtool**: Lightweight, C++20-modern, with SSE optimizations, wide/narrow string support, Unicode, path handling, and hashing. Lacks regex (use `std::regex`) and advanced parsing.
- **Boost.StringAlgo/Filesystem**: Feature-rich but heavy dependencies, older C++ style, limited wide string support.
- **Abseil**: High-performance, UTF-8-focused, but no wide strings or paths.
- **folly**: SIMD-optimized, but complex and dependency-heavy.
- **StrTk**: Excellent for parsing, but no wide strings or paths.
- **Poco**: Comprehensive, but large framework with complex setup.
- **C++20 std**: Zero dependencies, but lacks case-insensitive ops and wide string conversions.

## Why Choose xstrtool?

- **Lightweight and Modern**: Minimal dependencies, C++20 features, and ASCII-compliant source.
- **High Performance**: SSE optimizations for key operations, efficient `std::string_view` usage.
- **Versatile**: Supports narrow/wide strings, Unicode, paths (with Windows drive letters), and multiple hashing algorithms.
- **Easy to Use**: Clear API, comprehensive documentation, and robust unit tests.

Dive into **xstrtool** today – star, fork, and contribute to supercharge your string manipulation! 🚀