# xerr: Blazing-Fast C++ Error Handling

**xerr** is a revolutionary C++ error handling library for **performance-critical applications**. With a **4-8 byte footprint**, **zero-overhead** happy paths, and **type-safe** error management, xerr crushes exceptions and outshines `std::expected`. Lockless chaining, RAII cleanup, and rich debugging make it the ultimate choice, all in a header-only package with no dependencies.

## Why xerr?
- **Tiny**: Errors are a single `const char*` (4-8 bytes), 4 bytes per-thread.
- **Fast**: Constexpr creation, ~1-5 cycles for errors.
- **Type-Safe**: Enum-based states with compile-time checks.
- **Chaining**: Lockless, allocation-free error cause tracking.
- **RAII Cleanup**: Auto-resource management on errors.
- **Debugging**: Callbacks for call site, state, and message logging.
- **Cross-Platform**: MSVC, Clang, GCC support.
- **No Dependencies**: Just include `xerr.h`.
- **MIT License**: Free to use and extend.

## Quick Start
Clone [xerr on GitHub](https://github.com/LIONant-depot/xerr), include `source/xerr.h`, and start:

```cpp
#include "xerr.h"

enum class Error : std::uint8_t { OK, FAILURE, NOT_FOUND, INVALID };

xerr low_level() {
    return xerr::create<Error::NOT_FOUND, "File not found|Check path">();
}

xerr high_level() {
    xerr err;
    xerr::cleanup s(err, [] { printf("Cleaning up\n"); });
    if ((err = low_level())) return xerr::create<Error::INVALID, "Processing failed|Retry operation">(err);
    return {};
}

int main() {
    if (auto err = high_level(); err) {
        xerr::ForEachInChain([](const xerr& e) {
            printf("Error: %s, Hint: %s\n", e.getMessage().data(), e.getHint().data());
        });
        // Outputs:
        // Error: File not found, Hint: Check path
        // Error: Processing failed, Hint: Retry operation
    }
}
```

## Documentation
Dive deeper:
- [Getting Started](documentation/getting-started.md): Basic usage and setup.
- [Advanced Usage](documentation/advanced-usage.md): Chaining, cleanup, debugging.
- [API Reference](documentation/api-reference.md): Full class and method details.
- [Performance](documentation/performance.md): Why xerr is the fastest.
- [Contributing](documentation/contributing.md): Join the xerr community.

## Why xerr Wins
- **Beats Error Codes**: Same speed, adds type safety and `"error|hint"`.
- **Crushes Exceptions**: 10-100x faster, no unwinding.
- **Outpaces std::expected**: Smaller, no checks, supports chaining.
- **Simpler than Boost.Outcome**: Leaner, just as powerful.

Star [xerr on GitHub](https://github.com/LIONant-depot/xerr) and revolutionize C++ error handling!