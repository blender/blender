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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_WindowWin32.h
 *  \ingroup GHOST
 * Declaration of GHOST_WindowWin32 class.
 */

#ifndef __GHOST_WINDOWWIN32_H__
#define __GHOST_WINDOWWIN32_H__

#ifndef WIN32
#error WIN32 only!
#endif // WIN32

#include "GHOST_Window.h"
#include "GHOST_TaskbarWin32.h"
#ifdef WITH_INPUT_IME
#  include "GHOST_ImeWin32.h"
#endif

#include <wintab.h>
#define PACKETDATA  (PK_BUTTONS | PK_NORMAL_PRESSURE | PK_ORIENTATION | PK_CURSOR)
#define PACKETMODE  PK_BUTTONS
#include <pktdef.h>

class GHOST_SystemWin32;
class GHOST_DropTargetWin32;

// typedefs for WinTab functions to allow dynamic loading
typedef UINT (API * GHOST_WIN32_WTInfo)(UINT, UINT, LPVOID);
typedef HCTX (API * GHOST_WIN32_WTOpen)(HWND, LPLOGCONTEXTA, BOOL);
typedef BOOL (API * GHOST_WIN32_WTClose)(HCTX);
typedef BOOL (API * GHOST_WIN32_WTPacket)(HCTX, UINT, LPVOID);
typedef BOOL (API * GHOST_WIN32_WTEnable)(HCTX, BOOL);
typedef BOOL (API * GHOST_WIN32_WTOverlap)(HCTX, BOOL);

// typedefs for user32 functions to allow dynamic loading of Windows 10 DPI scaling functions
typedef UINT(API * GHOST_WIN32_GetDpiForWindow)(HWND);
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif // USER_DEFAULT_SCREEN_DPI

/**
 * GHOST window on M$ Windows OSs.
 * \author	Maarten Gribnau
 * \date	May 10, 2001
 */
class GHOST_WindowWin32 : public GHOST_Window {
public:
	/**
	 * Constructor.
	 * Creates a new window and opens it.
	 * To check if the window was created properly, use the getValid() method.
	 * \param title		The text shown in the title bar of the window.
	 * \param left		The coordinate of the left edge of the window.
	 * \param top		The coordinate of the top edge of the window.
	 * \param width		The width the window.
	 * \param height	The height the window.
	 * \param state		The state the window is initially opened with.
	 * \param type		The type of drawing context installed in this window.
	 * \param wantStereoVisual   Stereo visual for quad buffered stereo.
	 * \param wantNumOfAASamples Number of samples used for AA (zero if no AA)
	 * \param parentWindowHwnd
	 */
	GHOST_WindowWin32(GHOST_SystemWin32 *system,
	    const STR_String& title,
	    GHOST_TInt32 left,
	    GHOST_TInt32 top,
	    GHOST_TUns32 width,
	    GHOST_TUns32 height,
	    GHOST_TWindowState state,
	    GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
	    bool wantStereoVisual = false,
	    bool alphaBackground = false,
	    GHOST_TUns16 wantNumOfAASamples = 0,
	    GHOST_TEmbedderWindowID parentWindowHwnd = 0,
	    bool is_debug = false);

	/**
	 * Destructor.
	 * Closes the window and disposes resources allocated.
	 */
	~GHOST_WindowWin32();

	/**
	 * Returns indication as to whether the window is valid.
	 * \return The validity of the window.
	 */
	bool getValid() const;

	/**
	 * Access to the handle of the window.
	 * \return The handle of the window.
	 */
	HWND getHWND() const;

	/**
	 * Sets the title displayed in the title bar.
	 * \param title	The title to display in the title bar.
	 */
	void setTitle(const STR_String& title);

	/**
	 * Returns the title displayed in the title bar.
	 * \param title	The title displayed in the title bar.
	 */
	void getTitle(STR_String& title) const;

	/**
	 * Returns the window rectangle dimensions.
	 * The dimensions are given in screen coordinates that are relative to the upper-left corner of the screen.
	 * \param bounds The bounding rectangle of the window.
	 */
	void getWindowBounds(GHOST_Rect& bounds) const;

	/**
	 * Returns the client rectangle dimensions.
	 * The left and top members of the rectangle are always zero.
	 * \param bounds The bounding rectangle of the client area of the window.
	 */
	void getClientBounds(GHOST_Rect& bounds) const;

	/**
	 * Resizes client rectangle width.
	 * \param width The new width of the client area of the window.
	 */
	GHOST_TSuccess setClientWidth(GHOST_TUns32 width);

	/**
	 * Resizes client rectangle height.
	 * \param height The new height of the client area of the window.
	 */
	GHOST_TSuccess setClientHeight(GHOST_TUns32 height);

	/**
	 * Resizes client rectangle.
	 * \param width		The new width of the client area of the window.
	 * \param height	The new height of the client area of the window.
	 */
	GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height);

	/**
	 * Returns the state of the window (normal, minimized, maximized).
	 * \return The state of the window.
	 */
	GHOST_TWindowState getState() const;

	/**
	 * Converts a point in screen coordinates to client rectangle coordinates
	 * \param inX	The x-coordinate on the screen.
	 * \param inY	The y-coordinate on the screen.
	 * \param outX	The x-coordinate in the client rectangle.
	 * \param outY	The y-coordinate in the client rectangle.
	 */
	void screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	/**
	 * Converts a point in screen coordinates to client rectangle coordinates
	 * \param inX	The x-coordinate in the client rectangle.
	 * \param inY	The y-coordinate in the client rectangle.
	 * \param outX	The x-coordinate on the screen.
	 * \param outY	The y-coordinate on the screen.
	 */
	void clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const;

	/**
	 * Sets the state of the window (normal, minimized, maximized).
	 * \param state The state of the window.
	 * \return Indication of success.
	 */
	GHOST_TSuccess setState(GHOST_TWindowState state);

	/**
	 * Sets the order of the window (bottom, top).
	 * \param order The order of the window.
	 * \return Indication of success.
	 */
	GHOST_TSuccess setOrder(GHOST_TWindowOrder order);

	/**
	 * Invalidates the contents of this window.
	 */
	GHOST_TSuccess invalidate();

	/**
	 * Sets the progress bar value displayed in the window/application icon
	 * \param progress The progress %
	 */
	GHOST_TSuccess setProgressBar(float progress);

	/**
	 * Hides the progress bar in the icon
	 */
	GHOST_TSuccess endProgressBar();


	/**
	 * Register a mouse click event (should be called
	 * for any real button press, controls mouse
	 * capturing).
	 *
	 * \param press
	 *		0 - mouse pressed
	 *		1 - mouse released
	 *		2 - operator grab
	 *		3 - operator ungrab
	 */
	void registerMouseClickEvent(int press);

	/**
	 * Inform the window that it has lost mouse capture,
	 * called in response to native window system messages.
	 */
	void lostMouseCapture();

	/**
	 * Loads the windows equivalent of a standard GHOST cursor.
	 * \param visible		Flag for cursor visibility.
	 * \param cursorShape	The cursor shape.
	 */
	void loadCursor(bool visible, GHOST_TStandardCursor cursorShape) const;

	const GHOST_TabletData *GetTabletData()
	{
		return m_tabletData;
	}

	void processWin32TabletActivateEvent(WORD state);
	void processWin32TabletInitEvent();
	void processWin32TabletEvent(WPARAM wParam, LPARAM lParam);
	void bringTabletContextToFront();

	GHOST_TSuccess beginFullScreen() const {return GHOST_kFailure;}

	GHOST_TSuccess endFullScreen() const {return GHOST_kFailure;}

	GHOST_TUns16 getDPIHint() override;

	/** if the window currently resizing */
	bool m_inLiveResize;

#ifdef WITH_INPUT_IME
	GHOST_ImeWin32 *getImeInput() {return &m_imeInput;}

	void beginIME(
	        GHOST_TInt32 x, GHOST_TInt32 y,
	        GHOST_TInt32 w, GHOST_TInt32 h,
	        int completed);

	void endIME();
#endif /* WITH_INPUT_IME */

private:

	/**
	 * \param type	The type of rendering context create.
	 * \return Indication of success.
	 */
	GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type);

	/**
	 * Sets the cursor visibility on the window using
	 * native window system calls.
	 */
	GHOST_TSuccess setWindowCursorVisibility(bool visible);

	/**
	 * Sets the cursor grab on the window using native window system calls.
	 * Using registerMouseClickEvent.
	 * \param mode	GHOST_TGrabCursorMode.
	 */
	GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode);

	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape);

	/**
	 * Sets the cursor shape on the window using
	 * native window system calls.
	 */
	GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2],
	                                                  GHOST_TUns8 mask[16][2],
	                                                  int hotX, int hotY);

	GHOST_TSuccess setWindowCustomCursorShape(
	    GHOST_TUns8 *bitmap,
	    GHOST_TUns8 *mask,
	    int sizex,
	    int sizey,
	    int hotX,
	    int hotY,
	    int fg_color,
	    int bg_color
	    );

	/** Pointer to system */
	GHOST_SystemWin32 *m_system;
	/** Pointer to COM IDropTarget implementor */
	GHOST_DropTargetWin32 *m_dropTarget;
	/** Window handle. */
	HWND m_hWnd;
	/** Device context handle. */
	HDC m_hDC;

	/** Flag for if window has captured the mouse */
	bool m_hasMouseCaptured;
	/** Flag if an operator grabs the mouse with WM_cursor_grab_enable/ungrab()
	 * Multiple grabs must be released with a single ungrab */
	bool m_hasGrabMouse;
	/** Count of number of pressed buttons */
	int m_nPressedButtons;
	/** HCURSOR structure of the custom cursor */
	HCURSOR m_customCursor;
	/** request GL context aith alpha channel */
	bool m_wantAlphaBackground;

	/** ITaskbarList3 structure for progress bar*/
	ITaskbarList3 *m_Bar;

	static const wchar_t *s_windowClassName;
	static const int s_maxTitleLength;

	/** WinTab dll handle */
	HMODULE m_wintab;

	/** Tablet data for GHOST */
	GHOST_TabletData *m_tabletData;

	/** Stores the Tablet context if detected Tablet features using WinTab.dll */
	HCTX m_tablet;
	LONG m_maxPressure;
	LONG m_maxAzimuth, m_maxAltitude;

	GHOST_TWindowState m_normal_state;

	/** user32 dll handle*/
	HMODULE m_user32;

	/** Hwnd to parent window */
	GHOST_TEmbedderWindowID m_parentWindowHwnd;

#ifdef WITH_INPUT_IME
	/** Handle input method editors event */
	GHOST_ImeWin32 m_imeInput;
#endif
	bool m_debug_context;
};

#endif // __GHOST_WINDOWWIN32_H__
