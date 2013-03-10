cmake_minimum_required(VERSION 2.8)
project(avplayer)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_STATIC_RUNTIME    ON)

find_package( Boost 1.50 REQUIRED COMPONENTS thread filesystem)

add_definitions(-D__STDC_CONSTANT_MACROS)

#dont't define this, libbt need depreated api
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR})
include_directories(libav/)
include_directories(source/)
include_directories(audio/)

option(bt "support bt protocol" ON)
option(yk "support youku video" ON)

add_subdirectory(libav)
add_subdirectory(video)
add_subdirectory(audio)
add_subdirectory(source)

if(bt)
	add_subdirectory(libtorrent)
endif()

if(yk)
	add_subdirectory(libyk)
endif()

if(UNIX)
	add_subdirectory(linux)
endif()

if(WIN32)
	add_subdirectory(win32)
endif()
