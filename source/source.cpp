#include "internal.h"
#include "globals.h"
#include "source.h"
#include "av_source.h"
#include "file_source.h"

#ifdef USE_TORRENT
#include <boost/any.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include "torrent_source.h"
#include "libtorrent/interface.hpp"

#endif // USE_TORRENT


#ifdef USE_YK
#include "yk_source.h"
#endif // USE_YK

#ifdef  __cplusplus
extern "C" {
#endif

static
void ansi_wide(std::string &ansi, std::wstring &wide)
{
	if (ansi.size() == 0)
		return ;
	wide.resize(ansi.size());
	std::size_t size = ansi.size();
	mbstowcs((wchar_t*)wide.c_str(), ansi.c_str(), size);
}

EXPORT_API int file_init_source(struct source_context *ctx)
{
	// 得到文件信息的引用, 方便取出文件名信息.
	file_source_info &file = ctx->info.file;

	// 检查文件名长度是否有效.
	if (strlen(file.file_name) <= 0)
		return -1;

	// 创建file_source对象.
	file_source *fs = new file_source();

	// 保存到priv指针.
	ctx->priv = (void*)fs;

	// new open_file_data 由file_source内部管理内存.
	open_file_data *od = new open_file_data;

	// 保存文件名到open_file_data中.
	od->filename = std::string(file.file_name);

	// 设置线程标识.
	od->is_multithread = false;

	// 打开文件.
	return fs->open((void*)od) ? 0 : -1;
}

EXPORT_API int64_t file_read_data(struct source_context *ctx, char* buff, size_t buf_size)
{
	file_source *fs = (file_source *)ctx->priv;
	size_t read_size = 0;

	bool ret = fs->read_data(buff, buf_size, read_size);
	if (ret)
		return read_size;

	return -1;
}

EXPORT_API int64_t file_read_seek(struct source_context *ctx, int64_t offset, int whence)
{
	file_source *fs = (file_source *)ctx->priv;
	return fs->read_seek(offset, whence);
}

EXPORT_API void file_close(struct source_context *ctx)
{
	file_source *fs = (file_source *)ctx->priv;
	fs->close();
}

EXPORT_API void file_destory(struct source_context *ctx)
{
	file_source *fs = (file_source *)ctx->priv;
	if (fs)
	{
		fs->close();
		delete fs;
		ctx->priv = NULL;
	}
}

EXPORT_API int bt_init_source(struct source_context *ctx)
{
#ifdef USE_TORRENT
	torrent_source *ts = new torrent_source();
	open_torrent_data *otd = new open_torrent_data;
	bt_source_info &bt_info = ctx->info.bt;

	// 保存torrent种子数据.
	otd->is_file = false;

	// 分配空间.
	char *dst = new char[bt_info.torrent_length];
	otd->torrent_data.reset(dst);

	// 复制torrent数据到open_torrent_data中.
	memcpy(dst, bt_info.torrent_data, bt_info.torrent_length);

	// 更新种子数据长度.
	otd->data_size = bt_info.torrent_length;

	// 得到当前路径, 并以utf8编码.
	// windows平台才需要，Linux下就是utf8,无需转化.
	std::string ansi;

#ifdef _WIN32
	std::wstring path;
	setlocale(LC_ALL, "chs");
#endif

	// 得到保存路径, 如果为空, 则下载数据保存在当前目录下.
	if (!strlen(bt_info.save_path))
	{
		ansi = boost::filesystem::current_path().string();
#ifdef _WIN32
		ansi_wide(ansi, path);
		libtorrent::wchar_utf8(path, ansi);
#endif
		// 更新保存路径.
		otd->save_path = ansi;
		strcpy(bt_info.save_path, ansi.c_str());
	}
	else
	{
		ansi = std::string(bt_info.save_path);
#ifdef _WIN32
		ansi_wide(ansi, path);
		libtorrent::wchar_utf8(path, ansi);
#endif
		// 更新保存路径.
		otd->save_path = ansi;
		strcpy(bt_info.save_path, ansi.c_str());
	}

	// 保存到priv指针.
	ctx->priv = (void*)ts;

	// 打开种子.
	if (ts->open((void*)otd))
	{
		// 更新视频信息, 保存到bt_info当中.
		std::vector<video_file_info> vfi = ts->video_list();

		bt_info.info_size = vfi.size();
		bt_info.info = (media_info*)malloc(sizeof(media_info) * bt_info.info_size);

		for (int i = 0; i < bt_info.info_size; i++)
		{
			video_file_info &f = vfi[i];

			strcpy(bt_info.info[i].file_name, f.filename.c_str());
			bt_info.info[i].file_size = f.data_size;
			bt_info.info[i].start_pos = f.base_offset;
		}

		return 0;
	}

	return -1;
#else
	return -1;
#endif // USE_TORRENT
}

EXPORT_API int64_t bt_read_data(struct source_context *ctx, char* buff, size_t buf_size)
{
#ifdef USE_TORRENT
	torrent_source *ts = (torrent_source*)ctx->priv;
	size_t readbytes = 0;

	if (!ts->read_data(buff, buf_size, readbytes))
		return -1;
	return readbytes;

#else
	return -1;
#endif // USE_TORRENT
}

EXPORT_API int64_t bt_read_seek(struct source_context *ctx, int64_t offset, int whence)
{
#ifdef USE_TORRENT
	torrent_source *ts = (torrent_source*)ctx->priv;

	// 如果返回true, 则表示数据不够, 需要缓冲.
	int64_t ret = ts->read_seek(offset, whence);

	if (whence == SEEK_SET || whence == SEEK_CUR || whence == SEEK_END)
	{
		if (!ts->has_data(ret))
		{
			// 表示seek位置没有数据, 上层应该根据dl_info.not_enough判断是否暂时进行缓冲.
			ctx->dl_info.not_enough = 1;
		}
	}

	return ret;

#else
	return -1;
#endif // USE_TORRENT
}

EXPORT_API void bt_close(struct source_context *ctx)
{
#ifdef USE_TORRENT
	torrent_source *ts = (torrent_source*)ctx->priv;
	ts->close();
	bt_source_info &bt_info = ctx->info.bt;
	if (bt_info.info)
	{
		free(bt_info.info);
		bt_info.info = NULL;
	}
#endif // USE_TORRENT
}

EXPORT_API void bt_destory(struct source_context *ctx)
{
#ifdef USE_TORRENT
	torrent_source *ts = (torrent_source*)ctx->priv;
	if (ts)
	{
		ts->close();
		bt_source_info &bt_info = ctx->info.bt;
		if (bt_info.info)
		{
			free(bt_info.info);
			bt_info.info = NULL;
		}
		delete ts;
		ctx->priv = NULL;
	}
#endif // USE_TORRENT
}


EXPORT_API int yk_init_source(struct source_context *ctx)
{
#ifdef USE_YK
	yk_source_info &yk_info = ctx->info.yk;

	// 检查参数有效性.
	if (!strlen(yk_info.url))
		return -1;

    yk_source *ys = new yk_source();
	open_yk_data *yd = new open_yk_data;

	// 更新priv, 把yk_source对象保存到priv中.
    ctx->priv = (void*)ys;

	// 保存url.
	yd->url = std::string(yk_info.url);

	// 得到当前路径, 并以utf8编码.
	// windows平台才需要，Linux下就是utf8,无需转化.
	std::string ansi;

#ifdef _WIN32
	std::wstring path;
	setlocale(LC_ALL, "chs");
#endif

	// 得到保存路径, 如果为空, 则下载数据保存在当前目录下.
	if (!strlen(yk_info.save_path))
	{
		ansi = boost::filesystem::current_path().string();
#ifdef _WIN32
		ansi_wide(ansi, path);
		libtorrent::wchar_utf8(path, ansi);
#endif
		// 更新保存路径.
		yd->save_path = ansi;
		strcpy(yk_info.save_path, ansi.c_str());
	}
	else
	{
		ansi = std::string(yk_info.save_path);
#ifdef _WIN32
		ansi_wide(ansi, path);
		libtorrent::wchar_utf8(path, ansi);
#endif
		// 更新保存路径.
		yd->save_path = ansi;
		strcpy(yk_info.save_path, ansi.c_str());
	}

	// 保存请求的视频类型.
	yd->type = yk_info.type;

	// 打开视频.
	if (ys->open((void*)yd))
		return 0;
	return -1;

#else
    return -1;
#endif // USE_YK
    return 0;
}

EXPORT_API int yk_media_info(struct source_context *ctx, char *name, int64_t *pos, int64_t *size)
{
#ifdef USE_YK
	return -1;
//     source_context *sc = (source_context*)ctx;
//     yk_source *ts = (yk_source*)sc->io_dev;
// 
//     if (!ts)
//         return -1;
// 
//     std::vector<std::string> videos;
//     if (ts->m_yk_video.parse_video_files(videos)==0)
//     {
//         int i=0;
//         BOOST_FOREACH(std::string& video,videos)
//         {
//             yk_video_file_info v;
//             v.source=video;
//             v.index=i;
//             ts->m_videos.push_back(v);
//             ++i;
//         }
//     }
//     else
//         return -1;
//     
//     
//     std::vector<yk_video_file_info> vfi = ts->video_list();
// 
//     yk_video_file_info &info = vfi.at(0);
// 
//     strcpy(name, info.source.c_str());
//     *size=1;//videos.size();
// 
//     return videos.size();
#else
    return -1;
#endif // USE_YK
}

EXPORT_API int64_t yk_read_data(struct source_context *ctx, char* buff, size_t buf_size)
{
#ifdef USE_YK
//     source_context *sc = (source_context*)ctx;
//     torrent_source *ts = (torrent_source*)sc->io_dev;
	size_t readbytes = 0;
// 
//     if (!ts->read_data(buff, offset, buf_size, readbytes))
//         return -1;
// 
	return readbytes;
#else
	return -1;
#endif // USE_YK
}

EXPORT_API int64_t yk_read_seek(struct source_context *ctx, int64_t offset, int whence)
{
#ifdef USE_YK
//     source_context *sc = (source_context*)ctx;
//     torrent_source *ts = (torrent_source*)sc->io_dev;
// 
//     // 如果返回true, 则表示数据不够, 需要缓冲.
//     if (ts->read_seek(offset, whence))
//     {
//         printf("!!!!!!!!!!! data is not enough: %lld, whence: %d !!!!!!!!!!!\n", offset, whence);
//         sc->info.not_enough = 1;
//     }
// 
//     // 此处的返回值无意义.
    return 0;

#else
    return -1;
#endif // USE_YK
}

EXPORT_API void yk_close(struct source_context *ctx)
{
#ifdef USE_YK
//     source_context *sc = (source_context*)ctx;
//     torrent_source *ts = (torrent_source*)sc->io_dev;
//     ts->close();
#endif // USE_YK
}

EXPORT_API void yk_destory(struct source_context *ctx)
{
#ifdef USE_YK
//     source_context *sc = (source_context*)ctx;
//     torrent_source *ts = (torrent_source*)sc->io_dev;
//     ts->close();
//     delete ts;
#endif // USE_YK
}

#ifdef  __cplusplus
}
#endif

