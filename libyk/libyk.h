//
// yk_source.h
// ~~~~~~~~~~~
//
// Copyright (c) 2013 InvXp (invidentxp@gmail.com)
//

#ifndef __LIBYK_H__
#define __LIBYK_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <vector>
#include <string>
#include <boost/noncopyable.hpp>

namespace libyk
{
	class youku_impl;
	// ÓÅ¿áÊÓÆµ·ÃÎÊÊµÏÖ.
	class youku : public boost::noncopyable
	{
	public:
		youku();
		virtual ~youku();

	public:
		// ½âÎöÓÅ¿áÊÓÆµurl.
		bool parse_url(const std::string &url);
		// ½âÎöurlÖÐµÄÊÓÆµÎÄ¼þ.
		bool parse_video_files(std::vector<std::string> &videos, const std::string &password = "");

	private:
// 		bool parse_json(const std::string &data, boost::property_tree::wptree &root);
// 		std::string location(const std::string &url);

	private:
		youku_impl *m_impl;		
	};
}

#endif // __LIBYK_H__

