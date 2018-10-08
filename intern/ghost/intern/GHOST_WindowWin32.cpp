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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Maarten Gribnau.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_WindowWin32.cpp
 *  \ingroup GHOST
 */

#define _USE_MATH_DEFINES

#include "GHOST_WindowWin32.h"
#include "GHOST_SystemWin32.h"
#include "GHOST_DropTargetWin32.h"
#include "GHOST_ContextNone.h"
#include "utfconv.h"
#include "utf_winfunc.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextWGL.h"
#endif
#ifdef WIN32_COMPOSITING
#include <Dwmapi.h>
#endif

#include <math.h>
#include <string.h>
#include <assert.h>



const wchar_t *GHOST_WindowWin32::s_windowClassName = L"GHOST_WindowClass";
const int GHOST_WindowWin32::s_maxTitleLength = 128;




/* force NVidia Optimus to used dedicated graphics */
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

GHOST_WindowWin32::GHOST_WindowWin32(GHOST_SystemWin32 *system,
        const STR_String &title,
        GHOST_TInt32 left,
        GHOST_TInt32 top,
        GHOST_TUns32 width,
        GHOST_TUns32 height,
        GHOST_TWindowState state,
        GHOST_TDrawingContextType type,
	bool wantStereoVisual,
	bool alphaBackground,
        GHOST_TUns16 wantNumOfAASamples,
        GHOST_TEmbedderWindowID parentwindowhwnd,
        bool is_debug)
    : GHOST_Window(width, height, state,
                   wantStereoVisual, false, wantNumOfAASamples),
      m_inLiveResize(false),
      m_system(system),
      m_hDC(0),
      m_hasMouseCaptured(false),
      m_hasGrabMouse(false),
      m_nPressedButtons(0),
      m_customCursor(0),
      m_wantAlphaBackground(alphaBackground),
      m_wintab(NULL),
      m_tabletData(NULL),
      m_tablet(0),
      m_maxPressure(0),
      m_normal_state(GHOST_kWindowStateNormal),
	  m_user32(NULL),
      m_parentWindowHwnd(parentwindowhwnd),
      m_debug_context(is_debug)
{
	if (state != GHOST_kWindowStateFullScreen) {
		RECT rect;
		MONITORINFO monitor;
		GHOST_TUns32 tw, th;

#ifndef _MSC_VER
		int cxsizeframe = GetSystemMetrics(SM_CXSIZEFRAME);
		int cysizeframe = GetSystemMetrics(SM_CYSIZEFRAME);
#else
		// MSVC 2012+ returns bogus values from GetSystemMetrics, bug in Windows
		// http://connect.microsoft.com/VisualStudio/feedback/details/753224/regression-getsystemmetrics-delivers-different-values
		RECT cxrect = {0, 0, 0, 0};
		AdjustWindowRectEx(&cxrect, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_THICKFRAME | WS_DLGFRAME, FALSE, 0);

		int cxsizeframe = abs(cxrect.bottom);
		int cysizeframe = abs(cxrect.left);
#endif

		width += cxsizeframe * 2;
		height += cysizeframe * 2 + GetSystemMetrics(SM_CYCAPTION);

		rect.left = left;
		rect.right = left + width;
		rect.top = top;
		rect.bottom = top + height;

		monitor.cbSize = sizeof(monitor);
		monitor.dwFlags = 0;

		// take taskbar into account
		GetMonitorInfo(MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &monitor);

		th = monitor.rcWork.bottom - monitor.rcWork.top;
		tw = monitor.rcWork.right - monitor.rcWork.left;

		if (tw < width) {
			width = tw;
			left = monitor.rcWork.left;
		}
		else if (monitor.rcWork.right < left + (int)width)
			left = monitor.rcWork.right - width;
		else if (left < monitor.rcWork.left)
			left = monitor.rcWork.left;

		if (th < height) {
			height = th;
			top = monitor.rcWork.top;
		}
		else if (monitor.rcWork.bottom < top + (int)height)
			top = monitor.rcWork.bottom - height;
		else if (top < monitor.rcWork.top)
			top = monitor.rcWork.top;

		int wintype = WS_OVERLAPPEDWINDOW;
		if (m_parentWindowHwnd != 0) {
			wintype = WS_CHILD;
			GetWindowRect((HWND)m_parentWindowHwnd, &rect);
			left = 0;
			top = 0;
			width = rect.right - rect.left;
			height = rect.bottom - rect.top;
		}

		wchar_t *title_16 = alloc_utf16_from_8((char *)(const char *)title, 0);
		m_hWnd = ::CreateWindowW(
			s_windowClassName,          // pointer to registered class name
			title_16,                   // pointer to window name
			wintype,                    // window style
			left,                       // horizontal position of window
			top,                        // vertical position of window
			width,                      // window width
			height,                     // window height
			(HWND)m_parentWindowHwnd,  // handle to parent or owner window
			0,                          // handle to menu or child-window identifier
			::GetModuleHandle(0),       // handle to application instance
			0);                         // pointer to window-creation data
		free(title_16);
	}
	else {
		wchar_t *title_16 = alloc_utf16_from_8((char *)(const char *)title, 0);
		m_hWnd = ::CreateWindowW(
		    s_windowClassName,          // pointer to registered class name
		    title_16,                   // pointer to window name
		    WS_POPUP | WS_MAXIMIZE,     // window style
		    left,                       // horizontal position of window
		    top,                        // vertical position of window
		    width,                      // window width
		    height,                     // window height
		    HWND_DESKTOP,               // handle to parent or owner window
		    0,                          // handle to menu or child-window identifier
		    ::GetModuleHandle(0),       // handle to application instance
		    0);                         // pointer to window-creation data
		free(title_16);
	}
	if (m_hWnd) {
		// Register this window as a droptarget. Requires m_hWnd to be valid.
		// Note that OleInitialize(0) has to be called prior to this. Done in GHOST_SystemWin32.
		m_dropTarget = new GHOST_DropTargetWin32(this, m_system);
		if (m_dropTarget) {
			::RegisterDragDrop(m_hWnd, m_dropTarget);
		}

		// Store a pointer to this class in the window structure
		::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR) this);

		if (!m_system->m_windowFocus) {
			// Lower to bottom and don't activate if we don't want focus
			::SetWindowPos(m_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}

		// Store the device context
		m_hDC = ::GetDC(m_hWnd);

		GHOST_TSuccess success = setDrawingContextType(type);

		if (success) {
			// Show the window
			int nCmdShow;
			switch (state) {
				case GHOST_kWindowStateMaximized:
					nCmdShow = SW_SHOWMAXIMIZED;
					break;
				case GHOST_kWindowStateMinimized:
					nCmdShow = (m_system->m_windowFocus) ? SW_SHOWMINIMIZED : SW_SHOWMINNOACTIVE;
					break;
				case GHOST_kWindowStateNormal:
				default:
					nCmdShow = (m_system->m_windowFocus) ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE;
					break;
			}

			::ShowWindow(m_hWnd, nCmdShow);
#ifdef WIN32_COMPOSITING
			if (alphaBackground && parentwindowhwnd == 0) {

				HRESULT hr = S_OK;

				// Create and populate the Blur Behind structure
				DWM_BLURBEHIND bb = { 0 };

				// Enable Blur Behind and apply to the entire client area
				bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
				bb.fEnable = true;
				bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);

				// Apply Blur Behind
				hr = DwmEnableBlurBehindWindow(m_hWnd, &bb);
				DeleteObject(bb.hRgnBlur);
			}
#endif
			// Force an initial paint of the window
			::UpdateWindow(m_hWnd);
		}
		else {
			//invalidate the window
			::DestroyWindow(m_hWnd);
			m_hWnd = NULL;
		}
	}

	if (parentwindowhwnd != 0) {
		RAWINPUTDEVICE device = {0};
		device.usUsagePage  = 0x01; /* usUsagePage & usUsage for keyboard*/
		device.usUsage      = 0x06; /* http://msdn.microsoft.com/en-us/windows/hardware/gg487473.aspx */
		device.dwFlags |= RIDEV_INPUTSINK; // makes WM_INPUT is visible for ghost when has parent window
		device.hwndTarget = m_hWnd;
		RegisterRawInputDevices(&device, 1, sizeof(device));
	}

	m_wintab = ::LoadLibrary("Wintab32.dll");
	if (m_wintab) {
		GHOST_WIN32_WTInfo fpWTInfo = (GHOST_WIN32_WTInfo) ::GetProcAddress(m_wintab, "WTInfoA");
		GHOST_WIN32_WTOpen fpWTOpen = (GHOST_WIN32_WTOpen) ::GetProcAddress(m_wintab, "WTOpenA");

		// Let's see if we can initialize tablet here.
		// Check if WinTab available by getting system context info.
		LOGCONTEXT lc = { 0 };
		lc.lcOptions |= CXO_SYSTEM;
		if (fpWTInfo && fpWTInfo(WTI_DEFSYSCTX, 0, &lc)) {
			// Now init the tablet
			/* The maximum tablet size, pressure and orientation (tilt) */
			AXIS TabletX, TabletY, Pressure, Orientation[3];

			// Open a Wintab context

			// Open the context
			lc.lcPktData = PACKETDATA;
			lc.lcPktMode = PACKETMODE;
			lc.lcOptions |= CXO_MESSAGES;
			lc.lcMoveMask = PACKETDATA;

			/* Set the entire tablet as active */
			fpWTInfo(WTI_DEVICES, DVC_X, &TabletX);
			fpWTInfo(WTI_DEVICES, DVC_Y, &TabletY);

			/* get the max pressure, to divide into a float */
			BOOL pressureSupport = fpWTInfo(WTI_DEVICES, DVC_NPRESSURE, &Pressure);
			if (pressureSupport)
				m_maxPressure = Pressure.axMax;
			else
				m_maxPressure = 0;

			/* get the max tilt axes, to divide into floats */
			BOOL tiltSupport = fpWTInfo(WTI_DEVICES, DVC_ORIENTATION, &Orientation);
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
				// The Wintab spec says we must open the context disabled if we are using cursor masks.
				m_tablet = fpWTOpen(m_hWnd, &lc, FALSE);
				if (m_tablet) {
					m_tabletData = new GHOST_TabletData();
					m_tabletData->Active = GHOST_kTabletModeNone;
				}

				GHOST_WIN32_WTEnable fpWTEnable = (GHOST_WIN32_WTEnable) ::GetProcAddress(m_wintab, "WTEnable");
				if (fpWTEnable) {
					fpWTEnable(m_tablet, TRUE);
				}
			}
		}
	}
	CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (LPVOID *)&m_Bar);
}


GHOST_WindowWin32::~GHOST_WindowWin32()
{
	if (m_Bar) {
		m_Bar->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
		m_Bar->Release();
	}

	if (m_wintab) {
		GHOST_WIN32_WTClose fpWTClose = (GHOST_WIN32_WTClose) ::GetProcAddress(m_wintab, "WTClose");
		if (fpWTClose) {
			if (m_tablet)
				fpWTClose(m_tablet);
			delete m_tabletData;
			m_tabletData = NULL;
		}
	}

	if (m_customCursor) {
		DestroyCursor(m_customCursor);
		m_customCursor = NULL;
	}

	if (m_hWnd != NULL && m_hDC != NULL && releaseNativeHandles()) {
		::ReleaseDC(m_hWnd, m_hDC);
	}

	if (m_hWnd) {
		if (m_dropTarget) {
			// Disable DragDrop
			RevokeDragDrop(m_hWnd);
			// Release our reference of the DropTarget and it will delete itself eventually.
			m_dropTarget->Release();
		}
		::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, NULL);
		::DestroyWindow(m_hWnd);
		m_hWnd = 0;
	}
}

bool GHOST_WindowWin32::getValid() const
{
	return GHOST_Window::getValid() && m_hWnd != 0 && m_hDC != 0;
}

HWND GHOST_WindowWin32::getHWND() const
{
	return m_hWnd;
}

void GHOST_WindowWin32::setTitle(const STR_String &title)
{
	wchar_t *title_16 = alloc_utf16_from_8((char *)(const char *)title, 0);
	::SetWindowTextW(m_hWnd, (wchar_t *)title_16);
	free(title_16);
}


void GHOST_WindowWin32::getTitle(STR_String &title) const
{
	char buf[s_maxTitleLength]; /*CHANGE + never used yet*/
	::GetWindowText(m_hWnd, buf, s_maxTitleLength);
	STR_String temp(buf);
	title = buf;
}


void GHOST_WindowWin32::getWindowBounds(GHOST_Rect &bounds) const
{
	RECT rect;
	::GetWindowRect(m_hWnd, &rect);
	bounds.m_b = rect.bottom;
	bounds.m_l = rect.left;
	bounds.m_r = rect.right;
	bounds.m_t = rect.top;
}


void GHOST_WindowWin32::getClientBounds(GHOST_Rect &bounds) const
{
	RECT rect;
	POINT coord;
	if (!IsIconic(m_hWnd)) {
		::GetClientRect(m_hWnd, &rect);

		coord.x = rect.left;
		coord.y = rect.top;
		::ClientToScreen(m_hWnd, &coord);

		bounds.m_l = coord.x;
		bounds.m_t = coord.y;

		coord.x = rect.right;
		coord.y = rect.bottom;
		::ClientToScreen(m_hWnd, &coord);

		bounds.m_r = coord.x;
		bounds.m_b = coord.y;
	}
	else {
		bounds.m_b = 0;
		bounds.m_l = 0;
		bounds.m_r = 0;
		bounds.m_t = 0;
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

	// XXX 27.04.2011
	// we need to find a way to combine parented windows + resizing if we simply set the
	// state as GHOST_kWindowStateEmbedded we will need to check for them somewhere else.
	// It's also strange that in Windows is the only platform we need to make this separation.
	if (m_parentWindowHwnd != 0) {
		state = GHOST_kWindowStateEmbedded;
		return state;
	}

	if (::IsIconic(m_hWnd)) {
		state = GHOST_kWindowStateMinimized;
	}
	else if (::IsZoomed(m_hWnd)) {
		LONG_PTR result = ::GetWindowLongPtr(m_hWnd, GWL_STYLE);
		if ((result & (WS_POPUP | WS_MAXIMIZE)) != (WS_POPUP | WS_MAXIMIZE))
			state = GHOST_kWindowStateMaximized;
		else
			state = GHOST_kWindowStateFullScreen;
	}
	else {
		state = GHOST_kWindowStateNormal;
	}
	return state;
}


void GHOST_WindowWin32::screenToClient(
        GHOST_TInt32 inX, GHOST_TInt32 inY,
        GHOST_TInt32 &outX, GHOST_TInt32 &outY) const
{
	POINT point = {inX, inY};
	::ScreenToClient(m_hWnd, &point);
	outX = point.x;
	outY = point.y;
}


void GHOST_WindowWin32::clientToScreen(
        GHOST_TInt32 inX, GHOST_TInt32 inY,
        GHOST_TInt32 &outX, GHOST_TInt32 &outY) const
{
	POINT point = {inX, inY};
	::ClientToScreen(m_hWnd, &point);
	outX = point.x;
	outY = point.y;
}


GHOST_TSuccess GHOST_WindowWin32::setState(GHOST_TWindowState state)
{
	GHOST_TWindowState curstate = getState();
	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	::GetWindowPlacement(m_hWnd, &wp);

	if (state == GHOST_kWindowStateNormal)
		state = m_normal_state;

	switch (state) {
		case GHOST_kWindowStateMinimized:
			wp.showCmd = SW_SHOWMINIMIZED;
			break;
		case GHOST_kWindowStateMaximized:
			wp.showCmd = SW_SHOWMAXIMIZED;
			::SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			break;
		case GHOST_kWindowStateFullScreen:
			if (curstate != state && curstate != GHOST_kWindowStateMinimized)
				m_normal_state = curstate;
			wp.showCmd = SW_SHOWMAXIMIZED;
			wp.ptMaxPosition.x = 0;
			wp.ptMaxPosition.y = 0;
			::SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_POPUP | WS_MAXIMIZE);
			break;
		case GHOST_kWindowStateEmbedded:
			::SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_CHILD);
			break;
		case GHOST_kWindowStateNormal:
		default:
			wp.showCmd = SW_SHOWNORMAL;
			::SetWindowLongPtr(m_hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			break;
	}
	/* Clears window cache for SetWindowLongPtr */
	::SetWindowPos(m_hWnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	return ::SetWindowPlacement(m_hWnd, &wp) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}


GHOST_TSuccess GHOST_WindowWin32::setOrder(GHOST_TWindowOrder order)
{
	HWND hWndInsertAfter, hWndToRaise;

	if (order == GHOST_kWindowOrderBottom) {
		hWndInsertAfter = HWND_BOTTOM;
		hWndToRaise = ::GetWindow(m_hWnd, GW_HWNDNEXT); /* the window to raise */
	}
	else {
		hWndInsertAfter = HWND_TOP;
		hWndToRaise = NULL;
	}

	if (::SetWindowPos(m_hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE) {
		return GHOST_kFailure;
	}

	if (hWndToRaise && ::SetWindowPos(hWndToRaise, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE) {
		return GHOST_kFailure;
	}
	return GHOST_kSuccess;
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


GHOST_Context *GHOST_WindowWin32::newDrawingContext(GHOST_TDrawingContextType type)
{
	if (type == GHOST_kDrawingContextTypeOpenGL) {
#if !defined(WITH_GL_EGL)

#if defined(WITH_GL_PROFILE_CORE)
		GHOST_Context *context = new GHOST_ContextWGL(
		        m_wantStereoVisual,
		        m_wantAlphaBackground,
		        m_wantNumOfAASamples,
		        m_hWnd,
		        m_hDC,
		        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		        3, 2,
		        GHOST_OPENGL_WGL_CONTEXT_FLAGS,
		        GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);
#elif defined(WITH_GL_PROFILE_ES20)
		GHOST_Context *context = new GHOST_ContextWGL(
		        m_wantStereoVisual,
		        m_wantAlphaBackground,
		        m_wantNumOfAASamples,
		        m_hWnd,
		        m_hDC,
		        WGL_CONTEXT_ES2_PROFILE_BIT_EXT,
		        2, 0,
		        GHOST_OPENGL_WGL_CONTEXT_FLAGS,
		        GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);
#elif defined(WITH_GL_PROFILE_COMPAT)
		GHOST_Context *context = new GHOST_ContextWGL(
		        m_wantStereoVisual,
		        m_wantAlphaBackground,
		        m_wantNumOfAASamples,
		        m_hWnd,
		        m_hDC,
#if 1
		        0, // profile bit
		        2, 1, // GL version requested
#else
		        // switch to this for Blender 2.8 development
		        WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		        3, 2,
#endif
		        (m_debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
		        GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);
#else
#  error
#endif

#else

#if defined(WITH_GL_PROFILE_CORE)
		GHOST_Context *context = new GHOST_ContextEGL(
		        m_wantStereoVisual,
		        m_wantNumOfAASamples,
		        m_hWnd,
		        m_hDC,
		        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
		        3, 2,
		        GHOST_OPENGL_EGL_CONTEXT_FLAGS,
		        GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
		        EGL_OPENGL_API);
#elif defined(WITH_GL_PROFILE_ES20)
		GHOST_Context *context = new GHOST_ContextEGL(
		        m_wantStereoVisual,
		        m_wantNumOfAASamples,
		        m_hWnd,
		        m_hDC,
		        0, // profile bit
		        2, 0,
		        GHOST_OPENGL_EGL_CONTEXT_FLAGS,
		        GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
		        EGL_OPENGL_ES_API);
#elif defined(WITH_GL_PROFILE_COMPAT)
		GHOST_Context *context = new GHOST_ContextEGL(
		        m_wantStereoVisual,
		        m_wantNumOfAASamples,
		        m_hWnd,
		        m_hDC,
#if 1
		        0, // profile bit
		        2, 1, // GL version requested
#else
		        // switch to this for Blender 2.8 development
		        EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
		        3, 2,
#endif
		        GHOST_OPENGL_EGL_CONTEXT_FLAGS,
		        GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
		        EGL_OPENGL_API);
#else
#  error
#endif

#endif
		if (context->initializeDrawingContext())
			return context;
		else
			delete context;
	}

	return NULL;
}

void GHOST_WindowWin32::lostMouseCapture()
{
	if (m_hasMouseCaptured) {
		m_hasGrabMouse = false;
		m_nPressedButtons = 0;
		m_hasMouseCaptured = false;
	}
}

void GHOST_WindowWin32::registerMouseClickEvent(int press)
{

	switch (press) {
		case 0: m_nPressedButtons++;    break;
		case 1: if (m_nPressedButtons) m_nPressedButtons--; break;
		case 2: m_hasGrabMouse = true;    break;
		case 3: m_hasGrabMouse = false;   break;
	}

	if (!m_nPressedButtons && !m_hasGrabMouse && m_hasMouseCaptured) {
		::ReleaseCapture();
		m_hasMouseCaptured = false;
	}
	else if ((m_nPressedButtons || m_hasGrabMouse) && !m_hasMouseCaptured) {
		::SetCapture(m_hWnd);
		m_hasMouseCaptured = true;

	}
}


void GHOST_WindowWin32::loadCursor(bool visible, GHOST_TStandardCursor cursor) const
{
	if (!visible) {
		while (::ShowCursor(FALSE) >= 0) ;
	}
	else {
		while (::ShowCursor(TRUE) < 0) ;
	}

	if (cursor == GHOST_kStandardCursorCustom && m_customCursor) {
		::SetCursor(m_customCursor);
	}
	else {
		// Convert GHOST cursor to Windows OEM cursor
		bool success = true;
		LPCSTR id;
		switch (cursor) {
			case GHOST_kStandardCursorDefault:              id = IDC_ARROW;     break;
			case GHOST_kStandardCursorRightArrow:           id = IDC_ARROW;     break;
			case GHOST_kStandardCursorLeftArrow:            id = IDC_ARROW;     break;
			case GHOST_kStandardCursorInfo:                 id = IDC_SIZEALL;   break;  // Four-pointed arrow pointing north, south, east, and west
			case GHOST_kStandardCursorDestroy:              id = IDC_NO;        break;  // Slashed circle
			case GHOST_kStandardCursorHelp:                 id = IDC_HELP;      break;  // Arrow and question mark
			case GHOST_kStandardCursorCycle:                id = IDC_NO;        break;  // Slashed circle
			case GHOST_kStandardCursorSpray:                id = IDC_SIZEALL;   break;  // Four-pointed arrow pointing north, south, east, and west
			case GHOST_kStandardCursorWait:                 id = IDC_WAIT;      break;  // Hourglass
			case GHOST_kStandardCursorText:                 id = IDC_IBEAM;     break;  // I-beam
			case GHOST_kStandardCursorCrosshair:            id = IDC_CROSS;     break;  // Crosshair
			case GHOST_kStandardCursorUpDown:               id = IDC_SIZENS;    break;  // Double-pointed arrow pointing north and south
			case GHOST_kStandardCursorLeftRight:            id = IDC_SIZEWE;    break;  // Double-pointed arrow pointing west and east
			case GHOST_kStandardCursorTopSide:              id = IDC_UPARROW;   break;  // Vertical arrow
			case GHOST_kStandardCursorBottomSide:           id = IDC_SIZENS;    break;
			case GHOST_kStandardCursorLeftSide:             id = IDC_SIZEWE;    break;
			case GHOST_kStandardCursorTopLeftCorner:        id = IDC_SIZENWSE;  break;
			case GHOST_kStandardCursorTopRightCorner:       id = IDC_SIZENESW;  break;
			case GHOST_kStandardCursorBottomRightCorner:    id = IDC_SIZENWSE;  break;
			case GHOST_kStandardCursorBottomLeftCorner:     id = IDC_SIZENESW;  break;
			case GHOST_kStandardCursorPencil:               id = IDC_ARROW;     break;
			case GHOST_kStandardCursorCopy:                 id = IDC_ARROW;     break;
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
	if (mode != GHOST_kGrabDisable) {
		if (mode != GHOST_kGrabNormal) {
			m_system->getCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
			setCursorGrabAccum(0, 0);

			if (mode == GHOST_kGrabHide)
				setWindowCursorVisibility(false);
		}
		registerMouseClickEvent(2);
	}
	else {
		if (m_cursorGrab == GHOST_kGrabHide) {
			m_system->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
			setWindowCursorVisibility(true);
		}
		if (m_cursorGrab != GHOST_kGrabNormal) {
			/* use to generate a mouse move event, otherwise the last event
			 * blender gets can be outside the screen causing menus not to show
			 * properly unless the user moves the mouse */
			GHOST_TInt32 pos[2];
			m_system->getCursorPosition(pos[0], pos[1]);
			m_system->setCursorPosition(pos[0], pos[1]);
		}

		/* Almost works without but important otherwise the mouse GHOST location can be incorrect on exit */
		setCursorGrabAccum(0, 0);
		m_cursorGrabBounds.m_l = m_cursorGrabBounds.m_r = -1; /* disable */
		registerMouseClickEvent(3);
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

void GHOST_WindowWin32::processWin32TabletActivateEvent(WORD state)
{
	if (!m_tablet) {
		return;
	}

	GHOST_WIN32_WTEnable fpWTEnable = (GHOST_WIN32_WTEnable) ::GetProcAddress(m_wintab, "WTEnable");
	GHOST_WIN32_WTOverlap fpWTOverlap = (GHOST_WIN32_WTOverlap) ::GetProcAddress(m_wintab, "WTOverlap");

	if (fpWTEnable) {
		fpWTEnable(m_tablet, state);
		if (fpWTOverlap && state) {
			fpWTOverlap(m_tablet, TRUE);
		}
	}
}

void GHOST_WindowWin32::processWin32TabletInitEvent()
{
	if (m_wintab && m_tabletData) {
		GHOST_WIN32_WTInfo fpWTInfo = (GHOST_WIN32_WTInfo) ::GetProcAddress(m_wintab, "WTInfoA");

		// let's see if we can initialize tablet here
		/* check if WinTab available. */
		if (fpWTInfo) {
			AXIS Pressure, Orientation[3]; /* The maximum tablet size */

			BOOL pressureSupport = fpWTInfo(WTI_DEVICES, DVC_NPRESSURE, &Pressure);
			if (pressureSupport)
				m_maxPressure = Pressure.axMax;
			else
				m_maxPressure = 0;

			BOOL tiltSupport = fpWTInfo(WTI_DEVICES, DVC_ORIENTATION, &Orientation);
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
		GHOST_WIN32_WTPacket fpWTPacket = (GHOST_WIN32_WTPacket) ::GetProcAddress(m_wintab, "WTPacket");
		if (fpWTPacket) {
			if (fpWTPacket((HCTX)lParam, wParam, &pkt)) {
				if (m_tabletData) {
					switch (pkt.pkCursor % 3) { /* % 3 for multiple devices ("DualTrack") */
						case 0:
							m_tabletData->Active = GHOST_kTabletModeNone; /* puck - not yet supported */
							break;
						case 1:
							m_tabletData->Active = GHOST_kTabletModeStylus; /* stylus */
							break;
						case 2:
							m_tabletData->Active = GHOST_kTabletModeEraser; /* eraser */
							break;
					}
					if (m_maxPressure > 0) {
						m_tabletData->Pressure = (float)pkt.pkNormalPressure / (float)m_maxPressure;
					}
					else {
						m_tabletData->Pressure = 1.0f;
					}

					if ((m_maxAzimuth > 0) && (m_maxAltitude > 0)) {
						ORIENTATION ort = pkt.pkOrientation;
						float vecLen;
						float altRad, azmRad;   /* in radians */

						/*
						 * from the wintab spec:
						 * orAzimuth	Specifies the clockwise rotation of the
						 * cursor about the z axis through a full circular range.
						 *
						 * orAltitude	Specifies the angle with the x-y plane
						 * through a signed, semicircular range.  Positive values
						 * specify an angle upward toward the positive z axis;
						 * negative values specify an angle downward toward the negative z axis.
						 *
						 * wintab.h defines .orAltitude as a UINT but documents .orAltitude
						 * as positive for upward angles and negative for downward angles.
						 * WACOM uses negative altitude values to show that the pen is inverted;
						 * therefore we cast .orAltitude as an (int) and then use the absolute value.
						 */

						/* convert raw fixed point data to radians */
						altRad = (float)((fabs((float)ort.orAltitude) / (float)m_maxAltitude) * M_PI / 2.0);
						azmRad = (float)(((float)ort.orAzimuth / (float)m_maxAzimuth) * M_PI * 2.0);

						/* find length of the stylus' projected vector on the XY plane */
						vecLen = cos(altRad);

						/* from there calculate X and Y components based on azimuth */
						m_tabletData->Xtilt = sin(azmRad) * vecLen;
						m_tabletData->Ytilt = (float)(sin(M_PI / 2.0 - azmRad) * vecLen);

					}
					else {
						m_tabletData->Xtilt = 0.0f;
						m_tabletData->Ytilt = 0.0f;
					}
				}
			}
		}
	}
}

void GHOST_WindowWin32::bringTabletContextToFront()
{
	if (m_wintab) {
		GHOST_WIN32_WTOverlap fpWTOverlap = (GHOST_WIN32_WTOverlap) ::GetProcAddress(m_wintab, "WTOverlap");
		if (fpWTOverlap) {
			fpWTOverlap(m_tablet, TRUE);
		}
	}
}

GHOST_TUns16 GHOST_WindowWin32::getDPIHint()
{
	if (!m_user32) {
		m_user32 = ::LoadLibrary("user32.dll");
	}

	if (m_user32) {
		GHOST_WIN32_GetDpiForWindow fpGetDpiForWindow = (GHOST_WIN32_GetDpiForWindow) ::GetProcAddress(m_user32, "GetDpiForWindow");

		if (fpGetDpiForWindow) {
			return fpGetDpiForWindow(this->m_hWnd);
		}
	}

	return USER_DEFAULT_SCREEN_DPI;
}

/** Reverse the bits in a GHOST_TUns8 */
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
	ch = ((ch >> 1) & 0x55) | ((ch << 1) & 0xAA);
	ch = ((ch >> 2) & 0x33) | ((ch << 2) & 0xCC);
	ch = ((ch >> 4) & 0x0F) | ((ch << 4) & 0xF0);
	return ch;
}

#if 0  /* UNUSED */
/** Reverse the bits in a GHOST_TUns16 */
static GHOST_TUns16 uns16ReverseBits(GHOST_TUns16 shrt)
{
	shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
	shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
	shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
	shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
	return shrt;
}
#endif
GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(
        GHOST_TUns8 bitmap[16][2],
        GHOST_TUns8 mask[16][2],
        int hotX, int hotY)
{
	return setWindowCustomCursorShape((GHOST_TUns8 *)bitmap, (GHOST_TUns8 *)mask,
	                                  16, 16, hotX, hotY, 0, 1);
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(
        GHOST_TUns8 *bitmap,
        GHOST_TUns8 *mask, int sizeX, int sizeY, int hotX, int hotY,
        int fg_color, int bg_color)
{
	GHOST_TUns32 andData[32];
	GHOST_TUns32 xorData[32];
	GHOST_TUns32 fullBitRow, fullMaskRow;
	int x, y, cols;

	cols = sizeX / 8; /* Num of whole bytes per row (width of bm/mask) */
	if (sizeX % 8) cols++;

	if (m_customCursor) {
		DestroyCursor(m_customCursor);
		m_customCursor = NULL;
	}

	memset(&andData, 0xFF, sizeof(andData));
	memset(&xorData, 0, sizeof(xorData));

	for (y = 0; y < sizeY; y++) {
		fullBitRow = 0;
		fullMaskRow = 0;
		for (x = cols - 1; x >= 0; x--) {
			fullBitRow <<= 8;
			fullMaskRow <<= 8;
			fullBitRow  |= uns8ReverseBits(bitmap[cols * y + x]);
			fullMaskRow |= uns8ReverseBits(mask[cols * y + x]);
		}
		xorData[y] = fullBitRow & fullMaskRow;
		andData[y] = ~fullMaskRow;
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


GHOST_TSuccess GHOST_WindowWin32::setProgressBar(float progress)
{
	/*SetProgressValue sets state to TBPF_NORMAL automaticly*/
	if (m_Bar && S_OK == m_Bar->SetProgressValue(m_hWnd, 10000 * progress, 10000))
		return GHOST_kSuccess;

	return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::endProgressBar()
{
	if (m_Bar && S_OK == m_Bar->SetProgressState(m_hWnd, TBPF_NOPROGRESS))
		return GHOST_kSuccess;

	return GHOST_kFailure;
}


#ifdef WITH_INPUT_IME
void GHOST_WindowWin32::beginIME(GHOST_TInt32 x, GHOST_TInt32 y, GHOST_TInt32 w, GHOST_TInt32 h, int completed)
{
	m_imeInput.BeginIME(m_hWnd, GHOST_Rect(x, y - h, x, y), (bool)completed);
}


void GHOST_WindowWin32::endIME()
{
	m_imeInput.EndIME(m_hWnd);
}
#endif /* WITH_INPUT_IME */
