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

extern "C"
{
#include "libswscale/swscale.h"
#include "libavcodec/avcodec.h"
}

#include <windows.h>
#include <gl\gl.h>
#include <gl\glu.h>

#include <ddraw.h>
#include <d3d9.h>

#include <MMSystem.h>
#include <dsound.h>

#endif // __INS_H__
