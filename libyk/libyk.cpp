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

}
