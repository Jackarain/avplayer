#include "curl.h"

size_t curl::write_memcb_(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;

    curl* pcurl=(curl*)userp;

    pcurl->sPtr_.append((const char*)contents,realsize);

    return realsize;
}

curl::curl(bool header,bool use_ssl):curl_(0),timeout_(99999),curl_code_(CURLE_OK)
{
    curl_=curl_easy_init();
    if (curl_)
    {
        if (header)
            curl_easy_setopt(curl_,CURLOPT_HEADER, 1L);
            
        if (use_ssl)
        {
            curl_easy_setopt(curl_,CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl_,CURLOPT_SSL_VERIFYHOST, 0L);
        }
    }

}

curl::~curl()
{
    if (curl_)
        curl_easy_cleanup(curl_);
}

std::string curl::curl_send_request(const std::string &url,const void* bd,size_t len,bool rst)
{

    int retry=CURL_MAX_RETRY_COUNT;

    if (rst)
        curl_easy_reset(curl_);

    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_memcb_);

    curl_easy_setopt(curl_, CURLOPT_WRITEDATA,this);

    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, timeout_);

    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_);

    if ( len && bd )
    {
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, bd);
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, len);
    }

    sPtr_.clear();

    do 
    {
        curl_code_ = curl_easy_perform(curl_);
        if (curl_code_!=CURLE_OK)
        {
            retry--;
            continue;
        }else
            break;
    } while (retry);

    return sPtr_;
}

CURLcode curl::curl_get_errcode()
{
    return curl_code_;
}