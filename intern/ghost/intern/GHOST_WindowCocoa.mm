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
 * Contributor(s): Maarten Gribnau 05/2001
 *                 Damien Plisson  10/2009
 *                 Jason Wilkins   02/2014
 *                 Jens Verwiebe   10/2014
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GHOST_WindowCocoa.h"
#include "GHOST_SystemCocoa.h"
#include "GHOST_ContextNone.h"
#include "GHOST_Debug.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextCGL.h"
#endif

#include <Cocoa/Cocoa.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED <= 1050
   //Use of the SetSystemUIMode function (64bit compatible)
#  include <Carbon/Carbon.h>
#endif

#include <sys/sysctl.h>

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
/* Lion style fullscreen support when building with the 10.6 SDK */
enum {
	NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
	NSFullScreenWindowMask = 1 << 14
};
#endif

#pragma mark Cocoa window delegate object

@interface CocoaWindowDelegate : NSObject
<NSWindowDelegate>
{
	GHOST_SystemCocoa *systemCocoa;
	GHOST_WindowCocoa *associatedWindow;
}

- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;
- (void)windowDidExpose:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
- (void)windowDidMove:(NSNotification *)notification;
- (void)windowWillMove:(NSNotification *)notification;
- (BOOL)windowShouldClose:(id)sender;	
- (void)windowDidChangeBackingProperties:(NSNotification *)notification;
@end

@implementation CocoaWindowDelegate : NSObject
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
	systemCocoa = sysCocoa;
	associatedWindow = winCocoa;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowActivate, associatedWindow);
	// work around for broken appswitching when combining cmd-tab and missioncontrol
	[(NSWindow*)associatedWindow->getOSWindow() orderFrontRegardless];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowDeactivate, associatedWindow);
}

- (void)windowDidExpose:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowUpdate, associatedWindow);
}

- (void)windowDidMove:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowMove, associatedWindow);
}

- (void)windowWillMove:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventWindowMove, associatedWindow);
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
	associatedWindow->setImmediateDraw(true);
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
	associatedWindow->setImmediateDraw(false);
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
	associatedWindow->setImmediateDraw(true);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
	associatedWindow->setImmediateDraw(false);
}

- (void)windowDidResize:(NSNotification *)notification
{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	//if (![[notification object] inLiveResize]) {
		//Send event only once, at end of resize operation (when user has released mouse button)
#endif
		systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	//}
#endif
	/* Live resize, send event, gets handled in wm_window.c. Needed because live resize runs in a modal loop, not letting main loop run */
	 if ([[notification object] inLiveResize]) {
		systemCocoa->dispatchEvents();
	}
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
	systemCocoa->handleWindowEvent(GHOST_kEventNativeResolutionChange, associatedWindow);
}

- (BOOL)windowShouldClose:(id)sender;
{
	//Let Blender close the window rather than closing immediately
	systemCocoa->handleWindowEvent(GHOST_kEventWindowClose, associatedWindow);
	return false;
}

@end

#pragma mark NSWindow subclass
//We need to subclass it to tell that even borderless (fullscreen), it can become key (receive user events)
@interface CocoaWindow: NSWindow
{
	GHOST_SystemCocoa *systemCocoa;
	GHOST_WindowCocoa *associatedWindow;
	GHOST_TDragnDropTypes m_draggedObjectType;
}
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa;
- (GHOST_SystemCocoa*)systemCocoa;
@end

@implementation CocoaWindow
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
	systemCocoa = sysCocoa;
	associatedWindow = winCocoa;
}
- (GHOST_SystemCocoa*)systemCocoa
{
	return systemCocoa;
}

-(BOOL)canBecomeKeyWindow
{
	return YES;
}

//The drag'n'drop dragging destination methods
- (NSDragOperation)draggingEntered:(id < NSDraggingInfo >)sender
{
	NSPoint mouseLocation = [sender draggingLocation];
	NSPasteboard *draggingPBoard = [sender draggingPasteboard];
	
	if ([[draggingPBoard types] containsObject:NSTIFFPboardType]) m_draggedObjectType = GHOST_kDragnDropTypeBitmap;
	else if ([[draggingPBoard types] containsObject:NSFilenamesPboardType]) m_draggedObjectType = GHOST_kDragnDropTypeFilenames;
	else if ([[draggingPBoard types] containsObject:NSStringPboardType]) m_draggedObjectType = GHOST_kDragnDropTypeString;
	else return NSDragOperationNone;
	
	associatedWindow->setAcceptDragOperation(TRUE); //Drag operation is accepted by default
	systemCocoa->handleDraggingEvent(GHOST_kEventDraggingEntered, m_draggedObjectType, associatedWindow, mouseLocation.x, mouseLocation.y, nil);
	return NSDragOperationCopy;
}

- (BOOL)wantsPeriodicDraggingUpdates
{
	return NO; //No need to overflow blender event queue. Events shall be sent only on changes
}

- (NSDragOperation)draggingUpdated:(id < NSDraggingInfo >)sender
{
	NSPoint mouseLocation = [sender draggingLocation];
	
	systemCocoa->handleDraggingEvent(GHOST_kEventDraggingUpdated, m_draggedObjectType, associatedWindow, mouseLocation.x, mouseLocation.y, nil);
	return associatedWindow->canAcceptDragOperation() ? NSDragOperationCopy : NSDragOperationNone;
}

- (void)draggingExited:(id < NSDraggingInfo >)sender
{
	systemCocoa->handleDraggingEvent(GHOST_kEventDraggingExited, m_draggedObjectType, associatedWindow, 0, 0, nil);
	m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
}

- (BOOL)prepareForDragOperation:(id < NSDraggingInfo >)sender
{
	if (associatedWindow->canAcceptDragOperation())
		return YES;
	else
		return NO;
}

- (BOOL)performDragOperation:(id < NSDraggingInfo >)sender
{
	NSPoint mouseLocation = [sender draggingLocation];
	NSPasteboard *draggingPBoard = [sender draggingPasteboard];
	NSImage *droppedImg;
	id data;
	
	switch (m_draggedObjectType) {
		case GHOST_kDragnDropTypeBitmap:
			if ([NSImage canInitWithPasteboard:draggingPBoard]) {
				droppedImg = [[NSImage alloc]initWithPasteboard:draggingPBoard];
				data = droppedImg; //[draggingPBoard dataForType:NSTIFFPboardType];
			}
			else return NO;
			break;
		case GHOST_kDragnDropTypeFilenames:
			data = [draggingPBoard propertyListForType:NSFilenamesPboardType];
			break;
		case GHOST_kDragnDropTypeString:
			data = [draggingPBoard stringForType:NSStringPboardType];
			break;
		default:
			return NO;
			break;
	}
	systemCocoa->handleDraggingEvent(GHOST_kEventDraggingDropDone, m_draggedObjectType, associatedWindow, mouseLocation.x, mouseLocation.y, (void*)data);
	return YES;
}

@end

#pragma mark NSOpenGLView subclass
//We need to subclass it in order to give Cocoa the feeling key events are trapped
@interface CocoaOpenGLView : NSOpenGLView <NSTextInput>
{
	GHOST_SystemCocoa *systemCocoa;
	GHOST_WindowCocoa *associatedWindow;

	bool composing;
	NSString *composing_text;

	bool immediate_draw;
}
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa;
@end

@implementation CocoaOpenGLView

- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
	systemCocoa = sysCocoa;
	associatedWindow = winCocoa;

	composing = false;
	composing_text = nil;

	immediate_draw = false;
}

- (BOOL)acceptsFirstResponder
{
	return YES;
}

// The trick to prevent Cocoa from complaining (beeping)
- (void)keyDown:(NSEvent *)event
{
	systemCocoa->handleKeyEvent(event);

	/* Start or continue composing? */
	if ([[event characters] length] == 0  ||
	    [[event charactersIgnoringModifiers] length] == 0 ||
	    composing)
	{
		composing = YES;

		// interpret event to call insertText
		NSMutableArray *events;
		events = [[NSMutableArray alloc] initWithCapacity:1];
		[events addObject:event];
		[self interpretKeyEvents:events]; // calls insertText
		[events removeObject:event];
		[events release];
		return;
	}
}

- (void)keyUp:(NSEvent *)event
{
	systemCocoa->handleKeyEvent(event);
}

- (void)flagsChanged:(NSEvent *)event
{
	systemCocoa->handleKeyEvent(event);
}

- (void)mouseDown:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)mouseUp:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)rightMouseDown:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)rightMouseUp:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)mouseMoved:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)mouseDragged:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)rightMouseDragged:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)scrollWheel:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)otherMouseDown:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)otherMouseUp:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)otherMouseDragged:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)magnifyWithEvent:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)rotateWithEvent:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)beginGestureWithEvent:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)endGestureWithEvent:(NSEvent *)event
{
	systemCocoa->handleMouseEvent(event);
}

- (void)tabletPoint:(NSEvent *)event
{
	systemCocoa->handleTabletEvent(event,[event type]);
}

- (void)tabletProximity:(NSEvent *)event
{
	systemCocoa->handleTabletEvent(event,[event type]);
}

- (BOOL)isOpaque
{
	return YES;
}

- (void) drawRect:(NSRect)rect
{
	if ([self inLiveResize]) {
		/* Don't redraw while in live resize */
	}
	else {
		[super drawRect:rect];
		systemCocoa->handleWindowEvent(GHOST_kEventWindowUpdate, associatedWindow);

		/* For some cases like entering fullscreen we need to redraw immediately
		 * so our window does not show blank during the animation */
		if (associatedWindow->getImmediateDraw())
			systemCocoa->dispatchEvents();
	}
}

// Text input

- (void)composing_free
{
	composing = NO;

	if (composing_text) {
		[composing_text release];
		composing_text = nil;
	}
}

- (void)insertText:(id)chars
{
	[self composing_free];
}

- (void)setMarkedText:(id)chars selectedRange:(NSRange)range
{
	[self composing_free];
	if ([chars length] == 0)
		return;

	// start composing
	composing = YES;
	composing_text = [chars copy];

	// if empty, cancel
	if ([composing_text length] == 0)
		[self composing_free];
}

- (void)unmarkText
{
	[self composing_free];
}

- (BOOL)hasMarkedText
{
	return (composing) ? YES : NO;
}

- (void)doCommandBySelector:(SEL)selector
{
}

- (BOOL)isComposing
{
	return composing;
}

- (NSInteger)conversationIdentifier
{
	return (NSInteger)self;
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)range
{
	return [NSAttributedString new]; // XXX does this leak?
}

- (NSRange)markedRange
{
	unsigned int length = (composing_text) ? [composing_text length] : 0;

	if (composing)
		return NSMakeRange(0, length);

	return NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange
{
	unsigned int length = (composing_text) ? [composing_text length] : 0;
	return NSMakeRange(0, length);
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
{
	return NSZeroRect;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
	return NSNotFound;
}

- (NSArray*)validAttributesForMarkedText
{
	return [NSArray array]; // XXX does this leak?
}

@end

#pragma mark initialization / finalization

#if MAC_OS_X_VERSION_MAX_ALLOWED < 1070
@interface NSView (NSOpenGLSurfaceResolution)
- (BOOL)wantsBestResolutionOpenGLSurface;
- (void)setWantsBestResolutionOpenGLSurface:(BOOL)flag;
- (NSRect)convertRectToBacking:(NSRect)bounds;
@end
#endif

GHOST_WindowCocoa::GHOST_WindowCocoa(
	GHOST_SystemCocoa *systemCocoa,
	const STR_String& title,
	GHOST_TInt32 left,
	GHOST_TInt32 bottom,
	GHOST_TUns32 width,
	GHOST_TUns32 height,
	GHOST_TWindowState state,
	GHOST_TDrawingContextType type,
	const bool stereoVisual, const GHOST_TUns16 numOfAASamples
) :
	GHOST_Window(width, height, state, stereoVisual, false, numOfAASamples),
	m_customCursor(0)
{
	m_systemCocoa = systemCocoa;
	m_fullScreen = false;
	m_immediateDraw = false;
	m_lionStyleFullScreen = false;

	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	//Creates the window
	NSRect rect;
	NSSize	minSize;
	
	rect.origin.x = left;
	rect.origin.y = bottom;
	rect.size.width = width;
	rect.size.height = height;
	
	m_window = [[CocoaWindow alloc] initWithContentRect:rect
	        styleMask:NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask
	        backing:NSBackingStoreBuffered defer:NO];

	if (m_window == nil) {
		[pool drain];
		return;
	}
	
	[m_window setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
	
	//Forbid to resize the window below the blender defined minimum one
	minSize.width = 320;
	minSize.height = 240;
	[m_window setContentMinSize:minSize];
	
	//Creates the OpenGL View inside the window
	m_openGLView = [[CocoaOpenGLView alloc] initWithFrame:rect];
	
	[m_openGLView setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
	
	[m_window setContentView:m_openGLView];
	[m_window setInitialFirstResponder:m_openGLView];
	
	[m_window makeKeyAndOrderFront:nil];
	
	setDrawingContextType(type);
	updateDrawingContext();
	activateDrawingContext();

	// XXX jwilkins: This seems like it belongs in GHOST_ContextCGL, but probably not GHOST_ContextEGL
	if (m_systemCocoa->m_nativePixel) {
		if ([m_openGLView respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
			[m_openGLView setWantsBestResolutionOpenGLSurface:YES];
		
			NSRect backingBounds = [m_openGLView convertRectToBacking:[m_openGLView bounds]];
			m_nativePixelSize = (float)backingBounds.size.width / (float)rect.size.width;
		}
	}
	
	setTitle(title);
	
	m_tablet.Active = GHOST_kTabletModeNone;
	
	CocoaWindowDelegate *windowDelegate = [[CocoaWindowDelegate alloc] init];
	[windowDelegate setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
	[m_window setDelegate:windowDelegate];
	
	[m_window setAcceptsMouseMovedEvents:YES];
	
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	NSView *view = [m_window contentView];
	[view setAcceptsTouchEvents:YES];
#endif
	
	[m_window registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
	                                   NSStringPboardType, NSTIFFPboardType, nil]];
	
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	if (state != GHOST_kWindowStateFullScreen) {
		[m_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
	}
#endif
	
	if (state == GHOST_kWindowStateFullScreen)
		setState(GHOST_kWindowStateFullScreen);

	// Starting with 10.9 (darwin 13.x.x), we can use Lion fullscreen,
	// since it now has better multi-monitor support
	// if the screens are spawned, additional screens get useless,
	// so we only use lionStyleFullScreen when screens have separate spaces
	
	if ([NSScreen respondsToSelector:@selector(screensHaveSeparateSpaces)] && [NSScreen screensHaveSeparateSpaces]) {
		// implies we are on >= OSX 10.9
		m_lionStyleFullScreen = true;
	}
	
	[NSApp activateIgnoringOtherApps:YES]; // raise application to front, important for new blender instance animation play case
	
	[pool drain];
}


GHOST_WindowCocoa::~GHOST_WindowCocoa()
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	if (m_customCursor) {
		[m_customCursor release];
		m_customCursor = nil;
	}

	releaseNativeHandles();

	[m_openGLView release];
	
	if (m_window) {
		[m_window close];
	}
	
	// Check for other blender opened windows and make the frontmost key
	// Note: for some reason the closed window is still in the list
	NSArray *windowsList = [NSApp orderedWindows];
	for (int a = 0; a < [windowsList count]; a++) {
		if (m_window != (CocoaWindow *)[windowsList objectAtIndex:a]) {
			[[windowsList objectAtIndex:a] makeKeyWindow];
			break;
		}
	}
	m_window = nil;

	[pool drain];
}

#pragma mark accessors

bool GHOST_WindowCocoa::getValid() const
{
	return GHOST_Window::getValid() && m_window != 0 && m_openGLView != 0;
}

void* GHOST_WindowCocoa::getOSWindow() const
{
	return (void*)m_window;
}

void GHOST_WindowCocoa::setTitle(const STR_String& title)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setTitle(): window invalid");
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	NSString *windowTitle = [[NSString alloc] initWithCString:title encoding:NSUTF8StringEncoding];
	
	//Set associated file if applicable
	if (windowTitle && [windowTitle hasPrefix:@"Blender"]) {
		NSRange fileStrRange;
		NSString *associatedFileName;
		int len;
		
		fileStrRange.location = [windowTitle rangeOfString:@"["].location+1;
		len = [windowTitle rangeOfString:@"]"].location - fileStrRange.location;
	
		if (len > 0) {
			fileStrRange.length = len;
			associatedFileName = [windowTitle substringWithRange:fileStrRange];
			[m_window setTitle:[associatedFileName lastPathComponent]];

			//Blender used file open/save functions converte file names into legal URL ones
			associatedFileName = [associatedFileName stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
			@try {
				[m_window setRepresentedFilename:associatedFileName];
			}
			@catch (NSException * e) {
				printf("\nInvalid file path given in window title");
			}
		}
		else {
			[m_window setTitle:windowTitle];
			[m_window setRepresentedFilename:@""];
		}

	}
	else {
		[m_window setTitle:windowTitle];
		[m_window setRepresentedFilename:@""];
	}

	
	[windowTitle release];
	[pool drain];
}


void GHOST_WindowCocoa::getTitle(STR_String& title) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getTitle(): window invalid");

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
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getWindowBounds(): window invalid");

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
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getClientBounds(): window invalid");
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	if (!m_fullScreen) {
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
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientWidth(): window invalid");
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (((GHOST_TUns32)cBnds.getWidth()) != width) {
		NSSize size;
		size.width=width;
		size.height=cBnds.getHeight();
		[m_window setContentSize:size];
	}
	[pool drain];
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCocoa::setClientHeight(GHOST_TUns32 height)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientHeight(): window invalid");
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if (((GHOST_TUns32)cBnds.getHeight()) != height) {
		NSSize size;
		size.width=cBnds.getWidth();
		size.height=height;
		[m_window setContentSize:size];
	}
	[pool drain];
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCocoa::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientSize(): window invalid");
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	GHOST_Rect cBnds, wBnds;
	getClientBounds(cBnds);
	if ((((GHOST_TUns32)cBnds.getWidth())  != width) ||
	    (((GHOST_TUns32)cBnds.getHeight()) != height))
	{
		NSSize size;
		size.width=width;
		size.height=height;
		[m_window setContentSize:size];
	}
	[pool drain];
	return GHOST_kSuccess;
}


GHOST_TWindowState GHOST_WindowCocoa::getState() const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getState(): window invalid");
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	GHOST_TWindowState state;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
	NSUInteger masks = [m_window styleMask];

	if (masks & NSFullScreenWindowMask) {
		// Lion style fullscreen
		if (!m_immediateDraw) {
			state = GHOST_kWindowStateFullScreen;
		}
		else {
			state = GHOST_kWindowStateNormal;
		}
	}
	else
#endif
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
		if (m_immediateDraw) {
			state = GHOST_kWindowStateFullScreen;
		}
		else {
			state = GHOST_kWindowStateNormal;
		}
	}
	[pool drain];
	return state;
}


void GHOST_WindowCocoa::screenToClient(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::screenToClient(): window invalid");

	screenToClientIntern(inX, inY, outX, outY);

	/* switch y to match ghost convention */
	GHOST_Rect cBnds;
	getClientBounds(cBnds);
	outY = (cBnds.getHeight() - 1) - outY;
}


void GHOST_WindowCocoa::clientToScreen(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::clientToScreen(): window invalid");

	/* switch y to match ghost convention */
	GHOST_Rect cBnds;
	getClientBounds(cBnds);
	inY = (cBnds.getHeight() - 1) - inY;

	clientToScreenIntern(inX, inY, outX, outY);
}

void GHOST_WindowCocoa::screenToClientIntern(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
	NSPoint screenCoord;
	NSPoint baseCoord;
	
	screenCoord.x = inX;
	screenCoord.y = inY;
	
	baseCoord = [m_window convertScreenToBase:screenCoord];
	
	outX = baseCoord.x;
	outY = baseCoord.y;
}

void GHOST_WindowCocoa::clientToScreenIntern(GHOST_TInt32 inX, GHOST_TInt32 inY, GHOST_TInt32& outX, GHOST_TInt32& outY) const
{
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

/* called for event, when window leaves monitor to another */
void GHOST_WindowCocoa::setNativePixelSize(void)
{
	/* make sure 10.6 keeps running */
	if ([m_openGLView respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
		NSRect backingBounds = [m_openGLView convertRectToBacking:[m_openGLView bounds]];
		
		GHOST_Rect rect;
		getClientBounds(rect);

		m_nativePixelSize = (float)backingBounds.size.width / (float)rect.getWidth();
	}
}

/**
 * \note Fullscreen switch is not actual fullscreen with display capture.
 * As this capture removes all OS X window manager features.
 *
 * Instead, the menu bar and the dock are hidden, and the window is made borderless and enlarged.
 * Thus, process switch, exposé, spaces, ... still work in fullscreen mode
 */
GHOST_TSuccess GHOST_WindowCocoa::setState(GHOST_TWindowState state)
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setState(): window invalid");
	switch (state) {
		case GHOST_kWindowStateMinimized:
			[m_window miniaturize:nil];
			break;
		case GHOST_kWindowStateMaximized:
			[m_window zoom:nil];
			break;
		
		case GHOST_kWindowStateFullScreen:
		{
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
			NSUInteger masks = [m_window styleMask];

			if (!m_fullScreen && !(masks & NSFullScreenWindowMask)) {
				if (m_lionStyleFullScreen) {
					[m_window toggleFullScreen:nil];
					break;
				}
#else
			if (!m_fullScreen) {
#endif
				NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
			
				/* This status change needs to be done before Cocoa call to enter fullscreen mode
				 * to give window delegate hint not to forward its deactivation to ghost wm that
				 * doesn't know view/window difference. */
				m_fullScreen = true;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
				/* Disable toggle for Lion style fullscreen */
				[m_window setCollectionBehavior:NSWindowCollectionBehaviorDefault];
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
				//10.6 provides Cocoa functions to autoshow menu bar, and to change a window style
				//Hide menu & dock if on primary screen. else only menu
				if ([[m_window screen] isEqual:[[NSScreen screens] objectAtIndex:0]]) {
					[NSApp setPresentationOptions:(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar)];
				}
				//Make window borderless and enlarge it
				[m_window setStyleMask:NSBorderlessWindowMask];
				[m_window setFrame:[[m_window screen] frame] display:YES];
				[m_window makeFirstResponder:m_openGLView];
#else
				//With 10.5, we need to create a new window to change its style to borderless
				//Hide menu & dock if needed
				if ([[m_window screen] isEqual:[[NSScreen screens] objectAtIndex:0]]) {
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
				[tmpWindow setAcceptsMouseMovedEvents:YES];
				[tmpWindow setDelegate:[m_window delegate]];
				[tmpWindow setSystemAndWindowCocoa:[m_window systemCocoa] windowCocoa:this];
				[tmpWindow registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
				                                    NSStringPboardType, NSTIFFPboardType, nil]];
				
				//Assign the openGL view to the new window
				[tmpWindow setContentView:m_openGLView];
				
				//Show the new window
				[tmpWindow makeKeyAndOrderFront:m_openGLView];
				//Close and release old window
				[m_window close];
				m_window = tmpWindow;
#endif
			
				//Tell WM of view new size
				m_systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, this);
				
				[pool drain];
			}
			break;
		}
		case GHOST_kWindowStateNormal:
		default:
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
			NSUInteger masks = [m_window styleMask];

			if (masks & NSFullScreenWindowMask) {
				// Lion style fullscreen
				[m_window toggleFullScreen:nil];
			}
			else
#endif
			if (m_fullScreen) {
				m_fullScreen = false;

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
				/* Enable toggle for into Lion style fullscreen */
				[m_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
#endif

				//Exit fullscreen
#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
				//Show again menu & dock if needed
				if ([[m_window screen] isEqual:[NSScreen mainScreen]]) {
					[NSApp setPresentationOptions:NSApplicationPresentationDefault];
				}
				//Make window normal and resize it
				[m_window setStyleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask)];
				[m_window setFrame:[[m_window screen] visibleFrame] display:YES];
				//TODO for 10.6 only : window title is forgotten after the style change
				[m_window makeFirstResponder:m_openGLView];
#else
				//With 10.5, we need to create a new window to change its style to borderless
				//Show menu & dock if needed
				if ([[m_window screen] isEqual:[NSScreen mainScreen]]) {
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
				[tmpWindow setAcceptsMouseMovedEvents:YES];
				[tmpWindow setDelegate:[m_window delegate]];
				[tmpWindow setSystemAndWindowCocoa:[m_window systemCocoa] windowCocoa:this];
				[tmpWindow registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
				                                    NSStringPboardType, NSTIFFPboardType, nil]];
				//Forbid to resize the window below the blender defined minimum one
				[tmpWindow setContentMinSize:NSMakeSize(320, 240)];
				
				//Assign the openGL view to the new window
				[tmpWindow setContentView:m_openGLView];
				
				//Show the new window
				[tmpWindow makeKeyAndOrderFront:nil];
				//Close and release old window
				[m_window close];
				m_window = tmpWindow;
#endif
			
				//Tell WM of view new size
				m_systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, this);
			}
			else if ([m_window isMiniaturized])
				[m_window deminiaturize:nil];
			else if ([m_window isZoomed])
				[m_window zoom:nil];
			[pool drain];
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
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setOrder(): window invalid");
	if (order == GHOST_kWindowOrderTop) {
		[m_window makeKeyAndOrderFront:nil];
	}
	else {
		NSArray *windowsList;
		
		[m_window orderBack:nil];
		
		//Check for other blender opened windows and make the frontmost key
		windowsList = [NSApp orderedWindows];
		if ([windowsList count]) {
			[[windowsList objectAtIndex:0] makeKeyAndOrderFront:nil];
		}
	}
	
	[pool drain];
	return GHOST_kSuccess;
}

#pragma mark Drawing context

GHOST_Context *GHOST_WindowCocoa::newDrawingContext(GHOST_TDrawingContextType type)
{
	if (type == GHOST_kDrawingContextTypeOpenGL) {
#if !defined(WITH_GL_EGL)

#if defined(WITH_GL_PROFILE_CORE)
		GHOST_Context *context = new GHOST_ContextCGL(
			m_initStereoVisual,
			m_initNumOfAASamples,
			m_window,
			m_openGLView,
			CGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
			3, 2,
			GHOST_OPENGL_CGL_CONTEXT_FLAGS,
			GHOST_OPENGL_CGL_RESET_NOTIFICATION_STRATEGY);
#elif defined(WITH_GL_PROFILE_ES20)
		GHOST_Context *context = new GHOST_ContextCGL(
			m_initStereoVisual,
			m_initNumOfAASamples,
			m_window,
			m_openGLView,
			CGL_CONTEXT_ES2_PROFILE_BIT_EXT,
			2, 0,
			GHOST_OPENGL_CGL_CONTEXT_FLAGS,
			GHOST_OPENGL_CGL_RESET_NOTIFICATION_STRATEGY);
#elif defined(WITH_GL_PROFILE_COMPAT)
		GHOST_Context *context = new GHOST_ContextCGL(
			m_wantStereoVisual,
			m_wantNumOfAASamples,
			m_window,
			m_openGLView,
			0, // profile bit
			0, 0,
			GHOST_OPENGL_CGL_CONTEXT_FLAGS,
			GHOST_OPENGL_CGL_RESET_NOTIFICATION_STRATEGY);
#else
#  error
#endif

#else

#if defined(WITH_GL_PROFILE_CORE)
		GHOST_Context *context = new GHOST_ContextEGL(
			m_wantStereoVisual,
			m_wantNumOfAASamples,
			m_window,
			m_openGLView,
			EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
			3, 2,
			GHOST_OPENGL_EGL_CONTEXT_FLAGS,
			GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
			EGL_OPENGL_API);
#elif defined(WITH_GL_PROFILE_ES20)
		GHOST_Context *context = new GHOST_ContextEGL(
			m_wantStereoVisual,
			m_wantNumOfAASamples,
			m_window,
			m_openGLView,
			0, // profile bit
			2, 0,
			GHOST_OPENGL_EGL_CONTEXT_FLAGS,
			GHOST_OPENGL_EGL_RESET_NOTIFICATION_STRATEGY,
			EGL_OPENGL_ES_API);
#elif defined(WITH_GL_PROFILE_COMPAT)
		GHOST_Context *context = new GHOST_ContextEGL(
			m_wantStereoVisual,
			m_wantNumOfAASamples,
			m_window,
			m_openGLView,
			0, // profile bit
			0, 0,
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

#pragma mark invalidate

GHOST_TSuccess GHOST_WindowCocoa::invalidate()
{
	GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::invalidate(): window invalid");
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	[m_openGLView setNeedsDisplay:YES];
	[pool drain];
	return GHOST_kSuccess;
}

#pragma mark Progress bar

GHOST_TSuccess GHOST_WindowCocoa::setProgressBar(float progress)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	if ((progress >=0.0) && (progress <=1.0)) {
		NSImage* dockIcon = [[NSImage alloc] initWithSize:NSMakeSize(128,128)];
		
		[dockIcon lockFocus];
		NSRect progressBox = {{4, 4}, {120, 16}};

		[[NSImage imageNamed:@"NSApplicationIcon"] drawAtPoint:NSZeroPoint fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];

		// Track & Outline
		[[NSColor blackColor] setFill];
		NSRectFill(progressBox);

		[[NSColor whiteColor] set];
		NSFrameRect(progressBox);

		// Progress fill
		progressBox = NSInsetRect(progressBox, 1, 1);

		progressBox.size.width = progressBox.size.width * progress;
		NSGradient *gradient = [[NSGradient alloc] initWithStartingColor:[NSColor darkGrayColor] endingColor:[NSColor lightGrayColor]];
		[gradient drawInRect:progressBox angle:90];
		[gradient release];
		
		[dockIcon unlockFocus];
		
		[NSApp setApplicationIconImage:dockIcon];
		[dockIcon release];
		
		m_progressBarVisible = true;
	}
	
	[pool drain];
	return GHOST_kSuccess;
}

static void postNotification()
{
	NSUserNotification *notification = [[NSUserNotification alloc] init];
	notification.title = @"Blender progress notification";
	notification.informativeText = @"Calculation is finished";
	notification.soundName = NSUserNotificationDefaultSoundName;
	[[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
	[notification release];
}
	
GHOST_TSuccess GHOST_WindowCocoa::endProgressBar()
{
	if (!m_progressBarVisible) return GHOST_kFailure;
	m_progressBarVisible = false;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSImage* dockIcon = [[NSImage alloc] initWithSize:NSMakeSize(128,128)];
	[dockIcon lockFocus];
	[[NSImage imageNamed:@"NSApplicationIcon"] drawAtPoint:NSZeroPoint fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
	[dockIcon unlockFocus];
	[NSApp setApplicationIconImage:dockIcon];
	
	
	// With OSX 10.8 and later, we can use notifications to inform the user when the progress reached 100%
	// Atm. just fire this when the progressbar ends, the behavior is controlled in the NotificationCenter
	// If Blender is not frontmost window, a message pops up with sound, in any case an entry in notifications
	
	if ([NSUserNotificationCenter respondsToSelector:@selector(defaultUserNotificationCenter)]) {
		postNotification();
	}
	
	[dockIcon release];
	
	[pool drain];
	return GHOST_kSuccess;
}

#pragma mark Cursor handling

void GHOST_WindowCocoa::loadCursor(bool visible, GHOST_TStandardCursor cursor) const
{
	static bool systemCursorVisible = true;
	
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
	}
	else {
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
			case GHOST_kStandardCursorCopy:
			case GHOST_kStandardCursorDefault:
			default:
				tmpCursor = [NSCursor arrowCursor];
				break;
		};
	}
	[tmpCursor set];
}



GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorVisibility(bool visible)
{
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc]init];
	
	if ([m_window isVisible]) {
		loadCursor(visible, getCursorShape());
	}
	
	[pool drain];
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
	GHOST_TSuccess err = GHOST_kSuccess;
	
	if (mode != GHOST_kGrabDisable) {
		//No need to perform grab without warp as it is always on in OS X
		if (mode != GHOST_kGrabNormal) {
			GHOST_TInt32 x_old,y_old;
			NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

			m_systemCocoa->getCursorPosition(x_old,y_old);
			screenToClientIntern(x_old, y_old, m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
			//Warp position is stored in client (window base) coordinates
			setCursorGrabAccum(0, 0);
			
			if (mode == GHOST_kGrabHide) {
				setWindowCursorVisibility(false);
			}
			
			//Make window key if it wasn't to get the mouse move events
			[m_window makeKeyWindow];
			
			//Dissociate cursor position even for warp mode, to allow mouse acceleration to work even when warping the cursor
			err = CGAssociateMouseAndMouseCursorPosition(false) == kCGErrorSuccess ? GHOST_kSuccess : GHOST_kFailure;
			
			[pool drain];
		}
	}
	else {
		if (m_cursorGrab==GHOST_kGrabHide) {
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
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

	if (m_customCursor) {
		[m_customCursor release];
		m_customCursor = nil;
	}

	if ([m_window isVisible]) {
		loadCursor(getCursorVisibility(), shape);
	}
	
	[pool drain];
	return GHOST_kSuccess;
}

/** Reverse the bits in a GHOST_TUns8
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
	ch= ((ch >> 1) & 0x55) | ((ch << 1) & 0xAA);
	ch= ((ch >> 2) & 0x33) | ((ch << 2) & 0xCC);
	ch= ((ch >> 4) & 0x0F) | ((ch << 4) & 0xF0);
	return ch;
}
*/


/** Reverse the bits in a GHOST_TUns16 */
static GHOST_TUns16 uns16ReverseBits(GHOST_TUns16 shrt)
{
	shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
	shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
	shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
	shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
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
