#include "stdafx.h"
#include "avplayer.h"
#include "player_impl.h"

avplayer::avplayer(void)
	: m_impl(new player_impl())
{
}

avplayer::~avplayer(void)
{
	if (m_impl)
		delete m_impl;
}

HWND avplayer::create_window(const char *player_name)
{
	return m_impl->create_window(player_name);
}

BOOL avplayer::destory_window()
{
	return m_impl->destory_window();
}

BOOL avplayer::subclasswindow(HWND hwnd, BOOL in_process/* = TRUE*/)
{
	return m_impl->subclasswindow(hwnd, in_process);
}

BOOL avplayer::unsubclasswindow(HWND hwnd)
{
	return m_impl->unsubclasswindow(hwnd);
}

BOOL avplayer::open(const char *movie, int media_type, int render_type/* = 0*/)
{
	return m_impl->open(movie, media_type, render_type);
}

BOOL avplayer::play(double fact/* = 0.0f*/, int index/* = 0*/)
{
	return m_impl->play(fact, index);
}

BOOL avplayer::load_subtitle(const char *subtitle)
{
	return m_impl->load_subtitle(subtitle);
}

BOOL avplayer::pause()
{
	return m_impl->pause();
}

BOOL avplayer::resume()
{
	return m_impl->resume();
}

BOOL avplayer::wait_for_completion()
{
	return m_impl->wait_for_completion();
}

BOOL avplayer::stop()
{
	return m_impl->stop();
}

BOOL avplayer::close()
{
	return m_impl->close();
}

void avplayer::seek_to(double fact)
{
	m_impl->seek_to(fact);
}

void avplayer::volume(double l, double r)
{
	m_impl->volume(l, r);
}

void avplayer::mute_set(bool s)
{
	m_impl->mute_set(s);
}

BOOL avplayer::full_screen(BOOL fullscreen)
{
	return m_impl->full_screen(fullscreen);
}

int avplayer::download_rate()
{
	return m_impl->download_rate();
}

void avplayer::set_download_rate(int k)
{
	return m_impl->set_download_rate(k);
}

double avplayer::curr_play_time()
{
	return m_impl->curr_play_time();
}

double avplayer::duration()
{
	return m_impl->duration();
}

int avplayer::video_width()
{
	return m_impl->video_width();
}

int avplayer::video_height()
{
	return m_impl->video_height();
}

int avplayer::media_count()
{
	return m_impl->play_list().size();
}

int avplayer::media_list(char ***list, int *size)
{
	std::map<std::string, std::string> mlist = m_impl->play_list();

	*size = mlist.size();
	char **temp = new char*[*size];

	int n = 0;
	for (std::map<std::string, std::string>::iterator i = mlist.begin();
		i != mlist.end(); i++)
	{
		char *file_name = strdup(i->second.c_str());
		temp[n] = file_name;
	}
	*list = temp;

	return*size;
}

void avplayer::free_media_list(char **list, int size)
{
	for (int i = 0; i < size; i++)
		free(list[i]);
	delete[] list;
}

HWND avplayer::get_window_handle()
{
	return m_impl->get_window_handle();
}

