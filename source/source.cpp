#include "ins.h"
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

EXPORT_API int file_init_source(void **ctx, char *data, int len, char *save_path)
{
	file_source *fs = new file_source();
	*ctx = (void*)fs;
	open_file_data *od = new open_file_data;
	od->filename = data;
	od->is_multithread = false;
	return fs->open((void*)od) ? 0 : -1;
}

EXPORT_API int file_read_data(void *ctx, char* buff, int64_t offset, int buf_size)
{
	file_source *fs = (file_source *)ctx;
	boost::uint64_t read_size = 0;
	bool ret = fs->read_data(buff, offset, buf_size, read_size);
	return ret ? read_size : -1;
}

EXPORT_API void file_destory(void *ctx)
{
	file_source *fs = (file_source *)ctx;
	fs->close();
	delete fs;
}

EXPORT_API int bt_init_source(void **ctx, char *data, int len, char *save_path)
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

EXPORT_API void bt_destory(void *ctx)
{
}

#ifdef  __cplusplus
}
#endif
