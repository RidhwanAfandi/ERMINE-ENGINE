project "imnodes"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    -- Match engine’s dynamic CRT to avoid LNK2038/LNK4098
    staticruntime "off"

    buildoptions { "/bigobj", "/MP" } -- optional, safe for MSVC

    targetdir ("../../Build/bin/" .. outputdir .. "/Ermine-Editor/imnodes")
    objdir ("../../Build/obj/" .. outputdir .. "/Ermine-Editor/imnodes")

    files
    {
        "**.h",
        "**.cpp"
    }

    includedirs
    {
        "../",         -- root of imnodes (for imnodes.h)
        "../imgui"  -- if imnodes depends on ImGui
    }

    filter "configurations:*Debug"
        targetname "imnodesD"
        runtime "Debug"   -- /MDd
        symbols "on"

    filter "configurations:*Release"
        targetname "imnodes"
        runtime "Release" -- /MD
        optimize "on"

    filter "system:windows"
        systemversion "latest"