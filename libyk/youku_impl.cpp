#include <cstddef>
#include "utf8.h"
#include "youku_impl.h"

namespace libyk {

youku_impl::youku_impl(void)
	: m_http_stream(m_io_service)
{
	// 设置为ssl不认证模式.
	m_http_stream.check_certificate(false);
}

youku_impl::~youku_impl(void)
{
}

bool youku_impl::parse_url(const std::string& url)
{
	std::string prefix_youku_url = "http://v.youku.com/v_show/id_";
	const int vid_length = 13;

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

	// 得到视频id.
    m_vid = vid;

    return true;
}

bool youku_impl::parse_video_files(std::vector<std::string> &videos, const std::string &password)
{
    if (m_vid.empty())
        return -1;

	std::string prefix_query_url =
		"https://openapi.youku.com/v2/videos/files.json?"
		"client_id=e57bc82b1a9dcd2f&"
		"client_secret=a361608273b857415ee91a8285a16b4a&video_id=";

	// 添加id.
    std::string query = prefix_query_url + m_vid;

	// 添加passwd.
    query += password.empty() ? "" : "&watch_password=" + password;

	// 打开连接开始请求.
	boost::system::error_code ec;
	m_http_stream.open(query, ec);
	if (ec && ec != boost::asio::error::eof)
	{
		std::cerr << ec.message().c_str() << std::endl;
		// 查询url失败.
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

	// 转为宽字符串.
	std::wstring utf = utf8_wide(oss.str());
	std::wstringstream stream;
	stream << utf;

	// 解析json字符串.
	boost::property_tree::wptree root;
	try {
		boost::property_tree::read_json<boost::property_tree::wptree>(stream, root);
		try {
			boost::property_tree::wptree errinfo = root.get_child(L"error");
			int err = errinfo.get<int>(L"code");
			// 输出json中包含的错误代码.
			std::cerr << "error code: " << err << std::endl;
			return false;
		}
		catch (std::exception &)
		{}

		// 得到文件表.
		boost::property_tree::wptree files = root.get_child(L"files");
		boost::property_tree::wptree type;

		// 说明: 在得到对应的视频文件表后, 然后解析segs表, 在这个表中, 包含了视频分段信息.
		// 注意m3u8是没有视频分段信息的, 它只有一个m3u8的地址url+duration信息. ok, 在
		// 得到了这些信息后, 我们就可以按此下载数据提供给播放器播放, so, 现在要做的是解析
		// 他们.

		// 得到hd2文件表.

		// 得到mp4文件表.

		// 得到3gp文件表.

		// 得到3gphd文件表.

		// 得到flv文件表.

		// 得到m3u8文件表.

	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return false;
	}

	// boost::property_tree::parse_json();

	// 为了编译通过!!
	return false;

	// 查询.

//     curl p;
//     boost::property_tree::wptree root;
//     if (!parse_json(p.curl_send_request(query), root))
//         return -1;

//     boost::property_tree::wptree files=root.get_child(L"files");
// 
//     boost::property_tree::wptree type;

    // 暂时不播放高清和超清视频
    /*
    try
    {
        type=files.get_child(L"hd2");
        BOOST_FOREACH(boost::property_tree::wptree::value_type& v,type.get_child(L"segs"))
        {
            boost::property_tree::wptree value=v.second;
            p.detail[index-1]->hd2.push_back(codepage::w2utf(value.get<std::wstring>(L"url")));
        }
        ret++;
    }
    catch(...)
    {

    }

    try
    {
        type=files.get_child(L"mp4");
        BOOST_FOREACH(boost::property_tree::wptree::value_type& v,type.get_child(L"segs"))
        {
            boost::property_tree::wptree value=v.second;
            p.detail[index-1]->mp4.push_back(codepage::w2utf(value.get<std::wstring>(L"url")));
        }
        ret++;
    }
    catch(...)
    {

    }
    */
//     try
//     {
//         type=files.get_child(L"flv");
//         BOOST_FOREACH(boost::property_tree::wptree::value_type& v,type.get_child(L"segs"))
//         {
//             boost::property_tree::wptree value=v.second;
//             std::string relocation=location(codepage::w2utf(value.get<std::wstring>(L"url")));
//             if (!relocation.empty())
//                 videos.push_back(relocation);
//             else
//                 return -1;
//         }
//         return 0;
//     }
//     catch(...)
//     {
//         return -1;
//     }

}

/*
bool libykvideo::parse_json(const std::string& data,boost::property_tree::wptree &root)
{
    if (data.empty())
        return false;

    std::wstringstream stream;

    std::wstring utf=codepage::utf2w(data);

    stream<<utf;

    try
    {
        boost::property_tree::read_json<boost::property_tree::wptree>(stream,root);
    }
    catch(...)
    {
        return false;
    }

    try
    {
        boost::property_tree::wptree errinfo=root.get_child(L"error");
        int err=errinfo.get<int>(L"code");
        return false;
    }
    catch(...)
    {
        return true;
    }
}

std::string libykvideo::location(const std::string& url)
{
    curl p(true);
    std::string header=p.curl_send_request(url);
    boost::to_lower<std::string>(header);
    size_t pos=header.find("location: ")+1;
    if (!pos)
        return false;
    
    header=header.substr((pos-1)+strlen("location: "));
    pos=header.find("\r\n")+1;
    if (!pos)
        return false;
    return header.substr(0,pos-1);    
}

*/

}



