#include "libyk.h"
#include "youku_impl.h"

namespace libyk {

youku::youku()
	: m_impl(NULL)
{
	m_impl = new youku_impl();
}

youku::~youku()
{
	delete m_impl;
}

bool youku::parse_url(const std::string &url)
{
	return m_impl->parse_url(url);
}

bool youku::parse_video_files(std::vector<std::string> &videos, const std::string &password /*= ""*/)
{
	return m_impl->parse_video_files(videos, password);
}

}
