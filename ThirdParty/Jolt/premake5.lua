project "Jolt"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"

    
    staticruntime "off"

    buildoptions { "/bigobj", "/MP" }

    targetdir ("../../Build/bin/" .. outputdir .. "/Ermine-Editor/Jolt")
    objdir ("../../Build/obj/" .. outputdir .. "/Ermine-Editor/Jolt")

    files
    {
        "**.h",
        "**.cpp"
    }

    includedirs
    {
        "../"
    }

    defines {"JPH_DEBUG_RENDERER","JPH_USE_SUBMERGENCE" }

    filter "configurations:*Debug"
        targetname "joltD"
        runtime "Debug"   -- /MDd
        symbols "on"

    filter "configurations:*Release"
        targetname "jolt"
        runtime "Release" -- /MD
        optimize "on"

    filter "system:windows"
        systemversion "latest"