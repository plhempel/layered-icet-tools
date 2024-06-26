cmake_minimum_required (VERSION 3.16)
project (icet-demo)

# Includes.
include (ExternalProject)
include (FetchContent)

# Project configuration.
set (CMAKE_CXX_STANDARD             "20")
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_property (DIRECTORY PROPERTY EP_PREFIX "${FETCHCONTENT_BASE_DIR}")

# Source generation.
configure_file (
	"${CMAKE_CURRENT_SOURCE_DIR}/src/buildinfo.hpp.in"
	"${CMAKE_CURRENT_BINARY_DIR}/src/buildinfo.hpp"
	)

# Target: icet-demo.
add_executable (icet-demo "src/main.cpp")
target_include_directories (icet-demo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")

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
target_link_libraries (icet-demo glm::glm)

# Option: Local IceT.
set (ICETDEMO_LOCAL_ICET "" CACHE PATH
		"Path to the source directory of IceT. If set to a false value, IceT will be downloaded from the Kitware GitLab repository.")

# Add dependency: IceT.
if (ICETDEMO_LOCAL_ICET)
	if (NOT EXISTS "${ICETDEMO_LOCAL_ICET}/CMakeLists.txt")
		message (FATAL_ERROR "The configured IceT source directory at ${ICETDEMO_LOCAL_ICET} does not contain a CMake project!")
		endif ()

	FetchContent_Declare (icet
		SOURCE_DIR "${ICETDEMO_LOCAL_ICET}"
		)
else ()
	FetchContent_Declare (icet
		GIT_REPOSITORY "https://gitlab.kitware.com/icet/icet"
		GIT_TAG        "365e3695"
		)
	endif ()

list (APPEND FETCH_CONTENT_DEPS icet)
set (BUILD_TESTING "0" CACHE BOOL "")

if (ICET_USE_PARICOMPRESS)
	# Option: Local PARI Compression.
	set (ICETDEMO_LOCAL_PARICOMP "" CACHE PATH
			"Path to the source directory of PARI Compression. If set to a false value, PARI will be downloaded from the GitHub repository.")

	if (ICETDEMO_LOCAL_PARICOMP)
		if (NOT EXISTS "${ICETDEMO_LOCAL_PARICOMP}/Makefile")
			message (FATAL_ERROR "The configured PARI Compression source directory at ${ICETDEMO_LOCAL_PARICOMP} does not contain a Makefile!")
			endif ()

		set (PARICOMP_DOWNLOAD_ARGS "SOURCE_DIR" "${ICETDEMO_LOCAL_PARICOMP}")
	else ()
		set (PARICOMP_DOWNLOAD_ARGS
				GIT_REPOSITORY    "https://github.com/tmarrinan/pari-compression"
				GIT_TAG           "a5ee535"
				)
		endif()

	# Dependency: PARI Compression.
	ExternalProject_Add (paricomp
		"${PARICOMP_DOWNLOAD_ARGS}"
		CONFIGURE_COMMAND ""
		BUILD_IN_SOURCE   1
		)
	ExternalProject_Get_Property (paricomp SOURCE_DIR)
	set (paricomp_SOURCE_DIR "${SOURCE_DIR}")

	set (PARICOMPRESS_INCLUDE_DIRECTORY "${paricomp_SOURCE_DIR}/include")
	set (PARICOMPRESS_LIBRARY           "${paricomp_SOURCE_DIR}/lib/libparicompress.a")
	endif ()

# Dependency: MPI.
find_package (MPI REQUIRED COMPONENTS C CXX)
target_link_libraries (icet-demo
	MPI::MPI_C
	MPI::MPI_CXX
	)

# Dependency: OpenGL.
set (GL_COMPONENTS OpenGL)
target_link_libraries (icet-demo OpenGL::OpenGL)

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
	target_compile_definitions (icet-demo PRIVATE ICETDEMO_PLATFORM_INTERFACE=0)

	# Dependency: EGL.
	list (APPEND GL_COMPONENTS EGL)
	target_link_libraries (icet-demo OpenGL::EGL)

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

	# Dependency: PNG.
	find_package (PNG REQUIRED)
	target_link_libraries (icet-demo PNG::PNG)

	# Add dependency: png++.
	FetchContent_Declare(png++
		URL "https://download.savannah.nongnu.org/releases/pngpp/png++-0.2.9.tar.gz"
		)
	list (APPEND FETCH_CONTENT_DEPS "png++")
else ()
	# Dependency: System GLEW.
	find_package (GLEW REQUIRED)
	target_link_libraries (icet-demo GLEW::GLEW)

	if (ICETDEMO_PLATFORM_INTERFACE STREQUAL "glfw")
		target_compile_definitions (icet-demo PRIVATE ICETDEMO_PLATFORM_INTERFACE=1)

		# Dependency: glfw.
		target_link_libraries (icet-demo glfw)
	else ()
		message (FATAL_ERROR "Unknown platform interface ${ICETDEMO_PLATFORM_INTERFACE}, must be one of ${PLATFORM_INTERFACES}.")
		endif ()
	endif ()

# Get dependencies.
FetchContent_MakeAvailable ("${FETCH_CONTENT_DEPS}")
find_package (OpenGL REQUIRED "${GL_COMPONENTS}")

# Use dependency: EGL GLEW.
if ("glew" IN_LIST FETCH_CONTENT_DEPS)
	target_include_directories (icet-demo PRIVATE "${glew_SOURCE_DIR}/include")
	target_link_libraries (icet-demo glew)
	endif ()

# Use dependency: IceT.
if (ICET_USE_PARICOMPRESS)
	add_dependencies (IceTCore paricomp)
	endif ()

target_include_directories (icet-demo PRIVATE
	"${icet_SOURCE_DIR}/src/include"
	"${icet_BINARY_DIR}/src/include"
	)
target_link_libraries (icet-demo
	IceTCore
	IceTGL
	IceTGL3
	IceTMPI
	)

# Use dependency: png++.
if ("png++" IN_LIST FETCH_CONTENT_DEPS)
	add_library (png++ INTERFACE IMPORTED)
	target_link_libraries (icet-demo png++)
	target_include_directories (png++ INTERFACE "${png++_SOURCE_DIR}")
	endif ()

# Configure installation.
install(TARGETS icet-demo)
