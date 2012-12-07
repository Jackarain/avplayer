add_library(avsource STATIC file_source.cpp source.cpp torrent_source.cpp)

if(bt)
	add_definitions(-DUSE_TORRENT -DANSI_TERMINAL_COLORS -DBOOST_ASIO_DYN_LINK)
endif()

include_directories(${PROJECT_SOURCE_DIR}/libtorrent/include)
