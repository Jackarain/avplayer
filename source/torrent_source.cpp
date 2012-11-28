#include "ins.h"

#ifdef USE_TORRENT

#include <boost/filesystem.hpp>
#include "torrent_source.h"
#include "libtorrent/escape_string.hpp"
#include "libtorrent/fingerprint.hpp"

#ifdef WIN32
// #pragma comment(lib, "../openssl/libs/libeay32.lib");
// #pragma comment(lib, "../openssl/libs/ssleay32.lib");
#endif // WIN32


torrent_source::torrent_source()
	: m_abort(false)
	, m_reset(false)
{
	m_current_video.index = -1;
}

torrent_source::~torrent_source()
{
	close();
}

bool torrent_source::open(void* ctx)
{
	if (!ctx)
		return false;

	// 保存打开指针.
	m_open_data.reset((open_torrent_data*)ctx);

	// 开启下载对象.
	add_torrent_params p;

	p.save_path = m_open_data->save_path;
	error_code ec;

	// 打开种子, 开始下载.
	if (m_open_data->is_file)
	{
		p.ti = new torrent_info(m_open_data->filename, ec);
		if (ec)
		{
			printf("%s\n", ec.message().c_str());
			return false;
		}
	}

	// 非文件的种子, 直接使用种子数据打开torrent.
	if (!m_open_data->is_file)
	{
		p.ti = new torrent_info((const char*)m_open_data->torrent_data.get(), 
			m_open_data->data_size, ec);
		if (ec)
		{
			printf("%s\n", ec.message().c_str());
			return false;
		}
	}

	int index = 0;
	// 遍历视频文件.
	file_storage fs = p.ti->files();
	for (file_storage::iterator i = fs.begin();
		i != fs.end(); i++)
	{
		boost::filesystem::path p(convert_to_native(i->filename()));
		std::string ext = p.extension().string();
		if (ext == ".rmvb" ||
			ext == ".wmv" ||
			ext == ".avi" ||
			ext == ".mkv" ||
			ext == ".flv" ||
			ext == ".rm" ||
			ext == ".mp4" ||
			ext == ".3gp" ||
			ext == ".webm" ||
			ext == ".mpg")
		{
			video_file_info vfi;
			vfi.filename = convert_to_native(i->filename());
			vfi.base_offset = i->offset;
			vfi.data_size = i->size;
			vfi.index = index++;
			vfi.status = 0;

			// 当前视频默认置为第一个视频.
			if (m_current_video.index == -1)
				m_current_video = vfi;

			// 保存到视频列表中.
			m_videos.push_back(vfi);
		}
	}

	m_session.add_dht_router(std::make_pair(
		std::string("router.bittorrent.com"), 6881));
	m_session.add_dht_router(std::make_pair(
		std::string("router.utorrent.com"), 6881));
	m_session.add_dht_router(std::make_pair(
		std::string("router.bitcomet.com"), 6881));

	m_session.start_dht();

	//    m_session.load_asnum_db("GeoIPASNum.dat");
	//    m_session.load_country_db("GeoIP.dat");

	m_session.listen_on(std::make_pair(6881, 6889));

	// 设置缓冲.
	session_settings settings = m_session.settings();
	settings.use_read_cache = false;
	settings.disk_io_read_mode = session_settings::disable_os_cache;
	settings.broadcast_lsd = true;
	settings.allow_multiple_connections_per_ip = true;
	settings.local_service_announce_interval = 15;
	settings.min_announce_interval = 20;
	m_session.set_settings(settings);

	// 添加到session中.
	m_torrent_handle = m_session.add_torrent(p, ec);
	if (ec)
	{
		printf("%s\n", ec.message().c_str());
		return false;
	}

	m_torrent_handle.force_reannounce();

	// 自定义播放模式下载.
	m_torrent_handle.set_user_defined_download(true);

	// 限制上传速率为测试.
	// m_session.set_upload_rate_limit(80 * 1024);
	// m_session.set_local_upload_rate_limit(80 * 1024);

	// 创建bt数据读取对象.
	m_read_op.reset(new extern_read_op(m_torrent_handle, m_session));
	m_abort = false;

	return true;
}

bool torrent_source::read_data(char* data, uint64_t offset, size_t size, size_t& read_size)
{
	if (!m_read_op || !data || m_videos.size() == 0)
		return false;

	bool ret = false;
	int piece_offset = 0;

	// 此处有bug, 必须在read_data函数退出后, 才能destroy.
	read_size = 0;
	m_reset = false;

	// 读取数据越界.
	if (offset >= m_current_video.data_size)
		return false;

	// 修正偏移.
	offset += m_current_video.base_offset;
	const torrent_info& info = m_torrent_handle.get_torrent_info();
	piece_offset = offset / info.piece_length();

	boost::mutex::scoped_lock lock(m_abort_mutex);
	// 读取数据.
	try
	{
		while (!m_abort && !m_reset)
		{
			boost::int64_t rs = 0;
			ret = m_read_op->read_data(data, offset, size, rs);
			if (ret)
			{
				read_size = rs;
				break;
			}
			else
			{
				boost::this_thread::sleep(boost::posix_time::millisec(100));
			}

			if (m_reset)
				return true;
		}
	}
	catch (boost::thread_interrupted& e)
	{
		printf("read thread is interrupted!\n");
		return false;
	}
	catch (...)
	{
		printf("read thread is interrupted!\n");
		return false;
	}

	return ret;
}

void torrent_source::close()
{
	m_abort = true;
	boost::mutex::scoped_lock lock(m_abort_mutex);
	m_open_data.reset();
	m_read_op.reset();
}

bool torrent_source::set_current_video(int index)
{
	// 检查是否初始化及参数是否有效.
	if (!m_open_data)
		return false;
	if (index >= m_videos.size() || index < 0)
		return false;
	// 设置为当前视频.
	m_current_video = m_videos[index];

	return true;
}

bool torrent_source::get_current_video(video_file_info& vfi) const
{
	if (!m_open_data)
		return false;
	// 设置为当前视频.
	vfi = m_current_video;

	return true;
}

void torrent_source::reset()
{
	m_reset = true;
}

#endif // USE_TORRENT
