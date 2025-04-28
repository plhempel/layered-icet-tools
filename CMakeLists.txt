cmake_minimum_required (VERSION 3.19)
project (layered-icet-tool)

# Includes.
include (FetchContent)
cmake_policy (SET CMP0135 NEW)

set (FETCHCONTENT_SOURCE_DIR_ICET "${CMAKE_CURRENT_SOURCE_DIR}/../icet" CACHE PATH
		"Directory containing layered IceT source code.")

# Project configuration.
set (CMAKE_CXX_STANDARD             20)
set (CMAKE_CXX_STANDARD_REQUIRED    TRUE)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")

# Target: common.
# Utilities shared between tools.
add_library (common
	src/common.hpp
	src/common.cpp
	)
target_compile_options (common PUBLIC -Wall -Wextra -Wpedantic -Werror)

# Basic setup for tool targets.
function (add_tool NAME)
	add_executable ("${NAME}" "src/${NAME}.cpp")
	target_include_directories ("${NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
	target_link_libraries ("${NAME}" common)
	endfunction ()

# Tools.
add_tool (benchmark)
add_tool (blend)
add_tool (compress)
add_tool (icet-blend-png)
add_tool (icet-blend-raw)
add_tool (icet-compress)
add_tool (icet-decompress)
add_tool (icet-to-png)
add_tool (layer)
add_tool (merge)

# Generate a lookup table for compositing strategy names using gperf.
function (strategy_lookup FILE CLASS)
	add_custom_command (
		OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/src/${FILE}.hpp"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/src"
		COMMAND gperf "${CMAKE_CURRENT_SOURCE_DIR}/res/${FILE}.gperf" -L C++ -N find -Z ${CLASS}
			          -S 1 -tlcCEID > "${CMAKE_CURRENT_BINARY_DIR}/src/${FILE}.hpp"
		DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/res/${FILE}.gperf"
		)
	target_sources (icet-blend-png PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src/${FILE}.hpp")
	target_sources (icet-blend-raw PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src/${FILE}.hpp")
	endfunction ()

strategy_lookup (strategy-hash              StrategyLut)
strategy_lookup (single-image-strategy-hash SingleImageStrategyLut)

# Tracks dependencies to be downloaded.
set (FETCH_CONTENT_DEPS)

# Add dependency: IceT.
FetchContent_Declare (IceT)
list (APPEND FETCH_CONTENT_DEPS IceT)

set (ICET_INSTALL_NO_RUNTIME     TRUE                                CACHE BOOL "")
set (ICET_INSTALL_NO_DEVELOPMENT TRUE                                CACHE BOOL "")
set (ICET_LIBRARY_DIR            "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" CACHE PATH "" FORCE)
set (ICET_EXECUTABLE_DIR         "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" CACHE PATH "" FORCE)

# Dependency: MPI.
find_package (MPI REQUIRED COMPONENTS C CXX)

# Dependency: PNG.
find_package (PNG REQUIRED)

# Add dependency: png++.
FetchContent_Declare (png++
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
