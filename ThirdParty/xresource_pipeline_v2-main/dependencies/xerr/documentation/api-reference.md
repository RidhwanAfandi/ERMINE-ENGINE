# xerr API Reference

This document details **xerr**’s API, split between `xerr.h` (declarations) and `implementation/xerr_inline.h` (implementations). For usage, see [Getting Started](getting-started.md) or [Advanced Usage](advanced-usage.md).

## Header: `xerr.h`
Include `source/xerr.h` for all functionality.

### Class: `xerr`
Minimal error object, 4-8 bytes (`const char*`).

#### Data Members
- `const char* m_pMessage`: Error string (`"error|hint"`) or `nullptr`.

#### Type Aliases
- `fn_error_callback`: `void(const char* pEnumValue, std::uint8_t State, const char* Message)`.

#### Methods
- `constexpr operator bool() const noexcept`: Returns `true` if error exists.
- `inline void clear() noexcept`: Clears error, resets chain.
- `constexpr std::uint32_t getStateUID() const noexcept`: Returns type UID.
- `template<typename T_STATE_ENUM> consteval static std::uint32_t fromStateUID() noexcept`: Returns UID for `T_STATE_ENUM`.
- `template<typename T_STATE_ENUM> constexpr bool isState() const noexcept`: Checks enum type match.
- `constexpr bool hasChain() const noexcept`: Returns `true` if chained.
- `inline std::string_view getMessage() const noexcept`: Returns error message (before `|`).
- `inline std::string_view getHint() const noexcept`: Returns hint (after `|`).
- `template<typename T_STATE_ENUM> constexpr T_STATE_ENUM getState() const noexcept`: Returns enum state.
- `template<typename T_CALLBACK> static void ForEachInChain(T_CALLBACK&& Callback) noexcept`: Iterates chain oldest to newest.
- `template<typename T_CALLBACK> static void ForEachInChainBackwards(T_CALLBACK&& Callback) noexcept`: Iterates newest to oldest.

#### Static Members
- `inline static fn_error_callback* m_pCallback`: Debugging callback.
- `inline static xerr_details::chain_pool m_ChainPool`: Lockless chain pool.

#### Template Class: `xerr::cleanup`
RAII cleanup.
- **Constructor**: `cleanup(xerr& Error, T_CALLBACK&& Lambda) noexcept`.
- **Destructor**: Calls `Lambda` if `Error` is set.
- **Members**: `xerr& m_Error`, `T_CALLBACK m_Lambda`.

#### Template Functions
- `template<auto T_STATE_V, xerr_details::string_literal T_STR_V> constexpr static xerr create() noexcept`: Creates standalone error.
- `template<auto T_STATE_V, xerr_details::string_literal T_STR_V> constexpr static xerr create(const xerr& PrevError) noexcept`: Chains error.
- `template<xerr_details::string_literal T_STR_V> constexpr static xerr create_f() noexcept`: Creates `FAILURE` error.
- `template<xerr_details::string_literal T_STR_V> constexpr static xerr create_f(const xerr& PrevError) noexcept`: Chains `FAILURE` error.

### Namespace: `xerr_details`
- `string_literal<N>`: Compile-time string literal.
- `chain_pool`: Lockless node pool.
  - `node`: `{char* m_pError, std::int16_t m_iNext, std::int16_t m_iPrev}`.
  - `Alloc()`: Pops node index, asserts on exhaustion.
  - `Free(std::int16_t& iHead, std::int16_t& iTail)`: Frees chain.
  - `std::array<node, 1024> m_Pool`: Global pool.
  - `std::atomic<std::int16_t> m_iEmpty`: Free list head.

## Header: `xerr_inline.h`
Contains implementations:
- `create_uid<T_STATE_ENUM>`: Generates type UID.
- `getValueTypeName<T>`: Extracts enum type name.
- `info_construct<T_SIZE_V>`: Stores UID, state, message.
- `data_v<T_STR_V, T_STATE_V>`: Compile-time error data.
- `chain_pool` methods: Constructor, `Alloc`, `Free`.
- `CreateEntry()`: Helper for chaining.
- `xerr` methods: All inline or constexpr.

## Notes
- **Traversal**: `ForEachInChain` uses `m_iNext` (oldest to newest); `ForEachInChainBackwards` uses `m_iPrev` (newest to oldest). Callbacks should take `const xerr&` for safety, though any callable is allowed.
- **Pool Exhaustion**: `Alloc` asserts and returns index 0 on exhaustion (debug-only). In release builds, this may overwrite node 0; monitor chain sizes.

## Next Steps
- Try xerr in [Getting Started](getting-started.md).
- Explore features in [Advanced Usage](advanced-usage.md).
- See performance in [Performance](performance.md).