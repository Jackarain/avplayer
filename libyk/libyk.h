#pragma once

#include <vector>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#ifdef _MSVC
#ifdef SOURCE_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

namespace libyk
{
    class EXPORT_API libykvideo
    {
    public:
        libykvideo();
        ~libykvideo();
    public:
        int         parse_url(const std::string& url);
        int         parse_video_files(std::vector<std::string>& videos,const std::string& password="");
    private:
        bool        parse_json(const std::string& data,boost::property_tree::wptree &root);
        std::string location(const std::string& url);
        
    private:
        std::string vid_;
    };  
}

