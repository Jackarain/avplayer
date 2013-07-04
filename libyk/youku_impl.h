#ifndef __YOUKU_IMPL_H__
#define __YOUKU_IMPL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "avhttp.hpp"

#include <boost/shared_ptr.hpp>
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

	// 读取当前下载的index对应视频的数据.
	bool read_data(char* data, std::size_t size, std::size_t &read_size);

	// seek文件位置.
	boost::int64_t read_seek(boost::uint64_t offset, int whence);

	// 停止下载.
	void stop();

	///等待下载完成.
	// 返回true, 表示下载完成, false表示下载未完成时就退出了.
	bool wait_for_complete();

	// 切换下载的视频文件.
	bool change_download(int index);

	// 时长信息.
	duration_info current_duration_info();

private:

	void io_service_thread();
	void async_request_youku();
	void handle_check_download(const boost::system::error_code &ec);
	void handle_change_download(boost::condition &cond, int index);
	template <typename MutableBufferSequence>
	void handle_read_data(boost::condition &cond,
		const MutableBufferSequence &buffers, std::size_t &read_size);
	void handle_read_seek(boost::condition &cond,
		boost::uint64_t &offset, int whence);
	video_type query_quality();

private:
	boost::asio::io_service m_io_service;
	boost::thread m_work_thread;
	video_group m_video_group;
	avhttp::http_stream m_http_stream;
	boost::shared_ptr<avhttp::multi_download> m_multi_http;
	boost::asio::deadline_timer m_timer;
	std::string m_url;
	video_quality m_quality;
	int m_current_index;
	boost::int64_t m_offset;
	std::vector<video_clip> *m_video_info;
	boost::mutex m_mutex;
	bool m_abort;
};

}

#endif // __YOUKU_IMPL_H__

