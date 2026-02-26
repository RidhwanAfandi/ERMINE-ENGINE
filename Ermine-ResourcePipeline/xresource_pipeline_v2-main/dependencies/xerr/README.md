# xerr: Lightweight C++ Error Handling

**xerr** is the ultimate C++ error handling library—**4-8 bytes**, **zero-overhead** on happy paths, 
and type-safe. Built for performance-critical code, it crushes exceptions and outshines `std::expected` with a 
minimalist design, lockless chaining, and RAII cleanup. No dependencies, just pure speed and dead simple to use.

## Key Features

- **Tiny**: Errors are a single `const char*` (4-8 bytes), 4 bytes per-thread.
- **Fast**: Constexpr creation, ~1-5 cycles for errors.
- **Zero allocations**: No calls to the memory manager.
- **Type-Safe**: Enum-based states with compile-time checks.
- **Chaining**: Lockless, allocation-free error cause tracking.
- **RAII Cleanup**: Auto-resource management on error paths.
- **Debugging**: Custom callbacks for call site, state, and message logging.
- **Cross-Platform**: MSVC, Clang, GCC with compile-time type parsing.
- **No Dependencies**: Header-only, drop in `xerr.h`.
- **Hints**: You can create hints in your errors to help the user solve their issues.
- **MIT License**: Free to hack.
- **Header only**: Simple to integrate in your projects
- **Documentation**: [documentation!](https://github.com/LIONant-depot/xerr/blob/main/documentation/index.md)

## Quick Start
Grab [xerr on GitHub](https://github.com/LIONant-depot/xerr), include `source/xerr.h`, and go!

## Code Example

```cpp
#include "source/xerr.h"
#include <cassert>

// Simple file handler class
class FileHandler {
public:
    // Custom error enum
    enum class Error : std::uint8_t 
    { OK            // These two are required by the system
    , FAILURE       // These two are required by the system
    , NOT_FOUND     // Custom...
    };

    constexpr xerr open(const std::string& filename) noexcept {
        if (filename.empty()) return xerr::create<Error::NOT_FOUND, "Empty filename">();
        m_isOpen = true;
        return {};
    }
private:
    bool m_isOpen = false;
};

int main() {
    FileHandler file;
    if( auto Err = file.open(""); Err )
    {
        std::cout << Err.getMessage() << "\n";_
        assert(Err.getState<FileHandler::Error>() == FileHandler::Error::NOT_FOUND);
    }
    return 0;
}
```

## Why xerr Wins
- **Beats Error Codes**: Same speed, adds type safety and `"error|hint"`.
- **Crushes Exceptions**: 10-100x faster, no unwinding.
- **Outpaces std::expected**: Smaller, no checks, supports chaining.
- **Simpler than Boost.Outcome**: Leaner, just as powerful.

## Join the Revolution
Star and fork [xerr on GitHub](https://github.com/LIONant-depot/xerr). Make C++ error handling epic!