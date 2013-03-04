#ifndef __LIBYK_H__
#define __LIBYK_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>
#include <string>

#include "avhttp.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/noncopyable.hpp>


namespace libyk
{
	// 优酷视频访问实现.
	class libykvideo : public boost::noncopyable
	{
	public:
		libykvideo();
		~libykvideo();

	public:
		// 解析优酷视频url.
		bool parse_url(const std::string &url);
		// 解析url中的视频文件.
		bool parse_video_files(std::vector<std::string> &videos, const std::string &password = "");

	private:
		bool parse_json(const std::string &data, boost::property_tree::wptree &root);
		std::string location(const std::string &url);

	private:
		std::string m_vid;
		boost::asio::io_service m_io_service;
		avhttp::http_stream m_http_stream;
	};
}

#endif // __LIBYK_H__

