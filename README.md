一直以来, 在多媒体播放器这块, 即使目前有许多开源的播放器项目, 但要写一个播放器仍然是件非常困难的事, 如果在windows上你有可能需要熟悉DShow, 另外的话, 你需要学习一堆开源项目(比如FFmpeg, MPC, VLC, Mplayer), 而且多数都是基于linux, 在windows上学习起来很不容易, 然而这些开源项目对于一些希望快速实现自己播放器, 就显得很困难.
因此, 我创建了这个项目, 致力于以最简单的方法实现自己的播放器, 并提供一个可以很方便使用的接口.

目前, 在这个代码中, 主要链接到FFmpeg来进行解码, 并将其改造成一个通用的播放器框架. 在这个框架中能够接受各种数据的读入, 可以很方便的封装自己的数据读取模块, 也可以很方便的定制自己的视频渲染模块和音频播放模块, 你只需要参考其中的实现即可.
另外在当前的实现中, 因为个人精力实在有限, 所以借鉴了一些开源项目的代码(如Mplayer), 并且该代码跨平台(目前在linux平台的实现稍简单). 所以, 我希望有朋友能参与到这个项目中一起研究和学习, 并完成这个目标.

在 https://dl.dropbox.com/u/1530370/bin.7z 中有已经编译好的exe可供测试, 测试命令格式如下:

avplayer.exe <文件名|URL|TORRENT>


整个项目分为几个模块:


1. win32 这个目录下, 有相关windows平台上的封装和实现.

	1. avcore 这只是一个对libav的包装类, 实现windows上的窗口创建以及消息响应, 比如: 右击暂停/开始, 单击按屏幕宽百分比seek, F2全屏切换.

		这个模块中, avplayer类是外部接口类, 具体实现在player_impl类中. 在player_impl类中, 初始化各模块结构指针由下面几个函数实现,

		void init_file_source(source_context *sc);

		void init_audio(ao_context *ao);
	
		void init_video(vo_context *vo);

		可以根据自己的需求来修改这些函数实现.

		source_context是一个提供数据访问的结构, 包含一些函数指针, 如果你需要重新从其它地方读取数据进来提供给播放器, 那么你可以参照这个结构体, 实现这些函数并指向它, 就可以获得从你指定的地方读取数据来进行播放, 这里实现了一个从文件读取数据播放的dll.

		ao_context是用于播放音频的结构, 同样包含了一些函数指针, 只要实现这些函数指针, 就可以改变音频输出, 比如把音频数据写到文件, 这里实现了2种方式输出音频, 一种是使用dsound来输出音频, 另一种是使用waveout来输出音频.

		vo_context是用于渲染视频的结构, 原理同上, 需要注意的是render_one_frame的data是YUV420格式.
	
	2. avplayer 是一个使用avcore实现一个简单播放器的示范, 如果要创建一个播放器, 只需要avplayer.h, 然后使用创建一个avplayer对象, 就可以创建一个播放器, 关于如何使用avcore的具体细节可以参考avplayer/main.cpp.

2. linux 这个目录下是类似avcore对libav的调用实现, 使用sdl渲染视频和音频输出.

3. libav 这是一个基于ffmpeg的播放框架, 该框架不包含视频渲染和音频播放以及数据读取, 若需要可以实现上述结构体各函数即可, 这个框架中主要完成了使用ffmpeg解码, 以及音视频同步, seek操作处理, 并提供一些基本的控制播放器的函数(在avplay.h和globals.h中定义).

4. audio 是一个音频播放输出模块实现, 主要实现了ao_context中那几个函数指针. 在这个模块中实现了3套音频输出dsound和waveout, 还有用在linux下sdl音频输出.

5. video 是一个视频渲染模块的实现, 实现了vo_context中的那几个函数指针. 在这个模块中, 实现了4套视频渲染输出d3d, ddraw, opengl, sdl(linux下使用)这些方式(还有gdi方式渲染没有添加到video_out.h中).


源代码:

请使用git下载, 以便随时更新代码, 这样做也可以方便在你自己的分支中开发, 并提交补丁.

$ git clone https://github.com/avplayer/avplayer.git avplayer

更新

$ git pull

##### 使用 cmake 编译

	cmake . && make

####本程序基于FFmpeg, 所以只能以GPL协议发布, 任何人请在遵守协议的前提下复制、发布、修改.
####最后本程序作者不承担使用该程序所带来的任何问题并拥有一切解释权.


mail: jack.wgm AT gmail.com microcaicai AT gmail.com

