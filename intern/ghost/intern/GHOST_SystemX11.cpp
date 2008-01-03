/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "GHOST_SystemX11.h"
#include "GHOST_WindowX11.h"
#include "GHOST_WindowManager.h"
#include "GHOST_TimerManager.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventWheel.h"
#include "GHOST_DisplayManagerX11.h"

#include "GHOST_Debug.h"

#include <X11/Xatom.h>
#include <X11/keysym.h>

#ifdef __sgi

#if defined(_SGI_EXTRA_PREDEFINES) && !defined(NO_FAST_ATOMS)
#include <X11/SGIFastAtom.h>
#else
#define XSGIFastInternAtom(dpy,string,fast_name,how) XInternAtom(dpy,string,how)
#endif

#endif

// For timing

#include <sys/time.h>
#include <unistd.h>

#include <vector>

using namespace std;

GHOST_SystemX11::
GHOST_SystemX11(
) : 
	GHOST_System(),
	m_start_time(0)
{
	m_display = XOpenDisplay(NULL);
	
	if (!m_display) return;
	
#ifdef __sgi
	m_delete_window_atom 
	  = XSGIFastInternAtom(m_display,
			       "WM_DELETE_WINDOW", 
			       SGI_XA_WM_DELETE_WINDOW, False);
	/* Some one with SGI can tell me about this ? */
	m_wm_state= None;
	m_wm_change_state= None;
	m_net_state= None;
	m_net_max_horz= None;
	m_net_max_vert= None;
	m_net_fullscreen= None;
	m_motif = None;
#else
	m_delete_window_atom= XInternAtom(m_display, "WM_DELETE_WINDOW", False);
	m_wm_state= XInternAtom(m_display, "WM_STATE", False);
	m_wm_change_state= XInternAtom(m_display, "WM_CHANGE_STATE", False);
	m_net_state= XInternAtom(m_display, "_NET_WM_STATE", False);
	m_net_max_horz= XInternAtom(m_display,
					"_NET_WM_STATE_MAXIMIZED_HORZ", False);
	m_net_max_vert= XInternAtom(m_display,
					"_NET_WM_STATE_MAXIMIZED_VERT", False);
	m_net_fullscreen= XInternAtom(m_display,
					"_NET_WM_STATE_FULLSCREEN", False);
	m_motif= XInternAtom(m_display, "_MOTIF_WM_HINTS", False);
#endif

	// compute the initial time
	timeval tv;
	if (gettimeofday(&tv,NULL) == -1) {
		GHOST_ASSERT(false,"Could not instantiate timer!");
	}

	m_start_time = GHOST_TUns64(tv.tv_sec*1000 + tv.tv_usec/1000);
}

	GHOST_TSuccess 
GHOST_SystemX11::
init(
){
	GHOST_TSuccess success = GHOST_System::init();

	if (success) {
		m_keyboard_vector = new char[32];

		m_displayManager = new GHOST_DisplayManagerX11(this);

		if (m_keyboard_vector && m_displayManager) {
			return GHOST_kSuccess;
		}
	}

	return GHOST_kFailure;
}
	


	GHOST_TUns64
GHOST_SystemX11::
getMilliSeconds(
) const {
	timeval tv;
	if (gettimeofday(&tv,NULL) == -1) {
		GHOST_ASSERT(false,"Could not compute time!");
	}

	return  GHOST_TUns64(tv.tv_sec*1000 + tv.tv_usec/1000) - m_start_time;
}
	
	GHOST_TUns8 
GHOST_SystemX11::
getNumDisplays(
) const {
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
	GHOST_TUns32& height
) const {	
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
	 * @return	The new window (or 0 if creation failed).
	 */
	GHOST_IWindow* 
GHOST_SystemX11::
createWindow(
	const STR_String& title,
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	bool stereoVisual
){
	GHOST_WindowX11 * window = 0;
	
	if (!m_display) return 0;
	
	window = new GHOST_WindowX11 (
		this,m_display,title, left, top, width, height, state, type, stereoVisual
	);

	if (window) {

		// Install a new protocol for this window - so we can overide
		// the default window closure mechanism.

		XSetWMProtocols(m_display, window->getXWindow(), &m_delete_window_atom, 1);

		if (window->getValid()) {
			// Store the pointer to the window 
			m_windowManager->addWindow(window);
			
			pushEvent( new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window) );
		}
		else {
			delete window;
			window = 0;
		}
	}
	return window;

}

	GHOST_WindowX11 * 
GHOST_SystemX11::
findGhostWindow(
	Window xwind
) const {
	
	if (xwind == 0) return NULL;

	// It is not entirely safe to do this as the backptr may point
	// to a window that has recently been removed. 
	// We should always check the window manager's list of windows 
	// and only process events on these windows.

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();

	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	vector<GHOST_IWindow *>::const_iterator win_end = win_vec.end();
	
	for (; win_it != win_end; ++win_it) {
		GHOST_WindowX11 * window = static_cast<GHOST_WindowX11 *>(*win_it);
		if (window->getXWindow() == xwind) {
			return window;
		}
	}
	return NULL;
	
}

static void SleepTillEvent(Display *display, GHOST_TInt64 maxSleep) {
	int fd = ConnectionNumber(display);
	fd_set fds;
	
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (maxSleep == -1) {
	    select(fd + 1, &fds, NULL, NULL, NULL);
	} else {
		timeval tv;

		tv.tv_sec = maxSleep/1000;
		tv.tv_usec = (maxSleep - tv.tv_sec*1000)*1000;
	
	    select(fd + 1, &fds, NULL, NULL, &tv);
	}
}

	bool 
GHOST_SystemX11::
processEvents(
	bool waitForEvent
){
	// Get all the current events -- translate them into 
	// ghost events and call base class pushEvent() method.
	
	bool anyProcessed = false;
	
	do {
		GHOST_TimerManager* timerMgr = getTimerManager();
		
		if (waitForEvent && m_dirty_windows.empty() && !XPending(m_display)) {
			GHOST_TUns64 next = timerMgr->nextFireTime();
			
			if (next==GHOST_kFireTimeNever) {
				SleepTillEvent(m_display, -1);
			} else {
				SleepTillEvent(m_display, next - getMilliSeconds());
			}
		}
		
		if (timerMgr->fireTimers(getMilliSeconds())) {
			anyProcessed = true;
		}
		
		while (XPending(m_display)) {
			XEvent xevent;
			XNextEvent(m_display, &xevent);
			processEvent(&xevent);
			anyProcessed = true;
		}
		
		if (generateWindowExposeEvents()) {
			anyProcessed = true;
		}
	} while (waitForEvent && !anyProcessed);
	
	return anyProcessed;
}

	void
GHOST_SystemX11::processEvent(XEvent *xe)
{
	GHOST_WindowX11 * window = findGhostWindow(xe->xany.window);	
	GHOST_Event * g_event = NULL;

	if (!window) {
		return;
	}

	switch (xe->type) {
		case Expose:
		{
			XExposeEvent & xee = xe->xexpose;

			if (xee.count == 0) {
				// Only generate a single expose event
				// per read of the event queue.

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
			
			g_event = new 
			GHOST_EventCursor(
				getMilliSeconds(),
				GHOST_kEventCursorMove,
				window,
				xme.x_root,
				xme.y_root
			);
			break;
		}

		case KeyPress:
		case KeyRelease:
		{
			XKeyEvent *xke = &(xe->xkey);
		
			KeySym key_sym = XLookupKeysym(xke,0);
			char ascii;
			
			GHOST_TKey gkey = convertXKey(key_sym);
			GHOST_TEventType type = (xke->type == KeyPress) ? 
				GHOST_kEventKeyDown : GHOST_kEventKeyUp;
			
			if (!XLookupString(xke, &ascii, 1, NULL, NULL)) {
				ascii = '\0';
			}
			
			g_event = new
			GHOST_EventKey(
				getMilliSeconds(),
				type,
				window,
				gkey,
				ascii
			);
			
		break;
		}

		case ButtonPress:
		{
			/* process wheel mouse events and break */
			if (xe->xbutton.button == 4) {
				g_event = new GHOST_EventWheel(getMilliSeconds(), window, 1);
				break;
			}
			if (xe->xbutton.button == 5) {
				g_event = new GHOST_EventWheel(getMilliSeconds(), window, -1);
				break;
			}
		}
		case ButtonRelease:
		{

			XButtonEvent & xbe = xe->xbutton;
			GHOST_TButtonMask gbmask = GHOST_kButtonMaskLeft;

			switch (xbe.button) {
				case Button1 : gbmask = GHOST_kButtonMaskLeft; break;
				case Button3 : gbmask = GHOST_kButtonMaskRight; break;
				default:
				case Button2 : gbmask = GHOST_kButtonMaskMiddle; break;
			}
			
			GHOST_TEventType type = (xbe.type == ButtonPress) ? 
				GHOST_kEventButtonDown : GHOST_kEventButtonUp;
			
			g_event = new
			GHOST_EventButton(
				getMilliSeconds(),
				type,
				window,
				gbmask
			);
			break;
		}
			
			// change of size, border, layer etc.
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
		
			// May have to look at the type of event and filter some
			// out.
									
			GHOST_TEventType gtype = (xfe.type == FocusIn) ? 
				GHOST_kEventWindowActivate : GHOST_kEventWindowDeactivate;

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

#ifndef __sgi			
			if (xcme.data.l[0] == m_delete_window_atom) {
				g_event = new 
				GHOST_Event(	
					getMilliSeconds(),
					GHOST_kEventWindowClose,
					window
				);
			} else {
				/* Unknown client message, ignore */
			}
#endif
			break;
		}
			
		// We're not interested in the following things.(yet...)
		case NoExpose : 
		case GraphicsExpose :
		
		case EnterNotify:
		case LeaveNotify:
			// XCrossingEvents pointer leave enter window.
			break;
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
				window->setState (window->m_post_state);
				window->m_post_init = False;
			}
			break;
		case UnmapNotify:
			break;
		case MappingNotify:
		case ReparentNotify:
			break;

      	default: {
			if(xe->type == window->GetXTablet().MotionEvent) 
			{
				XDeviceMotionEvent* data = (XDeviceMotionEvent*)xe;
				window->GetXTablet().CommonData.Pressure= 
					data->axis_data[2]/((float)window->GetXTablet().PressureLevels);
			
			/* the (short) cast and the &0xffff is bizarre and unexplained anywhere,
			 * but I got garbage data without it. Found it in the xidump.c source --matt */
				window->GetXTablet().CommonData.Xtilt= 
					(short)(data->axis_data[3]&0xffff)/((float)window->GetXTablet().XtiltLevels);
				window->GetXTablet().CommonData.Ytilt= 
					(short)(data->axis_data[4]&0xffff)/((float)window->GetXTablet().YtiltLevels);
			}
			else if(xe->type == window->GetXTablet().ProxInEvent) 
			{
				XProximityNotifyEvent* data = (XProximityNotifyEvent*)xe;
				if(data->deviceid == window->GetXTablet().StylusID)
					window->GetXTablet().CommonData.Active= 1;
				else if(data->deviceid == window->GetXTablet().EraserID)
					window->GetXTablet().CommonData.Active= 2;
			}
			else if(xe->type == window->GetXTablet().ProxOutEvent)
				window->GetXTablet().CommonData.Active= 0;

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
	GHOST_ModifierKeys& keys
) const {

	// analyse the masks retuned from XQueryPointer.

	memset(m_keyboard_vector,0,sizeof(m_keyboard_vector));

	XQueryKeymap(m_display,m_keyboard_vector);

	// now translate key symobols into keycodes and
	// test with vector.

	const KeyCode shift_l = XKeysymToKeycode(m_display,XK_Shift_L);
	const KeyCode shift_r = XKeysymToKeycode(m_display,XK_Shift_R);
	const KeyCode control_l = XKeysymToKeycode(m_display,XK_Control_L);
	const KeyCode control_r = XKeysymToKeycode(m_display,XK_Control_R);
	const KeyCode alt_l = XKeysymToKeycode(m_display,XK_Alt_L);
	const KeyCode alt_r = XKeysymToKeycode(m_display,XK_Alt_R);

	// Shift
	if ((m_keyboard_vector[shift_l >> 3] >> (shift_l & 7)) & 1) {
		keys.set(GHOST_kModifierKeyLeftShift,true);
	} else {
		keys.set(GHOST_kModifierKeyLeftShift,false);
	}
	if ((m_keyboard_vector[shift_r >> 3] >> (shift_r & 7)) & 1) {

		keys.set(GHOST_kModifierKeyRightShift,true);
	} else {
		keys.set(GHOST_kModifierKeyRightShift,false);
	}

	// control (weep)
	if ((m_keyboard_vector[control_l >> 3] >> (control_l & 7)) & 1) {
		keys.set(GHOST_kModifierKeyLeftControl,true);
	} else {
		keys.set(GHOST_kModifierKeyLeftControl,false);
	}
	if ((m_keyboard_vector[control_r >> 3] >> (control_r & 7)) & 1) {
		keys.set(GHOST_kModifierKeyRightControl,true);
	} else {
		keys.set(GHOST_kModifierKeyRightControl,false);
	}

	// Alt (yawn)
	if ((m_keyboard_vector[alt_l >> 3] >> (alt_l & 7)) & 1) {
		keys.set(GHOST_kModifierKeyLeftAlt,true);
	} else {
		keys.set(GHOST_kModifierKeyLeftAlt,false);
	}	
	if ((m_keyboard_vector[alt_r >> 3] >> (alt_r & 7)) & 1) {
		keys.set(GHOST_kModifierKeyRightAlt,true);
	} else {
		keys.set(GHOST_kModifierKeyRightAlt,false);
	}
	return GHOST_kSuccess;
}

	GHOST_TSuccess 
GHOST_SystemX11::
getButtons(
	GHOST_Buttons& buttons
) const {

	Window root_return, child_return;
	int rx,ry,wx,wy;
	unsigned int mask_return;

	if (XQueryPointer(
		m_display,
		RootWindow(m_display,DefaultScreen(m_display)),
		&root_return,
		&child_return,
		&rx,&ry,
		&wx,&wy,
		&mask_return
	) == False) {
		return GHOST_kFailure;
	} else {

		if (mask_return & Button1Mask) {
			buttons.set(GHOST_kButtonMaskLeft,true);
		} else {
			buttons.set(GHOST_kButtonMaskLeft,false);
		}

		if (mask_return & Button2Mask) {
			buttons.set(GHOST_kButtonMaskMiddle,true);
		} else {
			buttons.set(GHOST_kButtonMaskMiddle,false);
		}

		if (mask_return & Button3Mask) {
			buttons.set(GHOST_kButtonMaskRight,true);
		} else {
			buttons.set(GHOST_kButtonMaskRight,false);
		}
	}	

	return GHOST_kSuccess;
}


	GHOST_TSuccess 
GHOST_SystemX11::
getCursorPosition(
	GHOST_TInt32& x,
	GHOST_TInt32& y
) const {

	Window root_return, child_return;
	int rx,ry,wx,wy;
	unsigned int mask_return;

	if (XQueryPointer(
		m_display,
		RootWindow(m_display,DefaultScreen(m_display)),
		&root_return,
		&child_return,
		&rx,&ry,
		&wx,&wy,
		&mask_return
	) == False) {
		return GHOST_kFailure;
	} else {
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
) const {

	// This is a brute force move in screen coordinates
	// XWarpPointer does relative moves so first determine the
	// current pointer position.

	int cx,cy;
	if (getCursorPosition(cx,cy) == GHOST_kFailure) {
		return GHOST_kFailure;
	}

	int relx = x-cx;
	int rely = y-cy;

	XWarpPointer(m_display,None,None,0,0,0,0,relx,rely);
	XFlush(m_display);
	
	return GHOST_kSuccess;
}


	void
GHOST_SystemX11::
addDirtyWindow(
	GHOST_WindowX11 * bad_wind
){

	GHOST_ASSERT((bad_wind != NULL), "addDirtyWindow() NULL ptr trapped (window)");
	
	m_dirty_windows.push_back(bad_wind);
}


	bool
GHOST_SystemX11::
generateWindowExposeEvents(
){

	vector<GHOST_WindowX11 *>::iterator w_start = m_dirty_windows.begin();
	vector<GHOST_WindowX11 *>::const_iterator w_end = m_dirty_windows.end();
	bool anyProcessed = false;
	
	for (;w_start != w_end; ++w_start) {
		GHOST_Event * g_event = new 
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

#define GXMAP(k,x,y) case x: k = y; break; 

	GHOST_TKey
GHOST_SystemX11::
convertXKey(
	KeySym key
){
	GHOST_TKey type;

	if ((key >= XK_A) && (key <= XK_Z)) {
		type = GHOST_TKey( key - XK_A + int(GHOST_kKeyA));
	} else if ((key >= XK_a) && (key <= XK_z)) {
		type = GHOST_TKey(key - XK_a + int(GHOST_kKeyA));
	} else if ((key >= XK_0) && (key <= XK_9)) {
		type = GHOST_TKey(key - XK_0 + int(GHOST_kKey0));
	} else if ((key >= XK_F1) && (key <= XK_F24)) {
		type = GHOST_TKey(key - XK_F1 + int(GHOST_kKeyF1));
#if defined(__sun) || defined(__sun__) 
		/* This is a bit of a hack, but it looks like sun
		   Used F11 and friends for its special keys Stop,again etc..
		   So this little patch enables F11 and F12 to work as expected
		   following link has documentation on it: 
		   http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4734408
		   also from /usr/include/X11/Sunkeysym.h 
#define SunXK_F36               0x1005FF10      // Labeled F11
#define SunXK_F37               0x1005FF11      // Labeled F12 

				mein@cs.umn.edu
		 */
		
	} else if (key == 268828432) {
		type = GHOST_kKeyF11;
	} else if (key == 268828433) {
		type = GHOST_kKeyF12;
#endif
	} else {
		switch(key) {
			GXMAP(type,XK_BackSpace,	GHOST_kKeyBackSpace);
			GXMAP(type,XK_Tab,      	GHOST_kKeyTab);
			GXMAP(type,XK_Return,   	GHOST_kKeyEnter);
			GXMAP(type,XK_Escape,   	GHOST_kKeyEsc);
			GXMAP(type,XK_space,    	GHOST_kKeySpace);
			
			GXMAP(type,XK_Linefeed,		GHOST_kKeyLinefeed);
			GXMAP(type,XK_semicolon,	GHOST_kKeySemicolon);
			GXMAP(type,XK_period,		GHOST_kKeyPeriod);
			GXMAP(type,XK_comma,		GHOST_kKeyComma);
			GXMAP(type,XK_quoteright,	GHOST_kKeyQuote);
			GXMAP(type,XK_quoteleft,	GHOST_kKeyAccentGrave);
			GXMAP(type,XK_minus,		GHOST_kKeyMinus);
			GXMAP(type,XK_slash,		GHOST_kKeySlash);
			GXMAP(type,XK_backslash,	GHOST_kKeyBackslash);
			GXMAP(type,XK_equal,		GHOST_kKeyEqual);
			GXMAP(type,XK_bracketleft,	GHOST_kKeyLeftBracket);
			GXMAP(type,XK_bracketright,	GHOST_kKeyRightBracket);
			GXMAP(type,XK_Pause,		GHOST_kKeyPause);
			
			GXMAP(type,XK_Shift_L,  	GHOST_kKeyLeftShift);
			GXMAP(type,XK_Shift_R,  	GHOST_kKeyRightShift);
			GXMAP(type,XK_Control_L,	GHOST_kKeyLeftControl);
			GXMAP(type,XK_Control_R,	GHOST_kKeyRightControl);
			GXMAP(type,XK_Alt_L,	 	GHOST_kKeyLeftAlt);
			GXMAP(type,XK_Alt_R,	 	GHOST_kKeyRightAlt);

			GXMAP(type,XK_Insert,	 	GHOST_kKeyInsert);
			GXMAP(type,XK_Delete,	 	GHOST_kKeyDelete);
			GXMAP(type,XK_Home,	 		GHOST_kKeyHome);
			GXMAP(type,XK_End,			GHOST_kKeyEnd);
			GXMAP(type,XK_Page_Up,		GHOST_kKeyUpPage);
			GXMAP(type,XK_Page_Down, 	GHOST_kKeyDownPage);

			GXMAP(type,XK_Left,			GHOST_kKeyLeftArrow);
			GXMAP(type,XK_Right,		GHOST_kKeyRightArrow);
			GXMAP(type,XK_Up,			GHOST_kKeyUpArrow);
			GXMAP(type,XK_Down,			GHOST_kKeyDownArrow);

			GXMAP(type,XK_Caps_Lock,	GHOST_kKeyCapsLock);
			GXMAP(type,XK_Scroll_Lock,	GHOST_kKeyScrollLock);
			GXMAP(type,XK_Num_Lock,		GHOST_kKeyNumLock);
			
				/* keypad events */
				
			GXMAP(type,XK_KP_0,	 		GHOST_kKeyNumpad0);
			GXMAP(type,XK_KP_1,	 		GHOST_kKeyNumpad1);
			GXMAP(type,XK_KP_2,	 		GHOST_kKeyNumpad2);
			GXMAP(type,XK_KP_3,	 		GHOST_kKeyNumpad3);
			GXMAP(type,XK_KP_4,	 		GHOST_kKeyNumpad4);
			GXMAP(type,XK_KP_5,	 		GHOST_kKeyNumpad5);
			GXMAP(type,XK_KP_6,	 		GHOST_kKeyNumpad6);
			GXMAP(type,XK_KP_7,	 		GHOST_kKeyNumpad7);
			GXMAP(type,XK_KP_8,	 		GHOST_kKeyNumpad8);
			GXMAP(type,XK_KP_9,	 		GHOST_kKeyNumpad9);
			GXMAP(type,XK_KP_Decimal,	GHOST_kKeyNumpadPeriod);

			GXMAP(type,XK_KP_Insert, 	GHOST_kKeyNumpad0);
			GXMAP(type,XK_KP_End,	 	GHOST_kKeyNumpad1);
			GXMAP(type,XK_KP_Down,	 	GHOST_kKeyNumpad2);
			GXMAP(type,XK_KP_Page_Down,	GHOST_kKeyNumpad3);
			GXMAP(type,XK_KP_Left,	 	GHOST_kKeyNumpad4);
			GXMAP(type,XK_KP_Begin, 	GHOST_kKeyNumpad5);
			GXMAP(type,XK_KP_Right,		GHOST_kKeyNumpad6);
			GXMAP(type,XK_KP_Home,	 	GHOST_kKeyNumpad7);
			GXMAP(type,XK_KP_Up,	 	GHOST_kKeyNumpad8);
			GXMAP(type,XK_KP_Page_Up,	GHOST_kKeyNumpad9);
			GXMAP(type,XK_KP_Delete,	GHOST_kKeyNumpadPeriod);

			GXMAP(type,XK_KP_Enter,		GHOST_kKeyNumpadEnter);
			GXMAP(type,XK_KP_Add,		GHOST_kKeyNumpadPlus);
			GXMAP(type,XK_KP_Subtract,	GHOST_kKeyNumpadMinus);
			GXMAP(type,XK_KP_Multiply,	GHOST_kKeyNumpadAsterisk);
			GXMAP(type,XK_KP_Divide,	GHOST_kKeyNumpadSlash);

				/* some extra sun cruft (NICE KEYBOARD!) */
#ifdef __sun__
			GXMAP(type,0xffde,			GHOST_kKeyNumpad1);
			GXMAP(type,0xffe0,			GHOST_kKeyNumpad3);
			GXMAP(type,0xffdc,			GHOST_kKeyNumpad5);
			GXMAP(type,0xffd8,			GHOST_kKeyNumpad7);
			GXMAP(type,0xffda,			GHOST_kKeyNumpad9);

			GXMAP(type,0xffd6,			GHOST_kKeyNumpadSlash);
			GXMAP(type,0xffd7,			GHOST_kKeyNumpadAsterisk);
#endif

			default :
				type = GHOST_kKeyUnknown;
				break;
		}
	}

	return type;
}

#undef GXMAP
