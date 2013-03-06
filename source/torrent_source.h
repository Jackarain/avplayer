//
// torrent_source.h
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __TORRENT_SOURCE_H__
#define __TORRENT_SOURCE_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "av_source.h"

#include <iterator>
#include <exception>
#include <string>

#include <boost/progress.hpp>
#include <boost/tokenizer.hpp>

#include "libtorrent/utf8.hpp"
#include "libtorrent/entry.hpp"
#include "libtorrent/bencode.hpp"
#include "libtorrent/session.hpp"
#include "libtorrent/storage.hpp"
#include "libtorrent/interface.hpp"

using namespace libtorrent;
#if BOOST_VERSION < 103400
namespace fs = boost::filesystem;
fs::path::default_name_check(fs::no_check);
#endif

// 打开torrent时种子数据.
struct open_torrent_data 
{
	bool is_file;							// 是否是种子文件.
	std::string filename;					// 种子文件名.
	boost::shared_ptr<char> torrent_data;	// 种子数据.
	int data_size;							// 种子数据大小.
	std::string save_path;					// 保存路径.
};

// torrent中的视频信息.
struct video_file_info 
{
	video_file_info()
		: offset(-1)	// m_offset标识为-1, 当初始化时应该修正为base_offset.
	{}

	int index;						// id.
	std::string filename;			// 视频文件名.
	boost::uint64_t data_size;		// 视频大小.
	boost::uint64_t base_offset;	// 视频在torrent中的偏移.
	uint64_t offset;				// 数据访问的偏移, 相对于base_offset.
	int status;						// 当前播放状态.
};

class torrent_source
	: public av_source
{
public:
	torrent_source();
	virtual ~torrent_source();

public:
	// 打开.
	virtual bool open(void* ctx);

	// 读取数据.
	virtual bool read_data(char* data, size_t size, size_t &read_size);

	// seek操作, 此处返回true, 表示数据不够, 需要缓冲.
	virtual int64_t read_seek(uint64_t offset, int whence);

	// 关闭.
	virtual void close();

	// 设置或获得当前播放的视频文件.
	virtual bool set_current_video(int index);
	virtual bool get_current_video(video_file_info& vfi) const;

	// 判断某个偏移位置是否数据已经下载.
	virtual bool has_data(uint64_t offset);

	// 当前视频列表.
	virtual std::vector<video_file_info> video_list() const { return m_videos; }

	// 重置读取数据.
	virtual void reset();

private:
	// session下载对象.
	session m_session;

	// torrent handle.
	torrent_handle m_torrent_handle;

	// 数据读取对象.
	boost::shared_ptr<extern_read_op> m_read_op;

	// 数据打开结构.
	boost::shared_ptr<open_torrent_data> m_open_data;

	// 当前播放的视频.
	video_file_info m_current_video;

	// torrent中所有视频信息.
	std::vector<video_file_info> m_videos;

	// 用于控制重置读取数据.
	bool m_reset;

	// 关闭控制.
	bool m_abort;
	boost::mutex m_abort_mutex;
};

#endif // __TORRENT_SOURCE_H__

