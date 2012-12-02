//
// source.h
// ~~~~~~~~
//
// Copyright (c) 2011 Jack (jack.wgm@gmail.com)
//

#ifndef __SOURCE_H__
#define __SOURCE_H__
#ifdef _MSC_VER
#ifdef SOURCE_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

#ifdef  __cplusplus
extern "C" {
#endif

EXPORT_API int file_init_source(void *ctx);
EXPORT_API int64_t file_read_data(void *ctx, char* buff, int64_t offset, size_t buf_size);
EXPORT_API void file_close(void *ctx);
EXPORT_API void file_destory(void *ctx);

EXPORT_API int bt_init_source(void *ctx);
EXPORT_API int bt_media_info(void *ctx, char *name, int64_t *pos, int64_t *size);
EXPORT_API int64_t bt_read_data(void *ctx, char* buff, int64_t offset, size_t buf_size);
EXPORT_API int64_t bt_read_seek(void *ctx, int64_t offset, int whence);
EXPORT_API void bt_close(void *ctx);
EXPORT_API void bt_destory(void *ctx);

#ifdef  __cplusplus
}
#endif


#endif // __SOURCE_H__
