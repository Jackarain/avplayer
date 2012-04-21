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

#include "av_source.h"

struct open_file_data 
{
   // 是否是多线程访问, 如果单线程访问
   // , 则内部自动使用无锁设计.
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
   virtual bool open(void* ctx);

   // 读取数据.
   virtual bool read_data(char* data, uint64_t offset, uint64_t size, uint64_t& read_size);

   // 关闭.
   virtual void close();

private:
   // 循环缓冲操作函数.
   unsigned int put_data(char* buffer, unsigned int len);
   unsigned int get_data(char* buffer, unsigned int len);
   inline unsigned int available_size() { return m_write_p - m_read_p; }
   inline unsigned int _max(unsigned int a, unsigned int b);
   inline unsigned int _min(unsigned int a, unsigned int b);

private:
   // 文件打开结构.
   open_file_data *m_open_data;

   // 文件指针.
   FILE* m_file;

   // 文件大小.
   uint64_t m_file_size;

   // 循环缓冲位置.
   unsigned int m_offset;

   // 循环缓冲区.
   char* m_circle_buffer;

   // 缓冲大小.
   unsigned int m_buffer_size;

   // 写指针下标.
   unsigned int m_write_p;

   // 读指针下标.
   unsigned int m_read_p;

   // 线程安全锁.
	mutable pthread_mutex_t m_mutex;
};

#endif // __FILE_SOURCE_H__

