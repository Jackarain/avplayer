#include "internal.h"
#include "unkown_demux.h"

#define IO_BUFFER_SIZE	32768

int unkown_demux::decode_interrupt_cb(void *ctx)
{
	unkown_demux *demux = (unkown_demux*)ctx;
	return (int)demux->m_abort;
	// return demux->is_abort();
}

int unkown_demux::read_data(void *opaque, uint8_t *buf, int buf_size)
{
 	int ret = 0;
//  	unkown_demux *demux = (avplay*)opaque;
//  
//  	// 已被中止.
//  	if (demux->is_abort())
//  		return 0;
//  
//  	// 读取数据.
//  	ret = play->m_source_ctx->read_data(play->m_source_ctx, buf, buf_size);
//  
//  	// 读取失败, 跳过, 这样就可以继续缓冲数据或者跳回前面播放.
//  	if (ret == -1)
//  		return 0;
 
 	return ret;
}


int unkown_demux::write_data(void *opaque, uint8_t *buf, int buf_size)
{
	unkown_demux *demux = (unkown_demux*)opaque;
	return 0;
}


int64_t unkown_demux::seek_data(void *opaque, int64_t offset, int whence)
{
 	unkown_demux *demux = (unkown_demux*)opaque;
 
//  	if (play->m_abort)
//  		return -1;
//  
//  	// 如果存在read_seek函数实现, 则调用相应的函数实现, 处理相关事件.
//  	if (play->m_source_ctx && play->m_source_ctx->read_seek)
//  		offset = play->m_source_ctx->read_seek(play->m_source_ctx, offset, whence);
//  	else
//  		assert(0);
//  
//  	if (play->m_source_ctx->dl_info.not_enough)
//  	{
//  		// TODO: 判断是否数据足够, 如果不足以播放, 则暂停播放.
//  	}
 
 	return offset;
}

unkown_demux::unkown_demux(void)
	: m_format_ctx(NULL)
	, m_avio_ctx(NULL)
	, m_source_ctx(NULL)
	, m_io_buffer(NULL)
	, m_abort(false)
{}

unkown_demux::~unkown_demux(void)
{}

bool unkown_demux::open(boost::any ctx)
{
	// 得到传入的参数.
	m_unkown_demux_data = boost::any_cast<unkown_demux_data>(ctx);

	// 分配m_format_ctx.
	m_format_ctx = avformat_alloc_context();
	if (!m_format_ctx)
		goto FAILED_FLG;

	// 设置参数.
	m_format_ctx->flags = AVFMT_FLAG_GENPTS;
	m_format_ctx->interrupt_callback.callback = decode_interrupt_cb;
	m_format_ctx->interrupt_callback.opaque = (void*)this;

	// 保存指针.
	m_source_ctx = *m_unkown_demux_data.source_ctx;

	// 初始化数据源.
	if (m_source_ctx)
	{
		// 如果初始化失败.
		if (m_source_ctx->init_source(m_source_ctx) < 0)
			goto FAILED_FLG;
	}

	int ret = 0;
	AVInputFormat *iformat = NULL;

	// 支持BT和本地文件.
	if (m_source_ctx->type == MEDIA_TYPE_BT || m_source_ctx->type == MEDIA_TYPE_FILE)
	{
		m_io_buffer = (unsigned char*)malloc(IO_BUFFER_SIZE);
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
	}
	else
	{
		// 得到相应的url.
		char url[MAX_URI_PATH];
		if (m_source_ctx->type == MEDIA_TYPE_HTTP)
			strcpy(url, m_source_ctx->info.http.url);
		else if (m_source_ctx->type == MEDIA_TYPE_RTSP)
			strcpy(url, m_source_ctx->info.rtsp.url);
		else
			goto FAILED_FLG;

		// 空串, 跳到错误.
		if (strlen(url) == 0)
			goto FAILED_FLG;

		/* HTTP和RTSP直接使用ffmpeg来处理.	*/
		int ret = avformat_open_input(&m_format_ctx, url, iformat, NULL);
		if (ret < 0)
		{
			std::cerr << "av_open_input_stream call failed!\n";
			goto FAILED_FLG;
		}
	}

	ret = avformat_find_stream_info(m_format_ctx, NULL);
	if (ret < 0)
		goto FAILED_FLG;

#ifdef _DEBUG
	av_dump_format(m_format_ctx, 0, NULL, 0);
#endif // _DEBUG

	// 置空.
	memset(&m_base_info, 0, sizeof(m_base_info));

	// 获得一些基本信息.
	m_base_info.has_video = query_index(AVMEDIA_TYPE_VIDEO, m_format_ctx);
	m_base_info.has_audio = query_index(AVMEDIA_TYPE_AUDIO, m_format_ctx);

	// 如果有音频, 获得音频的一些基本信息.
	if (m_base_info.has_audio >= 0)
	{
		avrational_copy(m_format_ctx->streams[m_base_info.has_audio]->r_frame_rate, m_base_info.audio_frame_rate);
		avrational_copy(m_format_ctx->streams[m_base_info.has_audio]->time_base, m_base_info.audio_time_base);
		m_base_info.audio_start_time = m_format_ctx->streams[m_base_info.has_audio]->start_time;
	}

	// 如果有视频, 获得视频的一些基本信息.
	if (m_base_info.has_video >= 0)
	{
		avrational_copy(m_format_ctx->streams[m_base_info.has_video]->r_frame_rate, m_base_info.video_frame_rate);
		avrational_copy(m_format_ctx->streams[m_base_info.has_video]->time_base, m_base_info.video_time_base);
		m_base_info.video_start_time = m_format_ctx->streams[m_base_info.has_video]->start_time;
	}

FAILED_FLG:
	if (m_format_ctx)
		avformat_close_input(&m_format_ctx);
	if (m_avio_ctx)
		av_free(m_avio_ctx);
	if (m_io_buffer)
		av_free(m_io_buffer);
	return false;
}

bool unkown_demux::read_packet(AVPacket *pkt)
{
	return false;
}

bool unkown_demux::seek_packet(int64_t timestamp)
{
	return false;
}

bool unkown_demux::stream_index(enum AVMediaType type, int &index)
{
	return false;
}

bool unkown_demux::query_avcodec_id(int index, enum AVCodecID &codec_id)
{
	return false;
}

void unkown_demux::close()
{
}

int unkown_demux::read_pause()
{
	return -1;
}

int unkown_demux::read_play()
{
	return -1;
}

int unkown_demux::query_index(enum AVMediaType type, AVFormatContext *ctx)
{
	unsigned int i;

	for (i = 0; (unsigned int) i < ctx->nb_streams; i++)
		if (ctx->streams[i]->codec->codec_type == type)
			return i;
	return -1;
}

media_base_info unkown_demux::base_info()
{
	return m_base_info;
}
