//
// yk_source.h
// ~~~~~~~~~~~~~~~~
//
// Copyright (c) 2013 InvXp (invidentxp@gmail.com)
//

#ifndef __YK_SOURCE_H__
#define __YK_SOURCE_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <boost/thread/mutex.hpp>

#include <stdint.h>
#include "av_source.h"

#include <string>
#include <vector>

#include "libyk.h"

using namespace libyk;
// yk中的视频信息.
struct yk_video_file_info 
{
    int index;              // id.
    std::string title;      // 视频文件名.
    std::string source;     // 视频源地址
    uint64_t data_size;    // 视频大小.
    uint64_t base_offset;  // 视频在yk中的偏移.
    int status;             // 当前播放状态.
};

class yk_source
    : public av_source
{
public:
    yk_source();
    virtual ~yk_source();

public:
    // 打开.
    virtual bool open(void* ctx);

    // 读取数据.
    virtual bool read_data(char* data, uint64_t offset, size_t size, size_t& read_size);

    // seek操作, 此处返回true, 表示数据不够, 需要缓冲.
    virtual bool read_seek(uint64_t offset, int whence);

    // 关闭.
    virtual void close();

    // 设置或获得当前播放的视频文件.
    virtual bool set_current_video(int index);
    virtual bool get_current_video(yk_video_file_info& vfi) const;

    // 当前视频列表.
    virtual std::vector<yk_video_file_info> video_list() const { return m_videos; }

    // 重置读取数据.
    virtual void reset();
    
public:
    libykvideo                      m_yk_video;
    std::vector<yk_video_file_info> m_videos;
 
private:

    // 当前播放的视频.
    yk_video_file_info m_current_video;

    // yk中所有视频信息.

    // 用于控制重置读取数据.
    bool m_reset;

    // 关闭控制.
    bool m_abort;
    boost::mutex m_abort_mutex;
};

#endif // __YK_SOURCE_H__

