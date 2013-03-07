//
// unkown_demux.h
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __UNKOWN_DEMUX_H__
#define __UNKOWN_DEMUX_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "demuxer.h"

struct unkown_demux_data
{
	std::string file_name;
};

class unkown_demux : public demuxer
{
public:
	unkown_demux(void);
	virtual ~unkown_demux(void);

public:
	// 打开demuxer, 参数为any, 以传入任意参数.
	virtual bool open(boost::any ctx);

	// 读取一个packet到pkt中.
	// 返回true表示成功.
	virtual bool read_packet(AVPacket *pkt);

	// seek_packet 是用于seek到指定的timestamp位置.
	// timestamp 时间戳.
	virtual bool seek_packet(int64_t timestamp);

	// stream_index 得到指定AVMediaType类型的index.
	// index 是返回的指定AVMediaType类型的index.
	// 返回true表示成功.
	virtual bool stream_index(enum AVMediaType type, int &index);

	// query_avcodec_id 查询指定index的codec的id值.
	// 指定的index.
	// 指定的index的codec_id.
	// 成功返回true.
	virtual bool query_avcodec_id(int index, enum AVCodecID &codec_id);

	// 关闭.
	virtual void close();

};

#endif // __UNKOWN_DEMUX_H__
