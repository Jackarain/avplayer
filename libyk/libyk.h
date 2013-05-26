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
#include <map>

#include <boost/cstdint.hpp>
#include <boost/noncopyable.hpp>

namespace libyk
{
	// 质量分级.
	typedef enum {
		low_quality = 1,
		normal_quality = 2,
		high_quality = 3,
		mobile_quality = 96
	} video_quality;

	class youku_impl;
	// 优酷视频访问实现.
	class youku : public boost::noncopyable
	{
	public:
		youku();
		virtual ~youku();

	public:
		///打开视频链接, 开始顺序下载视频.
		// @prarm url 指定的视频url, 相对于youku网页上的视频url.
		// @prarm save_path 指定保存视频的位置, 默认为当前目录.
		// @prarm quality 指定的视频质量, 默认为普通质量.
		// 返回true表示打开成功, false表示失败.
		bool open(const std::string &url,
			std::string save_path = ".", video_quality quality = normal_quality);

		///停止下载.
		void stop();

		///等待下载完成.
		// 返回true, 表示下载完成, false表示下载未完成时就退出了.
		bool wait_for_complete();

	private:
		youku_impl *m_impl;		
	};
}

#endif // __LIBYK_H__

