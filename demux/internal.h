//
// internal.h
// ~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4244)
#endif // _MS_VER

// 内部使用的头文件.
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

#include <string>
#include <boost/any.hpp>

#ifdef _MSC_VER
# pragma warning(pop)
#endif // _MS_VER

#endif // __INTERNAL_H__
