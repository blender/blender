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
 * Contributor(s):	Maarten Gribnau 05/2001
					Damien Plisson 10/2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <Cocoa/Cocoa.h>

#include "GHOST_WindowCocoa.h"
#include "GHOST_SystemCocoa.h"
#include "GHOST_Debug.h"
/*
AGLContext GHOST_WindowCocoa::s_firstaglCtx = NULL;
#ifdef GHOST_DRAW_CARBON_GUTTER
const GHOST_TInt32 GHOST_WindowCocoa::s_sizeRectSize = 16;
#endif //GHOST_DRAW_CARBON_GUTTER

static const GLint sPreferredFormatWindow[8] = {
AGL_RGBA,
AGL_DOUBLEBUFFER,	
AGL_ACCELERATED,
AGL_DEPTH_SIZE,		32,
AGL_NONE,
};

static const GLint sPreferredFormatFullScreen[9] = {
AGL_RGBA,
AGL_DOUBLEBUFFER,
AGL_ACCELERATED,
AGL_FULLSCREEN,
AGL_DEPTH_SIZE,		32,
AGL_NONE,
};



WindowRef ugly_hack=NULL;

const EventTypeSpec	kWEvents[] = {
	{ kEventClassWindow, kEventWindowZoom },  // for new zoom behaviour  
};

static OSStatus myWEventHandlerProc(EventHandlerCallRef handler, EventRef event, void* userData) {
	WindowRef mywindow;
	GHOST_WindowCocoa *ghost_window;
	OSStatus err;
	int theState;
	
	if (::GetEventKind(event) == kEventWindowZoom) {
		err =  ::GetEventParameter (event,kEventParamDirectObject,typeWindowRef,NULL,sizeof(mywindow),NULL, &mywindow);
		ghost_window = (GHOST_WindowCocoa *) GetWRefCon(mywindow);
		theState = ghost_window->getMac_windowState();
		if (theState == 1) 
			ghost_window->setMac_windowState(2);
		else if (theState == 2)
			ghost_window->setMac_windowState(1);

	}
	return eventNotHandledErr;
}*/

#pragma mark Cocoa delegate object
@interface CocoaWindowDelegate : NSObject
{
	GHOST_SystemCocoa *systemCocoa;
	GHOST_WindowCocoa *associatedWindow;
}

- (void)setSystemAndWindowCocoa:(const GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa;
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;
- (void)windowDidUpdate:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
@end

@implementation CocoaWindowDelegate : NSObject
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
	systemCocoa = sysCocoa;
	associatedWindow = winCocoa;
}

- (void)windowWillClose:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowClose, associatedWindow);
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowActivate, associatedWindow);
}

- (void)windowDidResignKey:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowDeactivate, associatedWindow);
}

- (void)windowDidUpdate:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowUpdate, associatedWindow);
}

- (void)windowDidResize:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);
}
@end

#pragma mark NSOpenGLView subclass
//We need to subclass it in order to give Cocoa the feeling key events are trapped
@interface CocoaOpenGLView : NSOpenGLView
{
	
}
@end
@implementation CocoaOpenGLView

- (BOOL)acceptsFirstResponder
{
    return YES;
}

//The trick to prevent Cocoa from complaining (beeping)
- (void)keyDown:(NSEvent *)theEvent
{}

- (BOOL)isOpaque
{
    return YES;
}

@end


#pragma mark initialization / finalization

GHOST_WindowCocoa::GHOST_WindowCocoa(
	const GHOST_SystemCocoa *systemCocoa,
	const STR_String& title,
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	const bool stereoVisual
) :
	GHOST_Window(title, left, top, width, height, state, GHOST_kDrawingContextTypeNone),
	m_customCursor(0),
	m_fullScreenDirty(false)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	//fprintf(stderr," main screen top %i left %i height %i width %i\n", top, left, height, width);
	/*
	if (state >= GHOST_kWindowState8Normal ) {
		if(state == GHOST_kWindowState8Normal) state= GHOST_kWindowStateNormal;
		else if(state == GHOST_kWindowState8Maximized) state= GHOST_kWindowStateMaximized;
		else if(state == GHOST_kWindowState8Minimized) state= GHOST_kWindowStateMinimized;
		else if(state == GHOST_kWindowState8FullScreen) state= GHOST_kWindowStateFullScreen;
		
		// state = state - 8;	this was the simple version of above code, doesnt work in gcc 4.0
		
		setMac_windowState(1);
	} else 
		setMac_windowState(0);
*/
	if (state != GHOST_kWindowStateFullScreen) {
		
		//Creates the window
		NSRect rect;
		
		rect.origin.x = left;
		rect.origin.y = top;
		rect.size.width = width;
		rect.size.height = height;
		
		m_window = [[NSWindow alloc] initWithContentRect:rect
											   styleMask:NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask
												 backing:NSBackingStoreBuffered defer:NO];
		if (m_window == nil) {
			[pool drain];
			return;
		}
		
		[m_window setTitle:[NSString stringWithUTF8String:title]];
		
				
		//Creates the OpenGL View inside the window
		NSOpenGLPixelFormatAttribute attributes[] =
		{
			NSOpenGLPFADoubleBuffer,
			NSOpenGLPFAAccelerated,
			NSOpenGLPFAAllowOfflineRenderers,   // NOTE: Needed to connect to secondary GPUs
			NSOpenGLPFADepthSize, 32,
			0
		};
		
		NSOpenGLPixelFormat *pixelFormat =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
		
		m_openGLView = [[CocoaOpenGLView alloc] initWithFrame:rect
													 pixelFormat:pixelFormat];
		
		[pixelFormat release];
		
		m_openGLContext = [m_openGLView openGLContext];
		
		[m_window setContentView:m_openGLView];
		[m_window setInitialFirstResponder:m_openGLView];
		
		[m_window setReleasedWhenClosed:NO]; //To avoid bad pointer exception in case of user closing the window
		
		[m_window makeKeyAndOrderFront:nil];
		
		setDrawingContextType(type);
		updateDrawingContext();
		activateDrawingContext();
		
		// Boolean visible = (state == GHOST_kWindowStateNormal) || (state == GHOST_kWindowStateMaximized); /*unused*/
        /*gen2mac(title, title255);
        
		
		err =  ::CreateNewWindow( kDocumentWindowClass,
								 kWindowStandardDocumentAttributes+kWindowLiveResizeAttribute,
								 &bnds,
								 &m_windowRef);
		
		if ( err != noErr) {
			fprintf(stderr," error creating window %i \n",(int)err);
		} else {
			
			::SetWRefCon(m_windowRef,(SInt32)this);
			setTitle(title);
			err = InstallWindowEventHandler (m_windowRef, myWEventHandlerProc, GetEventTypeCount(kWEvents), kWEvents,NULL,NULL); 
			if ( err != noErr) {
				fprintf(stderr," error creating handler %i \n",(int)err);
			} else {
				//	::TransitionWindow (m_windowRef,kWindowZoomTransitionEffect,kWindowShowTransitionAction,NULL);
				::ShowWindow(m_windowRef);
				::MoveWindow (m_windowRef, left, top,true);
				
			}
		}
        if (m_windowRef) {
            m_grafPtr = ::GetWindowPort(m_windowRef);
            setDrawingContextType(type);
            updateDrawingContext();
            activateDrawingContext();
        }
		if(ugly_hack==NULL) {
			ugly_hack= m_windowRef;
			// when started from commandline, window remains in the back... also for play anim
			ProcessSerialNumber psn;
			GetCurrentProcess(&psn);
			SetFrontProcess(&psn);
		}*/
    }
    else {
    /*
        Rect bnds = { top, left, top+height, left+width };
        gen2mac("", title255);
        m_windowRef = ::NewCWindow(
            nil,							// Storage 
            &bnds,							// Bounding rectangle of the window
            title255,						// Title of the window
            0,								// Window initially visible
            plainDBox, 						// procID
            (WindowRef)-1L,					// Put window before all other windows
            0,								// Window has minimize box
            (SInt32)this);					// Store a pointer to the class in the refCon
    */
        //GHOST_PRINT("GHOST_WindowCocoa::GHOST_WindowCocoa(): creating full-screen OpenGL context\n");
        setDrawingContextType(GHOST_kDrawingContextTypeOpenGL);
		installDrawingContext(GHOST_kDrawingContextTypeOpenGL);
        updateDrawingContext();
        activateDrawingContext();
    }
	m_tablet.Active = GHOST_kTabletModeNone;
	
	CocoaWindowDelegate *windowDelegate = [[CocoaWindowDelegate alloc] init];
	[windowDelegate setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
	[m_window setDelegate:windowDelegate];
	
	[m_window setAcceptsMouseMovedEvents:YES];
	
	[pool drain];
}


GHOST_WindowCocoa::~GHOST_WindowCocoa()
{
	if (m_customCursor) delete m_customCursor;

	/*if(ugly_hack==m_windowRef) ugly_hack= NULL;
	
	if(ugly_hack==NULL) setDrawingContextType(GHOST_kDrawingContextTypeNone);*/
    
	[m_openGLView release];
	
	if (m_window) {
		[m_window close];
		[m_window release];
		m_window = nil;
	}
}

#pragma mark accessors

bool GHOST_WindowCocoa::getValid() const
{
    bool valid;
    if (!m_fullScreen) {
        valid = (m_window != 0); //&& ::IsValidWindowPtr(m_windowRef);
    }
    else {
        valid = true;
    }
    return valid;
}


void GHOST_WindowCocoa::setTitle(const STR_String& title)
{
    GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setTitle(): window invalid")
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSString *windowTitle = [[NSString alloc] initWithUTF8String:title];
	
	[m_window setTitle:windowTitle];
	
	[windowTitle release];
	[pool drain];
}


void GHOST_WindowCocoa::getTitle(STR_String& title) const
{
    GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getTitle(): window invalid")

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSString *windowTitle = [m_window title];

	if (windowTitle != nil) {
		title = [windowTitle UTF8String];		
	}
	
	[pool drain];
}


void GHOST_WindowCocoa::getWindowBounds(GHOST_Rect& bounds) const
{
	NSRect rect;
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getWindowBounds(): window invalid")

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSRect screenSize = [[m_window screen] visibleFrame];

	rect = [m_window frame];

	bounds.m_b = screenSize.size.height - (rect.origin.y -screenSize.origin.y);
	bounds.m_l = rect.origin.x -screenSize.origin.x;
	bounds.m_r = rect.origin.x-screenSize.origin.x + rect.size.width;
	bounds.m_t = screenSize.size.height - (rect.origin.y + rect.size.height -screenSize.origin.y);
	
	[pool drain];
}


void GHOST_WindowCocoa::getClientBounds(GHOST_Rect& bounds) const
{
	NSRect rect;
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getClientBounds(): window invalid")
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSRect screenSize = [[m_window screen] visibleFrame];

	//Max window contents as screen size (excluding title bar...)
	NSRect contentRect = [NSWindow contentRectForFrameRect:screenSize
												 styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask)];

	rect = [m_window contentRectForFrameRect:[m_window frame]];
	
	bounds.m_b = contentRect.size.height - (rect.origin.y -contentRect.origin.y);
	bounds.m_l = rect.origin.x -contentRect.origin.x;
	bounds.m_r = rect.origin.x-contentRect.origin.x + rect.size.width;
	bounds.m_t = contentRect.size.height - (rect.origin.y + rect.size.height -contentRect.origin.y);
	
	[pool drain];
}


GHOST_TSuccess GHOST_WindowCocoa::setClientWidth(GHOST_TUns32 width)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientWidth(): window invalid")
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (((GHOST_TUns32)cBnds.getWidth()) != width) {
		NSSize size;
		size.width=width;
		size.height=cBnds.getHeight();
		[m_window setContentSize:size];
	}
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCocoa::setClientHeight(GHOST_TUns32 height)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientHeight(): window invalid")
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (((GHOST_TUns32)cBnds.getHeight()) != height) {
		NSSize size;
		size.width=cBnds.getWidth();
		size.height=height;
		[m_window setContentSize:size];
	}
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCocoa::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientSize(): window invalid")
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if ((((GHOST_TUns32)cBnds.getWidth()) != width) ||
	    (((GHOST_TUns32)cBnds.getHeight()) != height)) {
		NSSize size;
		size.width=width;
		size.height=height;
		[m_window setContentSize:size];
	}
	return GHOST_kSuccess;
}


GHOST_TWindowState GHOST_WindowCocoa::getState() const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getState(): window invalid")
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	GHOST_TWindowState state;
	if ([m_window isMiniaturized]) {
		state = GHOST_kWindowStateMinimized;
	}
	else if ([m_window isZoomed]) {
		state = GHOST_kWindowStateMaximized;
	}
	else {
		state = GHOST_kWindowStateNormal;
	}
	[pool drain];
	return state;
}


void GHOST_WindowCocoa::screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::screenToClient(): window invalid")
	
	NSPoint screenCoord;
	NSPoint baseCoord;
	
	screenCoord.x = inX;
	screenCoord.y = inY;
	
	baseCoord = [m_window convertScreenToBase:screenCoord];
	
	outX = baseCoord.x;
	outY = baseCoord.y;
}


void GHOST_WindowCocoa::clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::clientToScreen(): window invalid")
	
	NSPoint screenCoord;
	NSPoint baseCoord;
	
	baseCoord.x = inX;
	baseCoord.y = inY;
	
	screenCoord = [m_window convertBaseToScreen:baseCoord];
	
	outX = screenCoord.x;
	outY = screenCoord.y;
}


GHOST_TSuccess GHOST_WindowCocoa::setState(GHOST_TWindowState state)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setState(): window invalid")
    switch (state) {
	case GHOST_kWindowStateMinimized:
            [m_window miniaturize:nil];
            break;
	case GHOST_kWindowStateMaximized:
			[m_window zoom:nil];
			break;
	case GHOST_kWindowStateNormal:
        default:
            if ([m_window isMiniaturized])
				[m_window deminiaturize:nil];
			else if ([m_window isZoomed])
				[m_window zoom:nil];
            break;
    }
    return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setModifiedState(bool isUnsavedChanges)
{
	[m_window setDocumentEdited:isUnsavedChanges];
	
	return GHOST_Window::setModifiedState(isUnsavedChanges);
}



GHOST_TSuccess GHOST_WindowCocoa::setOrder(GHOST_TWindowOrder order)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setOrder(): window invalid")
    if (order == GHOST_kWindowOrderTop) {
		[m_window orderFront:nil];
    }
    else {
		[m_window orderBack:nil];
    }
    return GHOST_kSuccess;
}

#pragma mark Drawing context

/*#define  WAIT_FOR_VSYNC 1*/

GHOST_TSuccess GHOST_WindowCocoa::swapBuffers()
{
    if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
        if (m_openGLContext != nil) {
			[m_openGLContext flushBuffer];
            return GHOST_kSuccess;
        }
    }
    return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowCocoa::updateDrawingContext()
{
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_openGLContext != nil) {
			[m_openGLContext update];
			return GHOST_kSuccess;
		}
	}
	return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowCocoa::activateDrawingContext()
{
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_openGLContext != nil) {
			[m_openGLContext makeCurrentContext];
#ifdef GHOST_DRAW_CARBON_GUTTER
			// Restrict drawing to non-gutter area
			::aglEnable(m_aglCtx, AGL_BUFFER_RECT);
			GHOST_Rect bnds;
			getClientBounds(bnds);
			GLint b[4] =
			{
				bnds.m_l,
				bnds.m_t+s_sizeRectSize,
				bnds.m_r-bnds.m_l,
				bnds.m_b-bnds.m_t
			};
			GLboolean result = ::aglSetInteger(m_aglCtx, AGL_BUFFER_RECT, b);
#endif //GHOST_DRAW_CARBON_GUTTER
			return GHOST_kSuccess;
		}
	}
	return GHOST_kFailure;
}


GHOST_TSuccess GHOST_WindowCocoa::installDrawingContext(GHOST_TDrawingContextType type)
{
	GHOST_TSuccess success = GHOST_kFailure;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSOpenGLPixelFormat *pixelFormat;
	NSOpenGLContext *tmpOpenGLContext;
	
	switch (type) {
		case GHOST_kDrawingContextTypeOpenGL:
			if (!getValid()) break;
            				
			if(!m_fullScreen)
			{
				pixelFormat = [m_openGLView pixelFormat];
				tmpOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
															  shareContext:m_openGLContext];
				if (tmpOpenGLContext == nil)
					break;
#ifdef WAIT_FOR_VSYNC
				/* wait for vsync, to avoid tearing artifacts */
				[tmpOpenGLContext setValues:1 forParameter:NSOpenGLCPSwapInterval];
#endif
				[m_openGLView setOpenGLContext:tmpOpenGLContext];
				[tmpOpenGLContext setView:m_openGLView];
				
				//[m_openGLContext release];
				m_openGLContext = tmpOpenGLContext;
			}
			/*	
            AGLPixelFormat pixelFormat;
            if (!m_fullScreen) {
                pixelFormat = ::aglChoosePixelFormat(0, 0, sPreferredFormatWindow);
                m_aglCtx = ::aglCreateContext(pixelFormat, s_firstaglCtx);
                if (!m_aglCtx) break;
				if (!s_firstaglCtx) s_firstaglCtx = m_aglCtx;
                 success = ::aglSetDrawable(m_aglCtx, m_grafPtr) == GL_TRUE ? GHOST_kSuccess : GHOST_kFailure;
            }
            else {
                //GHOST_PRINT("GHOST_WindowCocoa::installDrawingContext(): init full-screen OpenGL\n");
GDHandle device=::GetMainDevice();pixelFormat=::aglChoosePixelFormat(&device,1,sPreferredFormatFullScreen);
                m_aglCtx = ::aglCreateContext(pixelFormat, 0);
                if (!m_aglCtx) break;
				if (!s_firstaglCtx) s_firstaglCtx = m_aglCtx;
                //GHOST_PRINT("GHOST_WindowCocoa::installDrawingContext(): created OpenGL context\n");
                //::CGGetActiveDisplayList(0, NULL, &m_numDisplays)
                success = ::aglSetFullScreen(m_aglCtx, m_fullScreenWidth, m_fullScreenHeight, 75, 0) == GL_TRUE ? GHOST_kSuccess : GHOST_kFailure;
                
                if (success == GHOST_kSuccess) {
                    GHOST_PRINT("GHOST_WindowCocoa::installDrawingContext(): init full-screen OpenGL succeeded\n");
                }
                else {
                    GHOST_PRINT("GHOST_WindowCocoa::installDrawingContext(): init full-screen OpenGL failed\n");
                }
                
            }
            ::aglDestroyPixelFormat(pixelFormat);*/
			break;
		
		case GHOST_kDrawingContextTypeNone:
			success = GHOST_kSuccess;
			break;
		
		default:
			break;
	}
	[pool drain];
	return success;
}


GHOST_TSuccess GHOST_WindowCocoa::removeDrawingContext()
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	switch (m_drawingContextType) {
		case GHOST_kDrawingContextTypeOpenGL:
			[m_openGLView clearGLContext];
			return GHOST_kSuccess;
		case GHOST_kDrawingContextTypeNone:
			return GHOST_kSuccess;
			break;
		default:
			return GHOST_kFailure;
	}
	[pool drain];
}


GHOST_TSuccess GHOST_WindowCocoa::invalidate()
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::invalidate(): window invalid")
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    if (!m_fullScreen) {
		[m_openGLView setNeedsDisplay:YES];
    }
    else {
        //EventRef event;
        //OSStatus status = ::CreateEvent(NULL, kEventClassWindow, kEventWindowUpdate, 0, 0, &event);
        //GHOST_PRINT("GHOST_WindowCocoa::invalidate(): created event " << status << " \n");
        //status = ::SetEventParameter(event, kEventParamDirectObject, typeWindowRef, sizeof(WindowRef), this);
        //GHOST_PRINT("GHOST_WindowCocoa::invalidate(): set event parameter " << status << " \n");
        //status = ::PostEventToQueue(::GetMainEventQueue(), event, kEventPriorityStandard);
        //status = ::SendEventToEventTarget(event, ::GetApplicationEventTarget());
        //GHOST_PRINT("GHOST_WindowCocoa::invalidate(): added event to queue " << status << " \n");
        m_fullScreenDirty = true;
    }
	[pool drain];
	return GHOST_kSuccess;
}

#pragma mark Cursor handling

void GHOST_WindowCocoa::loadCursor(bool visible, GHOST_TStandardCursor cursor) const
{
	static bool systemCursorVisible = true;
	
	NSAutoreleasePool *pool =[[NSAutoreleasePool alloc] init];

	NSCursor *tmpCursor =nil;
	
	if (visible != systemCursorVisible) {
		if (visible) {
			[NSCursor unhide];
			systemCursorVisible = true;
		}
		else {
			[NSCursor hide];
			systemCursorVisible = false;
		}
	}

	if (cursor == GHOST_kStandardCursorCustom && m_customCursor) {
		tmpCursor = m_customCursor;
	} else {
		switch (cursor) {
			case GHOST_kStandardCursorDestroy:
				tmpCursor = [NSCursor disappearingItemCursor];
				break;
			case GHOST_kStandardCursorText:
				tmpCursor = [NSCursor IBeamCursor];
				break;
			case GHOST_kStandardCursorCrosshair:
				tmpCursor = [NSCursor crosshairCursor];
				break;
			case GHOST_kStandardCursorUpDown:
				tmpCursor = [NSCursor resizeUpDownCursor];
				break;
			case GHOST_kStandardCursorLeftRight:
				tmpCursor = [NSCursor resizeLeftRightCursor];
				break;
			case GHOST_kStandardCursorTopSide:
				tmpCursor = [NSCursor resizeUpCursor];
				break;
			case GHOST_kStandardCursorBottomSide:
				tmpCursor = [NSCursor resizeDownCursor];
				break;
			case GHOST_kStandardCursorLeftSide:
				tmpCursor = [NSCursor resizeLeftCursor];
				break;
			case GHOST_kStandardCursorRightSide:
				tmpCursor = [NSCursor resizeRightCursor];
				break;
			case GHOST_kStandardCursorRightArrow:
			case GHOST_kStandardCursorInfo:
			case GHOST_kStandardCursorLeftArrow:
			case GHOST_kStandardCursorHelp:
			case GHOST_kStandardCursorCycle:
			case GHOST_kStandardCursorSpray:
			case GHOST_kStandardCursorWait:
			case GHOST_kStandardCursorTopLeftCorner:
			case GHOST_kStandardCursorTopRightCorner:
			case GHOST_kStandardCursorBottomRightCorner:
			case GHOST_kStandardCursorBottomLeftCorner:
			case GHOST_kStandardCursorDefault:
			default:
				tmpCursor = [NSCursor arrowCursor];
				break;
		};
	}
	[tmpCursor set];
	[pool drain];
}


bool GHOST_WindowCocoa::getFullScreenDirty()
{
    return m_fullScreen && m_fullScreenDirty;
}


GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorVisibility(bool visible)
{
	if ([m_window isVisible]) {
		loadCursor(visible, getCursorShape());
	}
	
	return GHOST_kSuccess;
}
	
GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorShape(GHOST_TStandardCursor shape)
{
	if (m_customCursor) {
		[m_customCursor release];
		m_customCursor = nil;
	}

	if ([m_window isVisible]) {
		loadCursor(getCursorVisibility(), shape);
	}
	
	return GHOST_kSuccess;
}

#if 0
/** Reverse the bits in a GHOST_TUns8 */
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
	ch= ((ch>>1)&0x55) | ((ch<<1)&0xAA);
	ch= ((ch>>2)&0x33) | ((ch<<2)&0xCC);
	ch= ((ch>>4)&0x0F) | ((ch<<4)&0xF0);
	return ch;
}
#endif


/** Reverse the bits in a GHOST_TUns16 */
static GHOST_TUns16 uns16ReverseBits(GHOST_TUns16 shrt)
{
	shrt= ((shrt>>1)&0x5555) | ((shrt<<1)&0xAAAA);
	shrt= ((shrt>>2)&0x3333) | ((shrt<<2)&0xCCCC);
	shrt= ((shrt>>4)&0x0F0F) | ((shrt<<4)&0xF0F0);
	shrt= ((shrt>>8)&0x00FF) | ((shrt<<8)&0xFF00);
	return shrt;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCustomCursorShape(GHOST_TUns8 *bitmap, GHOST_TUns8 *mask,
					int sizex, int sizey, int hotX, int hotY, int fg_color, int bg_color)
{
	int y;
	NSPoint hotSpotPoint;
	NSImage *cursorImage;
	
	if (m_customCursor) {
		[m_customCursor release];
		m_customCursor = nil;
	}
	/*TODO: implement this (but unused inproject at present)
	cursorImage = [[NSImage alloc] initWithData:bitmap];
	
	for (y=0; y<16; y++) {
#if !defined(__LITTLE_ENDIAN__)
		m_customCursor->data[y] = uns16ReverseBits((bitmap[2*y]<<0) | (bitmap[2*y+1]<<8));
		m_customCursor->mask[y] = uns16ReverseBits((mask[2*y]<<0) | (mask[2*y+1]<<8));
#else
		m_customCursor->data[y] = uns16ReverseBits((bitmap[2*y+1]<<0) | (bitmap[2*y]<<8));
		m_customCursor->mask[y] = uns16ReverseBits((mask[2*y+1]<<0) | (mask[2*y]<<8));
#endif
			
	}
	
	
	hotSpotPoint.x = hotX;
	hotSpotPoint.y = hotY;
	
	m_customCursor = [[NSCursor alloc] initWithImage:cursorImage
								 foregroundColorHint:<#(NSColor *)fg#>
								 backgroundColorHint:<#(NSColor *)bg#>
											 hotSpot:hotSpotPoint];
	
	[cursorImage release];
	
	if ([m_window isVisible]) {
		loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
	}
	*/
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2], 
												GHOST_TUns8 mask[16][2], int hotX, int hotY)
{
	return setWindowCustomCursorShape((GHOST_TUns8*)bitmap, (GHOST_TUns8*) mask, 16, 16, hotX, hotY, 0, 1);
}

#pragma mark Old carbon stuff to remove

#if 0
void GHOST_WindowCocoa::setMac_windowState(short value)
{
	mac_windowState = value;
}

short GHOST_WindowCocoa::getMac_windowState()
{
	return mac_windowState;
}

void GHOST_WindowCocoa::gen2mac(const STR_String& in, Str255 out) const
{
	STR_String tempStr  = in;
	int num = tempStr.Length();
	if (num > 255) num = 255;
	::memcpy(out+1, tempStr.Ptr(), num);
	out[0] = num;
}


void GHOST_WindowCocoa::mac2gen(const Str255 in, STR_String& out) const
{
	char tmp[256];
	::memcpy(tmp, in+1, in[0]);
	tmp[in[0]] = '\0';
	out = tmp;
}

#endif