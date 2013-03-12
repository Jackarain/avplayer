#include "internal.h"
#include "generic_demux.h"
#include "source.h"
#include "avplay.h"

#define IO_BUFFER_SIZE	32768

// INT64最大最小取值范围.
#ifndef INT64_MIN
#define INT64_MIN (-9223372036854775807LL - 1)
#endif
#ifndef INT64_MAX
#define INT64_MAX (9223372036854775807LL)
#endif

int generic_demux::decode_interrupt_cb(void *ctx)
{
	generic_demux *demux = (generic_demux*)ctx;
	return (int)demux->m_abort;
	// return demux->is_abort();
}

int generic_demux::read_data(void *opaque, uint8_t *buf, int buf_size)
{
	generic_demux *demux = (generic_demux*)opaque;

	// 已经中止播放.
	if (demux->is_abort())
		return 0;

	int ret = demux->m_source_ctx->read_data(demux->m_source_ctx, (char*)buf, buf_size);

	// 读取失败, 跳过, 这样就可以继续缓冲数据或者跳回前面播放.
	if (ret == -1)
		return 0;

 	return ret;
}


int generic_demux::write_data(void *opaque, uint8_t *buf, int buf_size)
{
	generic_demux *demux = (generic_demux*)opaque;
	return 0;
}


int64_t generic_demux::seek_data(void *opaque, int64_t offset, int whence)
{
 	generic_demux *demux = (generic_demux*)opaque;

	// 已经中止播放.
	if (demux->is_abort())
		return -1;

	offset = demux->m_source_ctx->read_seek(demux->m_source_ctx, offset, whence);

	if (demux->m_source_ctx->dl_info.not_enough)
	{
		// TODO: 判断是否数据足够, 如果不足以播放, 则暂停播放.
	}

	return offset;
}

generic_demux::generic_demux(void)
	: m_format_ctx(NULL)
	, m_avio_ctx(NULL)
	, m_source_ctx(NULL)
	, m_io_buffer(NULL)
	, m_abort(false)
{}

generic_demux::~generic_demux(void)
{}

bool generic_demux::open(boost::any ctx)
{
	// 得到传入的参数.
	m_generic_demux_data = boost::any_cast<generic_demux_data>(ctx);

	// 分配m_format_ctx.
	m_format_ctx = avformat_alloc_context();
	if (!m_format_ctx)
		goto FAILED_FLG;

	// 设置参数.
	m_format_ctx->flags = AVFMT_FLAG_GENPTS;
	m_format_ctx->interrupt_callback.callback = decode_interrupt_cb;
	m_format_ctx->interrupt_callback.opaque = (void*)this;

	if (m_generic_demux_data.type == MEDIA_TYPE_BT)
	{
		FILE *fp = fopen(m_generic_demux_data.file_name.c_str(), "r+b");
		if (!fp)
			goto FAILED_FLG;
		uint64_t file_lentgh = fs::file_size(m_generic_demux_data.file_name);
		char *torrent_data = (char*)malloc(file_lentgh);
		if (!torrent_data)
			goto FAILED_FLG;
		int readbytes = fread(torrent_data, 1, file_lentgh, fp);
		m_source_ctx = alloc_media_source(MEDIA_TYPE_BT, torrent_data, readbytes, 0);
		free(torrent_data);
		if (!m_source_ctx)
			goto FAILED_FLG;

		// 初始化bt读取函数指针.
		m_source_ctx->init_source = bt_init_source;
		m_source_ctx->read_data = bt_read_data;
		m_source_ctx->read_seek = bt_read_seek;
		m_source_ctx->close = bt_close;
		m_source_ctx->destory = bt_destory;
	}
	else
	{
		m_source_ctx = alloc_media_source(m_generic_demux_data.type,
			m_generic_demux_data.file_name.c_str(), m_generic_demux_data.file_name.length(), 0);
		if (!m_source_ctx)
			goto FAILED_FLG;
		// 按种类分配不同的数据源处理函数.
		switch (m_generic_demux_data.type)
		{
		case MEDIA_TYPE_FILE:
			{
				m_source_ctx->init_source = file_init_source;
				m_source_ctx->read_data = file_read_data;
				m_source_ctx->read_seek = file_read_seek;
				m_source_ctx->close = file_close;
				m_source_ctx->destory = file_destory;
			}
			break;
		case MEDIA_TYPE_HTTP: break;
		case MEDIA_TYPE_RTSP: break;
		case MEDIA_TYPE_YK: goto FAILED_FLG;	// 暂不支持自动识别YK格式视频.
		default: break;
		}
	}

	// 如果初始化失败.
	if (m_source_ctx->init_source(m_source_ctx) < 0)
		goto FAILED_FLG;

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

bool generic_demux::read_packet(AVPacket *pkt)
{
	int ret = av_read_frame(m_format_ctx, pkt);
	if (ret < 0)
		return false;
	return true;
}

bool generic_demux::seek_packet(int64_t timestamp)
{
	int64_t seek_min = INT64_MIN;
	int64_t seek_max = INT64_MAX;
	int seek_flags = 0 & (~AVSEEK_FLAG_BYTE);
	int ret = avformat_seek_file(m_format_ctx, -1, seek_min, timestamp, seek_max, seek_flags);
	if (ret < 0)
		return false;
	return true;
}

bool generic_demux::stream_index(enum AVMediaType type, int &index)
{
	index = -1;

	for (unsigned int i = 0; (unsigned int) i < m_format_ctx->nb_streams; i++)
	{
		if (m_format_ctx->streams[i]->codec->codec_type == type)
		{
			i = index;
			return true;
		}
	}

	return false;
}

bool generic_demux::query_avcodec_id(int index, enum AVCodecID &codec_id)
{
	if (index >= 0 && index < m_format_ctx->nb_streams)
	{
		codec_id = m_format_ctx->streams[index]->codec->codec_id;
		return true;
	}
	return false;
}

void generic_demux::close()
{
	if (m_format_ctx)
	{
		avformat_close_input(&m_format_ctx);
	}

	if (m_io_buffer)
	{
		free(m_io_buffer);
		m_io_buffer = NULL;
	}
}

int generic_demux::read_pause()
{
	return av_read_pause(m_format_ctx);
}

int generic_demux::read_play()
{
	return av_read_play(m_format_ctx);
}

int generic_demux::query_index(enum AVMediaType type, AVFormatContext *ctx)
{
	unsigned int i;

	for (i = 0; (unsigned int) i < ctx->nb_streams; i++)
		if (ctx->streams[i]->codec->codec_type == type)
			return i;
	return -1;
}

media_base_info generic_demux::base_info()
{
	return m_base_info;
}
