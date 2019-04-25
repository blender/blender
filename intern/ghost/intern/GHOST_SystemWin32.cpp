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

/** \file
 * \ingroup GHOST
 */

#include "GHOST_SystemWin32.h"
#include "GHOST_EventDragnDrop.h"

#ifndef _WIN32_IE
#  define _WIN32_IE 0x0501 /* shipped before XP, so doesn't impose additional requirements */
#endif

#include <shlobj.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <windowsx.h>

#include "utfconv.h"

#include "GHOST_DisplayManagerWin32.h"
#include "GHOST_EventButton.h"
#include "GHOST_EventCursor.h"
#include "GHOST_EventKey.h"
#include "GHOST_EventWheel.h"
#include "GHOST_TimerTask.h"
#include "GHOST_TimerManager.h"
#include "GHOST_WindowManager.h"
#include "GHOST_WindowWin32.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextWGL.h"
#endif

#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerWin32.h"
#endif

// Key code values not found in winuser.h
#ifndef VK_MINUS
#  define VK_MINUS 0xBD
#endif  // VK_MINUS
#ifndef VK_SEMICOLON
#  define VK_SEMICOLON 0xBA
#endif  // VK_SEMICOLON
#ifndef VK_PERIOD
#  define VK_PERIOD 0xBE
#endif  // VK_PERIOD
#ifndef VK_COMMA
#  define VK_COMMA 0xBC
#endif  // VK_COMMA
#ifndef VK_QUOTE
#  define VK_QUOTE 0xDE
#endif  // VK_QUOTE
#ifndef VK_BACK_QUOTE
#  define VK_BACK_QUOTE 0xC0
#endif  // VK_BACK_QUOTE
#ifndef VK_SLASH
#  define VK_SLASH 0xBF
#endif  // VK_SLASH
#ifndef VK_BACK_SLASH
#  define VK_BACK_SLASH 0xDC
#endif  // VK_BACK_SLASH
#ifndef VK_EQUALS
#  define VK_EQUALS 0xBB
#endif  // VK_EQUALS
#ifndef VK_OPEN_BRACKET
#  define VK_OPEN_BRACKET 0xDB
#endif  // VK_OPEN_BRACKET
#ifndef VK_CLOSE_BRACKET
#  define VK_CLOSE_BRACKET 0xDD
#endif  // VK_CLOSE_BRACKET
#ifndef VK_GR_LESS
#  define VK_GR_LESS 0xE2
#endif  // VK_GR_LESS

#ifndef VK_MEDIA_NEXT_TRACK
#  define VK_MEDIA_NEXT_TRACK 0xB0
#endif  // VK_MEDIA_NEXT_TRACK
#ifndef VK_MEDIA_PREV_TRACK
#  define VK_MEDIA_PREV_TRACK 0xB1
#endif  // VK_MEDIA_PREV_TRACK
#ifndef VK_MEDIA_STOP
#  define VK_MEDIA_STOP 0xB2
#endif  // VK_MEDIA_STOP
#ifndef VK_MEDIA_PLAY_PAUSE
#  define VK_MEDIA_PLAY_PAUSE 0xB3
#endif  // VK_MEDIA_PLAY_PAUSE

// Window message newer than Windows 7
#ifndef WM_DPICHANGED
#  define WM_DPICHANGED 0x02E0
#endif  // WM_DPICHANGED

#ifndef WM_POINTERUPDATE
#  define WM_POINTERUPDATE 0x0245
#endif  // WM_POINTERUPDATE

#define WM_POINTERDOWN 0x0246
#define WM_POINTERUP 0x0247

/* Workaround for some laptop touchpads, some of which seems to
 * have driver issues which makes it so window function receives
 * the message, but PeekMessage doesn't pick those messages for
 * some reason.
 *
 * We send a dummy WM_USER message to force PeekMessage to receive
 * something, making it so blender's window manager sees the new
 * messages coming in.
 */
#define BROKEN_PEEK_TOUCHPAD

static void initRawInput()
{
#ifdef WITH_INPUT_NDOF
#  define DEVICE_COUNT 2
#else
#  define DEVICE_COUNT 1
#endif

  RAWINPUTDEVICE devices[DEVICE_COUNT];
  memset(devices, 0, DEVICE_COUNT * sizeof(RAWINPUTDEVICE));

  // Initiates WM_INPUT messages from keyboard
  // That way GHOST can retrieve true keys
  devices[0].usUsagePage = 0x01;
  devices[0].usUsage = 0x06; /* http://msdn.microsoft.com/en-us/windows/hardware/gg487473.aspx */

#ifdef WITH_INPUT_NDOF
  // multi-axis mouse (SpaceNavigator, etc.)
  devices[1].usUsagePage = 0x01;
  devices[1].usUsage = 0x08;
#endif

  if (RegisterRawInputDevices(devices, DEVICE_COUNT, sizeof(RAWINPUTDEVICE)))
    ;  // yay!
  else
    GHOST_PRINTF("could not register for RawInput: %d\n", (int)GetLastError());

#undef DEVICE_COUNT
}

#ifndef DPI_ENUMS_DECLARED
typedef enum PROCESS_DPI_AWARENESS {
  PROCESS_DPI_UNAWARE = 0,
  PROCESS_SYSTEM_DPI_AWARE = 1,
  PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;

typedef enum MONITOR_DPI_TYPE {
  MDT_EFFECTIVE_DPI = 0,
  MDT_ANGULAR_DPI = 1,
  MDT_RAW_DPI = 2,
  MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

#  define USER_DEFAULT_SCREEN_DPI 96

#  define DPI_ENUMS_DECLARED
#endif
typedef HRESULT(API *GHOST_WIN32_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
typedef BOOL(API *GHOST_WIN32_EnableNonClientDpiScaling)(HWND);

GHOST_SystemWin32::GHOST_SystemWin32() : m_hasPerformanceCounter(false), m_freq(0), m_start(0)
{
  m_displayManager = new GHOST_DisplayManagerWin32();
  GHOST_ASSERT(m_displayManager, "GHOST_SystemWin32::GHOST_SystemWin32(): m_displayManager==0\n");
  m_displayManager->initialize();

  m_consoleStatus = 1;

  // Tell Windows we are per monitor DPI aware. This disables the default
  // blurry scaling and enables WM_DPICHANGED to allow us to draw at proper DPI.
  HMODULE m_shcore = ::LoadLibrary("Shcore.dll");
  if (m_shcore) {
    GHOST_WIN32_SetProcessDpiAwareness fpSetProcessDpiAwareness =
        (GHOST_WIN32_SetProcessDpiAwareness)::GetProcAddress(m_shcore, "SetProcessDpiAwareness");

    if (fpSetProcessDpiAwareness) {
      fpSetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    }
  }

  // Check if current keyboard layout uses AltGr and save keylayout ID for
  // specialized handling if keys like VK_OEM_*. I.e. french keylayout
  // generates VK_OEM_8 for their exclamation key (key left of right shift)
  this->handleKeyboardChange();
  // Require COM for GHOST_DropTargetWin32 created in GHOST_WindowWin32.
  OleInitialize(0);

#ifdef WITH_INPUT_NDOF
  m_ndofManager = new GHOST_NDOFManagerWin32(*this);
#endif
}

GHOST_SystemWin32::~GHOST_SystemWin32()
{
  // Shutdown COM
  OleUninitialize();
  toggleConsole(1);
}

GHOST_TUns64 GHOST_SystemWin32::getMilliSeconds() const
{
  // Hardware does not support high resolution timers. We will use GetTickCount instead then.
  if (!m_hasPerformanceCounter) {
    return ::GetTickCount();
  }

  // Retrieve current count
  __int64 count = 0;
  ::QueryPerformanceCounter((LARGE_INTEGER *)&count);

  // Calculate the time passed since system initialization.
  __int64 delta = 1000 * (count - m_start);

  GHOST_TUns64 t = (GHOST_TUns64)(delta / m_freq);
  return t;
}

GHOST_TUns8 GHOST_SystemWin32::getNumDisplays() const
{
  GHOST_ASSERT(m_displayManager, "GHOST_SystemWin32::getNumDisplays(): m_displayManager==0\n");
  GHOST_TUns8 numDisplays;
  m_displayManager->getNumDisplays(numDisplays);
  return numDisplays;
}

void GHOST_SystemWin32::getMainDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const
{
  width = ::GetSystemMetrics(SM_CXSCREEN);
  height = ::GetSystemMetrics(SM_CYSCREEN);
}

void GHOST_SystemWin32::getAllDisplayDimensions(GHOST_TUns32 &width, GHOST_TUns32 &height) const
{
  width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
  height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

GHOST_IWindow *GHOST_SystemWin32::createWindow(const STR_String &title,
                                               GHOST_TInt32 left,
                                               GHOST_TInt32 top,
                                               GHOST_TUns32 width,
                                               GHOST_TUns32 height,
                                               GHOST_TWindowState state,
                                               GHOST_TDrawingContextType type,
                                               GHOST_GLSettings glSettings,
                                               const bool exclusive,
                                               const GHOST_TEmbedderWindowID parentWindow)
{
  GHOST_WindowWin32 *window = new GHOST_WindowWin32(
      this,
      title,
      left,
      top,
      width,
      height,
      state,
      type,
      ((glSettings.flags & GHOST_glStereoVisual) != 0),
      ((glSettings.flags & GHOST_glAlphaBackground) != 0),
      parentWindow,
      ((glSettings.flags & GHOST_glDebugContext) != 0));

  if (window->getValid()) {
    // Store the pointer to the window
    m_windowManager->addWindow(window);
    m_windowManager->setActiveWindow(window);
  }
  else {
    GHOST_PRINT("GHOST_SystemWin32::createWindow(): window invalid\n");
    delete window;
    window = NULL;
  }

  return window;
}

/**
 * Create a new offscreen context.
 * Never explicitly delete the window, use #disposeContext() instead.
 * \return The new context (or 0 if creation failed).
 */
GHOST_IContext *GHOST_SystemWin32::createOffscreenContext()
{
  bool debug_context = false; /* TODO: inform as a parameter */

  GHOST_Context *context;

  HWND wnd = CreateWindowA("STATIC",
                           "BlenderGLEW",
                           WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                           0,
                           0,
                           64,
                           64,
                           NULL,
                           NULL,
                           GetModuleHandle(NULL),
                           NULL);

  HDC mHDC = GetDC(wnd);
  HDC prev_hdc = wglGetCurrentDC();
  HGLRC prev_context = wglGetCurrentContext();
#if defined(WITH_GL_PROFILE_CORE)
  for (int minor = 5; minor >= 0; --minor) {
    context = new GHOST_ContextWGL(false,
                                   true,
                                   wnd,
                                   mHDC,
                                   WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                   4,
                                   minor,
                                   (debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
                                   GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);

    if (context->initializeDrawingContext()) {
      goto finished;
    }
    else {
      delete context;
    }
  }

  context = new GHOST_ContextWGL(false,
                                 true,
                                 wnd,
                                 mHDC,
                                 WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                 3,
                                 3,
                                 (debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
                                 GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);

  if (context->initializeDrawingContext()) {
    goto finished;
  }
  else {
    MessageBox(NULL,
               "A graphics card and driver with support for OpenGL 3.3 or higher is required.\n"
               "Installing the latest driver for your graphics card may resolve the issue.\n\n"
               "The program will now close.",
               "Blender - Unsupported Graphics Card or Driver",
               MB_OK | MB_ICONERROR);
    delete context;
    exit();
  }

#elif defined(WITH_GL_PROFILE_COMPAT)
  // ask for 2.1 context, driver gives any GL version >= 2.1 (hopefully the latest compatibility profile)
  // 2.1 ignores the profile bit & is incompatible with core profile
  context = new GHOST_ContextWGL(false,
                                 true,
                                 NULL,
                                 NULL,
                                 0,  // no profile bit
                                 2,
                                 1,
                                 (debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
                                 GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);

  if (context->initializeDrawingContext()) {
    return context;
  }
  else {
    delete context;
  }
#else
#  error  // must specify either core or compat at build time
#endif
finished:
  wglMakeCurrent(prev_hdc, prev_context);
  return context;
}

/**
 * Dispose of a context.
 * \param   context Pointer to the context to be disposed.
 * \return  Indication of success.
 */
GHOST_TSuccess GHOST_SystemWin32::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

bool GHOST_SystemWin32::processEvents(bool waitForEvent)
{
  MSG msg;
  bool hasEventHandled = false;

  do {
    GHOST_TimerManager *timerMgr = getTimerManager();

    if (waitForEvent && !::PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
#if 1
      ::Sleep(1);
#else
      GHOST_TUns64 next = timerMgr->nextFireTime();
      GHOST_TInt64 maxSleep = next - getMilliSeconds();

      if (next == GHOST_kFireTimeNever) {
        ::WaitMessage();
      }
      else if (maxSleep >= 0.0) {
        ::SetTimer(NULL, 0, maxSleep, NULL);
        ::WaitMessage();
        ::KillTimer(NULL, 0);
      }
#endif
    }

    if (timerMgr->fireTimers(getMilliSeconds())) {
      hasEventHandled = true;
    }

    // Process all the events waiting for us
    while (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
      // TranslateMessage doesn't alter the message, and doesn't change our raw keyboard data.
      // Needed for MapVirtualKey or if we ever need to get chars from wm_ime_char or similar.
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
      hasEventHandled = true;
    }
  } while (waitForEvent && !hasEventHandled);

  return hasEventHandled;
}

GHOST_TSuccess GHOST_SystemWin32::getCursorPosition(GHOST_TInt32 &x, GHOST_TInt32 &y) const
{
  POINT point;
  if (::GetCursorPos(&point)) {
    x = point.x;
    y = point.y;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemWin32::setCursorPosition(GHOST_TInt32 x, GHOST_TInt32 y)
{
  if (!::GetActiveWindow())
    return GHOST_kFailure;
  return ::SetCursorPos(x, y) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemWin32::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  bool down = HIBYTE(::GetKeyState(VK_LSHIFT)) != 0;
  keys.set(GHOST_kModifierKeyLeftShift, down);
  down = HIBYTE(::GetKeyState(VK_RSHIFT)) != 0;
  keys.set(GHOST_kModifierKeyRightShift, down);

  down = HIBYTE(::GetKeyState(VK_LMENU)) != 0;
  keys.set(GHOST_kModifierKeyLeftAlt, down);
  down = HIBYTE(::GetKeyState(VK_RMENU)) != 0;
  keys.set(GHOST_kModifierKeyRightAlt, down);

  down = HIBYTE(::GetKeyState(VK_LCONTROL)) != 0;
  keys.set(GHOST_kModifierKeyLeftControl, down);
  down = HIBYTE(::GetKeyState(VK_RCONTROL)) != 0;
  keys.set(GHOST_kModifierKeyRightControl, down);

  bool lwindown = HIBYTE(::GetKeyState(VK_LWIN)) != 0;
  bool rwindown = HIBYTE(::GetKeyState(VK_RWIN)) != 0;
  if (lwindown || rwindown)
    keys.set(GHOST_kModifierKeyOS, true);
  else
    keys.set(GHOST_kModifierKeyOS, false);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWin32::getButtons(GHOST_Buttons &buttons) const
{
  /* Check for swapped buttons (left-handed mouse buttons)
   * GetAsyncKeyState() will give back the state of the physical mouse buttons.
   */
  bool swapped = ::GetSystemMetrics(SM_SWAPBUTTON) == TRUE;

  bool down = HIBYTE(::GetKeyState(VK_LBUTTON)) != 0;
  buttons.set(swapped ? GHOST_kButtonMaskRight : GHOST_kButtonMaskLeft, down);

  down = HIBYTE(::GetKeyState(VK_MBUTTON)) != 0;
  buttons.set(GHOST_kButtonMaskMiddle, down);

  down = HIBYTE(::GetKeyState(VK_RBUTTON)) != 0;
  buttons.set(swapped ? GHOST_kButtonMaskLeft : GHOST_kButtonMaskRight, down);
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWin32::init()
{
  GHOST_TSuccess success = GHOST_System::init();

  /* Disable scaling on high DPI displays on Vista */
  HMODULE
  user32 = ::LoadLibraryA("user32.dll");
  typedef BOOL(WINAPI * LPFNSETPROCESSDPIAWARE)();
  LPFNSETPROCESSDPIAWARE SetProcessDPIAware = (LPFNSETPROCESSDPIAWARE)GetProcAddress(
      user32, "SetProcessDPIAware");
  if (SetProcessDPIAware)
    SetProcessDPIAware();
  FreeLibrary(user32);
  initRawInput();

  // Determine whether this system has a high frequency performance counter. */
  m_hasPerformanceCounter = ::QueryPerformanceFrequency((LARGE_INTEGER *)&m_freq) == TRUE;
  if (m_hasPerformanceCounter) {
    GHOST_PRINT("GHOST_SystemWin32::init: High Frequency Performance Timer available\n");
    ::QueryPerformanceCounter((LARGE_INTEGER *)&m_start);
  }
  else {
    GHOST_PRINT("GHOST_SystemWin32::init: High Frequency Performance Timer not available\n");
  }

  if (success) {
    WNDCLASSW wc = {0};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = s_wndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = ::GetModuleHandle(0);
    wc.hIcon = ::LoadIcon(wc.hInstance, "APPICON");

    if (!wc.hIcon) {
      ::LoadIcon(NULL, IDI_APPLICATION);
    }
    wc.hCursor = ::LoadCursor(0, IDC_ARROW);
    wc.hbrBackground =
#ifdef INW32_COMPISITING
        (HBRUSH)CreateSolidBrush
#endif
        (0x00000000);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"GHOST_WindowClass";

    // Use RegisterClassEx for setting small icon
    if (::RegisterClassW(&wc) == 0) {
      success = GHOST_kFailure;
    }
  }

  return success;
}

GHOST_TSuccess GHOST_SystemWin32::exit()
{
  return GHOST_System::exit();
}

GHOST_TKey GHOST_SystemWin32::hardKey(RAWINPUT const &raw, int *keyDown, char *vk)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  GHOST_TKey key = GHOST_kKeyUnknown;
  GHOST_ModifierKeys modifiers;
  system->retrieveModifierKeys(modifiers);

  // RI_KEY_BREAK doesn't work for sticky keys release, so we also
  // check for the up message
  unsigned int msg = raw.data.keyboard.Message;
  *keyDown = !(raw.data.keyboard.Flags & RI_KEY_BREAK) && msg != WM_KEYUP && msg != WM_SYSKEYUP;

  key = this->convertKey(raw.data.keyboard.VKey,
                         raw.data.keyboard.MakeCode,
                         (raw.data.keyboard.Flags & (RI_KEY_E1 | RI_KEY_E0)));

  // extra handling of modifier keys: don't send repeats out from GHOST
  if (key >= GHOST_kKeyLeftShift && key <= GHOST_kKeyRightAlt) {
    bool changed = false;
    GHOST_TModifierKeyMask modifier;
    switch (key) {
      case GHOST_kKeyLeftShift: {
        changed = (modifiers.get(GHOST_kModifierKeyLeftShift) != (bool)*keyDown);
        modifier = GHOST_kModifierKeyLeftShift;
        break;
      }
      case GHOST_kKeyRightShift: {
        changed = (modifiers.get(GHOST_kModifierKeyRightShift) != (bool)*keyDown);
        modifier = GHOST_kModifierKeyRightShift;
        break;
      }
      case GHOST_kKeyLeftControl: {
        changed = (modifiers.get(GHOST_kModifierKeyLeftControl) != (bool)*keyDown);
        modifier = GHOST_kModifierKeyLeftControl;
        break;
      }
      case GHOST_kKeyRightControl: {
        changed = (modifiers.get(GHOST_kModifierKeyRightControl) != (bool)*keyDown);
        modifier = GHOST_kModifierKeyRightControl;
        break;
      }
      case GHOST_kKeyLeftAlt: {
        changed = (modifiers.get(GHOST_kModifierKeyLeftAlt) != (bool)*keyDown);
        modifier = GHOST_kModifierKeyLeftAlt;
        break;
      }
      case GHOST_kKeyRightAlt: {
        changed = (modifiers.get(GHOST_kModifierKeyRightAlt) != (bool)*keyDown);
        modifier = GHOST_kModifierKeyRightAlt;
        break;
      }
      default:
        break;
    }

    if (changed) {
      modifiers.set(modifier, (bool)*keyDown);
      system->storeModifierKeys(modifiers);
    }
    else {
      key = GHOST_kKeyUnknown;
    }
  }

  if (vk)
    *vk = raw.data.keyboard.VKey;

  return key;
}

//! note: this function can be extended to include other exotic cases as they arise.
// This function was added in response to bug [#25715]
// This is going to be a long list [T42426]
GHOST_TKey GHOST_SystemWin32::processSpecialKey(short vKey, short scanCode) const
{
  GHOST_TKey key = GHOST_kKeyUnknown;
  switch (PRIMARYLANGID(m_langId)) {
    case LANG_FRENCH:
      if (vKey == VK_OEM_8)
        key = GHOST_kKeyF13;  // oem key; used purely for shortcuts .
      break;
    case LANG_ENGLISH:
      if (SUBLANGID(m_langId) == SUBLANG_ENGLISH_UK && vKey == VK_OEM_8)  // "`¬"
        key = GHOST_kKeyAccentGrave;
      break;
  }

  return key;
}

GHOST_TKey GHOST_SystemWin32::convertKey(short vKey, short scanCode, short extend) const
{
  GHOST_TKey key;

  if ((vKey >= '0') && (vKey <= '9')) {
    // VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39)
    key = (GHOST_TKey)(vKey - '0' + GHOST_kKey0);
  }
  else if ((vKey >= 'A') && (vKey <= 'Z')) {
    // VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A)
    key = (GHOST_TKey)(vKey - 'A' + GHOST_kKeyA);
  }
  else if ((vKey >= VK_F1) && (vKey <= VK_F24)) {
    key = (GHOST_TKey)(vKey - VK_F1 + GHOST_kKeyF1);
  }
  else {
    switch (vKey) {
      case VK_RETURN:
        key = (extend) ? GHOST_kKeyNumpadEnter : GHOST_kKeyEnter;
        break;

      case VK_BACK:
        key = GHOST_kKeyBackSpace;
        break;
      case VK_TAB:
        key = GHOST_kKeyTab;
        break;
      case VK_ESCAPE:
        key = GHOST_kKeyEsc;
        break;
      case VK_SPACE:
        key = GHOST_kKeySpace;
        break;

      case VK_INSERT:
      case VK_NUMPAD0:
        key = (extend) ? GHOST_kKeyInsert : GHOST_kKeyNumpad0;
        break;
      case VK_END:
      case VK_NUMPAD1:
        key = (extend) ? GHOST_kKeyEnd : GHOST_kKeyNumpad1;
        break;
      case VK_DOWN:
      case VK_NUMPAD2:
        key = (extend) ? GHOST_kKeyDownArrow : GHOST_kKeyNumpad2;
        break;
      case VK_NEXT:
      case VK_NUMPAD3:
        key = (extend) ? GHOST_kKeyDownPage : GHOST_kKeyNumpad3;
        break;
      case VK_LEFT:
      case VK_NUMPAD4:
        key = (extend) ? GHOST_kKeyLeftArrow : GHOST_kKeyNumpad4;
        break;
      case VK_CLEAR:
      case VK_NUMPAD5:
        key = (extend) ? GHOST_kKeyUnknown : GHOST_kKeyNumpad5;
        break;
      case VK_RIGHT:
      case VK_NUMPAD6:
        key = (extend) ? GHOST_kKeyRightArrow : GHOST_kKeyNumpad6;
        break;
      case VK_HOME:
      case VK_NUMPAD7:
        key = (extend) ? GHOST_kKeyHome : GHOST_kKeyNumpad7;
        break;
      case VK_UP:
      case VK_NUMPAD8:
        key = (extend) ? GHOST_kKeyUpArrow : GHOST_kKeyNumpad8;
        break;
      case VK_PRIOR:
      case VK_NUMPAD9:
        key = (extend) ? GHOST_kKeyUpPage : GHOST_kKeyNumpad9;
        break;
      case VK_DECIMAL:
      case VK_DELETE:
        key = (extend) ? GHOST_kKeyDelete : GHOST_kKeyNumpadPeriod;
        break;

      case VK_SNAPSHOT:
        key = GHOST_kKeyPrintScreen;
        break;
      case VK_PAUSE:
        key = GHOST_kKeyPause;
        break;
      case VK_MULTIPLY:
        key = GHOST_kKeyNumpadAsterisk;
        break;
      case VK_SUBTRACT:
        key = GHOST_kKeyNumpadMinus;
        break;
      case VK_DIVIDE:
        key = GHOST_kKeyNumpadSlash;
        break;
      case VK_ADD:
        key = GHOST_kKeyNumpadPlus;
        break;

      case VK_SEMICOLON:
        key = GHOST_kKeySemicolon;
        break;
      case VK_EQUALS:
        key = GHOST_kKeyEqual;
        break;
      case VK_COMMA:
        key = GHOST_kKeyComma;
        break;
      case VK_MINUS:
        key = GHOST_kKeyMinus;
        break;
      case VK_PERIOD:
        key = GHOST_kKeyPeriod;
        break;
      case VK_SLASH:
        key = GHOST_kKeySlash;
        break;
      case VK_BACK_QUOTE:
        key = GHOST_kKeyAccentGrave;
        break;
      case VK_OPEN_BRACKET:
        key = GHOST_kKeyLeftBracket;
        break;
      case VK_BACK_SLASH:
        key = GHOST_kKeyBackslash;
        break;
      case VK_CLOSE_BRACKET:
        key = GHOST_kKeyRightBracket;
        break;
      case VK_QUOTE:
        key = GHOST_kKeyQuote;
        break;
      case VK_GR_LESS:
        key = GHOST_kKeyGrLess;
        break;

      case VK_SHIFT:
        /* Check single shift presses */
        if (scanCode == 0x36) {
          key = GHOST_kKeyRightShift;
        }
        else if (scanCode == 0x2a) {
          key = GHOST_kKeyLeftShift;
        }
        else {
          /* Must be a combination SHIFT (Left or Right) + a Key
           * Ignore this as the next message will contain
           * the desired "Key" */
          key = GHOST_kKeyUnknown;
        }
        break;
      case VK_CONTROL:
        key = (extend) ? GHOST_kKeyRightControl : GHOST_kKeyLeftControl;
        break;
      case VK_MENU:
        key = (extend) ? GHOST_kKeyRightAlt : GHOST_kKeyLeftAlt;
        break;
      case VK_LWIN:
      case VK_RWIN:
        key = GHOST_kKeyOS;
        break;
      case VK_NUMLOCK:
        key = GHOST_kKeyNumLock;
        break;
      case VK_SCROLL:
        key = GHOST_kKeyScrollLock;
        break;
      case VK_CAPITAL:
        key = GHOST_kKeyCapsLock;
        break;
      case VK_OEM_8:
        key = ((GHOST_SystemWin32 *)getSystem())->processSpecialKey(vKey, scanCode);
        break;
      case VK_MEDIA_PLAY_PAUSE:
        key = GHOST_kKeyMediaPlay;
        break;
      case VK_MEDIA_STOP:
        key = GHOST_kKeyMediaStop;
        break;
      case VK_MEDIA_PREV_TRACK:
        key = GHOST_kKeyMediaFirst;
        break;
      case VK_MEDIA_NEXT_TRACK:
        key = GHOST_kKeyMediaLast;
        break;
      default:
        key = GHOST_kKeyUnknown;
        break;
    }
  }

  return key;
}

GHOST_EventButton *GHOST_SystemWin32::processButtonEvent(GHOST_TEventType type,
                                                         GHOST_WindowWin32 *window,
                                                         GHOST_TButtonMask mask)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  if (window->useTabletAPI(GHOST_kTabletNative)) {
    window->setTabletData(NULL);
  }
  return new GHOST_EventButton(system->getMilliSeconds(), type, window, mask);
}

GHOST_Event *GHOST_SystemWin32::processPointerEvent(GHOST_TEventType type,
                                                    GHOST_WindowWin32 *window,
                                                    WPARAM wParam,
                                                    LPARAM lParam,
                                                    bool &eventHandled)
{
  GHOST_PointerInfoWin32 pointerInfo;
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  if (!window->useTabletAPI(GHOST_kTabletNative)) {
    return NULL;
  }

  if (window->getPointerInfo(&pointerInfo, wParam, lParam) != GHOST_kSuccess) {
    return NULL;
  }

  if (!pointerInfo.isPrimary) {
    eventHandled = true;
    return NULL;  // For multi-touch displays we ignore these events
  }

  system->setCursorPosition(pointerInfo.pixelLocation.x, pointerInfo.pixelLocation.y);

  switch (type) {
    case GHOST_kEventButtonDown:
      window->setTabletData(&pointerInfo.tabletData);
      eventHandled = true;
      return new GHOST_EventButton(
          system->getMilliSeconds(), GHOST_kEventButtonDown, window, pointerInfo.buttonMask);
    case GHOST_kEventButtonUp:
      eventHandled = true;
      return new GHOST_EventButton(
          system->getMilliSeconds(), GHOST_kEventButtonUp, window, pointerInfo.buttonMask);
    case GHOST_kEventCursorMove:
      window->setTabletData(&pointerInfo.tabletData);
      eventHandled = true;
      return new GHOST_EventCursor(system->getMilliSeconds(),
                                   GHOST_kEventCursorMove,
                                   window,
                                   pointerInfo.pixelLocation.x,
                                   pointerInfo.pixelLocation.y);
    default:
      return NULL;
  }
}

GHOST_EventCursor *GHOST_SystemWin32::processCursorEvent(GHOST_TEventType type,
                                                         GHOST_WindowWin32 *window)
{
  GHOST_TInt32 x_screen, y_screen;
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  system->getCursorPosition(x_screen, y_screen);

  /* TODO: CHECK IF THIS IS A TABLET EVENT */
  bool is_tablet = false;

  if (is_tablet == false && window->getCursorGrabModeIsWarp()) {
    GHOST_TInt32 x_new = x_screen;
    GHOST_TInt32 y_new = y_screen;
    GHOST_TInt32 x_accum, y_accum;
    GHOST_Rect bounds;

    /* fallback to window bounds */
    if (window->getCursorGrabBounds(bounds) == GHOST_kFailure) {
      window->getClientBounds(bounds);
    }

    /* could also clamp to screen bounds
     * wrap with a window outside the view will fail atm  */

    bounds.wrapPoint(x_new, y_new, 2); /* offset of one incase blender is at screen bounds */

    window->getCursorGrabAccum(x_accum, y_accum);
    if (x_new != x_screen || y_new != y_screen) {
      /* when wrapping we don't need to add an event because the
       * setCursorPosition call will cause a new event after */
      system->setCursorPosition(x_new, y_new); /* wrap */
      window->setCursorGrabAccum(x_accum + (x_screen - x_new), y_accum + (y_screen - y_new));
    }
    else {
      return new GHOST_EventCursor(system->getMilliSeconds(),
                                   GHOST_kEventCursorMove,
                                   window,
                                   x_screen + x_accum,
                                   y_screen + y_accum);
    }
  }
  else {
    return new GHOST_EventCursor(
        system->getMilliSeconds(), GHOST_kEventCursorMove, window, x_screen, y_screen);
  }
  return NULL;
}

void GHOST_SystemWin32::processWheelEvent(GHOST_WindowWin32 *window, WPARAM wParam, LPARAM lParam)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  int acc = system->m_wheelDeltaAccum;
  int delta = GET_WHEEL_DELTA_WPARAM(wParam);

  if (acc * delta < 0) {
    // scroll direction reversed.
    acc = 0;
  }
  acc += delta;
  int direction = (acc >= 0) ? 1 : -1;
  acc = abs(acc);

  while (acc >= WHEEL_DELTA) {
    system->pushEvent(new GHOST_EventWheel(system->getMilliSeconds(), window, direction));
    acc -= WHEEL_DELTA;
  }
  system->m_wheelDeltaAccum = acc * direction;
}

GHOST_EventKey *GHOST_SystemWin32::processKeyEvent(GHOST_WindowWin32 *window, RAWINPUT const &raw)
{
  int keyDown = 0;
  char vk;
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  GHOST_TKey key = system->hardKey(raw, &keyDown, &vk);
  GHOST_EventKey *event;

  if (key != GHOST_kKeyUnknown) {
    char utf8_char[6] = {0};
    char ascii = 0;

    wchar_t utf16[3] = {0};
    BYTE state[256] = {0};
    int r;
    GetKeyboardState((PBYTE)state);

    // don't call ToUnicodeEx on dead keys as it clears the buffer and so won't allow diacritical composition.
    if (MapVirtualKeyW(vk, 2) != 0) {
      // todo: ToUnicodeEx can respond with up to 4 utf16 chars (only 2 here). Could be up to 24 utf8 bytes.
      if ((r = ToUnicodeEx(
               vk, raw.data.keyboard.MakeCode, state, utf16, 2, 0, system->m_keylayout))) {
        if ((r > 0 && r < 3)) {
          utf16[r] = 0;
          conv_utf_16_to_8(utf16, utf8_char, 6);
        }
        else if (r == -1) {
          utf8_char[0] = '\0';
        }
      }
    }

    if (!keyDown) {
      utf8_char[0] = '\0';
      ascii = '\0';
    }
    else {
      ascii = utf8_char[0] & 0x80 ? '?' : utf8_char[0];
    }

    event = new GHOST_EventKey(system->getMilliSeconds(),
                               keyDown ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
                               window,
                               key,
                               ascii,
                               utf8_char);

    // GHOST_PRINTF("%c\n", ascii); // we already get this info via EventPrinter
  }
  else {
    event = NULL;
  }
  return event;
}

GHOST_Event *GHOST_SystemWin32::processWindowEvent(GHOST_TEventType type,
                                                   GHOST_WindowWin32 *window)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  if (type == GHOST_kEventWindowActivate) {
    system->getWindowManager()->setActiveWindow(window);
    window->bringTabletContextToFront();
  }

  return new GHOST_Event(system->getMilliSeconds(), type, window);
}

#ifdef WITH_INPUT_IME
GHOST_Event *GHOST_SystemWin32::processImeEvent(GHOST_TEventType type,
                                                GHOST_WindowWin32 *window,
                                                GHOST_TEventImeData *data)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  return new GHOST_EventIME(system->getMilliSeconds(), type, window, data);
}
#endif

GHOST_TSuccess GHOST_SystemWin32::pushDragDropEvent(GHOST_TEventType eventType,
                                                    GHOST_TDragnDropTypes draggedObjectType,
                                                    GHOST_WindowWin32 *window,
                                                    int mouseX,
                                                    int mouseY,
                                                    void *data)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  return system->pushEvent(new GHOST_EventDragnDrop(
      system->getMilliSeconds(), eventType, draggedObjectType, window, mouseX, mouseY, data));
}

void GHOST_SystemWin32::processMinMaxInfo(MINMAXINFO *minmax)
{
  minmax->ptMinTrackSize.x = 320;
  minmax->ptMinTrackSize.y = 240;
}

#ifdef WITH_INPUT_NDOF
bool GHOST_SystemWin32::processNDOF(RAWINPUT const &raw)
{
  bool eventSent = false;
  GHOST_TUns64 now = getMilliSeconds();

  static bool firstEvent = true;
  if (firstEvent) {  // determine exactly which device is plugged in
    RID_DEVICE_INFO info;
    unsigned infoSize = sizeof(RID_DEVICE_INFO);
    info.cbSize = infoSize;

    GetRawInputDeviceInfo(raw.header.hDevice, RIDI_DEVICEINFO, &info, &infoSize);
    if (info.dwType == RIM_TYPEHID)
      m_ndofManager->setDevice(info.hid.dwVendorId, info.hid.dwProductId);
    else
      GHOST_PRINT("<!> not a HID device... mouse/kb perhaps?\n");

    firstEvent = false;
  }

  // The NDOF manager sends button changes immediately, and *pretends* to
  // send motion. Mark as 'sent' so motion will always get dispatched.
  eventSent = true;

  BYTE const *data = raw.data.hid.bRawData;

  BYTE packetType = data[0];
  switch (packetType) {
    case 1:  // translation
    {
      const short *axis = (short *)(data + 1);
      // massage into blender view coords (same goes for rotation)
      const int t[3] = {axis[0], -axis[2], axis[1]};
      m_ndofManager->updateTranslation(t, now);

      if (raw.data.hid.dwSizeHid == 13) {
        // this report also includes rotation
        const int r[3] = {-axis[3], axis[5], -axis[4]};
        m_ndofManager->updateRotation(r, now);

        // I've never gotten one of these, has anyone else?
        GHOST_PRINT("ndof: combined T + R\n");
      }
      break;
    }
    case 2:  // rotation
    {
      const short *axis = (short *)(data + 1);
      const int r[3] = {-axis[0], axis[2], -axis[1]};
      m_ndofManager->updateRotation(r, now);
      break;
    }
    case 3:  // buttons
    {
      int button_bits;
      memcpy(&button_bits, data + 1, sizeof(button_bits));
      m_ndofManager->updateButtons(button_bits, now);
      break;
    }
  }
  return eventSent;
}
#endif  // WITH_INPUT_NDOF

LRESULT WINAPI GHOST_SystemWin32::s_wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  GHOST_Event *event = NULL;
  bool eventHandled = false;

  LRESULT lResult = 0;
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  GHOST_EventManager *eventManager = system->getEventManager();
  GHOST_ASSERT(system, "GHOST_SystemWin32::s_wndProc(): system not initialized");

  if (hwnd) {
#if 0
    // Disabled due to bug in Intel drivers, see T51959
    if (msg == WM_NCCREATE) {
      // Tell Windows to automatically handle scaling of non-client areas
      // such as the caption bar. EnableNonClientDpiScaling was introduced in Windows 10
      HMODULE m_user32 = ::LoadLibrary("User32.dll");
      if (m_user32) {
        GHOST_WIN32_EnableNonClientDpiScaling fpEnableNonClientDpiScaling =
            (GHOST_WIN32_EnableNonClientDpiScaling)::GetProcAddress(m_user32,
                                                                    "EnableNonClientDpiScaling");

        if (fpEnableNonClientDpiScaling) {
          fpEnableNonClientDpiScaling(hwnd);
        }
      }
    }
#endif

    GHOST_WindowWin32 *window = (GHOST_WindowWin32 *)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (window) {
      switch (msg) {
        // we need to check if new key layout has AltGr
        case WM_INPUTLANGCHANGE: {
          system->handleKeyboardChange();
#ifdef WITH_INPUT_IME
          window->getImeInput()->SetInputLanguage();
#endif
          break;
        }
        ////////////////////////////////////////////////////////////////////////
        // Keyboard events, processed
        ////////////////////////////////////////////////////////////////////////
        case WM_INPUT: {
          // check WM_INPUT from input sink when ghost window is not in the foreground
          if (wParam == RIM_INPUTSINK) {
            if (GetFocus() != hwnd)  // WM_INPUT message not for this window
              return 0;
          }  //else wParam == RIM_INPUT

          RAWINPUT raw;
          RAWINPUT *raw_ptr = &raw;
          UINT rawSize = sizeof(RAWINPUT);

          GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw_ptr, &rawSize, sizeof(RAWINPUTHEADER));

          switch (raw.header.dwType) {
            case RIM_TYPEKEYBOARD:
              event = processKeyEvent(window, raw);
              if (!event) {
                GHOST_PRINT("GHOST_SystemWin32::wndProc: key event ");
                GHOST_PRINT(msg);
                GHOST_PRINT(" key ignored\n");
              }
              break;
#ifdef WITH_INPUT_NDOF
            case RIM_TYPEHID:
              if (system->processNDOF(raw)) {
                eventHandled = true;
              }
              break;
#endif
          }
          break;
        }
#ifdef WITH_INPUT_IME
        ////////////////////////////////////////////////////////////////////////
        // IME events, processed, read more in GHOST_IME.h
        ////////////////////////////////////////////////////////////////////////
        case WM_IME_SETCONTEXT: {
          GHOST_ImeWin32 *ime = window->getImeInput();
          ime->SetInputLanguage();
          ime->CreateImeWindow(hwnd);
          ime->CleanupComposition(hwnd);
          ime->CheckFirst(hwnd);
          break;
        }
        case WM_IME_STARTCOMPOSITION: {
          GHOST_ImeWin32 *ime = window->getImeInput();
          eventHandled = true;
          /* remove input event before start comp event, avoid redundant input */
          eventManager->removeTypeEvents(GHOST_kEventKeyDown, window);
          ime->CreateImeWindow(hwnd);
          ime->ResetComposition(hwnd);
          event = processImeEvent(GHOST_kEventImeCompositionStart, window, &ime->eventImeData);
          break;
        }
        case WM_IME_COMPOSITION: {
          GHOST_ImeWin32 *ime = window->getImeInput();
          eventHandled = true;
          ime->UpdateImeWindow(hwnd);
          ime->UpdateInfo(hwnd);
          if (ime->eventImeData.result_len) {
            /* remove redundant IME event */
            eventManager->removeTypeEvents(GHOST_kEventImeComposition, window);
          }
          event = processImeEvent(GHOST_kEventImeComposition, window, &ime->eventImeData);
          break;
        }
        case WM_IME_ENDCOMPOSITION: {
          GHOST_ImeWin32 *ime = window->getImeInput();
          eventHandled = true;
          /* remove input event after end comp event, avoid redundant input */
          eventManager->removeTypeEvents(GHOST_kEventKeyDown, window);
          ime->ResetComposition(hwnd);
          ime->DestroyImeWindow(hwnd);
          event = processImeEvent(GHOST_kEventImeCompositionEnd, window, &ime->eventImeData);
          break;
        }
#endif /* WITH_INPUT_IME */
        ////////////////////////////////////////////////////////////////////////
        // Keyboard events, ignored
        ////////////////////////////////////////////////////////////////////////
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        /* These functions were replaced by WM_INPUT*/
        case WM_CHAR:
        /* The WM_CHAR message is posted to the window with the keyboard focus when
         * a WM_KEYDOWN message is translated by the TranslateMessage function. WM_CHAR
         * contains the character code of the key that was pressed.
         */
        case WM_DEADCHAR:
          /* The WM_DEADCHAR message is posted to the window with the keyboard focus when a
           * WM_KEYUP message is translated by the TranslateMessage function. WM_DEADCHAR
           * specifies a character code generated by a dead key. A dead key is a key that
           * generates a character, such as the umlaut (double-dot), that is combined with
           * another character to form a composite character. For example, the umlaut-O
           * character (Ö) is generated by typing the dead key for the umlaut character, and
           * then typing the O key.
           */
          break;
        case WM_SYSDEADCHAR:
        /* The WM_SYSDEADCHAR message is sent to the window with the keyboard focus when
         * a WM_SYSKEYDOWN message is translated by the TranslateMessage function.
         * WM_SYSDEADCHAR specifies the character code of a system dead key - that is,
         * a dead key that is pressed while holding down the alt key.
         */
        case WM_SYSCHAR:
          /* The WM_SYSCHAR message is sent to the window with the keyboard focus when
           * a WM_SYSCHAR message is translated by the TranslateMessage function.
           * WM_SYSCHAR specifies the character code of a dead key - that is,
           * a dead key that is pressed while holding down the alt key.
           * To prevent the sound, DefWindowProc must be avoided by return
           */
          break;
        case WM_SYSCOMMAND:
          /* The WM_SYSCHAR message is sent to the window when system commands such as
           * maximize, minimize  or close the window are triggered. Also it is sent when ALT
           * button is press for menu. To prevent this we must return preventing DefWindowProc.
           */
          if (wParam == SC_KEYMENU) {
            eventHandled = true;
          }
          break;
        ////////////////////////////////////////////////////////////////////////
        // Tablet events, processed
        ////////////////////////////////////////////////////////////////////////
        case WT_PACKET:
          window->processWin32TabletEvent(wParam, lParam);
          break;
        case WT_CSRCHANGE:
        case WT_PROXIMITY:
          window->processWin32TabletInitEvent();
          break;
        ////////////////////////////////////////////////////////////////////////
        // Pointer events, processed
        ////////////////////////////////////////////////////////////////////////
        case WM_POINTERDOWN:
          event = processPointerEvent(
              GHOST_kEventButtonDown, window, wParam, lParam, eventHandled);
          if (event && eventHandled) {
            window->registerMouseClickEvent(0);
          }
          break;
        case WM_POINTERUP:
          event = processPointerEvent(GHOST_kEventButtonUp, window, wParam, lParam, eventHandled);
          if (event && eventHandled) {
            window->registerMouseClickEvent(1);
          }
          break;
        case WM_POINTERUPDATE:
          event = processPointerEvent(
              GHOST_kEventCursorMove, window, wParam, lParam, eventHandled);
          break;
        ////////////////////////////////////////////////////////////////////////
        // Mouse events, processed
        ////////////////////////////////////////////////////////////////////////
        case WM_LBUTTONDOWN:
          window->registerMouseClickEvent(0);
          event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskLeft);
          break;
        case WM_MBUTTONDOWN:
          window->registerMouseClickEvent(0);
          event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskMiddle);
          break;
        case WM_RBUTTONDOWN:
          window->registerMouseClickEvent(0);
          event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskRight);
          break;
        case WM_XBUTTONDOWN:
          window->registerMouseClickEvent(0);
          if ((short)HIWORD(wParam) == XBUTTON1) {
            event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskButton4);
          }
          else if ((short)HIWORD(wParam) == XBUTTON2) {
            event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskButton5);
          }
          break;
        case WM_LBUTTONUP:
          window->registerMouseClickEvent(1);
          event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskLeft);
          break;
        case WM_MBUTTONUP:
          window->registerMouseClickEvent(1);
          event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskMiddle);
          break;
        case WM_RBUTTONUP:
          window->registerMouseClickEvent(1);
          event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskRight);
          break;
        case WM_XBUTTONUP:
          window->registerMouseClickEvent(1);
          if ((short)HIWORD(wParam) == XBUTTON1) {
            event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskButton4);
          }
          else if ((short)HIWORD(wParam) == XBUTTON2) {
            event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskButton5);
          }
          break;
        case WM_MOUSEMOVE:
          event = processCursorEvent(GHOST_kEventCursorMove, window);
          break;
        case WM_MOUSEWHEEL: {
          /* The WM_MOUSEWHEEL message is sent to the focus window
           * when the mouse wheel is rotated. The DefWindowProc
           * function propagates the message to the window's parent.
           * There should be no internal forwarding of the message,
           * since DefWindowProc propagates it up the parent chain
           * until it finds a window that processes it.
           */

          /* Get the window under the mouse and send event to its queue. */
          POINT mouse_pos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
          HWND mouse_hwnd = ChildWindowFromPoint(HWND_DESKTOP, mouse_pos);
          GHOST_WindowWin32 *mouse_window = (GHOST_WindowWin32 *)::GetWindowLongPtr(mouse_hwnd,
                                                                                    GWLP_USERDATA);

          processWheelEvent(mouse_window ? mouse_window : window, wParam, lParam);
          eventHandled = true;
#ifdef BROKEN_PEEK_TOUCHPAD
          PostMessage(hwnd, WM_USER, 0, 0);
#endif
          break;
        }
        case WM_SETCURSOR:
          /* The WM_SETCURSOR message is sent to a window if the mouse causes the cursor
           * to move within a window and mouse input is not captured.
           * This means we have to set the cursor shape every time the mouse moves!
           * The DefWindowProc function uses this message to set the cursor to an
           * arrow if it is not in the client area.
           */
          if (LOWORD(lParam) == HTCLIENT) {
            // Load the current cursor
            window->loadCursor(window->getCursorVisibility(), window->getCursorShape());
            // Bypass call to DefWindowProc
            return 0;
          }
          else {
            // Outside of client area show standard cursor
            window->loadCursor(true, GHOST_kStandardCursorDefault);
          }
          break;

        ////////////////////////////////////////////////////////////////////////
        // Mouse events, ignored
        ////////////////////////////////////////////////////////////////////////
        case WM_NCMOUSEMOVE:
        /* The WM_NCMOUSEMOVE message is posted to a window when the cursor is moved
         * within the nonclient area of the window. This message is posted to the window
         * that contains the cursor. If a window has captured the mouse, this message is not posted.
         */
        case WM_NCHITTEST:
          /* The WM_NCHITTEST message is sent to a window when the cursor moves, or
           * when a mouse button is pressed or released. If the mouse is not captured,
           * the message is sent to the window beneath the cursor. Otherwise, the message
           * is sent to the window that has captured the mouse.
           */
          break;

        ////////////////////////////////////////////////////////////////////////
        // Window events, processed
        ////////////////////////////////////////////////////////////////////////
        case WM_CLOSE:
          /* The WM_CLOSE message is sent as a signal that a window or an application should terminate. */
          event = processWindowEvent(GHOST_kEventWindowClose, window);
          break;
        case WM_ACTIVATE:
          /* The WM_ACTIVATE message is sent to both the window being activated and the window being
           * deactivated. If the windows use the same input queue, the message is sent synchronously,
           * first to the window procedure of the top-level window being deactivated, then to the window
           * procedure of the top-level window being activated. If the windows use different input queues,
           * the message is sent asynchronously, so the window is activated immediately.
           */
          {
            GHOST_ModifierKeys modifiers;
            modifiers.clear();
            system->storeModifierKeys(modifiers);
            system->m_wheelDeltaAccum = 0;
            event = processWindowEvent(LOWORD(wParam) ? GHOST_kEventWindowActivate :
                                                        GHOST_kEventWindowDeactivate,
                                       window);
            /* WARNING: Let DefWindowProc handle WM_ACTIVATE, otherwise WM_MOUSEWHEEL
           * will not be dispatched to OUR active window if we minimize one of OUR windows. */
            if (LOWORD(wParam) == WA_INACTIVE)
              window->lostMouseCapture();
            window->processWin32TabletActivateEvent(GET_WM_ACTIVATE_STATE(wParam, lParam));
            lResult = ::DefWindowProc(hwnd, msg, wParam, lParam);
            break;
          }
        case WM_ENTERSIZEMOVE:
          /* The WM_ENTERSIZEMOVE message is sent one time to a window after it enters the moving
           * or sizing modal loop. The window enters the moving or sizing modal loop when the user
           * clicks the window's title bar or sizing border, or when the window passes the
           * WM_SYSCOMMAND message to the DefWindowProc function and the wParam parameter of the
           * message specifies the SC_MOVE or SC_SIZE value. The operation is complete when
           * DefWindowProc returns.
           */
          window->m_inLiveResize = 1;
          break;
        case WM_EXITSIZEMOVE:
          window->m_inLiveResize = 0;
          break;
        case WM_PAINT:
          /* An application sends the WM_PAINT message when the system or another application
           * makes a request to paint a portion of an application's window. The message is sent
           * when the UpdateWindow or RedrawWindow function is called, or by the DispatchMessage
           * function when the application obtains a WM_PAINT message by using the GetMessage or
           * PeekMessage function.
           */
          if (!window->m_inLiveResize) {
            event = processWindowEvent(GHOST_kEventWindowUpdate, window);
            ::ValidateRect(hwnd, NULL);
          }
          else {
            eventHandled = true;
          }
          break;
        case WM_GETMINMAXINFO:
          /* The WM_GETMINMAXINFO message is sent to a window when the size or
           * position of the window is about to change. An application can use
           * this message to override the window's default maximized size and
           * position, or its default minimum or maximum tracking size.
           */
          processMinMaxInfo((MINMAXINFO *)lParam);
          /* Let DefWindowProc handle it. */
          break;
        case WM_SIZING:
        case WM_SIZE:
          /* The WM_SIZE message is sent to a window after its size has changed.
           * The WM_SIZE and WM_MOVE messages are not sent if an application handles the
           * WM_WINDOWPOSCHANGED message without calling DefWindowProc. It is more efficient
           * to perform any move or size change processing during the WM_WINDOWPOSCHANGED
           * message without calling DefWindowProc.
           */
          /* we get first WM_SIZE before we fully init. So, do not dispatch before we continiously resizng */
          if (window->m_inLiveResize) {
            system->pushEvent(processWindowEvent(GHOST_kEventWindowSize, window));
            system->dispatchEvents();
          }
          else {
            event = processWindowEvent(GHOST_kEventWindowSize, window);
          }
          break;
        case WM_CAPTURECHANGED:
          window->lostMouseCapture();
          break;
        case WM_MOVING:
          /* The WM_MOVING message is sent to a window that the user is moving. By processing
           * this message, an application can monitor the size and position of the drag rectangle
           * and, if needed, change its size or position.
           */
        case WM_MOVE:
          /* The WM_SIZE and WM_MOVE messages are not sent if an application handles the
           * WM_WINDOWPOSCHANGED message without calling DefWindowProc. It is more efficient
           * to perform any move or size change processing during the WM_WINDOWPOSCHANGED
           * message without calling DefWindowProc.
           */
          /* see WM_SIZE comment*/
          if (window->m_inLiveResize) {
            system->pushEvent(processWindowEvent(GHOST_kEventWindowMove, window));
            system->dispatchEvents();
          }
          else {
            event = processWindowEvent(GHOST_kEventWindowMove, window);
          }

          break;
        case WM_DPICHANGED:
          /* The WM_DPICHANGED message is sent when the effective dots per inch (dpi) for a window has changed.
           * The DPI is the scale factor for a window. There are multiple events that can cause the DPI to
           * change such as when the window is moved to a monitor with a different DPI.
           */
          {
            // The suggested new size and position of the window.
            RECT *const suggestedWindowRect = (RECT *)lParam;

            // Push DPI change event first
            system->pushEvent(processWindowEvent(GHOST_kEventWindowDPIHintChanged, window));
            system->dispatchEvents();
            eventHandled = true;

            // Then move and resize window
            SetWindowPos(hwnd,
                         NULL,
                         suggestedWindowRect->left,
                         suggestedWindowRect->top,
                         suggestedWindowRect->right - suggestedWindowRect->left,
                         suggestedWindowRect->bottom - suggestedWindowRect->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
          }
          break;
        ////////////////////////////////////////////////////////////////////////
        // Window events, ignored
        ////////////////////////////////////////////////////////////////////////
        case WM_WINDOWPOSCHANGED:
        /* The WM_WINDOWPOSCHANGED message is sent to a window whose size, position, or place
         * in the Z order has changed as a result of a call to the SetWindowPos function or
         * another window-management function.
         * The WM_SIZE and WM_MOVE messages are not sent if an application handles the
         * WM_WINDOWPOSCHANGED message without calling DefWindowProc. It is more efficient
         * to perform any move or size change processing during the WM_WINDOWPOSCHANGED
         * message without calling DefWindowProc.
         */
        case WM_ERASEBKGND:
        /* An application sends the WM_ERASEBKGND message when the window background must be
         * erased (for example, when a window is resized). The message is sent to prepare an
         * invalidated portion of a window for painting.
         */
        case WM_NCPAINT:
        /* An application sends the WM_NCPAINT message to a window when its frame must be painted. */
        case WM_NCACTIVATE:
        /* The WM_NCACTIVATE message is sent to a window when its nonclient area needs to be changed
         * to indicate an active or inactive state.
         */
        case WM_DESTROY:
        /* The WM_DESTROY message is sent when a window is being destroyed. It is sent to the window
         * procedure of the window being destroyed after the window is removed from the screen.
         * This message is sent first to the window being destroyed and then to the child windows
         * (if any) as they are destroyed. During the processing of the message, it can be assumed
         * that all child windows still exist.
         */
        case WM_NCDESTROY:
          /* The WM_NCDESTROY message informs a window that its nonclient area is being destroyed. The
           * DestroyWindow function sends the WM_NCDESTROY message to the window following the WM_DESTROY
           * message. WM_DESTROY is used to free the allocated memory object associated with the window.
           */
          break;
        case WM_KILLFOCUS:
          /* The WM_KILLFOCUS message is sent to a window immediately before it loses the keyboard focus.
           * We want to prevent this if a window is still active and it loses focus to nowhere*/
          if (!wParam && hwnd == ::GetActiveWindow())
            ::SetFocus(hwnd);
        case WM_SHOWWINDOW:
        /* The WM_SHOWWINDOW message is sent to a window when the window is about to be hidden or shown. */
        case WM_WINDOWPOSCHANGING:
        /* The WM_WINDOWPOSCHANGING message is sent to a window whose size, position, or place in
         * the Z order is about to change as a result of a call to the SetWindowPos function or
         * another window-management function.
         */
        case WM_SETFOCUS:
          /* The WM_SETFOCUS message is sent to a window after it has gained the keyboard focus. */
          break;
        ////////////////////////////////////////////////////////////////////////
        // Other events
        ////////////////////////////////////////////////////////////////////////
        case WM_GETTEXT:
        /* An application sends a WM_GETTEXT message to copy the text that
         * corresponds to a window into a buffer provided by the caller.
         */
        case WM_ACTIVATEAPP:
        /* The WM_ACTIVATEAPP message is sent when a window belonging to a
         * different application than the active window is about to be activated.
         * The message is sent to the application whose window is being activated
         * and to the application whose window is being deactivated.
         */
        case WM_TIMER:
          /* The WIN32 docs say:
           * The WM_TIMER message is posted to the installing thread's message queue
           * when a timer expires. You can process the message by providing a WM_TIMER
           * case in the window procedure. Otherwise, the default window procedure will
           * call the TimerProc callback function specified in the call to the SetTimer
           * function used to install the timer.
           *
           * In GHOST, we let DefWindowProc call the timer callback.
           */
          break;
      }
    }
    else {
      // Event found for a window before the pointer to the class has been set.
      GHOST_PRINT("GHOST_SystemWin32::wndProc: GHOST window event before creation\n");
      /* These are events we typically miss at this point:
       * WM_GETMINMAXINFO 0x24
       * WM_NCCREATE          0x81
       * WM_NCCALCSIZE        0x83
       * WM_CREATE            0x01
       * We let DefWindowProc do the work.
       */
    }
  }
  else {
    // Events without valid hwnd
    GHOST_PRINT("GHOST_SystemWin32::wndProc: event without window\n");
  }

  if (event) {
    system->pushEvent(event);
    eventHandled = true;
  }

  if (!eventHandled)
    lResult = ::DefWindowProcW(hwnd, msg, wParam, lParam);

  return lResult;
}

GHOST_TUns8 *GHOST_SystemWin32::getClipboard(bool selection) const
{
  char *temp_buff;

  if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(NULL)) {
    wchar_t *buffer;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData == NULL) {
      CloseClipboard();
      return NULL;
    }
    buffer = (wchar_t *)GlobalLock(hData);
    if (!buffer) {
      CloseClipboard();
      return NULL;
    }

    temp_buff = alloc_utf_8_from_16(buffer, 0);

    /* Buffer mustn't be accessed after CloseClipboard
     * it would like accessing free-d memory */
    GlobalUnlock(hData);
    CloseClipboard();

    return (GHOST_TUns8 *)temp_buff;
  }
  else if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(NULL)) {
    char *buffer;
    size_t len = 0;
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == NULL) {
      CloseClipboard();
      return NULL;
    }
    buffer = (char *)GlobalLock(hData);
    if (!buffer) {
      CloseClipboard();
      return NULL;
    }

    len = strlen(buffer);
    temp_buff = (char *)malloc(len + 1);
    strncpy(temp_buff, buffer, len);
    temp_buff[len] = '\0';

    /* Buffer mustn't be accessed after CloseClipboard
     * it would like accessing free-d memory */
    GlobalUnlock(hData);
    CloseClipboard();

    return (GHOST_TUns8 *)temp_buff;
  }
  else {
    return NULL;
  }
}

void GHOST_SystemWin32::putClipboard(GHOST_TInt8 *buffer, bool selection) const
{
  if (selection) {
    return;
  }  // for copying the selection, used on X11

  if (OpenClipboard(NULL)) {
    HLOCAL clipbuffer;
    wchar_t *data;

    if (buffer) {
      size_t len = count_utf_16_from_8(buffer);
      EmptyClipboard();

      clipbuffer = LocalAlloc(LMEM_FIXED, sizeof(wchar_t) * len);
      data = (wchar_t *)GlobalLock(clipbuffer);

      conv_utf_8_to_16(buffer, data, len);

      LocalUnlock(clipbuffer);
      SetClipboardData(CF_UNICODETEXT, clipbuffer);
    }
    CloseClipboard();
  }
  else {
    return;
  }
}

static DWORD GetParentProcessID(void)
{
  HANDLE snapshot;
  PROCESSENTRY32 pe32 = {0};
  DWORD ppid = 0, pid = GetCurrentProcessId();
  snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    return -1;
  }
  pe32.dwSize = sizeof(pe32);
  if (!Process32First(snapshot, &pe32)) {
    CloseHandle(snapshot);
    return -1;
  }
  do {
    if (pe32.th32ProcessID == pid) {
      ppid = pe32.th32ParentProcessID;
      break;
    }
  } while (Process32Next(snapshot, &pe32));
  CloseHandle(snapshot);
  return ppid;
}

static bool getProcessName(int pid, char *buffer, int max_len)
{
  bool result = false;
  HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  if (handle) {
    GetModuleFileNameEx(handle, 0, buffer, max_len);
    result = true;
  }
  CloseHandle(handle);
  return result;
}

static bool isStartedFromCommandPrompt()
{
  HWND hwnd = GetConsoleWindow();

  if (hwnd) {
    DWORD pid = (DWORD)-1;
    DWORD ppid = GetParentProcessID();
    char parent_name[MAX_PATH];
    bool start_from_launcher = false;

    GetWindowThreadProcessId(hwnd, &pid);
    if (getProcessName(ppid, parent_name, sizeof(parent_name))) {
      char *filename = strrchr(parent_name, '\\');
      if (filename != NULL) {
        start_from_launcher = strstr(filename, "blender.exe") != NULL;
      }
    }

    /* When we're starting from a wrapper we need to compare with parent process ID. */
    if (pid != (start_from_launcher ? ppid : GetCurrentProcessId()))
      return true;
  }

  return false;
}

int GHOST_SystemWin32::toggleConsole(int action)
{
  HWND wnd = GetConsoleWindow();

  switch (action) {
    case 3:  // startup: hide if not started from command prompt
    {
      if (!isStartedFromCommandPrompt()) {
        ShowWindow(wnd, SW_HIDE);
        m_consoleStatus = 0;
      }
      break;
    }
    case 0:  // hide
      ShowWindow(wnd, SW_HIDE);
      m_consoleStatus = 0;
      break;
    case 1:  // show
      ShowWindow(wnd, SW_SHOW);
      if (!isStartedFromCommandPrompt()) {
        DeleteMenu(GetSystemMenu(wnd, FALSE), SC_CLOSE, MF_BYCOMMAND);
      }
      m_consoleStatus = 1;
      break;
    case 2:  // toggle
      ShowWindow(wnd, m_consoleStatus ? SW_HIDE : SW_SHOW);
      m_consoleStatus = !m_consoleStatus;
      if (m_consoleStatus && !isStartedFromCommandPrompt()) {
        DeleteMenu(GetSystemMenu(wnd, FALSE), SC_CLOSE, MF_BYCOMMAND);
      }
      break;
  }

  return m_consoleStatus;
}

int GHOST_SystemWin32::confirmQuit(GHOST_IWindow *window) const
{
  return (MessageBox(window ? ((GHOST_WindowWin32 *)window)->getHWND() : 0,
                     "Some changes have not been saved.\nDo you really want to quit?",
                     "Exit Blender",
                     MB_OKCANCEL | MB_ICONWARNING | MB_TOPMOST) == IDOK);
}
