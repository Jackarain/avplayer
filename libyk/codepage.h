#pragma once
#include <string>
#include <boost/locale.hpp>

namespace codepage
{

    std::string utf2a(const std::string& source,const std::string& charset)
    {
        return boost::locale::conv::between(source,charset,"UTF-8");
    }

    std::wstring utf2w(const std::string& source)
    {
        return boost::locale::conv::utf_to_utf<wchar_t>(source);
    }

    std::string w2utf(const std::wstring& source)
    {
        return boost::locale::conv::utf_to_utf<char>(source);
    }

    std::string a2utf(const std::string& source,const std::string& charset)
    {
        return boost::locale::conv::between(source,"UTF-8",charset);
    }

    std::wstring a2w(const std::string& source,const std::string& charset)
    {
        return utf2w(a2utf(source,charset));
    }

    std::string w2a(const std::wstring& source,const std::string& charset)
    {
        return utf2a(w2utf(source),charset);
    }
}