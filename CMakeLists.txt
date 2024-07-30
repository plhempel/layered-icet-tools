cmake_minimum_required (VERSION 3.19)
project (deep-icet)

# Includes.
include (FetchContent)

# Project configuration.
set (CMAKE_CXX_STANDARD             "20")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Source generation.
configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.hpp.in"
	"${CMAKE_CURRENT_BINARY_DIR}/src/buildinfo.hpp"
	)

# Target: blend.
add_executable (blend
	"src/blend.cpp"
	"src/common.hpp"
	)
target_compile_options (blend PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Target: deep-icet.
add_executable (deep-icet
	"src/buildinfo.hpp.in"
	"src/common.hpp"
	"src/deep-icet.cpp"
	)
target_include_directories (deep-icet PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")

# Tracks dependencies to be downloaded.
set (FETCH_CONTENT_DEPS)

# Dependency: glm.
find_package (glm QUIET)
if (NOT glm_FOUND)
	FetchContent_Declare (glm
		GIT_REPOSITORY "https://github.com/g-truc/glm/"
		GIT_TAG        "45008b2"
		)
	list (APPEND FETCH_CONTENT_DEPS glm)
	endif ()
target_link_libraries (deep-icet glm::glm)

# Add dependency: IceT.
FetchContent_Declare (icet
	SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/dep/icet"
	)

list (APPEND FETCH_CONTENT_DEPS icet)
set (ICET_INSTALL_NO_RUNTIME     "1" CACHE BOOL "")
set (ICET_INSTALL_NO_DEVELOPMENT "1" CACHE BOOL "")

if (ICET_USE_PARICOMPRESS)
	enable_language (CUDA)

	# Dependency: PARI Compression.
	add_library (paricompress)

	set (paricompress_SOURCE_DIR        "${CMAKE_CURRENT_LIST_DIR}/dep/paricompress")
	set (PARICOMPRESS_INCLUDE_DIRECTORY "${paricompress_SOURCE_DIR}/include" CACHE PATH "" FORCE)
	set (PARICOMPRESS_LIBRARY           "paricompress"                       CACHE PATH "" FORCE)

	target_sources (paricompress PRIVATE "${paricompress_SOURCE_DIR}/src/paricompress.cu")
	target_include_directories (paricompress PUBLIC "${PARICOMPRESS_INCLUDE_DIRECTORY}")
	endif ()

# Dependency: MPI.
find_package (MPI REQUIRED COMPONENTS C CXX)
target_link_libraries (deep-icet
	MPI::MPI_C
	MPI::MPI_CXX
	)

# Dependency: OpenGL.
set (GL_COMPONENTS OpenGL)
target_link_libraries (deep-icet OpenGL::OpenGL)

# Dependency: PNG.
find_package (PNG REQUIRED)

# Add dependency: png++.
FetchContent_Declare(png++
	URL "https://download.savannah.nongnu.org/releases/pngpp/png++-0.2.9.tar.gz"
	)
list (APPEND FETCH_CONTENT_DEPS "png++")

# Option: Platform graphics interface.
set (PLATFORM_INTERFACES        "egl, glfw")
set (DEFAULT_PLATFORM_INTERFACE "egl")

find_package (glfw3 QUIET)
if (glfw3_FOUND)
	set (DEFAULT_PLATFORM_INTERFACE "glfw")
	endif ()

set (ICETDEMO_PLATFORM_INTERFACE "${DEFAULT_PLATFORM_INTERFACE}" CACHE STRING
	"The API/library through which IceT Demo interacts with the native platform. Must be one of ${PLATFORM_INTERFACES}.")

if (ICETDEMO_PLATFORM_INTERFACE STREQUAL "egl")
	target_compile_definitions (deep-icet PRIVATE ICETDEMO_PLATFORM_INTERFACE=0)

	# Dependency: EGL.
	list (APPEND GL_COMPONENTS EGL)
	target_link_libraries (deep-icet OpenGL::EGL)

	# Add dependency: EGL GLEW.
	FetchContent_Declare (glew
		GIT_REPOSITORY "https://github.com/Perlmint/glew-cmake"
		GIT_TAG        "a5494db"
		SOURCE_SUBDIR  "build/cmake"
		)
	list (APPEND FETCH_CONTENT_DEPS "glew")
	set (BUILD_UTILS "0" CACHE BOOL "")
	set (GLEW_X11    "0" CACHE BOOL "")
	set (GLEW_EGL    "1" CACHE BOOL "")
else ()
	# Dependency: System GLEW.
	find_package (GLEW REQUIRED)
	target_link_libraries (deep-icet GLEW::GLEW)

	if (ICETDEMO_PLATFORM_INTERFACE STREQUAL "glfw")
		target_compile_definitions (deep-icet PRIVATE ICETDEMO_PLATFORM_INTERFACE=1)

		# Dependency: glfw.
		target_link_libraries (deep-icet glfw)
	else ()
		message (FATAL_ERROR "Unknown platform interface ${ICETDEMO_PLATFORM_INTERFACE}, must be one of ${PLATFORM_INTERFACES}.")
		endif ()
	endif ()

# Get dependencies.
FetchContent_MakeAvailable ("${FETCH_CONTENT_DEPS}")
find_package (OpenGL REQUIRED "${GL_COMPONENTS}")

# Use dependency: EGL GLEW.
if ("glew" IN_LIST FETCH_CONTENT_DEPS)
	target_include_directories (deep-icet PRIVATE "${glew_SOURCE_DIR}/include")
	target_compile_definitions (deep-icet PRIVATE "GLEW_NO_GLU")
	target_link_libraries (deep-icet glew)
	endif ()

# Use dependency: IceT.
target_include_directories (deep-icet PRIVATE
	"${icet_SOURCE_DIR}/src/include"
	"${icet_BINARY_DIR}/src/include"
	)
target_link_libraries (deep-icet
	IceTCore
	IceTGL
	IceTGL3
	IceTMPI
	)

# Use dependency: png++.
add_library (png++ INTERFACE IMPORTED)
target_include_directories (png++ INTERFACE "${png++_SOURCE_DIR}")
target_link_libraries (png++ INTERFACE PNG::PNG)

target_link_libraries (blend png++)
target_link_libraries (deep-icet png++)
