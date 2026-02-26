# Advanced Usage of xerr

This guide covers **xerr**’s advanced features: multi-level error chaining, RAII cleanup, debugging, and custom enums. For basics, see [Getting Started](getting-started.md).

## Multi-Level Error Chaining
Chain errors across multiple levels to track causes:
```cpp
enum class Error : std::uint8_t { OK, FAILURE, NOT_FOUND, IO_ERROR, INVALID };

xerr low_level() {
    return xerr::create<Error::NOT_FOUND, "File not found|Check path">();
}

xerr mid_level() {
    if (auto err = low_level(); err) {
        return xerr::create<Error::IO_ERROR, "IO operation failed|Check device">(err);
    }
    return {};
}

xerr high_level() {
    if (auto err = mid_level(); err) {
        return xerr::create<Error::INVALID, "Processing failed|Retry operation">(err);
    }
    return {};
}

int main() {
    if (auto err = high_level(); err) {
        xerr::ForEachInChain([](const xerr& e) {
            printf("Error: %s, Hint: %s\n", e.getMessage().data(), e.getHint().data());
        });
        // Outputs:
        // Error: File not found, Hint: Check path
        // Error: IO operation failed, Hint: Check device
        // Error: Processing failed, Hint: Retry operation
    }
}
```

**Note**: `ForEachInChain` iterates oldest to newest (root cause to latest). `ForEachInChainBackwards` iterates newest to oldest.

## RAII Cleanup
Use `xerr::cleanup` for automatic resource cleanup:
```cpp
xerr process_file(const char* path) {
    FILE* fp = fopen(path, "r");
    xerr err;
    xerr::cleanup s(err, [&fp] { if (fp) fclose(fp); });
    if ((err = high_level())) return err;
    return {};
}
```

## Debugging
Set a callback to log errors:
```cpp
void log_error(const char* func, std::uint8_t state, const char* msg, std::uint32_t line, std::string_view file ) {
    std::cout << "Error in  = " << func
              << "\n state  = " << state
              << "\n msg    = " << xerr{msg}.getMessage()
              << "\n hint   = " << xerr{msg}.getHint()
              << "\n source = " << file
              << "\n line   = " << line
              << "\n";
}

int main() {
    xerr::m_pCallback = log_error;
    if (auto err = high_level(); err) {
        // Logs for each error in chain, e.g.:
        // Error in low_level: state=2, msg=File not found|Check path, hint=Check path
        // ...
    }
}
```

**Note**: Callbacks should take `const xerr&` for safety, though any callable is allowed.

## Custom Enums
Extend `default_states`:
```cpp
enum class MyError : std::uint8_t {
    OK = 0,      // Required
    FAILURE = 1, // Required
    TIMEOUT
};

xerr operation() {
    return xerr::create<MyError::TIMEOUT, "Operation timed out|Try again later">();
}
```

## Next Steps
- See API details in [API Reference](api-reference.md).
- Explore performance in [Performance](performance.md).
- Contribute at [Contributing](contributing.md).