/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GPW_Canvas.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GPW_Canvas::GPW_Canvas(HWND hWnd, HDC hDC, int width, int height)
	: GPC_Canvas(width, height), m_hWnd(hWnd), m_hRC(0), m_hDC(hDC)
{
}


GPW_Canvas::~GPW_Canvas()
{
	if (m_hRC) {
 		::wglDeleteContext(m_hRC);
	}
	//if (m_hDC) {
	//	::ReleaseDC(m_hWnd, m_hDC);
	//}
}


void GPW_Canvas::Init()
{

// 	log_entry("GPW_Canvas::Init");

	/*
	 * Color and depth bit values are not to be trusted.
	 * For instance, on TNT2:
	 * When the screen color depth is set to 16 bit, we get 5 color bits
	 * and 16 depth bits.
	 * When the screen color depth is set to 32 bit, we get 8 color bits
	 * and 24 depth bits.
	 * Just to be safe, we request high quality settings.
	 */
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// iSize
		1,								// iVersion
		PFD_DRAW_TO_WINDOW |
		PFD_SUPPORT_OPENGL |
//		PFD_STEREO |
		PFD_DOUBLEBUFFER,				// dwFlags
		PFD_TYPE_RGBA,					// iPixelType
		32,								// cColorBits
		0, 0,							// cRedBits, cRedShift (ignored)
		0, 0,							// cGreenBits, cGreenShift (ignored)
		0, 0,							// cBlueBits, cBlueShift (ignored)
        0, 0,							// cAlphaBits, cAlphaShift (ignored)
		0, 0, 0, 0, 0,					// cAccum_X_Bits
		32,								// cDepthBits
		0,								// cStencilBits
		0,								// cAuxBuffers
		PFD_MAIN_PLANE,					// iLayerType
		0,								// bReserved
		0,								// dwLayerMask
		0,								// dwVisibleMask
		0								// dwDamageMask
	};
	PIXELFORMATDESCRIPTOR match;

	// Look what we get back for this pixel format
	int pixelFormat = ::ChoosePixelFormat(m_hDC, &pfd);
	if (!pixelFormat) {
		DWORD error = ::GetLastError();
	}
	::DescribePixelFormat(m_hDC, pixelFormat, sizeof(match), &match);

	// Activate the pixel format for this context
	::SetPixelFormat(m_hDC, ::ChoosePixelFormat(m_hDC, &match), &match);

	// Create the OpenGL context and make it current
	m_hRC = ::wglCreateContext(m_hDC);
	::wglMakeCurrent(m_hDC, m_hRC);

}

void GPW_Canvas::SetMousePosition(int x, int y)
{
	POINT point = { x, y };
	if (m_hWnd)
	{
		::ClientToScreen(m_hWnd, &point);
		::SetCursorPos(point.x, point.y);
	}
}


void GPW_Canvas::SetMouseState(RAS_MouseState mousestate)
{
	LPCSTR id;
	switch (mousestate)
	{
	case MOUSE_INVISIBLE:
		HideCursor();
		break;
	case MOUSE_WAIT:
		::SetCursor(::LoadCursor(0, IDC_WAIT));
		ShowCursor();
		break;
	case MOUSE_NORMAL:
		::SetCursor(::LoadCursor(0, IDC_ARROW));
		ShowCursor();
		break;
	}
}


bool GPW_Canvas::BeginDraw(void)
{
	::wglMakeCurrent(m_hDC, m_hRC);
	// check errors, anyone?
	return true;
}


void GPW_Canvas::EndDraw(void)
{
	::wglMakeCurrent(NULL, NULL);
}

void GPW_Canvas::SwapBuffers(void)
{
	if (m_hDC) {
		::SwapBuffers(m_hDC);
	}
}


void GPW_Canvas::HideCursor(void)
{
	int count = ::ShowCursor(FALSE);
	while (count >= 0) 
	{
		count = ::ShowCursor(FALSE);
	}
}


void GPW_Canvas::ShowCursor(void)
{
	::ShowCursor(TRUE);
}

