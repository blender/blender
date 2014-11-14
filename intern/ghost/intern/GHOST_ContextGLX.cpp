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

	m_context = glXCreateContext(m_display, m_visualInfo, s_sharedContext, True);

	GHOST_TSuccess success;

	if (m_context != NULL) {
		if (!s_sharedContext)
			s_sharedContext = m_context;

		s_sharedCount++;

		glXMakeCurrent(m_display, m_window, m_context);

		// Seems that this has to be called after MakeCurrent,
		// which means we cannot use glX extensions until after we create a context
		initContextGLXEW();

		initClearGL();
		::glXSwapBuffers(m_display, m_window);

		success = GHOST_kSuccess;
	}
	else {
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
