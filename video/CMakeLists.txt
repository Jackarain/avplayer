add_library(avvideo STATIC sdl_render.cpp)

target_link_libraries(avvideo -lswscale)