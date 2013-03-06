#include "yk_source.h"

yk_source::yk_source(void)
	: m_abort(false)
	, m_reset(false)
{
}

yk_source::~yk_source(void)
{
}

bool yk_source::open(void* ctx)
{
	return false;
}

bool yk_source::read_data(char* data, size_t size, size_t& read_size)
{
	// 暂无实现.
	return false;
}

bool yk_source::read_seek(uint64_t offset, int whence)
{
	return false;
}

void yk_source::close()
{
}

bool yk_source::set_current_video(int index)
{
	return false;
}

bool yk_source::get_current_video(open_yk_data& vfi) const
{
	return false;
}

std::vector<open_yk_data> yk_source::video_list() const
{
	return std::vector<open_yk_data>();
}

void yk_source::reset()
{

}

bool yk_source::parse_url(const std::string &url)
{
	return m_yk_video.parse_url(url);
}

