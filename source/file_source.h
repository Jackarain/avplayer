//
// file_source.h
// ~~~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __FILE_SOURCE_H__
#define __FILE_SOURCE_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include <string>
#include <boost/filesystem/fstream.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

#include "av_source.h"


struct open_file_data
{
   // 是否是多线程访问.
   bool is_multithread;

   // 打开文件名.
   std::string filename;
};

class file_source
   : public av_source
{
public:
   file_source();
   virtual ~file_source();

public:
   // 打开.
   virtual bool open(boost::any ctx);

   // 读取数据.
   virtual bool read_data(char* data, size_t size, size_t &read_size);

   // seek操作.
   virtual int64_t read_seek(uint64_t offset, int whence);

   // 关闭.
   virtual void close();

private:
   // 文件打开结构.
   open_file_data *m_open_data;

   // 文件指针.
   boost::filesystem::fstream m_file;

   // 线程安全锁.
   mutable boost::mutex m_mutex;
};

#endif // __FILE_SOURCE_H__

