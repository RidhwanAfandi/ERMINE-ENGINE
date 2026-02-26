#ifndef XRESOURCE_GUID_H
#define XRESOURCE_GUID_H
#pragma once

#include <cstdint>  // uint64_t
#include <cassert>  // assert
#include <thread>   // std::thread::id
#include <random>   // std::random_device, std::mt19937_64, std::uniform_int_distribution
#include <chrono>   // std::chrono::system_clock, std::chrono::duration_cast, std::chrono::seconds
#include <map>      // std::hash

// This header is meant to solve the problem for unique identifiers for resources
// The main concept here is that we define one GUID (Global Unique Identifier) as two elements...
// The type which specifies what type of resource it is with a unique identifier
// and the Instance which identifies which actual resource we are talking about
// The type should be fairly confortable with 64bits not matter what...
// But the instance depending of the problem you are trying to solve may require to be larger
// For that porpouse we hace provided a _large version of the instance which is 128bits which
// is the standard for commercial products (overkilled for smaller projects)
// The def_guid is typically what most project will use in their components since the type will be fixed

namespace xresource
{
    struct guid_generator
    {
        // This is a struct instead of a namespace because I need the functions not to inline yet still be in the header files
        guid_generator() = delete;

        // Generates a 64-bit Globally Unique Identifier (GUID) for resources.
        // Ensures uniqueness across time, machines, processes, and threads by combining:
        // - Bit      0: Fixed bit set to 1 (1 bit, reserved to mark this GUID type)
        // - Bits  1-13: Counter (13 bits, thread-local, increments per GUID in a second)
        // - Bits 14-42: Timestamp (29 bits, seconds since 2025-01-01 UTC, spans ~17 years)
        // - Bits 43-50: Thread ID hash (8 bits, distinguishes threads within a process)
        // - Bits 51-58: Machine salt (8 bits, unique per process/machine)
        // - Bits 59-63: Random value (5 bits, adds entropy to reduce collisions)
        // Total: 1 + 13 + 29 + 8 + 8 + 5 = 64 bits
        // Each component is masked to its bit size to prevent overlap and ensure correctness.
        static __declspec(noinline) [[nodiscard]] uint64_t Instance64() noexcept
        {
            // Thread-local random number generator for the random component
            thread_local std::mt19937_64                            rng(std::random_device{}());
            thread_local std::uniform_int_distribution<uint64_t>    dist;

            // Thread-local counter, only 13 bits used to track GUIDs per thread
            thread_local std::uint16_t counter = 0;

            // Thread ID hash: 8-bit value computed once per thread from counter's address
            thread_local uint8_t threadSalt = []
                {
                    uintptr_t addr = reinterpret_cast<uintptr_t>(&counter);
                    uint8_t hash = 0;
                    for (size_t i = 0; i < sizeof(addr); ++i)
                    {
                        hash ^= static_cast<uint8_t>((addr >> (i * 8)) & 0xFF);
                    }
                    return hash;
                }();

            // Machine salt: 8-bit random value, unique per process instance
            static const uint8_t machineSalt = static_cast<uint8_t>(std::random_device{}() & 0xFF);

            // Epoch set to January 1, 2025 UTC (Unix timestamp: 1735689600)
            static const auto   epoch       = std::chrono::system_clock::from_time_t(1735689600);

            // Calculate seconds since epoch for the timestamp
            const auto          timestamp   = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - epoch).count();

            // Timestamp: 29 bits, masked to prevent overflow into other components
            const auto          timeComponent = static_cast<std::uint64_t>(timestamp & 0x1FFFFFFF);

            // Random component: 5 bits, adds entropy within the same second
            const auto          randomComponent = static_cast<uint8_t>(dist(rng) & 0x1F);

            // Counter: 13 bits, masked to stay within allocation, increments per GUID
            const auto          counterComponent = static_cast<std::uint64_t>(counter++ & 0x1FFF);

            // Assemble the 64-bit GUID using bitwise operations
            std::uint64_t guid =
                1                                                       // Bit      0: fixed to 1
                | (counterComponent << 1)                               // Bits  1-13: counter
                | (timeComponent << 14)                                 // Bits 14-42: timestamp
                | (static_cast<std::uint64_t>(threadSalt) << 43)        // Bits 43-50: thread ID
                | (static_cast<std::uint64_t>(machineSalt) << 51)       // Bits 51-58: machine salt
                | (static_cast<std::uint64_t>(randomComponent) << 59)   // Bits 59-63: random
                ;
            return guid;
        }

        //------------------------------------------------------------------------------------------------
        // Works Similar to the instance version except bit 0 is not longer a constant 1
        // Generates a 64-bit Globally Unique Identifier (GUID) for resources.
        // Ensures uniqueness across time, machines, processes, and threads by combining:
        // - Bits  0-12: Counter (13 bits, thread-local, increments per GUID in a second)
        // - Bits 13-42: Timestamp (30 bits, seconds since 2025-01-01 UTC, spans ~34 years)
        // - Bits 43-50: Thread ID hash (8 bits, distinguishes threads within a process)
        // - Bits 51-58: Machine salt (8 bits, unique per process/machine)
        // - Bits 59-63: Random value (5 bits, adds entropy to reduce collisions)
        // Total: 13 + 30 + 8 + 8 + 5 = 64 bits
        // Each component is masked to its bit size to prevent overlap and ensure correctness.
        static __declspec(noinline) [[nodiscard]] uint64_t Type64() noexcept
        {
            // Thread-local random number generator for the random component
            thread_local std::mt19937_64                            rng(std::random_device{}());
            thread_local std::uniform_int_distribution<uint64_t>    dist;

            // Thread-local counter, only 13 bits used to track GUIDs per thread
            thread_local std::uint16_t      counter = 0;

            // Thread ID hash: 8-bit value computed once per thread from counter's address
            thread_local uint8_t            threadSalt = []
            {
                uintptr_t addr = reinterpret_cast<uintptr_t>(&counter);
                uint8_t hash = 0;
                for (size_t i = 0; i < sizeof(addr); ++i)
                {
                    hash ^= static_cast<uint8_t>((addr >> (i * 8)) & 0xFF);
                }
                return hash;
            }();

            // Machine salt: 8-bit random value, unique per process instance
            static const uint8_t        machineSalt = static_cast<uint8_t>(std::random_device{}() & 0xFF);

            // Epoch set to January 1, 2025 UTC (Unix timestamp: 1735689600)
            static const auto           epoch       = std::chrono::system_clock::from_time_t(1735689600);

            // Calculate seconds since epoch for the timestamp
            const auto                  timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - epoch).count();

            // Timestamp: 30 bits, masked to prevent overflow into other components
            const auto                  timeComponent = static_cast<std::uint64_t>(timestamp & 0x3FFFFFFF);

            // Random component: 5 bits, adds entropy within the same second
            const auto                  randomComponent = static_cast<uint8_t>(dist(rng) & 0x1F);

            // Counter: 13 bits, masked to stay within allocation, increments per GUID
            const auto                  counterComponent = static_cast<std::uint64_t>(counter++ & 0x1FFF);

            // Assemble the 64-bit GUID using bitwise operations
            std::uint64_t guid =
                   counterComponent                                     // Bits  0-12: counter
                | (timeComponent << 13)                                 // Bits 13-42: timestamp
                | (static_cast<std::uint64_t>(threadSalt) << 43)        // Bits 43-50: thread ID
                | (static_cast<std::uint64_t>(machineSalt) << 51)       // Bits 51-58: machine salt
                | (static_cast<std::uint64_t>(randomComponent) << 59)   // Bits 59-63: random
                ;

            return guid;
        }

        //------------------------------------------------------------------------------------------------

        // Generates a 128-bit Globally Unique Identifier (GUID) for resources.
        // Ensures uniqueness across time, machines, processes, and threads by combining:
        // - Bit 0: Fixed bit set to 1 (1 bit, reserved to mark this GUID type)
        // - Bits 1-24: Counter (24 bits, thread-local, increments per GUID in a second)
        // - Bits 25-72: Timestamp (48 bits, seconds since 2025-01-01 UTC, spans ~8,925 years)
        // - Bits 73-80: Thread ID hash (8 bits, distinguishes threads within a process)
        // - Bits 81-96: Machine salt (16 bits, unique per process/machine)
        // - Bits 97-127: Random value (31 bits, adds entropy to reduce collisions)
        // Total: 1 + 24 + 48 + 8 + 16 + 31 = 128 bits
        // Each component is masked to its bit size to prevent overlap and ensure correctness.
        static __declspec(noinline) [[nodiscard]] std::pair<std::uint64_t, std::uint64_t> Instance128() noexcept
        {
            // Thread-local random number generator for the random component
            thread_local std::mt19937_64                                rng(std::random_device{}());
            thread_local std::uniform_int_distribution<std::uint64_t>   dist;

            // Thread-local counter, only 24 bits used to track GUIDs per thread
            thread_local std::uint32_t counter = 0;

            // Thread ID hash: 8-bit value computed once per thread from counter's address
            thread_local std::uint8_t threadSalt = []
                {
                    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(&counter);
                    std::uint8_t hash = 0;
                    for (size_t i = 0; i < sizeof(addr); ++i)
                    {
                        hash ^= static_cast<std::uint8_t>((addr >> (i * 8)) & 0xFF);
                    }
                    return hash;
                }();

            // Machine salt: 16-bit random value, unique per process instance
            static const std::uint16_t machineSalt = static_cast<std::uint16_t>(std::random_device{}() & 0xFFFF);

            // Epoch set to January 1, 2025 UTC (Unix timestamp: 1735689600)
            static const auto epoch = std::chrono::system_clock::from_time_t(1735689600);

            // Calculate seconds since epoch for the timestamp
            const auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - epoch).count();

            // Timestamp: 48 bits, masked to prevent overflow into other components
            const std::uint64_t timeComponent = static_cast<std::uint64_t>(timestamp & 0xFFFFFFFFFFFF);

            // Random component: 31 bits, adds entropy within the same second
            const std::uint32_t randomComponent = static_cast<std::uint32_t>(dist(rng) & 0x7FFFFFFF);

            // Counter: 24 bits, masked to stay within allocation, increments per GUID
            const std::uint32_t counterComponent = static_cast<std::uint32_t>(counter++ & 0xFFFFFF);

            // Assemble the 128-bit GUID using bitwise operations
            // Lower 64 bits: fixed bit + counter (24 bits) + timestamp (39 bits)
            const std::uint64_t lower =
                1ULL                                                    // Bit 0: fixed to 1
                | (static_cast<std::uint64_t>(counterComponent) << 1)   // Bits 1-24: counter
                | (timeComponent << 25);                                // Bits 25-63: timestamp (lower 39 bits)

            // Upper 64 bits: timestamp (9 bits) + thread ID (8 bits) + machine salt (16 bits) + random (31 bits)
            const std::uint64_t upper =
                (static_cast<std::uint64_t>(timeComponent >> 39) << 55) // Bits 0-8: timestamp (upper 9 bits)
                | (static_cast<std::uint64_t>(threadSalt) << 47)        // Bits 9-16: thread ID
                | (static_cast<std::uint64_t>(machineSalt) << 31)       // Bits 17-32: machine salt
                | (static_cast<std::uint64_t>(randomComponent) << 0);   // Bits 33-63: random

            return { lower, upper };
        }
        //------------------------------------------------------------------------------------------------

        // Compile time version of GenerateGUID for strings
        template<std::size_t N>
        static consteval
        [[nodiscard]] std::uint64_t Type64FromString(const char(&str)[N], uint64_t hash = 0x548c9decbce65297ULL) noexcept
        {
            constexpr uint64_t m = 0xc6a4a7935bd1e995ULL;

            // Skip null terminator
            for (std::size_t i = 0; i < N - 1; ++i) 
            { 
                hash = (hash ^ static_cast<std::uint64_t>(str[i])) * m;
            }

            hash ^= static_cast<std::uint64_t>(N - 1); // Length without null
            hash ^= hash >> 33;
            hash *= 0x85ebca6b;
            hash ^= hash >> 13;
            hash *= 0xc2b2ae35;
            hash ^= hash >> 16;

            return hash;
        }

        //------------------------------------------------------------------------------------------------

        template<std::size_t N>
        static consteval
        [[nodiscard]] std::uint64_t Instance64FromString(const char(&str)[N], const uint64_t hash = 0x548c9decbce65297ULL) noexcept
        {
            return (Type64FromString(str, hash) << 1) | 1;
        }

    };

    //------------------------------------------------------------------------------------------------

    template<typename T_ARG>
    struct guid
    {
        union
        {
            std::uint64_t m_Value=0;
            void*         m_Pointer;
        };

        guid() = default;

        template<std::size_t N>
        constexpr guid(const char(&str)[N]) noexcept 
        {
            uint64_t hash = 0x548c9decbce65297ULL;

            constexpr uint64_t m = 0xc6a4a7935bd1e995ULL;

            // Skip null terminator
            for (std::size_t i = 0; i < N - 1; ++i)
            {
                hash = (hash ^ static_cast<std::uint64_t>(str[i])) * m;
            }

            hash ^= static_cast<std::uint64_t>(N - 1); // Length without null
            hash ^= hash >> 33;
            hash *= 0x85ebca6b;
            hash ^= hash >> 13;
            hash *= 0xc2b2ae35;
            hash ^= hash >> 16;

            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>) m_Value = (hash<<1) | 1;
            else m_Value = hash;
        }

        constexpr guid(std::uint64_t Value) : m_Value { Value } {}

        constexpr
        [[nodiscard]]bool operator == (const guid& B) const noexcept
        {
            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
            {
                assert(isValid() || empty());
                assert(B.isValid() || B.empty());
            }
            return m_Value == B.m_Value;
        }

        constexpr
        [[nodiscard]]bool operator != (const guid& B) const noexcept
        {
            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
            {
                assert(isValid() || empty());
                assert(B.isValid() || B.empty());
            }
            return m_Value != B.m_Value;
        }

        constexpr
       [[nodiscard]] bool operator < (const guid& B) const noexcept
        {
            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
            {
                assert(isValid() || empty());
                assert(B.isValid() || B.empty());
            }
            return m_Value < B.m_Value;
        }

        constexpr
        [[nodiscard]]bool operator > (const guid& B) const noexcept
        {
            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
            {
                assert(isValid() || empty());
                assert(B.isValid() || B.empty());
            }
            return m_Value > B.m_Value;
        }

        constexpr 
        [[nodiscard]] bool empty() const noexcept
        {
            return m_Value == 0;
        }

        inline void clear() noexcept
        {
            m_Value = 0;
        }

        constexpr
        [[nodiscard]] bool isValid() const noexcept requires std::is_same_v<T_ARG, struct rsc_instance_guid_tag>
        {
            return m_Value;
        }

        constexpr
        bool isPointer () const noexcept
        {
            return !(m_Value & 1);
        }

        inline static [[nodiscard]] guid GenerateGUIDCopy() noexcept
        {
            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
                return { guid_generator::Instance64()};
            else
                return { guid_generator::Type64()};
        }

        inline guid& GenerateGUID() noexcept
        {
            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
            {
                m_Value = guid_generator::Instance64();
                return *this;
            }
            else
            {
                m_Value = guid_generator::Type64();
                return *this;
            }
        }

        // Generate GUID based on a string... we try to make it well distributed in the space of bits rather than unique in time
        static inline
        [[nodiscard]] guid GenerateGUIDCopy(const char* str, uint64_t hash = 0xcbf29ce484222325ULL) noexcept
        {
            // MurmurHash3 (64-bit)
            const uint64_t m = 0xc6a4a7935bd1e995ULL;
            int len = 0;

            const unsigned char* data = reinterpret_cast<const unsigned char*>(str);
            while (*data) 
            {
                hash ^= static_cast<uint64_t>(*data++);
                hash *= m;
                len++;
            }

            // Finalization
            hash ^= len;
            hash ^= hash >> 33;
            hash *= 0x85ebca6b;
            hash ^= hash >> 13;
            hash *= 0xc2b2ae35;
            hash ^= hash >> 16;

            if constexpr (std::is_same_v<T_ARG, struct rsc_instance_guid_tag>)
                return {(hash<<1) | 1};
            else
                return {hash};
        }

        inline
        guid& GenerateGUID(const char* str, uint64_t hash = 0xcbf29ce484222325ULL) noexcept
        {
            m_Value = GenerateGUIDCopy(str,hash);
            return *this;
        }
    };

    //------------------------------------------------------------------------------------------------

    struct instance_guid_large
    {
        union
        {
            struct
            {
                std::uint64_t m_Low;
                std::uint64_t m_High;
            };

            void* m_Pointer{};
        };

        //constexpr instance_guid_large() : m_Low{0}, m_High{0} {}

        constexpr
        bool operator == (const instance_guid_large& B) const noexcept
        {
            assert(isValid() || empty());
            assert(B.isValid() || B.empty());
            return m_Low == B.m_Low && m_High == B.m_High;
        }

        constexpr
        bool operator != (const instance_guid_large& B) const noexcept
        {
            assert(isValid() || empty());
            assert(B.isValid() || B.empty());
            return m_Low != B.m_Low || m_High != B.m_High;
        }

        constexpr
        bool operator < (const instance_guid_large& B) const noexcept
        {
            assert(isValid() || empty());
            assert(B.isValid() || B.empty());
            return m_Low < B.m_Low || (m_Low == B.m_Low && m_High < B.m_High);
        }

        constexpr
        bool operator > (const instance_guid_large& B) const noexcept
        {
            assert(isValid() || empty());
            assert(B.isValid() || B.empty());
            return m_Low > B.m_Low || (m_Low == B.m_Low && m_High > B.m_High);
        }

        constexpr 
        [[nodiscard]] bool empty() const noexcept
        {
            return m_Low == 0 && m_High==0;
        }

        inline void clear() noexcept
        {
            m_Low = m_High = 0;
        }

        constexpr
        [[nodiscard]] bool isValid() const noexcept
        {
            return (m_Low | m_High) && !!(m_Low & 1);
        }

        constexpr
        [[nodiscard]] bool isPointer() const noexcept
        {
            return !!(m_Low & 1);
        }

        inline static [[nodiscard]] instance_guid_large GenerateGUIDCopy() noexcept
        {
            const auto P = guid_generator::Instance128();
            return {{P.first, P.second}};
        }

        inline instance_guid_large& GenerateGUID() noexcept
        {
            const auto P = guid_generator::Instance128();
            *this = instance_guid_large{{P.first, P.second}};
            return *this;
        }

        static inline
        [[nodiscard]] instance_guid_large GenerateGUIDCopy(const char* str, uint64_t seed1 = 0x548c9decbce65297ULL, uint64_t seed2 = 0x548c9decbce65297ULL) noexcept
        {
            // Constants for MurmurHash3 128-bit variant
            constexpr uint64_t m1 = 0x87c37b91114253d5ULL;
            constexpr uint64_t m2 = 0x4cf5ad432745937fULL;

            uint64_t h1 = seed1;  // Will become high 64 bits
            uint64_t h2 = seed2;  // Will become low 64 bits
            int len = 0;

            const unsigned char* data = reinterpret_cast<const unsigned char*>(str);

            // Process the string 16 bytes at a time (128 bits)
            while (*data) 
            {
                uint64_t k1 = 0;
                uint64_t k2 = 0;

                // Read up to 16 bytes (8 for k1, 8 for k2)
                for (int i = 0; i < 8 && *data; i++) 
                {
                    k1 |= static_cast<uint64_t>(*data++) << (i * 8);
                    len++;
                }
                for (int i = 0; i < 8 && *data; i++) 
                {
                    k2 |= static_cast<uint64_t>(*data++) << (i * 8);
                    len++;
                }

                // Mix k1 into h1
                k1 *= m1;
                k1 ^= k1 >> 33;
                k1 *= m2;
                h1 ^= k1;

                // Mix k2 into h2
                k2 *= m2;
                k2 ^= k2 >> 33;
                k2 *= m1;
                h2 ^= k2;

                // Mix h1 and h2
                h1 ^= h2;
                h2 ^= h1;
            }

            // Finalization
            h1 ^= len;
            h2 ^= len;

            h1 += h2;
            h2 += h1;

            h1 ^= h1 >> 33;
            h1 *= 0x85ebca6b;
            h1 ^= h1 >> 13;
            h1 *= 0xc2b2ae35;
            h1 ^= h1 >> 16;

            h2 ^= h2 >> 33;
            h2 *= 0x85ebca6b;
            h2 ^= h2 >> 13;
            h2 *= 0xc2b2ae35;
            h2 ^= h2 >> 16;

            h1 += h2;
            h2 += h1;

            // For instance GUIDs, set the least significant bit to 1
            h2 = (h2 << 1) | 1;

            return instance_guid_large{ {h2, h1}};
        }

        inline
        instance_guid_large& GenerateGUID(const char* str, uint64_t seed1 = 0x548c9decbce65297ULL, uint64_t seed2 = 0x548c9decbce65297ULL) noexcept
        {
            *this = GenerateGUIDCopy(str,seed1,seed2);
            return *this;
        }

        static consteval
        [[nodiscard]] instance_guid_large generate_guid(const char* str, uint64_t h1 = 0x548c9decbce65297ULL, uint64_t h2 = 0x548c9decbce65297ULL, int len = 0) noexcept
        {
            constexpr uint64_t m1 = 0x87c37b91114253d5ULL;  // Multiplication constant for h1
            constexpr uint64_t m2 = 0x4cf5ad432745937fULL;  // Multiplication constant for h2

            // Process string character-by-character
            return (*str == '\0') ?
                // Finalization when string ends
                [](uint64_t h1, uint64_t h2, int len) consteval noexcept -> instance_guid_large
                {
                    // Finalization mix for h1
                    h1 ^= len;
                    h1 ^= h1 >> 33;
                    h1 *= 0x85ebca6b;
                    h1 ^= h1 >> 13;
                    h1 *= 0xc2b2ae35;
                    h1 ^= h1 >> 16;

                    // Finalization mix for h2
                    h2 ^= len;
                    h2 ^= h2 >> 33;
                    h2 *= 0x85ebca6b;
                    h2 ^= h2 >> 13;
                    h2 *= 0xc2b2ae35;
                    h2 ^= h2 >> 16;

                    // Additional mixing between h1 and h2
                    h1 += h2;
                    h2 += h1;

                    // Set LSB to 1 for instance GUID
                    h2 = (h2 << 1) | 1;

                    return instance_guid_large{.m_Low = h1, .m_High = h2};
                }(h1, h2, len) :
                // Continue hashing: alternate between affecting h1 and h2
                (len % 2 == 0) ?
                    generate_guid(str + 1, 
                                 (h1 ^ static_cast<uint64_t>(*str)) * m1,  // Mix current char into h1
                                 h2,
                                 len + 1) :
                    generate_guid(str + 1,
                                 h1,
                                 (h2 ^ static_cast<uint64_t>(*str)) * m2,  // Mix current char into h2
                                 len + 1);
        }
    };

    //------------------------------------------------------------------------------------------------

    using instance_guid     = guid< struct rsc_instance_guid_tag >;
    using type_guid         = guid< struct rsc_type_guid_tag >;

    //------------------------------------------------------------------------------------------------

    template< typename T_INSTANCE_TYPE >
    struct full_guid_t
    {
        T_INSTANCE_TYPE         m_Instance;
        type_guid               m_Type;

        constexpr bool operator == (const full_guid_t& B) const noexcept
        {
            return m_Instance == B.m_Instance && m_Type == B.m_Type;
        }

        constexpr bool operator != (const full_guid_t& B) const noexcept
        {
            return m_Instance != B.m_Instance || m_Type != B.m_Type;
        }

        constexpr bool empty() const noexcept
        {
            return m_Instance.empty();
        }

        void clear() noexcept
        {
            m_Instance.clear();
            m_Type.clear();
        }

        constexpr bool isValid() const noexcept
        {
            return m_Instance.isValid();
        }
    };

    //------------------------------------------------------------------------------------------------
    
    using full_guid       = full_guid_t<instance_guid>;
    using full_guid_large = full_guid_t<instance_guid_large>;

    //------------------------------------------------------------------------------------------------
    // This is provided when you want to use a full guid yet still be type safe...
    // Honestly I am not sure if this is even a good idea it could literally be an anti-pattern
    // since you should be using stead a def_guid. But none the less I will leave it here
    // even if it is for documented that this pattern was considered.
    template<typename T_TAG>
    struct full_guid_taged : full_guid
    {
        using type = T_TAG;
        using full_guid::full_guid_t;
        full_guid_taged( T_TAG V ) : full_guid( V.m_Instance, V.m_Type ) {}
    };

    //------------------------------------------------------------------------------------------------

    template< typename T_INSTANCE_TYPE, type_guid T_TYPE_GUID_V >
    struct def_guid_t
    {
        T_INSTANCE_TYPE                 m_Instance;     // also used in the pool as the next empty entry...
        inline static constexpr auto    m_Type = T_TYPE_GUID_V;

        template< typename T>
        constexpr bool operator == (const T& B) const noexcept
        {
            return m_Instance == B.m_Instance && m_Type == B.m_Type;
        }

        constexpr bool empty() const noexcept
        {
            return m_Instance.m_Value == 0;
        }

        void clear() noexcept
        {
            m_Instance.m_Value = 0;
        }

        constexpr bool isValid() const noexcept
        {
            return m_Instance.isValid();
        }

        operator full_guid() const noexcept
        {
            return full_guid{ m_Instance, m_Type };
        }
    };

    //------------------------------------------------------------------------------------------------
    template< typename type_guid T_TYPE_GUID_V >
    using def_guid = def_guid_t<instance_guid, T_TYPE_GUID_V>;

    template< typename type_guid T_TYPE_GUID_V >
    using def_guid_large = def_guid_t<instance_guid_large, T_TYPE_GUID_V>;

    //------------------------------------------------------------------------------------------------
    
    template< typename T >
    inline static std::size_t ComputeHash(guid<T> k) noexcept
    {
        return std::hash<std::uint64_t>{}(k.m_Value);
    }

    //------------------------------------------------------------------------------------------------

    inline
    static std::size_t ComputeHash(const full_guid& k) noexcept
    {
        // Use a combination of the two uint64_t values
        std::size_t Value = std::hash<std::uint64_t>{}(k.m_Instance.m_Value) ^ (std::hash<std::uint64_t>{}(k.m_Type.m_Value) << 1);
        return Value;
    }

    //------------------------------------------------------------------------------------------------

    template<type_guid T_TYPE_GUID_V >
    inline
    static std::size_t ComputeHash(const def_guid<T_TYPE_GUID_V>& k) noexcept
    {
        // Use a combination of the two uint64_t values
        std::size_t Value = std::hash<std::uint64_t>{}(k.m_Instance) ^ (std::hash<std::uint64_t>{}(k.m_Type) << 1);
        return Value;
    }
}

//------------------------------------------------------------------------------------------------
// Support for std::hash
//------------------------------------------------------------------------------------------------
namespace std
{
    template<>
    struct hash<xresource::instance_guid>
    {
        inline
        std::size_t operator()(const xresource::instance_guid& k) const noexcept
        {
            return std::hash<std::uint64_t>{}(k.m_Value);
        }
    };
    template<>
    struct hash<xresource::type_guid>
    {
        inline
        std::size_t operator()(const xresource::type_guid& k) const noexcept
        {
            return std::hash<std::uint64_t>{}(k.m_Value);
        }
    };
    template<>
    struct hash<xresource::instance_guid_large>
    {
        inline
        std::size_t operator()(const xresource::instance_guid_large& k) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(k.m_Low);
            std::size_t h2 = std::hash<uint64_t>{}(k.m_High);

            // Combine the hashes (simple but effective method)
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    template<>
    struct hash<xresource::full_guid>
    {
        inline
        std::size_t operator()(const xresource::full_guid& k) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(k.m_Instance.m_Value);
            std::size_t h2 = std::hash<uint64_t>{}(k.m_Type.m_Value);

            // Combine the hashes (simple but effective method)
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    template<typename T>
    struct hash<xresource::full_guid_taged<T>>
    {
        inline
        std::size_t operator()(const xresource::full_guid& k) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(k.m_Instance.m_Value);
            std::size_t h2 = std::hash<uint64_t>{}(k.m_Type.m_Value);

            // Combine the hashes (simple but effective method)
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };


    template<>
    struct hash<xresource::full_guid_large>
    {
        inline
        std::size_t operator()(const xresource::full_guid_large& k) const noexcept
        {
            std::size_t h1 = std::hash<xresource::instance_guid_large>{}(k.m_Instance);
            std::size_t h2 = std::hash<uint64_t>{}(k.m_Type.m_Value);

            // Combine the hashes (simple but effective method)
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    template< xresource::type_guid T_TYPE_GUID_V >
    struct hash<xresource::def_guid<T_TYPE_GUID_V>>
    {
        inline
        std::size_t operator()(const xresource::def_guid<T_TYPE_GUID_V>& k) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(k.m_Instance.m_Value);
            std::size_t h2 = std::hash<uint64_t>{}(k.m_Type.m_Value);

            // Combine the hashes (simple but effective method)
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
    template< xresource::type_guid T_TYPE_GUID_V >
    struct hash<xresource::def_guid_large<T_TYPE_GUID_V>>
    {
        inline
        std::size_t operator()(const xresource::def_guid_large<T_TYPE_GUID_V>& k) const noexcept
        {
            std::size_t h1 = std::hash<xresource::instance_guid_large>{}(k.m_Instance);
            std::size_t h2 = std::hash<uint64_t>{}(k.m_Type.m_Value);

            // Combine the hashes (simple but effective method)
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}

#endif