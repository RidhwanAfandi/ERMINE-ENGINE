# xerr Performance

**xerr** is built for **near-zero overhead** and unmatched speed. This document explains why xerr is the fastest C++ error system. For usage, see [Getting Started](getting-started.md) or [Advanced Usage](advanced-usage.md).

## Performance Highlights
- **Size**: `xerr` is `const char*` (4-8 bytes). Per-thread: 4 bytes (`g_iCurChain`, `g_iCurTail`).
- **Happy Path**: Zero overhead—constexpr `create<>`, ~1 cycle return.
- **Error Path**: ~1-5 cycles for creation, ~1-5 cycles for chaining (atomic pop, O(1) linking), ~10-20 cycles for `getMessage`/`getHint`.
- **No Allocations**: Static `chain_pool` (~10,240 bytes globally).
- **Thread Safety**: Lockless atomics, no mutexes.

## Comparisons
- **Error Codes**: Fast (4 bytes), but no context, safety, or chaining. xerr matches speed with added features.
- **Exceptions**: 10-100x slower (~100-1000 cycles for unwinding). xerr is near-zero cost.
- **std::optional/std::expected**: Larger (8-16 bytes), needs checks (~1-2 cycles). xerr is smaller, no checks.
- **Boost.Outcome/LEAF**: Larger, complex (~2-10 cycles overhead). xerr is leaner, simpler.

## Why xerr Wins
xerr’s 4-8 byte errors, constexpr creation, and lockless chaining make it the smallest, fastest error system. No other library matches its speed, safety, and features.

## Next Steps
- Try xerr in [Getting Started](getting-started.md).
- Explore chaining in [Advanced Usage](advanced-usage.md).
- See API details in [API Reference](api-reference.md).