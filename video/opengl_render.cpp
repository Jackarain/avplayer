#include "internal.h"
#include "opengl_render.h"


opengl_render::opengl_render()
	: m_hglrc(NULL)
	, m_hdc(NULL)
	, m_swsctx(NULL)
	, m_framebuffer(NULL)
	, m_image_width(0)
	, m_image_height(0)
	, m_texture(0)
	, m_keep_aspect(false)
{

}

opengl_render::~opengl_render()
{
   destory_render();
}

bool opengl_render::init_render(void* ctx, int w, int h, int pix_fmt)
{
	m_hwnd = (HWND)ctx;
	m_image_width = w;
	m_image_height = h;
	m_window_aspect = (float)w / (float)h;

	static PIXELFORMATDESCRIPTOR pfd =  // pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_WINDOW |					// Format Must Support Window
		PFD_SUPPORT_OPENGL |					// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,						// Must Support Double Buffering
		PFD_TYPE_RGBA,                   // Request An RGBA Format
		24,										// Select Our Color Depth
		0, 0, 0, 0, 0, 0,						// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,								// Accumulation Bits Ignored
		16,										// 16Bit Z-Buffer (Depth Buffer)  
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,						// Main Drawing Layer
		0,											// Reserved
		0, 0, 0									// Layer Masks Ignored
	};

	if (!(m_hdc = GetDC(m_hwnd)))
	{
		printf("Can't Create A GL Device Context.\n");
		return false;
	}

	GLuint PixelFormat;

	if (!(PixelFormat = ChoosePixelFormat(m_hdc, &pfd)))	// Did Windows Find A Matching Pixel Format?
	{
		printf("Can't Find A Suitable PixelFormat.\n");
		return false;
	}

	if(!SetPixelFormat(m_hdc, PixelFormat, &pfd))        // Are We Able To Set The Pixel Format?
	{
		printf("Can't Set The PixelFormat.\n");
		return false;
	}

	if (!(m_hglrc = wglCreateContext(m_hdc)))  // Are We Able To Get A Rendering Context?
	{
		printf("Can't Create A GL Rendering Context.\n");
		return false;
	}

	if(!wglMakeCurrent(m_hdc, m_hglrc))        // Try To Activate The Rendering Context
	{
		printf("Can't Activate The GL Rendering Context.\n");
		return false;
	}

	RECT rc = { 0 };
	GetClientRect(m_hwnd, &rc);

	re_size(rc.right - rc.left, rc.bottom - rc.top);
	if (!InitGL())
	{
		printf("Initialization Failed.\n");
		return false;
	}

	m_swsctx = sws_getContext(m_image_width, m_image_height, PIX_FMT_YUV420P, m_image_width, m_image_height, 
		PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	m_current_width = m_image_width;
	m_current_height = m_image_height;
	m_framebuffer = (uint8_t*)malloc(w * h * sizeof(uint32_t));

	return true;
}

bool opengl_render::render_one_frame(AVFrame* data, int pix_fmt)
{
	uint8_t* pixels[3] = { data->data[0], 
		data->data[1], 
		data->data[2] };

	int linesize[3] = { data->linesize[0], 
		data->linesize[1], 
		data->linesize[2] };

	AVFrame* pic = avcodec_alloc_frame();
	avpicture_fill((AVPicture*)pic, m_framebuffer, PIX_FMT_RGB24, m_image_width, m_image_height);
	sws_scale(m_swsctx, pixels, linesize, 0, m_image_height, pic->data, pic->linesize);

	int width = 0;
	int height = 0;
	RECT rect_client;
	GetClientRect(m_hwnd, &rect_client);

	if (m_keep_aspect)
	{
		int win_width = width = rect_client.right - rect_client.left;
		int win_height = height = rect_client.bottom - rect_client.top;
		int tmpheight = ((float)width / m_window_aspect);

		tmpheight += tmpheight % 2;
		if(tmpheight > height)
		{
			width = ((float)height * m_window_aspect);
			width += width % 2;
		}
		else 
		{
			height = tmpheight;
		}

		// 居中对齐.
		rect_client.left += ((win_width - width) / 2);
		rect_client.top += ((win_height - height) / 2);
		rect_client.bottom -= ((win_height - height) / 2);
		rect_client.right -= ((win_width - width) / 2);

		if (!EqualRect(&m_last_rect_client, &rect_client))
		{
			m_last_rect_client = rect_client;

			glViewport(rect_client.left, rect_client.top, width, height);             // Reset The Current Viewport

			glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
			glLoadIdentity();									   // Reset The Projection Matrix

			// Calculate The Aspect Ratio Of The Window
			// GLfloat wh = (GLfloat)m_width / (GLfloat)m_height;
			// gluPerspective(120.0f, wh < 1.0f ? 1.0f : wh, 0.0f, 100.0f);

			glMatrixMode(GL_MODELVIEW);                  // Select The Modelview Matrix
			glLoadIdentity();                            // Reset The Modelview Matrix
		}
	}
	else
	{
		width = rect_client.right - rect_client.left;
		height = rect_client.bottom - rect_client.top;

		if (!EqualRect(&m_last_rect_client, &rect_client))
		{
			m_last_rect_client = rect_client;

			glViewport(rect_client.left, rect_client.top, width, height);             // Reset The Current Viewport

			glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
			glLoadIdentity();									   // Reset The Projection Matrix

			// Calculate The Aspect Ratio Of The Window
			// GLfloat wh = (GLfloat)m_width / (GLfloat)m_height;
			// gluPerspective(120.0f, wh < 1.0f ? 1.0f : wh, 0.0f, 100.0f);

			glMatrixMode(GL_MODELVIEW);                  // Select The Modelview Matrix
			glLoadIdentity();                            // Reset The Modelview Matrix
		}

	}

	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, 3, m_image_width, m_image_height, 0, GL_RGB, GL_UNSIGNED_BYTE, m_framebuffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	av_free(pic);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear The Screen And The Depth Buffer
	glLoadIdentity();									// Reset The View
	glTranslatef(0.0f, 0.0f, -0.4f);
	glRotatef(180.0f, 1.0f, 0.0f, 0.0f);

	glBegin(GL_QUADS);
	glTexCoord2d(0.0, 0.0); glVertex2d(-1.0, -1.0);
	glTexCoord2d(1.0, 0.0); glVertex2d(+1.0, -1.0);
	glTexCoord2d(1.0, 1.0); glVertex2d(+1.0, +1.0);
	glTexCoord2d(0.0, 1.0); glVertex2d(-1.0, +1.0);
	glEnd();
	glFlush();

	glDeleteTextures(1, &m_texture);

	SwapBuffers(m_hdc);

	return true;
}

void opengl_render::re_size(int width, int height)
{
	if (height == 0)                             // Prevent A Divide By Zero By
		height = 1;                               // Making Height Equal One

	m_current_width = width;
	m_current_width = height;
}

void opengl_render::aspect_ratio(int srcw, int srch, bool enable_aspect)
{
	if (enable_aspect)
	{
		enable_aspect = true;
		m_window_aspect = (float)srcw / (float)srch;
	}
	else
	{
		enable_aspect = false;
	}
}

void opengl_render::destory_render()
{
	KillGLWindow();

	if (m_framebuffer)
	{
		free(m_framebuffer);
		m_framebuffer = NULL;
	}

	if (m_swsctx)
	{
		sws_freeContext(m_swsctx);
		m_swsctx = NULL;
	}

	return ;
}

bool opengl_render::InitGL(GLvoid)                     // All Setup For OpenGL Goes Here
{
	glEnable(GL_TEXTURE_2D);							// Enable Texture Mapping ( NEW )
	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);        // Black Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations

	return true;										         // Initialization Went OK
}

GLvoid opengl_render::KillGLWindow(GLvoid)            // Properly Kill The Window
{
	if (m_hglrc)                                       // Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL, NULL))					   // Are We Able To Release The DC And RC Contexts?
		{
			printf("Release Of DC And RC Failed.\n");
		}

		if (!wglDeleteContext(m_hglrc))                 // Are We Able To Delete The RC?
		{
			printf("Release Rendering Context Failed.\n");
		}

		m_hglrc = NULL;                                 // Set RC To NULL
	}

	if (m_hdc && !ReleaseDC(m_hwnd, m_hdc))            // Are We Able To Release The DC
	{
		printf("Release Device Context Failed.\n");
		m_hdc = NULL;                                   // Set DC To NULL
	}
}
