#include "internal.h"
#include "globals.h"
#include "demux.h"

#include "unkown_demux.h"

// 具体的demux组件实现.

EXPORT_API int unkown_init_demux(struct demux_context *demux_ctx, source_context **source_ctx)
{
	unkown_demux_info &unkown_info = demux_ctx->info.unkown;
	unkown_demux *demux = new unkown_demux();
	unkown_demux_data d;

	// 保存文件名到d, 以作为参数传入unkown_demux中打开.
	d.file_name = std::string(unkown_info.file_name);

	// 保存source_context指针到d, 如果不为空, 则内部使用这个source_context来读取数据.
	// 否则使用demux内部默认的ffmpeg来访问数据.
	d.source_ctx = source_ctx;

	// 保存到demux_context.priv中做为私有的上下文.
	demux_ctx->priv = (void*)demux;

	// 执行打开操作.
	if (demux->open(d))
		return 0;

	return -1;
}

EXPORT_API int unkown_read_packet(struct demux_context *demux_ctx, AVPacket *pkt)
{
	unkown_demux *demux = (unkown_demux*)demux_ctx->priv;
	if (demux->read_packet(pkt))
		return 0;
	return -1;
}

EXPORT_API int unkown_packet_seek(struct demux_context *demux_ctx, int64_t timestamp)
{
	unkown_demux *demux = (unkown_demux*)demux_ctx->priv;
	if (demux->seek_packet(timestamp))
		return 0;
	return -1;
}

EXPORT_API int unkown_stream_index(struct demux_context *demux_ctx, enum AVMediaType type)
{
	unkown_demux *demux = (unkown_demux*)demux_ctx->priv;
	int index = -1;
	if (demux->stream_index(type, index))
		return index;
	return -1;
}

EXPORT_API enum AVCodecID unkown_query_avcodec_id(struct demux_context *demux_ctx, int index)
{
	unkown_demux *demux = (unkown_demux*)demux_ctx->priv;
	enum AVCodecID id;
	if (demux->query_avcodec_id(index, id))
		return id;
	return AV_CODEC_ID_NONE;
}

EXPORT_API void unkown_destory(struct demux_context *demux_ctx)
{
	unkown_demux *demux = (unkown_demux*)demux_ctx->priv;
	demux->close();	// 关闭demux.
	delete demux;	// 释放资源.
}
