一直以来, 在多媒体播放器这块, 即使目前有许多开源的播放器项目, 但要写一个播放器仍然是件非常困难的事, 如果在windows上你有可能需要熟悉DShow, 另外的话, 你需要学习一堆开源项目(比如FFmpeg, MPC, VLC, Mplayer), 而且多数都是基于linux, 在windows上学习起来很不容易, 然而这些开源项目对于一些希望快速实现自己播放器, 就显得很困难.
因此, 我创建了这个项目, 致力于以最简单的方法实现自己的播放器, 并提供一个可以很方便使用的接口.

目前, 在这个代码中, 主要链接到FFmpeg来进行解码, 并将其改造成一个通用的播放器框架. 在这个框架中能够接受各种数据的读入, 可以很方便的封装自己的数据读取模块, 也可以很方便的定制自己的视频渲染模块和音频播放模块, 你只需要参考其中的实现即可.
另外在当前的实现中, 因为个人精力实在有限, 所以借鉴了一些开源项目的代码(如Mplayer), 并且该代码跨平台(目前在linux平台的实现稍简单). 所以, 我希望有朋友能参与到这个项目中一起研究和学习, 并完成这个目标.

在 https://sourceforge.net/projects/avplayer/files/ 中有已经编译好的exe和torrent可供测试, 测试命令格式如下:

avplayer.exe <文件名|URL|TORRENT>


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
