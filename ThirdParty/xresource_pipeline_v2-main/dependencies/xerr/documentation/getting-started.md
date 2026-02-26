# Getting Started with xerr

This guide covers basic usage of **xerr**, a zero-overhead, type-safe C++ error handling library. For advanced features, see [Advanced Usage](advanced-usage.md).

## Setup
1. Clone [xerr on GitHub](https://github.com/LIONant-depot/xerr).
2. Include `source/xerr.h` (includes `implementation/xerr_inline.h`):
   ```cpp
   #include "xerr.h"
   ```

## Creating Errors
Define a custom enum with `OK` and `FAILURE`:
```cpp
enum class FileError : std::uint8_t {
    OK = 0,      // Required
    FAILURE = 1, // Required
    NOT_FOUND
};
```

Create errors with `xerr::create`:
```cpp
xerr open_file(const char* path) {
    if (!path) return xerr::create<FileError::NOT_FOUND, "File not found|Check path">();
    return {};
}
```

## Checking Errors
Use `operator bool` and access message/hint:
```cpp
if (auto err = open_file(""); err) {
    printf("Error: %s, Hint: %s\n", err.getMessage().data(), err.getHint().data());
    // Outputs: Error: File not found, Hint: Check path
}
```

## Type-Safe State Checking
Verify state with `getState`:
```cpp
auto err = open_file("");
assert(err.getState<FileError>() == FileError::NOT_FOUND);
```

## RAII Cleanup
Use `xerr::cleanup` for resource management:
```cpp
xerr process_file(const char* path) {
    FILE* fp = fopen(path, "r");
    xerr err;
    xerr::cleanup s(err, [&fp] { if (fp) fclose(fp); });
    if ((err = open_file(path))) return err;
    return {};
}
```

## Next Steps
- Explore chaining and debugging in [Advanced Usage](advanced-usage.md).
- See full API in [API Reference](api-reference.md).
- Learn why xerr is fastest in [Performance](performance.md).