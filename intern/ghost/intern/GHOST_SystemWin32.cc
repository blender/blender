/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_SystemWin32.hh"
#include "GHOST_ContextD3D.hh"
#include "GHOST_EventDragnDrop.hh"
#include "GHOST_EventTrackpad.hh"

#ifndef _WIN32_IE
#  define _WIN32_IE 0x0501 /* shipped before XP, so doesn't impose additional requirements */
#endif

#include <commctrl.h>
#include <dwmapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <windowsx.h>

#include "utf_winfunc.h"
#include "utfconv.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "GHOST_DisplayManagerWin32.hh"
#include "GHOST_EventButton.hh"
#include "GHOST_EventCursor.hh"
#include "GHOST_EventKey.hh"
#include "GHOST_EventWheel.hh"
#include "GHOST_TimerManager.hh"
#include "GHOST_TimerTask.hh"
#include "GHOST_WindowManager.hh"
#include "GHOST_WindowWin32.hh"

#include "GHOST_ContextWGL.hh"
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#ifdef WITH_INPUT_NDOF
#  include "GHOST_NDOFManagerWin32.hh"
#endif

/* Key code values not found in `winuser.h`. */
#ifndef VK_MINUS
#  define VK_MINUS 0xBD
#endif /* VK_MINUS */
#ifndef VK_SEMICOLON
#  define VK_SEMICOLON 0xBA
#endif /* VK_SEMICOLON */
#ifndef VK_PERIOD
#  define VK_PERIOD 0xBE
#endif /* VK_PERIOD */
#ifndef VK_COMMA
#  define VK_COMMA 0xBC
#endif /* VK_COMMA */
#ifndef VK_BACK_QUOTE
#  define VK_BACK_QUOTE 0xC0
#endif /* VK_BACK_QUOTE */
#ifndef VK_SLASH
#  define VK_SLASH 0xBF
#endif /* VK_SLASH */
#ifndef VK_BACK_SLASH
#  define VK_BACK_SLASH 0xDC
#endif /* VK_BACK_SLASH */
#ifndef VK_EQUALS
#  define VK_EQUALS 0xBB
#endif /* VK_EQUALS */
#ifndef VK_OPEN_BRACKET
#  define VK_OPEN_BRACKET 0xDB
#endif /* VK_OPEN_BRACKET */
#ifndef VK_CLOSE_BRACKET
#  define VK_CLOSE_BRACKET 0xDD
#endif /* VK_CLOSE_BRACKET */
#ifndef VK_GR_LESS
#  define VK_GR_LESS 0xE2
#endif /* VK_GR_LESS */

/**
 * Workaround for some laptop touch-pads, some of which seems to
 * have driver issues which makes it so window function receives
 * the message, but #PeekMessage doesn't pick those messages for
 * some reason.
 *
 * We send a dummy WM_USER message to force #PeekMessage to receive
 * something, making it so blender's window manager sees the new
 * messages coming in.
 */
#define BROKEN_PEEK_TOUCHPAD

static bool isStartedFromCommandPrompt();

static void initRawInput()
{
#ifdef WITH_INPUT_NDOF
#  define DEVICE_COUNT 2
#else
#  define DEVICE_COUNT 1
#endif

  RAWINPUTDEVICE devices[DEVICE_COUNT];
  memset(devices, 0, DEVICE_COUNT * sizeof(RAWINPUTDEVICE));

  /* Initiates WM_INPUT messages from keyboard
   * That way GHOST can retrieve true keys. */
  devices[0].usUsagePage = 0x01;
  devices[0].usUsage = 0x06; /* http://msdn.microsoft.com/en-us/windows/hardware/gg487473.aspx */

#ifdef WITH_INPUT_NDOF
  /* multi-axis mouse (SpaceNavigator, etc.). */
  devices[1].usUsagePage = 0x01;
  devices[1].usUsage = 0x08;
#endif

  if (RegisterRawInputDevices(devices, DEVICE_COUNT, sizeof(RAWINPUTDEVICE))) {
    /* Success. */
  }
  else {
    GHOST_PRINTF("could not register for RawInput: %d\n", (int)GetLastError());
  }
#undef DEVICE_COUNT
}

typedef BOOL(API *GHOST_WIN32_EnableNonClientDpiScaling)(HWND);

GHOST_SystemWin32::GHOST_SystemWin32()
    : m_hasPerformanceCounter(false), m_freq(0), m_start(0), m_lfstart(0)
{
  m_displayManager = new GHOST_DisplayManagerWin32();
  GHOST_ASSERT(m_displayManager, "GHOST_SystemWin32::GHOST_SystemWin32(): m_displayManager==0\n");
  m_displayManager->initialize();

  m_consoleStatus = true;

  /* Tell Windows we are per monitor DPI aware. This disables the default
   * blurry scaling and enables WM_DPICHANGED to allow us to draw at proper DPI. */
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);

  /* Set App Id for the process so our console will be grouped on the Task Bar. */
  UTF16_ENCODE(BLENDER_WIN_APPID);
  SetCurrentProcessExplicitAppUserModelID(BLENDER_WIN_APPID_16);
  UTF16_UN_ENCODE(BLENDER_WIN_APPID);

  /* Check if current keyboard layout uses AltGr and save keylayout ID for
   * specialized handling if keys like VK_OEM_*. I.e. french keylayout
   * generates #VK_OEM_8 for their exclamation key (key left of right shift). */
  this->handleKeyboardChange();
  /* Require COM for GHOST_DropTargetWin32 created in GHOST_WindowWin32. */
  OleInitialize(0);

#ifdef WITH_INPUT_NDOF
  m_ndofManager = new GHOST_NDOFManagerWin32(*this);
#endif
}

GHOST_SystemWin32::~GHOST_SystemWin32()
{
  /* Shutdown COM. */
  OleUninitialize();

  if (isStartedFromCommandPrompt()) {
    setConsoleWindowState(GHOST_kConsoleWindowStateShow);
  }
}

uint64_t GHOST_SystemWin32::performanceCounterToMillis(__int64 perf_ticks) const
{
  /* Calculate the time passed since system initialization. */
  __int64 delta = (perf_ticks - m_start) * 1000;

  uint64_t t = (uint64_t)(delta / m_freq);
  return t;
}

uint64_t GHOST_SystemWin32::tickCountToMillis(__int64 ticks) const
{
  return ticks - m_lfstart;
}

uint64_t GHOST_SystemWin32::getMilliSeconds() const
{
  /* Hardware does not support high resolution timers. We will use GetTickCount instead then. */
  if (!m_hasPerformanceCounter) {
    return tickCountToMillis(::GetTickCount());
  }

  /* Retrieve current count */
  __int64 count = 0;
  ::QueryPerformanceCounter((LARGE_INTEGER *)&count);

  return performanceCounterToMillis(count);
}

uint8_t GHOST_SystemWin32::getNumDisplays() const
{
  GHOST_ASSERT(m_displayManager, "GHOST_SystemWin32::getNumDisplays(): m_displayManager==0\n");
  uint8_t numDisplays;
  m_displayManager->getNumDisplays(numDisplays);
  return numDisplays;
}

void GHOST_SystemWin32::getMainDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  width = ::GetSystemMetrics(SM_CXSCREEN);
  height = ::GetSystemMetrics(SM_CYSCREEN);
}

void GHOST_SystemWin32::getAllDisplayDimensions(uint32_t &width, uint32_t &height) const
{
  width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
  height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
}

GHOST_IWindow *GHOST_SystemWin32::createWindow(const char *title,
                                               int32_t left,
                                               int32_t top,
                                               uint32_t width,
                                               uint32_t height,
                                               GHOST_TWindowState state,
                                               GHOST_GPUSettings gpuSettings,
                                               const bool /*exclusive*/,
                                               const bool is_dialog,
                                               const GHOST_IWindow *parentWindow)
{
  GHOST_WindowWin32 *window = new GHOST_WindowWin32(
      this,
      title,
      left,
      top,
      width,
      height,
      state,
      gpuSettings.context_type,
      ((gpuSettings.flags & GHOST_gpuStereoVisual) != 0),
      false,
      (GHOST_WindowWin32 *)parentWindow,
      ((gpuSettings.flags & GHOST_gpuDebugContext) != 0),
      is_dialog);

  if (window->getValid()) {
    /* Store the pointer to the window */
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
 * Create a new off-screen context.
 * Never explicitly delete the window, use #disposeContext() instead.
 * \return The new context (or 0 if creation failed).
 */
GHOST_IContext *GHOST_SystemWin32::createOffscreenContext(GHOST_GPUSettings gpuSettings)
{
  const bool debug_context = (gpuSettings.flags & GHOST_gpuDebugContext) != 0;

  GHOST_Context *context = nullptr;

#ifdef WITH_VULKAN_BACKEND
  /* Vulkan does not need a window. */
  if (gpuSettings.context_type == GHOST_kDrawingContextTypeVulkan) {
    context = new GHOST_ContextVK(false, (HWND)0, 1, 2, debug_context);

    if (!context->initializeDrawingContext()) {
      delete context;
      return nullptr;
    }
    return context;
  }
#endif

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
    delete context;
    return NULL;
  }

finished:
  wglMakeCurrent(prev_hdc, prev_context);
  return context;
}

/**
 * Dispose of a context.
 * \param context: Pointer to the context to be disposed.
 * \return Indication of success.
 */
GHOST_TSuccess GHOST_SystemWin32::disposeContext(GHOST_IContext *context)
{
  delete context;

  return GHOST_kSuccess;
}

/**
 * Create a new off-screen DirectX 11 context.
 * Never explicitly delete the window, use #disposeContext() instead.
 * \return The new context (or 0 if creation failed).
 */
GHOST_ContextD3D *GHOST_SystemWin32::createOffscreenContextD3D()
{
  GHOST_ContextD3D *context;

  HWND wnd = CreateWindowA("STATIC",
                           "Blender XR",
                           WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                           0,
                           0,
                           64,
                           64,
                           NULL,
                           NULL,
                           GetModuleHandle(NULL),
                           NULL);

  context = new GHOST_ContextD3D(false, wnd);
  if (context->initializeDrawingContext() == GHOST_kFailure) {
    delete context;
    context = nullptr;
  }

  return context;
}

GHOST_TSuccess GHOST_SystemWin32::disposeContextD3D(GHOST_ContextD3D *context)
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
      uint64_t next = timerMgr->nextFireTime();
      int64_t maxSleep = next - getMilliSeconds();

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

    driveTrackpad();

    /* Process all the events waiting for us. */
    while (::PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
      /* #TranslateMessage doesn't alter the message, and doesn't change our raw keyboard data.
       * Needed for #MapVirtualKey or if we ever need to get chars from wm_ime_char or similar. */
      ::TranslateMessage(&msg);
      ::DispatchMessageW(&msg);
      hasEventHandled = true;
    }

    processTrackpad();

    /* `PeekMessage` above is allowed to dispatch messages to the `wndproc` without us
     * noticing, so we need to check the event manager here to see if there are
     * events waiting in the queue. */
    hasEventHandled |= this->m_eventManager->getNumEvents() > 0;

  } while (waitForEvent && !hasEventHandled);

  return hasEventHandled;
}

GHOST_TSuccess GHOST_SystemWin32::getCursorPosition(int32_t &x, int32_t &y) const
{
  POINT point;
  if (::GetCursorPos(&point)) {
    x = point.x;
    y = point.y;
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemWin32::setCursorPosition(int32_t x, int32_t y)
{
  if (!::GetActiveWindow()) {
    return GHOST_kFailure;
  }
  return ::SetCursorPos(x, y) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_SystemWin32::getModifierKeys(GHOST_ModifierKeys &keys) const
{
  /* `GetAsyncKeyState` returns the current interrupt-level state of the hardware, which is needed
   * when passing key states to a newly-activated window - #40059. Alternative `GetKeyState` only
   * returns the state as processed by the thread's message queue. */
  bool down = HIBYTE(::GetAsyncKeyState(VK_LSHIFT)) != 0;
  keys.set(GHOST_kModifierKeyLeftShift, down);
  down = HIBYTE(::GetAsyncKeyState(VK_RSHIFT)) != 0;
  keys.set(GHOST_kModifierKeyRightShift, down);

  down = HIBYTE(::GetAsyncKeyState(VK_LMENU)) != 0;
  keys.set(GHOST_kModifierKeyLeftAlt, down);
  down = HIBYTE(::GetAsyncKeyState(VK_RMENU)) != 0;
  keys.set(GHOST_kModifierKeyRightAlt, down);

  down = HIBYTE(::GetAsyncKeyState(VK_LCONTROL)) != 0;
  keys.set(GHOST_kModifierKeyLeftControl, down);
  down = HIBYTE(::GetAsyncKeyState(VK_RCONTROL)) != 0;
  keys.set(GHOST_kModifierKeyRightControl, down);

  down = HIBYTE(::GetAsyncKeyState(VK_LWIN)) != 0;
  keys.set(GHOST_kModifierKeyLeftOS, down);
  down = HIBYTE(::GetAsyncKeyState(VK_RWIN)) != 0;
  keys.set(GHOST_kModifierKeyRightOS, down);

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_SystemWin32::getButtons(GHOST_Buttons &buttons) const
{
  /* Check for swapped buttons (left-handed mouse buttons)
   * GetAsyncKeyState() will give back the state of the physical mouse buttons.
   */
  bool swapped = ::GetSystemMetrics(SM_SWAPBUTTON) == TRUE;

  bool down = HIBYTE(::GetAsyncKeyState(VK_LBUTTON)) != 0;
  buttons.set(swapped ? GHOST_kButtonMaskRight : GHOST_kButtonMaskLeft, down);

  down = HIBYTE(::GetAsyncKeyState(VK_MBUTTON)) != 0;
  buttons.set(GHOST_kButtonMaskMiddle, down);

  down = HIBYTE(::GetAsyncKeyState(VK_RBUTTON)) != 0;
  buttons.set(swapped ? GHOST_kButtonMaskLeft : GHOST_kButtonMaskRight, down);
  return GHOST_kSuccess;
}

GHOST_TCapabilityFlag GHOST_SystemWin32::getCapabilities() const
{
  return GHOST_TCapabilityFlag(GHOST_CAPABILITY_FLAG_ALL &
                               ~(
                                   /* WIN32 has no support for a primary selection clipboard. */
                                   GHOST_kCapabilityPrimaryClipboard));
}

GHOST_TSuccess GHOST_SystemWin32::init()
{
  GHOST_TSuccess success = GHOST_System::init();
  InitCommonControls();

  /* Disable scaling on high DPI displays on Vista */
  SetProcessDPIAware();
  initRawInput();

  m_lfstart = ::GetTickCount();
  /* Determine whether this system has a high frequency performance counter. */
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

    /* Use #RegisterClassEx for setting small icon. */
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

GHOST_TKey GHOST_SystemWin32::hardKey(RAWINPUT const &raw, bool *r_key_down)
{
  /* #RI_KEY_BREAK doesn't work for sticky keys release, so we also check for the up message. */
  uint msg = raw.data.keyboard.Message;
  *r_key_down = !(raw.data.keyboard.Flags & RI_KEY_BREAK) && msg != WM_KEYUP && msg != WM_SYSKEYUP;

  return this->convertKey(raw.data.keyboard.VKey,
                          raw.data.keyboard.MakeCode,
                          (raw.data.keyboard.Flags & (RI_KEY_E1 | RI_KEY_E0)));
}

/**
 * \note this function can be extended to include other exotic cases as they arise.
 *
 * This function was added in response to bug #25715.
 * This is going to be a long list #42426.
 */
GHOST_TKey GHOST_SystemWin32::processSpecialKey(short vKey, short /*scanCode*/) const
{
  GHOST_TKey key = GHOST_kKeyUnknown;
  if (vKey == 0xFF) {
    /* 0xFF is not a valid virtual key code. */
    return key;
  }

  char ch = char(MapVirtualKeyA(vKey, MAPVK_VK_TO_CHAR));
  switch (ch) {
    case u'\"':
    case u'\'':
      key = GHOST_kKeyQuote;
      break;
    case u'.':
      key = GHOST_kKeyNumpadPeriod;
      break;
    case u'/':
      key = GHOST_kKeySlash;
      break;
    case u'`':
    case u'²':
      key = GHOST_kKeyAccentGrave;
      break;
    default:
      if (vKey == VK_OEM_7) {
        key = GHOST_kKeyQuote;
      }
      else if (vKey == VK_OEM_8) {
        if (PRIMARYLANGID(m_langId) == LANG_FRENCH) {
          /* OEM key; used purely for shortcuts. */
          key = GHOST_kKeyF13;
        }
      }
      break;
  }

  return key;
}

GHOST_TKey GHOST_SystemWin32::convertKey(short vKey, short scanCode, short extend) const
{
  GHOST_TKey key;

  if ((vKey >= '0') && (vKey <= '9')) {
    /* VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39). */
    key = (GHOST_TKey)(vKey - '0' + GHOST_kKey0);
  }
  else if ((vKey >= 'A') && (vKey <= 'Z')) {
    /* VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A). */
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
        key = GHOST_kKeyLeftOS;
        break;
      case VK_RWIN:
        key = GHOST_kKeyRightOS;
        break;
      case VK_APPS:
        key = GHOST_kKeyApp;
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
      case VK_OEM_7:
      case VK_OEM_8:
      default:
        key = ((GHOST_SystemWin32 *)getSystem())->processSpecialKey(vKey, scanCode);
        break;
    }
  }

  return key;
}

GHOST_EventButton *GHOST_SystemWin32::processButtonEvent(GHOST_TEventType type,
                                                         GHOST_WindowWin32 *window,
                                                         GHOST_TButton mask)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  GHOST_TabletData td = window->getTabletData();

  /* Move mouse to button event position. */
  if (window->getTabletData().Active != GHOST_kTabletModeNone) {
    /* Tablet should be handling in between mouse moves, only move to event position. */
    DWORD msgPos = ::GetMessagePos();
    int msgPosX = GET_X_LPARAM(msgPos);
    int msgPosY = GET_Y_LPARAM(msgPos);
    system->pushEvent(new GHOST_EventCursor(
        ::GetMessageTime(), GHOST_kEventCursorMove, window, msgPosX, msgPosY, td));

    if (type == GHOST_kEventButtonDown) {
      WINTAB_PRINTF("HWND %p OS button down\n", window->getHWND());
    }
    else if (type == GHOST_kEventButtonUp) {
      WINTAB_PRINTF("HWND %p OS button up\n", window->getHWND());
    }
  }

  window->updateMouseCapture(type == GHOST_kEventButtonDown ? MousePressed : MouseReleased);
  return new GHOST_EventButton(system->getMilliSeconds(), type, window, mask, td);
}

void GHOST_SystemWin32::processWintabEvent(GHOST_WindowWin32 *window)
{
  GHOST_Wintab *wt = window->getWintab();
  if (!wt) {
    return;
  }

  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  std::vector<GHOST_WintabInfoWin32> wintabInfo;
  wt->getInput(wintabInfo);

  /* Wintab provided coordinates are untrusted until a Wintab and Win32 button down event match.
   * This is checked on every button down event, and revoked if there is a mismatch. This can
   * happen when Wintab incorrectly scales cursor position or is in mouse mode.
   *
   * If Wintab was never trusted while processing this Win32 event, a fallback Ghost cursor move
   * event is created at the position of the Win32 WT_PACKET event. */
  bool mouseMoveHandled;
  bool useWintabPos;
  mouseMoveHandled = useWintabPos = wt->trustCoordinates();

  for (GHOST_WintabInfoWin32 &info : wintabInfo) {
    switch (info.type) {
      case GHOST_kEventCursorMove: {
        if (!useWintabPos) {
          continue;
        }

        wt->mapWintabToSysCoordinates(info.x, info.y, info.x, info.y);
        system->pushEvent(new GHOST_EventCursor(
            info.time, GHOST_kEventCursorMove, window, info.x, info.y, info.tabletData));

        break;
      }
      case GHOST_kEventButtonDown: {
        WINTAB_PRINTF("HWND %p Wintab button down", window->getHWND());

        uint message;
        switch (info.button) {
          case GHOST_kButtonMaskLeft:
            message = WM_LBUTTONDOWN;
            break;
          case GHOST_kButtonMaskRight:
            message = WM_RBUTTONDOWN;
            break;
          case GHOST_kButtonMaskMiddle:
            message = WM_MBUTTONDOWN;
            break;
          default:
            continue;
        }

        /* Wintab buttons are modal, but the API does not inform us what mode a pressed button is
         * in. Only issue button events if we can steal an equivalent Win32 button event from the
         * event queue. */
        MSG msg;
        if (PeekMessage(&msg, window->getHWND(), message, message, PM_NOYIELD) &&
            msg.message != WM_QUIT) {

          /* Test for Win32/Wintab button down match. */
          useWintabPos = wt->testCoordinates(msg.pt.x, msg.pt.y, info.x, info.y);
          if (!useWintabPos) {
            WINTAB_PRINTF(" ... but associated system button mismatched position\n");
            continue;
          }

          WINTAB_PRINTF(" ... associated to system button\n");

          /* Steal the Win32 event which was previously peeked. */
          PeekMessage(&msg, window->getHWND(), message, message, PM_REMOVE | PM_NOYIELD);

          /* Move cursor to button location, to prevent incorrect cursor position when
           * transitioning from unsynchronized Win32 to Wintab cursor control. */
          wt->mapWintabToSysCoordinates(info.x, info.y, info.x, info.y);
          system->pushEvent(new GHOST_EventCursor(
              info.time, GHOST_kEventCursorMove, window, info.x, info.y, info.tabletData));

          window->updateMouseCapture(MousePressed);
          system->pushEvent(
              new GHOST_EventButton(info.time, info.type, window, info.button, info.tabletData));

          mouseMoveHandled = true;
        }
        else {
          WINTAB_PRINTF(" ... but no system button\n");
        }
        break;
      }
      case GHOST_kEventButtonUp: {
        WINTAB_PRINTF("HWND %p Wintab button up", window->getHWND());
        if (!useWintabPos) {
          WINTAB_PRINTF(" ... but Wintab position isn't trusted\n");
          continue;
        }

        uint message;
        switch (info.button) {
          case GHOST_kButtonMaskLeft:
            message = WM_LBUTTONUP;
            break;
          case GHOST_kButtonMaskRight:
            message = WM_RBUTTONUP;
            break;
          case GHOST_kButtonMaskMiddle:
            message = WM_MBUTTONUP;
            break;
          default:
            continue;
        }

        /* Wintab buttons are modal, but the API does not inform us what mode a pressed button is
         * in. Only issue button events if we can steal an equivalent Win32 button event from the
         * event queue. */
        MSG msg;
        if (PeekMessage(&msg, window->getHWND(), message, message, PM_REMOVE | PM_NOYIELD) &&
            msg.message != WM_QUIT)
        {

          WINTAB_PRINTF(" ... associated to system button\n");
          window->updateMouseCapture(MouseReleased);
          system->pushEvent(
              new GHOST_EventButton(info.time, info.type, window, info.button, info.tabletData));
        }
        else {
          WINTAB_PRINTF(" ... but no system button\n");
        }
        break;
      }
      default:
        break;
    }
  }

  /* Fallback cursor movement if Wintab position were never trusted while processing this event. */
  if (!mouseMoveHandled) {
    DWORD pos = GetMessagePos();
    int x = GET_X_LPARAM(pos);
    int y = GET_Y_LPARAM(pos);
    GHOST_TabletData td = wt->getLastTabletData();

    system->pushEvent(new GHOST_EventCursor(
        system->getMilliSeconds(), GHOST_kEventCursorMove, window, x, y, td));
  }
}

void GHOST_SystemWin32::processPointerEvent(
    uint type, GHOST_WindowWin32 *window, WPARAM wParam, LPARAM lParam, bool &eventHandled)
{
  /* Pointer events might fire when changing windows for a device which is set to use Wintab,
   * even when Wintab is left enabled but set to the bottom of Wintab overlap order. */
  if (!window->usingTabletAPI(GHOST_kTabletWinPointer)) {
    return;
  }

  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  std::vector<GHOST_PointerInfoWin32> pointerInfo;

  if (window->getPointerInfo(pointerInfo, wParam, lParam) != GHOST_kSuccess) {
    return;
  }

  switch (type) {
    case WM_POINTERUPDATE: {
      /* Coalesced pointer events are reverse chronological order, reorder chronologically.
       * Only contiguous move events are coalesced. */
      for (uint32_t i = pointerInfo.size(); i-- > 0;) {
        system->pushEvent(new GHOST_EventCursor(pointerInfo[i].time,
                                                GHOST_kEventCursorMove,
                                                window,
                                                pointerInfo[i].pixelLocation.x,
                                                pointerInfo[i].pixelLocation.y,
                                                pointerInfo[i].tabletData));
      }

      /* Leave event unhandled so that system cursor is moved. */

      break;
    }
    case WM_POINTERDOWN: {
      /* Move cursor to point of contact because GHOST_EventButton does not include position. */
      system->pushEvent(new GHOST_EventCursor(pointerInfo[0].time,
                                              GHOST_kEventCursorMove,
                                              window,
                                              pointerInfo[0].pixelLocation.x,
                                              pointerInfo[0].pixelLocation.y,
                                              pointerInfo[0].tabletData));
      system->pushEvent(new GHOST_EventButton(pointerInfo[0].time,
                                              GHOST_kEventButtonDown,
                                              window,
                                              pointerInfo[0].buttonMask,
                                              pointerInfo[0].tabletData));
      window->updateMouseCapture(MousePressed);

      /* Mark event handled so that mouse button events are not generated. */
      eventHandled = true;

      break;
    }
    case WM_POINTERUP: {
      system->pushEvent(new GHOST_EventButton(pointerInfo[0].time,
                                              GHOST_kEventButtonUp,
                                              window,
                                              pointerInfo[0].buttonMask,
                                              pointerInfo[0].tabletData));
      window->updateMouseCapture(MouseReleased);

      /* Mark event handled so that mouse button events are not generated. */
      eventHandled = true;

      break;
    }
    default: {
      break;
    }
  }
}

GHOST_EventCursor *GHOST_SystemWin32::processCursorEvent(GHOST_WindowWin32 *window,
                                                         const int32_t screen_co[2])
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  if (window->getTabletData().Active != GHOST_kTabletModeNone) {
    /* While pen devices are in range, cursor movement is handled by tablet input processing. */
    return NULL;
  }

  int32_t x_screen = screen_co[0], y_screen = screen_co[1];
  if (window->getCursorGrabModeIsWarp()) {
    /* WORKAROUND:
     * Sometimes Windows ignores `SetCursorPos()` or `SendInput()` calls or the mouse event is
     * outdated. Identify these cases by checking if the cursor is not yet within bounds. */
    static bool is_warping_x = false;
    static bool is_warping_y = false;

    int32_t x_new = x_screen;
    int32_t y_new = y_screen;
    int32_t x_accum, y_accum;

    /* Warp within bounds. */
    {
      GHOST_Rect bounds;
      int32_t bounds_margin = 0;
      GHOST_TAxisFlag bounds_axis = GHOST_kAxisNone;

      if (window->getCursorGrabMode() == GHOST_kGrabHide) {
        window->getClientBounds(bounds);

        /* WARNING(@ideasman42): The current warping logic fails to warp on every event,
         * so the box needs to small enough not to let the cursor escape the window but large
         * enough that the cursor isn't being warped every time.
         * If this was not the case it would be less trouble to simply warp the cursor to the
         * center of the screen on every motion, see: D16558 (alternative fix for #102346). */
        const int32_t subregion_div = 4; /* One quarter of the region. */
        const int32_t size[2] = {bounds.getWidth(), bounds.getHeight()};
        const int32_t center[2] = {(bounds.m_l + bounds.m_r) / 2, (bounds.m_t + bounds.m_b) / 2};
        /* Shrink the box to prevent the cursor escaping. */
        bounds.m_l = center[0] - (size[0] / (subregion_div * 2));
        bounds.m_r = center[0] + (size[0] / (subregion_div * 2));
        bounds.m_t = center[1] - (size[1] / (subregion_div * 2));
        bounds.m_b = center[1] + (size[1] / (subregion_div * 2));
        bounds_margin = 0;
        bounds_axis = GHOST_TAxisFlag(GHOST_kAxisX | GHOST_kAxisY);
      }
      else {
        /* Fallback to window bounds. */
        if (window->getCursorGrabBounds(bounds) == GHOST_kFailure) {
          window->getClientBounds(bounds);
        }
        bounds_margin = 2;
        bounds_axis = window->getCursorGrabAxis();
      }

      /* Could also clamp to screen bounds wrap with a window outside the view will
       * fail at the moment. Use inset in case the window is at screen bounds. */
      bounds.wrapPoint(x_new, y_new, bounds_margin, bounds_axis);
    }

    window->getCursorGrabAccum(x_accum, y_accum);
    if (x_new != x_screen || y_new != y_screen) {
      system->setCursorPosition(x_new, y_new); /* wrap */

      /* Do not update the accum values if we are an outdated or failed pos-warp event. */
      if (!is_warping_x) {
        is_warping_x = x_new != x_screen;
        if (is_warping_x) {
          x_accum += (x_screen - x_new);
        }
      }

      if (!is_warping_y) {
        is_warping_y = y_new != y_screen;
        if (is_warping_y) {
          y_accum += (y_screen - y_new);
        }
      }
      window->setCursorGrabAccum(x_accum, y_accum);

      /* When wrapping we don't need to add an event because the setCursorPosition call will cause
       * a new event after. */
      return NULL;
    }

    is_warping_x = false;
    is_warping_y = false;
    x_screen += x_accum;
    y_screen += y_accum;
  }

  return new GHOST_EventCursor(system->getMilliSeconds(),
                               GHOST_kEventCursorMove,
                               window,
                               x_screen,
                               y_screen,
                               GHOST_TABLET_DATA_NONE);
}

void GHOST_SystemWin32::processWheelEvent(GHOST_WindowWin32 *window,
                                          WPARAM wParam,
                                          LPARAM /*lParam*/)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  int acc = system->m_wheelDeltaAccum;
  int delta = GET_WHEEL_DELTA_WPARAM(wParam);

  if (acc * delta < 0) {
    /* Scroll direction reversed. */
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
  const char vk = raw.data.keyboard.VKey;
  bool key_down = false;
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  GHOST_TKey key = system->hardKey(raw, &key_down);
  GHOST_EventKey *event;

  /* NOTE(@ideasman42): key repeat in WIN32 also applies to modifier-keys.
   * Check for this case and filter out modifier-repeat.
   * Typically keyboard events are *not* filtered as part of GHOST's event handling.
   * As other GHOST back-ends don't have the behavior, it's simplest not to send them through.
   * Ideally it would be possible to check the key-map for keys that repeat but this doesn't look
   * to be supported. */
  bool is_repeat = false;
  bool is_repeated_modifier = false;
  if (key_down) {
    if (HIBYTE(::GetKeyState(vk)) != 0) {
      /* This thread's message queue shows this key as already down. */
      is_repeat = true;
      is_repeated_modifier = GHOST_KEY_MODIFIER_CHECK(key);
    }
  }

  /* We used to check `if (key != GHOST_kKeyUnknown)`, but since the message
   * values `WM_SYSKEYUP`, `WM_KEYUP` and `WM_CHAR` are ignored, we capture
   * those events here as well. */
  if (!is_repeated_modifier) {
    char utf8_char[6] = {0};
    BYTE state[256];
    const BOOL has_state = GetKeyboardState((PBYTE)state);
    const bool ctrl_pressed = has_state && state[VK_CONTROL] & 0x80;
    const bool alt_pressed = has_state && state[VK_MENU] & 0x80;

    /* We can be here with !key_down if processing dead keys (diacritics). See #103119. */

    /* No text with control key pressed (Alt can be used to insert special characters though!). */
    if (ctrl_pressed && !alt_pressed) {
      /* Pass. */
    }
    /* Don't call #ToUnicodeEx on dead keys as it clears the buffer and so won't allow diacritical
     * composition. XXX: we are not checking return of MapVirtualKeyW for high bit set, which is
     * what is supposed to indicate dead keys. But this is working now so approach cautiously. */
    else if (MapVirtualKeyW(vk, MAPVK_VK_TO_CHAR) != 0) {
      wchar_t utf16[3] = {0};
      int r;
      /* TODO: #ToUnicodeEx can respond with up to 4 utf16 chars (only 2 here).
       * Could be up to 24 utf8 bytes. */
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
      if (!key_down) {
        /* Clear or wm_event_add_ghostevent will warn of unexpected data on key up. */
        utf8_char[0] = '\0';
      }
    }

#ifdef WITH_INPUT_IME
    if (key_down && ((utf8_char[0] & 0x80) == 0)) {
      const char ascii = utf8_char[0];
      if (window->getImeInput()->IsImeKeyEvent(ascii, key)) {
        return NULL;
      }
    }
#endif /* WITH_INPUT_IME */

    event = new GHOST_EventKey(system->getMilliSeconds(),
                               key_down ? GHOST_kEventKeyDown : GHOST_kEventKeyUp,
                               window,
                               key,
                               is_repeat,
                               utf8_char);

#if 0 /* we already get this info via EventPrinter. */
    GHOST_PRINTF("%c\n", ascii);
#endif
  }
  else {
    event = NULL;
  }

  return event;
}

GHOST_Event *GHOST_SystemWin32::processWindowSizeEvent(GHOST_WindowWin32 *window)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
  GHOST_Event *sizeEvent = new GHOST_Event(
      system->getMilliSeconds(), GHOST_kEventWindowSize, window);

  /* We get WM_SIZE before we fully init. Do not dispatch before we are continuously resizing. */
  if (window->m_inLiveResize) {
    system->pushEvent(sizeEvent);
    system->dispatchEvents();
    return NULL;
  }
  return sizeEvent;
}

GHOST_Event *GHOST_SystemWin32::processWindowEvent(GHOST_TEventType type,
                                                   GHOST_WindowWin32 *window)
{
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  if (type == GHOST_kEventWindowActivate) {
    system->getWindowManager()->setActiveWindow(window);
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

void GHOST_SystemWin32::setTabletAPI(GHOST_TTabletAPI api)
{
  GHOST_System::setTabletAPI(api);

  /* If API is set to WinPointer (Windows Ink), unload Wintab so that trouble drivers don't disable
   * Windows Ink. Load Wintab when API is Automatic because decision logic relies on knowing
   * whether a Wintab device is present. */
  const bool loadWintab = GHOST_kTabletWinPointer != api;
  GHOST_WindowManager *wm = getWindowManager();

  for (GHOST_IWindow *win : wm->getWindows()) {
    GHOST_WindowWin32 *windowWin32 = (GHOST_WindowWin32 *)win;
    if (loadWintab) {
      windowWin32->loadWintab(GHOST_kWindowStateMinimized != windowWin32->getState());

      if (windowWin32->usingTabletAPI(GHOST_kTabletWintab)) {
        windowWin32->resetPointerPenInfo();
      }
    }
    else {
      windowWin32->closeWintab();
    }
  }
}

void GHOST_SystemWin32::initDebug(GHOST_Debug debug)
{
  GHOST_System::initDebug(debug);
  GHOST_Wintab::setDebug(debug.flags & GHOST_kDebugWintab);
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
  uint64_t now = getMilliSeconds();

  static bool firstEvent = true;
  if (firstEvent) { /* Determine exactly which device is plugged in. */
    RID_DEVICE_INFO info;
    unsigned infoSize = sizeof(RID_DEVICE_INFO);
    info.cbSize = infoSize;

    GetRawInputDeviceInfo(raw.header.hDevice, RIDI_DEVICEINFO, &info, &infoSize);
    if (info.dwType == RIM_TYPEHID) {
      m_ndofManager->setDevice(info.hid.dwVendorId, info.hid.dwProductId);
    }
    else {
      GHOST_PRINT("<!> not a HID device... mouse/kb perhaps?\n");
    }
    firstEvent = false;
  }

  /* The NDOF manager sends button changes immediately, and *pretends* to
   * send motion. Mark as 'sent' so motion will always get dispatched. */
  eventSent = true;

  BYTE const *data = raw.data.hid.bRawData;

  BYTE packetType = data[0];
  switch (packetType) {
    case 1: { /* Translation. */
      const short *axis = (short *)(data + 1);
      /* Massage into blender view coords (same goes for rotation). */
      const int t[3] = {axis[0], -axis[2], axis[1]};
      m_ndofManager->updateTranslation(t, now);

      if (raw.data.hid.dwSizeHid == 13) {
        /* This report also includes rotation. */
        const int r[3] = {-axis[3], axis[5], -axis[4]};
        m_ndofManager->updateRotation(r, now);

        /* I've never gotten one of these, has anyone else? */
        GHOST_PRINT("ndof: combined T + R\n");
      }
      break;
    }
    case 2: { /* Rotation. */

      const short *axis = (short *)(data + 1);
      const int r[3] = {-axis[0], axis[2], -axis[1]};
      m_ndofManager->updateRotation(r, now);
      break;
    }
    case 3: { /* Buttons. */
      int button_bits;
      memcpy(&button_bits, data + 1, sizeof(button_bits));
      m_ndofManager->updateButtons(button_bits, now);
      break;
    }
  }
  return eventSent;
}
#endif /* WITH_INPUT_NDOF */

void GHOST_SystemWin32::driveTrackpad()
{
  GHOST_WindowWin32 *active_window = static_cast<GHOST_WindowWin32 *>(
      getWindowManager()->getActiveWindow());
  if (active_window) {
    active_window->updateDirectManipulation();
  }
}

void GHOST_SystemWin32::processTrackpad()
{
  GHOST_WindowWin32 *active_window = static_cast<GHOST_WindowWin32 *>(
      getWindowManager()->getActiveWindow());

  if (!active_window) {
    return;
  }

  GHOST_TTrackpadInfo trackpad_info = active_window->getTrackpadInfo();
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();

  int32_t cursor_x, cursor_y;
  system->getCursorPosition(cursor_x, cursor_y);

  if (trackpad_info.x != 0 || trackpad_info.y != 0) {
    system->pushEvent(new GHOST_EventTrackpad(system->getMilliSeconds(),
                                              active_window,
                                              GHOST_kTrackpadEventScroll,
                                              cursor_x,
                                              cursor_y,
                                              trackpad_info.x,
                                              trackpad_info.y,
                                              trackpad_info.isScrollDirectionInverted));
  }
  if (trackpad_info.scale != 0) {
    system->pushEvent(new GHOST_EventTrackpad(system->getMilliSeconds(),
                                              active_window,
                                              GHOST_kTrackpadEventMagnify,
                                              cursor_x,
                                              cursor_y,
                                              trackpad_info.scale,
                                              0,
                                              false));
  }
}

LRESULT WINAPI GHOST_SystemWin32::s_wndProc(HWND hwnd, uint msg, WPARAM wParam, LPARAM lParam)
{
  GHOST_Event *event = NULL;
  bool eventHandled = false;

  LRESULT lResult = 0;
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)getSystem();
#ifdef WITH_INPUT_IME
  GHOST_EventManager *eventManager = system->getEventManager();
#endif
  GHOST_ASSERT(system, "GHOST_SystemWin32::s_wndProc(): system not initialized");

  if (hwnd) {

    if (msg == WM_NCCREATE) {
      /* Tell Windows to automatically handle scaling of non-client areas
       * such as the caption bar. #EnableNonClientDpiScaling was introduced in Windows 10. */
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

    GHOST_WindowWin32 *window = (GHOST_WindowWin32 *)::GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (window) {
      switch (msg) {
        /* We need to check if new key layout has AltGr. */
        case WM_INPUTLANGCHANGE: {
          system->handleKeyboardChange();
#ifdef WITH_INPUT_IME
          window->getImeInput()->UpdateInputLanguage();
          window->getImeInput()->UpdateConversionStatus(hwnd);
#endif
          break;
        }
        /* ==========================
         * Keyboard events, processed
         * ========================== */
        case WM_INPUT: {
          RAWINPUT raw;
          RAWINPUT *raw_ptr = &raw;
          uint rawSize = sizeof(RAWINPUT);

          GetRawInputData((HRAWINPUT)lParam, RID_INPUT, raw_ptr, &rawSize, sizeof(RAWINPUTHEADER));

          switch (raw.header.dwType) {
            case RIM_TYPEKEYBOARD: {
              event = processKeyEvent(window, raw);
              if (!event) {
                GHOST_PRINT("GHOST_SystemWin32::wndProc: key event ");
                GHOST_PRINT(msg);
                GHOST_PRINT(" key ignored\n");
              }
              break;
            }
#ifdef WITH_INPUT_NDOF
            case RIM_TYPEHID: {
              if (system->processNDOF(raw)) {
                eventHandled = true;
              }
              break;
            }
#endif
          }
          break;
        }
#ifdef WITH_INPUT_IME
        /* =================================================
         * IME events, processed, read more in `GHOST_IME.h`
         * ================================================= */
        case WM_IME_NOTIFY: {
          /* Update conversion status when IME is changed or input mode is changed. */
          if (wParam == IMN_SETOPENSTATUS || wParam == IMN_SETCONVERSIONMODE) {
            window->getImeInput()->UpdateConversionStatus(hwnd);
          }
          break;
        }
        case WM_IME_SETCONTEXT: {
          GHOST_ImeWin32 *ime = window->getImeInput();
          ime->UpdateInputLanguage();
          ime->UpdateConversionStatus(hwnd);
          ime->CreateImeWindow(hwnd);
          ime->CleanupComposition(hwnd);
          ime->CheckFirst(hwnd);
          break;
        }
        case WM_IME_STARTCOMPOSITION: {
          GHOST_ImeWin32 *ime = window->getImeInput();
          eventHandled = true;
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
        /* ========================
         * Keyboard events, ignored
         * ======================== */
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
          /* These functions were replaced by #WM_INPUT. */
        case WM_CHAR:
          /* The #WM_CHAR message is posted to the window with the keyboard focus when
           * a WM_KEYDOWN message is translated by the #TranslateMessage function.
           * WM_CHAR contains the character code of the key that was pressed. */
        case WM_DEADCHAR:
          /* The #WM_DEADCHAR message is posted to the window with the keyboard focus when a
           * WM_KEYUP message is translated by the #TranslateMessage function. WM_DEADCHAR
           * specifies a character code generated by a dead key. A dead key is a key that
           * generates a character, such as the umlaut (double-dot), that is combined with
           * another character to form a composite character. For example, the umlaut-O
           * character (Ö) is generated by typing the dead key for the umlaut character, and
           * then typing the O key. */
          break;
        case WM_SYSDEADCHAR:
          /* The #WM_SYSDEADCHAR message is sent to the window with the keyboard focus when
           * a WM_SYSKEYDOWN message is translated by the #TranslateMessage function.
           * WM_SYSDEADCHAR specifies the character code of a system dead key - that is,
           * a dead key that is pressed while holding down the alt key. */
        case WM_SYSCHAR: {
          /* #The WM_SYSCHAR message is sent to the window with the keyboard focus when
           * a WM_SYSCHAR message is translated by the #TranslateMessage function.
           * WM_SYSCHAR specifies the character code of a dead key - that is,
           * a dead key that is pressed while holding down the alt key.
           * To prevent the sound, #DefWindowProc must be avoided by return. */
          break;
        }
        case WM_SYSCOMMAND: {
          /* The #WM_SYSCOMMAND message is sent to the window when system commands such as
           * maximize, minimize  or close the window are triggered. Also it is sent when ALT
           * button is press for menu. To prevent this we must return preventing #DefWindowProc.
           *
           * Note that the four low-order bits of the wParam parameter are used internally by the
           * OS. To obtain the correct result when testing the value of wParam, an application must
           * combine the value 0xFFF0 with the wParam value by using the bit-wise AND operator. */
          switch (wParam & 0xFFF0) {
            case SC_KEYMENU: {
              eventHandled = true;
              break;
            }
            case SC_RESTORE: {
              ::ShowWindow(hwnd, SW_RESTORE);
              window->setState(window->getState());

              GHOST_Wintab *wt = window->getWintab();
              if (wt) {
                wt->enable();
              }

              eventHandled = true;
              break;
            }
            case SC_MAXIMIZE: {
              GHOST_Wintab *wt = window->getWintab();
              if (wt) {
                wt->enable();
              }
              /* Don't report event as handled so that default handling occurs. */
              break;
            }
            case SC_MINIMIZE: {
              GHOST_Wintab *wt = window->getWintab();
              if (wt) {
                wt->disable();
              }
              /* Don't report event as handled so that default handling occurs. */
              break;
            }
          }
          break;
        }
        /* ========================
         * Wintab events, processed
         * ======================== */
        case WT_CSRCHANGE: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_CSRCHANGE\n", window->getHWND(), (void *)lParam);
          GHOST_Wintab *wt = window->getWintab();
          if (wt) {
            wt->updateCursorInfo();
          }
          eventHandled = true;
          break;
        }
        case WT_PROXIMITY: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_PROXIMITY\n", window->getHWND(), (void *)wParam);
          if (LOWORD(lParam)) {
            WINTAB_PRINTF(" Cursor entering context.\n");
          }
          else {
            WINTAB_PRINTF(" Cursor leaving context.\n");
          }
          if (HIWORD(lParam)) {
            WINTAB_PRINTF(" Cursor entering or leaving hardware proximity.\n");
          }
          else {
            WINTAB_PRINTF(" Cursor neither entering nor leaving hardware proximity.\n");
          }

          GHOST_Wintab *wt = window->getWintab();
          if (wt) {
            bool inRange = LOWORD(lParam);
            if (inRange) {
              /* Some devices don't emit WT_CSRCHANGE events, so update cursor info here. */
              wt->updateCursorInfo();
            }
            else {
              wt->leaveRange();
            }
          }
          eventHandled = true;
          break;
        }
        case WT_INFOCHANGE: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_INFOCHANGE\n", window->getHWND(), (void *)wParam);
          GHOST_Wintab *wt = window->getWintab();
          if (wt) {
            wt->processInfoChange(lParam);

            if (window->usingTabletAPI(GHOST_kTabletWintab)) {
              window->resetPointerPenInfo();
            }
          }
          eventHandled = true;
          break;
        }
        case WT_PACKET: {
          processWintabEvent(window);
          eventHandled = true;
          break;
        }
        /* ====================
         * Wintab events, debug
         * ==================== */
        case WT_CTXOPEN: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_CTXOPEN\n", window->getHWND(), (void *)wParam);
          break;
        }
        case WT_CTXCLOSE: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_CTXCLOSE\n", window->getHWND(), (void *)wParam);
          break;
        }
        case WT_CTXUPDATE: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_CTXUPDATE\n", window->getHWND(), (void *)wParam);
          break;
        }
        case WT_CTXOVERLAP: {
          WINTAB_PRINTF("HWND %p HCTX %p WT_CTXOVERLAP", window->getHWND(), (void *)wParam);
          switch (lParam) {
            case CXS_DISABLED: {
              WINTAB_PRINTF(" CXS_DISABLED\n");
              break;
            }
            case CXS_OBSCURED: {
              WINTAB_PRINTF(" CXS_OBSCURED\n");
              break;
            }
            case CXS_ONTOP: {
              WINTAB_PRINTF(" CXS_ONTOP\n");
              break;
            }
          }
          break;
        }
        /* =========================
         * Pointer events, processed
         * ========================= */
        case WM_POINTERUPDATE:
        case WM_POINTERDOWN:
        case WM_POINTERUP: {
          processPointerEvent(msg, window, wParam, lParam, eventHandled);
          break;
        }
        case WM_POINTERLEAVE: {
          uint32_t pointerId = GET_POINTERID_WPARAM(wParam);
          POINTER_INFO pointerInfo;
          if (!GetPointerInfo(pointerId, &pointerInfo)) {
            break;
          }

          /* Reset pointer pen info if pen device has left tracking range. */
          if (pointerInfo.pointerType == PT_PEN) {
            window->resetPointerPenInfo();
            eventHandled = true;
          }
          break;
        }
        /* =======================
         * Mouse events, processed
         * ======================= */
        case WM_LBUTTONDOWN: {
          event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskLeft);
          break;
        }
        case WM_MBUTTONDOWN: {
          event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskMiddle);
          break;
        }
        case WM_RBUTTONDOWN: {
          event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskRight);
          break;
        }
        case WM_XBUTTONDOWN: {
          if (short(HIWORD(wParam)) == XBUTTON1) {
            event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskButton4);
          }
          else if (short(HIWORD(wParam)) == XBUTTON2) {
            event = processButtonEvent(GHOST_kEventButtonDown, window, GHOST_kButtonMaskButton5);
          }
          break;
        }
        case WM_LBUTTONUP: {
          event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskLeft);
          break;
        }
        case WM_MBUTTONUP: {
          event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskMiddle);
          break;
        }
        case WM_RBUTTONUP: {
          event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskRight);
          break;
        }
        case WM_XBUTTONUP: {
          if (short(HIWORD(wParam)) == XBUTTON1) {
            event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskButton4);
          }
          else if (short(HIWORD(wParam)) == XBUTTON2) {
            event = processButtonEvent(GHOST_kEventButtonUp, window, GHOST_kButtonMaskButton5);
          }
          break;
        }
        case WM_MOUSEMOVE: {
          if (!window->m_mousePresent) {
            WINTAB_PRINTF("HWND %p mouse enter\n", window->getHWND());
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            /* Request WM_MOUSELEAVE message when the cursor leaves the client area. */
            tme.dwFlags = TME_LEAVE;
            if (system->m_autoFocus) {
              /* Request WM_MOUSEHOVER message after 100ms when in the client area. */
              tme.dwFlags |= TME_HOVER;
              tme.dwHoverTime = 100;
            }
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            window->m_mousePresent = true;
            GHOST_Wintab *wt = window->getWintab();
            if (wt) {
              wt->gainFocus();
            }
          }

          const int32_t window_co[2] = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
          int32_t screen_co[2];
          window->clientToScreen(UNPACK2(window_co), UNPACK2(screen_co));
          event = processCursorEvent(window, screen_co);

          break;
        }
        case WM_MOUSEHOVER: {
          /* Mouse Tracking is now off. TrackMouseEvent restarts in MouseMove. */
          window->m_mousePresent = false;

          /* Auto-focus only occurs within Blender windows, not with _other_ applications. We are
           * notified of change of focus from our console, but it returns null from GetFocus. */
          HWND old_hwnd = ::GetFocus();
          if (old_hwnd && hwnd != old_hwnd) {
            HWND new_parent = ::GetParent(hwnd);
            HWND old_parent = ::GetParent(old_hwnd);
            if (hwnd == old_parent || old_hwnd == new_parent) {
              /* Child to its parent, parent to its child. */
              ::SetFocus(hwnd);
            }
            else if (new_parent != HWND_DESKTOP && new_parent == old_parent) {
              /* Between siblings of same parent. */
              ::SetFocus(hwnd);
            }
            else if (!new_parent && !old_parent) {
              /* Between main windows that don't overlap. */
              RECT new_rect, old_rect, dest_rect;

              /* The rects without the outside shadows and slightly inset. */
              DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &new_rect, sizeof(RECT));
              ::InflateRect(&new_rect, -1, -1);
              DwmGetWindowAttribute(
                  old_hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &old_rect, sizeof(RECT));
              ::InflateRect(&old_rect, -1, -1);

              if (!IntersectRect(&dest_rect, &new_rect, &old_rect)) {
                ::SetFocus(hwnd);
              }
            }
          }
          break;
        }
        case WM_MOUSEWHEEL: {
          /* The WM_MOUSEWHEEL message is sent to the focus window
           * when the mouse wheel is rotated. The DefWindowProc
           * function propagates the message to the window's parent.
           * There should be no internal forwarding of the message,
           * since DefWindowProc propagates it up the parent chain
           * until it finds a window that processes it.
           */
          processWheelEvent(window, wParam, lParam);
          eventHandled = true;
#ifdef BROKEN_PEEK_TOUCHPAD
          PostMessage(hwnd, WM_USER, 0, 0);
#endif
          break;
        }
        case WM_SETCURSOR: {
          /* The WM_SETCURSOR message is sent to a window if the mouse causes the cursor
           * to move within a window and mouse input is not captured.
           * This means we have to set the cursor shape every time the mouse moves!
           * The DefWindowProc function uses this message to set the cursor to an
           * arrow if it is not in the client area.
           */
          if (LOWORD(lParam) == HTCLIENT) {
            /* Load the current cursor. */
            window->loadCursor(window->getCursorVisibility(), window->getCursorShape());
            /* Bypass call to #DefWindowProc. */
            return 0;
          }
          else {
            /* Outside of client area show standard cursor. */
            window->loadCursor(true, GHOST_kStandardCursorDefault);
          }
          break;
        }
        case WM_MOUSELEAVE: {
          WINTAB_PRINTF("HWND %p mouse leave\n", window->getHWND());
          window->m_mousePresent = false;
          if (window->getTabletData().Active == GHOST_kTabletModeNone) {
            /* FIXME: document why the cursor motion event on mouse leave is needed. */
            int32_t screen_co[2] = {0, 0};
            system->getCursorPosition(screen_co[0], screen_co[1]);
            event = processCursorEvent(window, screen_co);
          }
          GHOST_Wintab *wt = window->getWintab();
          if (wt) {
            wt->loseFocus();
          }
          break;
        }
        /* =====================
         * Mouse events, ignored
         * ===================== */
        case WM_NCMOUSEMOVE: {
          /* The WM_NCMOUSEMOVE message is posted to a window when the cursor is moved
           * within the non-client area of the window. This message is posted to the window that
           * contains the cursor. If a window has captured the mouse, this message is not posted.
           */
        }
        case WM_NCHITTEST: {
          /* The WM_NCHITTEST message is sent to a window when the cursor moves, or
           * when a mouse button is pressed or released. If the mouse is not captured,
           * the message is sent to the window beneath the cursor. Otherwise, the message
           * is sent to the window that has captured the mouse.
           */
          break;
        }
        /* ========================
         * Window events, processed
         * ======================== */
        case WM_CLOSE: {
          /* The WM_CLOSE message is sent as a signal that a window
           * or an application should terminate. Restore if minimized. */
          if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
          }
          event = processWindowEvent(GHOST_kEventWindowClose, window);
          break;
        }
        case WM_ACTIVATE: {
          /* The WM_ACTIVATE message is sent to both the window being activated and the window
           * being deactivated. If the windows use the same input queue, the message is sent
           * synchronously, first to the window procedure of the top-level window being
           * deactivated, then to the window procedure of the top-level window being activated.
           * If the windows use different input queues, the message is sent asynchronously,
           * so the window is activated immediately. */

          system->m_wheelDeltaAccum = 0;
          event = processWindowEvent(
              LOWORD(wParam) ? GHOST_kEventWindowActivate : GHOST_kEventWindowDeactivate, window);
          /* WARNING: Let DefWindowProc handle WM_ACTIVATE, otherwise WM_MOUSEWHEEL
           * will not be dispatched to OUR active window if we minimize one of OUR windows. */
          if (LOWORD(wParam) == WA_INACTIVE) {
            window->lostMouseCapture();
          }

          lResult = ::DefWindowProc(hwnd, msg, wParam, lParam);
          break;
        }
        case WM_ENTERSIZEMOVE: {
          /* The WM_ENTERSIZEMOVE message is sent one time to a window after it enters the moving
           * or sizing modal loop. The window enters the moving or sizing modal loop when the user
           * clicks the window's title bar or sizing border, or when the window passes the
           * WM_SYSCOMMAND message to the DefWindowProc function and the wParam parameter of the
           * message specifies the SC_MOVE or SC_SIZE value. The operation is complete when
           * DefWindowProc returns.
           */
          window->m_inLiveResize = 1;
          break;
        }
        case WM_EXITSIZEMOVE: {
          window->m_inLiveResize = 0;
          break;
        }
        case WM_PAINT: {
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
        }
        case WM_GETMINMAXINFO: {
          /* The WM_GETMINMAXINFO message is sent to a window when the size or
           * position of the window is about to change. An application can use
           * this message to override the window's default maximized size and
           * position, or its default minimum or maximum tracking size.
           */
          processMinMaxInfo((MINMAXINFO *)lParam);
          /* Let DefWindowProc handle it. */
          break;
        }
        case WM_SIZING: {
          event = processWindowSizeEvent(window);
          break;
        }
        case WM_SIZE: {
          /* The WM_SIZE message is sent to a window after its size has changed.
           * The WM_SIZE and WM_MOVE messages are not sent if an application handles the
           * WM_WINDOWPOSCHANGED message without calling DefWindowProc. It is more efficient
           * to perform any move or size change processing during the WM_WINDOWPOSCHANGED
           * message without calling DefWindowProc.
           */
          event = processWindowSizeEvent(window);
          break;
        }
        case WM_CAPTURECHANGED: {
          window->lostMouseCapture();
          break;
        }
        case WM_MOVING:
          /* The WM_MOVING message is sent to a window that the user is moving. By processing
           * this message, an application can monitor the size and position of the drag rectangle
           * and, if needed, change its size or position.
           */
        case WM_MOVE: {
          /* The WM_SIZE and WM_MOVE messages are not sent if an application handles the
           * WM_WINDOWPOSCHANGED message without calling DefWindowProc. It is more efficient
           * to perform any move or size change processing during the WM_WINDOWPOSCHANGED
           * message without calling DefWindowProc.
           */
          /* See #WM_SIZE comment. */
          if (window->m_inLiveResize) {
            system->pushEvent(processWindowEvent(GHOST_kEventWindowMove, window));
            system->dispatchEvents();
          }
          else {
            event = processWindowEvent(GHOST_kEventWindowMove, window);
          }

          break;
        }
        case WM_DPICHANGED: {
          /* The WM_DPICHANGED message is sent when the effective dots per inch (dpi) for a
           * window has changed. The DPI is the scale factor for a window. There are multiple
           * events that can cause the DPI to change such as when the window is moved to a monitor
           * with a different DPI. */

          /* The suggested new size and position of the window. */
          RECT *const suggestedWindowRect = (RECT *)lParam;

          /* Push DPI change event first. */
          system->pushEvent(processWindowEvent(GHOST_kEventWindowDPIHintChanged, window));
          system->dispatchEvents();
          eventHandled = true;

          /* Then move and resize window. */
          SetWindowPos(hwnd,
                       NULL,
                       suggestedWindowRect->left,
                       suggestedWindowRect->top,
                       suggestedWindowRect->right - suggestedWindowRect->left,
                       suggestedWindowRect->bottom - suggestedWindowRect->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);

          window->updateDPI();
          break;
        }
        case WM_DISPLAYCHANGE: {
          GHOST_Wintab *wt = window->getWintab();
          if (wt) {
            wt->remapCoordinates();
          }
          break;
        }
        case WM_KILLFOCUS: {
          /* The WM_KILLFOCUS message is sent to a window immediately before it loses the keyboard
           * focus. We want to prevent this if a window is still active and it loses focus to
           * nowhere. */
          if (!wParam && hwnd == ::GetActiveWindow()) {
            ::SetFocus(hwnd);
          }
          break;
        }
        case WM_SETTINGCHANGE: {
          /* Microsoft: "Note that some applications send this message with lParam set to NULL" */
          if ((lParam != NULL) && (wcscmp(LPCWSTR(lParam), L"ImmersiveColorSet") == 0)) {
            window->ThemeRefresh();
          }
          break;
        }
        /* ======================
         * Window events, ignored
         * ====================== */
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
           * invalidated portion of a window for painting. */
        case WM_NCPAINT:
          /* An application sends the WM_NCPAINT message to a window
           * when its frame must be painted. */
        case WM_NCACTIVATE:
          /* The WM_NCACTIVATE message is sent to a window when its non-client area needs to be
           * changed to indicate an active or inactive state. */
        case WM_DESTROY:
          /* The WM_DESTROY message is sent when a window is being destroyed. It is sent to the
           * window procedure of the window being destroyed after the window is removed from the
           * screen. This message is sent first to the window being destroyed and then to the child
           * windows (if any) as they are destroyed. During the processing of the message, it can
           * be assumed that all child windows still exist. */
        case WM_NCDESTROY: {
          /* The WM_NCDESTROY message informs a window that its non-client area is being
           * destroyed. The DestroyWindow function sends the WM_NCDESTROY message to the window
           * following the WM_DESTROY message. WM_DESTROY is used to free the allocated memory
           * object associated with the window.
           */
          break;
        }
        case WM_SHOWWINDOW:
          /* The WM_SHOWWINDOW message is sent to a window when the window is
           * about to be hidden or shown. */
        case WM_WINDOWPOSCHANGING:
          /* The WM_WINDOWPOSCHANGING message is sent to a window whose size, position, or place in
           * the Z order is about to change as a result of a call to the SetWindowPos function or
           * another window-management function. */
        case WM_SETFOCUS: {
          /* The WM_SETFOCUS message is sent to a window after it has gained the keyboard focus. */
          break;
        }
        /* ============
         * Other events
         * ============ */
        case WM_GETTEXT:
          /* An application sends a WM_GETTEXT message to copy the text that
           * corresponds to a window into a buffer provided by the caller. */
        case WM_ACTIVATEAPP:
          /* The WM_ACTIVATEAPP message is sent when a window belonging to a
           * different application than the active window is about to be activated.
           * The message is sent to the application whose window is being activated
           * and to the application whose window is being deactivated. */
        case WM_TIMER: {
          /* The WIN32 docs say:
           * The WM_TIMER message is posted to the installing thread's message queue
           * when a timer expires. You can process the message by providing a WM_TIMER
           * case in the window procedure. Otherwise, the default window procedure will
           * call the TimerProc callback function specified in the call to the SetTimer
           * function used to install the timer.
           *
           * In GHOST, we let DefWindowProc call the timer callback. */
          break;
        }
        case DM_POINTERHITTEST: {
          /* The DM_POINTERHITTEST message is sent to a window, when pointer input is first
           * detected, in order to determine the most probable input target for Direct
           * Manipulation. */
          if (system->m_multitouchGestures) {
            window->onPointerHitTest(wParam);
          }
          break;
        }
      }
    }
    else {
      /* Event found for a window before the pointer to the class has been set. */
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
    /* Events without valid `hwnd`. */
    GHOST_PRINT("GHOST_SystemWin32::wndProc: event without window\n");
  }

  if (event) {
    system->pushEvent(event);
    eventHandled = true;
  }

  if (!eventHandled) {
    lResult = ::DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  return lResult;
}

char *GHOST_SystemWin32::getClipboard(bool /*selection*/) const
{
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

    char *temp_buff = alloc_utf_8_from_16(buffer, 0);

    /* Buffer mustn't be accessed after CloseClipboard
     * it would like accessing free-d memory */
    GlobalUnlock(hData);
    CloseClipboard();

    return temp_buff;
  }
  if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(NULL)) {
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
    char *temp_buff = (char *)malloc(len + 1);
    strncpy(temp_buff, buffer, len);
    temp_buff[len] = '\0';

    /* Buffer mustn't be accessed after CloseClipboard
     * it would like accessing free-d memory */
    GlobalUnlock(hData);
    CloseClipboard();

    return temp_buff;
  }
  return nullptr;
}

void GHOST_SystemWin32::putClipboard(const char *buffer, bool selection) const
{
  if (selection || !buffer) {
    return;
  } /* For copying the selection, used on X11. */

  if (OpenClipboard(NULL)) {
    EmptyClipboard();

    /* Get length of buffer including the terminating null. */
    size_t len = count_utf_16_from_8(buffer);

    HGLOBAL clipbuffer = GlobalAlloc(GMEM_MOVEABLE, sizeof(wchar_t) * len);
    if (clipbuffer) {
      wchar_t *data = (wchar_t *)GlobalLock(clipbuffer);

      conv_utf_8_to_16(buffer, data, len);

      GlobalUnlock(clipbuffer);
      SetClipboardData(CF_UNICODETEXT, clipbuffer);
    }

    CloseClipboard();
  }
}

GHOST_TSuccess GHOST_SystemWin32::hasClipboardImage(void) const
{
  if (IsClipboardFormatAvailable(CF_DIBV5) ||
      IsClipboardFormatAvailable(RegisterClipboardFormat("PNG")))
  {
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

static uint *getClipboardImageDibV5(int *r_width, int *r_height)
{
  HANDLE hGlobal = GetClipboardData(CF_DIBV5);
  if (hGlobal == nullptr) {
    return nullptr;
  }

  BITMAPV5HEADER *bitmapV5Header = (BITMAPV5HEADER *)GlobalLock(hGlobal);
  if (bitmapV5Header == nullptr) {
    return nullptr;
  }

  int offset = bitmapV5Header->bV5Size + bitmapV5Header->bV5ClrUsed * sizeof(RGBQUAD);

  if (bitmapV5Header->bV5Compression == BI_BITFIELDS) {
    offset += 12;
  }
  BYTE *buffer = (BYTE *)bitmapV5Header + offset;
  int bitcount = bitmapV5Header->bV5BitCount;
  int width = bitmapV5Header->bV5Width;
  int height = bitmapV5Header->bV5Height;
  *r_width = width;
  *r_height = height;

  DWORD ColorMasks[4];
  ColorMasks[0] = bitmapV5Header->bV5RedMask ? bitmapV5Header->bV5RedMask : 0xff;
  ColorMasks[1] = bitmapV5Header->bV5GreenMask ? bitmapV5Header->bV5GreenMask : 0xff00;
  ColorMasks[2] = bitmapV5Header->bV5BlueMask ? bitmapV5Header->bV5BlueMask : 0xff0000;
  ColorMasks[3] = bitmapV5Header->bV5AlphaMask ? bitmapV5Header->bV5AlphaMask : 0xff000000;

  /* Bit shifts needed for the ColorMasks. */
  DWORD ColorShifts[4];
  for (int i = 0; i < 4; i++) {
    _BitScanForward(&ColorShifts[i], ColorMasks[i]);
  }

  uchar *source = (uchar *)buffer;
  uint *rgba = (uint *)malloc(width * height * 4);
  uint8_t *target = (uint8_t *)rgba;

  if (bitmapV5Header->bV5Compression == BI_BITFIELDS && bitcount == 32) {
    for (int h = 0; h < height; h++) {
      for (int w = 0; w < width; w++, target += 4, source += 4) {
        DWORD *pix = (DWORD *)source;
        target[0] = uint8_t((*pix & ColorMasks[0]) >> ColorShifts[0]);
        target[1] = uint8_t((*pix & ColorMasks[1]) >> ColorShifts[1]);
        target[2] = uint8_t((*pix & ColorMasks[2]) >> ColorShifts[2]);
        target[3] = uint8_t((*pix & ColorMasks[3]) >> ColorShifts[3]);
      }
    }
  }
  else if (bitmapV5Header->bV5Compression == BI_RGB && bitcount == 32) {
    for (int h = 0; h < height; h++) {
      for (int w = 0; w < width; w++, target += 4, source += 4) {
        RGBQUAD *quad = (RGBQUAD *)source;
        target[0] = uint8_t(quad->rgbRed);
        target[1] = uint8_t(quad->rgbGreen);
        target[2] = uint8_t(quad->rgbBlue);
        target[3] = (bitmapV5Header->bV5AlphaMask) ? uint8_t(quad->rgbReserved) : 255;
      }
    }
  }
  else if (bitmapV5Header->bV5Compression == BI_RGB && bitcount == 24) {
    int bytes_per_row = ((((width * bitcount) + 31) & ~31) >> 3);
    int slack = bytes_per_row - (width * 3);
    for (int h = 0; h < height; h++, source += slack) {
      for (int w = 0; w < width; w++, target += 4, source += 3) {
        RGBTRIPLE *triple = (RGBTRIPLE *)source;
        target[0] = uint8_t(triple->rgbtRed);
        target[1] = uint8_t(triple->rgbtGreen);
        target[2] = uint8_t(triple->rgbtBlue);
        target[3] = 255;
      }
    }
  }

  GlobalUnlock(hGlobal);
  return rgba;
}

/* Works with any image format that ImBuf can load. */
static uint *getClipboardImageImBuf(int *r_width, int *r_height, UINT format)
{
  HANDLE hGlobal = GetClipboardData(format);
  if (hGlobal == nullptr) {
    return nullptr;
  }

  LPVOID pMem = GlobalLock(hGlobal);
  if (!pMem) {
    return nullptr;
  }

  uint *rgba = nullptr;

  ImBuf *ibuf = IMB_ibImageFromMemory(
      (uchar *)pMem, GlobalSize(hGlobal), IB_rect, nullptr, "<clipboard>");

  if (ibuf) {
    *r_width = ibuf->x;
    *r_height = ibuf->y;
    rgba = (uint *)malloc(4 * ibuf->x * ibuf->y);
    memcpy(rgba, ibuf->byte_buffer.data, 4 * ibuf->x * ibuf->y);
    IMB_freeImBuf(ibuf);
  }

  GlobalUnlock(hGlobal);
  return rgba;
}

uint *GHOST_SystemWin32::getClipboardImage(int *r_width, int *r_height) const
{
  if (!OpenClipboard(nullptr)) {
    return nullptr;
  }

  /* Synthesized formats are placed after posted formats. */
  UINT cfPNG = RegisterClipboardFormat("PNG");
  UINT format = 0;
  for (int cf = EnumClipboardFormats(0); cf; cf = EnumClipboardFormats(cf)) {
    if (ELEM(cf, CF_DIBV5, cfPNG)) {
      format = cf;
    }
    if (cf == CF_DIBV5 || (cf == CF_BITMAP && format == cfPNG)) {
      break; /* Favor CF_DIBV5, but not if synthesized. */
    }
  }

  uint *rgba = nullptr;

  if (format == CF_DIBV5) {
    rgba = getClipboardImageDibV5(r_width, r_height);
  }
  else if (format == cfPNG) {
    rgba = getClipboardImageImBuf(r_width, r_height, cfPNG);
  }
  else {
    *r_width = 0;
    *r_height = 0;
  }

  CloseClipboard();
  return rgba;
}

static bool putClipboardImageDibV5(uint *rgba, int width, int height)
{
  DWORD size_pixels = width * height * 4;

  /* Pixel data is 12 bytes after the header. */
  HGLOBAL hMem = GlobalAlloc(GHND, sizeof(BITMAPV5HEADER) + 12 + size_pixels);
  if (!hMem) {
    return false;
  }

  BITMAPV5HEADER *hdr = (BITMAPV5HEADER *)GlobalLock(hMem);
  if (!hdr) {
    GlobalFree(hMem);
    return false;
  }

  hdr->bV5Size = sizeof(BITMAPV5HEADER);
  hdr->bV5Width = width;
  hdr->bV5Height = height;
  hdr->bV5Planes = 1;
  hdr->bV5BitCount = 32;
  hdr->bV5SizeImage = size_pixels;
  hdr->bV5Compression = BI_BITFIELDS;
  hdr->bV5RedMask = 0x000000ff;
  hdr->bV5GreenMask = 0x0000ff00;
  hdr->bV5BlueMask = 0x00ff0000;
  hdr->bV5AlphaMask = 0xff000000;
  hdr->bV5CSType = LCS_sRGB;
  hdr->bV5Intent = LCS_GM_IMAGES;
  hdr->bV5ClrUsed = 0;

  memcpy((char *)hdr + sizeof(BITMAPV5HEADER) + 12, rgba, size_pixels);

  GlobalUnlock(hMem);

  if (!SetClipboardData(CF_DIBV5, hMem)) {
    GlobalFree(hMem);
    return false;
  }

  return true;
}

static bool putClipboardImagePNG(uint *rgba, int width, int height)
{
  UINT cf = RegisterClipboardFormat("PNG");

  /* Load buffer into ImBuf, convert to PNG. */
  ImBuf *ibuf = IMB_allocFromBuffer(reinterpret_cast<uint8_t *>(rgba), nullptr, width, height, 32);
  ibuf->ftype = IMB_FTYPE_PNG;
  ibuf->foptions.quality = 15;
  if (!IMB_saveiff(ibuf, "<memory>", IB_rect | IB_mem)) {
    IMB_freeImBuf(ibuf);
    return false;
  }

  HGLOBAL hMem = GlobalAlloc(GHND, ibuf->encoded_buffer_size);
  if (!hMem) {
    IMB_freeImBuf(ibuf);
    return false;
  }

  LPVOID pMem = GlobalLock(hMem);
  if (!pMem) {
    IMB_freeImBuf(ibuf);
    GlobalFree(hMem);
    return false;
  }

  memcpy(pMem, ibuf->encoded_buffer.data, ibuf->encoded_buffer_size);

  GlobalUnlock(hMem);
  IMB_freeImBuf(ibuf);

  if (!SetClipboardData(cf, hMem)) {
    GlobalFree(hMem);
    return false;
  }

  return true;
}

GHOST_TSuccess GHOST_SystemWin32::putClipboardImage(uint *rgba, int width, int height) const
{
  if (!OpenClipboard(nullptr) || !EmptyClipboard()) {
    return GHOST_kFailure;
  }

  bool ok = putClipboardImageDibV5(rgba, width, height) &&
            putClipboardImagePNG(rgba, width, height);

  CloseClipboard();

  return (ok) ? GHOST_kSuccess : GHOST_kFailure;
}

/* -------------------------------------------------------------------- */
/** \name Message Box
 * \{ */

GHOST_TSuccess GHOST_SystemWin32::showMessageBox(const char *title,
                                                 const char *message,
                                                 const char *help_label,
                                                 const char *continue_label,
                                                 const char *link,
                                                 GHOST_DialogOptions dialog_options) const
{
  const wchar_t *title_16 = alloc_utf16_from_8(title, 0);
  const wchar_t *message_16 = alloc_utf16_from_8(message, 0);
  const wchar_t *help_label_16 = alloc_utf16_from_8(help_label, 0);
  const wchar_t *continue_label_16 = alloc_utf16_from_8(continue_label, 0);

  int nButtonPressed = 0;
  TASKDIALOGCONFIG config = {0};
  const TASKDIALOG_BUTTON buttons[] = {{IDOK, help_label_16}, {IDCONTINUE, continue_label_16}};

  config.cbSize = sizeof(config);
  config.hInstance = 0;
  config.dwCommonButtons = 0;
  config.pszMainIcon = (dialog_options & GHOST_DialogError   ? TD_ERROR_ICON :
                        dialog_options & GHOST_DialogWarning ? TD_WARNING_ICON :
                                                               TD_INFORMATION_ICON);
  config.pszWindowTitle = L"Blender";
  config.pszMainInstruction = title_16;
  config.pszContent = message_16;
  config.pButtons = (link) ? buttons : buttons + 1;
  config.cButtons = (link) ? 2 : 1;

  TaskDialogIndirect(&config, &nButtonPressed, NULL, NULL);
  switch (nButtonPressed) {
    case IDOK:
      ShellExecute(NULL, "open", link, NULL, NULL, SW_SHOWNORMAL);
      break;
    case IDCONTINUE:
      break;
    default:
      break; /* Should never happen. */
  }

  free((void *)title_16);
  free((void *)message_16);
  free((void *)help_label_16);
  free((void *)continue_label_16);

  return GHOST_kSuccess;
}

/** \} */

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
    if (pid != (start_from_launcher ? ppid : GetCurrentProcessId())) {
      return true;
    }
  }

  return false;
}

bool GHOST_SystemWin32::setConsoleWindowState(GHOST_TConsoleWindowState action)
{
  HWND wnd = GetConsoleWindow();

  switch (action) {
    case GHOST_kConsoleWindowStateHideForNonConsoleLaunch: {
      if (!isStartedFromCommandPrompt()) {
        ShowWindow(wnd, SW_HIDE);
        m_consoleStatus = false;
      }
      break;
    }
    case GHOST_kConsoleWindowStateHide: {
      ShowWindow(wnd, SW_HIDE);
      m_consoleStatus = false;
      break;
    }
    case GHOST_kConsoleWindowStateShow: {
      ShowWindow(wnd, SW_SHOW);
      if (!isStartedFromCommandPrompt()) {
        DeleteMenu(GetSystemMenu(wnd, FALSE), SC_CLOSE, MF_BYCOMMAND);
      }
      m_consoleStatus = true;
      break;
    }
    case GHOST_kConsoleWindowStateToggle: {
      ShowWindow(wnd, m_consoleStatus ? SW_HIDE : SW_SHOW);
      m_consoleStatus = !m_consoleStatus;
      if (m_consoleStatus && !isStartedFromCommandPrompt()) {
        DeleteMenu(GetSystemMenu(wnd, FALSE), SC_CLOSE, MF_BYCOMMAND);
      }
      break;
    }
  }

  return m_consoleStatus;
}
