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

/** \file ghost/intern/GHOST_ContextWGL.h
 *  \ingroup GHOST
 */

#ifndef __GHOST_CONTEXTWGL_H__
#define __GHOST_CONTEXTWGL_H__

#include "GHOST_Context.h"

#ifdef WITH_GLEW_MX
#define wglewGetContext() wglewContext
#endif

#include <GL/wglew.h>

#ifdef WITH_GLEW_MX 
extern "C" WGLEWContext *wglewContext;
#endif

#ifndef GHOST_OPENGL_WGL_CONTEXT_FLAGS
#define GHOST_OPENGL_WGL_CONTEXT_FLAGS 0
#endif

#ifndef GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY
#define GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY 0
#endif


class GHOST_ContextWGL : public GHOST_Context
{
public:
	/**
	 * Constructor.
	 */
	GHOST_ContextWGL(
	        bool stereoVisual,
	        GHOST_TUns16 numOfAASamples,
	        HWND hWnd,
	        HDC hDC,
	        int contextProfileMask,
	        int contextMajorVersion,
	        int contextMinorVersion,
	        int contextFlags,
	        int contextResetNotificationStrategy);

	/**
	 * Destructor.
	 */
	virtual ~GHOST_ContextWGL();

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
	virtual GHOST_TSuccess getSwapInterval(int &intervalOut);

protected:
	inline void activateWGLEW() const {
#ifdef WITH_GLEW_MX 
		wglewContext = m_wglewContext;
#endif
	}

private:
	int choose_pixel_format(
	        bool stereoVisual,
	        int numOfAASamples,
	        bool needAlpha,
	        bool needStencil,
	        bool sRGB);

	int choose_pixel_format_arb(
	        bool stereoVisual,
	        int numOfAASamples,
	        bool needAlpha,
	        bool needStencil,
	        bool sRGB);

	int _choose_pixel_format_arb_1(
	        bool stereoVisual,
	        int numOfAASamples,
	        bool needAlpha,
	        bool needStencil,
	        bool sRGB,
	        int &swapMethodOut);

	int _choose_pixel_format_arb_2(
	        bool stereoVisual,
	        int numOfAASamples,
	        bool needAlpha,
	        bool needStencil,
	        bool sRGB,
	        int  swapMethod);

	void initContextWGLEW(PIXELFORMATDESCRIPTOR &preferredPFD);

	HDC  m_hDC;
	HWND m_hWnd;

	const int m_contextProfileMask;
	const int m_contextMajorVersion;
	const int m_contextMinorVersion;
	const int m_contextFlags;
	const int m_contextResetNotificationStrategy;

	HGLRC m_hGLRC;

#ifdef WITH_GLEW_MX 
	WGLEWContext *m_wglewContext;
#endif
	
#ifndef NDEBUG
	const char *m_dummyVendor;
	const char *m_dummyRenderer;
	const char *m_dummyVersion;
#endif

	static HGLRC s_sharedHGLRC;
	static int   s_sharedCount;

	static bool s_singleContextMode;
};

#endif  // __GHOST_CONTEXTWGL_H__
