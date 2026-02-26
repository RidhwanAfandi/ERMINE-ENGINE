cmake_minimum_required(VERSION 3.15)

# Set TARGET_PROJECT before any includes (required by xcmake_tools)
set(TARGET_PROJECT "xresource_pipeline_project")
project(xresource_pipeline_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set up FetchContent
include(FetchContent)

# First, fetch xcmake_tools to get the Common.cmake
FetchContent_Declare(
    xcmake_tools
    GIT_REPOSITORY https://github.com/LIONant-depot/xcmake_tools.git
    GIT_TAG main
)

FetchContent_MakeAvailable(xcmake_tools)

# Copy xcmake_tools to the expected location for dependencies
set(CMAKE_BINARY_DIR_DEPS ${CMAKE_BINARY_DIR}/_deps)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR_DEPS})
file(COPY ${xcmake_tools_SOURCE_DIR}/ DESTINATION ${CMAKE_BINARY_DIR_DEPS}/xcmake_tools/)

# Now include the common CMake utilities
include(${xcmake_tools_SOURCE_DIR}/Common.cmake)

# Now use the original dependency setup
FetchAndPopulate("https://github.com/LIONant-depot/xtextfile.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xcmdline.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xresource_guid.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xproperty.git" "master")
FetchAndPopulate("https://github.com/LIONant-depot/xstrtool.git" "main")

# Create interface component as defined in the original dependency CMakeLists.txt
DefineInterfaceComponent(xresource_pipeline_v2 "dependencies/xcore"
  "source/xresource_pipeline_version.h"
  "source/xresource_pipeline_tag.h"
  "source/xresource_pipeline_info.h"
  "source/xresource_pipeline_factory.h"
  "source/xresource_pipeline_descriptor_base.h"
  "source/xresource_pipeline_dependencies.h"
  "source/xresource_pipeline_config.h"
  "source/xresource_pipeline_compiler_base.h"
  "source/xresource_pipeline.h"
  "source/xresource_pipeline.cpp"
)