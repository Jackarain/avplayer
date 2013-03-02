#ifndef __LIBYK_H__
#define __LIBYK_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>


namespace libyk
{
	class libykvideo
	{
	public:
		libykvideo();
		~libykvideo();

	public:
		int parse_url(const std::string &url);
		int parse_video_files(std::vector<std::string>& videos, const std::string& password = "");
	private:
		bool parse_json(const std::string &data, boost::property_tree::wptree &root);
		std::string location(const std::string &url);

	private:
		std::string vid_;
	};
}

#endif // __LIBYK_H__

