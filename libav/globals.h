/*
 * globals.h
 * ~~~~~~~~~
 *
 * Copyright (c) 2012 Jack (jack.wgm@gmail.com)
 */

#ifndef __AVPLAYER_GLOBALS_H__
#define __AVPLAYER_GLOBALS_H__

#include <stdint.h>

struct AVFrame;

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
	char *torrent_data;				/* torrent种子数据缓冲, 在source外部分配和释放.	*/
	int torrent_length;				/* torrent种子数据缓冲长度. */
	char save_path[MAX_URI_PATH];	/* 视频下载保存位置. */

	/*
	 * torrent中包含的视频文件信息, 一个torrent种子中可能包含多个视频文件.
	 * info其中包含了视频文件名, 视频文件在torrent中的偏移起始位置, 以及视频文
	 * 件大小.
	 */
	media_info *info;

	/* info的个数. */
	int info_size;

} bt_source_info;

/* TODO: MEDIA_TYPE_HTTP文件信息, 暂无实现, 依赖ffmpeg内部实现. */
/* TODO: MEDIA_TYPE_RTSP文件信息, 暂无实现, 依赖ffmpeg内部实现. */

/* MEDIA_TYPE_YK文件信息. */
typedef enum youku_source_type
{
	youku_hd2,
	youku_mp4,
	youku_3gp,
	youku_3gphd,
	youku_flv,
	youku_m3u8
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
	file_source_info file;	/* file_source_info类型. */
	bt_source_info bt;		/* bt_source_info类型. */
	yk_source_info yk;		/* yk_source_info类型. */
} source_info;


/* 媒体数据源接口. */
typedef struct source_context
{
	/*
	 * init_source 函数用于初始化数据source部件, 因为视频数据的来源可能不同, 则具体实现
	 *	也可能不同, 所以, 初化时也会不同.
	 */
	int (*init_source)(void *ctx);

	/*
	 * read_data 函数用于读取指定大小的数据, 返回已经读取的数据大小.
	 */
	int64_t (*read_data)(void *ctx, char* buff, size_t buf_size);

	/*
	 * 跳转读取数据的位置.
	 */
	int64_t (*read_seek)(void *ctx, int64_t offset, int whence);

	/*
	 * 关闭数据读取源.
	 */
	void (*close)(void *ctx);

	/*
	 * 销毁数据读取源.
	 */
	void (*destory)(void *ctx);

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
	 * 记录当前播放数据偏移, 绝对位置.
	 */
	int64_t offset;

	/*
	 * 用于获得和控制下载.
	 * 
	 */
	download_info dl_info;

	/*
	 * 当前退出标识, 退出时为true.
	 */
	int abort;

// 	/*
// 	 * 如果类型是MEDIA_TYPE_BT, 则此指向bt种子数据.
// 	 */
// 	char *torrent_data;
// 	int torrent_len;
// 	char *save_path;	/* 在av_destory中释放. */

// 	/* torrent中的媒体文件信息, 只有在打开
// 	 * torrent之后, 这里面才可能有数据.
// 	 */
// 	media_info *media;

// 	/*
// 	 * 媒体文件信息个数, 主要为BT文件播放设计, 因为一个torrent中可
// 	 * 能存在多个视频文件.
// 	 */
// 	int media_size;




} source_context;


/*
 * 视频播放结构定义.
 */
typedef struct vo_context
{
	int (*init_video)(void* ctx, int w, int h, int pix_fmt);
	int (*render_one_frame)(void *ctx, AVFrame* data, int pix_fmt, double pts);
	void (*re_size)(void *ctx, int width, int height);
	void (*aspect_ratio)(void *ctx, int srcw, int srch, int enable_aspect);
	int (*use_overlay)(void *ctx);
	void (*destory_video)(void *ctx);
	void *video_dev;
	void *user_data;	/* for window hwnd. */
	void *user_ctx;		/* for user context. */
	float fps;			/* fps */
} vo_context;

/*
 * 音频播放输出结构定义.
 */
typedef struct ao_context
{
	int (*init_audio)(void *ctx, uint32_t channels, uint32_t bits_per_sample,
		uint32_t sample_rate, int format);
	int (*play_audio)(void *ctx, uint8_t *data, uint32_t size);
	void (*audio_control)(void *ctx, double l, double r);
	void (*mute_set)(void *ctx, int s);
	void (*destory_audio)(void *ctx);
	void *audio_dev;
} ao_context;

#endif /* __AVPLAYER_GLOBALS_H__ */
