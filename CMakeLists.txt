cmake_minimum_required(VERSION 2.8)
project(avplayer CXX)
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}")

include(pch.cmake)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

#SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_STATIC_RUNTIME    ON)

add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)

find_package( Boost 1.51 COMPONENTS date_time filesystem regex system)

#find_package( CryptoPP )

if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_libraries(${Boost_LIBRARIES})
    if(UNIX) # windows 下就算了
		link_libraries("-static-libgcc -static-libstdc++")
	endif()
endif()
# 
# if(CRYPTOPP_FOUND)
# 	include_directories(${CRYPTOPP_INCLUDE_DIRS})
# 	link_libraries(${CRYPTOPP_LIBRARIES})
# endif()
