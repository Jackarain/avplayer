if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    link_libraries(${Boost_LIBRARIES})
else()
	message(FATAL_ERROR "need boost" )
endif()

find_package(SDL)
find_package(OpenGL)

include_directories(${OPENGL_INCLUDE_DIR})
link_libraries(${OPENGL_LIBRARIES})

if(SDL_FOUND)
    include_directories(${SDL_INCLUDE_DIR})
    link_libraries(${SDL_LIBRARY})
else()
	message(FATAL_ERROR "need sdl" )
endif()

add_executable(avplayer player.cpp main.cpp
	$<TARGET_OBJECTS:libav> $<TARGET_OBJECTS:libavsource>
	$<TARGET_OBJECTS:libavvideo> $<TARGET_OBJECTS:libavaudio>)

target_link_libraries(avplayer -lavcodec -lavformat -lswscale)