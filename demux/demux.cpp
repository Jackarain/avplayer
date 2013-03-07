#include "internal.h"
#include "globals.h"
#include "demux.h"

// 具体的demux组件实现.

EXPORT_API int unkown_init_demux(struct demux_context *demux_ctx, source_context **source_ctx)
{
	return -1;
}

EXPORT_API int unkown_read_packet(struct demux_context *demux_ctx, AVPacket *pkt)
{
	return -1;
}

EXPORT_API int unkown_packet_seek(struct demux_context *demux_ctx, int64_t timestamp)
{
	return -1;
}

EXPORT_API int unkown_stream_index(struct demux_context *demux_ctx, enum AVMediaType type)
{
	return -1;
}

EXPORT_API enum AVCodecID unkown_query_avcodec_id(struct demux_context *demux_ctx, int index)
{
	return AV_CODEC_ID_NONE;
}

EXPORT_API void unkown_destory(struct demux_context *demux_ctx)
{
	return;
}
