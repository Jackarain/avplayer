add_library(libavsource OBJECT file_source.cpp source.cpp torrent_source.cpp)

add_definitions(-DUSE_TORRENT -DANSI_TERMINAL_COLORS -DBOOST_ASIO_DYN_LINK)

include_directories(${PROJECT_SOURCE_DIR}/libtorrent/include)
