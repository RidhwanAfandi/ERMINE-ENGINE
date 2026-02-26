project "recastnavigation"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    buildoptions { "/MP", "/bigobj" }

    targetdir ("../../Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("../../Build/obj/" .. outputdir .. "/%{prj.name}")

    files
    {
        "Recast/Include/**.h",
        "Recast/Source/**.cpp",
        "Detour/Include/**.h",
        "Detour/Source/**.cpp",

        "DetourCrowd/Include/**.h",
        "DetourCrowd/Source/**.cpp",
        "DetourTileCache/Include/**.h",
        "DetourTileCache/Source/**.cpp",
        "DebugUtils/Include/**.h",
        "DebugUtils/Source/**.cpp",
    }

    includedirs
    {
        "Recast/Include",
        "Detour/Include",
        "DetourCrowd/Include",
        "DetourTileCache/Include",
        "DebugUtils/Include"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:*Debug"
        targetname "recastnavigationD"
        runtime "Debug"
        symbols "on"

    filter "configurations:*Release"
        targetname "recastnavigation"
        runtime "Release"
        optimize "on"