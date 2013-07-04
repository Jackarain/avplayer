#include "libyk.h"
#include "youku_impl.h"

namespace libyk {

// youku的具体实现.
youku::youku()
	: m_impl(NULL)
{
	m_impl = new youku_impl();
}

youku::~youku()
{
	delete m_impl;
}

bool youku::open(const std::string &url, std::string save_path /*= "."*/, video_quality quality /*= normal_quality*/)
{
	return m_impl->open(url, save_path, quality);
}

void youku::stop()
{
	m_impl->stop();
}

bool youku::wait_for_complete()
{
	return m_impl->wait_for_complete();
}

duration_info youku::current_duration_info()
{
	return m_impl->current_duration_info();
}

bool youku::change_download(int index)
{
	return m_impl->change_download(index);
}

bool youku::read_data(char* data, std::size_t size, std::size_t &read_size)
{
	return m_impl->read_data(data, size, read_size);
}

boost::int64_t youku::read_seek(boost::uint64_t offset, int whence)
{
	return m_impl->read_seek(offset, whence);
}

}
