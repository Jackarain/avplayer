#include <cstddef>
#include <strstream>
#include <boost/crc.hpp>  // for boost::crc_32_type
#include <boost/thread/condition.hpp>
#include "youku_impl.h"

#ifndef AVSEEK_SIZE
#define AVSEEK_SIZE 0x10000
#endif

namespace libyk {

using boost::property_tree::wptree;

template <class Ptree, class Stream>
void dump_json(Ptree json, Stream &stream, int level = 0)
{
	if (level == 0)
	{
		level++;
		stream << "{\n";
	}
	int s = json.size();
	BOOST_FOREACH(Ptree::value_type &v, json)
	{
		std::string first = avhttp::detail::wide_utf8(v.first);
		if (v.second.size() == 0)
		{
			for (int i = 0; i < level; i++)
				stream << "  ";
			stream << "\"" << first << "\"" << ":" << "\"" << avhttp::detail::wide_utf8(v.second.get_value<std::wstring>()) << "\"";
			if (s != 1)
				stream << ",";
			stream << std::endl;
		}
		else
		{
			if (first != "")
			{
				for (int i = 0; i < level; i++)
					stream << "  ";
				stream << "\"" << first << "\"" << ":[" << std::endl;
			}
			else
			{
				for (int i = 0; i < level; i++)
					stream << "  ";
				stream << "{" << std::endl;
			}
			dump_json(v.second, stream, level + 1);
			if (first != "")
			{
				for (int i = 0; i < level; i++)
					stream << "  ";
				if (s != 1)
					stream << "]," << std::endl;
				else
					stream << "]" << std::endl;
			}
			else
			{
				for (int i = 0; i < level; i++)
					stream << "  ";
				if (s != 1)
					stream << "}," << std::endl;
				else
					stream << "}" << std::endl;
			}
		}
		s--;
	}
	if (level == 1)
	{
		stream << "}";
	}
}

youku_impl::youku_impl(void)
	: m_http_stream(m_io_service)
	, m_timer(m_io_service)
	, m_offset(0)
	, m_current_index(0)
	, m_quality(normal_quality)
{
#ifdef _DEBUG
	INIT_LOGGER(".", "youku.log");
#endif
	// 设置为ssl不认证模式.
	m_http_stream.check_certificate(false);
}

youku_impl::~youku_impl(void)
{
}

bool youku_impl::open(const std::string &url,
	std::string save_path/* = "."*/, video_quality quality/* = normal_quality*/)
{
	std::string prefix_youku_url = "http://v.youku.com/v_show/id_";
	const int vid_length = 13;

	m_quality = quality;
	m_url = url;
	m_abort = false;

	// 检查url是否是youku的视频链接.
	std::string::size_type pos = url.find(prefix_youku_url);
	if (pos == std::string::npos)
		return false;

	// 得到视频id.
    std::string vid = url.substr(pos + prefix_youku_url.length());
	if (vid.length() >= vid_length)
		vid = vid.substr(0, vid_length);
	else
		return false;

	// 下载视频文件列表json.
	std::string prefix_query_url =
		"https://openapi.youku.com/v2/videos/files.json?"
		"client_id=none&"
		"client_secret=none&video_id=";

	// 添加id.
	std::string query = prefix_query_url + vid;
	// 添加passwd.
	std::string password = "";
	query += password.empty() ? "" : "&watch_password=" + password;

	// 发起请求.
	avhttp::request_opts opt;
	opt.insert(avhttp::http_options::user_agent,
		"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.64 Safari/537.31");
	opt.insert(avhttp::http_options::referer, query);
	opt.insert("Accept-Language", "Accept-Language: zh-CN,zh;q=0.8");
	opt.insert("Accept-Charset", "gb18030,utf-8;q=0.7,*;q=0.3");

	boost::system::error_code ec;
	m_http_stream.open(query, ec);
	if (ec)
	{
		// 查询json失败.
		return false;
	}

	// 请求json字符串, 然后解析.
	boost::asio::streambuf response;
	std::ostringstream oss;

	while (boost::asio::read(m_http_stream,
		response, boost::asio::transfer_at_least(1), ec))
	{
		oss << &response;
	}
	// 关闭http链接.
	m_http_stream.close(ec);

	// 转为宽字符串流.
	std::wstring utf = avhttp::detail::utf8_wide(oss.str());
	std::wstringstream stream;
	stream << utf;

	// 解析json字符串.
	wptree root;
	try {
		boost::property_tree::read_json<wptree>(stream, root);
		try {
			wptree errinfo = root.get_child(L"error");
			int err = errinfo.get<int>(L"code");
			// 输出json中包含的错误代码.
			std::cerr << "error code: " << err << std::endl;
			return false;
		}
		catch (std::exception &)
		{}

		// 得到文件表.
		wptree files = root.get_child(L"files");

		// std::fstream file;
		// file.open("dump.js", std::ios::trunc|std::ios::binary|std::ios::out);
		// dump_json(files, file);

		BOOST_FOREACH(wptree::value_type &v, files)
		{
			video_info info;
			video_type vq;

			if (v.first == L"hd2")			// 高清.
			{
				vq = hd2;
			}
			else if (v.first == L"mp4")		// 普通质量.
			{
				vq = mp4;
			}
			else if (v.first == L"flv")
			{
				vq = flv;
			}
			else if (v.first == L"3gphd")
			{
				vq = gp3hd;
			}
			else if (v.first == L"m3u8")
			{
				vq = m3u8;
			}

			if (v.second.size() != 0)
			{
				BOOST_FOREACH(wptree::value_type &v, v.second)
				{
					if (v.first == L"duration")
					{
						std::string duration = avhttp::detail::wide_utf8(v.second.get_value<std::wstring>());
						info.duration = boost::lexical_cast<float>(duration);
					}

					if (v.second.size() != 0)
					{
						BOOST_FOREACH(wptree::value_type &v, v.second)
						{
							video_clip clip;
							BOOST_FOREACH(wptree::value_type &v, v.second)
							{
								if (v.first == L"duration")
								{
									std::string duration = avhttp::detail::wide_utf8(v.second.get_value<std::wstring>());
									clip.duration = boost::lexical_cast<float>(duration);
								}
								else if (v.first == L"no")
								{
									clip.id = v.second.get_value<int>();
								}
								else if (v.first == L"size")
								{
									clip.filesize = v.second.get_value<int>();
								}
								else if (v.first == L"url")
								{
									clip.url = avhttp::detail::wide_utf8(v.second.get_value<std::wstring>());
								}
							}

							info.fs.push_back(clip);
						}
					}
				}
			}

			// 保存到容器.
			m_video_group.insert(std::make_pair(vq, info));
		}

		// 得到当前需要下载的视频url.
		if (m_video_group.find(query_quality()) == m_video_group.end())
		{
			return false;
		}

		async_request_youku();

		// 启动io线程.
		m_work_thread = boost::thread(boost::bind(&youku_impl::io_service_thread, this));
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}

    return true;
}

bool youku_impl::read_data(char* data, std::size_t size, std::size_t &read_size)
{
	boost::condition cond;
	boost::mutex::scoped_lock lock(m_mutex);

	// 如果停止, 返回false, 表示读取失败.
	if (m_abort)
		return false;

	// 通知读取数据.
	boost::asio::mutable_buffers_1 bufs = boost::asio::buffer(data, size);
	m_io_service.post(boost::bind(
		&youku_impl::handle_read_data<boost::asio::mutable_buffers_1>, this,
		boost::ref(cond), boost::cref(bufs), boost::ref(read_size)));

	// 等待完成操作.
	cond.wait(lock);

	return false;
}

template <typename MutableBufferSequence>
void youku_impl::handle_read_data(boost::condition &cond,
	const MutableBufferSequence &buffers, std::size_t &read_size)
{
	// 如果http下载组件有效.
	if (m_multi_http)
	{
		read_size = m_multi_http->fetch_data(buffers, m_offset);
	}
	// 通知已经读取到数据.
	cond.notify_one();
}

void youku_impl::io_service_thread()
{
	while (!m_io_service.stopped())
	{
		m_io_service.run_one();

		if (m_abort)
			break;
	}
}

void youku_impl::stop()
{
	m_abort = true;
	if (m_work_thread.joinable())
	{
		m_work_thread.join();
	}
}

bool youku_impl::wait_for_complete()
{
	m_work_thread.join();
	if (m_abort == true)
		return false;
	return true;
}

void youku_impl::async_request_youku()
{
	video_info &info = m_video_group[query_quality()];

	std::string query;
	for (std::vector<video_clip>::iterator i = info.fs.begin();
		i != info.fs.end(); i++)
	{
		if (i->id >= m_current_index)
		{
			if (i->state != complete_state)	// 如果是stop状态, 继续下载.
			{
				i->state = start_state;
				query = i->url;
				m_current_index = i->id;
				m_video_info = &info.fs;
				break;
			}
		}
	}

	if (query.empty())
	{
		return;
	}

	avhttp::settings set;

	// 不允许使用meta中保存的url链接, 因为meta中保证的url会过期.
	set.allow_use_meta_url = false;

	// 生成唯一的meta文件名.
	boost::crc_32_type result;
	result.process_bytes(m_url.c_str(), m_url.size());
	std::stringstream ss;
	ss.imbue(std::locale("C"));
	ss << std::hex << result.checksum() << m_current_index << ".meta";
	set.meta_file = ss.str();

	// 设置request_opts.
	avhttp::request_opts &opt = set.opts;

	opt.insert(avhttp::http_options::user_agent,
		"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.31 (KHTML, like Gecko) Chrome/26.0.1410.64 Safari/537.31");
	opt.insert(avhttp::http_options::referer, m_url);
	opt.insert("Accept-Language", "Accept-Language: zh-CN,zh;q=0.8");
	opt.insert("Accept-Charset", "gb18030,utf-8;q=0.7,*;q=0.3");

	// 创建新的对象.
	if (!m_multi_http)
	{
		m_multi_http.reset(new avhttp::multi_download(m_io_service));
	}
	else
	{
		m_multi_http->stop();
		m_multi_http.reset(new avhttp::multi_download(m_io_service));
	}

	// 异步调用开始下载.
	m_multi_http->async_start(query, set,
		boost::bind(&youku_impl::handle_check_download,
			this,
			boost::asio::placeholders::error
		)
	);
}

void youku_impl::handle_check_download(const boost::system::error_code &ec)
{
	if (ec)
	{
		// 继续请求下一个视频.
		async_request_youku();
		return;
	}

	// 检查是否下载完成.
	boost::int64_t file_size = m_multi_http->file_size();
	boost::int64_t bytes_download = m_multi_http->bytes_download();

	// 服务已经停止, 重新启动任务, 开始请求时间.
	if (m_multi_http->stopped())
	{
		if (file_size != bytes_download) // 说明出了问题!!!
		{
			// 改变为停止状态, 以便下一次继续下载, 完成整个视频下载.
			m_video_info->at(m_current_index).state = stop_state;
			LOG_DEBUG("糟糕, 视频下载未完成, 出错了!");
		}
		else
		{
			m_video_info->at(m_current_index).state = complete_state;
			LOG_DEBUG("恭喜, 一个视频下载完成!");
		}

		// 继续请求下一个视频.
		async_request_youku();
		return;
	}

	// 在这里启动定时器, 检查是否下载完成.
	m_timer.expires_from_now(boost::posix_time::seconds(1));
	m_timer.async_wait(boost::bind(&youku_impl::handle_check_download,
		this, boost::asio::placeholders::error));
}

video_type youku_impl::query_quality()
{
	if (m_quality == high_quality)
	{
		if (m_video_group.find(hd2) != m_video_group.end())
		{
			return hd2;
		}
	}
	else if (m_quality == normal_quality)
	{
		if (m_video_group.find(mp4) != m_video_group.end())
		{
			return mp4;
		}
	}
	else if (m_quality == low_quality)
	{
		if (m_video_group.find(flv) != m_video_group.end())
		{
			return flv;
		}
		if (m_video_group.find(gp3hd) != m_video_group.end())
		{
			return gp3hd;
		}
	}
	else if (m_quality == mobile_quality)
	{
		if (m_video_group.find(m3u8) != m_video_group.end())
		{
			return m3u8;
		}
	}

	return m_video_group.begin()->first;
}

bool youku_impl::change_download(int index)
{
	boost::condition cond;
	boost::mutex::scoped_lock lock(m_mutex);

	// 如果下载index没有发生改变, 则返回false.
	if (index == m_current_index)
		return false;

	// 通知下载位置改变.
	m_io_service.post(boost::bind(&youku_impl::handle_change_download, this, boost::ref(cond), index));

	// 等待完成操作.
	cond.wait(lock);

	return true;
}

void youku_impl::handle_change_download(boost::condition &cond, int index)
{
	// 保存需要下载的index, 重置读取数据偏移为0.
	m_current_index = index;
	m_offset = 0;

	// 异步发起请求.
	async_request_youku();

	// 通知change完成.
	cond.notify_one();
}

duration_info youku_impl::current_duration_info()
{
	duration_info ret;
	video_group::iterator f;

	f = m_video_group.find(query_quality());
	if (f == m_video_group.end())
		return ret;
	std::vector<video_clip> &fs = f->second.fs;
	for (std::vector<video_clip>::iterator i = fs.begin();
		i != fs.end(); i++)
	{
		// 将视频文件id和对应的时长信息插件到容口.
		ret.insert(std::make_pair(i->id, i->duration));
	}

	return ret;
}

boost::int64_t youku_impl::read_seek(boost::uint64_t offset, int whence)
{
	boost::condition cond;
	boost::mutex::scoped_lock lock(m_mutex);

	// 通知下载位置改变.
	m_io_service.post(boost::bind(&youku_impl::handle_read_seek,
		this, boost::ref(cond), boost::ref(offset), whence));

	// 等待完成操作.
	cond.wait(lock);

	return offset;
}

void youku_impl::handle_read_seek(boost::condition &cond, boost::uint64_t &offset, int whence)
{
	if (!m_multi_http)
	{
		switch (whence)
		{
		case SEEK_SET:		// 文件起始位置计算.
			{
				// 只更新下载地址.
				m_offset = offset;
				char buf[1];
				m_multi_http->fetch_data(boost::asio::buffer(&buf[0], 0), m_offset);
			}
			break;
		case SEEK_CUR:		// 文件指针当前位置开始计算.
			{
				m_offset += offset;
				char buf[1];
				m_multi_http->fetch_data(boost::asio::buffer(&buf[0], 0), m_offset);
				offset = m_offset;
			}
			break;
		case SEEK_END:		// 文件尾开始计算.
			{
				m_offset = m_multi_http->file_size() - offset;
				char buf[1];
				m_multi_http->fetch_data(boost::asio::buffer(&buf[0], 0), m_offset);
				offset = m_offset;
			}
			break;
		case AVSEEK_SIZE:	// 文件大小.
			{
				offset = m_multi_http->file_size();
			}
			break;
		}
	}

	cond.notify_one();
}

}


