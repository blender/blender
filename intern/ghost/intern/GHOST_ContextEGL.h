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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_ContextEGL.h
 *  \ingroup GHOST
 */

#ifndef __GHOST_CONTEXTEGL_H__
#define __GHOST_CONTEXTEGL_H__

#include "GHOST_Context.h"

#ifdef WITH_GLEW_MX
#  define eglewGetContext() eglewContext
#endif

#include <GL/eglew.h>

#ifdef WITH_GLEW_MX
extern "C" EGLEWContext *eglewContext;
#endif


#ifndef GHOST_OPENGL_EGL_CONTEXT_FLAGS
#define GHOST_OPENGL_EGL_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY
#define GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY 0
#endif


class GHOST_ContextEGL : public GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_ContextEGL(
	        bool stereoVisual,
	        GHOST_TUns16 numOfAASamples,
	        EGLNativeWindowType nativeWindow,
	        EGLNativeDisplayType nativeDisplay,
	        EGLint contextProfileMask,
	        EGLint contextMajorVersion,
	        EGLint contextMinorVersion,
	        EGLint contextFlags,
	        EGLint contextResetNotificationStrategy,
	        EGLenum api);

	/**
	 * Destructor.
	 */
	virtual ~GHOST_ContextEGL();

	/**
	 * Swaps front and back buffers of a window.
	 * \return  A boolean success indicator.
	 */
	virtual GHOST_TSuccess swapBuffers();

	/**
	 * Activates the drawing context of this window.
	 * \return  A boolean success indicator.
	 */
	virtual GHOST_TSuccess activateDrawingContext();

	/**
	 * Call immediately after new to initialize.  If this fails then immediately delete the object.
	 * \return Indication as to whether initialization has succeeded.
	 */
	virtual GHOST_TSuccess initializeDrawingContext();

	/**
	 * Removes references to native handles from this context and then returns
	 * \return GHOST_kSuccess if it is OK for the parent to release the handles and
	 * GHOST_kFailure if releasing the handles will interfere with sharing
	 */
	virtual GHOST_TSuccess releaseNativeHandles();

	/**
	 * Sets the swap interval for swapBuffers.
	 * \param interval The swap interval to use.
	 * \return A boolean success indicator.
	 */
	virtual GHOST_TSuccess setSwapInterval(int interval);

	/**
	 * Gets the current swap interval for swapBuffers.
	 * \param intervalOut Variable to store the swap interval if it can be read.
	 * \return Whether the swap interval can be read.
	 */
	GHOST_TSuccess getSwapInterval(int &intervalOut);

protected:
	inline void activateEGLEW() const {
#ifdef WITH_GLEW_MX
		eglewContext = m_eglewContext;
#endif
	}

private:
	void initContextEGLEW();

	EGLNativeDisplayType m_nativeDisplay;
	EGLNativeWindowType  m_nativeWindow;

	const EGLint m_contextProfileMask;
	const EGLint m_contextMajorVersion;
	const EGLint m_contextMinorVersion;
	const EGLint m_contextFlags;
	const EGLint m_contextResetNotificationStrategy;

	const EGLenum m_api;

	EGLContext m_context;
	EGLSurface m_surface;
	EGLDisplay m_display;

	EGLint m_swap_interval;

#ifdef WITH_GLEW_MX
	EGLEWContext *m_eglewContext;
#endif

	EGLContext &m_sharedContext;
	EGLint     &m_sharedCount;

	static EGLContext s_gl_sharedContext;
	static EGLint     s_gl_sharedCount;

	static EGLContext s_gles_sharedContext;
	static EGLint     s_gles_sharedCount;

	static EGLContext s_vg_sharedContext;
	static EGLint     s_vg_sharedCount;

#ifdef WITH_GL_ANGLE
	static HMODULE s_d3dcompiler;
#endif
};

#endif // __GHOST_CONTEXTEGL_H__
