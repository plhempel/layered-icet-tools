cmake_minimum_required (VERSION 3.19)
project (layered-icet)

# Includes.
include (FetchContent)

# Project configuration.
set (CMAKE_CXX_STANDARD             20)
set (CMAKE_CXX_STANDARD_REQUIRED    TRUE)
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

# Basic setup for project executable targets.
function (layered_icet_exe NAME)
	add_executable ("${NAME}" "src/${NAME}.cpp")
	target_include_directories ("${NAME}" PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/src")
	target_link_libraries ("${NAME}" common)
	endfunction ()

# Generate a lookup table for compositing strategy names using perfect hashing.
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

# Project executables.
layered_icet_exe (blend)
layered_icet_exe (compress)
layered_icet_exe (icet-blend-png)
layered_icet_exe (icet-blend-raw)
layered_icet_exe (icet-compress)
layered_icet_exe (icet-decompress)
layered_icet_exe (icet-to-png)
layered_icet_exe (layer)
layered_icet_exe (merge)

strategy_lookup (strategy-hash              StrategyTable)
strategy_lookup (single-image-strategy-hash SingleImageStrategyTable)

# Tracks dependencies to be downloaded.
set (FETCH_CONTENT_DEPS)

# Add dependency: IceT.
FetchContent_Declare (icet
	SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep/icet"
	)

list (APPEND FETCH_CONTENT_DEPS icet)
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
