#include "internal.h"
#include "globals.h"
#include "demux.h"

#include "generic_demux.h"

// 具体的demux组件实现.

EXPORT_API int generic_init_demux(struct demux_context *demux_ctx)
{
	generic_demux_info &generic_info = demux_ctx->info.unkown;
	generic_demux *demux = new generic_demux();
	generic_demux_data d;

	// 保存文件名到d, 以作为参数传入generic_demux中打开.
	d.file_name = std::string(generic_info.file_name);

	// 保存到demux_context.priv中做为私有的上下文.
	demux_ctx->priv = (void*)demux;

	// 保存数据源类型.
	d.type = demux_ctx->type;

	// 执行打开操作.
	if (demux->open(d))
		return 0;

	return -1;
}

EXPORT_API int generic_read_packet(struct demux_context *demux_ctx, AVPacket *pkt)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	if (demux->read_packet(pkt))
		return 0;
	return -1;
}

EXPORT_API int generic_packet_seek(struct demux_context *demux_ctx, int64_t timestamp)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	if (demux->seek_packet(timestamp))
		return 0;
	return -1;
}

EXPORT_API int generic_stream_index(struct demux_context *demux_ctx, enum AVMediaType type)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	int index = -1;
	if (demux->stream_index(type, index))
		return index;
	return -1;
}

EXPORT_API enum AVCodecID generic_query_avcodec_id(struct demux_context *demux_ctx, int index)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	enum AVCodecID id;
	if (demux->query_avcodec_id(index, id))
		return id;
	return AV_CODEC_ID_NONE;
}

EXPORT_API void generic_destory(struct demux_context *demux_ctx)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	demux->close();	// 关闭demux.
	delete demux;	// 释放资源.
}
