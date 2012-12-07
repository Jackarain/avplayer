
add_library(av STATIC avplay.c)


target_link_libraries(av -lavcodec -lavformat -lswscale -lswresample)