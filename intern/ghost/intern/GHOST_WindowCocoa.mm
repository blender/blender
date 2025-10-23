/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "GHOST_WindowCocoa.hh"
#include "GHOST_ContextNone.hh"
#include "GHOST_Debug.hh"
#include "GHOST_SystemCocoa.hh"

/* Don't generate OpenGL deprecation warning. This is a known thing, and is not something easily
 * solvable in a short term. */
#ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef WITH_METAL_BACKEND
#  include "GHOST_ContextMTL.hh"
#endif

#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#include <sys/sysctl.h>

/* --------------------------------------------------------------------
 * Blender window delegate object.
 */

@interface BlenderWindowDelegate : NSObject <NSWindowDelegate>

@property(nonatomic, readonly, assign) GHOST_SystemCocoa *systemCocoa;
@property(nonatomic, readonly, assign) GHOST_WindowCocoa *windowCocoa;

- (instancetype)initWithSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
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

@implementation BlenderWindowDelegate : NSObject

@synthesize systemCocoa = system_cocoa_;
@synthesize windowCocoa = window_cocoa_;

- (instancetype)initWithSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
                        windowCocoa:(GHOST_WindowCocoa *)winCocoa
{
  self = [super init];

  if (self) {
    system_cocoa_ = sysCocoa;
    window_cocoa_ = winCocoa;
  }

  return self;
}

- (void)windowDidBecomeKey:(NSNotification *)notification
{
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowActivate, window_cocoa_);
  /* Workaround for broken app-switching when combining Command-Tab and mission-control. */
  [(NSWindow *)window_cocoa_->getOSWindow() orderFrontRegardless];
}

- (void)windowDidResignKey:(NSNotification *)notification
{
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowDeactivate, window_cocoa_);
}

- (void)windowDidExpose:(NSNotification *)notification
{
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowUpdate, window_cocoa_);
}

- (void)windowDidMove:(NSNotification *)notification
{
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowMove, window_cocoa_);
}

- (void)windowWillMove:(NSNotification *)notification
{
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowMove, window_cocoa_);
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
  window_cocoa_->setImmediateDraw(true);
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
  /* macOS does not send a window resize event when switching between zoomed
   * and full-screen, when automatic show/hide of dock and menu bar are enabled.
   * Send our own to prevent artifacts. */
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowSize, window_cocoa_);

  window_cocoa_->setImmediateDraw(false);
}

- (void)windowWillExitFullScreen:(NSNotification *)notification
{
  window_cocoa_->setImmediateDraw(true);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
  /* See comment for windowWillEnterFullScreen. */
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowSize, window_cocoa_);
  window_cocoa_->setImmediateDraw(false);
}

- (void)windowDidResize:(NSNotification *)notification
{
#if 0
  if (![[notification object] inLiveResize])
#endif
  {
    /* Send event only once, at end of resize operation (when user has released mouse button). */
    system_cocoa_->handleWindowEvent(GHOST_kEventWindowSize, window_cocoa_);
  }
  /* Live resize, send event, gets handled in wm_window.c.
   * Needed because live resize runs in a modal loop, not letting main loop run */
  if ([[notification object] inLiveResize]) {
    system_cocoa_->dispatchEvents();
  }
}

- (void)windowDidChangeBackingProperties:(NSNotification *)notification
{
  system_cocoa_->handleWindowEvent(GHOST_kEventNativeResolutionChange, window_cocoa_);
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowSize, window_cocoa_);
}

- (BOOL)windowShouldClose:(id)sender;
{
  /* Let Blender close the window rather than closing immediately. */
  system_cocoa_->handleWindowEvent(GHOST_kEventWindowClose, window_cocoa_);
  return false;
}

@end

@interface BlenderWindow : NSWindow

@property(nonatomic, readonly, assign) GHOST_SystemCocoa *systemCocoa;
@property(nonatomic, readonly, assign) GHOST_WindowCocoa *windowCocoa;
@property(nonatomic, readonly, assign) GHOST_TDragnDropTypes draggedObjectType;

- (instancetype)initWithSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
                        windowCocoa:(GHOST_WindowCocoa *)winCocoa
                        contentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag;

@end

@implementation BlenderWindow

@synthesize systemCocoa = system_cocoa_;
@synthesize windowCocoa = window_cocoa_;
@synthesize draggedObjectType = dragged_object_type_;

- (instancetype)initWithSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
                        windowCocoa:(GHOST_WindowCocoa *)winCocoa
                        contentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag
{
  self = [super initWithContentRect:contentRect
                          styleMask:style
                            backing:backingStoreType
                              defer:flag];

  if (self) {
    system_cocoa_ = sysCocoa;
    window_cocoa_ = winCocoa;
  }

  return self;
}

- (BOOL)canBecomeKeyWindow
{
  /* Don't make other windows active when a dialog window is open. */
  return (window_cocoa_->isDialog() || !system_cocoa_->hasDialogWindow());
}

/* The drag & drop dragging destination methods. */
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
  @autoreleasepool {
    NSPasteboard *draggingPBoard = sender.draggingPasteboard;
    if ([[draggingPBoard types] containsObject:NSPasteboardTypeTIFF]) {
      dragged_object_type_ = GHOST_kDragnDropTypeBitmap;
    }
    else if ([[draggingPBoard types] containsObject:NSFilenamesPboardType]) {
      dragged_object_type_ = GHOST_kDragnDropTypeFilenames;
    }
    else if ([[draggingPBoard types] containsObject:NSPasteboardTypeString]) {
      dragged_object_type_ = GHOST_kDragnDropTypeString;
    }
    else {
      return NSDragOperationNone;
    }

    const NSPoint mouseLocation = sender.draggingLocation;
    window_cocoa_->setAcceptDragOperation(TRUE); /* Drag operation is accepted by default. */
    system_cocoa_->handleDraggingEvent(GHOST_kEventDraggingEntered,
                                       dragged_object_type_,
                                       window_cocoa_,
                                       mouseLocation.x,
                                       mouseLocation.y,
                                       nil);
  }
  return NSDragOperationCopy;
}

- (BOOL)wantsPeriodicDraggingUpdates
{
  return NO; /* No need to overflow blender event queue. Events shall be sent only on changes. */
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
  const NSPoint mouseLocation = [sender draggingLocation];

  system_cocoa_->handleDraggingEvent(GHOST_kEventDraggingUpdated,
                                     dragged_object_type_,
                                     window_cocoa_,
                                     mouseLocation.x,
                                     mouseLocation.y,
                                     nil);
  return window_cocoa_->canAcceptDragOperation() ? NSDragOperationCopy : NSDragOperationNone;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender
{
  system_cocoa_->handleDraggingEvent(
      GHOST_kEventDraggingExited, dragged_object_type_, window_cocoa_, 0, 0, nil);
  dragged_object_type_ = GHOST_kDragnDropTypeUnknown;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender
{
  if (window_cocoa_->canAcceptDragOperation()) {
    return YES;
  }
  return NO;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
  @autoreleasepool {
    NSPasteboard *draggingPBoard = sender.draggingPasteboard;
    id data;

    switch (dragged_object_type_) {
      case GHOST_kDragnDropTypeBitmap: {
        if (![NSImage canInitWithPasteboard:draggingPBoard]) {
          return NO;
        }
        /* Caller must [release] the returned data in this case. */
        NSImage *droppedImg = [[NSImage alloc] initWithPasteboard:draggingPBoard];
        data = droppedImg;
        break;
      }
      case GHOST_kDragnDropTypeFilenames:
        data = [draggingPBoard propertyListForType:NSFilenamesPboardType];
        break;
      case GHOST_kDragnDropTypeString:
        data = [draggingPBoard stringForType:NSPasteboardTypeString];
        break;
      default:
        return NO;
        break;
    }

    const NSPoint mouseLocation = sender.draggingLocation;
    system_cocoa_->handleDraggingEvent(GHOST_kEventDraggingDropDone,
                                       dragged_object_type_,
                                       window_cocoa_,
                                       mouseLocation.x,
                                       mouseLocation.y,
                                       (void *)data);
  }
  return YES;
}

@end

/* NSView for handling input and drawing. */
#define COCOA_VIEW_CLASS CocoaOpenGLView
#define COCOA_VIEW_BASE_CLASS NSOpenGLView
#include "GHOST_WindowViewCocoa.hh"
#undef COCOA_VIEW_CLASS
#undef COCOA_VIEW_BASE_CLASS

#define COCOA_VIEW_CLASS CocoaMetalView
#define COCOA_VIEW_BASE_CLASS NSView
#include "GHOST_WindowViewCocoa.hh"
#undef COCOA_VIEW_CLASS
#undef COCOA_VIEW_BASE_CLASS

/* --------------------------------------------------------------------
 * Initialization / Finalization.
 */

GHOST_WindowCocoa::GHOST_WindowCocoa(GHOST_SystemCocoa *systemCocoa,
                                     const char *title,
                                     int32_t left,
                                     int32_t bottom,
                                     uint32_t width,
                                     uint32_t height,
                                     GHOST_TWindowState state,
                                     GHOST_TDrawingContextType type,
                                     const GHOST_ContextParams &context_params,
                                     bool is_dialog,
                                     GHOST_WindowCocoa *parent_window,
                                     const GHOST_GPUDevice &preferred_device)
    : GHOST_Window(width, height, state, context_params, false),
      opengl_view_(nil),
      metal_view_(nil),
      metal_layer_(nil),
      system_cocoa_(systemCocoa),
      custom_cursor_(nullptr),
      immediate_draw_(false),
      is_dialog_(is_dialog),
      preferred_device_(preferred_device)
{
  full_screen_ = false;

  @autoreleasepool {
    /* Create the window. */
    NSRect rect;
    rect.origin.x = left;
    rect.origin.y = bottom;
    rect.size.width = width;
    rect.size.height = height;

    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskResizable;
    if (!is_dialog) {
      styleMask |= NSWindowStyleMaskMiniaturizable;
    }

    window_ = [[BlenderWindow alloc] initWithSystemCocoa:systemCocoa
                                             windowCocoa:this
                                             contentRect:rect
                                               styleMask:styleMask
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
    /* By default, AppKit repositions the window in the context of the current "mainMonitor"
     * (the monitor which has focus), bypass this by forcing the window back into its correct
     * position. Since we use global screen coordinate indexed on the first, primary screen.
     */
    [window_ setFrameOrigin:NSMakePoint(left, bottom)];

    /* Forbid to resize the window below the blender defined minimum one. */
    const NSSize minSize = {320, 240};
    window_.contentMinSize = minSize;

    /* Create NSView inside the window. */
    id<MTLDevice> metalDevice = MTLCreateSystemDefaultDevice();
    NSView *view;

    if (metalDevice) {
      /* Create metal layer and view if supported. */
      metal_layer_ = [[CAMetalLayer alloc] init];
      metal_layer_.edgeAntialiasingMask = 0;
      metal_layer_.masksToBounds = NO;
      metal_layer_.opaque = YES;
      metal_layer_.framebufferOnly = YES;
      metal_layer_.presentsWithTransaction = NO;
      [metal_layer_ removeAllAnimations];
      metal_layer_.device = metalDevice;

      if (type == GHOST_kDrawingContextTypeMetal) {
        /* Enable EDR support. This is done by:
         * 1. Using a floating point render target, so that values outside 0..1 can be used
         * 2. Informing the OS that we are EDR aware, and intend to use values outside 0..1
         * 3. Setting the extended sRGB color space so that the OS knows how to interpret the
         *    values.
         */
        metal_layer_.wantsExtendedDynamicRangeContent = YES;
        metal_layer_.pixelFormat = MTLPixelFormatRGBA16Float;
        const CFStringRef name = kCGColorSpaceExtendedSRGB;
        CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(name);
        metal_layer_.colorspace = colorspace;
        CGColorSpaceRelease(colorspace);

        /* For Blender to know if this window supports HDR. */
        hdr_info_.hdr_enabled = true;
        hdr_info_.wide_gamut_enabled = true;
        hdr_info_.sdr_white_level = 1.0f;
      }

      metal_view_ = [[CocoaMetalView alloc] initWithSystemCocoa:systemCocoa
                                                    windowCocoa:this
                                                          frame:rect];
      metal_view_.wantsLayer = YES;
      metal_view_.layer = metal_layer_;
      view = metal_view_;
    }
    else {
      /* Fall back to OpenGL view if there is no Metal support. */
      opengl_view_ = [[CocoaOpenGLView alloc] initWithSystemCocoa:systemCocoa
                                                      windowCocoa:this
                                                            frame:rect];
      view = opengl_view_;
    }

    if (system_cocoa_->native_pixel_) {
      /* Needs to happen early when building with the 10.14 SDK, otherwise
       * has no effect until resizing the window. */
      if ([view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
        view.wantsBestResolutionOpenGLSurface = YES;
      }
    }

    window_.contentView = view;
    window_.initialFirstResponder = view;

    [window_ makeKeyAndOrderFront:nil];

    setDrawingContextType(type);
    updateDrawingContext();
    activateDrawingContext();

    setTitle(title);

    tablet_ = GHOST_TABLET_DATA_NONE;

    BlenderWindowDelegate *windowDelegate = [[BlenderWindowDelegate alloc]
        initWithSystemCocoa:systemCocoa
                windowCocoa:this];
    window_.delegate = windowDelegate;

    window_.acceptsMouseMovedEvents = YES;

    NSView *contentview = window_.contentView;
    contentview.allowedTouchTypes = (NSTouchTypeMaskDirect | NSTouchTypeMaskIndirect);

    [window_ registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType,
                                                               NSPasteboardTypeString,
                                                               NSPasteboardTypeTIFF,
                                                               nil]];

    if (is_dialog && parent_window) {
      [parent_window->getViewWindow() addChildWindow:window_ ordered:NSWindowAbove];
      window_.collectionBehavior = NSWindowCollectionBehaviorFullScreenAuxiliary;
    }
    else {
      window_.collectionBehavior = NSWindowCollectionBehaviorFullScreenPrimary;
    }

    if (state == GHOST_kWindowStateFullScreen) {
      setState(GHOST_kWindowStateFullScreen);
    }

    setNativePixelSize();
  }
}

GHOST_WindowCocoa::~GHOST_WindowCocoa()
{
  @autoreleasepool {
    if (custom_cursor_) {
      [custom_cursor_ release];
      custom_cursor_ = nil;
    }

    releaseNativeHandles();

    if (opengl_view_) {
      [opengl_view_ release];
      opengl_view_ = nil;
    }
    if (metal_view_) {
      [metal_view_ release];
      metal_view_ = nil;
    }
    if (metal_layer_) {
      [metal_layer_ release];
      metal_layer_ = nil;
    }

    if (window_) {
      [window_ close];
    }

    /* Check for other blender opened windows and make the front-most key
     * NOTE: for some reason the closed window is still in the list. */
    NSArray *windowsList = [NSApp orderedWindows];
    for (int a = 0; a < [windowsList count]; a++) {
      if (window_ != (BlenderWindow *)[windowsList objectAtIndex:a]) {
        [[windowsList objectAtIndex:a] makeKeyWindow];
        break;
      }
    }
    window_ = nil;
  }
}

/* --------------------------------------------------------------------
 * Accessors.
 */

bool GHOST_WindowCocoa::getValid() const
{
  NSView *view = (opengl_view_) ? opengl_view_ : metal_view_;
  return GHOST_Window::getValid() && window_ != nullptr && view != nullptr;
}

void *GHOST_WindowCocoa::getOSWindow() const
{
  return (void *)window_;
}

void GHOST_WindowCocoa::setTitle(const char *title)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setTitle(): window invalid");

  @autoreleasepool {
    NSString *windowTitle = [[NSString alloc] initWithCString:title encoding:NSUTF8StringEncoding];
    window_.title = windowTitle;

    [windowTitle release];
  }
}

std::string GHOST_WindowCocoa::getTitle() const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getTitle(): window invalid");

  std::string title;
  @autoreleasepool {
    NSString *windowTitle = window_.title;
    if (windowTitle != nil) {
      title = windowTitle.UTF8String;
    }
  }
  return title;
}

void GHOST_WindowCocoa::setPath(const char *filepath)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setAssociatedFile(): window invalid");

  @autoreleasepool {
    NSString *associatedFileName = [[[NSString alloc] initWithCString:filepath
                                                             encoding:NSUTF8StringEncoding]
        autorelease];

    window_.representedFilename = associatedFileName;
  }
}

GHOST_TSuccess GHOST_WindowCocoa::applyWindowDecorationStyle()
{
  @autoreleasepool {
    if (window_decoration_style_flags_ & GHOST_kDecorationColoredTitleBar) {
      const float *background_color = window_decoration_style_settings_.colored_titlebar_bg_color;

      /* Title-bar background color. */
      window_.backgroundColor = [NSColor colorWithRed:background_color[0]
                                                green:background_color[1]
                                                 blue:background_color[2]
                                                alpha:1.0];

      /* Title-bar foreground color.
       * Use the value component of the title-bar background's HSV representation to determine
       * whether we should use the macOS dark or light title-bar text appearance. With values below
       * 0.5 considered as dark themes, and values above 0.5 considered as light themes.
       */
      const float hsv_v = MAX(background_color[0], MAX(background_color[1], background_color[2]));

      const NSAppearanceName win_appearance = hsv_v > 0.5 ? NSAppearanceNameVibrantLight :
                                                            NSAppearanceNameVibrantDark;

      window_.appearance = [NSAppearance appearanceNamed:win_appearance];
      window_.titlebarAppearsTransparent = YES;
    }
    else {
      window_.titlebarAppearsTransparent = NO;
    }
  }
  return GHOST_kSuccess;
}

void GHOST_WindowCocoa::getWindowBounds(GHOST_Rect &bounds) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getWindowBounds(): window invalid");

  @autoreleasepool {
    /* All coordinates are based off the primary screen. */
    const NSRect screenFrame = [getPrimaryScreen() visibleFrame];
    const NSRect windowFrame = window_.frame;

    /* Flip the Y axis, from bottom left coordinate to top left, which is the expected coordinate
     * return format for GHOST, even though the Window Manager later reflips it to bottom-left
     * this is the expected coordinate system for all GHOST backends
     */

    const int32_t screenMaxY = screenFrame.origin.y + screenFrame.size.height;

    /* Flip the coordinates vertically from a bottom-left origin to a top-left origin,
     * as expected by GHOST. */
    bounds.b_ = screenMaxY - windowFrame.origin.y;
    bounds.t_ = screenMaxY - windowFrame.origin.y - windowFrame.size.height;

    bounds.l_ = windowFrame.origin.x;
    bounds.r_ = windowFrame.origin.x + windowFrame.size.width;
  }
}

void GHOST_WindowCocoa::getClientBounds(GHOST_Rect &bounds) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getClientBounds(): window invalid");

  @autoreleasepool {
    /* All coordinates are based off the primary screen. */
    const NSRect screenFrame = [getPrimaryScreen() visibleFrame];
    /* Screen Content Rectangle (excluding Menu Bar and Dock). */
    const NSRect screenContentRect = [NSWindow contentRectForFrameRect:screenFrame
                                                             styleMask:[window_ styleMask]];

    const NSRect windowFrame = window_.frame;
    /* Window Content Rectangle (excluding Titlebar and borders) */
    const NSRect windowContentRect = [window_ contentRectForFrameRect:windowFrame];

    const int32_t screenMaxY = screenContentRect.origin.y + screenContentRect.size.height;

    /* Flip the coordinates vertically from a bottom-left origin to a top-left origin,
     * as expected by GHOST. */
    bounds.b_ = screenMaxY - windowContentRect.origin.y;
    bounds.t_ = screenMaxY - windowContentRect.origin.y - windowContentRect.size.height;

    bounds.l_ = windowContentRect.origin.x;
    bounds.r_ = windowContentRect.origin.x + windowContentRect.size.width;
  }
}

GHOST_TSuccess GHOST_WindowCocoa::setClientWidth(uint32_t width)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientWidth(): window invalid");

  @autoreleasepool {
    GHOST_Rect cBnds;
    getClientBounds(cBnds);

    if ((uint32_t(cBnds.getWidth())) != width) {
      const NSSize size = {(CGFloat)width, (CGFloat)cBnds.getHeight()};
      [window_ setContentSize:size];
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setClientHeight(uint32_t height)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientHeight(): window invalid");

  @autoreleasepool {
    GHOST_Rect cBnds;
    getClientBounds(cBnds);

    if ((uint32_t(cBnds.getHeight())) != height) {
      const NSSize size = {(CGFloat)cBnds.getWidth(), (CGFloat)height};
      [window_ setContentSize:size];
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setClientSize(uint32_t width, uint32_t height)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientSize(): window invalid");

  @autoreleasepool {
    GHOST_Rect cBnds;
    getClientBounds(cBnds);
    if (((uint32_t(cBnds.getWidth())) != width) || ((uint32_t(cBnds.getHeight())) != height)) {
      const NSSize size = {(CGFloat)width, (CGFloat)height};
      [window_ setContentSize:size];
    }
  }
  return GHOST_kSuccess;
}

GHOST_TWindowState GHOST_WindowCocoa::getState() const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getState(): window invalid");

  @autoreleasepool {
    NSUInteger masks = window_.styleMask;

    if (masks & NSWindowStyleMaskFullScreen) {
      /* Lion style full-screen. */
      if (!immediate_draw_) {
        return GHOST_kWindowStateFullScreen;
      }
      return GHOST_kWindowStateNormal;
    }
    if (window_.isMiniaturized) {
      return GHOST_kWindowStateMinimized;
    }
    if (window_.isZoomed) {
      return GHOST_kWindowStateMaximized;
    }
    if (immediate_draw_) {
      return GHOST_kWindowStateFullScreen;
    }
    return GHOST_kWindowStateNormal;
  }
}

void GHOST_WindowCocoa::screenToClient(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::screenToClient(): window invalid");

  screenToClientIntern(inX, inY, outX, outY);

  /* switch y to match ghost convention */
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  outY = (cBnds.getHeight() - 1) - outY;
}

void GHOST_WindowCocoa::clientToScreen(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::clientToScreen(): window invalid");

  /* switch y to match ghost convention */
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  inY = (cBnds.getHeight() - 1) - inY;

  clientToScreenIntern(inX, inY, outX, outY);
}

void GHOST_WindowCocoa::screenToClientIntern(int32_t inX,
                                             int32_t inY,
                                             int32_t &outX,
                                             int32_t &outY) const
{
  NSRect screenCoord;
  screenCoord.origin = {(CGFloat)inX, (CGFloat)inY};

  const NSRect baseCoord = [window_ convertRectFromScreen:screenCoord];

  outX = baseCoord.origin.x;
  outY = baseCoord.origin.y;
}

void GHOST_WindowCocoa::clientToScreenIntern(int32_t inX,
                                             int32_t inY,
                                             int32_t &outX,
                                             int32_t &outY) const
{
  NSRect baseCoord;
  baseCoord.origin = {(CGFloat)inX, (CGFloat)inY};

  const NSRect screenCoord = [window_ convertRectToScreen:baseCoord];

  outX = screenCoord.origin.x;
  outY = screenCoord.origin.y;
}

NSScreen *GHOST_WindowCocoa::getScreen() const
{
  return window_.screen;
}

NSScreen *GHOST_WindowCocoa::getPrimaryScreen()
{
  /* The first element of the screens array is guaranted to be the primary screen by AppKit. */
  return [[NSScreen screens] firstObject];
}

/* called for event, when window leaves monitor to another */
void GHOST_WindowCocoa::setNativePixelSize()
{
  NSView *view = (opengl_view_) ? opengl_view_ : metal_view_;
  const NSRect backingBounds = [view convertRectToBacking:[view bounds]];

  GHOST_Rect rect;
  getClientBounds(rect);

  native_pixel_size_ = float(backingBounds.size.width) / float(rect.getWidth());
}

/**
 * \note Full-screen switch is not actual full-screen with display capture.
 * As this capture removes all OS X window manager features.
 *
 * Instead, the menu bar and the dock are hidden, and the window is made border-less and enlarged.
 * Thus, process switch, exposé, spaces, ... still work in full-screen mode.
 */
GHOST_TSuccess GHOST_WindowCocoa::setState(GHOST_TWindowState state)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setState(): window invalid");

  @autoreleasepool {
    switch (state) {
      case GHOST_kWindowStateMinimized:
        [window_ miniaturize:nil];
        break;
      case GHOST_kWindowStateMaximized:
        [window_ zoom:nil];
        break;

      case GHOST_kWindowStateFullScreen: {
        const NSUInteger masks = window_.styleMask;

        if (!(masks & NSWindowStyleMaskFullScreen)) {
          [window_ toggleFullScreen:nil];
        }
        break;
      }
      case GHOST_kWindowStateNormal:
      default:
        @autoreleasepool {
          const NSUInteger masks = window_.styleMask;

          if (masks & NSWindowStyleMaskFullScreen) {
            /* Lion style full-screen. */
            [window_ toggleFullScreen:nil];
          }
          else if (window_.isMiniaturized) {
            [window_ deminiaturize:nil];
          }
          else if (window_.isZoomed) {
            [window_ zoom:nil];
          }
        }
        break;
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setModifiedState(bool is_unsaved_changes)
{
  @autoreleasepool {
    window_.documentEdited = is_unsaved_changes;
  }
  return GHOST_Window::setModifiedState(is_unsaved_changes);
}

GHOST_TSuccess GHOST_WindowCocoa::setOrder(GHOST_TWindowOrder order)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setOrder(): window invalid");

  @autoreleasepool {
    if (order == GHOST_kWindowOrderTop) {
      [NSApp activateIgnoringOtherApps:YES];
      [window_ makeKeyAndOrderFront:nil];
    }
    else {
      NSArray *windowsList;

      [window_ orderBack:nil];

      /* Check for other blender opened windows and make the front-most key. */
      windowsList = [NSApp orderedWindows];
      if (windowsList.count) {
        [[windowsList objectAtIndex:0] makeKeyAndOrderFront:nil];
      }
    }
  }
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Drawing context.
 */

GHOST_Context *GHOST_WindowCocoa::newDrawingContext(GHOST_TDrawingContextType type)
{
  switch (type) {
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kDrawingContextTypeVulkan: {
      GHOST_Context *context = new GHOST_ContextVK(
          want_context_params_, metal_layer_, 1, 2, true, preferred_device_, &hdr_info_);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

#ifdef WITH_METAL_BACKEND
    case GHOST_kDrawingContextTypeMetal: {
      GHOST_Context *context = new GHOST_ContextMTL(
          want_context_params_, metal_view_, metal_layer_);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

    default:
      /* Unsupported backend. */
      return nullptr;
  }
}

/* --------------------------------------------------------------------
 * Invalidate.
 */

GHOST_TSuccess GHOST_WindowCocoa::invalidate()
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::invalidate(): window invalid");

  @autoreleasepool {
    NSView *view = (opengl_view_) ? opengl_view_ : metal_view_;
    view.needsDisplay = YES;
  }
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Progress bar.
 */

GHOST_TSuccess GHOST_WindowCocoa::setProgressBar(float progress)
{
  @autoreleasepool {
    if ((progress >= 0.0) && (progress <= 1.0)) {
      NSImage *dockIcon = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];

      [dockIcon lockFocus];

      [[NSImage imageNamed:@"NSApplicationIcon"] drawAtPoint:NSZeroPoint
                                                    fromRect:NSZeroRect
                                                   operation:NSCompositingOperationSourceOver
                                                    fraction:1.0];

      NSRect progressRect = {{8, 8}, {112, 14}};
      NSBezierPath *progressPath;

      /* Draw white track. */
      [[[NSColor whiteColor] colorWithAlphaComponent:0.6] setFill];
      progressPath = [NSBezierPath bezierPathWithRoundedRect:progressRect xRadius:7 yRadius:7];
      [progressPath fill];

      /* Black progress fill. */
      [[[NSColor blackColor] colorWithAlphaComponent:0.7] setFill];
      progressRect = NSInsetRect(progressRect, 2, 2);
      progressRect.size.width *= progress;
      progressPath = [NSBezierPath bezierPathWithRoundedRect:progressRect xRadius:5 yRadius:5];
      [progressPath fill];

      [dockIcon unlockFocus];
      [NSApp setApplicationIconImage:dockIcon];
      [dockIcon release];

      progress_bar_visible_ = true;
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::endProgressBar()
{
  if (!progress_bar_visible_) {
    return GHOST_kFailure;
  }
  progress_bar_visible_ = false;

  /* Reset application icon to remove the progress bar. */
  @autoreleasepool {
    NSImage *dockIcon = [[NSImage alloc] initWithSize:NSMakeSize(128, 128)];
    [dockIcon lockFocus];
    [[NSImage imageNamed:@"NSApplicationIcon"] drawAtPoint:NSZeroPoint
                                                  fromRect:NSZeroRect
                                                 operation:NSCompositingOperationSourceOver
                                                  fraction:1.0];
    [dockIcon unlockFocus];
    [NSApp setApplicationIconImage:dockIcon];
    [dockIcon release];
  }
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Cursor handling.
 */

static NSCursor *getImageCursor(GHOST_TStandardCursor shape, NSString *name, NSPoint hotspot)
{
  static NSCursor *cursors[GHOST_kStandardCursorNumCursors] = {nullptr};
  static bool loaded[GHOST_kStandardCursorNumCursors] = {false};

  const int index = int(shape);
  if (!loaded[index]) {
    /* Load image from file in application Resources folder. */
    @autoreleasepool {
      NSImage *image = [NSImage imageNamed:name];
      if (image != nullptr) {
        cursors[index] = [[NSCursor alloc] initWithImage:image hotSpot:hotspot];
      }
    }

    loaded[index] = true;
  }

  return cursors[index];
}

/* busyButClickableCursor is an undocumented NSCursor API, but
 * has been in use since at least OS X 10.4 and through 10.9. */
@interface NSCursor (Undocumented)
+ (NSCursor *)busyButClickableCursor;
@end

NSCursor *GHOST_WindowCocoa::getStandardCursor(GHOST_TStandardCursor shape) const
{
  @autoreleasepool {
    switch (shape) {
      case GHOST_kStandardCursorCustom:
        if (custom_cursor_) {
          return custom_cursor_;
        }
        else {
          return nullptr;
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
        return [NSCursor openHandCursor];
      case GHOST_kStandardCursorHandOpen:
        return [NSCursor openHandCursor];
      case GHOST_kStandardCursorHandClosed:
        return [NSCursor closedHandCursor];
      case GHOST_kStandardCursorHandPoint:
        return [NSCursor pointingHandCursor];
      case GHOST_kStandardCursorDefault:
        return [NSCursor arrowCursor];
      case GHOST_kStandardCursorWait:
        if ([NSCursor respondsToSelector:@selector(busyButClickableCursor)]) {
          return [NSCursor busyButClickableCursor];
        }
        return nullptr;
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
      case GHOST_kStandardCursorLeftHandle:
        return getImageCursor(shape, @"handle_left.pdf", NSMakePoint(12, 14));
      case GHOST_kStandardCursorRightHandle:
        return getImageCursor(shape, @"handle_right.pdf", NSMakePoint(10, 14));
      case GHOST_kStandardCursorBothHandles:
        return getImageCursor(shape, @"handle_both.pdf", NSMakePoint(11, 14));
      default:
        return nullptr;
    }
  }
}

void GHOST_WindowCocoa::loadCursor(bool visible, GHOST_TStandardCursor shape) const
{
  static bool systemCursorVisible = true;

  @autoreleasepool {
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
    if (cursor == nullptr) {
      cursor = getStandardCursor(GHOST_kStandardCursorDefault);
    }

    [cursor set];
  }
}

bool GHOST_WindowCocoa::isDialog() const
{
  return is_dialog_;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorVisibility(bool visible)
{
  @autoreleasepool {
    if (window_.isVisible) {
      loadCursor(visible, getCursorShape());
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  @autoreleasepool {
    if (mode != GHOST_kGrabDisable) {
      /* No need to perform grab without warp as it is always enabled in OS X. */
      if (mode != GHOST_kGrabNormal) {
        @autoreleasepool {
          system_cocoa_->getCursorPosition(cursor_grab_init_pos_[0], cursor_grab_init_pos_[1]);
          setCursorGrabAccum(0, 0);

          if (mode == GHOST_kGrabHide) {
            setWindowCursorVisibility(false);
          }

          /* Make window key if it wasn't to get the mouse move events. */
          [window_ makeKeyWindow];
        }
      }
    }
    else {
      if (cursor_grab_ == GHOST_kGrabHide) {
        system_cocoa_->setCursorPosition(cursor_grab_init_pos_[0], cursor_grab_init_pos_[1]);
        setWindowCursorVisibility(true);
      }

      /* Almost works without but important otherwise the mouse GHOST location
       * can be incorrect on exit. */
      setCursorGrabAccum(0, 0);
      cursor_grab_bounds_.l_ = cursor_grab_bounds_.r_ = -1; /* disable */
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorShape(GHOST_TStandardCursor shape)
{
  @autoreleasepool {
    if (window_.isVisible) {
      loadCursor(getCursorVisibility(), shape);
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::hasCursorShape(GHOST_TStandardCursor shape)
{
  @autoreleasepool {
    GHOST_TSuccess success = (getStandardCursor(shape)) ? GHOST_kSuccess : GHOST_kFailure;
    return success;
  }
}

/* Reverse the bits in a uint8_t */
#if 0
static uint8_t uns8ReverseBits(uint8_t ch)
{
  ch= ((ch >> 1) & 0x55) | ((ch << 1) & 0xAA);
  ch= ((ch >> 2) & 0x33) | ((ch << 2) & 0xCC);
  ch= ((ch >> 4) & 0x0F) | ((ch << 4) & 0xF0);
  return ch;
}
#endif

/** Reverse the bits in a uint16_t */
static uint16_t uns16ReverseBits(uint16_t shrt)
{
  shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
  shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
  shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
  shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
  return shrt;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCustomCursorShape(const uint8_t *bitmap,
                                                             const uint8_t *mask,
                                                             const int size[2],
                                                             const int hot_spot[2],
                                                             const bool can_invert_color)
{
  @autoreleasepool {
    if (custom_cursor_) {
      [custom_cursor_ release];
      custom_cursor_ = nil;
    }

    NSBitmapImageRep *cursorImageRep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:nil
                      pixelsWide:size[0]
                      pixelsHigh:size[1]
                   bitsPerSample:1
                 samplesPerPixel:2
                        hasAlpha:YES
                        isPlanar:YES
                  colorSpaceName:NSDeviceWhiteColorSpace
                     bytesPerRow:(size[0] / 8 + (size[0] % 8 > 0 ? 1 : 0))
                    bitsPerPixel:1];

    uint16_t *cursorBitmap = (uint16_t *)cursorImageRep.bitmapData;
    int nbUns16 = cursorImageRep.bytesPerPlane / 2;

    for (int y = 0; y < nbUns16; y++) {
#if !defined(__LITTLE_ENDIAN__)
      cursorBitmap[y] = uns16ReverseBits((bitmap[2 * y] << 0) | (bitmap[2 * y + 1] << 8));
      cursorBitmap[nbUns16 + y] = uns16ReverseBits((mask[2 * y] << 0) | (mask[2 * y + 1] << 8));
#else
      cursorBitmap[y] = uns16ReverseBits((bitmap[2 * y + 1] << 0) | (bitmap[2 * y] << 8));
      cursorBitmap[nbUns16 + y] = uns16ReverseBits((mask[2 * y + 1] << 0) | (mask[2 * y] << 8));
#endif

      /* Flip white cursor with black outline to black cursor with white outline
       * to match macOS platform conventions. */
      if (can_invert_color) {
        cursorBitmap[y] = ~cursorBitmap[y];
      }
    }

    const NSSize imSize = {(CGFloat)size[0], (CGFloat)size[1]};
    NSImage *cursorImage = [[NSImage alloc] initWithSize:imSize];
    [cursorImage addRepresentation:cursorImageRep];

    const NSPoint hotSpotPoint = {(CGFloat)(hot_spot[0]), (CGFloat)(hot_spot[1])};

    /* Foreground and background color parameter is not handled for now (10.6). */
    custom_cursor_ = [[NSCursor alloc] initWithImage:cursorImage hotSpot:hotSpotPoint];

    [cursorImageRep release];
    [cursorImage release];

    if (window_.isVisible) {
      loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
    }
  }
  return GHOST_kSuccess;
}

#ifdef WITH_INPUT_IME
void GHOST_WindowCocoa::beginIME(int32_t x, int32_t y, int32_t w, int32_t h, bool completed)
{
  if (opengl_view_) {
    [opengl_view_ beginIME:x y:y w:w h:h completed:completed];
  }
  else {
    [metal_view_ beginIME:x y:y w:w h:h completed:completed];
  }
}

void GHOST_WindowCocoa::endIME()
{
  if (opengl_view_) {
    [opengl_view_ endIME];
  }
  else {
    [metal_view_ endIME];
  }
}
#endif /* WITH_INPUT_IME */
