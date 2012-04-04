#ifdef _MSC_VER
#define MSVC_DETECT_MEMORY_LEAKS
#define _CRTDBG_MAP_ALLOC
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef MSVC_DETECT_MEMORY_LEAKS

#include <crtdbg.h>
#include <cassert>

// 并在main入口函数调用
void heap_debug()
{
	int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

	// Turn on leak-checking bit
	tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

	//tmpFlag |= _CRTDBG_CHECK_MasterLWMasterYS_DF;

	// Turn off CRT block checking bit
	tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;

	// Set flag to the new value
	_CrtSetDbgFlag( tmpFlag );
}
#else
void heap_debug()
{
}
#endif

#include "ffaudio.h"
#include "ffplay.h"

int main(int argc, char** argv)
{
	avplayer* play = NULL;
	char *filename = "d:\\media\\avda.rmvb";
	wave_render audio_render;

	heap_debug();

	audio_render.init_audio = wave_init_audio;
	audio_render.play_audio = wave_play_audio;
	audio_render.audio_control = wave_audio_control;
	audio_render.destory_audio = wave_destory_audio;
	audio_render.ctx = &audio_render;

	initialize(&play, filename);
	configure(play, &audio_render, AUDIO_REANDER);
	start(play);
	/* Sleep(5000); */
	wait_for_completion(play);
	stop(play);
	destory(play);

	return EXIT_SUCCESS;
}

