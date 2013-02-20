#include "libyk.h"
#include "codepage.h"
#include "curl.h"

namespace libyk{

libykvideo::libykvideo()
{

}
libykvideo::~libykvideo()
{

}
int libykvideo::parse_url(const std::string& url)
{
    std::string yk_url=url;
    size_t pos=yk_url.find("http://v.youku.com/v_show/id_")+1;
    if (!pos)
        return -1;
        
    std::string vid=yk_url.substr((pos-1)+strlen("http://v.youku.com/v_show/id_"));
    if (vid.length()>13)
        vid=vid.substr(0,13);
    else
        return -1;
    
    vid_=vid;
        
    return 0;
}

int libykvideo::parse_video_files(std::vector<std::string>& videos,const std::string& password)
{

    if (vid_.empty())
        return -1;
        
    std::string req="https://openapi.youku.com/v2/videos/files.json?client_id=e57bc82b1a9dcd2f&client_secret=a361608273b857415ee91a8285a16b4a&video_id="+vid_;

    req.append(password.empty()?"":"&watch_password="+password);

    curl p;
    boost::property_tree::wptree root;
    if (!parse_json(p.curl_send_request(req),root))
        return -1;

    boost::property_tree::wptree files=root.get_child(L"files");

    boost::property_tree::wptree type;

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
    try
    {
        type=files.get_child(L"flv");
        BOOST_FOREACH(boost::property_tree::wptree::value_type& v,type.get_child(L"segs"))
        {
            boost::property_tree::wptree value=v.second;
            std::string relocation=location(codepage::w2utf(value.get<std::wstring>(L"url")));
            if (!relocation.empty())
                videos.push_back(relocation);
            else
                return -1;
        }
        return 0;
    }
    catch(...)
    {
        return -1;
    }

}

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
        return "";
    
    header=header.substr((pos-1)+strlen("location: "));
    pos=header.find("\r\n")+1;
    if (!pos)
        return "";
    return header.substr(0,pos-1);    
}
}

