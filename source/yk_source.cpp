#include "yk_source.h"
#include "source.h"

yk_source::yk_source(void)
	: m_abort(false)
	, m_reset(false)
{
}

yk_source::~yk_source(void)
{
}

bool yk_source::open(boost::any ctx)
{
	// 保存打开参数信息参数.
	m_open_data = boost::any_cast<open_yk_data>(ctx);
	// 打开youku下载器, 质量自动选择高质量.
	if (!m_yk_video.open(m_open_data.url, m_open_data.save_path, libyk::high_quality))
		return false;
	return true;
}

bool yk_source::read_data(char* data, size_t size, size_t& read_size)
{
	// 暂无实现.
	return false;
}

int64_t yk_source::read_seek(uint64_t offset, int whence)
{
	return -1;
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

int yk_source::youku_video_list_size()
{
	libyk::duration_info d = m_yk_video.current_duration_info();
	return d.size();
}

bool yk_source::youku_video_list(struct youku_video *l)
{
	libyk::duration_info d = m_yk_video.current_duration_info();
	if (d.size() == 0)
		return false;
	int index = 0;
	for (libyk::duration_info::iterator i = d.begin();
		i != d.end(); i++)
	{
		l->index = i->first;
		l->duration = i->second;
		l++;
	}
	return true;
}
