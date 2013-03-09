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

// 文件数据源.
EXPORT_API int file_init_source(struct source_context *ctx);
EXPORT_API int64_t file_read_data(struct source_context *ctx, char* buff, size_t buf_size);
EXPORT_API int64_t file_read_seek(struct source_context *ctx, int64_t offset, int whence);
EXPORT_API void file_close(struct source_context *ctx);
EXPORT_API void file_destory(struct source_context *ctx);

// bt数据源.
EXPORT_API int bt_init_source(struct source_context *ctx);
EXPORT_API int64_t bt_read_data(struct source_context *ctx, char* buff, size_t buf_size);
EXPORT_API int64_t bt_read_seek(struct source_context *ctx, int64_t offset, int whence);
EXPORT_API void bt_close(struct source_context *ctx);
EXPORT_API void bt_destory(struct source_context *ctx);

// youku数据源.
EXPORT_API int yk_init_source(struct source_context *ctx);
EXPORT_API int yk_media_info(struct source_context *ctx, char *name, int64_t *pos, int64_t *size);
EXPORT_API int64_t yk_read_data(struct source_context *ctx, char* buff, size_t buf_size);
EXPORT_API int64_t yk_read_seek(struct source_context *ctx, int64_t offset, int whence);
EXPORT_API void yk_close(struct source_context *ctx);
EXPORT_API void yk_destory(struct source_context *ctx);

#ifdef  __cplusplus
}
#endif


#endif // __SOURCE_H__
