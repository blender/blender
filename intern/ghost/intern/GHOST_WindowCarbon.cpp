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

/** \file ghost/intern/GHOST_WindowCarbon.cpp
 *  \ingroup GHOST
 */


/**

 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	May 10, 2001
 */

#include "GHOST_WindowCarbon.h"
#include "GHOST_Debug.h"

AGLContext GHOST_WindowCarbon::s_firstaglCtx = NULL;
#ifdef GHOST_DRAW_CARBON_GUTTER
const GHOST_TInt32 GHOST_WindowCarbon::s_sizeRectSize = 16;
#endif //GHOST_DRAW_CARBON_GUTTER

static const GLint sPreferredFormatWindow[10] = {
	AGL_RGBA,
	AGL_DOUBLEBUFFER,
	AGL_ACCELERATED,
	AGL_DEPTH_SIZE, 32,
	AGL_NONE,
};

static const GLint sPreferredFormatFullScreen[11] = {
	AGL_RGBA,
	AGL_DOUBLEBUFFER,
	AGL_ACCELERATED,
	AGL_FULLSCREEN,
	AGL_DEPTH_SIZE, 32,
	AGL_NONE,
};



WindowRef ugly_hack = NULL;

const EventTypeSpec kWEvents[] = {
	{ kEventClassWindow, kEventWindowZoom },  /* for new zoom behaviour */ 
};

static OSStatus myWEventHandlerProc(EventHandlerCallRef handler, EventRef event, void *userData)
{
	WindowRef mywindow;
	GHOST_WindowCarbon *ghost_window;
	OSStatus err;
	int theState;
	
	if (::GetEventKind(event) == kEventWindowZoom) {
		err =  ::GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(mywindow), NULL, &mywindow);
		ghost_window = (GHOST_WindowCarbon *) GetWRefCon(mywindow);
		theState = ghost_window->getMac_windowState();
		if (theState == 1) 
			ghost_window->setMac_windowState(2);
		else if (theState == 2)
			ghost_window->setMac_windowState(1);

	}
	return eventNotHandledErr;
}

GHOST_WindowCarbon::GHOST_WindowCarbon(
    const STR_String& title,
    GHOST_TInt32 left,
    GHOST_TInt32 top,
    GHOST_TUns32 width,
    GHOST_TUns32 height,
    GHOST_TWindowState state,
    GHOST_TDrawingContextType type,
    const bool stereoVisual,
    const GHOST_TUns16 numOfAASamples
    ) :
	GHOST_Window(width, height, state, GHOST_kDrawingContextTypeNone),
	m_windowRef(0),
	m_grafPtr(0),
	m_aglCtx(0),
	m_customCursor(0),
	m_fullScreenDirty(false)
{
	Str255 title255;
	OSStatus err;
	
	//fprintf(stderr," main screen top %i left %i height %i width %i\n", top, left, height, width);
	
	if (state >= GHOST_kWindowState8Normal) {
		if      (state == GHOST_kWindowState8Normal) state = GHOST_kWindowStateNormal;
		else if (state == GHOST_kWindowState8Maximized) state = GHOST_kWindowStateMaximized;
		else if (state == GHOST_kWindowState8Minimized) state = GHOST_kWindowStateMinimized;
		else if (state == GHOST_kWindowState8FullScreen) state = GHOST_kWindowStateFullScreen;
		
		// state = state - 8;	this was the simple version of above code, doesnt work in gcc 4.0
		
		setMac_windowState(1);
	}
	else
		setMac_windowState(0);

	if (state != GHOST_kWindowStateFullScreen) {
		Rect bnds = { top, left, top + height, left + width };
		// Boolean visible = (state == GHOST_kWindowStateNormal) || (state == GHOST_kWindowStateMaximized); /*unused*/
		gen2mac(title, title255);

		err =  ::CreateNewWindow(kDocumentWindowClass,
		                         kWindowStandardDocumentAttributes + kWindowLiveResizeAttribute,
		                         &bnds,
		                         &m_windowRef);
			
		if (err != noErr) {
			fprintf(stderr, " error creating window %i \n", (int)err);
		}
		else {

			::SetWRefCon(m_windowRef, (SInt32) this);
			setTitle(title);
			err = InstallWindowEventHandler(m_windowRef, myWEventHandlerProc, GetEventTypeCount(kWEvents), kWEvents, NULL, NULL);
			if (err != noErr) {
				fprintf(stderr, " error creating handler %i \n", (int)err);
			}
			else {
				//	::TransitionWindow (m_windowRef,kWindowZoomTransitionEffect,kWindowShowTransitionAction,NULL);
				::ShowWindow(m_windowRef);
				::MoveWindow(m_windowRef, left, top, true);
				
			}
		}
		if (m_windowRef) {
			m_grafPtr = ::GetWindowPort(m_windowRef);
			setDrawingContextType(type);
			updateDrawingContext();
			activateDrawingContext();
		}
		if (ugly_hack == NULL) {
			ugly_hack = m_windowRef;
			// when started from commandline, window remains in the back... also for play anim
			ProcessSerialNumber psn;
			GetCurrentProcess(&psn);
			SetFrontProcess(&psn);
		}
	}
	else {
#if 0
		Rect bnds = { top, left, top + height, left + width };
		gen2mac("", title255);
		m_windowRef = ::NewCWindow(
		    nil,                                // Storage
		    &bnds,                              // Bounding rectangle of the window
		    title255,                           // Title of the window
		    0,                                  // Window initially visible
		    plainDBox,                          // procID
		    (WindowRef) - 1L,                   // Put window before all other windows
		    0,                                  // Window has minimize box
		    (SInt32) this);                     // Store a pointer to the class in the refCon
#endif
		//GHOST_PRINT("GHOST_WindowCarbon::GHOST_WindowCarbon(): creating full-screen OpenGL context\n");
		setDrawingContextType(GHOST_kDrawingContextTypeOpenGL);; installDrawingContext(GHOST_kDrawingContextTypeOpenGL);
		updateDrawingContext();
		activateDrawingContext();

		m_tablet.Active = GHOST_kTabletModeNone;
	}
}


GHOST_WindowCarbon::~GHOST_WindowCarbon()
{
	if (m_customCursor) delete m_customCursor;

	if (ugly_hack == m_windowRef) ugly_hack = NULL;
	
	// printf("GHOST_WindowCarbon::~GHOST_WindowCarbon(): removing drawing context\n");
	if (ugly_hack == NULL) setDrawingContextType(GHOST_kDrawingContextTypeNone);
	if (m_windowRef) {
		::DisposeWindow(m_windowRef);
		m_windowRef = 0;
	}
}

bool GHOST_WindowCarbon::getValid() const
{
	bool valid;
	if (!m_fullScreen) {
		valid = (m_windowRef != 0) && (m_grafPtr != 0) && ::IsValidWindowPtr(m_windowRef);
	}
	else {
		valid = true;
	}
	return valid;
}


void GHOST_WindowCarbon::setTitle(const STR_String& title)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::setTitle(): window invalid")
	Str255 title255;
	gen2mac(title, title255);
	::SetWTitle(m_windowRef, title255);
}


void GHOST_WindowCarbon::getTitle(STR_String& title) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::getTitle(): window invalid")
	Str255 title255;
	::GetWTitle(m_windowRef, title255);
	mac2gen(title255, title);
}


void GHOST_WindowCarbon::getWindowBounds(GHOST_Rect& bounds) const
{
	OSStatus success;
	Rect rect;
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::getWindowBounds(): window invalid")
	success = ::GetWindowBounds(m_windowRef, kWindowStructureRgn, &rect);
	bounds.m_b = rect.bottom;
	bounds.m_l = rect.left;
	bounds.m_r = rect.right;
	bounds.m_t = rect.top;
}


void GHOST_WindowCarbon::getClientBounds(GHOST_Rect& bounds) const
{
	Rect rect;
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::getClientBounds(): window invalid")
	//::GetPortBounds(m_grafPtr, &rect);
	::GetWindowBounds(m_windowRef, kWindowContentRgn, &rect);

	bounds.m_b = rect.bottom;
	bounds.m_l = rect.left;
	bounds.m_r = rect.right;
	bounds.m_t = rect.top;

	// Subtract gutter height from bottom
#ifdef GHOST_DRAW_CARBON_GUTTER
	if ((bounds.m_b - bounds.m_t) > s_sizeRectSize)
	{
		bounds.m_b -= s_sizeRectSize;
	}
	else {
		bounds.m_t = bounds.m_b;
	}
#endif //GHOST_DRAW_CARBON_GUTTER
}


GHOST_TSuccess GHOST_WindowCarbon::setClientWidth(GHOST_TUns32 width)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::setClientWidth(): window invalid")
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (((GHOST_TUns32)cBnds.getWidth()) != width) {
		::SizeWindow(m_windowRef, width, cBnds.getHeight(), true);
	}
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCarbon::setClientHeight(GHOST_TUns32 height)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::setClientHeight(): window invalid")
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
#ifdef GHOST_DRAW_CARBON_GUTTER
	if (((GHOST_TUns32)cBnds.getHeight()) != height + s_sizeRectSize) {
		::SizeWindow(m_windowRef, cBnds.getWidth(), height + s_sizeRectSize, true);
	}
#else //GHOST_DRAW_CARBON_GUTTER
	if (((GHOST_TUns32)cBnds.getHeight()) != height) {
		::SizeWindow(m_windowRef, cBnds.getWidth(), height, true);
	}
#endif //GHOST_DRAW_CARBON_GUTTER
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCarbon::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::setClientSize(): window invalid")
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
#ifdef GHOST_DRAW_CARBON_GUTTER
	if ((((GHOST_TUns32)cBnds.getWidth()) != width) ||
	    (((GHOST_TUns32)cBnds.getHeight()) != height + s_sizeRectSize))
	{
		::SizeWindow(m_windowRef, width, height + s_sizeRectSize, true);
	}
#else //GHOST_DRAW_CARBON_GUTTER
	if ((((GHOST_TUns32)cBnds.getWidth()) != width) ||
	    (((GHOST_TUns32)cBnds.getHeight()) != height)) {
		::SizeWindow(m_windowRef, width, height, true);
	}
#endif //GHOST_DRAW_CARBON_GUTTER
	return GHOST_kSuccess;
}


GHOST_TWindowState GHOST_WindowCarbon::getState() const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::getState(): window invalid")
	GHOST_TWindowState state;
	if (::IsWindowVisible(m_windowRef) == false) {
		state = GHOST_kWindowStateMinimized;
	}
	else if (::IsWindowInStandardState(m_windowRef, nil, nil)) {
		state = GHOST_kWindowStateMaximized;
	}
	else {
		state = GHOST_kWindowStateNormal;
	}
	return state;
}


void GHOST_WindowCarbon::screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::screenToClient(): window invalid")
	Point point;
	point.h = inX;
	point.v = inY;
	GrafPtr oldPort;
	::GetPort(&oldPort);
	::SetPort(m_grafPtr);
	::GlobalToLocal(&point);
	::SetPort(oldPort);
	outX = point.h;
	outY = point.v;
}


void GHOST_WindowCarbon::clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::clientToScreen(): window invalid")
	Point point;
	point.h = inX;
	point.v = inY;
	GrafPtr oldPort;
	::GetPort(&oldPort);
	::SetPort(m_grafPtr);
	::LocalToGlobal(&point);
	::SetPort(oldPort);
	outX = point.h;
	outY = point.v;
}


GHOST_TSuccess GHOST_WindowCarbon::setState(GHOST_TWindowState state)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::setState(): window invalid")
	switch (state) {
		case GHOST_kWindowStateMinimized:
			::HideWindow(m_windowRef);
			break;
		case GHOST_kWindowStateModified:
			SetWindowModified(m_windowRef, 1);
			break;
		case GHOST_kWindowStateUnModified:
			SetWindowModified(m_windowRef, 0);
			break;
		case GHOST_kWindowStateMaximized:
		case GHOST_kWindowStateNormal:
		default:
			::ShowWindow(m_windowRef);
			break;
	}
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCarbon::setOrder(GHOST_TWindowOrder order)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::setOrder(): window invalid")
	if (order == GHOST_kWindowOrderTop) {
		//::BringToFront(m_windowRef); is wrong, front window should be active for input too
		::SelectWindow(m_windowRef);
	}
	else {
		/* doesnt work if you do this with a mouseclick */
		::SendBehind(m_windowRef, nil);
	}
	return GHOST_kSuccess;
}

/*#define  WAIT_FOR_VSYNC 1*/
#ifdef WAIT_FOR_VSYNC
#include <OpenGL/OpenGL.h>
#endif

GHOST_TSuccess GHOST_WindowCarbon::swapBuffers()
{
#ifdef WAIT_FOR_VSYNC
/* wait for vsync, to avoid tearing artifacts */
	long VBL = 1;
	CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &VBL);
#endif

	GHOST_TSuccess succeeded = GHOST_kSuccess;
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_aglCtx) {
			::aglSwapBuffers(m_aglCtx);
		}
		else {
			succeeded = GHOST_kFailure;
		}
	}
	return succeeded;
}

GHOST_TSuccess GHOST_WindowCarbon::updateDrawingContext()
{
	GHOST_TSuccess succeeded = GHOST_kSuccess;
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_aglCtx) {
			::aglUpdateContext(m_aglCtx);
		}
		else {
			succeeded = GHOST_kFailure;
		}
	}
	return succeeded;
}

GHOST_TSuccess GHOST_WindowCarbon::activateDrawingContext()
{
	GHOST_TSuccess succeeded = GHOST_kSuccess;
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_aglCtx) {
			::aglSetCurrentContext(m_aglCtx);
#ifdef GHOST_DRAW_CARBON_GUTTER
			// Restrict drawing to non-gutter area
			::aglEnable(m_aglCtx, AGL_BUFFER_RECT);
			GHOST_Rect bnds;
			getClientBounds(bnds);
			GLint b[4] =
			{
				bnds.m_l,
				bnds.m_t + s_sizeRectSize,
				bnds.m_r - bnds.m_l,
				bnds.m_b - bnds.m_t
			};
			GLboolean result = ::aglSetInteger(m_aglCtx, AGL_BUFFER_RECT, b);
#endif //GHOST_DRAW_CARBON_GUTTER
		}
		else {
			succeeded = GHOST_kFailure;
		}
	}
	return succeeded;
}


GHOST_TSuccess GHOST_WindowCarbon::installDrawingContext(GHOST_TDrawingContextType type)
{
	GHOST_TSuccess success = GHOST_kFailure;
	switch (type) {
		case GHOST_kDrawingContextTypeOpenGL:
		{
			if (!getValid()) break;

			AGLPixelFormat pixelFormat;
			if (!m_fullScreen) {
				pixelFormat = ::aglChoosePixelFormat(0, 0, sPreferredFormatWindow);
				m_aglCtx = ::aglCreateContext(pixelFormat, s_firstaglCtx);
				if (!m_aglCtx) break;
				if (!s_firstaglCtx) s_firstaglCtx = m_aglCtx;
				success = ::aglSetDrawable(m_aglCtx, m_grafPtr) == GL_TRUE ? GHOST_kSuccess : GHOST_kFailure;
			}
			else {
				//GHOST_PRINT("GHOST_WindowCarbon::installDrawingContext(): init full-screen OpenGL\n");
				GDHandle device = ::GetMainDevice(); pixelFormat = ::aglChoosePixelFormat(&device, 1, sPreferredFormatFullScreen);
				m_aglCtx = ::aglCreateContext(pixelFormat, 0);
				if (!m_aglCtx) break;
				if (!s_firstaglCtx) s_firstaglCtx = m_aglCtx;
				//GHOST_PRINT("GHOST_WindowCarbon::installDrawingContext(): created OpenGL context\n");
				//::CGGetActiveDisplayList(0, NULL, &m_numDisplays)
				success = ::aglSetFullScreen(m_aglCtx, m_fullScreenWidth, m_fullScreenHeight, 75, 0) == GL_TRUE ? GHOST_kSuccess : GHOST_kFailure;
#if 0
				if (success == GHOST_kSuccess) {
					GHOST_PRINT("GHOST_WindowCarbon::installDrawingContext(): init full-screen OpenGL succeeded\n");
				}
				else {
					GHOST_PRINT("GHOST_WindowCarbon::installDrawingContext(): init full-screen OpenGL failed\n");
				}
#endif
			}
			::aglDestroyPixelFormat(pixelFormat);
		}
		break;
		
		case GHOST_kDrawingContextTypeNone:
			success = GHOST_kSuccess;
			break;
		
		default:
			break;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowCarbon::removeDrawingContext()
{
	GHOST_TSuccess success = GHOST_kFailure;
	switch (m_drawingContextType) {
		case GHOST_kDrawingContextTypeOpenGL:
			if (m_aglCtx) {
				aglSetCurrentContext(NULL);
				aglSetDrawable(m_aglCtx, NULL);
				//aglDestroyContext(m_aglCtx);
				if (s_firstaglCtx == m_aglCtx) s_firstaglCtx = NULL;
				success = ::aglDestroyContext(m_aglCtx) == GL_TRUE ? GHOST_kSuccess : GHOST_kFailure;
				m_aglCtx = 0;
			}
			break;
		case GHOST_kDrawingContextTypeNone:
			success = GHOST_kSuccess;
			break;
		default:
			break;
	}
	return success;
}


GHOST_TSuccess GHOST_WindowCarbon::invalidate()
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCarbon::invalidate(): window invalid")
	if (!m_fullScreen) {
		Rect rect;
		::GetPortBounds(m_grafPtr, &rect);
		::InvalWindowRect(m_windowRef, &rect);
	}
	else {
		//EventRef event;
		//OSStatus status = ::CreateEvent(NULL, kEventClassWindow, kEventWindowUpdate, 0, 0, &event);
		//GHOST_PRINT("GHOST_WindowCarbon::invalidate(): created event " << status << " \n");
		//status = ::SetEventParameter(event, kEventParamDirectObject, typeWindowRef, sizeof(WindowRef), this);
		//GHOST_PRINT("GHOST_WindowCarbon::invalidate(): set event parameter " << status << " \n");
		//status = ::PostEventToQueue(::GetMainEventQueue(), event, kEventPriorityStandard);
		//status = ::SendEventToEventTarget(event, ::GetApplicationEventTarget());
		//GHOST_PRINT("GHOST_WindowCarbon::invalidate(): added event to queue " << status << " \n");
		m_fullScreenDirty = true;
	}
	return GHOST_kSuccess;
}


void GHOST_WindowCarbon::gen2mac(const STR_String& in, Str255 out) const
{
	STR_String tempStr  = in;
	int num = tempStr.Length();
	if (num > 255) num = 255;
	::memcpy(out + 1, tempStr.Ptr(), num);
	out[0] = num;
}


void GHOST_WindowCarbon::mac2gen(const Str255 in, STR_String& out) const
{
	char tmp[256];
	::memcpy(tmp, in + 1, in[0]);
	tmp[in[0]] = '\0';
	out = tmp;
}

void GHOST_WindowCarbon::loadCursor(bool visible, GHOST_TStandardCursor cursor) const
{
	static bool systemCursorVisible = true;
	
	if (visible != systemCursorVisible) {
		if (visible) {
			::ShowCursor();
			systemCursorVisible = true;
		}
		else {
			::HideCursor();
			systemCursorVisible = false;
		}
	}

	if (cursor == GHOST_kStandardCursorCustom && m_customCursor) {
		::SetCursor(m_customCursor);
	}
	else {
		int carbon_cursor;
	
#define GCMAP(ghostCursor, carbonCursor)    case ghostCursor: carbon_cursor = carbonCursor; break
		switch (cursor) {
			default:
				GCMAP(GHOST_kStandardCursorDefault,                kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorRightArrow,             kThemeAliasArrowCursor);
				GCMAP(GHOST_kStandardCursorLeftArrow,              kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorInfo,                   kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorDestroy,                kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorHelp,                   kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorCycle,                  kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorSpray,                  kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorWait,                   kThemeWatchCursor);
				GCMAP(GHOST_kStandardCursorText,                   kThemeIBeamCursor);
				GCMAP(GHOST_kStandardCursorCrosshair,              kThemeCrossCursor);
				GCMAP(GHOST_kStandardCursorUpDown,                 kThemeClosedHandCursor);
				GCMAP(GHOST_kStandardCursorLeftRight,              kThemeClosedHandCursor);
				GCMAP(GHOST_kStandardCursorTopSide,                kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorBottomSide,             kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorLeftSide,               kThemeResizeLeftCursor);
				GCMAP(GHOST_kStandardCursorRightSide,              kThemeResizeRightCursor);
				GCMAP(GHOST_kStandardCursorTopLeftCorner,          kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorTopRightCorner,         kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorBottomRightCorner,      kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorBottomLeftCorner,       kThemeArrowCursor);
				GCMAP(GHOST_kStandardCursorCopy,                   kThemeCopyArrowCursor);
		};
#undef GCMAP

		::SetThemeCursor(carbon_cursor);
	}
}


bool GHOST_WindowCarbon::getFullScreenDirty()
{
	return m_fullScreen && m_fullScreenDirty;
}


GHOST_TSuccess GHOST_WindowCarbon::setWindowCursorVisibility(bool visible)
{
	if (::FrontWindow() == m_windowRef) {
		loadCursor(visible, getCursorShape());
	}
	
	return GHOST_kSuccess;
}
	
GHOST_TSuccess GHOST_WindowCarbon::setWindowCursorShape(GHOST_TStandardCursor shape)
{
	if (m_customCursor) {
		delete m_customCursor;
		m_customCursor = 0;
	}

	if (::FrontWindow() == m_windowRef) {
		loadCursor(getCursorVisibility(), shape);
	}
	
	return GHOST_kSuccess;
}

#if 0
/** Reverse the bits in a GHOST_TUns8 */
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
	ch = ((ch >> 1) & 0x55) | ((ch << 1) & 0xAA);
	ch = ((ch >> 2) & 0x33) | ((ch << 2) & 0xCC);
	ch = ((ch >> 4) & 0x0F) | ((ch << 4) & 0xF0);
	return ch;
}
#endif


/** Reverse the bits in a GHOST_TUns16 */
static GHOST_TUns16 uns16ReverseBits(GHOST_TUns16 shrt)
{
	shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
	shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
	shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
	shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
	return shrt;
}

GHOST_TSuccess GHOST_WindowCarbon::setWindowCustomCursorShape(GHOST_TUns8 *bitmap, GHOST_TUns8 *mask,
                                                              int sizex, int sizey, int hotX, int hotY, int fg_color, int bg_color)
{
	int y;
	
	if (m_customCursor) {
		delete m_customCursor;
		m_customCursor = 0;
	}
	
	m_customCursor = new Cursor;
	if (!m_customCursor) return GHOST_kFailure;
	
	for (y = 0; y < 16; y++) {
#if !defined(__LITTLE_ENDIAN__)
		m_customCursor->data[y] = uns16ReverseBits((bitmap[2 * y] << 0) | (bitmap[2 * y + 1] << 8));
		m_customCursor->mask[y] = uns16ReverseBits((mask[2 * y] << 0) | (mask[2 * y + 1] << 8));
#else
		m_customCursor->data[y] = uns16ReverseBits((bitmap[2 * y + 1] << 0) | (bitmap[2 * y] << 8));
		m_customCursor->mask[y] = uns16ReverseBits((mask[2 * y + 1] << 0) | (mask[2 * y] << 8));
#endif
			
	}
	
	m_customCursor->hotSpot.h = hotX;
	m_customCursor->hotSpot.v = hotY;
	
	if (::FrontWindow() == m_windowRef) {
		loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
	}
	
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCarbon::setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2], 
                                                              GHOST_TUns8 mask[16][2], int hotX, int hotY)
{
	return setWindowCustomCursorShape((GHOST_TUns8 *)bitmap, (GHOST_TUns8 *) mask, 16, 16, hotX, hotY, 0, 1);
}


void GHOST_WindowCarbon::setMac_windowState(short value)
{
	mac_windowState = value;
}

short GHOST_WindowCarbon::getMac_windowState()
{
	return mac_windowState;
}
