//
// demux.h
// ~~~~~~~
//
// Copyright (c) 2013 Jack (jack.wgm@gmail.com)
//

#ifndef __DEMUX_H__
#define __DEMUX_H__

#ifdef _MSC_VER
#ifdef DEMUX_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

#ifdef  __cplusplus
extern "C" {
#endif

// 除非特定的视频数据, 统一使用以下函数来demux, 实际上就是使用ffmpeg来实现demux.
EXPORT_API int generic_init_demux(struct demux_context *demux_ctx);
EXPORT_API int generic_read_packet(struct demux_context *demux_ctx, AVPacket *pkt);
EXPORT_API int generic_packet_seek(struct demux_context *demux_ctx, int64_t timestamp);
EXPORT_API int generic_read_pause(struct demux_context *demux_ctx);
EXPORT_API int generic_read_play(struct demux_context *demux_ctx);
EXPORT_API int generic_stream_index(struct demux_context *demux_ctx, enum AVMediaType type);
EXPORT_API enum AVCodecID generic_query_avcodec_id(struct demux_context *demux_ctx, int index);
EXPORT_API void generic_destory(struct demux_context *demux_ctx);

// 优酷视频demux, 由于youku视频是分段的flv, 所以必须处理各段的数据, 以保证看起来像是同一个文件.
EXPORT_API int youku_init_demux(struct demux_context *demux_ctx);
EXPORT_API int youku_read_packet(struct demux_context *demux_ctx, AVPacket *pkt);
EXPORT_API int youku_packet_seek(struct demux_context *demux_ctx, int64_t timestamp);
EXPORT_API int youku_read_pause(struct demux_context *demux_ctx);
EXPORT_API int youku_read_play(struct demux_context *demux_ctx);
EXPORT_API int youku_stream_index(struct demux_context *demux_ctx, enum AVMediaType type);
EXPORT_API enum AVCodecID youku_query_avcodec_id(struct demux_context *demux_ctx, int index);
EXPORT_API void youku_destory(struct demux_context *demux_ctx);

#ifdef  __cplusplus
}
#endif


#endif // __DEMUX_H__
