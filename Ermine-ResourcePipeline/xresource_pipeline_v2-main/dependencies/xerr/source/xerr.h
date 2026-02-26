#ifndef XERROR_H
#define XERROR_H
#pragma once

#include <array>
#include <atomic>
#include <string>
#include <source_location>

//-----------------------------------------------------------------------------------------
// XERR DETAILS
//-----------------------------------------------------------------------------------------
namespace xerr_details
{
    template<std::size_t N>
    struct string_literal
    {
        std::array<char, N> m_Value;
        consteval string_literal(const char(&str)[N]) noexcept { for (std::size_t i = 0; i < N; ++i) m_Value[i] = str[i]; }
    };

    //------------------------------------------------------------------------------------

    struct chain_pool
    {
        struct node
        {
            const char*  m_pError;  // Store XErr directly
            std::int16_t m_iNext;   // Index to next node (-1 for end)
            std::int16_t m_iPrev;   // Index to previous node (-1 for end)
        };

        inline                  chain_pool  (void)                                          noexcept;
        inline std::int16_t     Alloc       (void)                                          noexcept;
        inline void             Free        ( std::int16_t& iHead, std::int16_t& iTail )    noexcept;

        std::array<node, 1024>      m_Pool;
        std::atomic<std::int16_t>   m_iEmpty{ -1 };
    };
}

//-----------------------------------------------------------------------------------------
// XERR
//-----------------------------------------------------------------------------------------

struct xerr
{
    // Debugging callback for logging and special handling
    using fn_error_callback = void(const char* pEnumValue, std::uint8_t State, std::string_view Message, std::uint32_t Line, std::string_view file );

    // Handy object for cleaning up scopes that have errors
    template< typename T_CALLBACK> struct cleanup
    {
        inline  cleanup ( xerr& Error, T_CALLBACK&& Lambda )    noexcept : m_Error(Error), m_Lambda(std::move(Lambda)) {}
        inline ~cleanup (void)                                  noexcept { if (m_Error) m_Lambda(); }
        xerr&      m_Error;
        T_CALLBACK m_Lambda;
    };

    // The default states for xerr. Please note that this could be customized per error class
    enum class default_states : std::uint8_t
    { OK        = 0
    , FAILURE   = 1
    // The user can customize their own error states by copying this enum and then
    // adding additional states below these two
    };

    constexpr                                   operator bool               (void)                              const   noexcept;
    inline                  void                clear                       (void)                                      noexcept;
    constexpr               std::uint32_t       getStateUID                 (void)                              const   noexcept;

    template< typename T_STATE_ENUM >
    consteval static        std::uint32_t       fromStateUID                (void)                                      noexcept requires (std::is_enum_v<T_STATE_ENUM>);

    template< typename T_STATE_ENUM >
    constexpr               bool                isState                     (void)                              const   noexcept requires (std::is_enum_v<T_STATE_ENUM>);
    constexpr               bool                hasChain                    (void)                              const   noexcept;
    inline                  std::string_view    getMessage                  (void)                              const   noexcept;
    inline                  std::string_view    getHint                     (void)                              const   noexcept;
    inline static           std::string_view    getMessageFromMsg           (std::string_view msg)                      noexcept;
    inline static           std::string_view    getHintFromMsg              (std::string_view msg)                      noexcept;
    inline static           std::string_view    getMessageFromString        (const char* pMessage)                      noexcept;
    inline static           std::string_view    getHintFromString           (const char* pMessage)                      noexcept;
    template< typename T_CALLBACK> 
    inline                  void                ForEachInChain              (T_CALLBACK&& Callback )            const   noexcept requires std::invocable<T_CALLBACK, xerr>;
    template< typename T_CALLBACK>
    inline                  void                ForEachInChainBackwards     (T_CALLBACK&& Callback)             const   noexcept requires std::invocable<T_CALLBACK, xerr>;

    template< typename T_STATE_ENUM >
    constexpr               T_STATE_ENUM        getState                    (void)                              const   noexcept requires (std::is_enum_v<T_STATE_ENUM>);

    template <auto T_STATE_V, xerr_details::string_literal T_STR_V>
    constexpr static        xerr                create                      (const std::source_location loc = std::source_location::current())                          noexcept requires (std::is_enum_v<decltype(T_STATE_V)>);

    template <auto T_STATE_V, xerr_details::string_literal T_STR_V>
    constexpr static        xerr                create                      (const xerr PrevError, const std::source_location loc = std::source_location::current())    noexcept requires (std::is_enum_v<decltype(T_STATE_V)>);

    template <typename T_STATE_ENUM, xerr_details::string_literal T_STR_V>
    constexpr static        xerr                create_f                    (const std::source_location loc = std::source_location::current())                          noexcept requires (std::is_enum_v<T_STATE_ENUM>);

    template <typename T_STATE_ENUM, xerr_details::string_literal T_STR_V>
    constexpr static        xerr                create_f                    (const xerr PrevError, const std::source_location loc = std::source_location::current())    noexcept requires (std::is_enum_v<T_STATE_ENUM>);

    template <auto T_STATE_V>
    constexpr static        void                LogMessage                  ( std::string_view Message, const std::source_location loc = std::source_location::current())noexcept requires (std::is_enum_v<decltype(T_STATE_V)>);

    template <auto T_STATE_V>
    constexpr static        void                LogMessage                  ( std::string&& Message, const std::source_location loc = std::source_location::current() )  noexcept requires (std::is_enum_v<decltype(T_STATE_V)>);

    const char*                                 m_pMessage      = nullptr;

    inline static xerr_details::chain_pool      m_ChainPool     = {};
    inline static fn_error_callback*            m_pCallback     = nullptr;
};

//-----------------------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------------------
#include "implementation/xerr_inline.h"

#endif
