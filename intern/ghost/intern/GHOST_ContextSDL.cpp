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

/** \file ghost/intern/GHOST_ContextSDL.cpp
 *  \ingroup GHOST
 *
 * Definition of GHOST_ContextSDL class.
 */

#include "GHOST_ContextSDL.h"

#include <vector>

#include <cassert>
#include <cstdio>
#include <cstring>


SDL_GLContext GHOST_ContextSDL::s_sharedContext = NULL;
int           GHOST_ContextSDL::s_sharedCount   = 0;


GHOST_ContextSDL::GHOST_ContextSDL(
        bool stereoVisual,
        GHOST_TUns16 numOfAASamples,
        SDL_Window *window,
        int contextProfileMask,
        int contextMajorVersion,
        int contextMinorVersion,
        int contextFlags,
        int contextResetNotificationStrategy)
    : GHOST_Context(stereoVisual, numOfAASamples),
      m_window(window),
      m_contextProfileMask(contextProfileMask),
      m_contextMajorVersion(contextMajorVersion),
      m_contextMinorVersion(contextMinorVersion),
      m_contextFlags(contextFlags),
      m_contextResetNotificationStrategy(contextResetNotificationStrategy),
      m_context(NULL)
{
	assert(m_window  != NULL);
}


GHOST_ContextSDL::~GHOST_ContextSDL()
{
	if (m_context != NULL) {
		if (m_window != 0 && m_context == SDL_GL_GetCurrentContext())
			SDL_GL_MakeCurrent(m_window, m_context);

		if (m_context != s_sharedContext || s_sharedCount == 1) {
			assert(s_sharedCount > 0);

			s_sharedCount--;

			if (s_sharedCount == 0)
				s_sharedContext = NULL;

			SDL_GL_DeleteContext(m_context);
		}
	}
}


GHOST_TSuccess GHOST_ContextSDL::swapBuffers()
{
	SDL_GL_SwapWindow(m_window);

	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_ContextSDL::activateDrawingContext()
{
	if (m_context) {
		activateGLEW();

		return SDL_GL_MakeCurrent(m_window, m_context) ? GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextSDL::initializeDrawingContext()
{
#ifdef GHOST_OPENGL_ALPHA
	const bool needAlpha   = true;
#else
	const bool needAlpha   = false;
#endif

#ifdef GHOST_OPENGL_STENCIL
	const bool needStencil = true;
#else
	const bool needStencil = false;
#endif

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, m_contextProfileMask);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, m_contextMajorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, m_contextMinorVersion);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, m_contextFlags);

	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);

	if (needAlpha) {
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	}

	if (needStencil) {
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);
	}

	if (m_stereoVisual) {
		SDL_GL_SetAttribute(SDL_GL_STEREO, 1);
	}

	if (m_numOfAASamples) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, m_numOfAASamples);
	}

	m_context = SDL_GL_CreateContext(m_window);

	GHOST_TSuccess success;

	if (m_context != NULL) {
		if (!s_sharedContext)
			s_sharedContext = m_context;

		s_sharedCount++;

		success = (SDL_GL_MakeCurrent(m_window, m_context) < 0) ?
		           GHOST_kFailure : GHOST_kSuccess;

		initContextGLEW();

		initClearGL();
		SDL_GL_SwapWindow(m_window);

		success = GHOST_kSuccess;
	}
	else {
		success = GHOST_kFailure;
	}

	return success;
}


GHOST_TSuccess GHOST_ContextSDL::releaseNativeHandles()
{
	m_window = NULL;

	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_ContextSDL::setSwapInterval(int interval)
{
	if (SDL_GL_SetSwapInterval(interval) != -1) {
		return GHOST_kSuccess;
	}
	else {
		return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_ContextSDL::getSwapInterval(int &intervalOut)
{
	intervalOut = SDL_GL_GetSwapInterval();
	return GHOST_kSuccess;
}
