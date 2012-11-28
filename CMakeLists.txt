cmake_minimum_required(VERSION 2.8)
project(avplayer)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

#SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_STATIC_RUNTIME    ON)

add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)

find_package( Boost 1.51 COMPONENTS date_time filesystem regex system)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(libav/)
include_directories(source/)
include_directories(audio/)

add_subdirectory(libav)
add_subdirectory(video)
add_subdirectory(audio)
add_subdirectory(source)

if(UNIX)
	add_subdirectory(linux)
endif()

if(WIN32)
	add_subdirectory(win32)
endif()
