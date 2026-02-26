project "Glad"
	kind "StaticLib"
	language "C"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"include/glad/glad.h",
		"include/KHR/khrplatform.h",
		"src/glad.c",
		"vendor/stb/stb_image.h"
	}

	filter "system:windows"
		systemversion "latest"
		staticruntime "On" --staticly linking the runtime libraries

		
	includedirs
	{
		"include",
		"vendor/stb"
	}

	filter "configurations:Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		runtime "Release"
		optimize "on"