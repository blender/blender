/**
 * $Id$
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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	May 10, 2001
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "GHOST_WindowWin32.h"
#include "GHOST_SystemWin32.h"
#include "GHOST_DropTargetWin32.h"
#include <GL/gl.h>
#include <math.h>

// MSVC6 still doesn't define M_PI
#ifndef M_PI
#define M_PI 3.1415926536
#endif

// win64 doesn't define GWL_USERDATA
#ifdef WIN32
#ifndef GWL_USERDATA
#define GWL_USERDATA GWLP_USERDATA
#define GWL_WNDPROC GWLP_WNDPROC
#endif
#endif

LPCSTR GHOST_WindowWin32::s_windowClassName = "GHOST_WindowClass";
const int GHOST_WindowWin32::s_maxTitleLength = 128;
HGLRC GHOST_WindowWin32::s_firsthGLRc = NULL;

static int WeightPixelFormat(PIXELFORMATDESCRIPTOR& pfd);
static int EnumPixelFormats(HDC hdc);

/*
 * Color and depth bit values are not to be trusted.
 * For instance, on TNT2:
 * When the screen color depth is set to 16 bit, we get 5 color bits
 * and 16 depth bits.
 * When the screen color depth is set to 32 bit, we get 8 color bits
 * and 24 depth bits.
 * Just to be safe, we request high waulity settings.
 */
static PIXELFORMATDESCRIPTOR sPreferredFormat = {
	sizeof(PIXELFORMATDESCRIPTOR),  /* size */
	1,                              /* version */
	PFD_SUPPORT_OPENGL |
	PFD_DRAW_TO_WINDOW |
	PFD_SWAP_COPY |					/* support swap copy */
	PFD_DOUBLEBUFFER,               /* support double-buffering */
	PFD_TYPE_RGBA,                  /* color type */
	32,                             /* prefered color depth */
	0, 0, 0, 0, 0, 0,               /* color bits (ignored) */
	0,                              /* no alpha buffer */
	0,                              /* alpha bits (ignored) */
	0,                              /* no accumulation buffer */
	0, 0, 0, 0,                     /* accum bits (ignored) */
	32,                             /* depth buffer */
	0,                              /* no stencil buffer */
	0,                              /* no auxiliary buffers */
	PFD_MAIN_PLANE,                 /* main layer */
	0,                              /* reserved */
	0, 0, 0                         /* no layer, visible, damage masks */
};

GHOST_WindowWin32::GHOST_WindowWin32(
	GHOST_SystemWin32 * system,
	const STR_String& title,
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	const bool stereoVisual)
:
	GHOST_Window(title, left, top, width, height, state, GHOST_kDrawingContextTypeNone,
	stereoVisual),
	m_system(system),
	m_hDC(0),
	m_hGlRc(0),
	m_hasMouseCaptured(false),
	m_nPressedButtons(0),
	m_customCursor(0),
	m_wintab(NULL),
	m_tabletData(NULL),
	m_tablet(0),
	m_maxPressure(0)
{
	if (state != GHOST_kWindowStateFullScreen) {
		RECT rect;
		GHOST_TUns32 tw, th; 

		width += GetSystemMetrics(SM_CXSIZEFRAME)*2;
		height += GetSystemMetrics(SM_CYSIZEFRAME)*2 + GetSystemMetrics(SM_CYCAPTION);

		// take taskbar into account
		SystemParametersInfo(SPI_GETWORKAREA,0,&rect,0);
		th = rect.bottom - rect.top;
		tw = rect.right - rect.left;

		if(tw < width)
		{
			width = tw;
			left = rect.left;
		}
		if(th < height)
		{
			height = th;
			top = rect.top;
		}

		m_hWnd = ::CreateWindow(
			s_windowClassName,			// pointer to registered class name
			title,						// pointer to window name
			WS_OVERLAPPEDWINDOW,		// window style
			left,					// horizontal position of window
			top,					// vertical position of window
			width,						// window width
			height,						// window height
			0,							// handle to parent or owner window
			0,							// handle to menu or child-window identifier
			::GetModuleHandle(0),		// handle to application instance
			0);							// pointer to window-creation data
	}
	else {
		m_hWnd = ::CreateWindow(
			s_windowClassName,			// pointer to registered class name
			title,						// pointer to window name
			WS_POPUP | WS_MAXIMIZE,		// window style
			left,						// horizontal position of window
 			top,						// vertical position of window
			width,						// window width
			height,						// window height
			0,							// handle to parent or owner window
			0,							// handle to menu or child-window identifier
			::GetModuleHandle(0),		// handle to application instance
			0);							// pointer to window-creation data
	}
	if (m_hWnd) {
		// Register this window as a droptarget. Requires m_hWnd to be valid.
		// Note that OleInitialize(0) has to be called prior to this. Done in GHOST_SystemWin32.
		m_dropTarget = new GHOST_DropTargetWin32(this, m_system);
		// Store a pointer to this class in the window structure
		::SetWindowLongPtr(m_hWnd, GWL_USERDATA, (LONG_PTR)this);

		// Store the device context
		m_hDC = ::GetDC(m_hWnd);

		// Show the window
		int nCmdShow;
		switch (state) {
			case GHOST_kWindowStateMaximized:
				nCmdShow = SW_SHOWMAXIMIZED;
				break;
			case GHOST_kWindowStateMinimized:
				nCmdShow = SW_SHOWMINIMIZED;
				break;
			case GHOST_kWindowStateNormal:
			default:
				nCmdShow = SW_SHOWNORMAL;
				break;
		}
		setDrawingContextType(type);
		::ShowWindow(m_hWnd, nCmdShow);
		// Force an initial paint of the window
		::UpdateWindow(m_hWnd);
	}

	m_wintab = ::LoadLibrary("Wintab32.dll");
	if (m_wintab) {
		GHOST_WIN32_WTInfo fpWTInfo = ( GHOST_WIN32_WTInfo ) ::GetProcAddress( m_wintab, "WTInfoA" );
		GHOST_WIN32_WTOpen fpWTOpen = ( GHOST_WIN32_WTOpen ) ::GetProcAddress( m_wintab, "WTOpenA" );

		// let's see if we can initialize tablet here
		/* check if WinTab available. */
		if (fpWTInfo && fpWTInfo(0, 0, NULL)) {
			// Now init the tablet
			LOGCONTEXT lc;
			AXIS TabletX, TabletY, Pressure, Orientation[3]; /* The maximum tablet size, pressure and orientation (tilt) */

			// Open a Wintab context

			// Get default context information
			fpWTInfo( WTI_DEFCONTEXT, 0, &lc );

			// Open the context
			lc.lcPktData = PACKETDATA;
			lc.lcPktMode = PACKETMODE;
			lc.lcOptions |= CXO_MESSAGES | CXO_SYSTEM;

			/* Set the entire tablet as active */
			fpWTInfo(WTI_DEVICES,DVC_X,&TabletX);
			fpWTInfo(WTI_DEVICES,DVC_Y,&TabletY);

			/* get the max pressure, to divide into a float */
			BOOL pressureSupport = fpWTInfo (WTI_DEVICES, DVC_NPRESSURE, &Pressure);
			if (pressureSupport)
				m_maxPressure = Pressure.axMax;
			else
				m_maxPressure = 0;

			/* get the max tilt axes, to divide into floats */
			BOOL tiltSupport = fpWTInfo (WTI_DEVICES, DVC_ORIENTATION, &Orientation);
			if (tiltSupport) {
				/* does the tablet support azimuth ([0]) and altitude ([1]) */
				if (Orientation[0].axResolution && Orientation[1].axResolution) {
					/* all this assumes the minimum is 0 */
					m_maxAzimuth = Orientation[0].axMax;
					m_maxAltitude = Orientation[1].axMax;
				}
				else {  /* no so dont do tilt stuff */
					m_maxAzimuth = m_maxAltitude = 0;
				}
			}

			if (fpWTOpen) {
				m_tablet = fpWTOpen( m_hWnd, &lc, TRUE );
				if (m_tablet) {
					m_tabletData = new GHOST_TabletData();
					m_tabletData->Active = GHOST_kTabletModeNone;
				}
			}
		}
	}
}


GHOST_WindowWin32::~GHOST_WindowWin32()
{
	if (m_wintab) {
		GHOST_WIN32_WTClose fpWTClose = ( GHOST_WIN32_WTClose ) ::GetProcAddress( m_wintab, "WTClose" );
		if (fpWTClose) {
			if (m_tablet)
				fpWTClose(m_tablet);
			if (m_tabletData)
				delete m_tabletData;
				m_tabletData = NULL;
		}
	}
	if (m_customCursor) {
		DestroyCursor(m_customCursor);
		m_customCursor = NULL;
	}

	setDrawingContextType(GHOST_kDrawingContextTypeNone);
	if (m_hDC) {
		::ReleaseDC(m_hWnd, m_hDC);
		m_hDC = 0;
	}
	if (m_hWnd) {
		m_dropTarget->Release(); // frees itself.
		::DestroyWindow(m_hWnd);
		m_hWnd = 0;
	}
}

bool GHOST_WindowWin32::getValid() const
{
	return m_hWnd != 0;
}

HWND GHOST_WindowWin32::getHWND() const
{
	return m_hWnd;
}

void GHOST_WindowWin32::setTitle(const STR_String& title)
{
	::SetWindowText(m_hWnd, title);
}


void GHOST_WindowWin32::getTitle(STR_String& title) const
{
	char buf[s_maxTitleLength];
	::GetWindowText(m_hWnd, buf, s_maxTitleLength);
	STR_String temp (buf);
	title = buf;
}


void GHOST_WindowWin32::getWindowBounds(GHOST_Rect& bounds) const
{
	RECT rect;
	::GetWindowRect(m_hWnd, &rect);
	bounds.m_b = rect.bottom;
	bounds.m_l = rect.left;
	bounds.m_r = rect.right;
	bounds.m_t = rect.top;
}


void GHOST_WindowWin32::getClientBounds(GHOST_Rect& bounds) const
{
	RECT rect;

	LONG_PTR result = ::GetWindowLongPtr(m_hWnd, GWL_STYLE);
	::GetWindowRect(m_hWnd, &rect);

	if((result & (WS_POPUP | WS_MAXIMIZE)) != (WS_POPUP | WS_MAXIMIZE)) {
		bounds.m_b = rect.bottom-GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYSIZEFRAME)*2;
		bounds.m_l = rect.left;
		bounds.m_r = rect.right-GetSystemMetrics(SM_CYSIZEFRAME)*2;
		bounds.m_t = rect.top;
	} else {
		::GetWindowRect(m_hWnd, &rect);
		bounds.m_b = rect.bottom;
		bounds.m_l = rect.left;
		bounds.m_r = rect.right;
		bounds.m_t = rect.top;
	}
}


GHOST_TSuccess GHOST_WindowWin32::setClientWidth(GHOST_TUns32 width)
{
	GHOST_TSuccess success;
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (cBnds.getWidth() != (GHOST_TInt32)width) {
		getWindowBounds(wBnds);
		int cx = wBnds.getWidth() + width - cBnds.getWidth();
		int cy = wBnds.getHeight();
		success =  ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
			GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		success = GHOST_kSuccess;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowWin32::setClientHeight(GHOST_TUns32 height)
{
	GHOST_TSuccess success;
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (cBnds.getHeight() != (GHOST_TInt32)height) {
		getWindowBounds(wBnds);
		int cx = wBnds.getWidth();
		int cy = wBnds.getHeight() + height - cBnds.getHeight();
		success = ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
			GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		success = GHOST_kSuccess;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowWin32::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
	GHOST_TSuccess success;
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if ((cBnds.getWidth() != (GHOST_TInt32)width) || (cBnds.getHeight() != (GHOST_TInt32)height)) {
		getWindowBounds(wBnds);
		int cx = wBnds.getWidth() + width - cBnds.getWidth();
		int cy = wBnds.getHeight() + height - cBnds.getHeight();
		success = ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
			GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		success = GHOST_kSuccess;
	}
	return success;
}


GHOST_TWindowState GHOST_WindowWin32::getState() const
{
	GHOST_TWindowState state;
	if (::IsIconic(m_hWnd)) {
		state = GHOST_kWindowStateMinimized;
	}
	else if (::IsZoomed(m_hWnd)) {
		LONG_PTR result = ::GetWindowLongPtr(m_hWnd, GWL_STYLE);
		if((result & (WS_POPUP | WS_MAXIMIZE)) != (WS_POPUP | WS_MAXIMIZE))
			state = GHOST_kWindowStateMaximized;
		else
			state = GHOST_kWindowStateFullScreen;
	}
	else {
		state = GHOST_kWindowStateNormal;
	}
	return state;
}


void GHOST_WindowWin32::screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	POINT point = { inX, inY };
	::ScreenToClient(m_hWnd, &point);
	outX = point.x;
	outY = point.y;
}


void GHOST_WindowWin32::clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	POINT point = { inX, inY };
	::ClientToScreen(m_hWnd, &point);
	outX = point.x;
	outY = point.y;
}


GHOST_TSuccess GHOST_WindowWin32::setState(GHOST_TWindowState state)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	::GetWindowPlacement(m_hWnd, &wp);
	switch (state) {
	case GHOST_kWindowStateMinimized:
		wp.showCmd = SW_SHOWMINIMIZED;
		break;
	case GHOST_kWindowStateMaximized:
		ShowWindow(m_hWnd, SW_HIDE);
		wp.showCmd = SW_SHOWMAXIMIZED;
		SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		break;
	case GHOST_kWindowStateFullScreen:
		wp.showCmd = SW_SHOWMAXIMIZED;
		wp.ptMaxPosition.x = 0;
		wp.ptMaxPosition.y = 0;
		SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_POPUP | WS_MAXIMIZE);
		break;
	case GHOST_kWindowStateNormal:
	default:
		ShowWindow(m_hWnd, SW_HIDE);
		wp.showCmd = SW_SHOWNORMAL;
		SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
		break;
	}
	return ::SetWindowPlacement(m_hWnd, &wp) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}


GHOST_TSuccess GHOST_WindowWin32::setOrder(GHOST_TWindowOrder order)
{
	HWND hWndInsertAfter = order == GHOST_kWindowOrderTop ? HWND_TOP : HWND_BOTTOM;
	return ::SetWindowPos(m_hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}


GHOST_TSuccess GHOST_WindowWin32::swapBuffers()
{
	return ::SwapBuffers(m_hDC) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}


GHOST_TSuccess GHOST_WindowWin32::activateDrawingContext()
{
	GHOST_TSuccess success;
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_hDC && m_hGlRc) {
			success = ::wglMakeCurrent(m_hDC, m_hGlRc) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
		}
		else {
			success = GHOST_kFailure;
		}
	}
	else {
		success = GHOST_kSuccess;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowWin32::invalidate()
{
	GHOST_TSuccess success;
	if (m_hWnd) {
		success = ::InvalidateRect(m_hWnd, 0, FALSE) != 0 ? GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowWin32::installDrawingContext(GHOST_TDrawingContextType type)
{
	GHOST_TSuccess success;
	switch (type) {
	case GHOST_kDrawingContextTypeOpenGL:
		{
		if(m_stereoVisual)
			sPreferredFormat.dwFlags |= PFD_STEREO;

		// Attempt to match device context pixel format to the preferred format
		int iPixelFormat = EnumPixelFormats(m_hDC);
		if (iPixelFormat == 0) {
			success = GHOST_kFailure;
			break;
		}
		if (::SetPixelFormat(m_hDC, iPixelFormat, &sPreferredFormat) == FALSE) {
			success = GHOST_kFailure;
			break;
		}
		// For debugging only: retrieve the pixel format chosen
		PIXELFORMATDESCRIPTOR preferredFormat;
		::DescribePixelFormat(m_hDC, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &preferredFormat);
		// Create the context
		m_hGlRc = ::wglCreateContext(m_hDC);
		if (m_hGlRc) {
			if (s_firsthGLRc) {
				wglShareLists(s_firsthGLRc, m_hGlRc);
			} else {
				s_firsthGLRc = m_hGlRc;
			}

			success = ::wglMakeCurrent(m_hDC, m_hGlRc) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
		}
		else {
			success = GHOST_kFailure;
		}
		}
		break;

	case GHOST_kDrawingContextTypeNone:
		success = GHOST_kSuccess;
		break;

	default:
		success = GHOST_kFailure;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowWin32::removeDrawingContext()
{
	GHOST_TSuccess success;
	switch (m_drawingContextType) {
	case GHOST_kDrawingContextTypeOpenGL:
		if (m_hGlRc) {
			success = ::wglDeleteContext(m_hGlRc) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
			if (m_hGlRc == s_firsthGLRc) {
				s_firsthGLRc = NULL;
			}
			m_hGlRc = 0;
		}
		else {
			success = GHOST_kFailure;
		}
		break;
	case GHOST_kDrawingContextTypeNone:
		success = GHOST_kSuccess;
		break;
	default:
		success = GHOST_kFailure;
	}
	return success;
}

void GHOST_WindowWin32::lostMouseCapture()
{
	if (m_hasMouseCaptured) {
		m_hasMouseCaptured = false;
		m_nPressedButtons = 0;
	}
}

void GHOST_WindowWin32::registerMouseClickEvent(bool press)
{
	if (press) {
		if (!m_hasMouseCaptured) {
			::SetCapture(m_hWnd);
			m_hasMouseCaptured = true;
		}
		m_nPressedButtons++;
	} else {
		if (m_nPressedButtons) {
			m_nPressedButtons--;
			if (!m_nPressedButtons) {
				::ReleaseCapture();
				m_hasMouseCaptured = false;
			}
		}
	}
}


void GHOST_WindowWin32::loadCursor(bool visible, GHOST_TStandardCursor cursor) const
{
	if (!visible) {
		while (::ShowCursor(FALSE) >= 0);
	}
	else {
		while (::ShowCursor(TRUE) < 0);
	}

	if (cursor == GHOST_kStandardCursorCustom && m_customCursor) {
		::SetCursor( m_customCursor );
	} else {
		// Convert GHOST cursor to Windows OEM cursor
		bool success = true;
		LPCSTR id;
		switch (cursor) {
			case GHOST_kStandardCursorDefault:		id = IDC_ARROW; 	break;
			case GHOST_kStandardCursorRightArrow:		id = IDC_ARROW;		break;
			case GHOST_kStandardCursorLeftArrow:		id = IDC_ARROW;		break;
			case GHOST_kStandardCursorInfo:			id = IDC_SIZEALL;	break;	// Four-pointed arrow pointing north, south, east, and west
			case GHOST_kStandardCursorDestroy:		id = IDC_NO;		break;	// Slashed circle
			case GHOST_kStandardCursorHelp:			id = IDC_HELP;		break;	// Arrow and question mark
			case GHOST_kStandardCursorCycle:		id = IDC_NO;		break;	// Slashed circle
			case GHOST_kStandardCursorSpray:		id = IDC_SIZEALL;	break;	// Four-pointed arrow pointing north, south, east, and west
			case GHOST_kStandardCursorWait:			id = IDC_WAIT;		break;	// Hourglass
			case GHOST_kStandardCursorText:			id = IDC_IBEAM;		break;	// I-beam
			case GHOST_kStandardCursorCrosshair:		id = IDC_CROSS;		break;	// Crosshair
			case GHOST_kStandardCursorUpDown:		id = IDC_SIZENS;	break;	// Double-pointed arrow pointing north and south
			case GHOST_kStandardCursorLeftRight:		id = IDC_SIZEWE;	break;	// Double-pointed arrow pointing west and east
			case GHOST_kStandardCursorTopSide:		id = IDC_UPARROW;	break;	// Vertical arrow
			case GHOST_kStandardCursorBottomSide:		id = IDC_SIZENS;	break;
			case GHOST_kStandardCursorLeftSide:		id = IDC_SIZEWE;	break;
			case GHOST_kStandardCursorTopLeftCorner:	id = IDC_SIZENWSE;	break;
			case GHOST_kStandardCursorTopRightCorner:	id = IDC_SIZENESW;	break;
			case GHOST_kStandardCursorBottomRightCorner:	id = IDC_SIZENWSE;	break;
			case GHOST_kStandardCursorBottomLeftCorner:	id = IDC_SIZENESW;	break;
			case GHOST_kStandardCursorPencil:		id = IDC_ARROW; 	break;
			default:
			success = false;
		}

		if (success) {
			::SetCursor(::LoadCursor(0, id));
		}
	}
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorVisibility(bool visible)
{
	if (::GetForegroundWindow() == m_hWnd) {
		loadCursor(visible, getCursorShape());
	}

	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
	if(mode != GHOST_kGrabDisable) {
		if(mode != GHOST_kGrabNormal) {
			m_system->getCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
			setCursorGrabAccum(0, 0);

			if(mode == GHOST_kGrabHide)
				setWindowCursorVisibility(false);
		}
		registerMouseClickEvent(true);
	}
	else {
		if (m_cursorGrab==GHOST_kGrabHide) {
			m_system->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
			setWindowCursorVisibility(true);
		}
		if(m_cursorGrab != GHOST_kGrabNormal) {
			/* use to generate a mouse move event, otherwise the last event
			 * blender gets can be outside the screen causing menus not to show
			 * properly unless the user moves the mouse */
			 GHOST_TInt32 pos[2];
			 m_system->getCursorPosition(pos[0], pos[1]);
			 m_system->setCursorPosition(pos[0], pos[1]);
		}

		/* Almost works without but important otherwise the mouse GHOST location can be incorrect on exit */
		setCursorGrabAccum(0, 0);
		m_cursorGrabBounds.m_l= m_cursorGrabBounds.m_r= -1; /* disable */
		registerMouseClickEvent(false);
	}
	
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorShape(GHOST_TStandardCursor cursorShape)
{
	if (m_customCursor) {
		DestroyCursor(m_customCursor);
		m_customCursor = NULL;
	}

	if (::GetForegroundWindow() == m_hWnd) {
		loadCursor(getCursorVisibility(), cursorShape);
	}

	return GHOST_kSuccess;
}

void GHOST_WindowWin32::processWin32TabletInitEvent()
{
	if (m_wintab) {
		GHOST_WIN32_WTInfo fpWTInfo = ( GHOST_WIN32_WTInfo ) ::GetProcAddress( m_wintab, "WTInfoA" );

		// let's see if we can initialize tablet here
		/* check if WinTab available. */
		if (fpWTInfo) {
			AXIS Pressure, Orientation[3]; /* The maximum tablet size */

			BOOL pressureSupport = fpWTInfo (WTI_DEVICES, DVC_NPRESSURE, &Pressure);
			if (pressureSupport)
				m_maxPressure = Pressure.axMax;
			else
				m_maxPressure = 0;

			BOOL tiltSupport = fpWTInfo (WTI_DEVICES, DVC_ORIENTATION, &Orientation);
			if (tiltSupport) {
				/* does the tablet support azimuth ([0]) and altitude ([1]) */
				if (Orientation[0].axResolution && Orientation[1].axResolution) {
					m_maxAzimuth = Orientation[0].axMax;
					m_maxAltitude = Orientation[1].axMax;
				}
				else {  /* no so dont do tilt stuff */
					m_maxAzimuth = m_maxAltitude = 0;
				}
			}

			m_tabletData->Active = GHOST_kTabletModeNone;
		}
	}
}

void GHOST_WindowWin32::processWin32TabletEvent(WPARAM wParam, LPARAM lParam)
{
	PACKET pkt;
	if (m_wintab) {
		GHOST_WIN32_WTPacket fpWTPacket = ( GHOST_WIN32_WTPacket ) ::GetProcAddress( m_wintab, "WTPacket" );
		if (fpWTPacket) {
			if (fpWTPacket((HCTX)lParam, wParam, &pkt)) {
				if (m_tabletData) {
					switch (pkt.pkCursor) {
						case 0: /* first device */
						case 3: /* second device */
							m_tabletData->Active = GHOST_kTabletModeNone; /* puck - not yet supported */
							break;
						case 1:
						case 4:
							m_tabletData->Active = GHOST_kTabletModeStylus; /* stylus */
							break;
						case 2:
						case 5:
							m_tabletData->Active = GHOST_kTabletModeEraser; /* eraser */
							break;
					}
					if (m_maxPressure > 0) {
						m_tabletData->Pressure = (float)pkt.pkNormalPressure / (float)m_maxPressure;
					} else {
						m_tabletData->Pressure = 1.0f;
					}

					if ((m_maxAzimuth > 0) && (m_maxAltitude > 0)) {
						ORIENTATION ort = pkt.pkOrientation;
						float vecLen;
						float altRad, azmRad;	/* in radians */

						/*
						from the wintab spec:
						orAzimuth	Specifies the clockwise rotation of the
						cursor about the z axis through a full circular range.

						orAltitude	Specifies the angle with the x-y plane
						through a signed, semicircular range.  Positive values
						specify an angle upward toward the positive z axis;
						negative values specify an angle downward toward the negative z axis.

						wintab.h defines .orAltitude as a UINT but documents .orAltitude
						as positive for upward angles and negative for downward angles.
						WACOM uses negative altitude values to show that the pen is inverted;
						therefore we cast .orAltitude as an (int) and then use the absolute value.
						*/

						/* convert raw fixed point data to radians */
						altRad = (float)((fabs((float)ort.orAltitude)/(float)m_maxAltitude) * M_PI/2.0);
						azmRad = (float)(((float)ort.orAzimuth/(float)m_maxAzimuth) * M_PI*2.0);

						/* find length of the stylus' projected vector on the XY plane */
						vecLen = cos(altRad);

						/* from there calculate X and Y components based on azimuth */
						m_tabletData->Xtilt = sin(azmRad) * vecLen;
						m_tabletData->Ytilt = (float)(sin(M_PI/2.0 - azmRad) * vecLen);

					} else {
						m_tabletData->Xtilt = 0.0f;
						m_tabletData->Ytilt = 0.0f;
					}
				}
			}
		}
	}
}

/** Reverse the bits in a GHOST_TUns8 */
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
	ch= ((ch>>1)&0x55) | ((ch<<1)&0xAA);
	ch= ((ch>>2)&0x33) | ((ch<<2)&0xCC);
	ch= ((ch>>4)&0x0F) | ((ch<<4)&0xF0);
	return ch;
}

/** Reverse the bits in a GHOST_TUns16 */
static GHOST_TUns16 uns16ReverseBits(GHOST_TUns16 shrt)
{
	shrt= ((shrt>>1)&0x5555) | ((shrt<<1)&0xAAAA);
	shrt= ((shrt>>2)&0x3333) | ((shrt<<2)&0xCCCC);
	shrt= ((shrt>>4)&0x0F0F) | ((shrt<<4)&0xF0F0);
	shrt= ((shrt>>8)&0x00FF) | ((shrt<<8)&0xFF00);
	return shrt;
}
GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
					GHOST_TUns8 mask[16][2], int hotX, int hotY)
{
	return setWindowCustomCursorShape((GHOST_TUns8*)bitmap, (GHOST_TUns8*)mask,
									16, 16, hotX, hotY, 0, 1);
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
					GHOST_TUns8 *mask, int sizeX, int sizeY, int hotX, int hotY,
					int fg_color, int bg_color)
{
	GHOST_TUns32 andData[32];
	GHOST_TUns32 xorData[32];
	GHOST_TUns32 fullBitRow, fullMaskRow;
	int x, y, cols;

	cols=sizeX/8; /* Num of whole bytes per row (width of bm/mask) */
	if (sizeX%8) cols++;

	if (m_customCursor) {
		DestroyCursor(m_customCursor);
		m_customCursor = NULL;
	}

	memset(&andData, 0xFF, sizeof(andData));
	memset(&xorData, 0, sizeof(xorData));

	for (y=0; y<sizeY; y++) {
		fullBitRow=0;
		fullMaskRow=0;
		for (x=cols-1; x>=0; x--){
			fullBitRow<<=8;
			fullMaskRow<<=8;
			fullBitRow  |= uns8ReverseBits(bitmap[cols*y + x]);
			fullMaskRow |= uns8ReverseBits(  mask[cols*y + x]);
		}
		xorData[y]= fullBitRow & fullMaskRow;
		andData[y]= ~fullMaskRow;
	}

	m_customCursor = ::CreateCursor(::GetModuleHandle(0), hotX, hotY, 32, 32, andData, xorData);
	if (!m_customCursor) {
		return GHOST_kFailure;
	}

	if (::GetForegroundWindow() == m_hWnd) {
		loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
	}

	return GHOST_kSuccess;
}


/*  Ron Fosner's code for weighting pixel formats and forcing software.
	See http://www.opengl.org/resources/faq/technical/weight.cpp */

static int WeightPixelFormat(PIXELFORMATDESCRIPTOR& pfd) {
	int weight = 0;

	/* assume desktop color depth is 32 bits per pixel */

	/* cull unusable pixel formats */
	/* if no formats can be found, can we determine why it was rejected? */
	if( !(pfd.dwFlags & PFD_SUPPORT_OPENGL) ||
		!(pfd.dwFlags & PFD_DRAW_TO_WINDOW) ||
		!(pfd.dwFlags & PFD_DOUBLEBUFFER) || /* Blender _needs_ this */
		( pfd.cDepthBits <= 8 ) ||
		!(pfd.iPixelType == PFD_TYPE_RGBA) )
		return 0;

	weight = 1;  /* it's usable */

	/* the bigger the depth buffer the better */
	/* give no weight to a 16-bit depth buffer, because those are crap */
	weight += pfd.cDepthBits - 16;

	weight += pfd.cColorBits - 8;

	/* want swap copy capability -- it matters a lot */
	if(pfd.dwFlags & PFD_SWAP_COPY)	weight += 16;

	/* but if it's a generic (not accelerated) view, it's really bad */
	if(pfd.dwFlags & PFD_GENERIC_FORMAT) weight /= 10;

	return weight;
}

/* A modification of Ron Fosner's replacement for ChoosePixelFormat */
/* returns 0 on error, else returns the pixel format number to be used */
static int EnumPixelFormats(HDC hdc) {
	int iPixelFormat;
	int i, n, w, weight = 0;
	PIXELFORMATDESCRIPTOR pfd;

	/* we need a device context to do anything */
	if(!hdc) return 0;

	iPixelFormat = 1; /* careful! PFD numbers are 1 based, not zero based */

	/* obtain detailed information about
	the device context's first pixel format */
	n = 1+::DescribePixelFormat(hdc, iPixelFormat,
		sizeof(PIXELFORMATDESCRIPTOR), &pfd);

	/* choose a pixel format using the useless Windows function in case
		we come up empty handed */
	iPixelFormat = ::ChoosePixelFormat( hdc, &sPreferredFormat );

	if(!iPixelFormat) return 0; /* couldn't find one to use */

	for(i=1; i<=n; i++) { /* not the idiom, but it's right */
		::DescribePixelFormat( hdc, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd );
		w = WeightPixelFormat(pfd);
		// be strict on stereo
		if (!((sPreferredFormat.dwFlags ^ pfd.dwFlags) & PFD_STEREO))	{
			if(w > weight) {
				weight = w;
				iPixelFormat = i;
			}
		}
	}
	if (weight == 0) {
		// we could find the correct stereo setting, just find any suitable format
		for(i=1; i<=n; i++) { /* not the idiom, but it's right */
			::DescribePixelFormat( hdc, i, sizeof(PIXELFORMATDESCRIPTOR), &pfd );
			w = WeightPixelFormat(pfd);
			if(w > weight) {
				weight = w;
				iPixelFormat = i;
			}
		}
	}
	return iPixelFormat;
}


