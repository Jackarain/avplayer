//
// yk_source.h
// ~~~~~~~~~~~
//
// Copyright (c) 2013 invwin7
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

	typedef enum { init_state, start_state, stop_state, complete_state } video_status;
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

	// 视频信息.
	typedef std::map<video_type, video_info> video_group;
	// 视频时长信息, key为视频index, value为视频时长, 单位秒.
	typedef std::map<int, int> duration_info;

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

		// 读取当前下载的index对应视频的数据.
		bool read_data(char* data, std::size_t size, std::size_t &read_size);

		// seek文件位置.
		boost::int64_t read_seek(boost::uint64_t offset, int whence);

		///停止下载.
		void stop();

		///等待下载完成.
		// 返回true, 表示下载完成, false表示下载未完成时就退出了.
		bool wait_for_complete();

		// 切换下载的视频文件.
		bool change_download(int index);

		// 当前视频的时长信息, 每个key对应一个视频文件的时长.
		duration_info current_duration_info();

	private:
		youku_impl *m_impl;		
	};
}

#endif // __LIBYK_H__
