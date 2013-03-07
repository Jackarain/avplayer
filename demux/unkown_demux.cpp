#include "internal.h"
#include "unkown_demux.h"

unkown_demux::unkown_demux(void)
{
}

unkown_demux::~unkown_demux(void)
{
}

bool unkown_demux::open(boost::any ctx)
{
	return false;
}

bool unkown_demux::read_packet(AVPacket *pkt)
{
	return false;
}

bool unkown_demux::seek_packet(int64_t timestamp)
{
	return false;
}

bool unkown_demux::stream_index(enum AVMediaType type, int &index)
{
	return false;
}

bool unkown_demux::query_avcodec_id(int index, enum AVCodecID &codec_id)
{
	return false;
}

void unkown_demux::close()
{
}
