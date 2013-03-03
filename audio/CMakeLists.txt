find_package( Boost 1.50 COMPONENTS thread)

add_library(avaudio STATIC sdl_render.cpp)
target_link_libraries(avaudio ${Boost_LIBRARIES})
