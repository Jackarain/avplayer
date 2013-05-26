#ifndef __YOUKU_IMPL_H__
#define __YOUKU_IMPL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "avhttp.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/noncopyable.hpp>

#include "libyk.h"

namespace libyk {

class youku_impl : public boost::noncopyable
{
public:
	youku_impl(void);
	virtual ~youku_impl(void);

public:
	// 打开优酷视频url.
	bool open(const std::string &url,
		std::string save_path = ".", video_quality quality = normal_quality);

	// 停止下载.
	void stop();

	///等待下载完成.
	// 返回true, 表示下载完成, false表示下载未完成时就退出了.
	bool wait_for_complete();

private:
	typedef enum { init_state, start_state, stop_state } video_status;
	typedef enum { hd2, mp4, gp3hd, flv, gp3, m3u8 } video_type;

	struct video_clip
	{
		video_clip()
			: duration(0)
			, filesize(0)
			, id(0)
			, state(init_state)
		{}

		std::string url;	// 片段url.
		int duration;		// 时长.
		int filesize;		// 文件大小.
		int id;				// 视频ID.
		video_status state;	// 当前状态.
	};

	struct video_info
	{
		video_info()
			: duration(0)
		{}

		float duration;				// 视频时长.
		std::vector<video_clip> fs;	// 视频片段文件.
	};

	typedef std::map<video_type, video_info> video_group;

private:

	void io_service_thread();
	void async_request_youku();
	void handle_check_download(const boost::system::error_code &ec);
	video_type query_quality();

private:
	boost::asio::io_service m_io_service;
	boost::thread m_work_thread;
	video_group m_video_group;
	avhttp::http_stream m_http_stream;
	avhttp::multi_download m_multi_http;
	boost::asio::deadline_timer m_timer;
	std::string m_url;
	video_quality m_quality;
	bool m_abort;
};

}

#endif // __YOUKU_IMPL_H__

