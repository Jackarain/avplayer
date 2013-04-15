#include <string.h>

#include "internal.h"
#include "file_source.h"


#ifndef AVSEEK_SIZE
#define AVSEEK_SIZE 0x10000
#endif

file_source::file_source()
{}

file_source::~file_source()
{
	close();
	if (m_open_data)
		delete m_open_data;
}

bool file_source::open(boost::any ctx)
{
	// 保存ctx.
	m_open_data = boost::any_cast<open_file_data*>(ctx);

	// 文件是否存在.
	if (!boost::filesystem::exists(m_open_data->filename))
		return false;

	// 打开文件.
	m_file.open(m_open_data->filename, std::ios::binary|std::ios::in);
	if (!m_file.is_open())
		return false;

	return true;
}

bool file_source::read_data(char* data, size_t size, size_t &read_size)
{
	// 根据参数加锁.
	if (m_open_data->is_multithread)
		m_mutex.lock();

	if (!m_file)
	{
		if (m_open_data->is_multithread)
			m_mutex.unlock();
		return false;
	}

	// 开始读取数据.
	m_file.read(data, size);
	read_size = m_file.gcount();

	if (m_open_data->is_multithread)
		m_mutex.unlock();

	return true;
}

int64_t file_source::read_seek(uint64_t offset, int whence)
{
	// 参数检查.
	int64_t ret = boost::filesystem::file_size(m_open_data->filename);

	if (offset > ret || !m_file.is_open())
		return false;

	if (m_open_data->is_multithread)
		m_mutex.lock();

	std::ios_base::seekdir way = (std::ios_base::seekdir)whence;

	// 进行seek操作.
	if (whence != AVSEEK_SIZE)
	{
		m_file.clear();
		m_file.seekg(offset, way);
		if (m_file.good())
			ret = 0;
		else
			ret = -1;
	}

	if (m_open_data->is_multithread)
		m_mutex.unlock();

	return ret;
}

void file_source::close()
{
	if (m_file.is_open())
		m_file.close();
}
