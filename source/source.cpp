#include "ins.h"
#include "defs.h"
#include "source.h"
#include "av_source.h"
#include "file_source.h"

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

EXPORT_API int file_init_source(void *ctx)
{
	source_context *sc = (source_context*)ctx;
	if (sc->media_size <= 0 || !sc->media->name)
		return -1;

	file_source *fs = NULL;
	sc->io_dev = (void*)(fs = new file_source());

	// new open_file_data 由file_source内部管理内存.
	open_file_data *od = new open_file_data;
	od->filename = std::string(sc->media->name);
	od->is_multithread = false;

	return fs->open((void*)od) ? 0 : -1;
}

EXPORT_API int file_read_data(void *ctx, char* buff, int64_t offset, int buf_size)
{
	source_context *sc = (source_context*)ctx;
	file_source *fs = (file_source *)sc->io_dev;
	uint64_t read_size = 0;
	bool ret = fs->read_data(buff, offset, buf_size, read_size);
	return ret ? read_size : -1;
}

EXPORT_API void file_close(void *ctx)
{
	source_context *sc = (source_context*)ctx;
	file_source *fs = (file_source *)sc->io_dev;
	fs->close();
}

EXPORT_API void file_destory(void *ctx)
{
	source_context *sc = (source_context*)ctx;
	file_source *fs = (file_source *)sc->io_dev;
	if (fs)
	{
		fs->close();
		delete fs;
		sc->io_dev = NULL;
	}
}

EXPORT_API int bt_init_source(void *ctx)
{
	return -1;
}

EXPORT_API int bt_media_info(void *ctx, char *name, int64_t *pos, int64_t *size)
{
	return -1;
}

EXPORT_API int bt_read_data(void *ctx, char* buff, int64_t offset, int buf_size)
{
	return -1;
}

EXPORT_API void bt_close(void *ctx)
{
}
EXPORT_API void bt_destory(void *ctx)
{
}

#ifdef  __cplusplus
}
#endif
