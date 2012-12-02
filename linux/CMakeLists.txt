find_package(SDL)
find_package(X11)
include_directories(${Boost_INCLUDE_DIRS})

if(SDL_FOUND)
    include_directories(${SDL_INCLUDE_DIR})
    link_libraries(${SDL_LIBRARY})
else()
	message(FATAL_ERROR "need sdl" )
endif()

add_executable(avplayer player.cpp main.cpp
	$<TARGET_OBJECTS:libav> $<TARGET_OBJECTS:libavsource>
	$<TARGET_OBJECTS:libavvideo> $<TARGET_OBJECTS:libavaudio>)

if(bt)
	target_link_libraries(avplayer ${PROJECT_BINARY_DIR}/libtorrent/libtorrent-rasterbar.a )
endif()

target_link_libraries(avplayer -lavcodec -lavformat -lswscale -lswresample)

target_link_libraries(avplayer ${Boost_LIBRARIES} ${X11_LIBRARIES})
