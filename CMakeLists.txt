cmake_minimum_required(VERSION 3.10)
project(localsend_cxx)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DDEBUG)
endif()

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

add_subdirectory(src)
