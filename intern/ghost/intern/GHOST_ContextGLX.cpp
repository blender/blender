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

/* this should eventually be enabled, but causes T46431 */
// #define USE_CONTEXT_FLAGS

#ifdef WITH_GLEW_MX
GLXEWContext *glxewContext = NULL;
#endif

GLXContext GHOST_ContextGLX::s_sharedContext = None;
int        GHOST_ContextGLX::s_sharedCount   = 0;


GHOST_ContextGLX::GHOST_ContextGLX(
        bool stereoVisual,
        GHOST_TUns16 numOfAASamples,
        Window window,
        Display *display,
        XVisualInfo *visualInfo,
        int contextProfileMask,
        int contextMajorVersion,
        int contextMinorVersion,
        int contextFlags,
        int contextResetNotificationStrategy)
    : GHOST_Context(stereoVisual, numOfAASamples),
      m_display(display),
      m_visualInfo(visualInfo),
      m_window(window),
      m_contextProfileMask(contextProfileMask),
      m_contextMajorVersion(contextMajorVersion),
      m_contextMinorVersion(contextMinorVersion),
      m_contextFlags(contextFlags),
      m_contextResetNotificationStrategy(contextResetNotificationStrategy),
      m_context(None)
#ifdef WITH_GLEW_MX
      ,
      m_glxewContext(NULL)
#endif
{
	assert(m_window  != 0);
	assert(m_display != NULL);
}


GHOST_ContextGLX::~GHOST_ContextGLX()
{
	if (m_display != NULL) {
		activateGLXEW();

		if (m_context != None) {
			if (m_window != 0 && m_context == ::glXGetCurrentContext())
				::glXMakeCurrent(m_display, m_window, NULL);

			if (m_context != s_sharedContext || s_sharedCount == 1) {
				assert(s_sharedCount > 0);

				s_sharedCount--;

				if (s_sharedCount == 0)
					s_sharedContext = NULL;

				::glXDestroyContext(m_display, m_context);
			}
		}

#ifdef WITH_GLEW_MX
		if (m_glxewContext)
			delete m_glxewContext;
#endif
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
		activateGLXEW();
		activateGLEW();

		return ::glXMakeCurrent(m_display, m_window, m_context) ? GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		return GHOST_kFailure;
	}
}

void GHOST_ContextGLX::initContextGLXEW()
{
#ifdef WITH_GLEW_MX
	glxewContext = new GLXEWContext;
	memset(glxewContext, 0, sizeof(GLXEWContext));

	if (m_glxewContext)
		delete m_glxewContext;
	m_glxewContext = glxewContext;
#endif

	initContextGLEW();
}

GHOST_TSuccess GHOST_ContextGLX::initializeDrawingContext()
{
#ifdef WITH_X11_XINPUT
	/* use our own event handlers to avoid exiting blender,
	 * this would happen for eg:
	 * if you open blender, unplug a tablet, then open a new window. */
	XErrorHandler   old_handler    = XSetErrorHandler  (GHOST_X11_ApplicationErrorHandler  );
	XIOErrorHandler old_handler_io = XSetIOErrorHandler(GHOST_X11_ApplicationIOErrorHandler);
#endif

#ifdef USE_CONTEXT_FLAGS
	/* needed so 'GLXEW_ARB_create_context' is valid */
	mxIgnoreNoVersion(1);
	initContextGLXEW();
	mxIgnoreNoVersion(0);
#endif

#ifdef USE_CONTEXT_FLAGS
	if (GLXEW_ARB_create_context)
#else
	if (0)
#endif
	{
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
		}

		if (m_contextMinorVersion != 0) {
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

		/* Create a GL 3.x context */
		GLXFBConfig *framebuffer_config = NULL;
		{
			int glx_attribs[64];
			int fbcount = 0;

			GHOST_X11_GL_GetAttributes(glx_attribs, 64, m_numOfAASamples, m_stereoVisual, true);

			framebuffer_config = glXChooseFBConfig(m_display, DefaultScreen(m_display), glx_attribs, &fbcount);
		}

		if (framebuffer_config) {
			m_context = glXCreateContextAttribsARB(m_display, framebuffer_config[0], s_sharedContext, True, attribs);
			XFree(framebuffer_config);
		}
	}
	else {
		/* Create legacy context */
		m_context = glXCreateContext(m_display, m_visualInfo, s_sharedContext, True);
	}

	GHOST_TSuccess success;

	if (m_context != NULL) {
		if (!s_sharedContext)
			s_sharedContext = m_context;

		s_sharedCount++;

		glXMakeCurrent(m_display, m_window, m_context);

#ifndef USE_CONTEXT_FLAGS
		// Seems that this has to be called after MakeCurrent,
		// which means we cannot use glX extensions until after we create a context
		initContextGLXEW();
#endif

		initClearGL();
		::glXSwapBuffers(m_display, m_window);

		/* re initialize to get the extensions properly */
		initContextGLXEW();

		success = GHOST_kSuccess;
	}
	else {
		/* freeing well clean up the context initialized above */
		success = GHOST_kFailure;
	}

#ifdef WITH_X11_XINPUT
	/* Restore handler */
	XSetErrorHandler  (old_handler);
	XSetIOErrorHandler(old_handler_io);
#endif

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
        bool for_fb_config)
{
	int i = 0;

#ifdef GHOST_OPENGL_ALPHA
	const bool need_alpha = true;
#else
	const bool need_alpha = false;
#endif

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
