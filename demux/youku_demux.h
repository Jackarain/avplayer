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

class youku_demux : public demuxer
{
public:
	youku_demux(void);
	virtual ~youku_demux(void);
};

#endif // __YOUKU_DEMUX_H__
