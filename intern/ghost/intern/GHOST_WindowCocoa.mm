/*
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
 */

#include "GHOST_WindowCocoa.h"
#include "GHOST_ContextNone.h"
#include "GHOST_Debug.h"
#include "GHOST_SystemCocoa.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextCGL.h"
#endif

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <sys/sysctl.h>

/* clang-format off */

#pragma mark Cocoa window delegate object

@interface CocoaWindowDelegate : NSObject <NSWindowDelegate>
{
  GHOST_SystemCocoa *systemCocoa;
  GHOST_WindowCocoa *associatedWindow;
}

- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa
                    windowCocoa:(GHOST_WindowCocoa *)winCocoa;
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
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa
                    windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
  systemCocoa = sysCocoa;
  associatedWindow = winCocoa;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  systemCocoa->handleWindowEvent(GHOST_kEventWindowActivate, associatedWindow);
  // work around for broken appswitching when combining cmd-tab and missioncontrol
  [(NSWindow *)associatedWindow->getOSWindow() orderFrontRegardless];
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
  /* macOS does not send a window resize event when switching between zoomed
   * and fullscreen, when automatic show/hide of dock and menu bar are enabled.
   * Send our own to prevent artifacts. */
  systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);

  associatedWindow->setImmediateDraw(false);
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
  associatedWindow->setImmediateDraw(true);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
  /* See comment for windowWillEnterFullScreen. */
  systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);
  associatedWindow->setImmediateDraw(false);
}

- (void)windowDidResize:(NSNotification *)notification
{
  //if (![[notification object] inLiveResize]) {
  //Send event only once, at end of resize operation (when user has released mouse button)
  systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);
  //}
  /* Live resize, send event, gets handled in wm_window.c.
   * Needed because live resize runs in a modal loop, not letting main loop run */
  if ([[notification object] inLiveResize]) {
    systemCocoa->dispatchEvents();
  }
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
  systemCocoa->handleWindowEvent(GHOST_kEventNativeResolutionChange, associatedWindow);
  systemCocoa->handleWindowEvent(GHOST_kEventWindowSize, associatedWindow);
}

- (BOOL)windowShouldClose:(id)sender;
{
  //Let Blender close the window rather than closing immediately
  systemCocoa->handleWindowEvent(GHOST_kEventWindowClose, associatedWindow);
  return false;
}

@end

#pragma mark NSWindow subclass
// We need to subclass it to tell that even borderless (fullscreen),
// it can become key (receive user events)
@interface CocoaWindow : NSWindow
{
  GHOST_SystemCocoa *systemCocoa;
  GHOST_WindowCocoa *associatedWindow;
  GHOST_TDragnDropTypes m_draggedObjectType;
}
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa
                    windowCocoa:(GHOST_WindowCocoa *)winCocoa;
- (GHOST_SystemCocoa *)systemCocoa;
@end

@implementation CocoaWindow
- (void)setSystemAndWindowCocoa:(GHOST_SystemCocoa *)sysCocoa
                    windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
  systemCocoa = sysCocoa;
  associatedWindow = winCocoa;
}
- (GHOST_SystemCocoa *)systemCocoa
{
  return systemCocoa;
}

- (BOOL)canBecomeKeyWindow
{
  /* Don't make other windows active when a dialog window is open. */
  return (associatedWindow->isDialog() || !systemCocoa->hasDialogWindow());
}

//The drag'n'drop dragging destination methods
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
  NSPoint mouseLocation = [sender draggingLocation];
  NSPasteboard *draggingPBoard = [sender draggingPasteboard];

  if ([[draggingPBoard types] containsObject:NSTIFFPboardType])
    m_draggedObjectType = GHOST_kDragnDropTypeBitmap;
  else if ([[draggingPBoard types] containsObject:NSFilenamesPboardType])
    m_draggedObjectType = GHOST_kDragnDropTypeFilenames;
  else if ([[draggingPBoard types] containsObject:NSStringPboardType])
    m_draggedObjectType = GHOST_kDragnDropTypeString;
  else
    return NSDragOperationNone;

  associatedWindow->setAcceptDragOperation(TRUE);  //Drag operation is accepted by default
  systemCocoa->handleDraggingEvent(GHOST_kEventDraggingEntered,
                                   m_draggedObjectType,
                                   associatedWindow,
                                   mouseLocation.x,
                                   mouseLocation.y,
                                   nil);
  return NSDragOperationCopy;
}

- (BOOL)wantsPeriodicDraggingUpdates
{
  return NO;  //No need to overflow blender event queue. Events shall be sent only on changes
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
  NSPoint mouseLocation = [sender draggingLocation];

  systemCocoa->handleDraggingEvent(GHOST_kEventDraggingUpdated,
                                   m_draggedObjectType,
                                   associatedWindow,
                                   mouseLocation.x,
                                   mouseLocation.y,
                                   nil);
  return associatedWindow->canAcceptDragOperation() ? NSDragOperationCopy : NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender
{
  systemCocoa->handleDraggingEvent(
      GHOST_kEventDraggingExited, m_draggedObjectType, associatedWindow, 0, 0, nil);
  m_draggedObjectType = GHOST_kDragnDropTypeUnknown;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender
{
  if (associatedWindow->canAcceptDragOperation())
    return YES;
  else
    return NO;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
  NSPoint mouseLocation = [sender draggingLocation];
  NSPasteboard *draggingPBoard = [sender draggingPasteboard];
  NSImage *droppedImg;
  id data;

  switch (m_draggedObjectType) {
    case GHOST_kDragnDropTypeBitmap:
      if ([NSImage canInitWithPasteboard:draggingPBoard]) {
        droppedImg = [[NSImage alloc] initWithPasteboard:draggingPBoard];
        data = droppedImg;  //[draggingPBoard dataForType:NSTIFFPboardType];
      }
      else
        return NO;
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
  systemCocoa->handleDraggingEvent(GHOST_kEventDraggingDropDone,
                                   m_draggedObjectType,
                                   associatedWindow,
                                   mouseLocation.x,
                                   mouseLocation.y,
                                   (void *)data);
  return YES;
}

@end

/* NSView for handling input and drawing. */
#define COCOA_VIEW_CLASS CocoaOpenGLView
#define COCOA_VIEW_BASE_CLASS NSOpenGLView
#include "GHOST_WindowViewCocoa.h"
#undef COCOA_VIEW_CLASS
#undef COCOA_VIEW_BASE_CLASS

#define COCOA_VIEW_CLASS CocoaMetalView
#define COCOA_VIEW_BASE_CLASS NSView
#include "GHOST_WindowViewCocoa.h"
#undef COCOA_VIEW_CLASS
#undef COCOA_VIEW_BASE_CLASS

#pragma mark initialization / finalization

/* clang-format on */

GHOST_WindowCocoa::GHOST_WindowCocoa(GHOST_SystemCocoa *systemCocoa,
                                     const char *title,
                                     GHOST_TInt32 left,
                                     GHOST_TInt32 bottom,
                                     GHOST_TUns32 width,
                                     GHOST_TUns32 height,
                                     GHOST_TWindowState state,
                                     GHOST_TDrawingContextType type,
                                     const bool stereoVisual,
                                     bool is_debug,
                                     bool is_dialog,
                                     GHOST_WindowCocoa *parentWindow)
    : GHOST_Window(width, height, state, stereoVisual, false),
      m_openGLView(nil),
      m_metalView(nil),
      m_metalLayer(nil),
      m_systemCocoa(systemCocoa),
      m_customCursor(0),
      m_immediateDraw(false),
      m_debug_context(is_debug),
      m_is_dialog(is_dialog)
{
  m_fullScreen = false;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // Creates the window
  NSRect rect;
  NSSize minSize;

  rect.origin.x = left;
  rect.origin.y = bottom;
  rect.size.width = width;
  rect.size.height = height;

  NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                NSWindowStyleMaskResizable;
  if (!is_dialog) {
    styleMask |= NSWindowStyleMaskMiniaturizable;
  }

  m_window = [[CocoaWindow alloc] initWithContentRect:rect
                                            styleMask:styleMask
                                              backing:NSBackingStoreBuffered
                                                defer:NO];

  if (m_window == nil) {
    [pool drain];
    return;
  }

  [m_window setSystemAndWindowCocoa:systemCocoa windowCocoa:this];

  // Forbid to resize the window below the blender defined minimum one
  minSize.width = 320;
  minSize.height = 240;
  [m_window setContentMinSize:minSize];

  // Create NSView inside the window
  id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
  NSView *view;

  if (metalDevice) {
    // Create metal layer and view if supported
    m_metalLayer = [[CAMetalLayer alloc] init];
    [m_metalLayer setEdgeAntialiasingMask:0];
    [m_metalLayer setMasksToBounds:NO];
    [m_metalLayer setOpaque:YES];
    [m_metalLayer setFramebufferOnly:YES];
    [m_metalLayer setPresentsWithTransaction:NO];
    [m_metalLayer removeAllAnimations];
    [m_metalLayer setDevice:metalDevice];

    m_metalView = [[CocoaMetalView alloc] initWithFrame:rect];
    [m_metalView setWantsLayer:YES];
    [m_metalView setLayer:m_metalLayer];
    [m_metalView setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
    view = m_metalView;
  }
  else {
    // Fallback to OpenGL view if there is no Metal support
    m_openGLView = [[CocoaOpenGLView alloc] initWithFrame:rect];
    [m_openGLView setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
    view = m_openGLView;
  }

  if (m_systemCocoa->m_nativePixel) {
    // Needs to happen early when building with the 10.14 SDK, otherwise
    // has no effect until resizeing the window.
    if ([view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
      [view setWantsBestResolutionOpenGLSurface:YES];
    }
  }

  [m_window setContentView:view];
  [m_window setInitialFirstResponder:view];

  [m_window makeKeyAndOrderFront:nil];

  setDrawingContextType(type);
  updateDrawingContext();
  activateDrawingContext();

  setTitle(title);

  m_tablet = GHOST_TABLET_DATA_NONE;

  CocoaWindowDelegate *windowDelegate = [[CocoaWindowDelegate alloc] init];
  [windowDelegate setSystemAndWindowCocoa:systemCocoa windowCocoa:this];
  [m_window setDelegate:windowDelegate];

  [m_window setAcceptsMouseMovedEvents:YES];

  NSView *contentview = [m_window contentView];
  [contentview setAcceptsTouchEvents:YES];

  [m_window registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
                                                              NSStringPboardType,
                                                              NSTIFFPboardType,
                                                              nil]];

  if (is_dialog && parentWindow) {
    [parentWindow->getCocoaWindow() addChildWindow:m_window ordered:NSWindowAbove];
    [m_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenAuxiliary];
  }
  else if (state != GHOST_kWindowStateFullScreen) {
    [m_window setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];
  }

  if (state == GHOST_kWindowStateFullScreen)
    setState(GHOST_kWindowStateFullScreen);

  setNativePixelSize();

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

  if (m_openGLView) {
    [m_openGLView release];
    m_openGLView = nil;
  }
  if (m_metalView) {
    [m_metalView release];
    m_metalView = nil;
  }
  if (m_metalLayer) {
    [m_metalLayer release];
    m_metalLayer = nil;
  }

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
  NSView *view = (m_openGLView) ? m_openGLView : m_metalView;
  return GHOST_Window::getValid() && m_window != NULL && view != NULL;
}

void *GHOST_WindowCocoa::getOSWindow() const
{
  return (void *)m_window;
}

void GHOST_WindowCocoa::setTitle(const char *title)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setTitle(): window invalid");
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSString *windowTitle = [[NSString alloc] initWithCString:title encoding:NSUTF8StringEncoding];

  // Set associated file if applicable
  if (windowTitle && [windowTitle hasPrefix:@"Blender"]) {
    NSRange fileStrRange;
    NSString *associatedFileName;
    int len;

    fileStrRange.location = [windowTitle rangeOfString:@"["].location + 1;
    len = [windowTitle rangeOfString:@"]"].location - fileStrRange.location;

    if (len > 0) {
      fileStrRange.length = len;
      associatedFileName = [windowTitle substringWithRange:fileStrRange];
      [m_window setTitle:[associatedFileName lastPathComponent]];

      @try {
        [m_window setRepresentedFilename:associatedFileName];
      }
      @catch (NSException *e) {
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

std::string GHOST_WindowCocoa::getTitle() const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getTitle(): window invalid");

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSString *windowTitle = [m_window title];

  std::string title;
  if (windowTitle != nil) {
    title = [windowTitle UTF8String];
  }

  [pool drain];

  return title;
}

void GHOST_WindowCocoa::getWindowBounds(GHOST_Rect &bounds) const
{
  NSRect rect;
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getWindowBounds(): window invalid");

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSRect screenSize = [[m_window screen] visibleFrame];

  rect = [m_window frame];

  bounds.m_b = screenSize.size.height - (rect.origin.y - screenSize.origin.y);
  bounds.m_l = rect.origin.x - screenSize.origin.x;
  bounds.m_r = rect.origin.x - screenSize.origin.x + rect.size.width;
  bounds.m_t = screenSize.size.height - (rect.origin.y + rect.size.height - screenSize.origin.y);

  [pool drain];
}

void GHOST_WindowCocoa::getClientBounds(GHOST_Rect &bounds) const
{
  NSRect rect;
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getClientBounds(): window invalid");

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSRect screenSize = [[m_window screen] visibleFrame];

  // Max window contents as screen size (excluding title bar...)
  NSRect contentRect = [CocoaWindow contentRectForFrameRect:screenSize
                                                  styleMask:[m_window styleMask]];

  rect = [m_window contentRectForFrameRect:[m_window frame]];

  bounds.m_b = contentRect.size.height - (rect.origin.y - contentRect.origin.y);
  bounds.m_l = rect.origin.x - contentRect.origin.x;
  bounds.m_r = rect.origin.x - contentRect.origin.x + rect.size.width;
  bounds.m_t = contentRect.size.height - (rect.origin.y + rect.size.height - contentRect.origin.y);
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
    size.width = width;
    size.height = cBnds.getHeight();
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
    size.width = cBnds.getWidth();
    size.height = height;
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
  if ((((GHOST_TUns32)cBnds.getWidth()) != width) ||
      (((GHOST_TUns32)cBnds.getHeight()) != height)) {
    NSSize size;
    size.width = width;
    size.height = height;
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

  NSUInteger masks = [m_window styleMask];

  if (masks & NSWindowStyleMaskFullScreen) {
    // Lion style fullscreen
    if (!m_immediateDraw) {
      state = GHOST_kWindowStateFullScreen;
    }
    else {
      state = GHOST_kWindowStateNormal;
    }
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

void GHOST_WindowCocoa::screenToClient(GHOST_TInt32 inX,
                                       GHOST_TInt32 inY,
                                       GHOST_TInt32 &outX,
                                       GHOST_TInt32 &outY) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::screenToClient(): window invalid");

  screenToClientIntern(inX, inY, outX, outY);

  /* switch y to match ghost convention */
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  outY = (cBnds.getHeight() - 1) - outY;
}

void GHOST_WindowCocoa::clientToScreen(GHOST_TInt32 inX,
                                       GHOST_TInt32 inY,
                                       GHOST_TInt32 &outX,
                                       GHOST_TInt32 &outY) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::clientToScreen(): window invalid");

  /* switch y to match ghost convention */
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  inY = (cBnds.getHeight() - 1) - inY;

  clientToScreenIntern(inX, inY, outX, outY);
}

void GHOST_WindowCocoa::screenToClientIntern(GHOST_TInt32 inX,
                                             GHOST_TInt32 inY,
                                             GHOST_TInt32 &outX,
                                             GHOST_TInt32 &outY) const
{
  NSRect screenCoord;
  NSRect baseCoord;

  screenCoord.origin.x = inX;
  screenCoord.origin.y = inY;

  baseCoord = [m_window convertRectFromScreen:screenCoord];

  outX = baseCoord.origin.x;
  outY = baseCoord.origin.y;
}

void GHOST_WindowCocoa::clientToScreenIntern(GHOST_TInt32 inX,
                                             GHOST_TInt32 inY,
                                             GHOST_TInt32 &outX,
                                             GHOST_TInt32 &outY) const
{
  NSRect screenCoord;
  NSRect baseCoord;

  baseCoord.origin.x = inX;
  baseCoord.origin.y = inY;

  screenCoord = [m_window convertRectToScreen:baseCoord];

  outX = screenCoord.origin.x;
  outY = screenCoord.origin.y;
}

NSScreen *GHOST_WindowCocoa::getScreen()
{
  return [m_window screen];
}

/* called for event, when window leaves monitor to another */
void GHOST_WindowCocoa::setNativePixelSize(void)
{
  NSView *view = (m_openGLView) ? m_openGLView : m_metalView;
  NSRect backingBounds = [view convertRectToBacking:[view bounds]];

  GHOST_Rect rect;
  getClientBounds(rect);

  m_nativePixelSize = (float)backingBounds.size.width / (float)rect.getWidth();
}

/**
 * \note Fullscreen switch is not actual fullscreen with display capture.
 * As this capture removes all OS X window manager features.
 *
 * Instead, the menu bar and the dock are hidden, and the window is made border-less and enlarged.
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

    case GHOST_kWindowStateFullScreen: {
      NSUInteger masks = [m_window styleMask];

      if (!(masks & NSWindowStyleMaskFullScreen)) {
        [m_window toggleFullScreen:nil];
      }
      break;
    }
    case GHOST_kWindowStateNormal:
    default:
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      NSUInteger masks = [m_window styleMask];

      if (masks & NSWindowStyleMaskFullScreen) {
        // Lion style fullscreen
        [m_window toggleFullScreen:nil];
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
    [NSApp activateIgnoringOtherApps:YES];
    [m_window makeKeyAndOrderFront:nil];
  }
  else {
    NSArray *windowsList;

    [m_window orderBack:nil];

    // Check for other blender opened windows and make the frontmost key
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

    GHOST_Context *context = new GHOST_ContextCGL(
        m_wantStereoVisual, m_metalView, m_metalLayer, m_openGLView);

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
  NSView *view = (m_openGLView) ? m_openGLView : m_metalView;
  [view setNeedsDisplay:YES];
  [pool drain];
  return GHOST_kSuccess;
}

#pragma mark Progress bar

GHOST_TSuccess GHOST_WindowCocoa::setProgressBar(float progress)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  if ((progress >= 0.0) && (progress <= 1.0)) {
    NSImage *dockIcon = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];

    [dockIcon lockFocus];
    NSRect progressBox = {{4, 4}, {120, 16}};

    [[NSImage imageNamed:@"NSApplicationIcon"] drawAtPoint:NSZeroPoint
                                                  fromRect:NSZeroRect
                                                 operation:NSCompositingOperationSourceOver
                                                  fraction:1.0];

    // Track & Outline
    [[NSColor blackColor] setFill];
    NSRectFill(progressBox);

    [[NSColor whiteColor] set];
    NSFrameRect(progressBox);

    // Progress fill
    progressBox = NSInsetRect(progressBox, 1, 1);

    progressBox.size.width = progressBox.size.width * progress;
    NSGradient *gradient = [[NSGradient alloc] initWithStartingColor:[NSColor darkGrayColor]
                                                         endingColor:[NSColor lightGrayColor]];
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
  if (!m_progressBarVisible)
    return GHOST_kFailure;
  m_progressBarVisible = false;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSImage *dockIcon = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];
  [dockIcon lockFocus];
  [[NSImage imageNamed:@"NSApplicationIcon"] drawAtPoint:NSZeroPoint
                                                fromRect:NSZeroRect
                                               operation:NSCompositingOperationSourceOver
                                                fraction:1.0];
  [dockIcon unlockFocus];
  [NSApp setApplicationIconImage:dockIcon];

  // We use notifications to inform the user when the progress reached 100%
  // Atm. just fire this when the progressbar ends, the behavior is controlled
  // in the NotificationCenter If Blender is not frontmost window, a message
  // pops up with sound, in any case an entry in notifications
  if ([NSUserNotificationCenter respondsToSelector:@selector(defaultUserNotificationCenter)]) {
    postNotification();
  }

  [dockIcon release];

  [pool drain];
  return GHOST_kSuccess;
}

#pragma mark Cursor handling

static NSCursor *getImageCursor(GHOST_TStandardCursor shape, NSString *name, NSPoint hotspot)
{
  static NSCursor *cursors[(int)GHOST_kStandardCursorNumCursors] = {0};
  static bool loaded[(int)GHOST_kStandardCursorNumCursors] = {false};

  const int index = (int)shape;
  if (!loaded[index]) {
    /* Load image from file in application Resources folder. */
    /* clang-format off */
    @autoreleasepool {
      /* clang-format on */
      NSImage *image = [NSImage imageNamed:name];
      if (image != NULL) {
        cursors[index] = [[NSCursor alloc] initWithImage:image hotSpot:hotspot];
      }
    }

    loaded[index] = true;
  }

  return cursors[index];
}

NSCursor *GHOST_WindowCocoa::getStandardCursor(GHOST_TStandardCursor shape) const
{
  switch (shape) {
    case GHOST_kStandardCursorCustom:
      if (m_customCursor) {
        return m_customCursor;
      }
      else {
        return NULL;
      }
    case GHOST_kStandardCursorDestroy:
      return [NSCursor disappearingItemCursor];
    case GHOST_kStandardCursorText:
      return [NSCursor IBeamCursor];
    case GHOST_kStandardCursorCrosshair:
      return [NSCursor crosshairCursor];
    case GHOST_kStandardCursorUpDown:
      return [NSCursor resizeUpDownCursor];
    case GHOST_kStandardCursorLeftRight:
      return [NSCursor resizeLeftRightCursor];
    case GHOST_kStandardCursorTopSide:
      return [NSCursor resizeUpCursor];
    case GHOST_kStandardCursorBottomSide:
      return [NSCursor resizeDownCursor];
    case GHOST_kStandardCursorLeftSide:
      return [NSCursor resizeLeftCursor];
    case GHOST_kStandardCursorRightSide:
      return [NSCursor resizeRightCursor];
    case GHOST_kStandardCursorCopy:
      return [NSCursor dragCopyCursor];
    case GHOST_kStandardCursorStop:
      return [NSCursor operationNotAllowedCursor];
    case GHOST_kStandardCursorMove:
      return [NSCursor pointingHandCursor];
    case GHOST_kStandardCursorDefault:
      return [NSCursor arrowCursor];
    case GHOST_kStandardCursorKnife:
      return getImageCursor(shape, @"knife.pdf", NSMakePoint(6, 24));
    case GHOST_kStandardCursorEraser:
      return getImageCursor(shape, @"eraser.pdf", NSMakePoint(6, 24));
    case GHOST_kStandardCursorPencil:
      return getImageCursor(shape, @"pen.pdf", NSMakePoint(6, 24));
    case GHOST_kStandardCursorEyedropper:
      return getImageCursor(shape, @"eyedropper.pdf", NSMakePoint(6, 24));
    case GHOST_kStandardCursorZoomIn:
      return getImageCursor(shape, @"zoomin.pdf", NSMakePoint(8, 7));
    case GHOST_kStandardCursorZoomOut:
      return getImageCursor(shape, @"zoomout.pdf", NSMakePoint(8, 7));
    case GHOST_kStandardCursorNSEWScroll:
      return getImageCursor(shape, @"scrollnsew.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorNSScroll:
      return getImageCursor(shape, @"scrollns.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorEWScroll:
      return getImageCursor(shape, @"scrollew.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorUpArrow:
      return getImageCursor(shape, @"arrowup.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorDownArrow:
      return getImageCursor(shape, @"arrowdown.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorLeftArrow:
      return getImageCursor(shape, @"arrowleft.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorRightArrow:
      return getImageCursor(shape, @"arrowright.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorVerticalSplit:
      return getImageCursor(shape, @"splitv.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorHorizontalSplit:
      return getImageCursor(shape, @"splith.pdf", NSMakePoint(16, 16));
    case GHOST_kStandardCursorCrosshairA:
      return getImageCursor(shape, @"paint_cursor_cross.pdf", NSMakePoint(16, 15));
    case GHOST_kStandardCursorCrosshairB:
      return getImageCursor(shape, @"paint_cursor_dot.pdf", NSMakePoint(16, 15));
    case GHOST_kStandardCursorCrosshairC:
      return getImageCursor(shape, @"crossc.pdf", NSMakePoint(16, 16));
    default:
      return NULL;
  }
}

void GHOST_WindowCocoa::loadCursor(bool visible, GHOST_TStandardCursor shape) const
{
  static bool systemCursorVisible = true;
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

  NSCursor *cursor = getStandardCursor(shape);
  if (cursor == NULL) {
    cursor = getStandardCursor(GHOST_kStandardCursorDefault);
  }

  [cursor set];
}

bool GHOST_WindowCocoa::isDialog() const
{
  return m_is_dialog;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorVisibility(bool visible)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

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
    // No need to perform grab without warp as it is always on in OS X
    if (mode != GHOST_kGrabNormal) {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

      m_systemCocoa->getCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setCursorGrabAccum(0, 0);

      if (mode == GHOST_kGrabHide) {
        setWindowCursorVisibility(false);
      }

      // Make window key if it wasn't to get the mouse move events
      [m_window makeKeyWindow];

      [pool drain];
    }
  }
  else {
    if (m_cursorGrab == GHOST_kGrabHide) {
      m_systemCocoa->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setWindowCursorVisibility(true);
    }

    /* Almost works without but important otherwise the mouse GHOST location
     * can be incorrect on exit. */
    setCursorGrabAccum(0, 0);
    m_cursorGrabBounds.m_l = m_cursorGrabBounds.m_r = -1; /* disable */
  }
  return err;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorShape(GHOST_TStandardCursor shape)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  if ([m_window isVisible]) {
    loadCursor(getCursorVisibility(), shape);
  }

  [pool drain];
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::hasCursorShape(GHOST_TStandardCursor shape)
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  GHOST_TSuccess success = (getStandardCursor(shape)) ? GHOST_kSuccess : GHOST_kFailure;
  [pool drain];
  return success;
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

GHOST_TSuccess GHOST_WindowCocoa::setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                                             GHOST_TUns8 *mask,
                                                             int sizex,
                                                             int sizey,
                                                             int hotX,
                                                             int hotY,
                                                             bool canInvertColor)
{
  int y, nbUns16;
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

  cursorImageRep = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                    pixelsWide:sizex
                    pixelsHigh:sizey
                 bitsPerSample:1
               samplesPerPixel:2
                      hasAlpha:YES
                      isPlanar:YES
                colorSpaceName:NSDeviceWhiteColorSpace
                   bytesPerRow:(sizex / 8 + (sizex % 8 > 0 ? 1 : 0))
                  bitsPerPixel:1];

  cursorBitmap = (GHOST_TUns16 *)[cursorImageRep bitmapData];
  nbUns16 = [cursorImageRep bytesPerPlane] / 2;

  for (y = 0; y < nbUns16; y++) {
#if !defined(__LITTLE_ENDIAN__)
    cursorBitmap[y] = uns16ReverseBits((bitmap[2 * y] << 0) | (bitmap[2 * y + 1] << 8));
    cursorBitmap[nbUns16 + y] = uns16ReverseBits((mask[2 * y] << 0) | (mask[2 * y + 1] << 8));
#else
    cursorBitmap[y] = uns16ReverseBits((bitmap[2 * y + 1] << 0) | (bitmap[2 * y] << 8));
    cursorBitmap[nbUns16 + y] = uns16ReverseBits((mask[2 * y + 1] << 0) | (mask[2 * y] << 8));
#endif

    /* Flip white cursor with black outline to black cursor with white outline
     * to match macOS platform conventions. */
    if (canInvertColor) {
      cursorBitmap[y] = ~cursorBitmap[y];
    }
  }

  imSize.width = sizex;
  imSize.height = sizey;
  cursorImage = [[NSImage alloc] initWithSize:imSize];
  [cursorImage addRepresentation:cursorImageRep];

  hotSpotPoint.x = hotX;
  hotSpotPoint.y = hotY;

  // foreground and background color parameter is not handled for now (10.6)
  m_customCursor = [[NSCursor alloc] initWithImage:cursorImage hotSpot:hotSpotPoint];

  [cursorImageRep release];
  [cursorImage release];

  if ([m_window isVisible]) {
    loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
  }
  [pool drain];
  return GHOST_kSuccess;
}
