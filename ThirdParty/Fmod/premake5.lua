project "Fmod"
	kind "Utility"
	language "C++"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
	    "inc/**.h",
		"inc/**.hpp"
	}

	links
	{
	    "lib/fmod_vc.lib",
	    "lib/fmodL_vc.lib",
		"lib/fmodstudioL_vc.lib",
		"lib/fmodstudio_vc.lib"
	}


	filter "system:windows"
		systemversion "latest"
		staticruntime "On" --staticly linking the runtime libraries

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"