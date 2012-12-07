
add_executable(avplayer
	avplayer/avplayer.rc
	avplayer/main.cpp
	avcore/avplayer.cpp
	avcore/player_impl.cpp
	avcore/stdafx.cpp
)

target_link_libraries(avplayer av avsource avaudio avaudio avvideo)

if(bt)
	target_link_libraries(avplayer torrent-rasterbar)
endif()