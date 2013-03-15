//
// youku_demux.h
// ~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack.wgm@gmail.com)
//

#ifndef __YOUKU_DEMUX_H__
#define __YOUKU_DEMUX_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "demuxer.h"

/*
 * 用于解析youku的视频, 源包括hd2, mp4, 3gp, 3gphd, flv, m3u8.
 * 设计目标:
 * 客户可以得到一个youku链接中的源的数目和类型.
 * 客户可以任选某个格式做为播放源.
 */

class youku_demux : public demuxer
{
public:
	youku_demux(void);
	virtual ~youku_demux(void);
};

#endif // __YOUKU_DEMUX_H__
