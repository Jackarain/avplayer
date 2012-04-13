//
// ins.h
// ~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __INS_H__
#define __INS_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include <set>
#include <map>
#include <list>
#include <algorithm>

#include <boost/any.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/pool/singleton_pool.hpp>

#define BOOST_FILESYSTEM_VERSION 2
#include <boost/filesystem.hpp>


// using namespace boost;

extern "C"
{
// #include "stdint.h"
// #include "libavutil/avutil.h"
// #include "libavutil/avstring.h"
// #include "libavutil/pixdesc.h"
// #include "libavutil/imgutils.h"
// #include "libavutil/parseutils.h"
// #include "libavutil/samplefmt.h"
// #include "libavformat/avformat.h"
#include "libswscale/swscale.h"
// #include "libavutil/mathematics.h"
// 
// #include "libavfilter/avfilter.h"
// #include "libavfilter/avfiltergraph.h"
#include "libavcodec/avcodec.h"
// #include "libavcodec/opt.h"
// #include "libavcodec/avfft.h"
// #include "libavcodec/audioconvert.h"

// #include "libpng15/png.h"
}

#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>
// #include <gl\glaux.h>

#include <ddraw.h>
#include <d3d9.h>

#include <MMSystem.h>
#include <dsound.h>

// #include "logger.h"
// extern libavplayer::logger logs;

#endif // __INS_H__
