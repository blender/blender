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
 * Part of this code has been taken from Qt, under LGPL license
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_SystemX11.cpp
 *  \ingroup GHOST
 */

#include "GHOST_SystemX11.h"
#include "GHOST_WindowX11.h"
#include "GHOST_WindowManager.h"
#include "GHOST_TimerManager.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventWheel.h"
#include "GHOST_DisplayManagerX11.h"
#include "GHOST_EventDragnDrop.h"
#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerX11.h"
#endif

#ifdef WITH_XDND
#  include "GHOST_DropTargetX11.h"
#endif

#include "GHOST_Debug.h"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h> /* allow detectable autorepeate */

#ifdef WITH_XF86KEYSYM
#include <X11/XF86keysym.h>
#endif

/* For timing */
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <stdio.h> /* for fprintf only */
#include <cstdlib> /* for exit */

static GHOST_TKey
convertXKey(KeySym key);

/* these are for copy and select copy */
static char *txt_cut_buffer = NULL;
static char *txt_select_buffer = NULL;

using namespace std;

GHOST_SystemX11::
GHOST_SystemX11(
    ) :
	GHOST_System(),
	m_start_time(0)
{
	m_display = XOpenDisplay(NULL);
	
	if (!m_display) {
		std::cerr << "Unable to open a display" << std::endl;
		abort(); /* was return before, but this would just mean it will crash later */
	}

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
	/* note -- don't open connection to XIM server here, because the locale
	 * has to be set before opening the connection but setlocale() has not
	 * been called yet.  the connection will be opened after entering
	 * the event loop. */
	m_xim = NULL;
#endif

	m_delete_window_atom 
	    = XInternAtom(m_display, "WM_DELETE_WINDOW", True);

	m_wm_protocols = XInternAtom(m_display, "WM_PROTOCOLS", False);
	m_wm_take_focus = XInternAtom(m_display, "WM_TAKE_FOCUS", False);
	m_wm_state = XInternAtom(m_display, "WM_STATE", False);
	m_wm_change_state = XInternAtom(m_display, "WM_CHANGE_STATE", False);
	m_net_state = XInternAtom(m_display, "_NET_WM_STATE", False);
	m_net_max_horz = XInternAtom(m_display,
	                             "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	m_net_max_vert = XInternAtom(m_display,
	                             "_NET_WM_STATE_MAXIMIZED_VERT", False);
	m_net_fullscreen = XInternAtom(m_display,
	                               "_NET_WM_STATE_FULLSCREEN", False);
	m_motif = XInternAtom(m_display, "_MOTIF_WM_HINTS", False);
	m_targets = XInternAtom(m_display, "TARGETS", False);
	m_string = XInternAtom(m_display, "STRING", False);
	m_compound_text = XInternAtom(m_display, "COMPOUND_TEXT", False);
	m_text = XInternAtom(m_display, "TEXT", False);
	m_clipboard = XInternAtom(m_display, "CLIPBOARD", False);
	m_primary = XInternAtom(m_display, "PRIMARY", False);
	m_xclip_out = XInternAtom(m_display, "XCLIP_OUT", False);
	m_incr = XInternAtom(m_display, "INCR", False);
	m_utf8_string = XInternAtom(m_display, "UTF8_STRING", False);
	m_last_warp = 0;


	/* compute the initial time */
	timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		GHOST_ASSERT(false, "Could not instantiate timer!");
	}
	
	/* Taking care not to overflow the tv.tv_sec*1000 */
	m_start_time = GHOST_TUns64(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
	
	
	/* use detectable autorepeate, mac and windows also do this */
	int use_xkb;
	int xkb_opcode, xkb_event, xkb_error;
	int xkb_major = XkbMajorVersion, xkb_minor = XkbMinorVersion;
	
	use_xkb = XkbQueryExtension(m_display, &xkb_opcode, &xkb_event, &xkb_error, &xkb_major, &xkb_minor);
	if (use_xkb) {
		XkbSetDetectableAutoRepeat(m_display, true, NULL);
	}
	
}

GHOST_SystemX11::
~GHOST_SystemX11()
{
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
	XCloseIM(m_xim);
#endif

	XCloseDisplay(m_display);
}


GHOST_TSuccess
GHOST_SystemX11::
init()
{
	GHOST_TSuccess success = GHOST_System::init();

	if (success) {
#ifdef WITH_INPUT_NDOF
		m_ndofManager = new GHOST_NDOFManagerX11(*this);
#endif
		m_displayManager = new GHOST_DisplayManagerX11(this);

		if (m_displayManager) {
			return GHOST_kSuccess;
		}
	}

	return GHOST_kFailure;
}

GHOST_TUns64
GHOST_SystemX11::
getMilliSeconds() const
{
	timeval tv;
	if (gettimeofday(&tv, NULL) == -1) {
		GHOST_ASSERT(false, "Could not compute time!");
	}

	/* Taking care not to overflow the tv.tv_sec*1000 */
	return GHOST_TUns64(tv.tv_sec) * 1000 + tv.tv_usec / 1000 - m_start_time;
}
	
GHOST_TUns8
GHOST_SystemX11::
getNumDisplays() const
{
	return GHOST_TUns8(1);
}

/**
 * Returns the dimensions of the main display on this system.
 * @return The dimension of the main display.
 */
void
GHOST_SystemX11::
getMainDisplayDimensions(
		GHOST_TUns32& width,
		GHOST_TUns32& height) const
{
	if (m_display) {
		width  = DisplayWidth(m_display, DefaultScreen(m_display));
		height = DisplayHeight(m_display, DefaultScreen(m_display));
	}
}

/**
 * Create a new window.
 * The new window is added to the list of windows managed.
 * Never explicitly delete the window, use disposeWindow() instead.
 * @param	title	The name of the window (displayed in the title bar of the window if the OS supports it).
 * @param	left	The coordinate of the left edge of the window.
 * @param	top		The coordinate of the top edge of the window.
 * @param	width	The width the window.
 * @param	height	The height the window.
 * @param	state	The state of the window when opened.
 * @param	type	The type of drawing context installed in this window.
 * @param	stereoVisual	Stereo visual for quad buffered stereo.
 * @param	numOfAASamples	Number of samples used for AA (zero if no AA)
 * @param	parentWindow    Parent (embedder) window
 * @return	The new window (or 0 if creation failed).
 */
GHOST_IWindow *
GHOST_SystemX11::
createWindow(
		const STR_String& title,
		GHOST_TInt32 left,
		GHOST_TInt32 top,
		GHOST_TUns32 width,
		GHOST_TUns32 height,
		GHOST_TWindowState state,
		GHOST_TDrawingContextType type,
		bool stereoVisual,
		const GHOST_TUns16 numOfAASamples,
		const GHOST_TEmbedderWindowID parentWindow)
{
	GHOST_WindowX11 *window = 0;
	
	if (!m_display) return 0;
	

	

	window = new GHOST_WindowX11(this, m_display, title,
	                             left, top, width, height,
	                             state, parentWindow, type, stereoVisual);

	if (window) {
		/* Both are now handle in GHOST_WindowX11.cpp
		 * Focus and Delete atoms. */

		if (window->getValid()) {
			/* Store the pointer to the window */
			m_windowManager->addWindow(window);
			m_windowManager->setActiveWindow(window);
			pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window) );
		}
		else {
			delete window;
			window = 0;
		}
	}
	return window;
}

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
static void destroyIMCallback(XIM xim, XPointer ptr, XPointer data)
{
	GHOST_PRINT("XIM server died\n");

	if (ptr)
		*(XIM *)ptr = NULL;
}

bool GHOST_SystemX11::openX11_IM()
{
	if (!m_display)
		return false;

	/* set locale modifiers such as "@im=ibus" specified by XMODIFIERS */
	XSetLocaleModifiers("");

	m_xim = XOpenIM(m_display, NULL, (char *)GHOST_X11_RES_NAME, (char *)GHOST_X11_RES_CLASS);
	if (!m_xim)
		return false;

	XIMCallback destroy;
	destroy.callback = (XIMProc)destroyIMCallback;
	destroy.client_data = (XPointer)&m_xim;
	XSetIMValues(m_xim, XNDestroyCallback, &destroy, NULL);
	return true;
}
#endif

GHOST_WindowX11 *
GHOST_SystemX11::
findGhostWindow(
		Window xwind) const
{
	
	if (xwind == 0) return NULL;

	/* It is not entirely safe to do this as the backptr may point
	 * to a window that has recently been removed.
	 * We should always check the window manager's list of windows
	 * and only process events on these windows. */

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();

	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();
	
	for (; win_it != win_end; ++win_it) {
		GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
		if (window->getXWindow() == xwind) {
			return window;
		}
	}
	return NULL;
	
}

static void SleepTillEvent(Display *display, GHOST_TInt64 maxSleep)
{
	int fd = ConnectionNumber(display);
	fd_set fds;
	
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (maxSleep == -1) {
		select(fd + 1, &fds, NULL, NULL, NULL);
	}
	else {
		timeval tv;

		tv.tv_sec = maxSleep / 1000;
		tv.tv_usec = (maxSleep - tv.tv_sec * 1000) * 1000;
	
		select(fd + 1, &fds, NULL, NULL, &tv);
	}
}

/* This function borrowed from Qt's X11 support
 * qclipboard_x11.cpp
 *  */
struct init_timestamp_data {
	Time timestamp;
};

static Bool init_timestamp_scanner(Display *, XEvent *event, XPointer arg)
{
	init_timestamp_data *data =
	    reinterpret_cast<init_timestamp_data *>(arg);
	switch (event->type)
	{
		case ButtonPress:
		case ButtonRelease:
			data->timestamp = event->xbutton.time;
			break;
		case MotionNotify:
			data->timestamp = event->xmotion.time;
			break;
		case KeyPress:
		case KeyRelease:
			data->timestamp = event->xkey.time;
			break;
		case PropertyNotify:
			data->timestamp = event->xproperty.time;
			break;
		case EnterNotify:
		case LeaveNotify:
			data->timestamp = event->xcrossing.time;
			break;
		case SelectionClear:
			data->timestamp = event->xselectionclear.time;
			break;
		default:
			break;
	}

	return false;
}

Time
GHOST_SystemX11::
lastEventTime(Time default_time) {
	init_timestamp_data data;
	data.timestamp = default_time;
	XEvent ev;
	XCheckIfEvent(m_display, &ev, &init_timestamp_scanner, (XPointer) & data);

	return data.timestamp;
}

bool
GHOST_SystemX11::
processEvents(
		bool waitForEvent)
{
	/* Get all the current events -- translate them into
	 * ghost events and call base class pushEvent() method. */
	
	bool anyProcessed = false;
	
	do {
		GHOST_TimerManager *timerMgr = getTimerManager();
		
		if (waitForEvent && m_dirty_windows.empty() && !XPending(m_display)) {
			GHOST_TUns64 next = timerMgr->nextFireTime();
			
			if (next == GHOST_kFireTimeNever) {
				SleepTillEvent(m_display, -1);
			}
			else {
				GHOST_TInt64 maxSleep = next - getMilliSeconds();

				if (maxSleep >= 0)
					SleepTillEvent(m_display, next - getMilliSeconds());
			}
		}
		
		if (timerMgr->fireTimers(getMilliSeconds())) {
			anyProcessed = true;
		}
		
		while (XPending(m_display)) {
			XEvent xevent;
			XNextEvent(m_display, &xevent);

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
			/* open connection to XIM server and create input context (XIC)
			 * when receiving the first FocusIn or KeyPress event after startup,
			 * or recover XIM and XIC when the XIM server has been restarted */
			if (xevent.type == FocusIn || xevent.type == KeyPress) {
				if (!m_xim && openX11_IM()) {
					GHOST_PRINT("Connected to XIM server\n");
				}

				if (m_xim) {
					GHOST_WindowX11 * window = findGhostWindow(xevent.xany.window);
					if (window && !window->getX11_XIC() && window->createX11_XIC()) {
						GHOST_PRINT("XIM input context created\n");
						if (xevent.type == KeyPress)
							/* we can assume the window has input focus
							 * here, because key events are received only
							 * when the window is focused. */
							XSetICFocus(window->getX11_XIC());
					}
				}
			}

			/* dispatch event to XIM server */
			if ((XFilterEvent(&xevent, (Window)NULL) == True) && (xevent.type != KeyRelease)) {
				/* do nothing now, the event is consumed by XIM.
				 * however, KeyRelease event should be processed
				 * here, otherwise modifiers remain activated.   */
				continue;
			}
#endif

			processEvent(&xevent);
			anyProcessed = true;
		}
		
		if (generateWindowExposeEvents()) {
			anyProcessed = true;
		}

#ifdef WITH_INPUT_NDOF
		if (dynamic_cast<GHOST_NDOFManagerX11 *>(m_ndofManager)->processEvents()) {
			anyProcessed = true;
		}
#endif
		
	} while (waitForEvent && !anyProcessed);
	
	return anyProcessed;
}


#ifdef WITH_X11_XINPUT
/* set currently using tablet mode (stylus or eraser) depending on device ID */
static void setTabletMode(GHOST_WindowX11 *window, XID deviceid)
{
	if (deviceid == window->GetXTablet().StylusID)
		window->GetXTablet().CommonData.Active = GHOST_kTabletModeStylus;
	else if (deviceid == window->GetXTablet().EraserID)
		window->GetXTablet().CommonData.Active = GHOST_kTabletModeEraser;
}
#endif /* WITH_X11_XINPUT */

void
GHOST_SystemX11::processEvent(XEvent *xe)
{
	GHOST_WindowX11 *window = findGhostWindow(xe->xany.window);
	GHOST_Event *g_event = NULL;

	if (!window) {
		return;
	}
	
	switch (xe->type) {
		case Expose:
		{
			XExposeEvent & xee = xe->xexpose;

			if (xee.count == 0) {
				/* Only generate a single expose event
				 * per read of the event queue. */

				g_event = new
				          GHOST_Event(
				    getMilliSeconds(),
				    GHOST_kEventWindowUpdate,
				    window
				    );
			}
			break;
		}

		case MotionNotify:
		{
			XMotionEvent &xme = xe->xmotion;

#ifdef WITH_X11_XINPUT
			bool is_tablet = window->GetXTablet().CommonData.Active != GHOST_kTabletModeNone;
#else
			bool is_tablet = false;
#endif

			if (is_tablet == false && window->getCursorGrabModeIsWarp()) {
				GHOST_TInt32 x_new = xme.x_root;
				GHOST_TInt32 y_new = xme.y_root;
				GHOST_TInt32 x_accum, y_accum;
				GHOST_Rect bounds;

				/* fallback to window bounds */
				if (window->getCursorGrabBounds(bounds) == GHOST_kFailure)
					window->getClientBounds(bounds);

				/* could also clamp to screen bounds
				 * wrap with a window outside the view will fail atm  */
				bounds.wrapPoint(x_new, y_new, 8); /* offset of one incase blender is at screen bounds */
				window->getCursorGrabAccum(x_accum, y_accum);

				if (x_new != xme.x_root || y_new != xme.y_root) {
					if (xme.time > m_last_warp) {
						/* when wrapping we don't need to add an event because the
						 * setCursorPosition call will cause a new event after */
						setCursorPosition(x_new, y_new); /* wrap */
						window->setCursorGrabAccum(x_accum + (xme.x_root - x_new), y_accum + (xme.y_root - y_new));
						m_last_warp = lastEventTime(xme.time);
					}
					else {
						setCursorPosition(x_new, y_new); /* wrap but don't accumulate */
					}
				}
				else {
					g_event = new
					          GHOST_EventCursor(
					    getMilliSeconds(),
					    GHOST_kEventCursorMove,
					    window,
					    xme.x_root + x_accum,
					    xme.y_root + y_accum
					    );
				}
			}
			else {
				g_event = new
				          GHOST_EventCursor(
				    getMilliSeconds(),
				    GHOST_kEventCursorMove,
				    window,
				    xme.x_root,
				    xme.y_root
				    );
			}
			break;
		}

		case KeyPress:
		case KeyRelease:
		{
			XKeyEvent *xke = &(xe->xkey);
			KeySym key_sym = XLookupKeysym(xke, 0);
			char ascii;
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
			/* utf8_array[] is initial buffer used for Xutf8LookupString().
			 * if the length of the utf8 string exceeds this array, allocate
			 * another memory area and call Xutf8LookupString() again.
			 * the last 5 bytes are used to avoid segfault that might happen
			 * at the end of this buffer when the constructor of GHOST_EventKey
			 * reads 6 bytes regardless of the effective data length. */
			char utf8_array[16 * 6 + 5]; /* 16 utf8 characters */
			char *utf8_buf = utf8_array;
			int len = 1; /* at least one null character will be stored */
#else
			char *utf8_buf = NULL;
#endif
			
			GHOST_TKey gkey = convertXKey(key_sym);
			GHOST_TEventType type = (xke->type == KeyPress) ? 
			                        GHOST_kEventKeyDown : GHOST_kEventKeyUp;
			
			if (!XLookupString(xke, &ascii, 1, NULL, NULL)) {
				ascii = '\0';
			}
			
#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
			/* getting unicode on key-up events gives XLookupNone status */
			XIC xic = window->getX11_XIC();
			if (xic && xke->type == KeyPress) {
				Status status;

				/* use utf8 because its not locale depentant, from xorg docs */
				if (!(len = Xutf8LookupString(xic, xke, utf8_buf, sizeof(utf8_array) - 5, &key_sym, &status))) {
					utf8_buf[0] = '\0';
				}

				if (status == XBufferOverflow) {
					utf8_buf = (char *) malloc(len + 5);
					len = Xutf8LookupString(xic, xke, utf8_buf, len, &key_sym, &status);
				}

				if ((status == XLookupChars || status == XLookupBoth)) {
					if ((unsigned char)utf8_buf[0] >= 32) { /* not an ascii control character */
						/* do nothing for now, this is valid utf8 */
					}
					else {
						utf8_buf[0] = '\0';
					}
				}
				else if (status == XLookupKeySym) {
					/* this key doesn't have a text representation, it is a command
					 * key of some sort */;
				}
				else {
					printf("Bad keycode lookup. Keysym 0x%x Status: %s\n",
					       (unsigned int) key_sym,
					       (status == XLookupNone ? "XLookupNone" :
					        status == XLookupKeySym ? "XLookupKeySym" :
					        "Unknown status"));

					printf("'%.*s' %p %p\n", len, utf8_buf, xic, m_xim);
				}
			}
			else {
				utf8_buf[0] = '\0';
			}
#endif

			g_event = new
			          GHOST_EventKey(
			    getMilliSeconds(),
			    type,
			    window,
			    gkey,
			    ascii,
			    utf8_buf
			    );

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
			/* when using IM for some languages such as Japanese,
			 * one event inserts multiple utf8 characters */
			if (xic && xke->type == KeyPress) {
				unsigned char c;
				int i = 0;
				while (1) {
					/* search character boundary */
					if ((unsigned char)utf8_buf[i++] > 0x7f) {
						for (; i < len; ++i) {
							c = utf8_buf[i];
							if (c < 0x80 || c > 0xbf) break;
						}
					}

					if (i >= len) break;

					/* enqueue previous character */
					pushEvent(g_event);

					g_event = new
					          GHOST_EventKey(
					    getMilliSeconds(),
					    type,
					    window,
					    gkey,
					    '\0',
					    &utf8_buf[i]
					    );
				}
			}

			if (utf8_buf != utf8_array)
				free(utf8_buf);
#endif
			
			break;
		}

		case ButtonPress:
		case ButtonRelease:
		{
			XButtonEvent & xbe = xe->xbutton;
			GHOST_TButtonMask gbmask = GHOST_kButtonMaskLeft;
			GHOST_TEventType type = (xbe.type == ButtonPress) ? 
			                        GHOST_kEventButtonDown : GHOST_kEventButtonUp;

			/* process wheel mouse events and break, only pass on press events */
			if (xbe.button == Button4) {
				if (xbe.type == ButtonPress)
					g_event = new GHOST_EventWheel(getMilliSeconds(), window, 1);
				break;
			}
			else if (xbe.button == Button5) {
				if (xbe.type == ButtonPress)
					g_event = new GHOST_EventWheel(getMilliSeconds(), window, -1);
				break;
			}
			
			/* process rest of normal mouse buttons */
			if (xbe.button == Button1)
				gbmask = GHOST_kButtonMaskLeft;
			else if (xbe.button == Button2)
				gbmask = GHOST_kButtonMaskMiddle;
			else if (xbe.button == Button3)
				gbmask = GHOST_kButtonMaskRight;
			/* It seems events 6 and 7 are for horizontal scrolling.
			 * you can re-order button mapping like this... (swaps 6,7 with 8,9)
			 *   xmodmap -e "pointer = 1 2 3 4 5 8 9 6 7"
			 */
			else if (xbe.button == 8)
				gbmask = GHOST_kButtonMaskButton4;
			else if (xbe.button == 9)
				gbmask = GHOST_kButtonMaskButton5;
			else
				break;

			g_event = new
			          GHOST_EventButton(
			    getMilliSeconds(),
			    type,
			    window,
			    gbmask
			    );
			break;
		}
			
		/* change of size, border, layer etc. */
		case ConfigureNotify:
		{
			/* XConfigureEvent & xce = xe->xconfigure; */

			g_event = new 
			          GHOST_Event(
			    getMilliSeconds(),
			    GHOST_kEventWindowSize,
			    window
			    );
			break;
		}

		case FocusIn:
		case FocusOut:
		{
			XFocusChangeEvent &xfe = xe->xfocus;

			/* TODO: make sure this is the correct place for activate/deactivate */
			// printf("X: focus %s for window %d\n", xfe.type == FocusIn ? "in" : "out", (int) xfe.window);
		
			/* May have to look at the type of event and filter some out. */

			GHOST_TEventType gtype = (xfe.type == FocusIn) ? 
			                         GHOST_kEventWindowActivate : GHOST_kEventWindowDeactivate;

#if defined(WITH_X11_XINPUT) && defined(X_HAVE_UTF8_STRING)
			XIC xic = window->getX11_XIC();
			if (xic) {
				if (xe->type == FocusIn)
					XSetICFocus(xic);
				else
					XUnsetICFocus(xic);
			}
#endif

			g_event = new 
			          GHOST_Event(
			    getMilliSeconds(),
			    gtype,
			    window
			    );
			break;

		}
		case ClientMessage:
		{
			XClientMessageEvent & xcme = xe->xclient;

			if (((Atom)xcme.data.l[0]) == m_delete_window_atom) {
				g_event = new 
				          GHOST_Event(
				    getMilliSeconds(),
				    GHOST_kEventWindowClose,
				    window
				    );
			}
			else if (((Atom)xcme.data.l[0]) == m_wm_take_focus) {
				XWindowAttributes attr;
				Window fwin;
				int revert_to;

				/* as ICCCM say, we need reply this event
				 * with a SetInputFocus, the data[1] have
				 * the valid timestamp (send by the wm).
				 *
				 * Some WM send this event before the
				 * window is really mapped (for example
				 * change from virtual desktop), so we need
				 * to be sure that our windows is mapped
				 * or this call fail and close blender.
				 */
				if (XGetWindowAttributes(m_display, xcme.window, &attr) == True) {
					if (XGetInputFocus(m_display, &fwin, &revert_to) == True) {
						if (attr.map_state == IsViewable) {
							if (fwin != xcme.window)
								XSetInputFocus(m_display, xcme.window, RevertToParent, xcme.data.l[1]);
						}
					}
				}
			}
			else {
#ifdef WITH_XDND
				/* try to handle drag event (if there's no such events, GHOST_HandleClientMessage will return zero) */
				if (window->getDropTarget()->GHOST_HandleClientMessage(xe) == false) {
					/* Unknown client message, ignore */
				}
#else
				/* Unknown client message, ignore */
#endif
			}

			break;
		}
		
		case DestroyNotify:
			::exit(-1);	
		/* We're not interested in the following things.(yet...) */
		case NoExpose:
		case GraphicsExpose:
			break;
		
		case EnterNotify:
		case LeaveNotify:
		{
			/* XCrossingEvents pointer leave enter window.
			 * also do cursor move here, MotionNotify only
			 * happens when motion starts & ends inside window.
			 * we only do moves when the crossing mode is 'normal'
			 * (really crossing between windows) since some windowmanagers
			 * also send grab/ungrab crossings for mousewheel events.
			 */
			XCrossingEvent &xce = xe->xcrossing;
			if (xce.mode == NotifyNormal) {
				g_event = new 
				          GHOST_EventCursor(
				    getMilliSeconds(),
				    GHOST_kEventCursorMove,
				    window,
				    xce.x_root,
				    xce.y_root
				    );
			}

			// printf("X: %s window %d\n", xce.type == EnterNotify ? "entering" : "leaving", (int) xce.window);

			if (xce.type == EnterNotify)
				m_windowManager->setActiveWindow(window);
			else
				m_windowManager->setWindowInactive(window);

			break;
		}
		case MapNotify:
			/*
			 * From ICCCM:
			 * [ Clients can select for StructureNotify on their
			 *   top-level windows to track transition between
			 *   Normal and Iconic states. Receipt of a MapNotify
			 *   event will indicate a transition to the Normal
			 *   state, and receipt of an UnmapNotify event will
			 *   indicate a transition to the Iconic state. ]
			 */
			if (window->m_post_init == True) {
				/*
				 * Now we are sure that the window is
				 * mapped, so only need change the state.
				 */
				window->setState(window->m_post_state);
				window->m_post_init = False;
			}
			break;
		case UnmapNotify:
			break;
		case MappingNotify:
		case ReparentNotify:
			break;
		case SelectionRequest:
		{
			XEvent nxe;
			Atom target, utf8_string, string, compound_text, c_string;
			XSelectionRequestEvent *xse = &xe->xselectionrequest;
			
			target = XInternAtom(m_display, "TARGETS", False);
			utf8_string = XInternAtom(m_display, "UTF8_STRING", False);
			string = XInternAtom(m_display, "STRING", False);
			compound_text = XInternAtom(m_display, "COMPOUND_TEXT", False);
			c_string = XInternAtom(m_display, "C_STRING", False);
			
			/* support obsolete clients */
			if (xse->property == None) {
				xse->property = xse->target;
			}
			
			nxe.xselection.type = SelectionNotify;
			nxe.xselection.requestor = xse->requestor;
			nxe.xselection.property = xse->property;
			nxe.xselection.display = xse->display;
			nxe.xselection.selection = xse->selection;
			nxe.xselection.target = xse->target;
			nxe.xselection.time = xse->time;
			
			/* Check to see if the requestor is asking for String */
			if (xse->target == utf8_string ||
			    xse->target == string ||
			    xse->target == compound_text ||
			    xse->target == c_string)
			{
				if (xse->selection == XInternAtom(m_display, "PRIMARY", False)) {
					XChangeProperty(m_display, xse->requestor, xse->property, xse->target, 8, PropModeReplace,
					                (unsigned char *)txt_select_buffer, strlen(txt_select_buffer));
				}
				else if (xse->selection == XInternAtom(m_display, "CLIPBOARD", False)) {
					XChangeProperty(m_display, xse->requestor, xse->property, xse->target, 8, PropModeReplace,
					                (unsigned char *)txt_cut_buffer, strlen(txt_cut_buffer));
				}
			}
			else if (xse->target == target) {
				Atom alist[5];
				alist[0] = target;
				alist[1] = utf8_string;
				alist[2] = string;
				alist[3] = compound_text;
				alist[4] = c_string;
				XChangeProperty(m_display, xse->requestor, xse->property, xse->target, 32, PropModeReplace,
				                (unsigned char *)alist, 5);
				XFlush(m_display);
			}
			else {
				/* Change property to None because we do not support anything but STRING */
				nxe.xselection.property = None;
			}
			
			/* Send the event to the client 0 0 == False, SelectionNotify */
			XSendEvent(m_display, xse->requestor, 0, 0, &nxe);
			XFlush(m_display);
			break;
		}
		
		default: {
#ifdef WITH_X11_XINPUT
			if (xe->type == window->GetXTablet().MotionEvent)
			{
				XDeviceMotionEvent *data = (XDeviceMotionEvent *)xe;

				/* stroke might begin without leading ProxyIn event,
				 * this happens when window is opened when stylus is already hovering
				 * around tablet surface */
				setTabletMode(window, data->deviceid);

				window->GetXTablet().CommonData.Pressure =
				    data->axis_data[2] / ((float)window->GetXTablet().PressureLevels);
			
				/* the (short) cast and the &0xffff is bizarre and unexplained anywhere,
				 * but I got garbage data without it. Found it in the xidump.c source --matt */
				window->GetXTablet().CommonData.Xtilt =
				    (short)(data->axis_data[3] & 0xffff) / ((float)window->GetXTablet().XtiltLevels);
				window->GetXTablet().CommonData.Ytilt =
				    (short)(data->axis_data[4] & 0xffff) / ((float)window->GetXTablet().YtiltLevels);
			}
			else if (xe->type == window->GetXTablet().ProxInEvent)
			{
				XProximityNotifyEvent *data = (XProximityNotifyEvent *)xe;

				setTabletMode(window, data->deviceid);
			}
			else if (xe->type == window->GetXTablet().ProxOutEvent)
				window->GetXTablet().CommonData.Active = GHOST_kTabletModeNone;
#endif // WITH_X11_XINPUT
			break;
		}
	}

	if (g_event) {
		pushEvent(g_event);
	}
}

GHOST_TSuccess
GHOST_SystemX11::
getModifierKeys(
		GHOST_ModifierKeys& keys) const
{

	/* analyse the masks retuned from XQueryPointer. */

	memset((void *)m_keyboard_vector, 0, sizeof(m_keyboard_vector));

	XQueryKeymap(m_display, (char *)m_keyboard_vector);

	/* now translate key symobols into keycodes and
	 * test with vector. */

	const static KeyCode shift_l = XKeysymToKeycode(m_display, XK_Shift_L);
	const static KeyCode shift_r = XKeysymToKeycode(m_display, XK_Shift_R);
	const static KeyCode control_l = XKeysymToKeycode(m_display, XK_Control_L);
	const static KeyCode control_r = XKeysymToKeycode(m_display, XK_Control_R);
	const static KeyCode alt_l = XKeysymToKeycode(m_display, XK_Alt_L);
	const static KeyCode alt_r = XKeysymToKeycode(m_display, XK_Alt_R);
	const static KeyCode super_l = XKeysymToKeycode(m_display, XK_Super_L);
	const static KeyCode super_r = XKeysymToKeycode(m_display, XK_Super_R);

	/* shift */
	keys.set(GHOST_kModifierKeyLeftShift, ((m_keyboard_vector[shift_l >> 3] >> (shift_l & 7)) & 1) != 0);
	keys.set(GHOST_kModifierKeyRightShift, ((m_keyboard_vector[shift_r >> 3] >> (shift_r & 7)) & 1) != 0);
	/* control */
	keys.set(GHOST_kModifierKeyLeftControl, ((m_keyboard_vector[control_l >> 3] >> (control_l & 7)) & 1) != 0);
	keys.set(GHOST_kModifierKeyRightControl, ((m_keyboard_vector[control_r >> 3] >> (control_r & 7)) & 1) != 0);
	/* alt */
	keys.set(GHOST_kModifierKeyLeftAlt, ((m_keyboard_vector[alt_l >> 3] >> (alt_l & 7)) & 1) != 0);
	keys.set(GHOST_kModifierKeyRightAlt, ((m_keyboard_vector[alt_r >> 3] >> (alt_r & 7)) & 1) != 0);
	/* super (windows) - only one GHOST-kModifierKeyOS, so mapping to either */
	keys.set(GHOST_kModifierKeyOS, ( ((m_keyboard_vector[super_l >> 3] >> (super_l & 7)) & 1) ||
	                                 ((m_keyboard_vector[super_r >> 3] >> (super_r & 7)) & 1) ) != 0);

	return GHOST_kSuccess;
}

GHOST_TSuccess
GHOST_SystemX11::
getButtons(
		GHOST_Buttons& buttons) const
{
	Window root_return, child_return;
	int rx, ry, wx, wy;
	unsigned int mask_return;

	if (XQueryPointer(m_display,
	                  RootWindow(m_display, DefaultScreen(m_display)),
	                  &root_return,
	                  &child_return,
	                  &rx, &ry,
	                  &wx, &wy,
	                  &mask_return) == True)
	{
		buttons.set(GHOST_kButtonMaskLeft,   (mask_return & Button1Mask) != 0);
		buttons.set(GHOST_kButtonMaskMiddle, (mask_return & Button2Mask) != 0);
		buttons.set(GHOST_kButtonMaskRight,  (mask_return & Button3Mask) != 0);
	}
	else {
		return GHOST_kFailure;
	}	

	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_SystemX11::
getCursorPosition(
		GHOST_TInt32& x,
		GHOST_TInt32& y) const
{

	Window root_return, child_return;
	int rx, ry, wx, wy;
	unsigned int mask_return;

	if (XQueryPointer(
	        m_display,
	        RootWindow(m_display, DefaultScreen(m_display)),
	        &root_return,
	        &child_return,
	        &rx, &ry,
	        &wx, &wy,
	        &mask_return
	        ) == False) {
		return GHOST_kFailure;
	}
	else {
		x = rx;
		y = ry;
	}	
	return GHOST_kSuccess;
}


GHOST_TSuccess
GHOST_SystemX11::
setCursorPosition(
		GHOST_TInt32 x,
		GHOST_TInt32 y
        ) {

	/* This is a brute force move in screen coordinates
	 * XWarpPointer does relative moves so first determine the
	 * current pointer position. */

	int cx, cy;
	if (getCursorPosition(cx, cy) == GHOST_kFailure) {
		return GHOST_kFailure;
	}

	int relx = x - cx;
	int rely = y - cy;

	XWarpPointer(m_display, None, None, 0, 0, 0, 0, relx, rely);
	XSync(m_display, 0); /* Sync to process all requests */
	
	return GHOST_kSuccess;
}


void
GHOST_SystemX11::
addDirtyWindow(
		GHOST_WindowX11 *bad_wind)
{
	GHOST_ASSERT((bad_wind != NULL), "addDirtyWindow() NULL ptr trapped (window)");
	
	m_dirty_windows.push_back(bad_wind);
}


bool
GHOST_SystemX11::
generateWindowExposeEvents()
{
	vector<GHOST_WindowX11 *>::iterator w_start = m_dirty_windows.begin();
	vector<GHOST_WindowX11 *>::const_iterator w_end = m_dirty_windows.end();
	bool anyProcessed = false;
	
	for (; w_start != w_end; ++w_start) {
		GHOST_Event *g_event = new
		                       GHOST_Event(
		    getMilliSeconds(),
		    GHOST_kEventWindowUpdate,
		    *w_start
		    );

		(*w_start)->validate();	
		
		if (g_event) {
			pushEvent(g_event);
			anyProcessed = true;
		}
	}

	m_dirty_windows.clear();
	return anyProcessed;
}

#define GXMAP(k, x, y) case x: k = y; break

static GHOST_TKey
convertXKey(KeySym key)
{
	GHOST_TKey type;

	if ((key >= XK_A) && (key <= XK_Z)) {
		type = GHOST_TKey(key - XK_A + int(GHOST_kKeyA));
	}
	else if ((key >= XK_a) && (key <= XK_z)) {
		type = GHOST_TKey(key - XK_a + int(GHOST_kKeyA));
	}
	else if ((key >= XK_0) && (key <= XK_9)) {
		type = GHOST_TKey(key - XK_0 + int(GHOST_kKey0));
	}
	else if ((key >= XK_F1) && (key <= XK_F24)) {
		type = GHOST_TKey(key - XK_F1 + int(GHOST_kKeyF1));
#if defined(__sun) || defined(__sun__) 
		/* This is a bit of a hack, but it looks like sun
		 * Used F11 and friends for its special keys Stop,again etc..
		 * So this little patch enables F11 and F12 to work as expected
		 * following link has documentation on it:
		 * http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4734408
		 * also from /usr/include/X11/Sunkeysym.h
		 * #define SunXK_F36               0x1005FF10      // Labeled F11
		 * #define SunXK_F37               0x1005FF11      // Labeled F12
		 *
		 *      mein@cs.umn.edu
		 */
		
	}
	else if (key == 268828432) {
		type = GHOST_kKeyF11;
	}
	else if (key == 268828433) {
		type = GHOST_kKeyF12;
#endif
	}
	else {
		switch (key) {
			GXMAP(type, XK_BackSpace,    GHOST_kKeyBackSpace);
			GXMAP(type, XK_Tab,          GHOST_kKeyTab);
			GXMAP(type, XK_Return,       GHOST_kKeyEnter);
			GXMAP(type, XK_Escape,       GHOST_kKeyEsc);
			GXMAP(type, XK_space,        GHOST_kKeySpace);

			GXMAP(type, XK_Linefeed,     GHOST_kKeyLinefeed);
			GXMAP(type, XK_semicolon,    GHOST_kKeySemicolon);
			GXMAP(type, XK_period,       GHOST_kKeyPeriod);
			GXMAP(type, XK_comma,        GHOST_kKeyComma);
			GXMAP(type, XK_quoteright,   GHOST_kKeyQuote);
			GXMAP(type, XK_quoteleft,    GHOST_kKeyAccentGrave);
			GXMAP(type, XK_minus,        GHOST_kKeyMinus);
			GXMAP(type, XK_slash,        GHOST_kKeySlash);
			GXMAP(type, XK_backslash,    GHOST_kKeyBackslash);
			GXMAP(type, XK_equal,        GHOST_kKeyEqual);
			GXMAP(type, XK_bracketleft,  GHOST_kKeyLeftBracket);
			GXMAP(type, XK_bracketright, GHOST_kKeyRightBracket);
			GXMAP(type, XK_Pause,        GHOST_kKeyPause);

			GXMAP(type, XK_Shift_L,      GHOST_kKeyLeftShift);
			GXMAP(type, XK_Shift_R,      GHOST_kKeyRightShift);
			GXMAP(type, XK_Control_L,    GHOST_kKeyLeftControl);
			GXMAP(type, XK_Control_R,    GHOST_kKeyRightControl);
			GXMAP(type, XK_Alt_L,        GHOST_kKeyLeftAlt);
			GXMAP(type, XK_Alt_R,        GHOST_kKeyRightAlt);
			GXMAP(type, XK_Super_L,      GHOST_kKeyOS);
			GXMAP(type, XK_Super_R,      GHOST_kKeyOS);

			GXMAP(type, XK_Insert,       GHOST_kKeyInsert);
			GXMAP(type, XK_Delete,       GHOST_kKeyDelete);
			GXMAP(type, XK_Home,         GHOST_kKeyHome);
			GXMAP(type, XK_End,          GHOST_kKeyEnd);
			GXMAP(type, XK_Page_Up,      GHOST_kKeyUpPage);
			GXMAP(type, XK_Page_Down,    GHOST_kKeyDownPage);

			GXMAP(type, XK_Left,         GHOST_kKeyLeftArrow);
			GXMAP(type, XK_Right,        GHOST_kKeyRightArrow);
			GXMAP(type, XK_Up,           GHOST_kKeyUpArrow);
			GXMAP(type, XK_Down,         GHOST_kKeyDownArrow);

			GXMAP(type, XK_Caps_Lock,    GHOST_kKeyCapsLock);
			GXMAP(type, XK_Scroll_Lock,  GHOST_kKeyScrollLock);
			GXMAP(type, XK_Num_Lock,     GHOST_kKeyNumLock);

			/* keypad events */

			GXMAP(type, XK_KP_0,         GHOST_kKeyNumpad0);
			GXMAP(type, XK_KP_1,         GHOST_kKeyNumpad1);
			GXMAP(type, XK_KP_2,         GHOST_kKeyNumpad2);
			GXMAP(type, XK_KP_3,         GHOST_kKeyNumpad3);
			GXMAP(type, XK_KP_4,         GHOST_kKeyNumpad4);
			GXMAP(type, XK_KP_5,         GHOST_kKeyNumpad5);
			GXMAP(type, XK_KP_6,         GHOST_kKeyNumpad6);
			GXMAP(type, XK_KP_7,         GHOST_kKeyNumpad7);
			GXMAP(type, XK_KP_8,         GHOST_kKeyNumpad8);
			GXMAP(type, XK_KP_9,         GHOST_kKeyNumpad9);
			GXMAP(type, XK_KP_Decimal,   GHOST_kKeyNumpadPeriod);

			GXMAP(type, XK_KP_Insert,    GHOST_kKeyNumpad0);
			GXMAP(type, XK_KP_End,       GHOST_kKeyNumpad1);
			GXMAP(type, XK_KP_Down,      GHOST_kKeyNumpad2);
			GXMAP(type, XK_KP_Page_Down, GHOST_kKeyNumpad3);
			GXMAP(type, XK_KP_Left,      GHOST_kKeyNumpad4);
			GXMAP(type, XK_KP_Begin,     GHOST_kKeyNumpad5);
			GXMAP(type, XK_KP_Right,     GHOST_kKeyNumpad6);
			GXMAP(type, XK_KP_Home,      GHOST_kKeyNumpad7);
			GXMAP(type, XK_KP_Up,        GHOST_kKeyNumpad8);
			GXMAP(type, XK_KP_Page_Up,   GHOST_kKeyNumpad9);
			GXMAP(type, XK_KP_Delete,    GHOST_kKeyNumpadPeriod);

			GXMAP(type, XK_KP_Enter,     GHOST_kKeyNumpadEnter);
			GXMAP(type, XK_KP_Add,       GHOST_kKeyNumpadPlus);
			GXMAP(type, XK_KP_Subtract,  GHOST_kKeyNumpadMinus);
			GXMAP(type, XK_KP_Multiply,  GHOST_kKeyNumpadAsterisk);
			GXMAP(type, XK_KP_Divide,    GHOST_kKeyNumpadSlash);

			/* Media keys in some keyboards and laptops with XFree86/Xorg */
#ifdef WITH_XF86KEYSYM
			GXMAP(type, XF86XK_AudioPlay,    GHOST_kKeyMediaPlay);
			GXMAP(type, XF86XK_AudioStop,    GHOST_kKeyMediaStop);
			GXMAP(type, XF86XK_AudioPrev,    GHOST_kKeyMediaFirst);
			GXMAP(type, XF86XK_AudioRewind,  GHOST_kKeyMediaFirst);
			GXMAP(type, XF86XK_AudioNext,    GHOST_kKeyMediaLast);
#ifdef XF86XK_AudioForward /* Debian lenny's XF86keysym.h has no XF86XK_AudioForward define */
			GXMAP(type, XF86XK_AudioForward, GHOST_kKeyMediaLast);
#endif
#endif

			/* some extra sun cruft (NICE KEYBOARD!) */
#ifdef __sun__
			GXMAP(type, 0xffde,          GHOST_kKeyNumpad1);
			GXMAP(type, 0xffe0,          GHOST_kKeyNumpad3);
			GXMAP(type, 0xffdc,          GHOST_kKeyNumpad5);
			GXMAP(type, 0xffd8,          GHOST_kKeyNumpad7);
			GXMAP(type, 0xffda,          GHOST_kKeyNumpad9);

			GXMAP(type, 0xffd6,          GHOST_kKeyNumpadSlash);
			GXMAP(type, 0xffd7,          GHOST_kKeyNumpadAsterisk);
#endif

			default:
				type = GHOST_kKeyUnknown;
				break;
		}
	}

	return type;
}

#undef GXMAP

/* from xclip.c xcout() v0.11 */

#define XCLIB_XCOUT_NONE            0 /* no context */
#define XCLIB_XCOUT_SENTCONVSEL     1 /* sent a request */
#define XCLIB_XCOUT_INCR            2 /* in an incr loop */
#define XCLIB_XCOUT_FALLBACK        3 /* STRING failed, need fallback to UTF8 */
#define XCLIB_XCOUT_FALLBACK_UTF8   4 /* UTF8 failed, move to compouned */
#define XCLIB_XCOUT_FALLBACK_COMP   5 /* compouned failed, move to text. */
#define XCLIB_XCOUT_FALLBACK_TEXT   6

/* Retrieves the contents of a selections. */
void GHOST_SystemX11::getClipboard_xcout(XEvent evt,
		Atom sel, Atom target, unsigned char **txt,
		unsigned long *len, unsigned int *context) const
{
	Atom pty_type;
	int pty_format;
	unsigned char *buffer;
	unsigned long pty_size, pty_items;
	unsigned char *ltxt = *txt;

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();
	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
	Window win = window->getXWindow();

	switch (*context) {
		/* There is no context, do an XConvertSelection() */
		case XCLIB_XCOUT_NONE:
			/* Initialise return length to 0 */
			if (*len > 0) {
				free(*txt);
				*len = 0;
			}

			/* Send a selection request */
			XConvertSelection(m_display, sel, target, m_xclip_out, win, CurrentTime);
			*context = XCLIB_XCOUT_SENTCONVSEL;
			return;

		case XCLIB_XCOUT_SENTCONVSEL:
			if (evt.type != SelectionNotify)
				return;

			if (target == m_utf8_string && evt.xselection.property == None) {
				*context = XCLIB_XCOUT_FALLBACK_UTF8;
				return;
			}
			else if (target == m_compound_text && evt.xselection.property == None) {
				*context = XCLIB_XCOUT_FALLBACK_COMP;
				return;
			}
			else if (target == m_text && evt.xselection.property == None) {
				*context = XCLIB_XCOUT_FALLBACK_TEXT;
				return;
			}

			/* find the size and format of the data in property */
			XGetWindowProperty(m_display, win, m_xclip_out, 0, 0, False,
			                   AnyPropertyType, &pty_type, &pty_format,
			                   &pty_items, &pty_size, &buffer);
			XFree(buffer);

			if (pty_type == m_incr) {
				/* start INCR mechanism by deleting property */
				XDeleteProperty(m_display, win, m_xclip_out);
				XFlush(m_display);
				*context = XCLIB_XCOUT_INCR;
				return;
			}

			/* if it's not incr, and not format == 8, then there's
			 * nothing in the selection (that xclip understands, anyway) */

			if (pty_format != 8) {
				*context = XCLIB_XCOUT_NONE;
				return;
			}

			// not using INCR mechanism, just read the property
			XGetWindowProperty(m_display, win, m_xclip_out, 0, (long) pty_size,
			                   False, AnyPropertyType, &pty_type,
			                   &pty_format, &pty_items, &pty_size, &buffer);

			/* finished with property, delete it */
			XDeleteProperty(m_display, win, m_xclip_out);

			/* copy the buffer to the pointer for returned data */
			ltxt = (unsigned char *) malloc(pty_items);
			memcpy(ltxt, buffer, pty_items);

			/* set the length of the returned data */
			*len = pty_items;
			*txt = ltxt;

			/* free the buffer */
			XFree(buffer);

			*context = XCLIB_XCOUT_NONE;

			/* complete contents of selection fetched, return 1 */
			return;

		case XCLIB_XCOUT_INCR:
			/* To use the INCR method, we basically delete the
			 * property with the selection in it, wait for an
			 * event indicating that the property has been created,
			 * then read it, delete it, etc. */

			/* make sure that the event is relevant */
			if (evt.type != PropertyNotify)
				return;

			/* skip unless the property has a new value */
			if (evt.xproperty.state != PropertyNewValue)
				return;

			/* check size and format of the property */
			XGetWindowProperty(m_display, win, m_xclip_out, 0, 0, False,
			                   AnyPropertyType, &pty_type, &pty_format,
			                   &pty_items, &pty_size, (unsigned char **) &buffer);

			if (pty_format != 8) {
				/* property does not contain text, delete it
				 * to tell the other X client that we have read
				 * it and to send the next property */
				XFree(buffer);
				XDeleteProperty(m_display, win, m_xclip_out);
				return;
			}

			if (pty_size == 0) {
				/* no more data, exit from loop */
				XFree(buffer);
				XDeleteProperty(m_display, win, m_xclip_out);
				*context = XCLIB_XCOUT_NONE;

				/* this means that an INCR transfer is now
				 * complete, return 1 */
				return;
			}

			XFree(buffer);

			/* if we have come this far, the property contains
			 * text, we know the size. */
			XGetWindowProperty(m_display, win, m_xclip_out, 0, (long) pty_size,
			                   False, AnyPropertyType, &pty_type, &pty_format,
			                   &pty_items, &pty_size, (unsigned char **) &buffer);

			/* allocate memory to accommodate data in *txt */
			if (*len == 0) {
				*len = pty_items;
				ltxt = (unsigned char *) malloc(*len);
			}
			else {
				*len += pty_items;
				ltxt = (unsigned char *) realloc(ltxt, *len);
			}

			/* add data to ltxt */
			memcpy(&ltxt[*len - pty_items], buffer, pty_items);

			*txt = ltxt;
			XFree(buffer);

			/* delete property to get the next item */
			XDeleteProperty(m_display, win, m_xclip_out);
			XFlush(m_display);
			return;
	}
	return;
}

GHOST_TUns8 *GHOST_SystemX11::getClipboard(bool selection) const
{
	Atom sseln;
	Atom target = m_utf8_string;
	Window owner;

	/* from xclip.c doOut() v0.11 */
	unsigned char *sel_buf;
	unsigned long sel_len = 0;
	XEvent evt;
	unsigned int context = XCLIB_XCOUT_NONE;

	if (selection == True)
		sseln = m_primary;
	else
		sseln = m_clipboard;

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();
	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
	Window win = window->getXWindow();

	/* check if we are the owner. */
	owner = XGetSelectionOwner(m_display, sseln);
	if (owner == win) {
		if (sseln == m_clipboard) {
			sel_buf = (unsigned char *)malloc(strlen(txt_cut_buffer) + 1);
			strcpy((char *)sel_buf, txt_cut_buffer);
			return((GHOST_TUns8 *)sel_buf);
		}
		else {
			sel_buf = (unsigned char *)malloc(strlen(txt_select_buffer) + 1);
			strcpy((char *)sel_buf, txt_select_buffer);
			return((GHOST_TUns8 *)sel_buf);
		}
	}
	else if (owner == None)
		return(NULL);

	while (1) {
		/* only get an event if xcout() is doing something */
		if (context != XCLIB_XCOUT_NONE)
			XNextEvent(m_display, &evt);

		/* fetch the selection, or part of it */
		getClipboard_xcout(evt, sseln, target, &sel_buf, &sel_len, &context);

		/* fallback is needed. set XA_STRING to target and restart the loop. */
		if (context == XCLIB_XCOUT_FALLBACK) {
			context = XCLIB_XCOUT_NONE;
			target = m_string;
			continue;
		}
		else if (context == XCLIB_XCOUT_FALLBACK_UTF8) {
			/* utf8 fail, move to compouned text. */
			context = XCLIB_XCOUT_NONE;
			target = m_compound_text;
			continue;
		}
		else if (context == XCLIB_XCOUT_FALLBACK_COMP) {
			/* compouned text faile, move to text. */
			context = XCLIB_XCOUT_NONE;
			target = m_text;
			continue;
		}

		/* only continue if xcout() is doing something */
		if (context == XCLIB_XCOUT_NONE)
			break;
	}

	if (sel_len) {
		/* only print the buffer out, and free it, if it's not
		 * empty
		 */
		unsigned char *tmp_data = (unsigned char *) malloc(sel_len + 1);
		memcpy((char *)tmp_data, (char *)sel_buf, sel_len);
		tmp_data[sel_len] = '\0';
		
		if (sseln == m_string)
			XFree(sel_buf);
		else
			free(sel_buf);
		
		return (GHOST_TUns8 *)tmp_data;
	}
	return(NULL);
}

void GHOST_SystemX11::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{
	Window m_window, owner;

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();	
	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	GHOST_WindowX11 *window = static_cast<GHOST_WindowX11 *>(*win_it);
	m_window = window->getXWindow();

	if (buffer) {
		if (selection == False) {
			XSetSelectionOwner(m_display, m_clipboard, m_window, CurrentTime);
			owner = XGetSelectionOwner(m_display, m_clipboard);
			if (txt_cut_buffer)
				free((void *)txt_cut_buffer);

			txt_cut_buffer = (char *) malloc(strlen(buffer) + 1);
			strcpy(txt_cut_buffer, buffer);
		}
		else {
			XSetSelectionOwner(m_display, m_primary, m_window, CurrentTime);
			owner = XGetSelectionOwner(m_display, m_primary);
			if (txt_select_buffer)
				free((void *)txt_select_buffer);

			txt_select_buffer = (char *) malloc(strlen(buffer) + 1);
			strcpy(txt_select_buffer, buffer);
		}

		if (owner != m_window)
			fprintf(stderr, "failed to own primary\n");
	}
}

#ifdef WITH_XDND
GHOST_TSuccess GHOST_SystemX11::pushDragDropEvent(GHOST_TEventType eventType, 
		GHOST_TDragnDropTypes draggedObjectType,
		GHOST_IWindow *window,
		int mouseX, int mouseY,
		void *data)
{
	GHOST_SystemX11 *system = ((GHOST_SystemX11 *)getSystem());
	return system->pushEvent(new GHOST_EventDragnDrop(system->getMilliSeconds(),
	                                                  eventType,
	                                                  draggedObjectType,
	                                                  window, mouseX, mouseY, data)
	                         );
}
#endif
