#ifndef XRESOURCE_GUID_PROPERTIES_H
#define XRESOURCE_GUID_PROPERTIES_H
#pragma once

namespace xresource
{
    // Give properties to the type_guid
    struct type_guid_give_properties : xresource::type_guid
    {
        XPROPERTY_DEF
        ("type_guid", xresource::type_guid
        , obj_member < "Value"
            , &xresource::type_guid::m_Value
            , member_flags<flags::SHOW_READONLY>
            , member_ui<std::uint64_t>::drag_bar<0, 0, std::numeric_limits<std::uint64_t>::max(), "%llX">
            , member_help<"64bit Unique identifier for the resource type, this is how the system knows about this resource type. "
                          "This is part of the full 128bit which is the true unique ID of the resource"
            >>
        );
    };
    XPROPERTY_REG(type_guid_give_properties)

    // Give properties to the instance_guid
    struct instance_guid_give_properties : xresource::instance_guid
    {
        XPROPERTY_DEF
        ("instance_guid", xresource::instance_guid
        , obj_member < "Value"
            , &xresource::instance_guid::m_Value
            , member_flags<flags::SHOW_READONLY>
            , member_ui<std::uint64_t>::drag_bar<0, 0, std::numeric_limits<std::uint64_t>::max(), "%llX">
            , member_help<"64bit Unique identifier for the resource, this is how the system knows about this resource "
                          "This is part of the full 128bit which is the true unique ID of the resource"
            >>
        );
    };
    XPROPERTY_REG(instance_guid_give_properties)

    // Give properties to the instance_guid
    struct instance_guid_large_give_properties : xresource::instance_guid_large
    {
        XPROPERTY_DEF
        ("instance_guid", xresource::instance_guid_large
        , obj_member < "Low"
            , &xresource::instance_guid_large::m_Low
            , member_flags<flags::SHOW_READONLY>
            , member_ui<std::uint64_t>::drag_bar<0, 0, std::numeric_limits<std::uint64_t>::max(), "%llX">
            , member_help<"64bit Unique identifier for the resource, this is how the system knows about this resource "
                          "This is part of the full 128bit which is the true unique ID of the resource"
            >>
        , obj_member < "High"
            , &xresource::instance_guid_large::m_High
            , member_flags<flags::SHOW_READONLY>
            , member_ui<std::uint64_t>::drag_bar<0, 0, std::numeric_limits<std::uint64_t>::max(), "%llX">
            , member_help<"64bit Unique identifier for the resource, this is how the system knows about this resource "
                          "This is part of the full 128bit which is the true unique ID of the resource"
            >>
        );
    };
    XPROPERTY_REG(instance_guid_large_give_properties)

    // Give properties to the full_guid
    struct full_guid_give_properties : xresource::full_guid
    {
        XPROPERTY_DEF
        ("full_guid", xresource::full_guid, xproperty::settings::vector2_group
        , obj_member < "Instance"
            , +[](xresource::full_guid& O) constexpr ->std::uint64_t& {return O.m_Instance.m_Value; }
            , member_ui<std::uint64_t>::drag_bar<0, 0, std::numeric_limits<std::uint64_t>::max(), "%llX">
            , member_help<"64bit Unique identifier for the resource, this is how the system knows about this resource "
                          "This is part of the full 128bit which is the true unique ID of the resource"
            >>
        , obj_member < "Type"
            , +[](xresource::full_guid& O) constexpr ->std::uint64_t& {return O.m_Type.m_Value; }
            , member_flags<flags::SHOW_READONLY>
            , member_ui<std::uint64_t>::drag_bar<0, 0, std::numeric_limits<std::uint64_t>::max(), "%llX">
            , member_help<"64bit Unique identifier for the resource type, this is how the system knows what type of resource this is. "
                          "This is part of the full 128bit which is the true unique ID of the resource"
            >>
        );
    };
    XPROPERTY_REG(full_guid_give_properties)

} // namespace xresource
#endif // XRESOURCE_GUID_PROPERTIES_H
