#define __STDC_CONSTANT_MACROS
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <SDL/SDL.h>
#include <X11/Xlib.h>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <avplay.h>
#include "player.h"

int main ( int argc, char *argv[] )
{
    // 判断播放参数是否足够.
    if ( argc != 2 )
    {
        printf ( "usage: avplayer <video>\n" );
        return -1;
    }

    player ply;

    XInitThreads();

    SDL_Init ( SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTTHREAD );


    // 判断打开的媒体类型,
    // 根据媒体文件类型选择不同的方式打开.
    std::string filename ( argv[1] );
    if ( fs::path ( filename ).extension() == ".torrent" )
    {
        if ( ply.open ( filename.c_str(), MEDIA_TYPE_BT ) < 0 )
            return -1;
    }
    else
    {
        std::string is_url = filename.substr ( 0, 7 );
        if ( is_url == "http://" )
        {
            if ( ply.open ( filename.c_str(), MEDIA_TYPE_HTTP ) < 0 )
                return -1;
        }
        else if ( is_url == "rtsp://" )
        {
            // if (!ply.open(filename.c_str(), MEDIA_TYPE_RTSP))
            return -1;
        }
        else
        {
            if ( ply.open ( filename.c_str(), MEDIA_TYPE_FILE ) < 0 )
                return -1;
        }
    }

    // 开始播放
    ply.play();

    SDL_WM_SetCaption ( ( std::string ( "正在播放" ) + filename ).c_str(),
                        "" );

    while ( true )
    {
        SDL_Event event;
        SDL_WaitEvent ( &event );

        if ( event.type == SDL_QUIT )
        {
            // ply.stop();
            break;
        }
        if ( event.type == SDL_KEYDOWN )
        {
            if ( event.key.keysym.sym == SDLK_RIGHT )
            {
                ply.fwd();
            }
        }
    }

    // ply.wait_for_completion();

    return 0;
}
