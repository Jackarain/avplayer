#include "internal.h"
#include "globals.h"
#include "youku_demux.h"
#include "avplay.h"
#include "source.h"

#define IO_BUFFER_SIZE 32768

// 具体实现.

int youku_demux::read_data(void *opaque, uint8_t *buf, int buf_size)
{
	youku_demux *demux = (youku_demux*)opaque;

	// 已经中止播放.
// 	if (demux->is_abort())
// 		return 0;

	int ret = demux->m_source_ctx->read_data(demux->m_source_ctx, (char*)buf, buf_size);

	// 读取失败, 跳过, 这样就可以继续缓冲数据或者跳回前面播放.
	if (ret == -1)
		return 0;

	return ret;
}


int youku_demux::write_data(void *opaque, uint8_t *buf, int buf_size)
{
	youku_demux *demux = (youku_demux*)opaque;
	return 0;
}


int64_t youku_demux::seek_data(void *opaque, int64_t offset, int whence)
{
	youku_demux *demux = (youku_demux*)opaque;

	// 已经中止播放.
// 	if (demux->is_abort())
// 		return -1;

	offset = demux->m_source_ctx->read_seek(demux->m_source_ctx, offset, whence);

	if (demux->m_source_ctx->dl_info.not_enough)
	{
		// TODO: 判断是否数据足够, 如果不足以播放, 则暂停播放.
	}

	return offset;
}


youku_demux::youku_demux(void)
	: m_source_ctx(NULL)
	, m_format_ctx(NULL)
	, m_io_buffer(NULL)
	, m_avio_ctx(NULL)
	, m_abort(false)
{}

youku_demux::~youku_demux(void)
{}

int youku_demux::decode_interrupt_cb(void *ctx)
{
	youku_demux *demux = (youku_demux*)ctx;
	return (int)demux->m_abort;
}

bool youku_demux::open(boost::any ctx)
{
	av_register_all();
	avformat_network_init();

	// 得到传入的参数.
	m_youku_demux_data = boost::any_cast<youku_demux_data>(ctx);
	BOOST_ASSERT(m_youku_demux_data.type == MEDIA_TYPE_YK);

	// 初始化youku的source.
	m_source_ctx = alloc_media_source(MEDIA_TYPE_YK,
		m_youku_demux_data.youku_url.c_str(), m_youku_demux_data.youku_url.size(), 0);

	// 创建m_format_ctx.
	m_format_ctx = avformat_alloc_context();
	if (!m_format_ctx)
		goto FAILED_FLG;

	// 设置参数.
	m_format_ctx->flags = AVFMT_FLAG_GENPTS;
	m_format_ctx->interrupt_callback.callback = decode_interrupt_cb;
	m_format_ctx->interrupt_callback.opaque = (void*)this;

	m_source_ctx->init_source = yk_init_source;
	m_source_ctx->read_data = yk_read_data;
	m_source_ctx->read_seek = yk_read_seek;
	m_source_ctx->close = yk_close;
	m_source_ctx->destory = yk_destory;

	// 如果初始化失败.
	if (m_source_ctx->init_source(m_source_ctx) < 0)
		goto FAILED_FLG;

	int ret = 0;
	AVInputFormat *iformat = NULL;
	m_io_buffer = (unsigned char*)av_malloc(IO_BUFFER_SIZE);
	if (!m_io_buffer)
	{
		std::cerr << "Create buffer failed!\n";
		goto FAILED_FLG;
	}

	// 分配io上下文.
	m_avio_ctx = avio_alloc_context(m_io_buffer,
		IO_BUFFER_SIZE, 0, (void*)this, read_data, NULL, seek_data);
	if (!m_avio_ctx)
	{
		std::cerr << "Create io context failed!\n";
		goto FAILED_FLG;
	}
	m_avio_ctx->write_flag = 0;

	ret = av_probe_input_buffer(m_avio_ctx, &iformat, "", NULL, 0, 0);
	if (ret < 0)
	{
		std::cerr << "av_probe_input_buffer call failed!\n";
		goto FAILED_FLG;
	}

	// 打开输入媒体流.
	m_format_ctx->pb = m_avio_ctx;
	ret = avformat_open_input(&m_format_ctx, "", iformat, NULL);
	if (ret < 0)
	{
		std::cerr << "av_open_input_stream call failed!\n";
		goto FAILED_FLG;
	}

	ret = avformat_find_stream_info(m_format_ctx, NULL);
	if (ret < 0)
		goto FAILED_FLG;

FAILED_FLG:
	// 遇到出错, 释放各种资源.
	if (m_source_ctx)
	{
		free_media_source(m_source_ctx);
		m_source_ctx = NULL;
	}
	if (m_format_ctx)
	{
		avformat_close_input(&m_format_ctx);
		m_format_ctx = NULL;
	}

	return false;
}

bool youku_demux::read_packet(AVPacket *pkt)
{
	return false;
}

bool youku_demux::seek_packet(int64_t timestamp)
{
	return false;
}

bool youku_demux::stream_index(enum AVMediaType type, int &index)
{
	return false;
}

bool youku_demux::query_avcodec_id(int index, enum AVCodecID &codec_id)
{
	return false;
}

void youku_demux::close()
{

}

media_base_info youku_demux::base_info()
{
	return media_base_info();
}








