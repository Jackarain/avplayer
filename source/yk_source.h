#pragma once

#include <iostream>
#include "ins.h"

#include "av_source.h"
#include "libyk.h"


// yk中的视频信息.
struct open_yk_data 
{
	std::string url;		// 播放youku的url.
	int type;				// 当前请求播放的类型, 有hd2,mp4,3gp,3gphd,flv,m3u8.
	std::string save_path;	// 下载的youku视频保存位置.
};

class yk_source
	: public av_source
{
public:
	yk_source(void);
	virtual ~yk_source(void);

public:
	// 打开.
	virtual bool open(void* ctx);

	// 读取数据.
	virtual bool read_data(char* data, size_t size, size_t& read_size);

	// seek操作, 此处返回true, 表示数据不够, 需要缓冲.
	virtual int64_t read_seek(uint64_t offset, int whence);

	// 关闭.
	virtual void close();

	// 设置或获得当前播放的视频文件.
	virtual bool set_current_video(int index);
	virtual bool get_current_video(open_yk_data& vfi) const;

	// 当前视频列表.
	virtual std::vector<open_yk_data> video_list() const;

	// 重置读取数据.
	virtual void reset();

	// 解析url.
	bool parse_url(const std::string &url);


private:
	libyk::youku m_yk_video;
	bool m_abort;
	bool m_reset;
};
