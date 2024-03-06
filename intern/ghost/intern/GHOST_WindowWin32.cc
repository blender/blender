/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_WindowWin32.hh"
#include "GHOST_ContextD3D.hh"
#include "GHOST_ContextNone.hh"
#include "GHOST_DropTargetWin32.hh"
#include "GHOST_SystemWin32.hh"
#include "GHOST_WindowManager.hh"
#include "utf_winfunc.hh"
#include "utfconv.hh"

#ifdef WITH_OPENGL_BACKEND
#  include "GHOST_ContextWGL.hh"
#endif
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#ifdef WIN32
#  include "BLI_path_util.h"
#endif

#include <Dwmapi.h>

#include <assert.h>
#include <math.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <string.h>
#include <windowsx.h>

#ifndef GET_POINTERID_WPARAM
#  define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))
#endif /* GET_POINTERID_WPARAM */

const wchar_t *GHOST_WindowWin32::s_windowClassName = L"GHOST_WindowClass";
const int GHOST_WindowWin32::s_maxTitleLength = 128;

/* force NVidia Optimus to used dedicated graphics */
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

GHOST_WindowWin32::GHOST_WindowWin32(GHOST_SystemWin32 *system,
                                     const char *title,
                                     int32_t left,
                                     int32_t top,
                                     uint32_t width,
                                     uint32_t height,
                                     GHOST_TWindowState state,
                                     GHOST_TDrawingContextType type,
                                     bool wantStereoVisual,
                                     bool alphaBackground,
                                     GHOST_WindowWin32 *parentwindow,
                                     bool is_debug,
                                     bool dialog)
    : GHOST_Window(width, height, state, wantStereoVisual, false),
      m_mousePresent(false),
      m_inLiveResize(false),
      m_system(system),
      m_dropTarget(nullptr),
      m_hWnd(0),
      m_hDC(0),
      m_isDialog(dialog),
      m_hasMouseCaptured(false),
      m_hasGrabMouse(false),
      m_nPressedButtons(0),
      m_customCursor(0),
      m_wantAlphaBackground(alphaBackground),
      m_Bar(nullptr),
      m_wintab(nullptr),
      m_lastPointerTabletData(GHOST_TABLET_DATA_NONE),
      m_normal_state(GHOST_kWindowStateNormal),
      m_user32(::LoadLibrary("user32.dll")),
      m_parentWindowHwnd(parentwindow ? parentwindow->m_hWnd : HWND_DESKTOP),
      m_directManipulationHelper(nullptr),
      m_debug_context(is_debug)
{
  DWORD style = parentwindow ?
                    WS_POPUPWINDOW | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX :
                    WS_OVERLAPPEDWINDOW;

  if (state == GHOST_kWindowStateFullScreen) {
    style |= WS_MAXIMIZE;
  }

  /* Forces owned windows onto taskbar and allows minimization. */
  DWORD extended_style = parentwindow ? WS_EX_APPWINDOW : 0;

  if (dialog) {
    /* When we are ready to make windows of this type:
     * style = WS_POPUPWINDOW | WS_CAPTION
     * extended_style = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST
     */
  }

  RECT win_rect = {left, top, long(left + width), long(top + height)};
  adjustWindowRectForClosestMonitor(&win_rect, style, extended_style);

  wchar_t *title_16 = alloc_utf16_from_8((char *)title, 0);
  m_hWnd = ::CreateWindowExW(extended_style,                 /* window extended style */
                             s_windowClassName,              /* pointer to registered class name */
                             title_16,                       /* pointer to window name */
                             style,                          /* window style */
                             win_rect.left,                  /* horizontal position of window */
                             win_rect.top,                   /* vertical position of window */
                             win_rect.right - win_rect.left, /* window width */
                             win_rect.bottom - win_rect.top, /* window height */
                             m_parentWindowHwnd,             /* handle to parent or owner window */
                             0,                    /* handle to menu or child-window identifier */
                             ::GetModuleHandle(0), /* handle to application instance */
                             0);                   /* pointer to window-creation data */
  free(title_16);

  if (m_hWnd == nullptr) {
    return;
  }

  registerWindowAppUserModelProperties();

  /*  Store the device context. */
  m_hDC = ::GetDC(m_hWnd);

  if (!setDrawingContextType(type)) {
    const char *title = "Blender - Unsupported Graphics Card Configuration";
    const char *text = "";
#if defined(WIN32)
    if (strncmp(BLI_getenv("PROCESSOR_IDENTIFIER"), "ARM", 3) == 0) {
      text =
          "A driver with support for OpenGL 4.3 or higher is required.\n\n"
          "If you are on a Qualcomm 8cx Gen3 device or newer, you need to download the"
          "\"OpenCL™, OpenGL®, and Vulkan® Compatibility Pack\" from the MS Store.";
    }
    else
#endif
    {
      text =
          "A graphics card and driver with support for OpenGL 4.3 or higher is "
          "required.\n\nInstalling the latest driver for your graphics card might resolve the "
          "issue.";
      if (GetSystemMetrics(SM_CMONITORS) > 1) {
        text =
            "A graphics card and driver with support for OpenGL 4.3 or higher is "
            "required.\n\nPlugging all monitors into your primary graphics card might resolve "
            "this issue. Installing the latest driver for your graphics card could also help.";
      }
    }
    MessageBox(m_hWnd, text, title, MB_OK | MB_ICONERROR);
    ::ReleaseDC(m_hWnd, m_hDC);
    ::DestroyWindow(m_hWnd);
    m_hWnd = nullptr;
    if (!parentwindow) {
      exit(0);
    }
    return;
  }

  RegisterTouchWindow(m_hWnd, 0);

  /* Register as drop-target. #OleInitialize(0) required first, done in GHOST_SystemWin32. */
  m_dropTarget = new GHOST_DropTargetWin32(this, m_system);
  ::RegisterDragDrop(m_hWnd, m_dropTarget);

  /* Store a pointer to this class in the window structure. */
  ::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

  if (!m_system->m_windowFocus) {
    /* If we don't want focus then lower to bottom. */
    ::SetWindowPos(m_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  if (parentwindow) {
    /* Release any parent capture to allow immediate interaction (#90110). */
    ::ReleaseCapture();
    parentwindow->lostMouseCapture();
  }

  /* Show the window. */
  int nCmdShow;
  switch (state) {
    case GHOST_kWindowStateMaximized:
      nCmdShow = SW_SHOWMAXIMIZED;
      break;
    case GHOST_kWindowStateMinimized:
      nCmdShow = (m_system->m_windowFocus) ? SW_SHOWMINIMIZED : SW_SHOWMINNOACTIVE;
      break;
    case GHOST_kWindowStateNormal:
    default:
      nCmdShow = (m_system->m_windowFocus) ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE;
      break;
  }

  ThemeRefresh();

  ::ShowWindow(m_hWnd, nCmdShow);

#ifdef WIN32_COMPOSITING
  if (alphaBackground && parentwindowhwnd == 0) {

    HRESULT hr = S_OK;

    /* Create and populate the Blur Behind structure. */
    DWM_BLURBEHIND bb = {0};

    /* Enable Blur Behind and apply to the entire client area. */
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = true;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);

    /* Apply Blur Behind. */
    hr = DwmEnableBlurBehindWindow(m_hWnd, &bb);
    DeleteObject(bb.hRgnBlur);
  }
#endif

  /* Force an initial paint of the window. */
  ::UpdateWindow(m_hWnd);

  /* Initialize WINTAB. */
  if (system->getTabletAPI() != GHOST_kTabletWinPointer) {
    loadWintab(GHOST_kWindowStateMinimized != state);
  }

  /* Allow the showing of a progress bar on the taskbar. */
  CoCreateInstance(
      CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (LPVOID *)&m_Bar);

  /* Initialize Direct Manipulation. */
  m_directManipulationHelper = GHOST_DirectManipulationHelper::create(m_hWnd, getDPIHint());
}

void GHOST_WindowWin32::updateDirectManipulation()
{
  if (!m_directManipulationHelper) {
    return;
  }

  m_directManipulationHelper->update();
}

void GHOST_WindowWin32::onPointerHitTest(WPARAM wParam)
{
  /* Only #DM_POINTERHITTEST can be the first message of input sequence of touch-pad input. */

  if (!m_directManipulationHelper) {
    return;
  }

  UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
  POINTER_INPUT_TYPE pointerType;
  if (GetPointerType(pointerId, &pointerType) && pointerType == PT_TOUCHPAD) {
    m_directManipulationHelper->onPointerHitTest(pointerId);
  }
}

GHOST_TTrackpadInfo GHOST_WindowWin32::getTrackpadInfo()
{
  if (!m_directManipulationHelper) {
    return {0, 0, 0};
  }

  return m_directManipulationHelper->getTrackpadInfo();
}

GHOST_WindowWin32::~GHOST_WindowWin32()
{
  if (m_hWnd) {
    unregisterWindowAppUserModelProperties();
  }

  if (m_Bar) {
    m_Bar->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
    m_Bar->Release();
    m_Bar = nullptr;
  }

  closeWintab();

  if (m_user32) {
    FreeLibrary(m_user32);
    m_user32 = nullptr;
  }

  if (m_customCursor) {
    DestroyCursor(m_customCursor);
    m_customCursor = nullptr;
  }

  if (m_hWnd != nullptr && m_hDC != nullptr && releaseNativeHandles()) {
    ::ReleaseDC(m_hWnd, m_hDC);
    m_hDC = nullptr;
  }

  if (m_hWnd) {
    /* If this window is referenced by others as parent, clear that relation or windows will free
     * the handle while we still reference it. */
    for (GHOST_IWindow *iter_win : m_system->getWindowManager()->getWindows()) {
      GHOST_WindowWin32 *iter_winwin = (GHOST_WindowWin32 *)iter_win;
      if (iter_winwin->m_parentWindowHwnd == m_hWnd) {
        ::SetWindowLongPtr(iter_winwin->m_hWnd, GWLP_HWNDPARENT, 0);
        iter_winwin->m_parentWindowHwnd = 0;
      }
    }

    if (m_dropTarget) {
      /* Disable DragDrop. */
      RevokeDragDrop(m_hWnd);
      /* Release our reference of the DropTarget and it will delete itself eventually. */
      m_dropTarget->Release();
      m_dropTarget = nullptr;
    }
    ::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, 0);
    ::DestroyWindow(m_hWnd);
    m_hWnd = 0;
  }

  delete m_directManipulationHelper;
  m_directManipulationHelper = nullptr;
}

void GHOST_WindowWin32::adjustWindowRectForClosestMonitor(LPRECT win_rect,
                                                          DWORD dwStyle,
                                                          DWORD dwExStyle)
{
  /* Get Details of the closest monitor. */
  HMONITOR hmonitor = MonitorFromRect(win_rect, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEX monitor;
  monitor.cbSize = sizeof(MONITORINFOEX);
  monitor.dwFlags = 0;
  GetMonitorInfo(hmonitor, &monitor);

  /* Constrain requested size and position to fit within this monitor. */
  LONG width = min(monitor.rcWork.right - monitor.rcWork.left, win_rect->right - win_rect->left);
  LONG height = min(monitor.rcWork.bottom - monitor.rcWork.top, win_rect->bottom - win_rect->top);
  win_rect->left = min(max(monitor.rcWork.left, win_rect->left), monitor.rcWork.right - width);
  win_rect->right = win_rect->left + width;
  win_rect->top = min(max(monitor.rcWork.top, win_rect->top), monitor.rcWork.bottom - height);
  win_rect->bottom = win_rect->top + height;

  /* With Windows 10 and newer we can adjust for chrome that differs with DPI and scale. */
  GHOST_WIN32_AdjustWindowRectExForDpi fpAdjustWindowRectExForDpi = nullptr;
  if (m_user32) {
    fpAdjustWindowRectExForDpi = (GHOST_WIN32_AdjustWindowRectExForDpi)::GetProcAddress(
        m_user32, "AdjustWindowRectExForDpi");
  }

  /* Adjust to allow for caption, borders, shadows, scaling, etc. Resulting values can be
   * correctly outside of monitor bounds. NOTE: You cannot specify #WS_OVERLAPPED when calling. */
  if (fpAdjustWindowRectExForDpi) {
    UINT dpiX, dpiY;
    GetDpiForMonitor(hmonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    fpAdjustWindowRectExForDpi(win_rect, dwStyle & ~WS_OVERLAPPED, FALSE, dwExStyle, dpiX);
  }
  else {
    AdjustWindowRectEx(win_rect, dwStyle & ~WS_OVERLAPPED, FALSE, dwExStyle);
  }

  /* But never allow a top position that can hide part of the title bar. */
  win_rect->top = max(monitor.rcWork.top, win_rect->top);
}

bool GHOST_WindowWin32::getValid() const
{
  return GHOST_Window::getValid() && m_hWnd != 0 && m_hDC != 0;
}

HWND GHOST_WindowWin32::getHWND() const
{
  return m_hWnd;
}

void *GHOST_WindowWin32::getOSWindow() const
{
  return (void *)m_hWnd;
}

void GHOST_WindowWin32::setTitle(const char *title)
{
  wchar_t *title_16 = alloc_utf16_from_8((char *)title, 0);
  ::SetWindowTextW(m_hWnd, (wchar_t *)title_16);
  free(title_16);
}

std::string GHOST_WindowWin32::getTitle() const
{
  std::wstring wtitle(::GetWindowTextLengthW(m_hWnd) + 1, L'\0');
  ::GetWindowTextW(m_hWnd, &wtitle[0], wtitle.capacity());

  std::string title(count_utf_8_from_16(wtitle.c_str()) + 1, '\0');
  conv_utf_16_to_8(wtitle.c_str(), &title[0], title.capacity());

  return title;
}

void GHOST_WindowWin32::getWindowBounds(GHOST_Rect &bounds) const
{
  RECT rect;
  ::GetWindowRect(m_hWnd, &rect);
  bounds.m_b = rect.bottom;
  bounds.m_l = rect.left;
  bounds.m_r = rect.right;
  bounds.m_t = rect.top;
}

void GHOST_WindowWin32::getClientBounds(GHOST_Rect &bounds) const
{
  RECT rect;
  POINT coord;
  if (!IsIconic(m_hWnd)) {
    ::GetClientRect(m_hWnd, &rect);

    coord.x = rect.left;
    coord.y = rect.top;
    ::ClientToScreen(m_hWnd, &coord);

    bounds.m_l = coord.x;
    bounds.m_t = coord.y;

    coord.x = rect.right;
    coord.y = rect.bottom;
    ::ClientToScreen(m_hWnd, &coord);

    bounds.m_r = coord.x;
    bounds.m_b = coord.y;
  }
  else {
    bounds.m_b = 0;
    bounds.m_l = 0;
    bounds.m_r = 0;
    bounds.m_t = 0;
  }
}

GHOST_TSuccess GHOST_WindowWin32::setClientWidth(uint32_t width)
{
  GHOST_TSuccess success;
  GHOST_Rect cBnds, wBnds;
  getClientBounds(cBnds);
  if (cBnds.getWidth() != int32_t(width)) {
    getWindowBounds(wBnds);
    int cx = wBnds.getWidth() + width - cBnds.getWidth();
    int cy = wBnds.getHeight();
    success = ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
                  GHOST_kSuccess :
                  GHOST_kFailure;
  }
  else {
    success = GHOST_kSuccess;
  }
  return success;
}

GHOST_TSuccess GHOST_WindowWin32::setClientHeight(uint32_t height)
{
  GHOST_TSuccess success;
  GHOST_Rect cBnds, wBnds;
  getClientBounds(cBnds);
  if (cBnds.getHeight() != int32_t(height)) {
    getWindowBounds(wBnds);
    int cx = wBnds.getWidth();
    int cy = wBnds.getHeight() + height - cBnds.getHeight();
    success = ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
                  GHOST_kSuccess :
                  GHOST_kFailure;
  }
  else {
    success = GHOST_kSuccess;
  }
  return success;
}

GHOST_TSuccess GHOST_WindowWin32::setClientSize(uint32_t width, uint32_t height)
{
  GHOST_TSuccess success;
  GHOST_Rect cBnds, wBnds;
  getClientBounds(cBnds);
  if ((cBnds.getWidth() != int32_t(width)) || (cBnds.getHeight() != int32_t(height))) {
    getWindowBounds(wBnds);
    int cx = wBnds.getWidth() + width - cBnds.getWidth();
    int cy = wBnds.getHeight() + height - cBnds.getHeight();
    success = ::SetWindowPos(m_hWnd, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
                  GHOST_kSuccess :
                  GHOST_kFailure;
  }
  else {
    success = GHOST_kSuccess;
  }
  return success;
}

GHOST_TWindowState GHOST_WindowWin32::getState() const
{
  if (::IsIconic(m_hWnd)) {
    return GHOST_kWindowStateMinimized;
  }
  else if (::IsZoomed(m_hWnd)) {
    LONG_PTR result = ::GetWindowLongPtr(m_hWnd, GWL_STYLE);
    return (result & WS_CAPTION) ? GHOST_kWindowStateMaximized : GHOST_kWindowStateFullScreen;
  }
  return GHOST_kWindowStateNormal;
}

void GHOST_WindowWin32::screenToClient(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  POINT point = {inX, inY};
  ::ScreenToClient(m_hWnd, &point);
  outX = point.x;
  outY = point.y;
}

void GHOST_WindowWin32::clientToScreen(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  POINT point = {inX, inY};
  ::ClientToScreen(m_hWnd, &point);
  outX = point.x;
  outY = point.y;
}

GHOST_TSuccess GHOST_WindowWin32::setState(GHOST_TWindowState state)
{
  GHOST_TWindowState curstate = getState();
  LONG_PTR style = GetWindowLongPtr(m_hWnd, GWL_STYLE) | WS_CAPTION;
  WINDOWPLACEMENT wp;
  wp.length = sizeof(WINDOWPLACEMENT);
  ::GetWindowPlacement(m_hWnd, &wp);

  switch (state) {
    case GHOST_kWindowStateMinimized:
      wp.showCmd = SW_MINIMIZE;
      break;
    case GHOST_kWindowStateMaximized:
      wp.showCmd = SW_SHOWMAXIMIZED;
      break;
    case GHOST_kWindowStateFullScreen:
      if (curstate != state && curstate != GHOST_kWindowStateMinimized) {
        m_normal_state = curstate;
      }
      wp.showCmd = SW_SHOWMAXIMIZED;
      wp.ptMaxPosition.x = 0;
      wp.ptMaxPosition.y = 0;
      style &= ~(WS_CAPTION | WS_MAXIMIZE);
      break;
    case GHOST_kWindowStateNormal:
    default:
      if (curstate == GHOST_kWindowStateFullScreen &&
          m_normal_state == GHOST_kWindowStateMaximized)
      {
        wp.showCmd = SW_SHOWMAXIMIZED;
        m_normal_state = GHOST_kWindowStateNormal;
      }
      else {
        wp.showCmd = SW_SHOWNORMAL;
      }
      break;
  }
  ::SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
  /* #SetWindowLongPtr Docs:
   * Frame changes not visible until #SetWindowPos with #SWP_FRAMECHANGED. */
  ::SetWindowPos(m_hWnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  return ::SetWindowPlacement(m_hWnd, &wp) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::setOrder(GHOST_TWindowOrder order)
{
  HWND hWndInsertAfter, hWndToRaise;

  if (order == GHOST_kWindowOrderBottom) {
    hWndInsertAfter = HWND_BOTTOM;
    hWndToRaise = ::GetWindow(m_hWnd, GW_HWNDNEXT); /* the window to raise */
  }
  else {
    if (getState() == GHOST_kWindowStateMinimized) {
      setState(GHOST_kWindowStateNormal);
    }
    hWndInsertAfter = HWND_TOP;
    hWndToRaise = nullptr;
  }

  if (::SetWindowPos(m_hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE) {
    return GHOST_kFailure;
  }

  if (hWndToRaise &&
      ::SetWindowPos(hWndToRaise, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE)
  {
    return GHOST_kFailure;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::invalidate()
{
  GHOST_TSuccess success;
  if (m_hWnd) {
    success = ::InvalidateRect(m_hWnd, 0, FALSE) != 0 ? GHOST_kSuccess : GHOST_kFailure;
  }
  else {
    success = GHOST_kFailure;
  }
  return success;
}

GHOST_Context *GHOST_WindowWin32::newDrawingContext(GHOST_TDrawingContextType type)
{
  switch (type) {
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kDrawingContextTypeVulkan: {
      GHOST_Context *context = new GHOST_ContextVK(false, m_hWnd, 1, 2, m_debug_context);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

#ifdef WITH_OPENGL_BACKEND
    case GHOST_kDrawingContextTypeOpenGL: {
      for (int minor = 6; minor >= 3; --minor) {
        GHOST_Context *context = new GHOST_ContextWGL(
            m_wantStereoVisual,
            m_wantAlphaBackground,
            m_hWnd,
            m_hDC,
            WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            4,
            minor,
            (m_debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
            GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);

        if (context->initializeDrawingContext()) {
          return context;
        }
        delete context;
      }
      return nullptr;
    }
#endif

    case GHOST_kDrawingContextTypeD3D: {
      GHOST_Context *context = new GHOST_ContextD3D(false, m_hWnd);

      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }

    default:
      /* Unsupported backend. */
      return nullptr;
  }
}

void GHOST_WindowWin32::lostMouseCapture()
{
  if (m_hasMouseCaptured) {
    m_hasGrabMouse = false;
    m_nPressedButtons = 0;
    m_hasMouseCaptured = false;
  }
}

bool GHOST_WindowWin32::isDialog() const
{
  return m_isDialog;
}

void GHOST_WindowWin32::updateMouseCapture(GHOST_MouseCaptureEventWin32 event)
{
  switch (event) {
    case MousePressed:
      m_nPressedButtons++;
      break;
    case MouseReleased:
      if (m_nPressedButtons) {
        m_nPressedButtons--;
      }
      break;
    case OperatorGrab:
      m_hasGrabMouse = true;
      break;
    case OperatorUngrab:
      m_hasGrabMouse = false;
      break;
  }

  if (!m_nPressedButtons && !m_hasGrabMouse && m_hasMouseCaptured) {
    ::ReleaseCapture();
    m_hasMouseCaptured = false;
  }
  else if ((m_nPressedButtons || m_hasGrabMouse) && !m_hasMouseCaptured) {
    ::SetCapture(m_hWnd);
    m_hasMouseCaptured = true;
  }
}

HCURSOR GHOST_WindowWin32::getStandardCursor(GHOST_TStandardCursor shape) const
{
  /* Convert GHOST cursor to Windows OEM cursor. */
  HANDLE cursor = nullptr;
  HMODULE module = ::GetModuleHandle(0);
  uint32_t flags = LR_SHARED | LR_DEFAULTSIZE;
  int cx = 0, cy = 0;

  switch (shape) {
    case GHOST_kStandardCursorCustom:
      if (m_customCursor) {
        return m_customCursor;
      }
      else {
        return nullptr;
      }
    case GHOST_kStandardCursorRightArrow:
      cursor = ::LoadImage(module, "arrowright_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorLeftArrow:
      cursor = ::LoadImage(module, "arrowleft_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorUpArrow:
      cursor = ::LoadImage(module, "arrowup_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorDownArrow:
      cursor = ::LoadImage(module, "arrowdown_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorVerticalSplit:
      cursor = ::LoadImage(module, "splitv_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorHorizontalSplit:
      cursor = ::LoadImage(module, "splith_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorKnife:
      cursor = ::LoadImage(module, "knife_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorEyedropper:
      cursor = ::LoadImage(module, "eyedropper_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorZoomIn:
      cursor = ::LoadImage(module, "zoomin_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorZoomOut:
      cursor = ::LoadImage(module, "zoomout_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorMove:
      cursor = ::LoadImage(module, "handopen_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorNSEWScroll:
      cursor = ::LoadImage(module, "scrollnsew_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorNSScroll:
      cursor = ::LoadImage(module, "scrollns_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorEWScroll:
      cursor = ::LoadImage(module, "scrollew_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorHelp:
      cursor = ::LoadImage(nullptr, IDC_HELP, IMAGE_CURSOR, cx, cy, flags);
      break; /* Arrow and question mark */
    case GHOST_kStandardCursorWait:
      cursor = ::LoadImage(nullptr, IDC_WAIT, IMAGE_CURSOR, cx, cy, flags);
      break; /* Hourglass */
    case GHOST_kStandardCursorText:
      cursor = ::LoadImage(nullptr, IDC_IBEAM, IMAGE_CURSOR, cx, cy, flags);
      break; /* I-beam */
    case GHOST_kStandardCursorCrosshair:
      cursor = ::LoadImage(module, "cross_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Standard Cross */
    case GHOST_kStandardCursorCrosshairA:
      cursor = ::LoadImage(module, "crossA_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Crosshair A */
    case GHOST_kStandardCursorCrosshairB:
      cursor = ::LoadImage(module, "crossB_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Diagonal Crosshair B */
    case GHOST_kStandardCursorCrosshairC:
      cursor = ::LoadImage(module, "crossC_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Minimal Crosshair C */
    case GHOST_kStandardCursorBottomSide:
    case GHOST_kStandardCursorUpDown:
      cursor = ::LoadImage(module, "movens_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Double-pointed arrow pointing north and south */
    case GHOST_kStandardCursorLeftSide:
    case GHOST_kStandardCursorLeftRight:
      cursor = ::LoadImage(module, "moveew_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Double-pointed arrow pointing west and east */
    case GHOST_kStandardCursorTopSide:
      cursor = ::LoadImage(nullptr, IDC_UPARROW, IMAGE_CURSOR, cx, cy, flags);
      break; /* Vertical arrow */
    case GHOST_kStandardCursorTopLeftCorner:
      cursor = ::LoadImage(nullptr, IDC_SIZENWSE, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorTopRightCorner:
      cursor = ::LoadImage(nullptr, IDC_SIZENESW, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorBottomRightCorner:
      cursor = ::LoadImage(nullptr, IDC_SIZENWSE, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorBottomLeftCorner:
      cursor = ::LoadImage(nullptr, IDC_SIZENESW, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorPencil:
      cursor = ::LoadImage(module, "pencil_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorEraser:
      cursor = ::LoadImage(module, "eraser_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorDestroy:
    case GHOST_kStandardCursorStop:
      cursor = ::LoadImage(module, "forbidden_cursor", IMAGE_CURSOR, cx, cy, flags);
      break; /* Slashed circle */
    case GHOST_kStandardCursorDefault:
      cursor = nullptr;
      break;
    default:
      return nullptr;
  }

  if (cursor == nullptr) {
    cursor = ::LoadImage(nullptr, IDC_ARROW, IMAGE_CURSOR, cx, cy, flags);
  }

  return (HCURSOR)cursor;
}

void GHOST_WindowWin32::loadCursor(bool visible, GHOST_TStandardCursor shape) const
{
  if (!visible) {
    while (::ShowCursor(FALSE) >= 0) {
      /* Pass. */
    }
  }
  else {
    while (::ShowCursor(TRUE) < 0) {
      /* Pass. */
    }
  }

  HCURSOR cursor = getStandardCursor(shape);
  if (cursor == nullptr) {
    cursor = getStandardCursor(GHOST_kStandardCursorDefault);
  }
  ::SetCursor(cursor);
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorVisibility(bool visible)
{
  if (::GetForegroundWindow() == m_hWnd) {
    loadCursor(visible, getCursorShape());
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  if (mode != GHOST_kGrabDisable) {
    if (mode != GHOST_kGrabNormal) {
      m_system->getCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setCursorGrabAccum(0, 0);

      if (mode == GHOST_kGrabHide) {
        setWindowCursorVisibility(false);
      }
    }
    updateMouseCapture(OperatorGrab);
  }
  else {
    if (m_cursorGrab == GHOST_kGrabHide) {
      m_system->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setWindowCursorVisibility(true);
    }
    if (m_cursorGrab != GHOST_kGrabNormal) {
      /* Use to generate a mouse move event, otherwise the last event
       * blender gets can be outside the screen causing menus not to show
       * properly unless the user moves the mouse. */
      int32_t pos[2];
      m_system->getCursorPosition(pos[0], pos[1]);
      m_system->setCursorPosition(pos[0], pos[1]);
    }

    /* Almost works without but important otherwise the mouse GHOST location
     * can be incorrect on exit. */
    setCursorGrabAccum(0, 0);
    m_cursorGrabBounds.m_l = m_cursorGrabBounds.m_r = -1; /* disable */
    updateMouseCapture(OperatorUngrab);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorShape(GHOST_TStandardCursor cursorShape)
{
  if (::GetForegroundWindow() == m_hWnd) {
    loadCursor(getCursorVisibility(), cursorShape);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::hasCursorShape(GHOST_TStandardCursor cursorShape)
{
  return (getStandardCursor(cursorShape)) ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::getPointerInfo(
    std::vector<GHOST_PointerInfoWin32> &outPointerInfo, WPARAM wParam, LPARAM /*lParam*/)
{
  int32_t pointerId = GET_POINTERID_WPARAM(wParam);
  int32_t isPrimary = IS_POINTER_PRIMARY_WPARAM(wParam);
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)GHOST_System::getSystem();
  uint32_t outCount = 0;

  if (!(GetPointerPenInfoHistory(pointerId, &outCount, nullptr))) {
    return GHOST_kFailure;
  }

  std::vector<POINTER_PEN_INFO> pointerPenInfo(outCount);
  outPointerInfo.resize(outCount);

  if (!(GetPointerPenInfoHistory(pointerId, &outCount, pointerPenInfo.data()))) {
    return GHOST_kFailure;
  }

  for (uint32_t i = 0; i < outCount; i++) {
    POINTER_INFO pointerApiInfo = pointerPenInfo[i].pointerInfo;
    /* Obtain the basic information from the event. */
    outPointerInfo[i].pointerId = pointerId;
    outPointerInfo[i].isPrimary = isPrimary;

    switch (pointerApiInfo.ButtonChangeType) {
      case POINTER_CHANGE_FIRSTBUTTON_DOWN:
      case POINTER_CHANGE_FIRSTBUTTON_UP:
        outPointerInfo[i].buttonMask = GHOST_kButtonMaskLeft;
        break;
      case POINTER_CHANGE_SECONDBUTTON_DOWN:
      case POINTER_CHANGE_SECONDBUTTON_UP:
        outPointerInfo[i].buttonMask = GHOST_kButtonMaskRight;
        break;
      case POINTER_CHANGE_THIRDBUTTON_DOWN:
      case POINTER_CHANGE_THIRDBUTTON_UP:
        outPointerInfo[i].buttonMask = GHOST_kButtonMaskMiddle;
        break;
      case POINTER_CHANGE_FOURTHBUTTON_DOWN:
      case POINTER_CHANGE_FOURTHBUTTON_UP:
        outPointerInfo[i].buttonMask = GHOST_kButtonMaskButton4;
        break;
      case POINTER_CHANGE_FIFTHBUTTON_DOWN:
      case POINTER_CHANGE_FIFTHBUTTON_UP:
        outPointerInfo[i].buttonMask = GHOST_kButtonMaskButton5;
        break;
      default:
        break;
    }

    outPointerInfo[i].pixelLocation = pointerApiInfo.ptPixelLocation;
    outPointerInfo[i].tabletData.Active = GHOST_kTabletModeStylus;
    outPointerInfo[i].tabletData.Pressure = 1.0f;
    outPointerInfo[i].tabletData.Xtilt = 0.0f;
    outPointerInfo[i].tabletData.Ytilt = 0.0f;
    outPointerInfo[i].time = system->performanceCounterToMillis(pointerApiInfo.PerformanceCount);

    if (pointerPenInfo[i].penMask & PEN_MASK_PRESSURE) {
      outPointerInfo[i].tabletData.Pressure = pointerPenInfo[i].pressure / 1024.0f;
    }

    if (pointerPenInfo[i].penFlags & PEN_FLAG_ERASER) {
      outPointerInfo[i].tabletData.Active = GHOST_kTabletModeEraser;
    }

    if (pointerPenInfo[i].penMask & PEN_MASK_TILT_X) {
      outPointerInfo[i].tabletData.Xtilt = fmin(fabs(pointerPenInfo[i].tiltX / 90.0f), 1.0f);
    }

    if (pointerPenInfo[i].penMask & PEN_MASK_TILT_Y) {
      outPointerInfo[i].tabletData.Ytilt = fmin(fabs(pointerPenInfo[i].tiltY / 90.0f), 1.0f);
    }
  }

  if (!outPointerInfo.empty()) {
    m_lastPointerTabletData = outPointerInfo.back().tabletData;
  }

  return GHOST_kSuccess;
}

void GHOST_WindowWin32::resetPointerPenInfo()
{
  m_lastPointerTabletData = GHOST_TABLET_DATA_NONE;
}

GHOST_Wintab *GHOST_WindowWin32::getWintab() const
{
  return m_wintab;
}

void GHOST_WindowWin32::loadWintab(bool enable)
{
  if (!m_wintab) {
    WINTAB_PRINTF("Loading Wintab for window %p\n", m_hWnd);
    if (m_wintab = GHOST_Wintab::loadWintab(m_hWnd)) {
      if (enable) {
        m_wintab->enable();

        /* Focus Wintab if cursor is inside this window. This ensures Wintab is enabled when the
         * tablet is used to change the Tablet API. */
        int32_t x, y;
        if (m_system->getCursorPosition(x, y)) {
          GHOST_Rect rect;
          getClientBounds(rect);

          if (rect.isInside(x, y)) {
            m_wintab->gainFocus();
          }
        }
      }
    }
  }
}

void GHOST_WindowWin32::closeWintab()
{
  WINTAB_PRINTF("Closing Wintab for window %p\n", m_hWnd);
  delete m_wintab;
  m_wintab = nullptr;
}

bool GHOST_WindowWin32::usingTabletAPI(GHOST_TTabletAPI api) const
{
  if (m_system->getTabletAPI() == api) {
    return true;
  }
  else if (m_system->getTabletAPI() == GHOST_kTabletAutomatic) {
    if (m_wintab && m_wintab->devicesPresent()) {
      return api == GHOST_kTabletWintab;
    }
    else {
      return api == GHOST_kTabletWinPointer;
    }
  }
  else {
    return false;
  }
}

GHOST_TabletData GHOST_WindowWin32::getTabletData()
{
  if (usingTabletAPI(GHOST_kTabletWintab)) {
    return m_wintab ? m_wintab->getLastTabletData() : GHOST_TABLET_DATA_NONE;
  }
  else {
    return m_lastPointerTabletData;
  }
}

void GHOST_WindowWin32::ThemeRefresh()
{
  DWORD lightMode;
  DWORD pcbData = sizeof(lightMode);
  if (RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\",
                   L"AppsUseLightTheme",
                   RRF_RT_REG_DWORD,
                   nullptr,
                   &lightMode,
                   &pcbData) == ERROR_SUCCESS)
  {
    BOOL DarkMode = !lightMode;

    /* `20 == DWMWA_USE_IMMERSIVE_DARK_MODE` in Windows 11 SDK.
     * This value was undocumented for Windows 10 versions 2004 and later,
     * supported for Windows 11 Build 22000 and later. */
    DwmSetWindowAttribute(this->m_hWnd, 20, &DarkMode, sizeof(DarkMode));
  }
}

void GHOST_WindowWin32::updateDPI()
{
  if (m_directManipulationHelper) {
    m_directManipulationHelper->setDPI(getDPIHint());
  }
}

uint16_t GHOST_WindowWin32::getDPIHint()
{
  if (m_user32) {
    GHOST_WIN32_GetDpiForWindow fpGetDpiForWindow = (GHOST_WIN32_GetDpiForWindow)::GetProcAddress(
        m_user32, "GetDpiForWindow");

    if (fpGetDpiForWindow) {
      return fpGetDpiForWindow(this->m_hWnd);
    }
  }

  return USER_DEFAULT_SCREEN_DPI;
}

/** Reverse the bits in a uint8_t */
static uint8_t uns8ReverseBits(uint8_t ch)
{
  ch = ((ch >> 1) & 0x55) | ((ch << 1) & 0xAA);
  ch = ((ch >> 2) & 0x33) | ((ch << 2) & 0xCC);
  ch = ((ch >> 4) & 0x0F) | ((ch << 4) & 0xF0);
  return ch;
}

#if 0 /* UNUSED */
/** Reverse the bits in a uint16_t */
static uint16_t uns16ReverseBits(uint16_t shrt)
{
  shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
  shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
  shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
  shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
  return shrt;
}
#endif

GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(uint8_t *bitmap,
                                                             uint8_t *mask,
                                                             int sizeX,
                                                             int sizeY,
                                                             int hotX,
                                                             int hotY,
                                                             bool /*canInvertColor*/)
{
  uint32_t andData[32];
  uint32_t xorData[32];
  uint32_t fullBitRow, fullMaskRow;
  int x, y, cols;

  cols = sizeX / 8; /* Number of whole bytes per row (width of bitmap/mask). */
  if (sizeX % 8) {
    cols++;
  }

  if (m_customCursor) {
    DestroyCursor(m_customCursor);
    m_customCursor = nullptr;
  }

  memset(&andData, 0xFF, sizeof(andData));
  memset(&xorData, 0, sizeof(xorData));

  for (y = 0; y < sizeY; y++) {
    fullBitRow = 0;
    fullMaskRow = 0;
    for (x = cols - 1; x >= 0; x--) {
      fullBitRow <<= 8;
      fullMaskRow <<= 8;
      fullBitRow |= uns8ReverseBits(bitmap[cols * y + x]);
      fullMaskRow |= uns8ReverseBits(mask[cols * y + x]);
    }
    xorData[y] = fullBitRow & fullMaskRow;
    andData[y] = ~fullMaskRow;
  }

  m_customCursor = ::CreateCursor(::GetModuleHandle(0), hotX, hotY, 32, 32, andData, xorData);
  if (!m_customCursor) {
    return GHOST_kFailure;
  }

  if (::GetForegroundWindow() == m_hWnd) {
    loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setProgressBar(float progress)
{
  /* #SetProgressValue sets state to #TBPF_NORMAL automatically. */
  if (m_Bar && S_OK == m_Bar->SetProgressValue(m_hWnd, 10000 * progress, 10000)) {
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::endProgressBar()
{
  if (m_Bar && S_OK == m_Bar->SetProgressState(m_hWnd, TBPF_NOPROGRESS)) {
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

#ifdef WITH_INPUT_IME
void GHOST_WindowWin32::beginIME(int32_t x, int32_t y, int32_t /*w*/, int32_t h, bool completed)
{
  m_imeInput.BeginIME(m_hWnd, GHOST_Rect(x, y - h, x, y), completed);
}

void GHOST_WindowWin32::endIME()
{
  m_imeInput.EndIME(m_hWnd);
}
#endif /* WITH_INPUT_IME */

void GHOST_WindowWin32::registerWindowAppUserModelProperties()
{
  IPropertyStore *pstore;
  char blender_path[MAX_PATH];
  wchar_t shell_command[MAX_PATH];

  /* Find the current executable, and see if it's blender.exe if not bail out. */
  GetModuleFileName(0, blender_path, sizeof(blender_path));
  char *blender_app = strstr(blender_path, "blender.exe");
  if (!blender_app) {
    return;
  }

  HRESULT hr = SHGetPropertyStoreForWindow(m_hWnd, IID_PPV_ARGS(&pstore));
  if (!SUCCEEDED(hr)) {
    return;
  }

  /* Set the launcher as the shell command so the console window will not flash.
   * when people pin blender to the taskbar. */
  strcpy(blender_app, "blender-launcher.exe");
  wsprintfW(shell_command, L"\"%S\"", blender_path);
  UTF16_ENCODE(BLENDER_WIN_APPID);
  UTF16_ENCODE(BLENDER_WIN_APPID_FRIENDLY_NAME);
  PROPVARIANT propvar;
  hr = InitPropVariantFromString(BLENDER_WIN_APPID_16, &propvar);
  hr = pstore->SetValue(PKEY_AppUserModel_ID, propvar);
  hr = InitPropVariantFromString(shell_command, &propvar);
  hr = pstore->SetValue(PKEY_AppUserModel_RelaunchCommand, propvar);
  hr = InitPropVariantFromString(BLENDER_WIN_APPID_FRIENDLY_NAME_16, &propvar);
  hr = pstore->SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource, propvar);
  pstore->Release();
  UTF16_UN_ENCODE(BLENDER_WIN_APPID_FRIENDLY_NAME);
  UTF16_UN_ENCODE(BLENDER_WIN_APPID);
}

/* as per MSDN: Any property not cleared before closing the window, will be leaked and NOT be
 * returned to the OS. */
void GHOST_WindowWin32::unregisterWindowAppUserModelProperties()
{
  IPropertyStore *pstore;
  HRESULT hr = SHGetPropertyStoreForWindow(m_hWnd, IID_PPV_ARGS(&pstore));
  if (SUCCEEDED(hr)) {
    PROPVARIANT value;
    PropVariantInit(&value);
    pstore->SetValue(PKEY_AppUserModel_ID, value);
    pstore->SetValue(PKEY_AppUserModel_RelaunchCommand, value);
    pstore->SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource, value);
    pstore->Release();
  }
}
