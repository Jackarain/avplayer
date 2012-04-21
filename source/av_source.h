//
// av_source.h
// ~~~~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
// 

#ifndef __AV_SOURCE_H__
#define __AV_SOURCE_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

class av_source
{
public:
   av_source() {}
   virtual ~av_source() {}

public:
   // 打开.
   virtual bool open(void *ctx) = 0;

   // 读取数据.
   virtual bool read_data(char *data, uint64_t offset, uint64_t size, uint64_t& read_size) = 0;

   // 关闭.
   virtual void close() = 0;
};


#endif // __AV_SOURCE_H__
