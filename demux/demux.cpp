#include "internal.h"
#include "globals.h"
#include "demux.h"

#include "generic_demux.h"
#include "youku_demux.h"

// 具体的demux组件实现.

EXPORT_API int generic_init_demux(struct demux_context *demux_ctx)
{
	generic_demux_info &generic_info = demux_ctx->info.generic;
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
	{
		demux_ctx->base_info = demux->base_info();
		return 0;
	}

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

EXPORT_API int generic_read_pause(struct demux_context *demux_ctx)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	return demux->read_pause();
}

EXPORT_API int generic_read_play(struct demux_context *demux_ctx)
{
	generic_demux *demux = (generic_demux*)demux_ctx->priv;
	return demux->read_play();
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
	demux_ctx->priv = NULL;
}



// 具体的youku视频相关处理.

EXPORT_API int youku_init_demux(struct demux_context *demux_ctx)
{
	youku_demux_info &youku_info = demux_ctx->info.youku;
	youku_demux *demux = new youku_demux();
	youku_demux_data d;

	// 保存文件名到d, 以作为参数传入generic_demux中打开.
	d.youku_url = std::string(youku_info.youku_url);

	// 保存到demux_context.priv中做为私有的上下文.
	demux_ctx->priv = (void*)demux;

	// 保存数据源的类型(也就是播放类型, 比如是BT还是YOUKU等, 不过在这里, 必然应该是
	// MEDIA_TYPE_YK, 否则就是用错了).
	d.type = demux_ctx->type;
	BOOST_ASSERT(d.type == MEDIA_TYPE_YK);

	// 执行打开操作.
	if (demux->open(d))
	{
		demux_ctx->base_info = demux->base_info();
		return 0;
	}

	return -1;
}

EXPORT_API int youku_read_packet(struct demux_context *demux_ctx, AVPacket *pkt)
{
	return -1;
}

EXPORT_API int youku_packet_seek(struct demux_context *demux_ctx, int64_t timestamp)
{
	return -1;
}

EXPORT_API int youku_read_pause(struct demux_context *demux_ctx)
{
	return -1;
}

EXPORT_API int youku_read_play(struct demux_context *demux_ctx)
{
	return -1;
}

EXPORT_API int youku_stream_index(struct demux_context *demux_ctx, enum AVMediaType type)
{
	return -1;
}

EXPORT_API enum AVCodecID youku_query_avcodec_id(struct demux_context *demux_ctx, int index)
{
	return AV_CODEC_ID_NONE;
}

EXPORT_API void youku_destory(struct demux_context *demux_ctx)
{

}
