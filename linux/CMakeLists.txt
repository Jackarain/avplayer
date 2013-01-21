find_package(SDL  REQUIRED)
find_package(X11)
find_package( Boost 1.51 COMPONENTS filesystem)

include_directories(${SDL_INCLUDE_DIR})

add_executable(avplayer player.cpp main.cpp)

target_link_libraries(avplayer av avsource avaudio avaudio avvideo)

if(bt)
	target_link_libraries(avplayer torrent-rasterbar)
endif()

target_link_libraries(avplayer ${X11_LIBRARIES})
target_link_libraries(avplayer ${SDL_LIBRARY})
target_link_libraries(avplayer ${Boost_LIBRARIES})

INSTALL(TARGETS avplayer RUNTIME DESTINATION bin)
