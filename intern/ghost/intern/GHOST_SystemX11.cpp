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
#include "GHOST_EventNDOF.h"
#include "GHOST_NDOFManager.h"
#include "GHOST_DisplayManagerX11.h"

#include "GHOST_Debug.h"

#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h> /* allow detectable autorepeate */

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

#include <iostream>
#include <vector>
#include <stdio.h> // for fprintf only
#include <cstdlib> // for exit

typedef struct NDOFPlatformInfo {
	Display *display;
	Window window;
	volatile GHOST_TEventNDOFData *currValues;
	Atom cmdAtom;
	Atom motionAtom;
	Atom btnPressAtom;
	Atom btnRelAtom;
} NDOFPlatformInfo;

static NDOFPlatformInfo sNdofInfo = {NULL, 0, NULL, 0, 0, 0, 0};


//these are for copy and select copy
static char *txt_cut_buffer= NULL;
static char *txt_select_buffer= NULL;

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
		abort(); //was return before, but this would just mean it will crash later
	}
	
#ifdef __sgi
	m_delete_window_atom 
	  = XSGIFastInternAtom(m_display,
			       "WM_DELETE_WINDOW", 
			       SGI_XA_WM_DELETE_WINDOW, False);
#else
	m_delete_window_atom 
	  = XInternAtom(m_display, "WM_DELETE_WINDOW", True);
#endif

	m_wm_protocols= XInternAtom(m_display, "WM_PROTOCOLS", False);
	m_wm_take_focus= XInternAtom(m_display, "WM_TAKE_FOCUS", False);
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
	m_targets= XInternAtom(m_display, "TARGETS", False);
	m_string= XInternAtom(m_display, "STRING", False);
	m_compound_text= XInternAtom(m_display, "COMPOUND_TEXT", False);
	m_text= XInternAtom(m_display, "TEXT", False);
	m_clipboard= XInternAtom(m_display, "CLIPBOARD", False);
	m_primary= XInternAtom(m_display, "PRIMARY", False);
	m_xclip_out= XInternAtom(m_display, "XCLIP_OUT", False);
	m_incr= XInternAtom(m_display, "INCR", False);
	m_utf8_string= XInternAtom(m_display, "UTF8_STRING", False);
	m_last_warp = 0;


	// compute the initial time
	timeval tv;
	if (gettimeofday(&tv,NULL) == -1) {
		GHOST_ASSERT(false,"Could not instantiate timer!");
	}

	m_start_time = GHOST_TUns64(tv.tv_sec*1000 + tv.tv_usec/1000);
	
	
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
	XCloseDisplay(m_display);
}


	GHOST_TSuccess 
GHOST_SystemX11::
init(
){
	GHOST_TSuccess success = GHOST_System::init();

	if (success) {
		m_displayManager = new GHOST_DisplayManagerX11(this);

		if (m_displayManager) {
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
	 * @param	stereoVisual	Stereo visual for quad buffered stereo.
	 * @param	numOfAASamples	Number of samples used for AA (zero if no AA)
	 * @param	parentWindow 	Parent (embedder) window
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
	bool stereoVisual,
	const GHOST_TUns16 numOfAASamples,
	const GHOST_TEmbedderWindowID parentWindow
){
	GHOST_WindowX11 * window = 0;
	
	if (!m_display) return 0;
	

	

	window = new GHOST_WindowX11 (
		this,m_display,title, left, top, width, height, state, parentWindow, type, stereoVisual
	);

	if (window) {
		// Both are now handle in GHOST_WindowX11.cpp
		// Focus and Delete atoms.

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

/* This function borrowed from Qt's X11 support
 * qclipboard_x11.cpp
 *  */
struct init_timestamp_data
{
    Time timestamp;
};

static Bool init_timestamp_scanner(Display*, XEvent *event, XPointer arg)
{
	init_timestamp_data *data =
        reinterpret_cast<init_timestamp_data*>(arg);
    switch(event->type)
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
    XCheckIfEvent(m_display, &ev, &init_timestamp_scanner, (XPointer)&data);

    return data.timestamp;
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
				GHOST_TInt64 maxSleep = next - getMilliSeconds();

				if(maxSleep >= 0)
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
			
			if(window->getCursorGrabMode() != GHOST_kGrabDisable && window->getCursorGrabMode() != GHOST_kGrabNormal)
			{
				GHOST_TInt32 x_new= xme.x_root;
				GHOST_TInt32 y_new= xme.y_root;
				GHOST_TInt32 x_accum, y_accum;
				GHOST_Rect bounds;

				/* fallback to window bounds */
				if(window->getCursorGrabBounds(bounds)==GHOST_kFailure)
					window->getClientBounds(bounds);

				/* could also clamp to screen bounds
				 * wrap with a window outside the view will fail atm  */
				bounds.wrapPoint(x_new, y_new, 2); /* offset of one incase blender is at screen bounds */
				window->getCursorGrabAccum(x_accum, y_accum);

				if(x_new != xme.x_root || y_new != xme.y_root) {
					if (xme.time > m_last_warp) {
						/* when wrapping we don't need to add an event because the
						 * setCursorPosition call will cause a new event after */
						setCursorPosition(x_new, y_new); /* wrap */
						window->setCursorGrabAccum(x_accum + (xme.x_root - x_new), y_accum + (xme.y_root - y_new));
						m_last_warp = lastEventTime(xme.time);
					} else {
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
				/* It seems events 6 and 7 are for horizontal scrolling.
				 * you can re-order button mapping like this... (swaps 6,7 with 8,9)
				 *   xmodmap -e "pointer = 1 2 3 4 5 8 9 6 7" 
				 */
				case 8 : gbmask = GHOST_kButtonMaskButton4; break; /* Button4 is the wheel */
				case 9 : gbmask = GHOST_kButtonMaskButton5; break; /* Button5 is a wheel too */
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
			if (((Atom)xcme.data.l[0]) == m_delete_window_atom) {
				g_event = new 
				GHOST_Event(	
					getMilliSeconds(),
					GHOST_kEventWindowClose,
					window
				);
			} else 
#endif
			if (sNdofInfo.currValues) {
				static GHOST_TEventNDOFData data = {0,0,0,0,0,0,0,0,0,0,0};
				if (xcme.message_type == sNdofInfo.motionAtom)
				{
					data.changed = 1;
					data.delta = xcme.data.s[8] - data.time;
					data.time = xcme.data.s[8];
					data.tx = xcme.data.s[2] >> 2;
					data.ty = xcme.data.s[3] >> 2;
					data.tz = xcme.data.s[4] >> 2;
					data.rx = xcme.data.s[5];
					data.ry = xcme.data.s[6];
					data.rz =-xcme.data.s[7];
					g_event = new GHOST_EventNDOF(getMilliSeconds(),
					                              GHOST_kEventNDOFMotion,
					                              window, data);
				} else if (xcme.message_type == sNdofInfo.btnPressAtom) {
					data.changed = 2;
					data.delta = xcme.data.s[8] - data.time;
					data.time = xcme.data.s[8];
					data.buttons = xcme.data.s[2];
					g_event = new GHOST_EventNDOF(getMilliSeconds(),
					                              GHOST_kEventNDOFButton,
					                              window, data);
				}
			} else if (((Atom)xcme.data.l[0]) == m_wm_take_focus) {
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
			} else {
				/* Unknown client message, ignore */
			}
			break;
		}
		
		case DestroyNotify:
			::exit(-1);	
		// We're not interested in the following things.(yet...)
		case NoExpose : 
		case GraphicsExpose :
			break;
		
		case EnterNotify:
		case LeaveNotify:
		{
			// XCrossingEvents pointer leave enter window.
			// also do cursor move here, MotionNotify only
			// happens when motion starts & ends inside window
			XCrossingEvent &xce = xe->xcrossing;
			
			g_event = new 
			GHOST_EventCursor(
				getMilliSeconds(),
				GHOST_kEventCursorMove,
				window,
				xce.x_root,
				xce.y_root
			);
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
				window->setState (window->m_post_state);
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
			Atom target, string, compound_text, c_string;
			XSelectionRequestEvent *xse = &xe->xselectionrequest;
			
			target = XInternAtom(m_display, "TARGETS", False);
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
			
			/*Check to see if the requestor is asking for String*/
			if(xse->target == string || xse->target == compound_text || xse->target == c_string) {
				if (xse->selection == XInternAtom(m_display, "PRIMARY", False)) {
					XChangeProperty(m_display, xse->requestor, xse->property, xse->target, 8, PropModeReplace, (unsigned char*)txt_select_buffer, strlen(txt_select_buffer));
				} else if (xse->selection == XInternAtom(m_display, "CLIPBOARD", False)) {
					XChangeProperty(m_display, xse->requestor, xse->property, xse->target, 8, PropModeReplace, (unsigned char*)txt_cut_buffer, strlen(txt_cut_buffer));
				}
			} else if (xse->target == target) {
				Atom alist[4];
				alist[0] = target;
				alist[1] = string;
				alist[2] = compound_text;
				alist[3] = c_string;
				XChangeProperty(m_display, xse->requestor, xse->property, xse->target, 32, PropModeReplace, (unsigned char*)alist, 4);
				XFlush(m_display);
			} else  {
				//Change property to None because we do not support anything but STRING
				nxe.xselection.property = None;
			}
			
			//Send the event to the client 0 0 == False, SelectionNotify
			XSendEvent(m_display, xse->requestor, 0, 0, &nxe);
			XFlush(m_display);
			break;
		}
		
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
					window->GetXTablet().CommonData.Active= GHOST_kTabletModeStylus;
				else if(data->deviceid == window->GetXTablet().EraserID)
					window->GetXTablet().CommonData.Active= GHOST_kTabletModeEraser;
			}
			else if(xe->type == window->GetXTablet().ProxOutEvent)
				window->GetXTablet().CommonData.Active= GHOST_kTabletModeNone;

			break;
		}
	}

	if (g_event) {
		pushEvent(g_event);
	}
}

	void *
GHOST_SystemX11::
prepareNdofInfo(volatile GHOST_TEventNDOFData *currentNdofValues)
{
	const vector<GHOST_IWindow*>& v(m_windowManager->getWindows());
	if (v.size() > 0)
		sNdofInfo.window = static_cast<GHOST_WindowX11*>(v[0])->getXWindow();
	sNdofInfo.display = m_display;
	sNdofInfo.currValues = currentNdofValues;
	return (void*)&sNdofInfo;
}

	GHOST_TSuccess 
GHOST_SystemX11::
getModifierKeys(
	GHOST_ModifierKeys& keys
) const {

	// analyse the masks retuned from XQueryPointer.

	memset((void *)m_keyboard_vector,0,sizeof(m_keyboard_vector));

	XQueryKeymap(m_display,(char *)m_keyboard_vector);

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
	XSync(m_display, 0); /* Sync to process all requests */
	
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

/* from xclip.c xcout() v0.11 */

#define XCLIB_XCOUT_NONE		0 /* no context */
#define XCLIB_XCOUT_SENTCONVSEL		1 /* sent a request */
#define XCLIB_XCOUT_INCR		2 /* in an incr loop */
#define XCLIB_XCOUT_FALLBACK		3 /* STRING failed, need fallback to UTF8 */
#define XCLIB_XCOUT_FALLBACK_UTF8	4 /* UTF8 failed, move to compouned */
#define XCLIB_XCOUT_FALLBACK_COMP	5 /* compouned failed, move to text. */
#define XCLIB_XCOUT_FALLBACK_TEXT	6

// Retrieves the contents of a selections.
void GHOST_SystemX11::getClipboard_xcout(XEvent evt,
	Atom sel, Atom target, unsigned char **txt,
	unsigned long *len, unsigned int *context) const
{
	Atom pty_type;
	int pty_format;
	unsigned char *buffer;
	unsigned long pty_size, pty_items;
	unsigned char *ltxt= *txt;

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();
	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	GHOST_WindowX11 * window = static_cast<GHOST_WindowX11 *>(*win_it);
	Window win = window->getXWindow();

	switch (*context) {
		// There is no context, do an XConvertSelection()
		case XCLIB_XCOUT_NONE:
			// Initialise return length to 0
			if (*len > 0) {
				free(*txt);
				*len = 0;
			}

			// Send a selection request
			XConvertSelection(m_display, sel, target, m_xclip_out, win, CurrentTime);
			*context = XCLIB_XCOUT_SENTCONVSEL;
			return;

		case XCLIB_XCOUT_SENTCONVSEL:
			if (evt.type != SelectionNotify)
				return;

			if (target == m_utf8_string && evt.xselection.property == None) {
				*context= XCLIB_XCOUT_FALLBACK_UTF8;
				return;
			}
			else if (target == m_compound_text && evt.xselection.property == None) {
				*context= XCLIB_XCOUT_FALLBACK_COMP;
				return;
			}
			else if (target == m_text && evt.xselection.property == None) {
				*context= XCLIB_XCOUT_FALLBACK_TEXT;
				return;
			}

			// find the size and format of the data in property
			XGetWindowProperty(m_display, win, m_xclip_out, 0, 0, False,
				AnyPropertyType, &pty_type, &pty_format,
				&pty_items, &pty_size, &buffer);
			XFree(buffer);

			if (pty_type == m_incr) {
				// start INCR mechanism by deleting property
				XDeleteProperty(m_display, win, m_xclip_out);
				XFlush(m_display);
				*context = XCLIB_XCOUT_INCR;
				return;
			}

			// if it's not incr, and not format == 8, then there's
			// nothing in the selection (that xclip understands, anyway)

			if (pty_format != 8) {
				*context = XCLIB_XCOUT_NONE;
				return;
			}

			// not using INCR mechanism, just read the property
			XGetWindowProperty(m_display, win, m_xclip_out, 0, (long) pty_size,
					False, AnyPropertyType, &pty_type,
					&pty_format, &pty_items, &pty_size, &buffer);

			// finished with property, delete it
			XDeleteProperty(m_display, win, m_xclip_out);

			// copy the buffer to the pointer for returned data
			ltxt = (unsigned char *) malloc(pty_items);
			memcpy(ltxt, buffer, pty_items);

			// set the length of the returned data
			*len = pty_items;
			*txt = ltxt;

			// free the buffer
			XFree(buffer);

			*context = XCLIB_XCOUT_NONE;

			// complete contents of selection fetched, return 1
			return;

		case XCLIB_XCOUT_INCR:
			// To use the INCR method, we basically delete the
			// property with the selection in it, wait for an
			// event indicating that the property has been created,
			// then read it, delete it, etc.

			// make sure that the event is relevant
			if (evt.type != PropertyNotify)
				return;

			// skip unless the property has a new value
			if (evt.xproperty.state != PropertyNewValue)
				return;

			// check size and format of the property
			XGetWindowProperty(m_display, win, m_xclip_out, 0, 0, False,
				AnyPropertyType, &pty_type, &pty_format,
				&pty_items, &pty_size, (unsigned char **) &buffer);

			if (pty_format != 8) {
				// property does not contain text, delete it
				// to tell the other X client that we have read	
				// it and to send the next property
				XFree(buffer);
				XDeleteProperty(m_display, win, m_xclip_out);
				return;
			}

			if (pty_size == 0) {
				// no more data, exit from loop
				XFree(buffer);
				XDeleteProperty(m_display, win, m_xclip_out);
				*context = XCLIB_XCOUT_NONE;

				// this means that an INCR transfer is now
				// complete, return 1
				return;
			}

			XFree(buffer);

			// if we have come this far, the propery contains
			// text, we know the size.
			XGetWindowProperty(m_display, win, m_xclip_out, 0, (long) pty_size,
				False, AnyPropertyType, &pty_type, &pty_format,
				&pty_items, &pty_size, (unsigned char **) &buffer);

			// allocate memory to accommodate data in *txt
			if (*len == 0) {
				*len = pty_items;
				ltxt = (unsigned char *) malloc(*len);
			}
			else {
				*len += pty_items;
				ltxt = (unsigned char *) realloc(ltxt, *len);
			}

			// add data to ltxt
			memcpy(&ltxt[*len - pty_items], buffer, pty_items);

			*txt = ltxt;
			XFree(buffer);

			// delete property to get the next item
			XDeleteProperty(m_display, win, m_xclip_out);
			XFlush(m_display);
			return;
	}
	return;
}

GHOST_TUns8 *GHOST_SystemX11::getClipboard(bool selection) const
{
	Atom sseln;
	Atom target= m_string;
	Window owner;

	// from xclip.c doOut() v0.11
	unsigned char *sel_buf;
	unsigned long sel_len= 0;
	XEvent evt;
	unsigned int context= XCLIB_XCOUT_NONE;

	if (selection == True)
		sseln= m_primary;
	else
		sseln= m_clipboard;

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();
	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	GHOST_WindowX11 * window = static_cast<GHOST_WindowX11 *>(*win_it);
	Window win = window->getXWindow();

	/* check if we are the owner. */
	owner= XGetSelectionOwner(m_display, sseln);
	if (owner == win) {
		if (sseln == m_clipboard) {
			sel_buf= (unsigned char *)malloc(strlen(txt_cut_buffer)+1);
			strcpy((char *)sel_buf, txt_cut_buffer);
			return((GHOST_TUns8*)sel_buf);
		}
		else {
			sel_buf= (unsigned char *)malloc(strlen(txt_select_buffer)+1);
			strcpy((char *)sel_buf, txt_select_buffer);
			return((GHOST_TUns8*)sel_buf);
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
			context= XCLIB_XCOUT_NONE;
			target= m_string;
			continue;
		}
		else if (context == XCLIB_XCOUT_FALLBACK_UTF8) {
			/* utf8 fail, move to compouned text. */
			context= XCLIB_XCOUT_NONE;
			target= m_compound_text;
			continue;
		}
		else if (context == XCLIB_XCOUT_FALLBACK_COMP) {
			/* compouned text faile, move to text. */
			context= XCLIB_XCOUT_NONE;
			target= m_text;
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
		unsigned char *tmp_data = (unsigned char*) malloc(sel_len+1);
		memcpy((char*)tmp_data, (char*)sel_buf, sel_len);
		tmp_data[sel_len] = '\0';
		
		if (sseln == m_string)
			XFree(sel_buf);
		else
			free(sel_buf);
		
		return (GHOST_TUns8*)tmp_data;
	}
	return(NULL);
}

void GHOST_SystemX11::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{
	Window m_window, owner;

	vector<GHOST_IWindow *> & win_vec = m_windowManager->getWindows();	
	vector<GHOST_IWindow *>::iterator win_it = win_vec.begin();
	GHOST_WindowX11 * window = static_cast<GHOST_WindowX11 *>(*win_it);
	m_window = window->getXWindow();

	if (buffer) {
		if (selection == False) {
			XSetSelectionOwner(m_display, m_clipboard, m_window, CurrentTime);
			owner= XGetSelectionOwner(m_display, m_clipboard);
			if (txt_cut_buffer)
				free((void*)txt_cut_buffer);

			txt_cut_buffer = (char*) malloc(strlen(buffer)+1);
			strcpy(txt_cut_buffer, buffer);
		} else {
			XSetSelectionOwner(m_display, m_primary, m_window, CurrentTime);
			owner= XGetSelectionOwner(m_display, m_primary);
			if (txt_select_buffer)
				free((void*)txt_select_buffer);

			txt_select_buffer = (char*) malloc(strlen(buffer)+1);
			strcpy(txt_select_buffer, buffer);
		}
	
		if (owner != m_window)
			fprintf(stderr, "failed to own primary\n");
	}
}

GHOST_TUns8* GHOST_SystemX11::getSystemDir() const
{

}

GHOST_TUns8* GHOST_SystemX11::getUserDir() const
{
	char* path;
	char* env = getenv("HOME");
	if(env) {
		path = (char*) malloc(strlen(env) + 10); // "/.blender/"
		strcat(path, env);
		strcat(path, "/,blender/");
		return (GHOST_TUns8*) path;
	} else {
		return NULL;
	}
}
