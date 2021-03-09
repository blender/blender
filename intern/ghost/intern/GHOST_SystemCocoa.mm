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

#include "GHOST_SystemCocoa.h"

#include "GHOST_DisplayManagerCocoa.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventDragnDrop.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventString.h"
#include "GHOST_EventTrackpad.h"
#include "GHOST_EventWheel.h"
#include "GHOST_TimerManager.h"
#include "GHOST_TimerTask.h"
#include "GHOST_WindowCocoa.h"
#include "GHOST_WindowManager.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextCGL.h"
#endif

#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerCocoa.h"
#endif

#include "AssertMacros.h"

#import <Cocoa/Cocoa.h>

/* For the currently not ported to Cocoa keyboard layout functions (64bit & 10.6 compatible) */
#include <Carbon/Carbon.h>

#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

#include <mach/mach_time.h>

#pragma mark KeyMap, mouse converters

static GHOST_TButtonMask convertButton(int button)
{
  switch (button) {
    case 0:
      return GHOST_kButtonMaskLeft;
    case 1:
      return GHOST_kButtonMaskRight;
    case 2:
      return GHOST_kButtonMaskMiddle;
    case 3:
      return GHOST_kButtonMaskButton4;
    case 4:
      return GHOST_kButtonMaskButton5;
    case 5:
      return GHOST_kButtonMaskButton6;
    case 6:
      return GHOST_kButtonMaskButton7;
    default:
      return GHOST_kButtonMaskLeft;
  }
}

/**
 * Converts Mac raw-key codes (same for Cocoa & Carbon)
 * into GHOST key codes
 * \param rawCode: The raw physical key code
 * \param recvChar: the character ignoring modifiers (except for shift)
 * \return Ghost key code
 */
static GHOST_TKey convertKey(int rawCode, unichar recvChar, UInt16 keyAction)
{
  // printf("\nrecvchar %c 0x%x",recvChar,recvChar);
  switch (rawCode) {
    /*Physical keycodes not used due to map changes in int'l keyboards
    case kVK_ANSI_A:    return GHOST_kKeyA;
    case kVK_ANSI_B:    return GHOST_kKeyB;
    case kVK_ANSI_C:    return GHOST_kKeyC;
    case kVK_ANSI_D:    return GHOST_kKeyD;
    case kVK_ANSI_E:    return GHOST_kKeyE;
    case kVK_ANSI_F:    return GHOST_kKeyF;
    case kVK_ANSI_G:    return GHOST_kKeyG;
    case kVK_ANSI_H:    return GHOST_kKeyH;
    case kVK_ANSI_I:    return GHOST_kKeyI;
    case kVK_ANSI_J:    return GHOST_kKeyJ;
    case kVK_ANSI_K:    return GHOST_kKeyK;
    case kVK_ANSI_L:    return GHOST_kKeyL;
    case kVK_ANSI_M:    return GHOST_kKeyM;
    case kVK_ANSI_N:    return GHOST_kKeyN;
    case kVK_ANSI_O:    return GHOST_kKeyO;
    case kVK_ANSI_P:    return GHOST_kKeyP;
    case kVK_ANSI_Q:    return GHOST_kKeyQ;
    case kVK_ANSI_R:    return GHOST_kKeyR;
    case kVK_ANSI_S:    return GHOST_kKeyS;
    case kVK_ANSI_T:    return GHOST_kKeyT;
    case kVK_ANSI_U:    return GHOST_kKeyU;
    case kVK_ANSI_V:    return GHOST_kKeyV;
    case kVK_ANSI_W:    return GHOST_kKeyW;
    case kVK_ANSI_X:    return GHOST_kKeyX;
    case kVK_ANSI_Y:    return GHOST_kKeyY;
    case kVK_ANSI_Z:    return GHOST_kKeyZ;*/

    /* Numbers keys mapped to handle some int'l keyboard (e.g. French)*/
    case kVK_ISO_Section:
      return GHOST_kKeyUnknown;
    case kVK_ANSI_1:
      return GHOST_kKey1;
    case kVK_ANSI_2:
      return GHOST_kKey2;
    case kVK_ANSI_3:
      return GHOST_kKey3;
    case kVK_ANSI_4:
      return GHOST_kKey4;
    case kVK_ANSI_5:
      return GHOST_kKey5;
    case kVK_ANSI_6:
      return GHOST_kKey6;
    case kVK_ANSI_7:
      return GHOST_kKey7;
    case kVK_ANSI_8:
      return GHOST_kKey8;
    case kVK_ANSI_9:
      return GHOST_kKey9;
    case kVK_ANSI_0:
      return GHOST_kKey0;

    case kVK_ANSI_Keypad0:
      return GHOST_kKeyNumpad0;
    case kVK_ANSI_Keypad1:
      return GHOST_kKeyNumpad1;
    case kVK_ANSI_Keypad2:
      return GHOST_kKeyNumpad2;
    case kVK_ANSI_Keypad3:
      return GHOST_kKeyNumpad3;
    case kVK_ANSI_Keypad4:
      return GHOST_kKeyNumpad4;
    case kVK_ANSI_Keypad5:
      return GHOST_kKeyNumpad5;
    case kVK_ANSI_Keypad6:
      return GHOST_kKeyNumpad6;
    case kVK_ANSI_Keypad7:
      return GHOST_kKeyNumpad7;
    case kVK_ANSI_Keypad8:
      return GHOST_kKeyNumpad8;
    case kVK_ANSI_Keypad9:
      return GHOST_kKeyNumpad9;
    case kVK_ANSI_KeypadDecimal:
      return GHOST_kKeyNumpadPeriod;
    case kVK_ANSI_KeypadEnter:
      return GHOST_kKeyNumpadEnter;
    case kVK_ANSI_KeypadPlus:
      return GHOST_kKeyNumpadPlus;
    case kVK_ANSI_KeypadMinus:
      return GHOST_kKeyNumpadMinus;
    case kVK_ANSI_KeypadMultiply:
      return GHOST_kKeyNumpadAsterisk;
    case kVK_ANSI_KeypadDivide:
      return GHOST_kKeyNumpadSlash;
    case kVK_ANSI_KeypadClear:
      return GHOST_kKeyUnknown;

    case kVK_F1:
      return GHOST_kKeyF1;
    case kVK_F2:
      return GHOST_kKeyF2;
    case kVK_F3:
      return GHOST_kKeyF3;
    case kVK_F4:
      return GHOST_kKeyF4;
    case kVK_F5:
      return GHOST_kKeyF5;
    case kVK_F6:
      return GHOST_kKeyF6;
    case kVK_F7:
      return GHOST_kKeyF7;
    case kVK_F8:
      return GHOST_kKeyF8;
    case kVK_F9:
      return GHOST_kKeyF9;
    case kVK_F10:
      return GHOST_kKeyF10;
    case kVK_F11:
      return GHOST_kKeyF11;
    case kVK_F12:
      return GHOST_kKeyF12;
    case kVK_F13:
      return GHOST_kKeyF13;
    case kVK_F14:
      return GHOST_kKeyF14;
    case kVK_F15:
      return GHOST_kKeyF15;
    case kVK_F16:
      return GHOST_kKeyF16;
    case kVK_F17:
      return GHOST_kKeyF17;
    case kVK_F18:
      return GHOST_kKeyF18;
    case kVK_F19:
      return GHOST_kKeyF19;
    case kVK_F20:
      return GHOST_kKeyF20;

    case kVK_UpArrow:
      return GHOST_kKeyUpArrow;
    case kVK_DownArrow:
      return GHOST_kKeyDownArrow;
    case kVK_LeftArrow:
      return GHOST_kKeyLeftArrow;
    case kVK_RightArrow:
      return GHOST_kKeyRightArrow;

    case kVK_Return:
      return GHOST_kKeyEnter;
    case kVK_Delete:
      return GHOST_kKeyBackSpace;
    case kVK_ForwardDelete:
      return GHOST_kKeyDelete;
    case kVK_Escape:
      return GHOST_kKeyEsc;
    case kVK_Tab:
      return GHOST_kKeyTab;
    case kVK_Space:
      return GHOST_kKeySpace;

    case kVK_Home:
      return GHOST_kKeyHome;
    case kVK_End:
      return GHOST_kKeyEnd;
    case kVK_PageUp:
      return GHOST_kKeyUpPage;
    case kVK_PageDown:
      return GHOST_kKeyDownPage;

      /*case kVK_ANSI_Minus:      return GHOST_kKeyMinus;
    case kVK_ANSI_Equal:        return GHOST_kKeyEqual;
    case kVK_ANSI_Comma:        return GHOST_kKeyComma;
    case kVK_ANSI_Period:       return GHOST_kKeyPeriod;
    case kVK_ANSI_Slash:        return GHOST_kKeySlash;
    case kVK_ANSI_Semicolon:    return GHOST_kKeySemicolon;
    case kVK_ANSI_Quote:        return GHOST_kKeyQuote;
    case kVK_ANSI_Backslash:    return GHOST_kKeyBackslash;
    case kVK_ANSI_LeftBracket:  return GHOST_kKeyLeftBracket;
    case kVK_ANSI_RightBracket: return GHOST_kKeyRightBracket;
    case kVK_ANSI_Grave:        return GHOST_kKeyAccentGrave;*/

    case kVK_VolumeUp:
    case kVK_VolumeDown:
    case kVK_Mute:
      return GHOST_kKeyUnknown;

    default: {
      /* alphanumerical or punctuation key that is remappable in int'l keyboards */
      if ((recvChar >= 'A') && (recvChar <= 'Z')) {
        return (GHOST_TKey)(recvChar - 'A' + GHOST_kKeyA);
      }
      else if ((recvChar >= 'a') && (recvChar <= 'z')) {
        return (GHOST_TKey)(recvChar - 'a' + GHOST_kKeyA);
      }
      else {
        /* Leopard and Snow Leopard 64bit compatible API*/
        CFDataRef uchrHandle; /*the keyboard layout*/
        TISInputSourceRef kbdTISHandle;

        kbdTISHandle = TISCopyCurrentKeyboardLayoutInputSource();
        uchrHandle = (CFDataRef)TISGetInputSourceProperty(kbdTISHandle,
                                                          kTISPropertyUnicodeKeyLayoutData);
        CFRelease(kbdTISHandle);

        /*get actual character value of the "remappable" keys in int'l keyboards,
        if keyboard layout is not correctly reported (e.g. some non Apple keyboards in Tiger),
        then fallback on using the received charactersIgnoringModifiers */
        if (uchrHandle) {
          UInt32 deadKeyState = 0;
          UniCharCount actualStrLength = 0;

          UCKeyTranslate((UCKeyboardLayout *)CFDataGetBytePtr(uchrHandle),
                         rawCode,
                         keyAction,
                         0,
                         LMGetKbdType(),
                         kUCKeyTranslateNoDeadKeysBit,
                         &deadKeyState,
                         1,
                         &actualStrLength,
                         &recvChar);
        }

        switch (recvChar) {
          case '-':
            return GHOST_kKeyMinus;
          case '+':
            return GHOST_kKeyPlus;
          case '=':
            return GHOST_kKeyEqual;
          case ',':
            return GHOST_kKeyComma;
          case '.':
            return GHOST_kKeyPeriod;
          case '/':
            return GHOST_kKeySlash;
          case ';':
            return GHOST_kKeySemicolon;
          case '\'':
            return GHOST_kKeyQuote;
          case '\\':
            return GHOST_kKeyBackslash;
          case '[':
            return GHOST_kKeyLeftBracket;
          case ']':
            return GHOST_kKeyRightBracket;
          case '`':
            return GHOST_kKeyAccentGrave;
          default:
            return GHOST_kKeyUnknown;
        }
      }
    }
  }
  return GHOST_kKeyUnknown;
}

#pragma mark Utility functions

#define FIRSTFILEBUFLG 512
static bool g_hasFirstFile = false;
static char g_firstFileBuf[512];

// TODO: Need to investigate this.
// Function called too early in creator.c to have g_hasFirstFile == true
extern "C" int GHOST_HACK_getFirstFile(char buf[FIRSTFILEBUFLG])
{
  if (g_hasFirstFile) {
    strncpy(buf, g_firstFileBuf, FIRSTFILEBUFLG - 1);
    buf[FIRSTFILEBUFLG - 1] = '\0';
    return 1;
  }
  else {
    return 0;
  }
}

/* clang-format off */
#pragma mark Cocoa objects

/**
 * CocoaAppDelegate
 * ObjC object to capture applicationShouldTerminate, and send quit event
 */
@interface CocoaAppDelegate : NSObject <NSApplicationDelegate>
{

  GHOST_SystemCocoa *systemCocoa;
}

- (id)init;
- (void)dealloc;
- (void)setSystemCocoa:(GHOST_SystemCocoa *)sysCocoa;
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification;
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (void)applicationWillTerminate:(NSNotification *)aNotification;
- (void)applicationWillBecomeActive:(NSNotification *)aNotification;
- (void)toggleFullScreen:(NSNotification *)notification;
- (void)windowWillClose:(NSNotification *)notification;
@end

@implementation CocoaAppDelegate : NSObject
- (id)init
{
  self = [super init];
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center addObserver:self
             selector:@selector(windowWillClose:)
                 name:NSWindowWillCloseNotification
               object:nil];
  return self;
}

- (void)dealloc
{
  NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
  [center removeObserver:self name:NSWindowWillCloseNotification object:nil];
  [super dealloc];
}

- (void)setSystemCocoa:(GHOST_SystemCocoa *)sysCocoa
{
  systemCocoa = sysCocoa;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
  if (systemCocoa->m_windowFocus) {
    // Raise application to front, convenient when starting from the terminal
    // and important for launching the animation player. we call this after the
    // application finishes launching, as doing it earlier can make us end up
    // with a frontmost window but an inactive application.
    [NSApp activateIgnoringOtherApps:YES];
  }

  [NSEvent setMouseCoalescingEnabled:NO];
}

- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
  return systemCocoa->handleOpenDocumentRequest(filename);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
  /* TODO: implement graceful termination through Cocoa mechanism
   * to avoid session log off to be canceled. */
  /* Note that Cmd+Q is already handled by keyhandler. */
  systemCocoa->handleQuitRequest();
  return NSTerminateCancel;
}

// To avoid canceling a log off process, we must use Cocoa termination process
// And this function is the only chance to perform clean up
// So WM_exit needs to be called directly, as the event loop will never run before termination
- (void)applicationWillTerminate:(NSNotification *)aNotification
{
  /*G.is_break = FALSE; //Let Cocoa perform the termination at the end
  WM_exit(C);*/
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
  systemCocoa->handleApplicationBecomeActiveEvent();
}

- (void)toggleFullScreen:(NSNotification *)notification
{
}

// The purpose of this function is to make sure closing "About" window does not
// leave Blender with no key windows. This is needed due to a custom event loop
// nature of the application: for some reason only using [NSApp run] will ensure
// correct behavior in this case.
//
// This is similar to an issue solved in SDL:
//   https://bugzilla.libsdl.org/show_bug.cgi?id=1825
//
// Our solution is different, since we want Blender to keep track of what is
// the key window during normal operation. In order to do so we exploit the
// fact that "About" window is never in the orderedWindows array: we only force
// key window from here if the closing one is not in the orderedWindows. This
// saves lack of key windows when closing "About", but does not interfere with
// Blender's window manager when closing Blender's windows.
- (void)windowWillClose:(NSNotification *)notification
{
  NSWindow *closing_window = (NSWindow *)[notification object];
  NSInteger index = [[NSApp orderedWindows] indexOfObject:closing_window];
  if (index != NSNotFound) {
    return;
  }
  // Find first suitable window from the current space.
  for (NSWindow *current_window in [NSApp orderedWindows]) {
    if (current_window == closing_window) {
      continue;
    }
    if ([current_window isOnActiveSpace] && [current_window canBecomeKeyWindow]) {
      [current_window makeKeyAndOrderFront:nil];
      return;
    }
  }
  // If that didn't find any windows, we try to find any suitable window of
  // the application.
  for (NSNumber *window_number in [NSWindow windowNumbersWithOptions:0]) {
    NSWindow *current_window = [NSApp windowWithWindowNumber:[window_number integerValue]];
    if (current_window == closing_window) {
      continue;
    }
    if ([current_window canBecomeKeyWindow]) {
      [current_window makeKeyAndOrderFront:nil];
      return;
    }
  }
}

@end

#pragma mark initialization/finalization

/* clang-format on */

GHOST_SystemCocoa::GHOST_SystemCocoa()
{
  int mib[2];
  struct timeval boottime;
  size_t len;
  char *rstring = NULL;

  m_modifierMask = 0;
  m_outsideLoopEventProcessed = false;
  m_needDelayedApplicationBecomeActiveEventProcessing = false;
  m_displayManager = new GHOST_DisplayManagerCocoa();
  GHOST_ASSERT(m_displayManager, "GHOST_SystemCocoa::GHOST_SystemCocoa(): m_displayManager==0\n");
  m_displayManager->initialize();

  // NSEvent timeStamp is given in system uptime, state start date is boot time
  mib[0] = CTL_KERN;
  mib[1] = KERN_BOOTTIME;
  len = sizeof(struct timeval);

  sysctl(mib, 2, &boottime, &len, NULL, 0);
  m_start_time = ((boottime.tv_sec * 1000) + (boottime.tv_usec / 1000));

  // Detect multitouch trackpad
  mib[0] = CTL_HW;
  mib[1] = HW_MODEL;
  sysctl(mib, 2, NULL, &len, NULL, 0);
  rstring = (char *)malloc(len);
  sysctl(mib, 2, rstring, &len, NULL, 0);

  free(rstring);
  rstring = NULL;

  m_ignoreWindowSizedMessages = false;
  m_ignoreMomentumScroll = false;
  m_multiTouchScroll = false;
  m_last_warp_timestamp = 0;
}

GHOST_SystemCocoa::~GHOST_SystemCocoa()
{
}

GHOST_TSuccess GHOST_SystemCocoa::init()
{
  GHOST_TSuccess success = GHOST_System::init();
  if (success) {

#ifdef WITH_INPUT_NDOF
    m_ndofManager = new GHOST_NDOFManagerCocoa(*this);
#endif

    // ProcessSerialNumber psn;

    // Carbon stuff to move window & menu to foreground
    /*if (!GetCurrentProcess(&psn)) {
      TransformProcessType(&psn, kProcessTransformToForegroundApplication);
      SetFrontProcess(&psn);
    }*/

    @autoreleasepool {
      [NSApplication sharedApplication];  // initializes   NSApp

      if ([NSApp mainMenu] == nil) {
        NSMenu *mainMenubar = [[NSMenu alloc] init];
        NSMenuItem *menuItem;
        NSMenu *windowMenu;
        NSMenu *appMenu;

        // Create the application menu
        appMenu = [[NSMenu alloc] initWithTitle:@"Blender"];

        [appMenu addItemWithTitle:@"About Blender"
                           action:@selector(orderFrontStandardAboutPanel:)
                    keyEquivalent:@""];
        [appMenu addItem:[NSMenuItem separatorItem]];

        menuItem = [appMenu addItemWithTitle:@"Hide Blender"
                                      action:@selector(hide:)
                               keyEquivalent:@"h"];
        [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

        menuItem = [appMenu addItemWithTitle:@"Hide Others"
                                      action:@selector(hideOtherApplications:)
                               keyEquivalent:@"h"];
        [menuItem
            setKeyEquivalentModifierMask:(NSEventModifierFlagOption | NSEventModifierFlagCommand)];

        [appMenu addItemWithTitle:@"Show All"
                           action:@selector(unhideAllApplications:)
                    keyEquivalent:@""];

        menuItem = [appMenu addItemWithTitle:@"Quit Blender"
                                      action:@selector(terminate:)
                               keyEquivalent:@"q"];
        [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

        menuItem = [[NSMenuItem alloc] init];
        [menuItem setSubmenu:appMenu];

        [mainMenubar addItem:menuItem];
        [menuItem release];
        [NSApp performSelector:@selector(setAppleMenu:) withObject:appMenu];  // Needed for 10.5
        [appMenu release];

        // Create the window menu
        windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];

        menuItem = [windowMenu addItemWithTitle:@"Minimize"
                                         action:@selector(performMiniaturize:)
                                  keyEquivalent:@"m"];
        [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

        [windowMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        menuItem = [windowMenu addItemWithTitle:@"Enter Full Screen"
                                         action:@selector(toggleFullScreen:)
                                  keyEquivalent:@"f"];
        [menuItem
            setKeyEquivalentModifierMask:NSEventModifierFlagControl | NSEventModifierFlagCommand];

        menuItem = [windowMenu addItemWithTitle:@"Close"
                                         action:@selector(performClose:)
                                  keyEquivalent:@"w"];
        [menuItem setKeyEquivalentModifierMask:NSEventModifierFlagCommand];

        menuItem = [[NSMenuItem alloc] init];
        [menuItem setSubmenu:windowMenu];

        [mainMenubar addItem:menuItem];
        [menuItem release];

        [NSApp setMainMenu:mainMenubar];
        [NSApp setWindowsMenu:windowMenu];
        [windowMenu release];
      }

      if ([NSApp delegate] == nil) {
        CocoaAppDelegate *appDelegate = [[CocoaAppDelegate alloc] init];
        [appDelegate setSystemCocoa:this];
        [NSApp setDelegate:appDelegate];
      }

      // AppKit provides automatic window tabbing. Blender is a single-tabbed application
      // without a macOS tab bar, and should explicitly opt-out of this. This is also
      // controlled by the macOS user default #NSWindowTabbingEnabled.
      NSWindow.allowsAutomaticWindowTabbing = NO;

      [NSApp finishLaunching];
    }
  }
  return success;
}

#pragma mark window management

GHOST_TUns64 GHOST_SystemCocoa::getMilliSeconds() const
{
  // Cocoa equivalent exists in 10.6 ([[NSProcessInfo processInfo] systemUptime])
  struct timeval currentTime;

  gettimeofday(&currentTime, NULL);

  // Return timestamp of system uptime

  return ((currentTime.tv_sec * 1000) + (currentTime.tv_usec / 1000) - m_start_time);
}

GHOST_TUns8 GHOST_SystemCocoa::getNumDisplays() const
{
  // Note that OS X supports monitor hot plug
  // We do not support multiple monitors at the moment
  @autoreleasepool {
    return NSScreen.screens.count;
  }
}

void GHOST_SystemCocoa::getMainDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const
{
  @autoreleasepool {
    // Get visible frame, that is frame excluding dock and top menu bar
    NSRect frame = [[NSScreen mainScreen] visibleFrame];

    // Returns max window contents (excluding title bar...)
    NSRect contentRect = [NSWindow
        contentRectForFrameRect:frame
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable)];

    width = contentRect.size.width;
    height = contentRect.size.height;
  }
}

void GHOST_SystemCocoa::getAllDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const
{
  /* TODO! */
  getMainDisplayDimensions(width, height);
}

GHOST_IWindow *GHOST_SystemCocoa::createWindow(const char *title,
                                               GHOST_TInt32 left,
                                               GHOST_TInt32 top,
                                               GHOST_TUns32 width,
                                               GHOST_TUns32 height,
                                               GHOST_TWindowState state,
                                               GHOST_TDrawingContextType type,
                                               GHOST_GLSettings glSettings,
                                               const bool exclusive,
                                               const bool is_dialog,
                                               const GHOST_IWindow *parentWindow)
{
  GHOST_IWindow *window = NULL;
  @autoreleasepool {

    // Get the available rect for including window contents
    NSRect frame = [[NSScreen mainScreen] visibleFrame];
    NSRect contentRect = [NSWindow
        contentRectForFrameRect:frame
                      styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable)];

    GHOST_TInt32 bottom = (contentRect.size.height - 1) - height - top;

    // Ensures window top left is inside this available rect
    left = left > contentRect.origin.x ? left : contentRect.origin.x;
    // Add contentRect.origin.y to respect docksize
    bottom = bottom > contentRect.origin.y ? bottom + contentRect.origin.y : contentRect.origin.y;

    window = new GHOST_WindowCocoa(this,
                                   title,
                                   left,
                                   bottom,
                                   width,
                                   height,
                                   state,
                                   type,
                                   glSettings.flags & GHOST_glStereoVisual,
                                   glSettings.flags & GHOST_glDebugContext,
                                   is_dialog,
                                   (GHOST_WindowCocoa *)parentWindow);

    if (window->getValid()) {
      // Store the pointer to the window
      GHOST_ASSERT(m_windowManager, "m_windowManager not initialized");
      m_windowManager->addWindow(window);
      m_windowManager->setActiveWindow(window);
      /* Need to tell window manager the new window is the active one
       * (Cocoa does not send the event activate upon window creation). */
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window));
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
    }
    else {
      GHOST_PRINT("GHOST_SystemCocoa::createWindow(): window invalid\n");
      delete window;
      window = NULL;
    }
  }
  return window;
}

/**
 * Create a new offscreen context.
 * Never explicitly delete the context, use #disposeContext() instead.
 * \return The new context (or 0 if creation failed).
 */
GHOST_IContext *GHOST_SystemCocoa::createOffscreenContext(GHOST_GLSettings glSettings)
{
  GHOST_Context *context = new GHOST_ContextCGL(false, NULL, NULL, NULL);
  if (context->initializeDrawingContext())
    return context;
  else
    delete context;

  return NULL;
}

/**
 * Dispose of a context.
 * \param context: Pointer to the context to be disposed.
 * \return Indication of success.
 */
GHOST_TSuccess GHOST_SystemCocoa::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

/**
 * \note : returns coordinates in Cocoa screen coordinates
 */
GHOST_TSuccess GHOST_SystemCocoa::getCursorPosition(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
  NSPoint mouseLoc = [NSEvent mouseLocation];

  // Returns the mouse location in screen coordinates
  x = (GHOST_TInt32)mouseLoc.x;
  y = (GHOST_TInt32)mouseLoc.y;
  return GHOST_kSuccess;
}

/**
 * \note : expect Cocoa screen coordinates
 */
GHOST_TSuccess GHOST_SystemCocoa::setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y)
{
  GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)m_windowManager->getActiveWindow();
  if (!window)
    return GHOST_kFailure;

  // Cursor and mouse dissociation placed here not to interfere with continuous grab
  // (in cont. grab setMouseCursorPosition is directly called)
  CGAssociateMouseAndMouseCursorPosition(false);
  setMouseCursorPosition(x, y);
  CGAssociateMouseAndMouseCursorPosition(true);

  // Force mouse move event (not pushed by Cocoa)
  pushEvent(new GHOST_EventCursor(
      getMilliSeconds(), GHOST_kEventCursorMove, window, x, y, window->GetCocoaTabletData()));
  m_outsideLoopEventProcessed = true;

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::setMouseCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y)
{
  float xf = (float)x, yf = (float)y;
  GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)m_windowManager->getActiveWindow();
  if (!window)
    return GHOST_kFailure;

  @autoreleasepool {
    NSScreen *windowScreen = window->getScreen();
    NSRect screenRect = [windowScreen frame];

    // Set position relative to current screen
    xf -= screenRect.origin.x;
    yf -= screenRect.origin.y;

    // Quartz Display Services uses the old coordinates (top left origin)
    yf = screenRect.size.height - yf;

    CGDisplayMoveCursorToPoint((CGDirectDisplayID)[[[windowScreen deviceDescription]
                                   objectForKey:@"NSScreenNumber"] unsignedIntValue],
                               CGPointMake(xf, yf));

    // See https://stackoverflow.com/a/17559012. By default, hardware events
    // will be suppressed for 500ms after a synthetic mouse event. For unknown
    // reasons CGEventSourceSetLocalEventsSuppressionInterval does not work,
    // however calling CGAssociateMouseAndMouseCursorPosition also removes the
    // delay, even if this is undocumented.
    CGAssociateMouseAndMouseCursorPosition(true);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  keys.set(GHOST_kModifierKeyOS, (m_modifierMask & NSEventModifierFlagCommand) ? true : false);
  keys.set(GHOST_kModifierKeyLeftAlt, (m_modifierMask & NSEventModifierFlagOption) ? true : false);
  keys.set(GHOST_kModifierKeyLeftShift,
           (m_modifierMask & NSEventModifierFlagShift) ? true : false);
  keys.set(GHOST_kModifierKeyLeftControl,
           (m_modifierMask & NSEventModifierFlagControl) ? true : false);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::getButtons(GHOST_Buttons &buttons) const
{
  UInt32 button_state = GetCurrentEventButtonState();

  buttons.clear();
  buttons.set(GHOST_kButtonMaskLeft, button_state & (1 << 0));
  buttons.set(GHOST_kButtonMaskRight, button_state & (1 << 1));
  buttons.set(GHOST_kButtonMaskMiddle, button_state & (1 << 2));
  buttons.set(GHOST_kButtonMaskButton4, button_state & (1 << 3));
  buttons.set(GHOST_kButtonMaskButton5, button_state & (1 << 4));
  return GHOST_kSuccess;
}

#pragma mark Event handlers

/**
 * The event queue polling function
 */
bool GHOST_SystemCocoa::processEvents(bool waitForEvent)
{
  bool anyProcessed = false;
  NSEvent *event;

  // TODO : implement timer ??
#if 0
  do {
    GHOST_TimerManager* timerMgr = getTimerManager();

    if (waitForEvent) {
      GHOST_TUns64 next = timerMgr->nextFireTime();
      double timeOut;

      if (next == GHOST_kFireTimeNever) {
        timeOut = kEventDurationForever;
      }
      else {
        timeOut = (double)(next - getMilliSeconds())/1000.0;
        if (timeOut < 0.0)
          timeOut = 0.0;
      }

      ::ReceiveNextEvent(0, NULL, timeOut, false, &event);
    }

    if (timerMgr->fireTimers(getMilliSeconds())) {
      anyProcessed = true;
    }
#endif
  do {
    @autoreleasepool {
      event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                 untilDate:[NSDate distantPast]
                                    inMode:NSDefaultRunLoopMode
                                   dequeue:YES];
      if (event == nil) {
        break;
      }

      anyProcessed = true;

      // Send event to NSApp to ensure Mac wide events are handled,
      // this will send events to CocoaWindow which will call back
      // to handleKeyEvent, handleMouseEvent and handleTabletEvent

      // There is on special exception for ctrl+(shift)+tab. We do not
      // get keyDown events delivered to the view because they are
      // special hotkeys to switch between views, so override directly

      if ([event type] == NSEventTypeKeyDown && [event keyCode] == kVK_Tab &&
          ([event modifierFlags] & NSEventModifierFlagControl)) {
        handleKeyEvent(event);
      }
      else {
        // For some reason NSApp is swallowing the key up events when modifier
        // key is pressed, even if there seems to be no apparent reason to do
        // so, as a workaround we always handle these up events.
        if ([event type] == NSEventTypeKeyUp &&
            ([event modifierFlags] & (NSEventModifierFlagCommand | NSEventModifierFlagOption)))
          handleKeyEvent(event);

        [NSApp sendEvent:event];
      }
    }
  } while (event != nil);
#if 0
  } while (waitForEvent && !anyProcessed); // Needed only for timer implementation
#endif

  if (m_needDelayedApplicationBecomeActiveEventProcessing)
    handleApplicationBecomeActiveEvent();

  if (m_outsideLoopEventProcessed) {
    m_outsideLoopEventProcessed = false;
    return true;
  }

  m_ignoreWindowSizedMessages = false;

  return anyProcessed;
}

// Note: called from NSApplication delegate
GHOST_TSuccess GHOST_SystemCocoa::handleApplicationBecomeActiveEvent()
{
  for (GHOST_IWindow *iwindow : m_windowManager->getWindows()) {
    GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)iwindow;
    if (window->isDialog()) {
      [window->getCocoaWindow() makeKeyAndOrderFront:nil];
    }
  }

  // Update the modifiers key mask, as its status may have changed when the application
  // was not active (that is when update events are sent to another application).
  unsigned int modifiers;
  GHOST_IWindow *window = m_windowManager->getActiveWindow();

  if (!window) {
    m_needDelayedApplicationBecomeActiveEventProcessing = true;
    return GHOST_kFailure;
  }
  else
    m_needDelayedApplicationBecomeActiveEventProcessing = false;

  modifiers = [[[NSApplication sharedApplication] currentEvent] modifierFlags];

  if ((modifiers & NSEventModifierFlagShift) != (m_modifierMask & NSEventModifierFlagShift)) {
    pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                 (modifiers & NSEventModifierFlagShift) ? GHOST_kEventKeyDown :
                                                                          GHOST_kEventKeyUp,
                                 window,
                                 GHOST_kKeyLeftShift,
                                 false));
  }
  if ((modifiers & NSEventModifierFlagControl) != (m_modifierMask & NSEventModifierFlagControl)) {
    pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                 (modifiers & NSEventModifierFlagControl) ? GHOST_kEventKeyDown :
                                                                            GHOST_kEventKeyUp,
                                 window,
                                 GHOST_kKeyLeftControl,
                                 false));
  }
  if ((modifiers & NSEventModifierFlagOption) != (m_modifierMask & NSEventModifierFlagOption)) {
    pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                 (modifiers & NSEventModifierFlagOption) ? GHOST_kEventKeyDown :
                                                                           GHOST_kEventKeyUp,
                                 window,
                                 GHOST_kKeyLeftAlt,
                                 false));
  }
  if ((modifiers & NSEventModifierFlagCommand) != (m_modifierMask & NSEventModifierFlagCommand)) {
    pushEvent(new GHOST_EventKey(getMilliSeconds(),
                                 (modifiers & NSEventModifierFlagCommand) ? GHOST_kEventKeyDown :
                                                                            GHOST_kEventKeyUp,
                                 window,
                                 GHOST_kKeyOS,
                                 false));
  }

  m_modifierMask = modifiers;

  m_outsideLoopEventProcessed = true;
  return GHOST_kSuccess;
}

bool GHOST_SystemCocoa::hasDialogWindow()
{
  for (GHOST_IWindow *iwindow : m_windowManager->getWindows()) {
    GHOST_WindowCocoa *window = (GHOST_WindowCocoa *)iwindow;
    if (window->isDialog()) {
      return true;
    }
  }
  return false;
}

void GHOST_SystemCocoa::notifyExternalEventProcessed()
{
  m_outsideLoopEventProcessed = true;
}

// Note: called from NSWindow delegate
GHOST_TSuccess GHOST_SystemCocoa::handleWindowEvent(GHOST_TEventType eventType,
                                                    GHOST_WindowCocoa *window)
{
  NSArray *windowsList;
  windowsList = [NSApp orderedWindows];
  if (!validWindow(window)) {
    return GHOST_kFailure;
  }
  switch (eventType) {
    case GHOST_kEventWindowClose:
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowClose, window));
      break;
    case GHOST_kEventWindowActivate:
      m_windowManager->setActiveWindow(window);
      window->loadCursor(window->getCursorVisibility(), window->getCursorShape());
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowActivate, window));
      break;
    case GHOST_kEventWindowDeactivate:
      m_windowManager->setWindowInactive(window);
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowDeactivate, window));
      break;
    case GHOST_kEventWindowUpdate:
      if (m_nativePixel) {
        window->setNativePixelSize();
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventNativeResolutionChange, window));
      }
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowUpdate, window));
      break;
    case GHOST_kEventWindowMove:
      pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowMove, window));
      break;
    case GHOST_kEventWindowSize:
      if (!m_ignoreWindowSizedMessages) {
        // Enforce only one resize message per event loop
        // (coalescing all the live resize messages)
        window->updateDrawingContext();
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventWindowSize, window));
        // Mouse up event is trapped by the resizing event loop,
        // so send it anyway to the window manager.
        pushEvent(new GHOST_EventButton(getMilliSeconds(),
                                        GHOST_kEventButtonUp,
                                        window,
                                        GHOST_kButtonMaskLeft,
                                        GHOST_TABLET_DATA_NONE));
        // m_ignoreWindowSizedMessages = true;
      }
      break;
    case GHOST_kEventNativeResolutionChange:

      if (m_nativePixel) {
        window->setNativePixelSize();
        pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventNativeResolutionChange, window));
      }

    default:
      return GHOST_kFailure;
      break;
  }

  m_outsideLoopEventProcessed = true;
  return GHOST_kSuccess;
}

// Note: called from NSWindow subclass
GHOST_TSuccess GHOST_SystemCocoa::handleDraggingEvent(GHOST_TEventType eventType,
                                                      GHOST_TDragnDropTypes draggedObjectType,
                                                      GHOST_WindowCocoa *window,
                                                      int mouseX,
                                                      int mouseY,
                                                      void *data)
{
  if (!validWindow(window)) {
    return GHOST_kFailure;
  }
  switch (eventType) {
    case GHOST_kEventDraggingEntered:
    case GHOST_kEventDraggingUpdated:
    case GHOST_kEventDraggingExited:
      pushEvent(new GHOST_EventDragnDrop(
          getMilliSeconds(), eventType, draggedObjectType, window, mouseX, mouseY, NULL));
      break;

    case GHOST_kEventDraggingDropDone: {
      GHOST_TUns8 *temp_buff;
      GHOST_TStringArray *strArray;
      NSArray *droppedArray;
      size_t pastedTextSize;
      NSString *droppedStr;
      GHOST_TEventDataPtr eventData;
      int i;

      if (!data)
        return GHOST_kFailure;

      switch (draggedObjectType) {
        case GHOST_kDragnDropTypeFilenames:
          droppedArray = (NSArray *)data;

          strArray = (GHOST_TStringArray *)malloc(sizeof(GHOST_TStringArray));
          if (!strArray)
            return GHOST_kFailure;

          strArray->count = [droppedArray count];
          if (strArray->count == 0) {
            free(strArray);
            return GHOST_kFailure;
          }

          strArray->strings = (GHOST_TUns8 **)malloc(strArray->count * sizeof(GHOST_TUns8 *));

          for (i = 0; i < strArray->count; i++) {
            droppedStr = [droppedArray objectAtIndex:i];

            pastedTextSize = [droppedStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
            temp_buff = (GHOST_TUns8 *)malloc(pastedTextSize + 1);

            if (!temp_buff) {
              strArray->count = i;
              break;
            }

            strncpy((char *)temp_buff,
                    [droppedStr cStringUsingEncoding:NSUTF8StringEncoding],
                    pastedTextSize);
            temp_buff[pastedTextSize] = '\0';

            strArray->strings[i] = temp_buff;
          }

          eventData = (GHOST_TEventDataPtr)strArray;
          break;

        case GHOST_kDragnDropTypeString:
          droppedStr = (NSString *)data;
          pastedTextSize = [droppedStr lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

          temp_buff = (GHOST_TUns8 *)malloc(pastedTextSize + 1);

          if (temp_buff == NULL) {
            return GHOST_kFailure;
          }

          strncpy((char *)temp_buff,
                  [droppedStr cStringUsingEncoding:NSUTF8StringEncoding],
                  pastedTextSize);

          temp_buff[pastedTextSize] = '\0';

          eventData = (GHOST_TEventDataPtr)temp_buff;
          break;

        case GHOST_kDragnDropTypeBitmap: {
          NSImage *droppedImg = (NSImage *)data;
          NSSize imgSize = [droppedImg size];
          ImBuf *ibuf = NULL;
          GHOST_TUns8 *rasterRGB = NULL;
          GHOST_TUns8 *rasterRGBA = NULL;
          GHOST_TUns8 *toIBuf = NULL;
          int x, y, to_i, from_i;
          NSBitmapImageRep *blBitmapFormatImageRGB, *blBitmapFormatImageRGBA, *bitmapImage = nil;
          NSEnumerator *enumerator;
          NSImageRep *representation;

          ibuf = IMB_allocImBuf(imgSize.width, imgSize.height, 32, IB_rect);
          if (!ibuf) {
            [droppedImg release];
            return GHOST_kFailure;
          }

          /*Get the bitmap of the image*/
          enumerator = [[droppedImg representations] objectEnumerator];
          while ((representation = [enumerator nextObject])) {
            if ([representation isKindOfClass:[NSBitmapImageRep class]]) {
              bitmapImage = (NSBitmapImageRep *)representation;
              break;
            }
          }
          if (bitmapImage == nil)
            return GHOST_kFailure;

          if (([bitmapImage bitsPerPixel] == 32) && (([bitmapImage bitmapFormat] & 0x5) == 0) &&
              ![bitmapImage isPlanar]) {
            /* Try a fast copy if the image is a meshed RGBA 32bit bitmap*/
            toIBuf = (GHOST_TUns8 *)ibuf->rect;
            rasterRGB = (GHOST_TUns8 *)[bitmapImage bitmapData];
            for (y = 0; y < imgSize.height; y++) {
              to_i = (imgSize.height - y - 1) * imgSize.width;
              from_i = y * imgSize.width;
              memcpy(toIBuf + 4 * to_i, rasterRGB + 4 * from_i, 4 * imgSize.width);
            }
          }
          else {
            /* Tell cocoa image resolution is same as current system one */
            [bitmapImage setSize:imgSize];

            /* Convert the image in a RGBA 32bit format */
            /* As Core Graphics does not support contexts with non premutliplied alpha,
             we need to get alpha key values in a separate batch */

            /* First get RGB values w/o Alpha to avoid pre-multiplication,
             * 32bit but last byte is unused */
            blBitmapFormatImageRGB = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:NULL
                              pixelsWide:imgSize.width
                              pixelsHigh:imgSize.height
                           bitsPerSample:8
                         samplesPerPixel:3
                                hasAlpha:NO
                                isPlanar:NO
                          colorSpaceName:NSDeviceRGBColorSpace
                            bitmapFormat:(NSBitmapFormat)0
                             bytesPerRow:4 * imgSize.width
                            bitsPerPixel:32 /*RGB format padded to 32bits*/];

            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext
                setCurrentContext:[NSGraphicsContext
                                      graphicsContextWithBitmapImageRep:blBitmapFormatImageRGB]];
            [bitmapImage draw];
            [NSGraphicsContext restoreGraphicsState];

            rasterRGB = (GHOST_TUns8 *)[blBitmapFormatImageRGB bitmapData];
            if (rasterRGB == NULL) {
              [bitmapImage release];
              [blBitmapFormatImageRGB release];
              [droppedImg release];
              return GHOST_kFailure;
            }

            /* Then get Alpha values by getting the RGBA image (that is pre-multiplied BTW) */
            blBitmapFormatImageRGBA = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:NULL
                              pixelsWide:imgSize.width
                              pixelsHigh:imgSize.height
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSDeviceRGBColorSpace
                            bitmapFormat:(NSBitmapFormat)0
                             bytesPerRow:4 * imgSize.width
                            bitsPerPixel:32 /* RGBA */];

            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext
                setCurrentContext:[NSGraphicsContext
                                      graphicsContextWithBitmapImageRep:blBitmapFormatImageRGBA]];
            [bitmapImage draw];
            [NSGraphicsContext restoreGraphicsState];

            rasterRGBA = (GHOST_TUns8 *)[blBitmapFormatImageRGBA bitmapData];
            if (rasterRGBA == NULL) {
              [bitmapImage release];
              [blBitmapFormatImageRGB release];
              [blBitmapFormatImageRGBA release];
              [droppedImg release];
              return GHOST_kFailure;
            }

            /*Copy the image to ibuf, flipping it vertically*/
            toIBuf = (GHOST_TUns8 *)ibuf->rect;
            for (y = 0; y < imgSize.height; y++) {
              for (x = 0; x < imgSize.width; x++) {
                to_i = (imgSize.height - y - 1) * imgSize.width + x;
                from_i = y * imgSize.width + x;

                toIBuf[4 * to_i] = rasterRGB[4 * from_i];          /* R */
                toIBuf[4 * to_i + 1] = rasterRGB[4 * from_i + 1];  /* G */
                toIBuf[4 * to_i + 2] = rasterRGB[4 * from_i + 2];  /* B */
                toIBuf[4 * to_i + 3] = rasterRGBA[4 * from_i + 3]; /* A */
              }
            }

            [blBitmapFormatImageRGB release];
            [blBitmapFormatImageRGBA release];
            [droppedImg release];
          }

          eventData = (GHOST_TEventDataPtr)ibuf;

          break;
        }
        default:
          return GHOST_kFailure;
          break;
      }
      pushEvent(new GHOST_EventDragnDrop(
          getMilliSeconds(), eventType, draggedObjectType, window, mouseX, mouseY, eventData));

      break;
    }
    default:
      return GHOST_kFailure;
  }
  m_outsideLoopEventProcessed = true;
  return GHOST_kSuccess;
}

void GHOST_SystemCocoa::handleQuitRequest()
{
  GHOST_Window *window = (GHOST_Window *)m_windowManager->getActiveWindow();

  // Discard quit event if we are in cursor grab sequence
  if (window && window->getCursorGrabModeIsWarp())
    return;

  // Push the event to Blender so it can open a dialog if needed
  pushEvent(new GHOST_Event(getMilliSeconds(), GHOST_kEventQuitRequest, window));
  m_outsideLoopEventProcessed = true;
}

bool GHOST_SystemCocoa::handleOpenDocumentRequest(void *filepathStr)
{
  NSString *filepath = (NSString *)filepathStr;
  NSArray *windowsList;
  char *temp_buff;
  size_t filenameTextSize;

  /* Check for blender opened windows and make the frontmost key. In case blender
   * is minimized, opened on another desktop space, or in full-screen mode. */
  windowsList = [NSApp orderedWindows];
  if ([windowsList count]) {
    [[windowsList objectAtIndex:0] makeKeyAndOrderFront:nil];
  }

  GHOST_Window *window = (GHOST_Window *)m_windowManager->getActiveWindow();

  if (!window) {
    return NO;
  }

  /* Discard event if we are in cursor grab sequence,
   * it'll lead to "stuck cursor" situation if the alert panel is raised. */
  if (window && window->getCursorGrabModeIsWarp()) {
    return NO;
  }

  filenameTextSize = [filepath lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
  temp_buff = (char *)malloc(filenameTextSize + 1);

  if (temp_buff == NULL) {
    return GHOST_kFailure;
  }

  strncpy(temp_buff, [filepath cStringUsingEncoding:NSUTF8StringEncoding], filenameTextSize);
  temp_buff[filenameTextSize] = '\0';

  pushEvent(new GHOST_EventString(
      getMilliSeconds(), GHOST_kEventOpenMainFile, window, (GHOST_TEventDataPtr)temp_buff));

  return YES;
}

GHOST_TSuccess GHOST_SystemCocoa::handleTabletEvent(void *eventPtr, short eventType)
{
  NSEvent *event = (NSEvent *)eventPtr;
  GHOST_IWindow *window;

  window = m_windowManager->getWindowAssociatedWithOSWindow((void *)[event window]);
  if (!window) {
    // printf("\nW failure for event 0x%x",[event type]);
    return GHOST_kFailure;
  }

  GHOST_TabletData &ct = ((GHOST_WindowCocoa *)window)->GetCocoaTabletData();

  switch (eventType) {
    case NSEventTypeTabletPoint:
      // workaround 2 cornercases:
      // 1. if [event isEnteringProximity] was not triggered since program-start
      // 2. device is not sending [event pointingDeviceType], due no eraser
      if (ct.Active == GHOST_kTabletModeNone)
        ct.Active = GHOST_kTabletModeStylus;

      ct.Pressure = [event pressure];
      ct.Xtilt = [event tilt].x;
      ct.Ytilt = [event tilt].y;
      break;

    case NSEventTypeTabletProximity:
      /* Reset tablet data when device enters proximity or leaves. */
      ct = GHOST_TABLET_DATA_NONE;
      if ([event isEnteringProximity]) {
        /* Pointer is entering tablet area proximity. */
        switch ([event pointingDeviceType]) {
          case NSPointingDeviceTypePen:
            ct.Active = GHOST_kTabletModeStylus;
            break;
          case NSPointingDeviceTypeEraser:
            ct.Active = GHOST_kTabletModeEraser;
            break;
          case NSPointingDeviceTypeCursor:
          case NSPointingDeviceTypeUnknown:
          default:
            break;
        }
      }
      break;

    default:
      GHOST_ASSERT(FALSE, "GHOST_SystemCocoa::handleTabletEvent : unknown event received");
      return GHOST_kFailure;
      break;
  }
  return GHOST_kSuccess;
}

bool GHOST_SystemCocoa::handleTabletEvent(void *eventPtr)
{
  NSEvent *event = (NSEvent *)eventPtr;

  switch ([event subtype]) {
    case NSEventSubtypeTabletPoint:
      handleTabletEvent(eventPtr, NSEventTypeTabletPoint);
      return true;
    case NSEventSubtypeTabletProximity:
      handleTabletEvent(eventPtr, NSEventTypeTabletProximity);
      return true;
    default:
      // No tablet event included : do nothing
      return false;
  }
}

GHOST_TSuccess GHOST_SystemCocoa::handleMouseEvent(void *eventPtr)
{
  NSEvent *event = (NSEvent *)eventPtr;
  GHOST_WindowCocoa *window;
  CocoaWindow *cocoawindow;

  /* [event window] returns other windows if mouse-over, that's OSX input standard
     however, if mouse exits window(s), the windows become inactive, until you click.
     We then fall back to the active window from ghost */
  window = (GHOST_WindowCocoa *)m_windowManager->getWindowAssociatedWithOSWindow(
      (void *)[event window]);
  if (!window) {
    window = (GHOST_WindowCocoa *)m_windowManager->getActiveWindow();
    if (!window) {
      // printf("\nW failure for event 0x%x",[event type]);
      return GHOST_kFailure;
    }
  }

  cocoawindow = (CocoaWindow *)window->getOSWindow();

  switch ([event type]) {
    case NSEventTypeLeftMouseDown:
      handleTabletEvent(event);  // Update window tablet state to be included in event.
      pushEvent(new GHOST_EventButton([event timestamp] * 1000,
                                      GHOST_kEventButtonDown,
                                      window,
                                      GHOST_kButtonMaskLeft,
                                      window -> GetCocoaTabletData()));
      break;
    case NSEventTypeRightMouseDown:
      handleTabletEvent(event);  // Update window tablet state to be included in event.
      pushEvent(new GHOST_EventButton([event timestamp] * 1000,
                                      GHOST_kEventButtonDown,
                                      window,
                                      GHOST_kButtonMaskRight,
                                      window -> GetCocoaTabletData()));
      break;
    case NSEventTypeOtherMouseDown:
      handleTabletEvent(event);  // Handle tablet events combined with mouse events
      pushEvent(new GHOST_EventButton([event timestamp] * 1000,
                                      GHOST_kEventButtonDown,
                                      window,
                                      convertButton([event buttonNumber]),
                                      window -> GetCocoaTabletData()));
      break;

    case NSEventTypeLeftMouseUp:
      handleTabletEvent(event);  // Update window tablet state to be included in event.
      pushEvent(new GHOST_EventButton([event timestamp] * 1000,
                                      GHOST_kEventButtonUp,
                                      window,
                                      GHOST_kButtonMaskLeft,
                                      window -> GetCocoaTabletData()));
      break;
    case NSEventTypeRightMouseUp:
      handleTabletEvent(event);  // Update window tablet state to be included in event.
      pushEvent(new GHOST_EventButton([event timestamp] * 1000,
                                      GHOST_kEventButtonUp,
                                      window,
                                      GHOST_kButtonMaskRight,
                                      window -> GetCocoaTabletData()));
      break;
    case NSEventTypeOtherMouseUp:
      handleTabletEvent(event);  // Update window tablet state to be included in event.
      pushEvent(new GHOST_EventButton([event timestamp] * 1000,
                                      GHOST_kEventButtonUp,
                                      window,
                                      convertButton([event buttonNumber]),
                                      window -> GetCocoaTabletData()));
      break;

    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
      handleTabletEvent(event);  // Update window tablet state to be included in event.

    case NSEventTypeMouseMoved: {
      GHOST_TGrabCursorMode grab_mode = window->getCursorGrabMode();

      /* TODO: CHECK IF THIS IS A TABLET EVENT */
      bool is_tablet = false;

      if (is_tablet && window->getCursorGrabModeIsWarp()) {
        grab_mode = GHOST_kGrabDisable;
      }

      switch (grab_mode) {
        case GHOST_kGrabHide:  // Cursor hidden grab operation : no cursor move
        {
          GHOST_TInt32 x_warp, y_warp, x_accum, y_accum, x, y;

          window->getCursorGrabInitPos(x_warp, y_warp);
          window->screenToClientIntern(x_warp, y_warp, x_warp, y_warp);

          window->getCursorGrabAccum(x_accum, y_accum);
          x_accum += [event deltaX];
          y_accum += -[event
              deltaY];  // Strange Apple implementation (inverted coordinates for the deltaY) ...
          window->setCursorGrabAccum(x_accum, y_accum);

          window->clientToScreenIntern(x_warp + x_accum, y_warp + y_accum, x, y);
          pushEvent(new GHOST_EventCursor([event timestamp] * 1000,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x,
                                          y,
                                          window -> GetCocoaTabletData()));
          break;
        }
        case GHOST_kGrabWrap:  // Wrap cursor at area/window boundaries
        {
          NSTimeInterval timestamp = [event timestamp];
          if (timestamp < m_last_warp_timestamp) {
            /* After warping we can still receive older unwarped mouse events,
             * ignore those. */
            break;
          }

          NSPoint mousePos = [event locationInWindow];
          GHOST_TInt32 x_mouse = mousePos.x;
          GHOST_TInt32 y_mouse = mousePos.y;
          GHOST_Rect bounds, windowBounds, correctedBounds;

          /* fallback to window bounds */
          if (window->getCursorGrabBounds(bounds) == GHOST_kFailure)
            window->getClientBounds(bounds);

          /* Switch back to Cocoa coordinates orientation
           * (y=0 at bottom, the same as blender internal BTW!), and to client coordinates. */
          window->getClientBounds(windowBounds);
          window->screenToClient(bounds.m_l, bounds.m_b, correctedBounds.m_l, correctedBounds.m_t);
          window->screenToClient(bounds.m_r, bounds.m_t, correctedBounds.m_r, correctedBounds.m_b);
          correctedBounds.m_b = (windowBounds.m_b - windowBounds.m_t) - correctedBounds.m_b;
          correctedBounds.m_t = (windowBounds.m_b - windowBounds.m_t) - correctedBounds.m_t;

          // Get accumulation from previous mouse warps
          GHOST_TInt32 x_accum, y_accum;
          window->getCursorGrabAccum(x_accum, y_accum);

          // Warp mouse cursor if needed
          GHOST_TInt32 warped_x_mouse = x_mouse;
          GHOST_TInt32 warped_y_mouse = y_mouse;

          correctedBounds.wrapPoint(
              warped_x_mouse, warped_y_mouse, 4, window->getCursorGrabAxis());

          // Set new cursor position
          if (x_mouse != warped_x_mouse || y_mouse != warped_y_mouse) {
            GHOST_TInt32 warped_x, warped_y;
            window->clientToScreenIntern(warped_x_mouse, warped_y_mouse, warped_x, warped_y);
            setMouseCursorPosition(warped_x, warped_y); /* wrap */
            window->setCursorGrabAccum(x_accum + (x_mouse - warped_x_mouse),
                                       y_accum + (y_mouse - warped_y_mouse));

            /* This is the current time that matches NSEvent timestamp. */
            m_last_warp_timestamp = mach_absolute_time() * 1e-9;
          }

          // Generate event
          GHOST_TInt32 x, y;
          window->clientToScreenIntern(x_mouse + x_accum, y_mouse + y_accum, x, y);
          pushEvent(new GHOST_EventCursor([event timestamp] * 1000,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x,
                                          y,
                                          window -> GetCocoaTabletData()));
          break;
        }
        default: {
          // Normal cursor operation: send mouse position in window
          NSPoint mousePos = [event locationInWindow];
          GHOST_TInt32 x, y;

          window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
          pushEvent(new GHOST_EventCursor([event timestamp] * 1000,
                                          GHOST_kEventCursorMove,
                                          window,
                                          x,
                                          y,
                                          window -> GetCocoaTabletData()));
          break;
        }
      }
    } break;

    case NSEventTypeScrollWheel: {
      NSEventPhase momentumPhase = NSEventPhaseNone;
      NSEventPhase phase = NSEventPhaseNone;

      momentumPhase = [event momentumPhase];
      phase = [event phase];

      /* when pressing a key while momentum scrolling continues after
       * lifting fingers off the trackpad, the action can unexpectedly
       * change from e.g. scrolling to zooming. this works around the
       * issue by ignoring momentum scroll after a key press */
      if (momentumPhase) {
        if (m_ignoreMomentumScroll)
          break;
      }
      else {
        m_ignoreMomentumScroll = false;
      }

      /* we assume phases are only set for gestures from trackpad or magic
       * mouse events. note that using tablet at the same time may not work
       * since this is a static variable */
      if (phase == NSEventPhaseBegan)
        m_multiTouchScroll = true;
      else if (phase == NSEventPhaseEnded)
        m_multiTouchScroll = false;

      /* Standard scroll-wheel case, if no swiping happened,
       * and no momentum (kinetic scroll) works. */
      if (!m_multiTouchScroll && momentumPhase == NSEventPhaseNone) {
        GHOST_TInt32 delta;

        double deltaF = [event deltaY];

        if (deltaF == 0.0)
          deltaF = [event deltaX];  // make blender decide if it's horizontal scroll
        if (deltaF == 0.0)
          break;  // discard trackpad delta=0 events

        delta = deltaF > 0.0 ? 1 : -1;
        pushEvent(new GHOST_EventWheel([event timestamp] * 1000, window, delta));
      }
      else {
        NSPoint mousePos = [event locationInWindow];
        GHOST_TInt32 x, y;
        double dx;
        double dy;

        /* with 10.7 nice scrolling deltas are supported */
        dx = [event scrollingDeltaX];
        dy = [event scrollingDeltaY];

        /* However, wacom tablet (intuos5) needs old deltas,
         * it then has momentum and phase at zero. */
        if (phase == NSEventPhaseNone && momentumPhase == NSEventPhaseNone) {
          dx = [event deltaX];
          dy = [event deltaY];
        }
        window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);

        NSPoint delta = [[cocoawindow contentView] convertPointToBacking:NSMakePoint(dx, dy)];
        pushEvent(new GHOST_EventTrackpad([event timestamp] * 1000,
                                          window,
                                          GHOST_kTrackpadEventScroll,
                                          x,
                                          y,
                                          delta.x,
                                          delta.y,
                                          [event isDirectionInvertedFromDevice]));
      }
    } break;

    case NSEventTypeMagnify: {
      NSPoint mousePos = [event locationInWindow];
      GHOST_TInt32 x, y;
      window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
      pushEvent(new GHOST_EventTrackpad([event timestamp] * 1000,
                                        window,
                                        GHOST_kTrackpadEventMagnify,
                                        x,
                                        y,
                                        [event magnification] * 125.0 + 0.1,
                                        0,
                                        false));
    } break;

    case NSEventTypeSmartMagnify: {
      NSPoint mousePos = [event locationInWindow];
      GHOST_TInt32 x, y;
      window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
      pushEvent(new GHOST_EventTrackpad(
          [event timestamp] * 1000, window, GHOST_kTrackpadEventSmartMagnify, x, y, 0, 0, false));
    } break;

    case NSEventTypeRotate: {
      NSPoint mousePos = [event locationInWindow];
      GHOST_TInt32 x, y;
      window->clientToScreenIntern(mousePos.x, mousePos.y, x, y);
      pushEvent(new GHOST_EventTrackpad([event timestamp] * 1000,
                                        window,
                                        GHOST_kTrackpadEventRotate,
                                        x,
                                        y,
                                        [event rotation] * -5.0,
                                        0,
                                        false));
    }
    default:
      return GHOST_kFailure;
      break;
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemCocoa::handleKeyEvent(void *eventPtr)
{
  NSEvent *event = (NSEvent *)eventPtr;
  GHOST_IWindow *window;
  unsigned int modifiers;
  NSString *characters;
  NSData *convertedCharacters;
  GHOST_TKey keyCode;
  unsigned char ascii;
  NSString *charsIgnoringModifiers;

  window = m_windowManager->getWindowAssociatedWithOSWindow((void *)[event window]);
  if (!window) {
    // printf("\nW failure for event 0x%x",[event type]);
    return GHOST_kFailure;
  }

  char utf8_buf[6] = {'\0'};
  ascii = 0;

  switch ([event type]) {

    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
      charsIgnoringModifiers = [event charactersIgnoringModifiers];
      if ([charsIgnoringModifiers length] > 0) {
        keyCode = convertKey([event keyCode],
                             [charsIgnoringModifiers characterAtIndex:0],
                             [event type] == NSEventTypeKeyDown ? kUCKeyActionDown :
                                                                  kUCKeyActionUp);
      }
      else {
        keyCode = convertKey([event keyCode],
                             0,
                             [event type] == NSEventTypeKeyDown ? kUCKeyActionDown :
                                                                  kUCKeyActionUp);
      }

      /* handling both unicode or ascii */
      characters = [event characters];
      if ([characters length] > 0) {
        convertedCharacters = [characters dataUsingEncoding:NSUTF8StringEncoding];

        for (int x = 0; x < [convertedCharacters length]; x++) {
          utf8_buf[x] = ((char *)[convertedCharacters bytes])[x];
        }
      }

      /* arrow keys should not have utf8 */
      if ((keyCode >= GHOST_kKeyLeftArrow) && (keyCode <= GHOST_kKeyDownArrow)) {
        utf8_buf[0] = '\0';
      }

      /* F keys should not have utf8 */
      if ((keyCode >= GHOST_kKeyF1) && (keyCode <= GHOST_kKeyF20))
        utf8_buf[0] = '\0';

      /* no text with command key pressed */
      if (m_modifierMask & NSEventModifierFlagCommand)
        utf8_buf[0] = '\0';

      if ((keyCode == GHOST_kKeyQ) && (m_modifierMask & NSEventModifierFlagCommand))
        break;  // Cmd-Q is directly handled by Cocoa

      /* ascii is a subset of unicode */
      if (utf8_buf[0] && !utf8_buf[1]) {
        ascii = utf8_buf[0];
      }

      if ([event type] == NSEventTypeKeyDown) {
        pushEvent(new GHOST_EventKey([event timestamp] * 1000,
                                     GHOST_kEventKeyDown,
                                     window,
                                     keyCode,
                                     ascii,
                                     utf8_buf,
                                     [event isARepeat]));
#if 0
        printf("Key down rawCode=0x%x charsIgnoringModifiers=%c keyCode=%u ascii=%i %c utf8=%s\n",
               [event keyCode],
               [charsIgnoringModifiers length] > 0 ? [charsIgnoringModifiers characterAtIndex:0] :
                                                     ' ',
               keyCode,
               ascii,
               ascii,
               utf8_buf);
#endif
      }
      else {
        pushEvent(new GHOST_EventKey(
            [event timestamp] * 1000, GHOST_kEventKeyUp, window, keyCode, 0, NULL, false));
#if 0
        printf("Key up rawCode=0x%x charsIgnoringModifiers=%c keyCode=%u ascii=%i %c utf8=%s\n",
               [event keyCode],
               [charsIgnoringModifiers length] > 0 ? [charsIgnoringModifiers characterAtIndex:0] :
                                                     ' ',
               keyCode,
               ascii,
               ascii,
               utf8_buf);
#endif
      }
      m_ignoreMomentumScroll = true;
      break;

    case NSEventTypeFlagsChanged:
      modifiers = [event modifierFlags];

      if ((modifiers & NSEventModifierFlagShift) != (m_modifierMask & NSEventModifierFlagShift)) {
        pushEvent(new GHOST_EventKey([event timestamp] * 1000,
                                     (modifiers & NSEventModifierFlagShift) ? GHOST_kEventKeyDown :
                                                                              GHOST_kEventKeyUp,
                                     window,
                                     GHOST_kKeyLeftShift,
                                     false));
      }
      if ((modifiers & NSEventModifierFlagControl) !=
          (m_modifierMask & NSEventModifierFlagControl)) {
        pushEvent(new GHOST_EventKey(
            [event timestamp] * 1000,
            (modifiers & NSEventModifierFlagControl) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
            window,
            GHOST_kKeyLeftControl,
            false));
      }
      if ((modifiers & NSEventModifierFlagOption) !=
          (m_modifierMask & NSEventModifierFlagOption)) {
        pushEvent(new GHOST_EventKey(
            [event timestamp] * 1000,
            (modifiers & NSEventModifierFlagOption) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
            window,
            GHOST_kKeyLeftAlt,
            false));
      }
      if ((modifiers & NSEventModifierFlagCommand) !=
          (m_modifierMask & NSEventModifierFlagCommand)) {
        pushEvent(new GHOST_EventKey(
            [event timestamp] * 1000,
            (modifiers & NSEventModifierFlagCommand) ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
            window,
            GHOST_kKeyOS,
            false));
      }

      m_modifierMask = modifiers;
      m_ignoreMomentumScroll = true;
      break;

    default:
      return GHOST_kFailure;
      break;
  }

  return GHOST_kSuccess;
}

#pragma mark Clipboard get/set

GHOST_TUns8 *GHOST_SystemCocoa::getClipboard(bool selection) const
{
  GHOST_TUns8 *temp_buff;
  size_t pastedTextSize;

  @autoreleasepool {

    NSPasteboard *pasteBoard = [NSPasteboard generalPasteboard];

    NSString *textPasted = [pasteBoard stringForType:NSStringPboardType];

    if (textPasted == nil) {
      return NULL;
    }

    pastedTextSize = [textPasted lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

    temp_buff = (GHOST_TUns8 *)malloc(pastedTextSize + 1);

    if (temp_buff == NULL) {
      return NULL;
    }

    strncpy(
        (char *)temp_buff, [textPasted cStringUsingEncoding:NSUTF8StringEncoding], pastedTextSize);

    temp_buff[pastedTextSize] = '\0';

    if (temp_buff) {
      return temp_buff;
    }
    else {
      return NULL;
    }
  }
}

void GHOST_SystemCocoa::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{
  if (selection)
    return;  // for copying the selection, used on X11

  @autoreleasepool {

    NSPasteboard *pasteBoard = NSPasteboard.generalPasteboard;
    [pasteBoard declareTypes:@[ NSStringPboardType ] owner:nil];
    NSString *textToCopy = [NSString stringWithCString:buffer encoding:NSUTF8StringEncoding];
    [pasteBoard setString:textToCopy forType:NSStringPboardType];
  }
}
