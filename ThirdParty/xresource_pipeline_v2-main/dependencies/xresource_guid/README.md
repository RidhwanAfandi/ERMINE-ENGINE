# xresource_guid_: Robust Resource Identification in C++

Unlock the power of unique resource identification with **xresource_guid_**, 
a modern C++ library designed to generate and manage Globally Unique Identifiers (GUIDs) for resources. 
Built with performance and flexibility in mind, xresource provides a type-safe, efficient way to create 64-bit 
and 128-bit GUIDs for resource types and instances, ensuring uniqueness across time, threads, and machines.

## Key Features

* **Dual GUID Sizes**: Supports 64-bit GUIDs for lightweight projects and 128-bit GUIDs for commercial-grade uniqueness.
* **Type & Instance Separation**: Distinct `type_guid` for resource types and `instance_guid` for specific resources, with a `full_guid` combining both.
* **Time-Based Uniqueness**: Uses timestamp (since 2025-01-01 UTC), thread ID, machine salt, and random components to ensure collision-free GUIDs.
* **String-Based GUIDs**: Generate deterministic GUIDs from strings using MurmurHash3 for consistent hashing.
* **Modern C++ Design**: Leverages C++20 features, `std::chrono`, and `std::random` for robust, portable code.
* **Thread-Safe**: Thread-local counters and salts ensure safe concurrent GUID generation.
* **Compile-Time Support**: `consteval` functions for generating GUIDs from strings at compile time.
* **MIT License**: Open and free to use with no restrictions.
* **No Dependencies**: Pure C++ implementation using standard library components.
* **Comprehensive Documentation**: Clear guides and examples to get you started.
* **Unit Tests**: Extensive tests to ensure reliability and correctness.

## Dependencies

* None! xresource relies solely on the C++ Standard Library (C++20).

## Code Example

```cpp
#include "source/xresource_guid.h"
#include <iostream>

int main() {
    // Generate a 64-bit instance GUID
    xresource::instance_guid guid = xresource::instance_guid::GenerateGUIDCopy();
    std::cout << "64-bit Instance GUID: " << guid.m_Value << std::endl;

    // Generate a 128-bit instance GUID
    xresource::instance_guid_large guid_large = xresource::instance_guid_large::GenerateGUIDCopy();
    std::cout << "128-bit Instance GUID: (" << guid_large.m_Low << ", " << guid_large.m_High << ")" << std::endl;

    // Generate a type GUID from a string at runtime
    xresource::type_guid type_guid = xresource::type_guid::GenerateGUIDCopy("resource_type");
    std::cout << "Type GUID from string: " << type_guid.m_Value << std::endl;

    // Generate a full GUID combining type and instance
    xresource::full_guid full = {guid, type_guid};
    std::cout << "Full GUID: (Instance: " << full.m_Instance.m_Value << ", Type: " << full.m_Type.m_Value << ")" << std::endl;

    // Compile-time string-based GUID
    constexpr xresource::type_guid compile_time_guid = xresource::guid_generator::Type64FromString("resource_type");
    std::cout << "Compile-time Type GUID: " << compile_time_guid.m_Value << std::endl;

    return 0;
}
```

Dive into unique resource identification with xresource – star, fork, and contribute now! 🚀