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
#include "globals.h"

struct unkown_demux_data
{
	std::string file_name;			// 文件名.
	int type;						// 数据类型.
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

	// 读取暂停, 主要为RTSP这种网络媒体协议.
	virtual int read_pause();

	// 同上, 恢复播放.
	virtual int read_play();

	// 关闭.
	virtual void close();

	// 是否中止.
	inline bool is_abort() { return m_abort; }

	// 获得视频的基本信息.
	media_base_info base_info();

protected:

	// 中止解码回调.
	static int decode_interrupt_cb(void *ctx);

	// 从source中读取数据.
	static int read_data(void *opaque, uint8_t *buf, int buf_size);

	// 向source中写入数据.
	static int write_data(void *opaque, uint8_t *buf, int buf_size);

	// 在source中进行seek.
	static int64_t seek_data(void *opaque, int64_t offset, int whence);

	// 查询指定的index.
	int query_index(enum AVMediaType type, AVFormatContext *ctx);

	// 复制AVRational.
	inline void avrational_copy(AVRational &src, AVRational &dst)
	{
		dst.den = src.den;
		dst.num = src.num;
	}


protected:
	// 参数信息.
	unkown_demux_data m_unkown_demux_data;

	// 使用FFmpeg的AVFormatContext来读取AVPacket.
	AVFormatContext *m_format_ctx;

	// 数据IO上下文指针.
	AVIOContext *m_avio_ctx;

	// source_context指针.
	source_context *m_source_ctx;

	// 视频的基本信息.
	media_base_info m_base_info;

	// 数据缓冲.
	unsigned char *m_io_buffer;

	// 是否中止.
	bool m_abort;
};

#endif // __UNKOWN_DEMUX_H__
