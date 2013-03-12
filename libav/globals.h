/*
 * globals.h
 * ~~~~~~~~~
 *
 * Copyright (c) 2012 Jack (jack.wgm@gmail.com)
 */

#ifndef __AVPLAYER_GLOBALS_H__
#define __AVPLAYER_GLOBALS_H__

#include <stdint.h>

#include "ffmpeg.h"

/* 媒体数据源接口. */
#define MEDIA_TYPE_FILE	0
#define MEDIA_TYPE_BT	1
#define MEDIA_TYPE_HTTP 2
#define MEDIA_TYPE_RTSP 3
#define MEDIA_TYPE_YK	4

#ifndef MAX_URI_PATH
#define MAX_URI_PATH	2048
#endif


/* 文件下载信息. */
typedef struct download_info
{
	int speed;			/* 下载下载速率. */
	int limit_speed;	/* 限制下载速率. */
	int not_enough;		/* 若为1, 则表示数据不够, 需要缓冲, 若为0, 表示缓冲完成. */
} download_info;


/* MEDIA_TYPE_FILE文件信息. */
typedef struct file_source_info
{
	char file_name[MAX_URI_PATH];	/*  文件名. */
} file_source_info;


/* 媒体文件信息. */
typedef struct media_info
{
	char file_name[MAX_URI_PATH];	/*  文件名. */
	int64_t start_pos;				/* 起始位置. */
	int64_t file_size;				/* 文件大小. */
} media_info;


/* MEDIA_TYPE_BT文件信息. */
typedef struct bt_source_info
{
	char *torrent_data;					/* torrent种子数据缓冲, 在source外部分配和释放.	*/
	int torrent_length;					/* torrent种子数据缓冲长度. */
	char save_path[MAX_URI_PATH];		/* 视频下载保存位置. */

	/*
	 * torrent中包含的视频文件信息, 一个torrent种子中可能包含多个视频文件.
	 * info其中包含了视频文件名, 视频文件在torrent中的偏移起始位置, 以及视频文
	 * 件大小.
	 * 内存在bt_init_source中分配, bt_close/bt_destory中释放.
	 */
	media_info *info;

	/* info的个数. */
	int info_size;

} bt_source_info;

/* MEDIA_TYPE_HTTP文件信息, 暂无实现, 依赖ffmpeg内部实现. */
typedef struct http_source_info
{
	char url[MAX_URI_PATH];
} http_source_info;

/* MEDIA_TYPE_RTSP文件信息, 暂无实现, 依赖ffmpeg内部实现. */
typedef struct rtsp_source_info
{
	char url[MAX_URI_PATH];
} rtsp_source_info;

/* MEDIA_TYPE_YK文件信息. */
typedef enum youku_source_type
{
	youku_hd2,		/* youku高清flv分段的数据. */
	youku_mp4,		/* youku的mp4分段的数据. */
	youku_3gp,		/* youku的3gp分段的数据. */
	youku_3gphd,	/* youku的3gphd分段的数据. */
	youku_flv,		/* youku的flv分段的数据. */
	youku_m3u8		/* youku的m3u8数据. */
} youku_source_type;

typedef struct yk_source_info
{
	char url[MAX_URI_PATH];			/* 优酷的url地址. */

	/*
	 * 播放类型, 有高清, mp4, 3gp, 3gp高清, flv, m3u8几种源, 在m3u8中, 
	 * 又可能有不同的类型的视频源.
	 */
	youku_source_type type;

	char save_path[MAX_URI_PATH];	/* 视频下载保存位置. */

} yk_source_info;


/*
 * source_info 是 source_context 包含的source信息, source_info可能是file_source_info,
 * bt_source_info或youku_source_type之中一个.
 * 可以根据source_context.type来区别具体的类型.
 */
typedef union source_info
{
	struct file_source_info file;	/* file_source_info类型. */
	struct bt_source_info bt;		/* bt_source_info类型. */
	struct yk_source_info yk;		/* yk_source_info类型. */
	struct http_source_info http;	/* http_source_info类型. */
	struct rtsp_source_info rtsp;	/* rtsp_source_info类型. */
} source_info;


/* 媒体数据源接口. */
typedef struct source_context
{
	/*
	 * init_source 函数用于初始化数据source部件, 因为视频数据的来源可能不同, 则具体实现
	 *	也可能不同, 所以, 初化时也会不同.
	 */
	int (*init_source)(struct source_context *source_ctx);

	/*
	 * read_data 函数用于读取指定大小的数据, 返回已经读取的数据大小.
	 */
	int64_t (*read_data)(struct source_context *source_ctx, char* buff, size_t buf_size);

	/*
	 * 跳转读取数据的位置.
	 */
	int64_t (*read_seek)(struct source_context *source_ctx, int64_t offset, int whence);

	/*
	 * 关闭数据读取源.
	 */
	void (*close)(struct source_context *source_ctx);

	/*
	 * 销毁数据读取源.
	 */
	void (*destory)(struct source_context *source_ctx);

	/*
	 * priv是保存内部用于访问实际数据的对象指针.
	 */
	void *priv;

	/*
	 * 数据类型, 可以是以下值
	 * MEDIA_TYPE_FILE
	 * MEDIA_TYPE_BT
	 * MEDIA_TYPE_HTTP
	 * MEDIA_TYPE_RTSP
	 * MEDIA_TYPE_YK
	 * 说明: 由于作者未实现http和rtsp协议而是直接使用了ffmpeg的demux, 所以就无需初始
	 * 化上面的各函数指针.
	 */
	int type;

	/*
	 * 具体的数据源信息, 这个信息是source和libav或其模块公有的信息.
	 * 可以被source修改或读取, 也可以被libav修改或读取.
	 * info里的信息一般是libav或其它模块传过来配合priv工作的参数信息.
	 */
	source_info info;

	/*
	 * 用于获得和控制下载.
	 * 
	 */
	download_info dl_info;

	/*
	 * 当前退出标识, 退出时为true.
	 */
	int abort;

} source_context;


/*
 * 数据源的类型.
 */
typedef enum source_type
{
	generic_source_type,		/* 未知的类型. */
	source_type_flv,		/* TODO: 普通的flv, 一个小示例. */
} source_type;

/* generic_type的信息, 由demux内部去实现分析. */
typedef struct generic_demux_info
{
	char file_name[MAX_URI_PATH];	/* 文件名. */
} generic_demux_info;

/* TODO: source_type_flv的信息, 由demux使用. */
typedef struct flv_demux_info
{
	int unused;						/* 暂时没有有用的信息. */
} flv_demux_info;

/* 包含具体的demux_info的信息共用. */
typedef union demux_info
{
	generic_demux_info unkown;	/* 未知的文件格式. */
	flv_demux_info flv;			/* TODO: flv格式. */
} demux_info;

/*
 * 媒体文件的基本信息, 这些信息都从demux解析获得, 由libav使用.
 */
typedef struct media_base_info
{
	int has_video;	/* 是否有视频信息, -1表示没有. */
	int has_audio;	/* 是否有音频信息, -1表示没有. */

	int64_t video_start_time;	/* 视频起始时间信息. */
	int64_t audio_start_time;	/* 音频起始时间信息. */

	AVRational video_time_base;	/* 视频基本时间. */
	AVRational audio_time_base;	/* 音频基本时间. */

	AVRational video_frame_rate;	/* 视频帧率. */
	AVRational audio_frame_rate;	/* 音频帧率. */

	int64_t duration;				/* 视频时长信息. */

} media_base_info;

/*
 * demuxer结构定义.
 */
typedef struct demux_context
{
	/*
	 * 初始化demux.
	 * demux_ctx 是demux_context本身的指针.
	 * 返回0表示成功, -1表示失败.
	 */
	int (*init_demux)(struct demux_context *demux_ctx);

	/*
	 * 读取一个packet到pkt中.
	 * demux_ctx 是demux_context本身的指针.
	 * ptk 是一个用于读取packet已经被av_init_packet初始化的指针.
	 * 返回0表示ok, 小于0表示失败或者已经到了文件尾.
	 */
	int (*read_packet)(struct demux_context *demux_ctx, AVPacket *pkt);

	/*
	 * seek_packet 是用于seek到指定的timestamp位置.
	 * demux_ctx 是demux_context本身的指针.
	 * timestamp 是指定的时间位置.
	 * 返回0表示ok, 其它值表示失败.
	 */
	int (*seek_packet)(struct demux_context *demux_ctx, int64_t timestamp);

	/*
	 * 读取暂停, 主要为RTSP这种网络媒体协议.
	 */
	int (*read_pause)(struct demux_context *demux_ctx);

	/*
	 * 同上, 恢复播放.
	 */
	int (*read_play)(struct demux_context *demux_ctx);

	/*
	 * 查询指定媒体类型的index.
	 * demux_ctx 是demux_context本身的指针.
	 * type 是指定的类型信息.
	 * 返回指定类型的index信息, -1表示失败.
	 */
	int (*stream_index)(struct demux_context *demux_ctx, enum AVMediaType type);

	/*
	 * 查询指定index上的AVCodecID信息.
	 * demux_ctx 是demux_context本身的指针.
	 * index 是指定的index信息, 可由stream_index查询得到.
	 * 返回指定的index的AVCodecID信息.
	 */
	enum AVCodecID (*query_avcodec_id)(struct demux_context *demux_ctx, int index);

	/*
	 * destory 用于销毁当前demux_context.
	 */
	void (*destory)(struct demux_context *demux_ctx);

	/*
	 * priv是保存内部用于实现demuxer对象指针.
	 */
	void *priv;

	/*
	 * 指定的类型, 可以取source_type或youku_source_type中的类型.
	 * 备注: 目前实现的type, 只有以下几个:
	 */
	int type;

	/*
	 * demux信息, 用于保存一些demux时需要用到的信息, 以及和外部通信的信息.
	 */
	demux_info info;

	/*
	 * 包含一些基本的视频信息, 由demux模块维护.
	 */
	media_base_info base_info;

	/*
	 * 当前退出标识, 退出时为true.
	 */
	int abort;

} demux_context;


/*
 * 视频播放结构定义.
 */
typedef struct vo_context
{
	int (*init_video)(struct vo_context *vo_ctx, int w, int h, int pix_fmt);
	int (*render_one_frame)(struct vo_context *vo_ctx, AVFrame* data, int pix_fmt, double pts);
	void (*re_size)(struct vo_context *vo_ctx, int width, int height);
	void (*aspect_ratio)(struct vo_context *vo_ctx, int srcw, int srch, int enable_aspect);
	int (*use_overlay)(struct vo_context *vo_ctx);
	void (*destory_video)(struct vo_context *vo_ctx);
	void *priv;
	void *user_data;	/* for window hwnd. */
	void *user_ctx;		/* for user context. */
	float fps;			/* fps */
} vo_context;


/*
 * 音频播放输出结构定义.
 */
typedef struct ao_context
{
	int (*init_audio)(struct ao_context *ao_ctx, uint32_t channels, uint32_t bits_per_sample,
		uint32_t sample_rate, int format);
	int (*play_audio)(struct ao_context *ao_ctx, uint8_t *data, uint32_t size);
	void (*audio_control)(struct ao_context *ao_ctx, double l, double r);
	void (*mute_set)(struct ao_context *ao_ctx, int s);
	void (*destory_audio)(struct ao_context *ao_ctx);
	void *priv;
} ao_context;

#endif /* __AVPLAYER_GLOBALS_H__ */
