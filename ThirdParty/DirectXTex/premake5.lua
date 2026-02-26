project "DirectXTex"
	kind "StaticLib"
	language "C++"

	staticruntime "off"

	targetdir ("bin/" .. outputdir .. "/%{prj.name}")
	objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
	    "inc/**.h",
		"inc/**.hpp",
		"inc/**.inl"
	}

	links
	{
	    "DirectXTex.lib"
	}

	filter "system:windows"
		systemversion "latest"

	filter "configurations:*Debug"
		runtime "Debug"
		symbols "on"

	filter "configurations:*Release"
		runtime "Release"
		optimize "on"