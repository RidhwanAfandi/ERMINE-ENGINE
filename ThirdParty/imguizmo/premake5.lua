project "ImGuizmo"
	kind "StaticLib"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"ImGuizmo.cpp",
		"ImGuizmo.h",

	}

	buildoptions { "/wd26495", "/wd6255", "/wd6263" , "/wd6001"}

	filter "system:windows"
		systemversion "latest"
		staticruntime "On" --staticly linking the runtime libraries


	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"