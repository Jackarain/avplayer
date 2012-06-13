/*

Copyright (c) 2003, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_INTERFACE_HPP_INCLUDED
#define TORRENT_INTERFACE_HPP_INCLUDED

#include "libtorrent/torrent_handle.hpp"
#include "libtorrent/escape_string.hpp"
#include "libtorrent/peer_info.hpp"
#include <boost/lambda/lambda.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/filesystem.hpp>

namespace libtorrent
{
   // 一个简单的扩展读取操作的封装, 线程操作安全.
   class TORRENT_EXPORT extern_read_op : public boost::noncopyable
   {
   public:
      extern_read_op(torrent_handle& h, session &s)
         : m_handle(h)
			, m_ses(s)
         , m_current_buffer(NULL)
         , m_read_offset(0)
         , m_read_size(NULL)
         , m_request_size(0)
         , m_is_reading(false)
      { }

      ~extern_read_op() { }


		char const* esc(char const* code)
		{
#ifdef ANSI_TERMINAL_COLORS
			// this is a silly optimization
			// to avoid copying of strings
			enum { num_strings = 200 };
			static char buf[num_strings][20];
			static int round_robin = 0;
			char* ret = buf[round_robin];
			++round_robin;
			if (round_robin >= num_strings) round_robin = 0;
			ret[0] = '\033';
			ret[1] = '[';
			int i = 2;
			int j = 0;
			while (code[j]) ret[i++] = code[j++];
			ret[i++] = 'm';
			ret[i++] = 0;
			return ret;
#else
			return "";
#endif
		}

		int peer_index(libtorrent::tcp::endpoint addr, std::vector<libtorrent::peer_info> const& peers)
		{
			using namespace libtorrent;
			std::vector<peer_info>::const_iterator i = std::find_if(peers.begin()
				, peers.end(), boost::bind(&peer_info::ip, _1) == addr);
			if (i == peers.end()) return -1;

			return i - peers.begin();
		}

		void print_piece(libtorrent::partial_piece_info* pp
			, libtorrent::cached_piece_info* cs
			, std::vector<libtorrent::peer_info> const& peers
			, std::string& out)
		{
			using namespace libtorrent;

			char str[1024];
			assert(pp == 0 || cs == 0 || cs->piece == pp->piece_index);
			int piece = pp ? pp->piece_index : cs->piece;
			int num_blocks = pp ? pp->blocks_in_piece : cs->blocks.size();

			snprintf(str, sizeof(str), "%5d: [", piece);
			out += str;
			char const* last_color = 0;
			for (int j = 0; j < num_blocks; ++j)
			{
				int index = pp ? peer_index(pp->blocks[j].peer(), peers) % 36 : -1;
				char chr = '+';
				if (index >= 0)
					chr = (index < 10)?'0' + index:'A' + index - 10;

				char const* color = "";

				if (pp == 0)
				{
					color = cs->blocks[j] ? esc("34;7") : esc("0");
					chr = ' ';
				}
				else
				{
#ifdef ANSI_TERMINAL_COLORS
					if (cs && cs->blocks[j]) color = esc("36;7");
					else if (pp->blocks[j].bytes_progress > 0
						&& pp->blocks[j].state == block_info::requested)
					{
						if (pp->blocks[j].num_peers > 1) color = esc("1;7");
						else color = esc("33;7");
						chr = '0' + (pp->blocks[j].bytes_progress * 10 / pp->blocks[j].block_size);
					}
					else if (pp->blocks[j].state == block_info::finished) color = esc("32;7");
					else if (pp->blocks[j].state == block_info::writing) color = esc("36;7");
					else if (pp->blocks[j].state == block_info::requested) color = esc("0");
					else { color = esc("0"); chr = ' '; }
#else
					if (cs && cs->blocks[j]) chr = 'c';
					else if (pp->blocks[j].state == block_info::finished) chr = '#';
					else if (pp->blocks[j].state == block_info::writing) chr = '+';
					else if (pp->blocks[j].state == block_info::requested) chr = '-';
					else chr = ' ';
#endif
				}
				if (last_color == 0 || strcmp(last_color, color) != 0)
					snprintf(str, sizeof(str), "%s%c", color, chr);
				else
					out += chr;

				out += str;
			}
#ifdef ANSI_TERMINAL_COLORS
			out += esc("0");
#endif
			char const* piece_state[4] = {"", " slow", " medium", " fast"};
			snprintf(str, sizeof(str), "] %3d cache age: %-.1f %s\n"
				, cs ? cs->next_to_hash : 0
				, cs ? (total_milliseconds(time_now() - cs->last_use) / 1000.f) : 0.f
				, pp ? piece_state[pp->piece_state] : "");
			out += str;
		}

   public:
      bool read_data(char* data, size_type offset, size_type size, size_type& read_size)
      {
         boost::mutex::scoped_lock lock(m_notify_mutex);
         const torrent_info& info = m_handle.get_torrent_info();
         bool ret = false;

         read_size = 0;

#if defined(_DEBUG) && defined(WIN32)
         unsigned int time = GetTickCount();
#endif
         // 计算偏移所在的片.
         int index = offset / info.piece_length();
         BOOST_ASSERT(index >= 0 && index < info.num_pieces());
         if (index >= info.num_pieces() || index < 0)
            return ret;
         torrent_status status = m_handle.status();
         if (!status.pieces.empty())
         {
            if (status.state != torrent_status::downloading &&
                status.state != torrent_status::finished &&
                status.state != torrent_status::seeding)
               return ret;

            // 设置下载位置.
            std::vector<int> pieces;

            pieces = m_handle.piece_priorities();
            std::for_each(pieces.begin(), pieces.end(), boost::lambda::_1 = 1);
            pieces[index] = 7;
            m_handle.prioritize_pieces(pieces);

            if (status.pieces.get_bit(index))
            {
               // 保存参数信息.
               m_current_buffer = data;
               m_read_offset = offset;
               m_read_size = &read_size;
               m_request_size = size;

               // 提交请求.
               m_handle.read_piece(index, boost::bind(&extern_read_op::on_read, this, _1, _2, _3));

               // 等待请求返回.
               m_notify.wait(lock);

               // 读取字节数.
               if (read_size != 0)
                  ret = true;
            }
         }
         else
         {
            // 直接读取文件.
            if (m_file_path.string() == "" || 
               !m_file.is_open())
            {
					boost::filesystem::path path = m_handle.save_path();
               file_storage stor = info.files();
               std::string name = stor.name();
               m_file_path = path / name;
               name = convert_to_native(m_file_path.string());

               // 打开文件, 直接从文件读取数据.
               if (!m_file.is_open())
               {
                  m_file.open(name.c_str(), std::ios::in | std::ios::binary);
                  if (!m_file.is_open())
                     return ret;
               }
            }

            if (!m_file.is_open())
               return ret;

            m_file.clear();
            m_file.seekg(offset, std::ios::beg);
            m_file.read(data, size);
            read_size = m_file.gcount();

            if (read_size != -1)
               ret = true;
         }

#if defined(_DEBUG) && defined(WIN32)
         static unsigned int cur_time = GetTickCount();
			char str_info[8192] = { 0 };
			char *ptr = (char*)str_info;
         if (GetTickCount() - cur_time >= 5000)
         {
            cur_time = GetTickCount();
            sprintf(str_info, "request: %d, size: %d, request time: %d, peers: %d\n", 
               index, (int)size, cur_time - time, status.num_peers);
				ptr = ptr + strlen(str_info);
            for (int i = 0; i < status.pieces.size() / 8; i++)
				{
               if (i % 32 == 0)
					{
						sprintf(ptr, "\n");
						ptr += 1;
					}
               sprintf(ptr, "%02X", (unsigned char)status.pieces.bytes()[i]);
					ptr += 2;
            }
            if (status.pieces.size() % 8 != 0)
				{
               sprintf(ptr, "%02X", (unsigned char)status.pieces.bytes()[status.pieces.size() / 8]);
					ptr += 2;
				}

            sprintf(ptr, "\n\n");

				// 显示当前正在下载的分片信息.
				std::string out;
				char str[500];
				std::vector<cached_piece_info> pieces;
				torrent_handle &h = m_handle;
				std::vector<partial_piece_info> queue;
				std::vector<peer_info> peers;

				m_ses.get_cache_info(h.info_hash(), pieces);
				h.get_download_queue(queue);
				h.get_peer_info(peers);

				std::sort(queue.begin(), queue.end(), boost::bind(&partial_piece_info::piece_index, _1)
					< boost::bind(&partial_piece_info::piece_index, _2));

				std::sort(pieces.begin(), pieces.end(), boost::bind(&cached_piece_info::last_use, _1)
					> boost::bind(&cached_piece_info::last_use, _2));

				for (std::vector<cached_piece_info>::iterator i = pieces.begin();
					i != pieces.end(); ++i)
				{
					partial_piece_info* pp = 0;
					partial_piece_info tmp;
					tmp.piece_index = i->piece;
					std::vector<partial_piece_info>::iterator ppi
						= std::lower_bound(queue.begin(), queue.end(), tmp
						, boost::bind(&partial_piece_info::piece_index, _1)
						< boost::bind(&partial_piece_info::piece_index, _2));
					if (ppi != queue.end() && ppi->piece_index == i->piece) pp = &*ppi;

					print_piece(pp, &*i, peers, out);

					if (pp) queue.erase(ppi);
				}

				for (std::vector<partial_piece_info>::iterator i = queue.begin()
					, end(queue.end()); i != end; ++i)
				{
					print_piece(&*i, 0, peers, out);
				}
				snprintf(str, sizeof(str), "%s %s: read cache %s %s: downloading %s %s: cached %s %s: flushed\n"
					, esc("34;7"), esc("0") // read cache
					, esc("33;7"), esc("0") // downloading
					, esc("36;7"), esc("0") // cached
					, esc("32;7"), esc("0")); // flushed
				out += str;
				out += "___________________________________\n";

				strcat(str_info, out.c_str());

				OutputDebugStringA(str_info);
				printf(str_info);
         }
#endif

         return ret;
      }

   protected:
      void on_read(char* data, size_type offset, size_type size)
      {
         boost::mutex::scoped_lock lock(m_notify_mutex);
         if (size != 0)
         {
            // 计算偏移点和数据大小.
            size_type cp_offset = m_read_offset - offset;
            size_type cp_size = size - cp_offset >= m_request_size 
               ? m_request_size : size - cp_offset;
            // 拷贝并通知读取数据完成.
            memcpy(m_current_buffer, data + cp_offset, cp_size);
            *m_read_size = cp_size;
            m_notify.notify_one();
         }
         else
         {
            // 错误也需要通知, 否则可能发生死锁.
            *m_read_size = 0;
            m_notify.notify_one();
         }
      }

   private:
      boost::mutex m_notify_mutex;
      boost::condition m_notify;
      torrent_handle &m_handle;
		session &m_ses;
      boost::filesystem::path m_file_path;
      std::fstream m_file;
      char *m_current_buffer;
      size_type m_read_offset;
      size_type *m_read_size;
      size_type m_request_size;
      bool m_is_reading;
   };

}

#endif // TORRENT_INTERFACE_HPP_INCLUDED