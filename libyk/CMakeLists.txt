cmake_minimum_required(VERSION 2.8)

find_package( Boost 1.48 REQUIRED COMPONENTS signals date_time program_options filesystem regex chrono locale system thread)
find_package( CURL REQUIRED )
find_package( Threads )

add_library(libky STATIC youku_impl.cpp libyk.cpp)

include_directories(${Boost_INCLUDE_DIRS})
include_directories( ../third_party/avhttp/include )

target_link_libraries(libky ${Boost_LIBRARIES})
target_link_libraries( libky ${CMAKE_THREAD_LIBS_INIT})

# fix static build
if(UNIX) # windows 下就算了
	target_link_libraries(libky -lrt)
endif()