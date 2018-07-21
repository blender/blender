/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_ContextGLX.cpp
 *  \ingroup GHOST
 *
 * Definition of GHOST_ContextGLX class.
 */

#include "GHOST_ContextGLX.h"
#include "GHOST_SystemX11.h"

#include <vector>

#include <cassert>
#include <cstdio>
#include <cstring>

/* needed for intel drivers (works w/ mesa-swrast & nvidia) */
#define USE_GLXEW_INIT_WORKAROUND

#ifdef USE_GLXEW_INIT_WORKAROUND
static GLuint _glewStrLen(const GLubyte *s);
static GLboolean _glewSearchExtension(const char *name, const GLubyte *start, const GLubyte *end);
#endif

GLXContext GHOST_ContextGLX::s_sharedContext = None;
int        GHOST_ContextGLX::s_sharedCount   = 0;


GHOST_ContextGLX::GHOST_ContextGLX(
        bool stereoVisual,
        GHOST_TUns16 numOfAASamples,
        Window window,
        Display *display,
        GLXFBConfig fbconfig,
        int contextProfileMask,
        int contextMajorVersion,
        int contextMinorVersion,
        int contextFlags,
        int contextResetNotificationStrategy)
    : GHOST_Context(stereoVisual, numOfAASamples),
      m_display(display),
      m_fbconfig(fbconfig),
      m_window(window),
      m_contextProfileMask(contextProfileMask),
      m_contextMajorVersion(contextMajorVersion),
      m_contextMinorVersion(contextMinorVersion),
      m_contextFlags(contextFlags),
      m_contextResetNotificationStrategy(contextResetNotificationStrategy),
      m_context(None)
{
	assert(m_display != NULL);
}


GHOST_ContextGLX::~GHOST_ContextGLX()
{
	if (m_display != NULL) {
		if (m_context != None) {
			if (m_window != 0 && m_context == ::glXGetCurrentContext())
				::glXMakeCurrent(m_display, None, NULL);

			if (m_context != s_sharedContext || s_sharedCount == 1) {
				assert(s_sharedCount > 0);

				s_sharedCount--;

				if (s_sharedCount == 0)
					s_sharedContext = NULL;

				::glXDestroyContext(m_display, m_context);
			}
		}
	}
}


GHOST_TSuccess GHOST_ContextGLX::swapBuffers()
{
	::glXSwapBuffers(m_display, m_window);

	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_ContextGLX::activateDrawingContext()
{
	if (m_display) {
		return ::glXMakeCurrent(m_display, m_window, m_context) ? GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		return GHOST_kFailure;
	}
}

GHOST_TSuccess GHOST_ContextGLX::releaseDrawingContext()
{
	if (m_display) {
		return ::glXMakeCurrent(m_display, None, NULL) ? GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		return GHOST_kFailure;
	}
}

void GHOST_ContextGLX::initContextGLXEW()
{
	initContextGLEW();
}

GHOST_TSuccess GHOST_ContextGLX::initializeDrawingContext()
{
	GHOST_X11_ERROR_HANDLERS_OVERRIDE(handler_store);

	/* -------------------------------------------------------------------- */
	/* Begin Inline Glew  */

#ifdef USE_GLXEW_INIT_WORKAROUND
	const GLubyte *extStart = (GLubyte *)"";
	const GLubyte *extEnd;
	if (glXQueryExtension(m_display, NULL, NULL)) {
		extStart = (const GLubyte *)glXGetClientString(m_display, GLX_EXTENSIONS);
		if ((extStart == NULL) ||
		    (glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)glXGetProcAddressARB(
		             (const GLubyte *)"glXChooseFBConfig")) == NULL ||
		    (glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddressARB(
		             (const GLubyte *)"glXCreateContextAttribsARB")) == NULL ||
		    (glXCreatePbuffer = (PFNGLXCREATEPBUFFERPROC)glXGetProcAddressARB(
		             (const GLubyte *)"glXCreatePbuffer")) == NULL)
		{
			extStart = (GLubyte *)"";
		}
	}
	extEnd = extStart + _glewStrLen(extStart);

#undef GLXEW_ARB_create_context
	const bool GLXEW_ARB_create_context =
	        _glewSearchExtension("GLX_ARB_create_context", extStart, extEnd);
#undef GLXEW_ARB_create_context_profile
	const bool GLXEW_ARB_create_context_profile =
	        _glewSearchExtension("GLX_ARB_create_context_profile", extStart, extEnd);
#undef GLXEW_ARB_create_context_robustness
const bool GLXEW_ARB_create_context_robustness =
	        _glewSearchExtension("GLX_ARB_create_context_robustness", extStart, extEnd);
#ifdef WITH_GLEW_ES
#undef GLXEW_EXT_create_context_es_profile
	const bool GLXEW_EXT_create_context_es_profile =
	        _glewSearchExtension("GLX_EXT_create_context_es_profile", extStart, extEnd);
#undef GLXEW_EXT_create_context_es2_profile
	const bool GLXEW_EXT_create_context_es2_profile =
	        _glewSearchExtension("GLX_EXT_create_context_es2_profile", extStart, extEnd);
#endif  /* WITH_GLEW_ES */

	/* End Inline Glew */
	/* -------------------------------------------------------------------- */
#else
	/* important to initialize only glxew (_not_ glew),
	 * since this breaks w/ Mesa's `swrast`, see: T46431 */
	glxewInit();
#endif  /* USE_GLXEW_INIT_WORKAROUND */



	if (GLXEW_ARB_create_context) {
		int profileBitCore   = m_contextProfileMask & GLX_CONTEXT_CORE_PROFILE_BIT_ARB;
		int profileBitCompat = m_contextProfileMask & GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;

#ifdef WITH_GLEW_ES
		int profileBitES     = m_contextProfileMask & GLX_CONTEXT_ES_PROFILE_BIT_EXT;
#endif

		if (!GLXEW_ARB_create_context_profile && profileBitCore)
			fprintf(stderr, "Warning! OpenGL core profile not available.\n");

		if (!GLXEW_ARB_create_context_profile && profileBitCompat)
			fprintf(stderr, "Warning! OpenGL compatibility profile not available.\n");

#ifdef WITH_GLEW_ES
		if (!GLXEW_EXT_create_context_es_profile && profileBitES && m_contextMajorVersion == 1)
			fprintf(stderr, "Warning! OpenGL ES profile not available.\n");

		if (!GLXEW_EXT_create_context_es2_profile && profileBitES && m_contextMajorVersion == 2)
			fprintf(stderr, "Warning! OpenGL ES2 profile not available.\n");
#endif

		int profileMask = 0;

		if (GLXEW_ARB_create_context_profile && profileBitCore)
			profileMask |= profileBitCore;

		if (GLXEW_ARB_create_context_profile && profileBitCompat)
			profileMask |= profileBitCompat;

#ifdef WITH_GLEW_ES
		if (GLXEW_EXT_create_context_es_profile && profileBitES)
			profileMask |= profileBitES;
#endif

		if (profileMask != m_contextProfileMask)
			fprintf(stderr, "Warning! Ignoring untested OpenGL context profile mask bits.");


		/* max 10 attributes plus terminator */
		int attribs[11];
		int i = 0;

		if (profileMask) {
			attribs[i++] = GLX_CONTEXT_PROFILE_MASK_ARB;
			attribs[i++] = profileMask;
		}

		if (m_contextMajorVersion != 0) {
			attribs[i++] = GLX_CONTEXT_MAJOR_VERSION_ARB;
			attribs[i++] = m_contextMajorVersion;
			attribs[i++] = GLX_CONTEXT_MINOR_VERSION_ARB;
			attribs[i++] = m_contextMinorVersion;
		}

		if (m_contextFlags != 0) {
			attribs[i++] = GLX_CONTEXT_FLAGS_ARB;
			attribs[i++] = m_contextFlags;
		}

		if (m_contextResetNotificationStrategy != 0) {
			if (GLXEW_ARB_create_context_robustness) {
				attribs[i++] = GLX_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB;
				attribs[i++] = m_contextResetNotificationStrategy;
			}
			else {
				fprintf(stderr, "Warning! Cannot set the reset notification strategy.");
			}
		}
		attribs[i++] = 0;

		/* Some drivers don't like having a true offscreen context.
		 * Create a pixel buffer instead of a window to render to.
		 * even if it will never be used for drawing. */
		int pbuffer_attribs[] = {
			GLX_PBUFFER_WIDTH, 1,
			GLX_PBUFFER_HEIGHT, 1,
			None
		};

		/* Create a GL 3.x context */
		if (m_fbconfig) {
			m_context = glXCreateContextAttribsARB(m_display, m_fbconfig, s_sharedContext, true, attribs);

			if (!m_window) {
				m_window = (Window)glXCreatePbuffer(m_display, m_fbconfig, pbuffer_attribs);
			}
		}
		else {
			GLXFBConfig *framebuffer_config = NULL;
			{
				int glx_attribs[64];
				int fbcount = 0;

				GHOST_X11_GL_GetAttributes(glx_attribs, 64, m_numOfAASamples, m_stereoVisual, false, true);

				framebuffer_config = glXChooseFBConfig(m_display, DefaultScreen(m_display), glx_attribs, &fbcount);
			}

			if (framebuffer_config) {
				m_context = glXCreateContextAttribsARB(m_display, framebuffer_config[0], s_sharedContext, True, attribs);

				if (!m_window) {
					m_window = (Window)glXCreatePbuffer(m_display, framebuffer_config[0], pbuffer_attribs);
				}

				XFree(framebuffer_config);
			}
		}
	}
	else {
		/* Don't create legacy context */
		fprintf(stderr, "Warning! GLX_ARB_create_context not available.\n");
	}

	GHOST_TSuccess success;

	if (m_context != NULL) {
		const unsigned char *version;

		if (!s_sharedContext)
			s_sharedContext = m_context;

		s_sharedCount++;

		glXMakeCurrent(m_display, m_window, m_context);

		// Seems that this has to be called after MakeCurrent,
		// which means we cannot use glX extensions until after we create a context
		initContextGLXEW();

		if (m_window) {
			initClearGL();
			::glXSwapBuffers(m_display, m_window);
		}

		/* re initialize to get the extensions properly */
		initContextGLXEW();

		version = glGetString(GL_VERSION);

		if (!version || version[0] < '3' || ((version[0] == '3') && (version[2] < '3'))) {
			fprintf(stderr, "Error! Blender requires OpenGL 3.3 to run. Try updating your drivers.\n");
			fflush(stderr);
			/* ugly, but we get crashes unless a whole bunch of systems are patched. */
			exit(0);
		}
		else
			success = GHOST_kSuccess;
	}
	else {
		/* freeing well clean up the context initialized above */
		success = GHOST_kFailure;
	}


	GHOST_X11_ERROR_HANDLERS_RESTORE(handler_store);

	return success;
}


GHOST_TSuccess GHOST_ContextGLX::releaseNativeHandles()
{
	m_window = 0;

	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_ContextGLX::setSwapInterval(int interval)
{
	if (GLXEW_EXT_swap_control) {
		::glXSwapIntervalEXT(m_display, m_window, interval);

		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextGLX::getSwapInterval(int &intervalOut)
{
	if (GLXEW_EXT_swap_control) {
		unsigned int interval = 0;

		::glXQueryDrawable(m_display, m_window, GLX_SWAP_INTERVAL_EXT, &interval);

		intervalOut = static_cast<int>(interval);

		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}

/**
 * Utility function to get GLX attributes.
 *
 * \param for_fb_config: There are some small differences in
 * #glXChooseVisual and #glXChooseFBConfig's attribute encoding.
 *
 * \note Similar to SDL's 'X11_GL_GetAttributes'
 */
int GHOST_X11_GL_GetAttributes(
        int *attribs, int attribs_max,
        int samples, bool is_stereo_visual,
        bool need_alpha,
        bool for_fb_config)
{
	int i = 0;

#ifdef GHOST_OPENGL_STENCIL
	const bool need_stencil = true;
#else
	const bool need_stencil = false;
#endif

	if (is_stereo_visual) {
		attribs[i++] = GLX_STEREO;
		if (for_fb_config) {
			attribs[i++] = True;
		}
	}

	if (for_fb_config) {
		attribs[i++] = GLX_RENDER_TYPE;
		attribs[i++] = GLX_RGBA_BIT;
	}
	else {
		attribs[i++] = GLX_RGBA;
	}

	attribs[i++] = GLX_DOUBLEBUFFER;
	if (for_fb_config) {
		attribs[i++] = True;
	}

	attribs[i++] = GLX_RED_SIZE;
	attribs[i++] = True;

	attribs[i++] = GLX_BLUE_SIZE;
	attribs[i++] = True;

	attribs[i++] = GLX_GREEN_SIZE;
	attribs[i++] = True;

	attribs[i++] = GLX_DEPTH_SIZE;
	attribs[i++] = True;

	if (need_alpha) {
		attribs[i++] = GLX_ALPHA_SIZE;
		attribs[i++] = True;
	}

	if (need_stencil) {
		attribs[i++] = GLX_STENCIL_SIZE;
		attribs[i++] = True;
	}

	if (samples) {
		attribs[i++] = GLX_SAMPLE_BUFFERS_ARB;
		attribs[i++] = True;

		attribs[i++] = GLX_SAMPLES_ARB;
		attribs[i++] = samples;
	}

	attribs[i++] = 0;

	GHOST_ASSERT(i <= attribs_max, "attribute size too small");

	(void)attribs_max;

	return i;
}


/* excuse inlining part of glew */
#ifdef USE_GLXEW_INIT_WORKAROUND
static GLuint _glewStrLen(const GLubyte *s)
{
	GLuint i = 0;
	if (s == NULL) return 0;
	while (s[i] != '\0') i++;
	return i;
}

static GLuint _glewStrCLen(const GLubyte *s, GLubyte c)
{
	GLuint i = 0;
	if (s == NULL) return 0;
	while (s[i] != '\0' && s[i] != c) i++;
	return (s[i] == '\0' || s[i] == c) ? i : 0;
}

static GLboolean _glewStrSame(const GLubyte *a, const GLubyte *b, GLuint n)
{
	GLuint i = 0;
	if (a == NULL || b == NULL)
		return (a == NULL && b == NULL && n == 0) ? GL_TRUE : GL_FALSE;
	while (i < n && a[i] != '\0' && b[i] != '\0' && a[i] == b[i]) i++;
	return i == n ? GL_TRUE : GL_FALSE;
}

static GLboolean _glewSearchExtension(const char *name, const GLubyte *start, const GLubyte *end)
{
	const GLubyte *p;
	GLuint len = _glewStrLen((const GLubyte *)name);
	p = start;
	while (p < end) {
		GLuint n = _glewStrCLen(p, ' ');
		if (len == n && _glewStrSame((const GLubyte *)name, p, n)) return GL_TRUE;
		p += n + 1;
	}
	return GL_FALSE;
}
#endif  /* USE_GLXEW_INIT_WORKAROUND */
