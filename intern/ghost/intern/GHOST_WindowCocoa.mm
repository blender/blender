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

#ifndef MAC_OS_X_VERSION_10_6
//Use of the SetSystemUIMode function (64bit compatible)
#include <Carbon/Carbon.h>
#endif

/***** Multithreaded opengl code : uncomment for enabling
#include <OpenGL/OpenGL.h>
*/
 
#include "GHOST_WindowCocoa.h"
#include "GHOST_SystemCocoa.h"
#include "GHOST_Debug.h"


#pragma mark Cocoa window delegate object
/* live resize ugly patch
extern "C" {
	struct bContext;
	typedef struct bContext bContext;
	bContext* ghostC;
	extern int wm_window_timer(const bContext *C);
	extern void wm_window_process_events(const bContext *C);
	extern void wm_event_do_handlers(bContext *C);
	extern void wm_event_do_notifiers(bContext *C);
	extern void wm_draw_update(bContext *C);
};*/
@interface CocoaWindowDelegate : NSObject
#ifdef MAC_OS_X_VERSION_10_6
<NSWindowDelegate>
#endif
{
	GHOST_SystemCocoa *systemCocoa;
	GHOST_WindowCocoa *associatedWindow;
}

- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa;
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
	//The window is no more key when its own view becomes fullscreen
	//but ghost doesn't know the view/window difference, so hide this fact
	if (associatedWindow->getState() != GHOST_kWindowStateFullScreen)
		systemCocoa->handleWindowEvent(GHOST_kEventWindowDeactivate, associatedWindow);
}

- (void)windowDidUpdate:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowUpdate, associatedWindow);
}

- (void)windowDidResize:(NSNotification *)notification
{
#ifdef MAC_OS_X_VERSION_10_6
	//if (![[notification object] inLiveResize]) {
		//Send event only once, at end of resize operation (when user has released mouse button)
#endif
		systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);
#ifdef MAC_OS_X_VERSION_10_6
	//}
#endif
	/* Live resize ugly patch. Needed because live resize runs in a modal loop, not letting main loop run
	 if ([[notification object] inLiveResize]) {
		systemCocoa->dispatchEvents();
		wm_window_timer(ghostC);
		wm_event_do_handlers(ghostC);
		wm_event_do_notifiers(ghostC);
		wm_draw_update(ghostC);
	}*/
}
@end

#pragma mark NSWindow subclass
//We need to subclass it to tell that even borderless (fullscreen), it can become key (receive user events)
@interface CocoaWindow: NSWindow
{

}
@end
@implementation CocoaWindow

-(BOOL)canBecomeKeyWindow
{
	return YES;
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

NSOpenGLContext* GHOST_WindowCocoa::s_firstOpenGLcontext = nil;

GHOST_WindowCocoa::GHOST_WindowCocoa(
	GHOST_SystemCocoa *systemCocoa,
	const STR_String& title,
	GHOST_TInt32 left,
	GHOST_TInt32 top,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	const bool stereoVisual
) :
	GHOST_Window(title, left, top, width, height, state, type,stereoVisual),
	m_customCursor(0)
{
	NSOpenGLPixelFormatAttribute pixelFormatAttrsWindow[40];
	int i;
	
	m_systemCocoa = systemCocoa;
	m_fullScreen = false;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	

	//Creates the window
	NSRect rect;
	NSSize	minSize;
	
	rect.origin.x = left;
	rect.origin.y = top;
	rect.size.width = width;
	rect.size.height = height;
	
	m_window = [[CocoaWindow alloc] initWithContentRect:rect
										   styleMask:NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask
											 backing:NSBackingStoreBuffered defer:NO];
	if (m_window == nil) {
		[pool drain];
		return;
	}
	
	//Forbid to resize the window below the blender defined minimum one
	minSize.width = 320;
	minSize.height = 240;
	[m_window setContentMinSize:minSize];
	
	setTitle(title);
	
	
	// Pixel Format Attributes for the windowed NSOpenGLContext
	i=0;
	pixelFormatAttrsWindow[i++] = NSOpenGLPFADoubleBuffer;
	pixelFormatAttrsWindow[i++] = NSOpenGLPFAAccelerated;
	//pixelFormatAttrsWindow[i++] = NSOpenGLPFAAllowOfflineRenderers,;   // Removed to allow 10.4 builds, and 2 GPUs rendering is not used anyway
	
	pixelFormatAttrsWindow[i++] = NSOpenGLPFADepthSize;
	pixelFormatAttrsWindow[i++] = (NSOpenGLPixelFormatAttribute) 32;
	
	if (stereoVisual) pixelFormatAttrsWindow[i++] = NSOpenGLPFAStereo;
	
	pixelFormatAttrsWindow[i] = (NSOpenGLPixelFormatAttribute) 0;
	

	//Creates the OpenGL View inside the window
	NSOpenGLPixelFormat *pixelFormat =
	[[NSOpenGLPixelFormat alloc] initWithAttributes:pixelFormatAttrsWindow];
	
	m_openGLView = [[CocoaOpenGLView alloc] initWithFrame:rect
												 pixelFormat:pixelFormat];
	
	[pixelFormat release];
	
	m_openGLContext = [m_openGLView openGLContext]; //This context will be replaced by the proper one just after
	
	[m_window setContentView:m_openGLView];
	[m_window setInitialFirstResponder:m_openGLView];
	
	[m_window setReleasedWhenClosed:NO]; //To avoid bad pointer exception in case of user closing the window
	
	[m_window makeKeyAndOrderFront:nil];
	
	setDrawingContextType(type);
	updateDrawingContext();
	activateDrawingContext();
	
	m_tablet.Active = GHOST_kTabletModeNone;
	
	CocoaWindowDelegate *windowDelegate = [[CocoaWindowDelegate alloc] init];
	[windowDelegate setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
	[m_window setDelegate:windowDelegate];
	
	[m_window setAcceptsMouseMovedEvents:YES];
	
	if (state == GHOST_kWindowStateFullScreen)
		setState(GHOST_kWindowStateFullScreen);
		
	[pool drain];
}


GHOST_WindowCocoa::~GHOST_WindowCocoa()
{
	if (m_customCursor) delete m_customCursor;

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[m_openGLView release];
	
	if (m_window) {
		[m_window close];
		[[m_window delegate] release];
		[m_window release];
		m_window = nil;
	}
	
	//Check for other blender opened windows and make the frontmost key
	NSArray *windowsList = [NSApp orderedWindows];
	if ([windowsList count]) {
		[[windowsList objectAtIndex:0] makeKeyAndOrderFront:nil];
	}
	[pool drain];
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
	
	//Set associated file if applicable
	if ([windowTitle hasPrefix:@"Blender"])
	{
		NSRange fileStrRange;
		NSString *associatedFileName;
		int len;
		
		fileStrRange.location = [windowTitle rangeOfString:@"["].location+1;
		len = [windowTitle rangeOfString:@"]"].location - fileStrRange.location;
	
		if (len >0)
		{
			fileStrRange.length = len;
			associatedFileName = [windowTitle substringWithRange:fileStrRange];
			@try {
				[m_window setRepresentedFilename:associatedFileName];
			}
			@catch (NSException * e) {
				printf("\nInvalid file path given in window title");
			}
			[m_window setTitle:[associatedFileName lastPathComponent]];
		}
		else {
			[m_window setTitle:windowTitle];
			[m_window setRepresentedFilename:@""];
		}

	} else {
		[m_window setTitle:windowTitle];
		[m_window setRepresentedFilename:@""];
	}

	
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
	
	if (!m_fullScreen)
	{
		NSRect screenSize = [[m_window screen] visibleFrame];

		//Max window contents as screen size (excluding title bar...)
		NSRect contentRect = [CocoaWindow contentRectForFrameRect:screenSize
													 styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)];

		rect = [m_window contentRectForFrameRect:[m_window frame]];
		
		bounds.m_b = contentRect.size.height - (rect.origin.y -contentRect.origin.y);
		bounds.m_l = rect.origin.x -contentRect.origin.x;
		bounds.m_r = rect.origin.x-contentRect.origin.x + rect.size.width;
		bounds.m_t = contentRect.size.height - (rect.origin.y + rect.size.height -contentRect.origin.y);
	}
	else {
		NSRect screenSize = [[m_window screen] frame];
		
		bounds.m_b = screenSize.origin.y + screenSize.size.height;
		bounds.m_l = screenSize.origin.x;
		bounds.m_r = screenSize.origin.x + screenSize.size.width;
		bounds.m_t = screenSize.origin.y;
	}
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
	if (m_fullScreen) {
		state = GHOST_kWindowStateFullScreen;
	} 
	else if ([m_window isMiniaturized]) {
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


NSScreen* GHOST_WindowCocoa::getScreen()
{
	return [m_window screen];
}


/**
 * @note Fullscreen switch is not actual fullscreen with display capture. As this capture removes all OS X window manager features.
 * Instead, the menu bar and the dock are hidden, and the window is made borderless and enlarged.
 * Thus, process switch, exposé, spaces, ... still work in fullscreen mode
 */
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
		
		case GHOST_kWindowStateFullScreen:
			if (!m_fullScreen)
			{
				NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
			
				//This status change needs to be done before Cocoa call to enter fullscreen mode
				//to give window delegate hint not to forward its deactivation to ghost wm that doesn't know view/window difference
				m_fullScreen = true;

#ifdef MAC_OS_X_VERSION_10_6
				//10.6 provides Cocoa functions to autoshow menu bar, and to change a window style
				//Hide menu & dock if needed
				if ([[m_window screen] isEqual:[NSScreen mainScreen]])
				{
					[NSApp setPresentationOptions:(NSApplicationPresentationHideDock | NSApplicationPresentationAutoHideMenuBar)];
				}
				//Make window borderless and enlarge it
				[m_window setStyleMask:NSBorderlessWindowMask];
				[m_window setFrame:[[m_window screen] frame] display:YES];
				[m_window makeFirstResponder:m_openGLView];
#else
				//With 10.5, we need to create a new window to change its style to borderless
				//Hide menu & dock if needed
				if ([[m_window screen] isEqual:[NSScreen mainScreen]])
				{
					//Cocoa function in 10.5 does not allow to set the menu bar in auto-show mode [NSMenu setMenuBarVisible:NO];
					//One of the very few 64bit compatible Carbon function
					SetSystemUIMode(kUIModeAllHidden,kUIOptionAutoShowMenuBar);
				}
				//Create a fullscreen borderless window
				CocoaWindow *tmpWindow = [[CocoaWindow alloc]
										  initWithContentRect:[[m_window screen] frame]
										  styleMask:NSBorderlessWindowMask
										  backing:NSBackingStoreBuffered
										  defer:YES];
				//Copy current window parameters
				[tmpWindow setTitle:[m_window title]];
				[tmpWindow setRepresentedFilename:[m_window representedFilename]];
				[tmpWindow setReleasedWhenClosed:NO];
				[tmpWindow setAcceptsMouseMovedEvents:YES];
				[tmpWindow setDelegate:[m_window delegate]];
				
				//Assign the openGL view to the new window
				[tmpWindow setContentView:m_openGLView];
				
				//Show the new window
				[tmpWindow makeKeyAndOrderFront:nil];
				//Close and release old window
				[m_window setDelegate:nil]; // To avoid the notification of "window closed" event
				[m_window close];
				[m_window release];
				m_window = tmpWindow;
#endif
			
				//Tell WM of view new size
				m_systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, this);
				
				[pool drain];
				}
			break;
		case GHOST_kWindowStateNormal:
        default:
			if (m_fullScreen)
			{
				NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
				m_fullScreen = false;

				//Exit fullscreen
#ifdef MAC_OS_X_VERSION_10_6
				//Show again menu & dock if needed
				if ([[m_window screen] isEqual:[NSScreen mainScreen]])
				{
					[NSApp setPresentationOptions:NSApplicationPresentationDefault];
				}
				//Make window normal and resize it
				[m_window setStyleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)];
				[m_window setFrame:[[m_window screen] visibleFrame] display:YES];
				[m_window makeFirstResponder:m_openGLView];
#else
				//With 10.5, we need to create a new window to change its style to borderless
				//Show menu & dock if needed
				if ([[m_window screen] isEqual:[NSScreen mainScreen]])
				{
					//Cocoa function in 10.5 does not allow to set the menu bar in auto-show mode [NSMenu setMenuBarVisible:YES];
					SetSystemUIMode(kUIModeNormal, 0); //One of the very few 64bit compatible Carbon function
				}
				//Create a fullscreen borderless window
				CocoaWindow *tmpWindow = [[CocoaWindow alloc]
										  initWithContentRect:[[m_window screen] frame]
													styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)
													  backing:NSBackingStoreBuffered
														defer:YES];
				//Copy current window parameters
				[tmpWindow setTitle:[m_window title]];
				[tmpWindow setRepresentedFilename:[m_window representedFilename]];
				[tmpWindow setReleasedWhenClosed:NO];
				[tmpWindow setAcceptsMouseMovedEvents:YES];
				[tmpWindow setDelegate:[m_window delegate]];
				
				//Assign the openGL view to the new window
				[tmpWindow setContentView:m_openGLView];
				
				//Show the new window
				[tmpWindow makeKeyAndOrderFront:nil];
				//Close and release old window
				[m_window setDelegate:nil]; // To avoid the notification of "window closed" event
				[m_window close];
				[m_window release];
				m_window = tmpWindow;
#endif
			
				//Tell WM of view new size
				m_systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, this);
				
				[pool drain];
			}
            else if ([m_window isMiniaturized])
				[m_window deminiaturize:nil];
			else if ([m_window isZoomed])
				[m_window zoom:nil];
            break;
    }
    return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setModifiedState(bool isUnsavedChanges)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	[m_window setDocumentEdited:isUnsavedChanges];
	
	[pool drain];
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
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
			[m_openGLContext flushBuffer];
			[pool drain];
            return GHOST_kSuccess;
        }
    }
    return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowCocoa::updateDrawingContext()
{
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_openGLContext != nil) {
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
			[m_openGLContext update];
			[pool drain];
			return GHOST_kSuccess;
		}
	}
	return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowCocoa::activateDrawingContext()
{
	if (m_drawingContextType == GHOST_kDrawingContextTypeOpenGL) {
		if (m_openGLContext != nil) {
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
			[m_openGLContext makeCurrentContext];
			[pool drain];
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
	
	/***** Multithreaded opengl code : uncomment for enabling
	CGLContextObj cglCtx;
	*/
	 
	switch (type) {
		case GHOST_kDrawingContextTypeOpenGL:
			if (!getValid()) break;
            				
			pixelFormat = [m_openGLView pixelFormat];
			tmpOpenGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat
															  shareContext:s_firstOpenGLcontext];
			if (tmpOpenGLContext == nil) {
				success = GHOST_kFailure;
				break;
			}
			
			//Switch openGL to multhreaded mode
			/******* Multithreaded opengl code : uncomment for enabling
			cglCtx = (CGLContextObj)[tmpOpenGLContext CGLContextObj];
			if (CGLEnable(cglCtx, kCGLCEMPEngine) == kCGLNoError)
				printf("\nSwitched openGL to multithreaded mode");
			 */
			
			if (!s_firstOpenGLcontext) s_firstOpenGLcontext = tmpOpenGLContext;
#ifdef WAIT_FOR_VSYNC
				/* wait for vsync, to avoid tearing artifacts */
				[tmpOpenGLContext setValues:1 forParameter:NSOpenGLCPSwapInterval];
#endif
				[m_openGLView setOpenGLContext:tmpOpenGLContext];
				[tmpOpenGLContext setView:m_openGLView];
				
				m_openGLContext = tmpOpenGLContext;
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
			if (m_openGLContext)
			{
				[m_openGLView clearGLContext];
				if (s_firstOpenGLcontext == m_openGLContext) s_firstOpenGLcontext = nil;
				m_openGLContext = nil;
			}
			[pool drain];
			return GHOST_kSuccess;
		case GHOST_kDrawingContextTypeNone:
			[pool drain];
			return GHOST_kSuccess;
			break;
		default:
			[pool drain];
			return GHOST_kFailure;
	}
}


GHOST_TSuccess GHOST_WindowCocoa::invalidate()
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::invalidate(): window invalid")
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[m_openGLView setNeedsDisplay:YES];
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



GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorVisibility(bool visible)
{
	if ([m_window isVisible]) {
		loadCursor(visible, getCursorShape());
	}
	
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
	GHOST_TSuccess err = GHOST_kSuccess;
	
	if (mode != GHOST_kGrabDisable)
	{
		//No need to perform grab without warp as it is always on in OS X
		if(mode != GHOST_kGrabNormal) {
			GHOST_TInt32 x_old,y_old;

			m_systemCocoa->getCursorPosition(x_old,y_old);
			screenToClient(x_old, y_old, m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
			//Warp position is stored in client (window base) coordinates
			setCursorGrabAccum(0, 0);
			
			if(mode == GHOST_kGrabHide) {
				setWindowCursorVisibility(false);
			}
			
			//Dissociate cursor position even for warp mode, to allow mouse acceleration to work even when warping the cursor
			err = CGAssociateMouseAndMouseCursorPosition(false) == kCGErrorSuccess ? GHOST_kSuccess : GHOST_kFailure;
		}
	}
	else {
		if(m_cursorGrab==GHOST_kGrabHide)
		{
			//No need to set again cursor position, as it has not changed for Cocoa
			setWindowCursorVisibility(true);
		}
		
		err = CGAssociateMouseAndMouseCursorPosition(true) == kCGErrorSuccess ? GHOST_kSuccess : GHOST_kFailure;
		/* Almost works without but important otherwise the mouse GHOST location can be incorrect on exit */
		setCursorGrabAccum(0, 0);
		m_cursorGrabBounds.m_l= m_cursorGrabBounds.m_r= -1; /* disable */
	}
	return err;
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

/** Reverse the bits in a GHOST_TUns8
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
	ch= ((ch>>1)&0x55) | ((ch<<1)&0xAA);
	ch= ((ch>>2)&0x33) | ((ch<<2)&0xCC);
	ch= ((ch>>4)&0x0F) | ((ch<<4)&0xF0);
	return ch;
}
*/


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
	int y,nbUns16;
	NSPoint hotSpotPoint;
	NSBitmapImageRep *cursorImageRep;
	NSImage *cursorImage;
	NSSize imSize;
	GHOST_TUns16 *cursorBitmap;
	
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	if (m_customCursor) {
		[m_customCursor release];
		m_customCursor = nil;
	}
	

	cursorImageRep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
														  	 pixelsWide:sizex
															 pixelsHigh:sizey
														  bitsPerSample:1 
														samplesPerPixel:2
															   hasAlpha:YES
															   isPlanar:YES
														 colorSpaceName:NSDeviceWhiteColorSpace
															bytesPerRow:(sizex/8 + (sizex%8 >0 ?1:0))
														   bitsPerPixel:1];
	
	
	cursorBitmap = (GHOST_TUns16*)[cursorImageRep bitmapData];
	nbUns16 = [cursorImageRep bytesPerPlane]/2;
	
	for (y=0; y<nbUns16; y++) {
#if !defined(__LITTLE_ENDIAN__)
		cursorBitmap[y] = ~uns16ReverseBits((bitmap[2*y]<<0) | (bitmap[2*y+1]<<8));
		cursorBitmap[nbUns16+y] = uns16ReverseBits((mask[2*y]<<0) | (mask[2*y+1]<<8));
#else
		cursorBitmap[y] = ~uns16ReverseBits((bitmap[2*y+1]<<0) | (bitmap[2*y]<<8));
		cursorBitmap[nbUns16+y] = uns16ReverseBits((mask[2*y+1]<<0) | (mask[2*y]<<8));
#endif
		
	}
	
	
	imSize.width = sizex;
	imSize.height= sizey;
	cursorImage = [[NSImage alloc] initWithSize:imSize];
	[cursorImage addRepresentation:cursorImageRep];
	
	hotSpotPoint.x = hotX;
	hotSpotPoint.y = hotY;
	
	//foreground and background color parameter is not handled for now (10.6)
	m_customCursor = [[NSCursor alloc] initWithImage:cursorImage
											 hotSpot:hotSpotPoint];
	
	[cursorImageRep release];
	[cursorImage release];
	
	if ([m_window isVisible]) {
		loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
	}
	[pool drain];
	return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCustomCursorShape(GHOST_TUns8 bitmap[16][2], 
												GHOST_TUns8 mask[16][2], int hotX, int hotY)
{
	return setWindowCustomCursorShape((GHOST_TUns8*)bitmap, (GHOST_TUns8*) mask, 16, 16, hotX, hotY, 0, 1);
}
