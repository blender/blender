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

#include "GHOST_WindowX11.h"
#include "GHOST_SystemX11.h"
#include "STR_String.h"
#include "GHOST_Debug.h"

// For standard X11 cursors
#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#if defined(__sun__) || defined( __sun ) || defined (__sparc) || defined (__sparc__)
#include <strings.h>
#endif


// For obscure full screen mode stuuf
// lifted verbatim from blut.

typedef struct {
	long flags;
	long functions;
	long decorations;
	long input_mode;
} MotifWmHints;

#define MWM_HINTS_DECORATIONS         (1L << 1)

GLXContext GHOST_WindowX11::s_firstContext = NULL;

GHOST_WindowX11::
GHOST_WindowX11(
	GHOST_SystemX11 *system,
	Display * display,
	const STR_String& title, 
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,	
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	const bool stereoVisual
) :
	GHOST_Window(title,left,top,width,height,state,type,stereoVisual),
	m_context(NULL),
	m_display(display),
	m_system (system),
	m_valid_setup (false),
	m_invalid_window(false),
	m_empty_cursor(None),
	m_custom_cursor(None)
{
	
	// Set up the minimum atrributes that we require and see if
	// X can find us a visual matching those requirements.

	int attributes[40], i = 0;

	if(m_stereoVisual)
		attributes[i++] = GLX_STEREO;

	attributes[i++] = GLX_RGBA;
	attributes[i++] = GLX_DOUBLEBUFFER;	
	attributes[i++] = GLX_RED_SIZE;   attributes[i++] = 1;
	attributes[i++] = GLX_BLUE_SIZE;  attributes[i++] = 1;
	attributes[i++] = GLX_GREEN_SIZE; attributes[i++] = 1;
	attributes[i++] = GLX_DEPTH_SIZE; attributes[i++] = 1;
	attributes[i] = None;
	
	m_visual = glXChooseVisual(m_display, DefaultScreen(m_display), attributes);

	if (m_visual == NULL) {
		// barf : no visual meeting these requirements could be found.
		printf("%s:%d: X11 glxChooseVisual() failed for OpenGL, verify working openGL system!\n", __FILE__, __LINE__);
		return;
	}

	// Create a bunch of attributes needed to create an X window.


	// First create a colormap for the window and visual. 
	// This seems pretty much a legacy feature as we are in rgba mode anyway.

	XSetWindowAttributes xattributes;
	memset(&xattributes, 0, sizeof(xattributes));

	xattributes.colormap= XCreateColormap(
		m_display, 
		RootWindow(m_display, m_visual->screen),
		m_visual->visual,
		AllocNone
	);

	xattributes.border_pixel= 0;

	// Specify which events we are interested in hearing.	

	xattributes.event_mask= 
		ExposureMask | StructureNotifyMask | 
		KeyPressMask | KeyReleaseMask |
		EnterWindowMask | LeaveWindowMask |
		ButtonPressMask | ButtonReleaseMask |
		PointerMotionMask | FocusChangeMask;

	// create the window!

	m_window = 
		XCreateWindow(
			m_display, 
			RootWindow(m_display, m_visual->screen), 
			left,
			top,
			width,
			height,
			0, // no border.
			m_visual->depth,
			InputOutput, 
			m_visual->visual,
			CWBorderPixel|CWColormap|CWEventMask, 
			&xattributes
		);

	// Are we in fullscreen mode - then include
	// some obscure blut code to remove decorations.

	if (state == GHOST_kWindowStateFullScreen) {

		MotifWmHints hints;
		Atom atom;
					
		atom = XInternAtom(m_display, "_MOTIF_WM_HINTS", False);
		
		if (atom == None) {
			GHOST_PRINT("Could not intern X atom for _MOTIF_WM_HINTS.\n");
		} else {
			hints.flags = MWM_HINTS_DECORATIONS;
			hints.decorations = 0;  /* Absolutely no decorations. */
			// other hints.decorations make no sense
			// you can't select individual decorations

			XChangeProperty(m_display, m_window,
				atom, atom, 32,
				PropModeReplace, (unsigned char *) &hints, 4);
		}
	} else if (state == GHOST_kWindowStateMaximized) {
		// With this, xprop should report the following just after launch
		// _NET_WM_STATE(ATOM) = _NET_WM_STATE_MAXIMIZED_VERT, _NET_WM_STATE_MAXIMIZED_HORZ
		// After demaximization the right side is empty, though (maybe not the most correct then?)
		Atom state, atomh, atomv;

		state = XInternAtom(m_display, "_NET_WM_STATE", False);
		atomh = XInternAtom(m_display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
		atomv = XInternAtom(m_display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
		if (state == None ) {
			GHOST_PRINT("Atom _NET_WM_STATE requested but not avaliable nor created.\n");
		} else {
			XChangeProperty(m_display, m_window,
				state, XA_ATOM, 32,
				PropModeAppend, (unsigned char *) &atomh, 1);
			XChangeProperty(m_display, m_window,
				state, XA_ATOM, 32,
				PropModeAppend, (unsigned char *) &atomv, 1);
		}
 	}
	
	// Create some hints for the window manager on how
	// we want this window treated.	

	XSizeHints * xsizehints = XAllocSizeHints();
	xsizehints->flags = USPosition | USSize;
	xsizehints->x = left;
	xsizehints->y = top;
	xsizehints->width = width;
	xsizehints->height = height;
	XSetWMNormalHints(m_display, m_window, xsizehints);
	XFree(xsizehints);

	XClassHint * xclasshint = XAllocClassHint();
	int len = title.Length() +1 ;
	char *wmclass = (char *)malloc(sizeof(char) * len);
	strncpy(wmclass, (const char*)title, sizeof(char) * len);
	xclasshint->res_name = wmclass;
	xclasshint->res_class = wmclass;
	XSetClassHint(m_display, m_window, xclasshint);
	free(wmclass);
	XFree(xclasshint);

	setTitle(title);

	initXInputDevices();

	// now set up the rendering context.
	if (installDrawingContext(type) == GHOST_kSuccess) {
		m_valid_setup = true;
		GHOST_PRINT("Created window\n");
	}

	XMapWindow(m_display, m_window);
	GHOST_PRINT("Mapped window\n");

	XFlush(m_display);
}

/* 
	Dummy function to get around IO Handler exiting if device invalid
	Basically it will not crash blender now if you have a X device that 
	is configured but not plugged in.

*/
static int ApplicationErrorHandler(Display *display, XErrorEvent *theEvent) {
	fprintf(stderr, "Ignoring Xlib error: error code %d request code %d\n",
		theEvent->error_code, theEvent->request_code) ;

	/* No exit! - but keep lint happy */
	return 0 ;
}

void GHOST_WindowX11::initXInputDevices()
{
	static XErrorHandler old_handler = (XErrorHandler) 0 ;
	XExtensionVersion *version = XGetExtensionVersion(m_display, INAME);

	if(version && (version != (XExtensionVersion*)NoSuchExtension)) {
		if(version->present) {
			int device_count;
			XDeviceInfo* device_info = XListInputDevices(m_display, &device_count);
			m_xtablet.StylusDevice = 0;
			m_xtablet.EraserDevice = 0;
			m_xtablet.CommonData.Active= 0;

			/* Install our error handler to override Xlib's termination behavior */
			old_handler = XSetErrorHandler(ApplicationErrorHandler) ;

			for(int i=0; i<device_count; ++i) {
				if(!strcasecmp(device_info[i].name, "stylus")) {
					m_xtablet.StylusID= device_info[i].id;
					m_xtablet.StylusDevice = XOpenDevice(m_display, m_xtablet.StylusID);

					if (m_xtablet.StylusDevice != NULL) {
						/* Find how many pressure levels tablet has */
						XAnyClassPtr ici = device_info[i].inputclassinfo;
						for(int j=0; j<m_xtablet.StylusDevice->num_classes; ++j) {
							if(ici->c_class==ValuatorClass) {
								XValuatorInfo* xvi = (XValuatorInfo*)ici;
								m_xtablet.PressureLevels = xvi->axes[2].max_value;
							
								/* this is assuming that the tablet has the same tilt resolution in both
								 * positive and negative directions. It would be rather weird if it didn't.. */
								m_xtablet.XtiltLevels = xvi->axes[3].max_value;
								m_xtablet.YtiltLevels = xvi->axes[4].max_value;
								break;
							}
						
							ici = (XAnyClassPtr)(((char *)ici) + ici->length);
						}
					} else {
 						m_xtablet.StylusID= 0;
					}
				}
				if(!strcasecmp(device_info[i].name, "eraser")) {
					m_xtablet.EraserID= device_info[i].id;
					m_xtablet.EraserDevice = XOpenDevice(m_display, m_xtablet.EraserID);
					if (m_xtablet.EraserDevice == NULL) m_xtablet.EraserID= 0;
				}
			}

			/* Restore handler */
			(void) XSetErrorHandler(old_handler) ;

			XFreeDeviceList(device_info);


			XEventClass xevents[10], ev;
			int dcount = 0;

			if(m_xtablet.StylusDevice) {
				DeviceMotionNotify(m_xtablet.StylusDevice, m_xtablet.MotionEvent, ev);
				if(ev) xevents[dcount++] = ev;
				ProximityIn(m_xtablet.StylusDevice, m_xtablet.ProxInEvent, ev);
				if(ev) xevents[dcount++] = ev;
				ProximityOut(m_xtablet.StylusDevice, m_xtablet.ProxOutEvent, ev);
				if(ev) xevents[dcount++] = ev;
			}
			if(m_xtablet.EraserDevice) {
				DeviceMotionNotify(m_xtablet.EraserDevice, m_xtablet.MotionEvent, ev);
				if(ev) xevents[dcount++] = ev;
				ProximityIn(m_xtablet.EraserDevice, m_xtablet.ProxInEvent, ev);
				if(ev) xevents[dcount++] = ev;
				ProximityOut(m_xtablet.EraserDevice, m_xtablet.ProxOutEvent, ev);
				if(ev) xevents[dcount++] = ev;
			}

			XSelectExtensionEvent(m_display, m_window, xevents, dcount);
		}
		XFree(version);
	}
}	


	Window 
GHOST_WindowX11::
getXWindow(
){
	return m_window;
}	

	bool 
GHOST_WindowX11::
getValid(
) const {
	return m_valid_setup;
}

	void 
GHOST_WindowX11::
setTitle(
	const STR_String& title
){
	XStoreName(m_display,m_window,title);
	XFlush(m_display);
}

	void 
GHOST_WindowX11::
getTitle(
	STR_String& title
) const {
	char *name = NULL;
	
	XFetchName(m_display,m_window,&name);
	title= name?name:"untitled";
	XFree(name);
}
	
	void 
GHOST_WindowX11::
getWindowBounds(
	GHOST_Rect& bounds
) const {
		// Getting the window bounds under X11 is not
		// really supported (nor should it be desired).
	getClientBounds(bounds);
}

	void 
GHOST_WindowX11::
getClientBounds(
	GHOST_Rect& bounds
) const {
	Window root_return;
	int x_return,y_return;
	unsigned int w_return,h_return,border_w_return,depth_return;
	GHOST_TInt32 screen_x, screen_y;
	
	XGetGeometry(m_display,m_window,&root_return,&x_return,&y_return,
		&w_return,&h_return,&border_w_return,&depth_return);

	clientToScreen(0, 0, screen_x, screen_y);
	
	bounds.m_l = screen_x;
	bounds.m_r = bounds.m_l + w_return;
	bounds.m_t = screen_y;
	bounds.m_b = bounds.m_t + h_return;

}

	GHOST_TSuccess 
GHOST_WindowX11::
setClientWidth(
	GHOST_TUns32 width
){	
	XWindowChanges values;
	unsigned int value_mask= CWWidth;		
	values.width = width;
	XConfigureWindow(m_display,m_window,value_mask,&values);

	return GHOST_kSuccess;
}

	GHOST_TSuccess 
GHOST_WindowX11::
setClientHeight(
	GHOST_TUns32 height
){
	XWindowChanges values;
	unsigned int value_mask= CWHeight;		
	values.height = height;
	XConfigureWindow(m_display,m_window,value_mask,&values);
	return GHOST_kSuccess;

}

	GHOST_TSuccess 
GHOST_WindowX11::
setClientSize(
	GHOST_TUns32 width,
	GHOST_TUns32 height
){
	XWindowChanges values;
	unsigned int value_mask= CWWidth | CWHeight;		
	values.width = width;
	values.height = height;
	XConfigureWindow(m_display,m_window,value_mask,&values);
	return GHOST_kSuccess;

}	

	void 
GHOST_WindowX11::
screenToClient(
	GHOST_TInt32 inX,
	GHOST_TInt32 inY,
	GHOST_TInt32& outX,
	GHOST_TInt32& outY
) const {
	// not sure about this one!

	int ax,ay;
	Window temp;

	XTranslateCoordinates(
			m_display,
			RootWindow(m_display, m_visual->screen),
			m_window,
			inX,
			inY,
			&ax,
			&ay,
			&temp
		);
	outX = ax;
	outY = ay;
}
		 
	void 
GHOST_WindowX11::
clientToScreen(
	GHOST_TInt32 inX,
	GHOST_TInt32 inY,
	GHOST_TInt32& outX,
	GHOST_TInt32& outY
) const {
	int ax,ay;
	Window temp;

	XTranslateCoordinates(
			m_display,
			m_window,
			RootWindow(m_display, m_visual->screen),
			inX,
			inY,
			&ax,
			&ay,
			&temp
		);
	outX = ax;
	outY = ay;
}


	GHOST_TWindowState 
GHOST_WindowX11::
getState(
) const {
	//FIXME 
	return GHOST_kWindowStateNormal;
}

	GHOST_TSuccess 
GHOST_WindowX11::
setState(
	GHOST_TWindowState state
){
	//TODO

        if (state == (int)getState()) {
		return GHOST_kSuccess;
	} else {
		return GHOST_kFailure;
	}

}

#include <iostream>
using namespace std;

	GHOST_TSuccess 
GHOST_WindowX11::
setOrder(
	GHOST_TWindowOrder order
){
	if (order == GHOST_kWindowOrderTop) {
		XWindowAttributes attr;	  
		Atom atom;

		/* We use both XRaiseWindow and _NET_ACTIVE_WINDOW, since some
		   window managers ignore the former (e.g. kwin from kde) and others
		   don't implement the latter (e.g. fluxbox pre 0.9.9) */

		XRaiseWindow(m_display, m_window);

		atom = XInternAtom(m_display, "_NET_ACTIVE_WINDOW", True);

		if (atom != None) {
			Window root;
			XEvent xev;
			long eventmask;

			xev.xclient.type = ClientMessage;
			xev.xclient.serial = 0;
			xev.xclient.send_event = True;
			xev.xclient.window = m_window;
			xev.xclient.message_type = atom;

			xev.xclient.format = 32;
			xev.xclient.data.l[0] = 0;
			xev.xclient.data.l[1] = 0;
			xev.xclient.data.l[2] = 0;
			xev.xclient.data.l[3] = 0;
			xev.xclient.data.l[4] = 0;

			root = RootWindow(m_display, m_visual->screen),
			eventmask = SubstructureRedirectMask | SubstructureNotifyMask;

			XSendEvent(m_display, root, False, eventmask, &xev);
		}

		XGetWindowAttributes(m_display, m_window, &attr);

		/* iconized windows give bad match error */
		if (attr.map_state == IsViewable)
			XSetInputFocus(m_display, m_window, RevertToPointerRoot,
						 CurrentTime);
		XFlush(m_display);
	} else if (order == GHOST_kWindowOrderBottom) {
		XLowerWindow(m_display,m_window);
		XFlush(m_display);
	} else {
		return GHOST_kFailure;
	}
	
	return GHOST_kSuccess;
}

	GHOST_TSuccess 
GHOST_WindowX11::
swapBuffers(
){
	if (getDrawingContextType() == GHOST_kDrawingContextTypeOpenGL) {
		glXSwapBuffers(m_display,m_window);
		return GHOST_kSuccess;
	} else {
		return GHOST_kFailure;
	}
}

	GHOST_TSuccess 
GHOST_WindowX11::
activateDrawingContext(
){
	if (m_context !=NULL) {
		glXMakeCurrent(m_display, m_window,m_context);						
		return GHOST_kSuccess;
	} 
	return GHOST_kFailure;
}

 	GHOST_TSuccess 
GHOST_WindowX11::
invalidate(
){
 	
	// So the idea of this function is to generate an expose event
	// for the window.
	// Unfortunately X does not handle expose events for you and 
	// it is the client's job to refresh the dirty part of the window.
	// We need to queue up invalidate calls and generate GHOST events 
	// for them in the system.

	// We implement this by setting a boolean in this class to concatenate 
	// all such calls into a single event for this window.

	// At the same time we queue the dirty windows in the system class
	// and generate events for them at the next processEvents call.

	if (m_invalid_window == false) {
		m_system->addDirtyWindow(this);
		m_invalid_window = true;
	} 
 
	return GHOST_kSuccess;
}

/**
 * called by the X11 system implementation when expose events
 * for the window have been pushed onto the GHOST queue
 */
 
	void
GHOST_WindowX11::
validate(
){
	m_invalid_window = false;
}	
 
 
/**
 * Destructor.
 * Closes the window and disposes resources allocated.
 */

GHOST_WindowX11::
~GHOST_WindowX11(
){
	std::map<unsigned int, Cursor>::iterator it = m_standard_cursors.begin();
	for (; it != m_standard_cursors.end(); it++) {
		XFreeCursor(m_display, it->second);
	}

	if (m_empty_cursor) {
		XFreeCursor(m_display, m_empty_cursor);
	}
	if (m_custom_cursor) {
		XFreeCursor(m_display, m_custom_cursor);
	}
	
	if (m_context) {
		if (m_context == s_firstContext) {
			s_firstContext = NULL;
		}
		glXDestroyContext(m_display, m_context);
	}
	XDestroyWindow(m_display, m_window);
	XFree(m_visual);
}




/**
 * Tries to install a rendering context in this window.
 * @param type	The type of rendering context installed.
 * @return Indication as to whether installation has succeeded.
 */
	GHOST_TSuccess 
GHOST_WindowX11::
installDrawingContext(
	GHOST_TDrawingContextType type
){
	// only support openGL for now.
	GHOST_TSuccess success;
	switch (type) {
	case GHOST_kDrawingContextTypeOpenGL:
		m_context = glXCreateContext(m_display, m_visual, s_firstContext, True);
		if (m_context !=NULL) {
			if (!s_firstContext) {
				s_firstContext = m_context;
			}
			glXMakeCurrent(m_display, m_window,m_context);						
			success = GHOST_kSuccess;
		} else {
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



/**
 * Removes the current drawing context.
 * @return Indication as to whether removal has succeeded.
 */
	GHOST_TSuccess 
GHOST_WindowX11::
removeDrawingContext(
){
	GHOST_TSuccess success;

	if (m_context != NULL) {
		glXDestroyContext(m_display, m_context);
		success = GHOST_kSuccess;
	} else {
		success = GHOST_kFailure;
	}
	return success;	
}


	Cursor
GHOST_WindowX11::
getStandardCursor(
	GHOST_TStandardCursor g_cursor
){
	unsigned int xcursor_id;

#define GtoX(gcurs, xcurs)	case gcurs: xcursor_id = xcurs
	switch (g_cursor) {
	GtoX(GHOST_kStandardCursorRightArrow, XC_arrow); break;
	GtoX(GHOST_kStandardCursorLeftArrow, XC_top_left_arrow); break;
	GtoX(GHOST_kStandardCursorInfo, XC_hand1); break;
	GtoX(GHOST_kStandardCursorDestroy, XC_pirate); break;
	GtoX(GHOST_kStandardCursorHelp, XC_question_arrow); break; 
	GtoX(GHOST_kStandardCursorCycle, XC_exchange); break;
	GtoX(GHOST_kStandardCursorSpray, XC_spraycan); break;
	GtoX(GHOST_kStandardCursorWait, XC_watch); break;
	GtoX(GHOST_kStandardCursorText, XC_xterm); break;
	GtoX(GHOST_kStandardCursorCrosshair, XC_crosshair); break;
	GtoX(GHOST_kStandardCursorUpDown, XC_sb_v_double_arrow); break;
	GtoX(GHOST_kStandardCursorLeftRight, XC_sb_h_double_arrow); break;
	GtoX(GHOST_kStandardCursorTopSide, XC_top_side); break;
	GtoX(GHOST_kStandardCursorBottomSide, XC_bottom_side); break;
	GtoX(GHOST_kStandardCursorLeftSide, XC_left_side); break;
	GtoX(GHOST_kStandardCursorRightSide, XC_right_side); break;
	GtoX(GHOST_kStandardCursorTopLeftCorner, XC_top_left_corner); break;
	GtoX(GHOST_kStandardCursorTopRightCorner, XC_top_right_corner); break;
	GtoX(GHOST_kStandardCursorBottomRightCorner, XC_bottom_right_corner); break;
	GtoX(GHOST_kStandardCursorBottomLeftCorner, XC_bottom_left_corner); break;
	GtoX(GHOST_kStandardCursorPencil, XC_pencil); break;
	default:
		xcursor_id = 0;
	}
#undef GtoX

	if (xcursor_id) {
		Cursor xcursor = m_standard_cursors[xcursor_id];
		
		if (!xcursor) {
			xcursor = XCreateFontCursor(m_display, xcursor_id);

			m_standard_cursors[xcursor_id] = xcursor;
		}
		
		return xcursor;
	} else {
		return None;
	}
}

	Cursor 
GHOST_WindowX11::
getEmptyCursor(
) {
	if (!m_empty_cursor) {
		Pixmap blank;
		XColor dummy;
		char data[1] = {0};
			
		/* make a blank cursor */
		blank = XCreateBitmapFromData (
			m_display, 
			RootWindow(m_display,DefaultScreen(m_display)),
			data, 1, 1
		);

		m_empty_cursor = XCreatePixmapCursor(m_display, blank, blank, &dummy, &dummy, 0, 0);
		XFreePixmap(m_display, blank);
	}

	return m_empty_cursor;
}

	GHOST_TSuccess
GHOST_WindowX11::
setWindowCursorVisibility(
	bool visible
){
	Cursor xcursor;
	
	if (visible) {
		xcursor = getStandardCursor( getCursorShape() );
	} else {
		xcursor = getEmptyCursor();
	}

	XDefineCursor(m_display, m_window, xcursor);
	XFlush(m_display);
	
	return GHOST_kSuccess;
}

	GHOST_TSuccess
GHOST_WindowX11::
setWindowCursorShape(
	GHOST_TStandardCursor shape
){
	Cursor xcursor = getStandardCursor( shape );
	
	XDefineCursor(m_display, m_window, xcursor);
	XFlush(m_display);

	return GHOST_kSuccess;
}

	GHOST_TSuccess
GHOST_WindowX11::
setWindowCustomCursorShape(
	GHOST_TUns8 bitmap[16][2], 
	GHOST_TUns8 mask[16][2], 
	int hotX, 
	int hotY
){

setWindowCustomCursorShape((GHOST_TUns8*)bitmap, (GHOST_TUns8*)mask, 
									16, 16, hotX, hotY, 0, 1);
	return GHOST_kSuccess;
}

	GHOST_TSuccess
GHOST_WindowX11::
setWindowCustomCursorShape(	
	GHOST_TUns8 *bitmap, 
	GHOST_TUns8 *mask, 
	int sizex, 
	int sizey, 
	int hotX, 
	int hotY, 
	int fg_color, 
	int bg_color
){
	Pixmap bitmap_pix, mask_pix;
	XColor fg, bg;
	
	if(XAllocNamedColor(m_display, DefaultColormap(m_display, DefaultScreen(m_display)),
		"White", &fg, &fg) == 0) return GHOST_kFailure;
	if(XAllocNamedColor(m_display, DefaultColormap(m_display, DefaultScreen(m_display)),
		"Black", &bg, &bg) == 0) return GHOST_kFailure;

	if (m_custom_cursor) {
		XFreeCursor(m_display, m_custom_cursor);
	}

	bitmap_pix = XCreateBitmapFromData(m_display, m_window, (char*) bitmap, sizex, sizey);
	mask_pix = XCreateBitmapFromData(m_display, m_window, (char*) mask, sizex, sizey);
		
	m_custom_cursor = XCreatePixmapCursor(m_display, bitmap_pix, mask_pix, &fg, &bg, hotX, hotY);
	XDefineCursor(m_display, m_window, m_custom_cursor);
	XFlush(m_display);
	
	XFreePixmap(m_display, bitmap_pix);
	XFreePixmap(m_display, mask_pix);

	return GHOST_kSuccess;
}

/*

void glutCustomCursor(char *data1, char *data2, int size)
{
	Pixmap source, mask;
	Cursor cursor;
	XColor fg, bg;
	
	if(XAllocNamedColor(__glutDisplay, DefaultColormap(__glutDisplay, __glutScreen),
		"White", &fg, &fg) == 0) return;
	if(XAllocNamedColor(__glutDisplay, DefaultColormap(__glutDisplay, __glutScreen),
		"Red", &bg, &bg) == 0) return;


	source= XCreateBitmapFromData(__glutDisplay, xdraw, data2, size, size);
	mask= XCreateBitmapFromData(__glutDisplay, xdraw, data1, size, size);
		
	cursor= XCreatePixmapCursor(__glutDisplay, source, mask, &fg, &bg, 7, 7);
		
	XFreePixmap(__glutDisplay, source);
	XFreePixmap(__glutDisplay, mask);
		
	XDefineCursor(__glutDisplay, xdraw, cursor);
}

*/
