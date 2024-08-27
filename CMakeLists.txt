cmake_minimum_required (VERSION 3.19)
project (deep-icet)

# Includes.
include (FetchContent)

# Project configuration.
set (CMAKE_CXX_STANDARD             20)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

# Target: common.
# Utilities shared between project executables.
add_library (common
	src/common.hpp
	src/common.cpp
	)
target_compile_options (common PUBLIC -Wall -Wextra -Wpedantic -Werror)

# basic setup for project executable targets.
function (deep_icet_exe NAME)
	add_executable ("${NAME}" "src/${NAME}.cpp")
	target_link_libraries ("${NAME}" common)
	endfunction ()

# Project executables.
deep_icet_exe (blend)
deep_icet_exe (compress)
deep_icet_exe (icet-blend)
deep_icet_exe (icet-compress)
deep_icet_exe (icet-decompress)
deep_icet_exe (icet-to-png)
deep_icet_exe (layer)

# Tracks dependencies to be downloaded.
set (FETCH_CONTENT_DEPS)

# Add dependency: IceT.
FetchContent_Declare (icet
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep/icet"
	)

list (APPEND FETCH_CONTENT_DEPS icet)
set (ICET_INSTALL_NO_RUNTIME     1                                   CACHE BOOL "")
set (ICET_INSTALL_NO_DEVELOPMENT 1                                   CACHE BOOL "")
set (ICET_LIBRARY_DIR            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" CACHE PATH "" FORCE)
set (ICET_EXECUTABLE_DIR         "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" CACHE PATH "" FORCE)

if (ICET_USE_PARICOMPRESS)
	enable_language (CUDA)

	# Dependency: PARI Compression.
	add_library (paricompress)

	set (paricompress_SOURCE_DIR        "${CMAKE_CURRENT_SOURCE_DIR}/dep/paricompress")
	set (PARICOMPRESS_INCLUDE_DIRECTORY "${paricompress_SOURCE_DIR}/include" CACHE PATH "" FORCE)
	set (PARICOMPRESS_LIBRARY           paricompress                         CACHE PATH "" FORCE)

	target_sources (paricompress PRIVATE "${paricompress_SOURCE_DIR}/src/paricompress.cu")
	target_include_directories (paricompress PUBLIC "${PARICOMPRESS_INCLUDE_DIRECTORY}")
	endif ()

# Dependency: MPI.
find_package (MPI REQUIRED COMPONENTS C CXX)

# Dependency: PNG.
find_package (PNG REQUIRED)

# Add dependency: png++.
FetchContent_Declare(png++
	URL https://download.savannah.nongnu.org/releases/pngpp/png++-0.2.9.tar.gz
	)
list (APPEND FETCH_CONTENT_DEPS png++)

# Get dependencies.
FetchContent_MakeAvailable ("${FETCH_CONTENT_DEPS}")

# Use dependency: IceT.
target_include_directories (IceTCore PUBLIC
	"${icet_SOURCE_DIR}/src/include"
	"${icet_BINARY_DIR}/src/include"
	)
target_link_libraries (IceTMPI
	MPI::MPI_C
	MPI::MPI_CXX
	)
target_link_libraries (common PUBLIC
	IceTCore
	IceTMPI
	)

# Use dependency: png++.
add_library (png++ INTERFACE IMPORTED)
target_include_directories (png++ INTERFACE "${png++_SOURCE_DIR}")
target_link_libraries (png++ INTERFACE PNG::PNG)

target_link_libraries (common PUBLIC png++)
