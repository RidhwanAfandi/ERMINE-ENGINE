workspace "Ermine"
    architecture "x64"
    configurations { "Editor-Debug", "Editor-Release", "Game-Debug", "Game-Release" }
    startproject "Ermine-Editor"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
fmod_dll = "../ThirdParty/Fmod/lib/fmod.dll"
fmodL_dll = "../ThirdParty/Fmod/lib/fmodL.dll"
fmodstudio_dll = "../ThirdParty/Fmod/lib/fmodstudio.dll"
fmodstudioL_dll = "../ThirdParty/Fmod/lib/fmodstudioL.dll"
mono_dll = "../ThirdParty/Mono/lib/mono-2.0-sgen.dll"
mono_assembly = "../ThirdParty/Mono/lib/"
mono_config = "../ThirdParty/Mono/etc"
assimp_dll = "../ThirdParty/assimp/bin/assimp-vc143-mt.dll"

-- Include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "ThirdParty/GLFW/include"
IncludeDir["Glad"] = "ThirdParty/Glad/include"
IncludeDir["ImGui"] = "ThirdParty/imgui"
IncludeDir["glm"] = "ThirdParty/glm"
IncludeDir["spdlog"] = "ThirdParty/spdlog/include"
IncludeDir["stb"] = "ThirdParty/stb"
IncludeDir["Mono"] = "ThirdParty/Mono/include"
IncludeDir["rapidjson"] = "ThirdParty/rapidjson"
IncludeDir["Fmod"] = "ThirdParty/Fmod/inc"
IncludeDir["Jolt"] = "ThirdParty"
IncludeDir["assimp"] = "ThirdParty/assimp/include"
IncludeDir["DirectXTex"] = "ThirdParty/DirectXTex/inc"
IncludeDir["xproperty"] = "ThirdParty/xproperty/source"
IncludeDir["xresource_Pipeline"] = "ThirdParty/xresource_pipeline_v2-main/source"
IncludeDir["imnodes"] = "ThirdParty/imnodes"
IncludeDir["recastnavigation"] = "ThirdParty/recastnavigation"
IncludeDir["pl_mpeg"] = "ThirdParty/pl_mpeg"

-- Libraries
LibraryDir = {}
LibraryDir["Fmod"] = "ThirdParty/Fmod/lib"
LibraryDir["Mono"] = "ThirdParty/Mono/lib"
LibraryDir["assimp"] = "ThirdParty/assimp/lib"
LibraryDir["DirectXTex"] = "ThirdParty/DirectXTex/lib"

-- External libraries
group "Dependencies"
    include "ThirdParty/GLFW"
    include "ThirdParty/Glad"
    include "ThirdParty/imgui"
    include "ThirdParty/Fmod"
    include "ThirdParty/Mono"
    include "ThirdParty/Jolt"
    include "ThirdParty/imnodes"
    include "ThirdParty/recastnavigation"
group ""

-- Engine Project
project "Ermine-Engine"
    location "Ermine-Engine"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off" -- Use dynamic runtime

    buildoptions { "/MP", "/bigobj" } -- Enable multi-processor compilation and big object files

    targetdir ("Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("Build/obj/" .. outputdir .. "/%{prj.name}")

    pchheader "PreCompile.h"
    pchsource "Ermine-Engine/src/PreCompile.cpp"

    files
    {
        "%{prj.name}/include/**.h",
        "%{prj.name}/include/**.tpp",
        "%{prj.name}/src/**.tpp",
        "%{prj.name}/src/**.cpp",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xtextfile/source/xtextfile.cpp",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xtextfile/source/xtextfile.h"
    }

    includedirs
    {
        "%{prj.name}/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.stb}",
        "%{IncludeDir.Fmod}",
        "%{IncludeDir.Mono}",
        "%{IncludeDir.Jolt}",
        "%{IncludeDir.rapidjson}",
        "%{IncludeDir.assimp}",
        "%{IncludeDir.DirectXTex}",
        "%{IncludeDir.xproperty}",
        "%{IncludeDir.recastnavigation}/Recast/Include",
        "%{IncludeDir.recastnavigation}/Detour/Include",
        "%{IncludeDir.recastnavigation}/DetourCrowd/Include",
        "%{IncludeDir.recastnavigation}/DetourTileCache/Include",
        "%{IncludeDir.recastnavigation}/DebugUtils/Include",
        "%{IncludeDir.xresource_Pipeline}",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xtextfile/source",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xerr/source",
        "%{IncludeDir.imnodes}",
        "%{IncludeDir.pl_mpeg}"
    }

    libdirs
    {
        "%{LibraryDir.Fmod}",
        "%{LibraryDir.Mono}",
        "%{LibraryDir.assimp}"
        -- "%{LibraryDir.DirectXTex}"
    }

    links
    {
        "GLFW",
        "Glad",
        "ImGui",
        "fmod_vc",
        "fmodL_vc",
        "fmodstudio_vc",
        "fmodstudioL_vc",
        "opengl32.lib",
		"mono-2.0-sgen.lib",
        "Jolt",
        "assimp-vc143-mt.lib",
        "imnodes",
        "recastnavigation"
        --"DirectXTex.lib"
    }

    postbuildcommands
    {
        -- Ensure output subfolders exist
        ("{MKDIR} \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{MKDIR} \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),
        ("{MKDIR} \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-ResourcePipeline\""),

        -- Copy engine DLL to Editor and Game
        ("{COPY} \"$(TargetPath)\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{COPY} \"$(TargetPath)\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),

        -- Copy resource pipeline database and project to output dir
        ("{COPYDIR} ../Ermine-ResourcePipeline/Ermine-Game.lion_rcdbase \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game.lion_rcdbase\""),
        ("{COPYDIR} ../Ermine-ResourcePipeline/Ermine-Game.lion_project \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game.lion_project\""),

        -- Runtime DLLs (Editor)
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmod.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmodL.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmodstudio.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmodstudioL.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Mono\\lib\\mono-2.0-sgen.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\assimp\\bin\\assimp-vc143-mt.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\""),

        -- Runtime DLLs (Game)
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmod.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmodL.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmodstudio.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Fmod\\lib\\fmodstudioL.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Mono\\lib\\mono-2.0-sgen.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\assimp\\bin\\assimp-vc143-mt.dll\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\""),

        -- Mono redist
        ("{COPY} \"$(SolutionDir)ThirdParty\\Mono\\lib\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\\mono\\lib\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Mono\\etc\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Editor\\mono\\etc\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Mono\\lib\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\\mono\\lib\""),
        ("{COPY} \"$(SolutionDir)ThirdParty\\Mono\\etc\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-Game\\mono\\etc\""),

        -- Resources (shared)
        ("{COPY} \"$(SolutionDir)Resources\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Resources\"")
    }

    filter "configurations:Editor-Debug or configurations:Game-Debug"
        links { "fmodL_vc", "fmodstudioL_vc" }

    filter "configurations:Editor-Release or configurations:Game-Release"
        links { "fmod_vc", "fmodstudio_vc" }

    filter "system:windows"
        systemversion "latest"

        warnings "Extra"

        buildoptions {  "/wd4251", -- Level 2 dll-interface to be used by clients of class 'class'; see C4251
                        "/wd4005", -- Level 1 macro redefinition
                        "/wd4267", -- Level 3 conversion from 'size_t' to 'type', possible loss of data
                        "/wd5054"  -- Level 4 operator '|': deprecated between enumerations of different types (Due to Rapidjson library at document.h)
        }

        defines
        {
            "EE_PLATFORM_WINDOWS",
            "EE_BUILD_DLL",
            "ERMINE_USE_SPDLOG",
            "GLFW_INCLUDE_NONE",
            "IMGUI_DEFINE_MATH_OPERATORS",
            "GLM_ENABLE_EXPERIMENTAL",
            "_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING", -- To slience the warnings from Rapidjson
            "JPH_USE_SUBMERGENCE",
            "JPH_DEBUG_RENDERER"
        }

    -- Editor vs Game feature flags for the engine build
    filter "configurations:*Editor*"
        defines { "EE_EDITOR" }

    filter "configurations:*Game*"
        defines { "EE_GAME" }

    filter "configurations:Editor-Debug or configurations:Game-Debug"
        defines "EE_DEBUG"
        runtime "Debug"
        symbols "on"
        linkoptions { "/NODEFAULTLIB:LIBCMTD", "/NODEFAULTLIB:LIBCMT", "/NODEFAULTLIB:MSVCRT", "/IGNORE:4099"}
        defines { "VERBOSE_LOGGING=1" }
        libdirs { "%{LibraryDir.DirectXTex}" }
        links { "DirectXTexD.lib" }
        postbuildcommands {("{COPYFILE} \"$(SolutionDir)ThirdParty\\DirectXTex\\lib\\DirectXTexD.pdb\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-ResourcePipeline\\DirectXTex.pdb\"")}

    filter "configurations:Editor-Release or configurations:Game-Release"
        defines "EE_RELEASE"
        runtime "Release"
        optimize "on"
        linkoptions { "/NODEFAULTLIB:LIBCMT", "/NODEFAULTLIB:LIBCMTD", "/NODEFAULTLIB:MSVCRTD" }
        defines { "VERBOSE_LOGGING=0" }
        libdirs { "%{LibraryDir.DirectXTex}" }
        links { "DirectXTex.lib" }
        postbuildcommands {("{COPYFILE} \"$(SolutionDir)ThirdParty\\DirectXTex\\lib\\DirectXTex.pdb\" \"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-ResourcePipeline\\DirectXTex.pdb\"")}

-- Editor Project
project "Ermine-Editor"
    location "Ermine-Editor"
    language "C++"
    cppdialect "C++20"
    staticruntime "off" -- Use dynamic runtime

    targetdir ("Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("Build/obj/" .. outputdir .. "/%{prj.name}")
    debugdir ("Build/bin/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xtextfile/source/xtextfile.cpp",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xtextfile/source/xtextfile.h"
    }

    includedirs
    {
        "%{prj.name}/include",
        "Ermine-Engine/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.stb}",
        "%{IncludeDir.Fmod}",
        "%{IncludeDir.Mono}",
        "%{IncludeDir.rapidjson}",
        "%{IncludeDir.xproperty}",
        "%{IncludeDir.recastnavigation}/Recast/Include",
        "%{IncludeDir.recastnavigation}/Detour/Include",
        "%{IncludeDir.recastnavigation}/DetourCrowd/Include",
        "%{IncludeDir.recastnavigation}/DetourTileCache/Include",
        "%{IncludeDir.recastnavigation}/DebugUtils/Include",
        "%{IncludeDir.xresource_Pipeline}",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xtextfile/source",
        "ThirdParty/xresource_pipeline_v2-main/dependencies/xerr/source",
        "%{IncludeDir.imnodes}",
        "%{IncludeDir.pl_mpeg}"
    } 

    -- Ensure the resource pipeline builds before running it
    dependson { "Ermine-ResourcePipeline" }

    prebuildcommands
    {
        'cd "%{wks.location}"',
        -- Run ResourcePipeline from Premake output directory
        ("\"%{wks.location}Build\\bin\\" .. outputdir .. "\\Ermine-ResourcePipeline\\Ermine-ResourcePipeline.exe\""),
        'if errorlevel 1 exit 1'
    }
    
    postbuildcommands
    {
        '{COPYDIR} "%{wks.location}Ermine-ResourcePipeline/Ermine-Game.lion_rcdbase" "%{cfg.targetdir}/../Ermine-Game.lion_rcdbase"'
    }

    links
    {
        "Ermine-Engine",
        "Ermine-ScriptAssembly",
        "Ermine-ScriptSandbox"
    }

    filter "system:windows"
        systemversion "latest"

        buildoptions { "/wd4251", "/wd4005"}

        defines
        {
            "EE_PLATFORM_WINDOWS"
        }

    filter "configurations:*Editor*"
        defines { "EE_EDITOR" }

    filter "configurations:*Game*"
        defines { "EE_GAME" } -- if selected, nothing SHOULD happen

    filter "configurations:Game-Debug"
        kind "ConsoleApp"
        defines "EE_DEBUG"

    filter "configurations:Game-Release"
        kind "WindowedApp"
        defines "EE_RELEASE"

    filter "configurations:Editor-Debug"
        defines "EE_DEBUG"
        kind "ConsoleApp"
        runtime "Debug"
        symbols "on"
        linkoptions { "/NODEFAULTLIB:LIBCMTD" }
        libdirs { "%{LibraryDir.DirectXTex}" }
        links { "DirectXTexD.lib" }

    filter "configurations:Editor-Release"
        defines "EE_RELEASE"
        kind "WindowedApp"
        runtime "Release"
        optimize "on"
        linkoptions { "/NODEFAULTLIB:LIBCMT" }
        libdirs { "%{LibraryDir.DirectXTex}" }
        links { "DirectXTex.lib" }

-- Game Project
project "Ermine-Game"
    location "Ermine-Game"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("Build/obj/" .. outputdir .. "/%{prj.name}")
    debugdir ("Build/bin/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
    }

    includedirs
    {
        "%{prj.name}/include",
        "Ermine-Engine/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        -- "%{IncludeDir.ImGui}", -- May not be needed in the game
        "%{IncludeDir.glm}",
        "%{IncludeDir.spdlog}",
        "%{IncludeDir.stb}",
        "%{IncludeDir.Fmod}",
        "%{IncludeDir.Mono}",
        "%{IncludeDir.rapidjson}",
        "%{IncludeDir.pl_mpeg}"
    }

    links
    {
        "Ermine-Engine",
        "Ermine-ScriptAssembly",
        "Ermine-ScriptSandbox"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/wd4251", "/wd4005" }
        defines { "EE_PLATFORM_WINDOWS" }

    filter "configurations:*Editor*"
        defines { "EE_EDITOR" }

    filter "configurations:*Game*"
        defines { "EE_GAME" }

    filter "configurations:Editor-Debug"
        kind "ConsoleApp"
        defines "EE_DEBUG"

    filter "configurations:Editor-Release"
        kind "WindowedApp"
        defines "EE_RELEASE"

    filter "configurations:Game-Debug"
        defines "EE_DEBUG"
        kind "ConsoleApp"
        runtime "Debug"
        symbols "on"
        linkoptions { "/NODEFAULTLIB:LIBCMTD" }

    filter "configurations:Game-Release"
        defines "EE_RELEASE"
        kind "WindowedApp"
        runtime "Release"
        optimize "on"
        linkoptions { "/NODEFAULTLIB:LIBCMT" }

-- Resource pipeline project
project "Ermine-ResourcePipeline"
    location "Ermine-ResourcePipeline"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("Build/obj/" .. outputdir .. "/%{prj.name}")

    files {
        "%{prj.name}/Ermine-ResourcePipeline.cpp",
        "%{prj.name}/xresource_pipeline_v2-main/dependencies/xtextfile/source/xtextfile.cpp",
        "%{prj.name}/xresource_pipeline_v2-main/dependencies/xtextfile/source/xtextfile.h"
    }

    includedirs {
        "%{prj.name}/xresource_pipeline_v2-main/source",
        "%{prj.name}/xresource_pipeline_v2-main/dependencies/xtextfile/source",
        "%{prj.name}/xresource_pipeline_v2-main/dependencies/xerr/source",
        "%{IncludeDir.DirectXTex}",
        "%{IncludeDir.assimp}"
    }

    libdirs
    {
        "%{LibraryDir.DirectXTex}",
        "%{LibraryDir.assimp}"
    }

    postbuildcommands
    {
        "{COPY} \"$(SolutionDir)ThirdParty\\assimp\\bin\\assimp-vc143-mt.dll\" \"$(TargetDir)\""
    }

    warnings "Extra"
    characterset "Unicode"

    filter "system:windows"
        defines { "PLATFORM_WINDOWS" }

    filter "configurations:*Debug"
        defines "EE_DEBUG"
        runtime "Debug"
        symbols "on"
        linkoptions { "/NODEFAULTLIB:LIBCMTD", "/IGNORE:4099", "/IGNORE:4204" }
        libdirs { "%{LibraryDir.DirectXTex}", "%{LibraryDir.assimp}" }
        links { "DirectXTexD.lib", "assimp-vc143-mt.lib" }

    filter "configurations:*Release"
        defines "EE_RELEASE"
        runtime "Release"
        optimize "on"
        linkoptions { "/NODEFAULTLIB:LIBCMT" }
        libdirs { "%{LibraryDir.DirectXTex}" ,"%{LibraryDir.assimp}" }
        links { "DirectXTex.lib", "assimp-vc143-mt.lib" }

-- Script Assembly Project
project "Ermine-ScriptAssembly"
    location "Ermine-ScriptAssembly"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"
    namespace "Ermine.ScriptAssembly"

    targetdir ("Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("Build/obj/" .. outputdir .. "/%{prj.name}")

    files { "%{prj.name}/**.cs" }

    filter "system:windows"
        systemversion "latest"
    filter "configurations:Editor-Debug or configurations:Game-Debug"
        defines { "DEBUG" }
        symbols "on"
    filter "configurations:Editor-Release or configurations:Game-Release"
        defines { "NDEBUG" }
        optimize "on"

-- Script Sandbox project
project "Ermine-ScriptSandbox"
    location "Ermine-ScriptSandbox"
    kind "SharedLib"
    language "C#"
    dotnetframework "4.7.2"
    namespace "Ermine.ScriptSandbox"

    targetdir ("Build/bin/" .. outputdir .. "/%{prj.name}")
    objdir ("Build/obj/" .. outputdir .. "/%{prj.name}")

    files { "%{prj.name}/**.cs" }
    includedirs { "Ermine-ScriptAssembly" }
    links { "Ermine-ScriptAssembly" }

    filter "system:windows"
        systemversion "latest"
    filter "configurations:Editor-Debug or configurations:Game-Debug"
        defines { "DEBUG" }
        symbols "on"
    filter "configurations:Editor-Release or configurations:Game-Release"
        defines { "NDEBUG" }
        optimize "on"