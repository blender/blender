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

#define _USE_MATH_DEFINES

#include "GHOST_WindowWin32.h"
#include "GHOST_ContextD3D.h"
#include "GHOST_ContextNone.h"
#include "GHOST_DropTargetWin32.h"
#include "GHOST_SystemWin32.h"
#include "GHOST_WindowManager.h"
#include "utf_winfunc.h"
#include "utfconv.h"

#if defined(WITH_GL_EGL)
#  include "GHOST_ContextEGL.h"
#else
#  include "GHOST_ContextWGL.h"
#endif
#ifdef WIN32_COMPOSITING
#  include <Dwmapi.h>
#endif

#include <assert.h>
#include <math.h>
#include <string.h>
#include <windowsx.h>

#ifndef GET_POINTERID_WPARAM
#  define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))
#endif  // GET_POINTERID_WPARAM

const wchar_t *GHOST_WindowWin32::s_windowClassName = L"GHOST_WindowClass";
const int GHOST_WindowWin32::s_maxTitleLength = 128;

/* force NVidia Optimus to used dedicated graphics */
extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

GHOST_WindowWin32::GHOST_WindowWin32(GHOST_SystemWin32 *system,
                                     const char *title,
                                     GHOST_TInt32 left,
                                     GHOST_TInt32 top,
                                     GHOST_TUns32 width,
                                     GHOST_TUns32 height,
                                     GHOST_TWindowState state,
                                     GHOST_TDrawingContextType type,
                                     bool wantStereoVisual,
                                     bool alphaBackground,
                                     GHOST_WindowWin32 *parentwindow,
                                     bool is_debug,
                                     bool dialog)
    : GHOST_Window(width, height, state, wantStereoVisual, false),
      m_tabletInRange(false),
      m_inLiveResize(false),
      m_system(system),
      m_hDC(0),
      m_isDialog(dialog),
      m_hasMouseCaptured(false),
      m_hasGrabMouse(false),
      m_nPressedButtons(0),
      m_customCursor(0),
      m_wantAlphaBackground(alphaBackground),
      m_normal_state(GHOST_kWindowStateNormal),
      m_user32(NULL),
      m_fpGetPointerInfoHistory(NULL),
      m_fpGetPointerPenInfoHistory(NULL),
      m_fpGetPointerTouchInfoHistory(NULL),
      m_parentWindowHwnd(parentwindow ? parentwindow->m_hWnd : HWND_DESKTOP),
      m_debug_context(is_debug)
{
  wchar_t *title_16 = alloc_utf16_from_8((char *)title, 0);
  RECT win_rect = {left, top, (long)(left + width), (long)(top + height)};
  RECT parent_rect = {0, 0, 0, 0};

  // Initialize tablet variables
  memset(&m_wintab, 0, sizeof(m_wintab));
  m_tabletData = GHOST_TABLET_DATA_NONE;

  if (parentwindow) {
    GetWindowRect(m_parentWindowHwnd, &parent_rect);
  }

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

  /* Monitor details. */
  MONITORINFOEX monitor;
  monitor.cbSize = sizeof(MONITORINFOEX);
  monitor.dwFlags = 0;
  GetMonitorInfo(
      MonitorFromRect(parentwindow ? &parent_rect : &win_rect, MONITOR_DEFAULTTONEAREST),
      &monitor);

  /* Adjust our requested size to allow for caption and borders and constrain to monitor. */
  AdjustWindowRectEx(&win_rect, WS_CAPTION, FALSE, 0);
  width = min(monitor.rcWork.right - monitor.rcWork.left, win_rect.right - win_rect.left);
  left = min(max(monitor.rcWork.left, win_rect.left), monitor.rcWork.right - width);
  height = min(monitor.rcWork.bottom - monitor.rcWork.top, win_rect.bottom - win_rect.top);
  top = min(max(monitor.rcWork.top, win_rect.top), monitor.rcWork.bottom - height);

  m_hWnd = ::CreateWindowExW(extended_style,        // window extended style
                             s_windowClassName,     // pointer to registered class name
                             title_16,              // pointer to window name
                             style,                 // window style
                             left,                  // horizontal position of window
                             top,                   // vertical position of window
                             width,                 // window width
                             height,                // window height
                             m_parentWindowHwnd,    // handle to parent or owner window
                             0,                     // handle to menu or child-window identifier
                             ::GetModuleHandle(0),  // handle to application instance
                             0);                    // pointer to window-creation data
  free(title_16);

  m_user32 = ::LoadLibrary("user32.dll");

  if (m_hWnd) {
    if (m_user32) {
      // Touch enabled screens with pen support by default have gestures
      // enabled, which results in a delay between the pointer down event
      // and the first move when using the stylus. RegisterTouchWindow
      // disables the new gesture architecture enabling the events to be
      // sent immediately to the application rather than being absorbed by
      // the gesture API.
      GHOST_WIN32_RegisterTouchWindow pRegisterTouchWindow = (GHOST_WIN32_RegisterTouchWindow)
          GetProcAddress(m_user32, "RegisterTouchWindow");
      if (pRegisterTouchWindow) {
        pRegisterTouchWindow(m_hWnd, 0);
      }
    }

    // Register this window as a droptarget. Requires m_hWnd to be valid.
    // Note that OleInitialize(0) has to be called prior to this. Done in GHOST_SystemWin32.
    m_dropTarget = new GHOST_DropTargetWin32(this, m_system);
    if (m_dropTarget) {
      ::RegisterDragDrop(m_hWnd, m_dropTarget);
    }

    // Store a pointer to this class in the window structure
    ::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

    if (!m_system->m_windowFocus) {
      // Lower to bottom and don't activate if we don't want focus
      ::SetWindowPos(m_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    // Store the device context
    m_hDC = ::GetDC(m_hWnd);

    GHOST_TSuccess success = setDrawingContextType(type);

    if (success) {
      // Show the window
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

      ::ShowWindow(m_hWnd, nCmdShow);
#ifdef WIN32_COMPOSITING
      if (alphaBackground && parentwindowhwnd == 0) {

        HRESULT hr = S_OK;

        // Create and populate the Blur Behind structure
        DWM_BLURBEHIND bb = {0};

        // Enable Blur Behind and apply to the entire client area
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = true;
        bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);

        // Apply Blur Behind
        hr = DwmEnableBlurBehindWindow(m_hWnd, &bb);
        DeleteObject(bb.hRgnBlur);
      }
#endif
      // Force an initial paint of the window
      ::UpdateWindow(m_hWnd);
    }
    else {
      // invalidate the window
      ::DestroyWindow(m_hWnd);
      m_hWnd = NULL;
    }
  }

  // Initialize Windows Ink
  if (m_user32) {
    m_fpGetPointerInfoHistory = (GHOST_WIN32_GetPointerInfoHistory)::GetProcAddress(
        m_user32, "GetPointerInfoHistory");
    m_fpGetPointerPenInfoHistory = (GHOST_WIN32_GetPointerPenInfoHistory)::GetProcAddress(
        m_user32, "GetPointerPenInfoHistory");
    m_fpGetPointerTouchInfoHistory = (GHOST_WIN32_GetPointerTouchInfoHistory)::GetProcAddress(
        m_user32, "GetPointerTouchInfoHistory");
  }

  // Initialize Wintab
  m_wintab.handle = ::LoadLibrary("Wintab32.dll");
  if (m_wintab.handle && m_system->getTabletAPI() != GHOST_kTabletNative) {
    // Get API functions
    m_wintab.info = (GHOST_WIN32_WTInfo)::GetProcAddress(m_wintab.handle, "WTInfoA");
    m_wintab.open = (GHOST_WIN32_WTOpen)::GetProcAddress(m_wintab.handle, "WTOpenA");
    m_wintab.close = (GHOST_WIN32_WTClose)::GetProcAddress(m_wintab.handle, "WTClose");
    m_wintab.packet = (GHOST_WIN32_WTPacket)::GetProcAddress(m_wintab.handle, "WTPacket");
    m_wintab.enable = (GHOST_WIN32_WTEnable)::GetProcAddress(m_wintab.handle, "WTEnable");
    m_wintab.overlap = (GHOST_WIN32_WTOverlap)::GetProcAddress(m_wintab.handle, "WTOverlap");

    // Let's see if we can initialize tablet here.
    // Check if WinTab available by getting system context info.
    LOGCONTEXT lc = {0};
    lc.lcOptions |= CXO_SYSTEM;
    if (m_wintab.open && m_wintab.info && m_wintab.info(WTI_DEFSYSCTX, 0, &lc)) {
      // Now init the tablet
      /* The maximum tablet size, pressure and orientation (tilt) */
      AXIS TabletX, TabletY, Pressure, Orientation[3];

      // Open a Wintab context

      // Open the context
      lc.lcPktData = PACKETDATA;
      lc.lcPktMode = PACKETMODE;
      lc.lcOptions |= CXO_MESSAGES;
      lc.lcMoveMask = PACKETDATA;

      /* Set the entire tablet as active */
      m_wintab.info(WTI_DEVICES, DVC_X, &TabletX);
      m_wintab.info(WTI_DEVICES, DVC_Y, &TabletY);

      /* get the max pressure, to divide into a float */
      BOOL pressureSupport = m_wintab.info(WTI_DEVICES, DVC_NPRESSURE, &Pressure);
      if (pressureSupport)
        m_wintab.maxPressure = Pressure.axMax;
      else
        m_wintab.maxPressure = 0;

      /* get the max tilt axes, to divide into floats */
      BOOL tiltSupport = m_wintab.info(WTI_DEVICES, DVC_ORIENTATION, &Orientation);
      if (tiltSupport) {
        /* does the tablet support azimuth ([0]) and altitude ([1]) */
        if (Orientation[0].axResolution && Orientation[1].axResolution) {
          /* all this assumes the minimum is 0 */
          m_wintab.maxAzimuth = Orientation[0].axMax;
          m_wintab.maxAltitude = Orientation[1].axMax;
        }
        else { /* no so dont do tilt stuff */
          m_wintab.maxAzimuth = m_wintab.maxAltitude = 0;
        }
      }

      // The Wintab spec says we must open the context disabled if we are using cursor masks.
      m_wintab.tablet = m_wintab.open(m_hWnd, &lc, FALSE);
      if (m_wintab.enable && m_wintab.tablet) {
        m_wintab.enable(m_wintab.tablet, TRUE);
      }

      m_wintab.info(WTI_INTERFACE, IFC_NDEVICES, &m_wintab.numDevices);
    }
  }
  CoCreateInstance(
      CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (LPVOID *)&m_Bar);
}

GHOST_WindowWin32::~GHOST_WindowWin32()
{
  if (m_Bar) {
    m_Bar->SetProgressState(m_hWnd, TBPF_NOPROGRESS);
    m_Bar->Release();
    m_Bar = NULL;
  }

  if (m_wintab.handle) {
    if (m_wintab.close && m_wintab.tablet) {
      m_wintab.close(m_wintab.tablet);
    }

    FreeLibrary(m_wintab.handle);
    memset(&m_wintab, 0, sizeof(m_wintab));
  }

  if (m_user32) {
    FreeLibrary(m_user32);
    m_user32 = NULL;
    m_fpGetPointerInfoHistory = NULL;
    m_fpGetPointerPenInfoHistory = NULL;
    m_fpGetPointerTouchInfoHistory = NULL;
  }

  if (m_customCursor) {
    DestroyCursor(m_customCursor);
    m_customCursor = NULL;
  }

  if (m_hWnd != NULL && m_hDC != NULL && releaseNativeHandles()) {
    ::ReleaseDC(m_hWnd, m_hDC);
    m_hDC = NULL;
  }

  if (m_hWnd) {
    /* If this window is referenced by others as parent, clear that relation or windows will free
     * the handle while we still reference it. */
    for (GHOST_IWindow *iter_win : m_system->getWindowManager()->getWindows()) {
      GHOST_WindowWin32 *iter_winwin = (GHOST_WindowWin32 *)iter_win;
      if (iter_winwin->m_parentWindowHwnd == m_hWnd) {
        ::SetWindowLongPtr(iter_winwin->m_hWnd, GWLP_HWNDPARENT, NULL);
        iter_winwin->m_parentWindowHwnd = 0;
      }
    }

    if (m_dropTarget) {
      // Disable DragDrop
      RevokeDragDrop(m_hWnd);
      // Release our reference of the DropTarget and it will delete itself eventually.
      m_dropTarget->Release();
      m_dropTarget = NULL;
    }
    ::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, NULL);
    ::DestroyWindow(m_hWnd);
    m_hWnd = 0;
  }
}

bool GHOST_WindowWin32::getValid() const
{
  return GHOST_Window::getValid() && m_hWnd != 0 && m_hDC != 0;
}

HWND GHOST_WindowWin32::getHWND() const
{
  return m_hWnd;
}

void GHOST_WindowWin32::setTitle(const char *title)
{
  wchar_t *title_16 = alloc_utf16_from_8((char *)title, 0);
  ::SetWindowTextW(m_hWnd, (wchar_t *)title_16);
  free(title_16);
}

std::string GHOST_WindowWin32::getTitle() const
{
  char buf[s_maxTitleLength]; /*CHANGE + never used yet*/
  ::GetWindowText(m_hWnd, buf, s_maxTitleLength);
  return std::string(buf);
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

GHOST_TSuccess GHOST_WindowWin32::setClientWidth(GHOST_TUns32 width)
{
  GHOST_TSuccess success;
  GHOST_Rect cBnds, wBnds;
  getClientBounds(cBnds);
  if (cBnds.getWidth() != (GHOST_TInt32)width) {
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

GHOST_TSuccess GHOST_WindowWin32::setClientHeight(GHOST_TUns32 height)
{
  GHOST_TSuccess success;
  GHOST_Rect cBnds, wBnds;
  getClientBounds(cBnds);
  if (cBnds.getHeight() != (GHOST_TInt32)height) {
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

GHOST_TSuccess GHOST_WindowWin32::setClientSize(GHOST_TUns32 width, GHOST_TUns32 height)
{
  GHOST_TSuccess success;
  GHOST_Rect cBnds, wBnds;
  getClientBounds(cBnds);
  if ((cBnds.getWidth() != (GHOST_TInt32)width) || (cBnds.getHeight() != (GHOST_TInt32)height)) {
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

void GHOST_WindowWin32::screenToClient(GHOST_TInt32 inX,
                                       GHOST_TInt32 inY,
                                       GHOST_TInt32 &outX,
                                       GHOST_TInt32 &outY) const
{
  POINT point = {inX, inY};
  ::ScreenToClient(m_hWnd, &point);
  outX = point.x;
  outY = point.y;
}

void GHOST_WindowWin32::clientToScreen(GHOST_TInt32 inX,
                                       GHOST_TInt32 inY,
                                       GHOST_TInt32 &outX,
                                       GHOST_TInt32 &outY) const
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
      wp.showCmd = SW_SHOWMINIMIZED;
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
      style &= ~WS_CAPTION;
      break;
    case GHOST_kWindowStateNormal:
    default:
      wp.showCmd = SW_SHOWNORMAL;
      break;
  }
  ::SetWindowLongPtr(m_hWnd, GWL_STYLE, style);
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
    hWndToRaise = NULL;
  }

  if (::SetWindowPos(m_hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE) {
    return GHOST_kFailure;
  }

  if (hWndToRaise &&
      ::SetWindowPos(hWndToRaise, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE) {
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
  if (type == GHOST_kDrawingContextTypeOpenGL) {
    GHOST_Context *context;

#if defined(WITH_GL_PROFILE_CORE)
    /* - AMD and Intel give us exactly this version
     * - NVIDIA gives at least this version <-- desired behavior
     * So we ask for 4.5, 4.4 ... 3.3 in descending order
     * to get the best version on the user's system. */
    for (int minor = 5; minor >= 0; --minor) {
      context = new GHOST_ContextWGL(m_wantStereoVisual,
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
      else {
        delete context;
      }
    }
    context = new GHOST_ContextWGL(m_wantStereoVisual,
                                   m_wantAlphaBackground,
                                   m_hWnd,
                                   m_hDC,
                                   WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                                   3,
                                   3,
                                   (m_debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
                                   GHOST_OPENGL_WGL_RESET_NOTIFICATION_STRATEGY);

    if (context->initializeDrawingContext()) {
      return context;
    }
    else {
      MessageBox(m_hWnd,
                 "A graphics card and driver with support for OpenGL 3.3 or higher is required.\n"
                 "Installing the latest driver for your graphics card may resolve the issue.\n\n"
                 "The program will now close.",
                 "Blender - Unsupported Graphics Card or Driver",
                 MB_OK | MB_ICONERROR);
      delete context;
      exit(0);
    }

#elif defined(WITH_GL_PROFILE_COMPAT)
    // ask for 2.1 context, driver gives any GL version >= 2.1
    // (hopefully the latest compatibility profile)
    // 2.1 ignores the profile bit & is incompatible with core profile
    context = new GHOST_ContextWGL(m_wantStereoVisual,
                                   m_wantAlphaBackground,
                                   m_hWnd,
                                   m_hDC,
                                   0,  // no profile bit
                                   2,
                                   1,
                                   (m_debug_context ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
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
  }
  else if (type == GHOST_kDrawingContextTypeD3D) {
    GHOST_Context *context;

    context = new GHOST_ContextD3D(false, m_hWnd);
    if (context->initializeDrawingContext()) {
      return context;
    }
    else {
      delete context;
    }

    return context;
  }

  return NULL;
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
      if (m_nPressedButtons)
        m_nPressedButtons--;
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
  // Convert GHOST cursor to Windows OEM cursor
  HANDLE cursor = NULL;
  HMODULE module = ::GetModuleHandle(0);
  GHOST_TUns32 flags = LR_SHARED | LR_DEFAULTSIZE;
  int cx = 0, cy = 0;

  switch (shape) {
    case GHOST_kStandardCursorCustom:
      if (m_customCursor) {
        return m_customCursor;
      }
      else {
        return NULL;
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
      cursor = ::LoadImage(NULL, IDC_HELP, IMAGE_CURSOR, cx, cy, flags);
      break;  // Arrow and question mark
    case GHOST_kStandardCursorWait:
      cursor = ::LoadImage(NULL, IDC_WAIT, IMAGE_CURSOR, cx, cy, flags);
      break;  // Hourglass
    case GHOST_kStandardCursorText:
      cursor = ::LoadImage(NULL, IDC_IBEAM, IMAGE_CURSOR, cx, cy, flags);
      break;  // I-beam
    case GHOST_kStandardCursorCrosshair:
      cursor = ::LoadImage(module, "cross_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;  // Standard Cross
    case GHOST_kStandardCursorCrosshairA:
      cursor = ::LoadImage(module, "crossA_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;  // Crosshair A
    case GHOST_kStandardCursorCrosshairB:
      cursor = ::LoadImage(module, "crossB_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;  // Diagonal Crosshair B
    case GHOST_kStandardCursorCrosshairC:
      cursor = ::LoadImage(module, "crossC_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;  // Minimal Crosshair C
    case GHOST_kStandardCursorBottomSide:
    case GHOST_kStandardCursorUpDown:
      cursor = ::LoadImage(module, "movens_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;  // Double-pointed arrow pointing north and south
    case GHOST_kStandardCursorLeftSide:
    case GHOST_kStandardCursorLeftRight:
      cursor = ::LoadImage(module, "moveew_cursor", IMAGE_CURSOR, cx, cy, flags);
      break;  // Double-pointed arrow pointing west and east
    case GHOST_kStandardCursorTopSide:
      cursor = ::LoadImage(NULL, IDC_UPARROW, IMAGE_CURSOR, cx, cy, flags);
      break;  // Vertical arrow
    case GHOST_kStandardCursorTopLeftCorner:
      cursor = ::LoadImage(NULL, IDC_SIZENWSE, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorTopRightCorner:
      cursor = ::LoadImage(NULL, IDC_SIZENESW, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorBottomRightCorner:
      cursor = ::LoadImage(NULL, IDC_SIZENWSE, IMAGE_CURSOR, cx, cy, flags);
      break;
    case GHOST_kStandardCursorBottomLeftCorner:
      cursor = ::LoadImage(NULL, IDC_SIZENESW, IMAGE_CURSOR, cx, cy, flags);
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
      break;  // Slashed circle
    case GHOST_kStandardCursorDefault:
      cursor = NULL;
      break;
    default:
      return NULL;
  }

  if (cursor == NULL) {
    cursor = ::LoadImage(NULL, IDC_ARROW, IMAGE_CURSOR, cx, cy, flags);
  }

  return (HCURSOR)cursor;
}

void GHOST_WindowWin32::loadCursor(bool visible, GHOST_TStandardCursor shape) const
{
  if (!visible) {
    while (::ShowCursor(FALSE) >= 0)
      ;
  }
  else {
    while (::ShowCursor(TRUE) < 0)
      ;
  }

  HCURSOR cursor = getStandardCursor(shape);
  if (cursor == NULL) {
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

      if (mode == GHOST_kGrabHide)
        setWindowCursorVisibility(false);
    }
    updateMouseCapture(OperatorGrab);
  }
  else {
    if (m_cursorGrab == GHOST_kGrabHide) {
      m_system->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setWindowCursorVisibility(true);
    }
    if (m_cursorGrab != GHOST_kGrabNormal) {
      /* use to generate a mouse move event, otherwise the last event
       * blender gets can be outside the screen causing menus not to show
       * properly unless the user moves the mouse */
      GHOST_TInt32 pos[2];
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
    std::vector<GHOST_PointerInfoWin32> &outPointerInfo, WPARAM wParam, LPARAM lParam)
{
  if (!useTabletAPI(GHOST_kTabletNative)) {
    return GHOST_kFailure;
  }

  GHOST_TInt32 pointerId = GET_POINTERID_WPARAM(wParam);
  GHOST_TInt32 isPrimary = IS_POINTER_PRIMARY_WPARAM(wParam);
  GHOST_SystemWin32 *system = (GHOST_SystemWin32 *)GHOST_System::getSystem();
  GHOST_TUns32 outCount;

  if (!(m_fpGetPointerInfoHistory && m_fpGetPointerInfoHistory(pointerId, &outCount, NULL))) {
    return GHOST_kFailure;
  }

  auto pointerPenInfo = std::vector<POINTER_PEN_INFO>(outCount);
  outPointerInfo.resize(outCount);

  if (!(m_fpGetPointerPenInfoHistory &&
        m_fpGetPointerPenInfoHistory(pointerId, &outCount, pointerPenInfo.data()))) {
    return GHOST_kFailure;
  }

  for (GHOST_TUns32 i = 0; i < outCount; i++) {
    POINTER_INFO pointerApiInfo = pointerPenInfo[i].pointerInfo;
    // Obtain the basic information from the event
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

  return GHOST_kSuccess;
}

void GHOST_WindowWin32::processWin32TabletActivateEvent(WORD state)
{
  if (!useTabletAPI(GHOST_kTabletWintab)) {
    return;
  }

  if (m_wintab.enable && m_wintab.tablet) {
    m_wintab.enable(m_wintab.tablet, state);

    if (m_wintab.overlap && state) {
      m_wintab.overlap(m_wintab.tablet, TRUE);
    }
  }
}

bool GHOST_WindowWin32::useTabletAPI(GHOST_TTabletAPI api) const
{
  if (m_system->getTabletAPI() == api) {
    return true;
  }
  else if (m_system->getTabletAPI() == GHOST_kTabletAutomatic) {
    if (m_wintab.numDevices)
      return api == GHOST_kTabletWintab;
    else
      return api == GHOST_kTabletNative;
  }
  else {
    return false;
  }
}

void GHOST_WindowWin32::processWin32TabletInitEvent()
{
  if (!useTabletAPI(GHOST_kTabletWintab)) {
    return;
  }

  // Let's see if we can initialize tablet here
  if (m_wintab.info && m_wintab.tablet) {
    AXIS Pressure, Orientation[3]; /* The maximum tablet size */

    BOOL pressureSupport = m_wintab.info(WTI_DEVICES, DVC_NPRESSURE, &Pressure);
    if (pressureSupport)
      m_wintab.maxPressure = Pressure.axMax;
    else
      m_wintab.maxPressure = 0;

    BOOL tiltSupport = m_wintab.info(WTI_DEVICES, DVC_ORIENTATION, &Orientation);
    if (tiltSupport) {
      /* does the tablet support azimuth ([0]) and altitude ([1]) */
      if (Orientation[0].axResolution && Orientation[1].axResolution) {
        m_wintab.maxAzimuth = Orientation[0].axMax;
        m_wintab.maxAltitude = Orientation[1].axMax;
      }
      else { /* no so dont do tilt stuff */
        m_wintab.maxAzimuth = m_wintab.maxAltitude = 0;
      }
    }
  }

  m_tabletData.Active = GHOST_kTabletModeNone;
}

void GHOST_WindowWin32::processWintabInfoChangeEvent(LPARAM lParam)
{
  /* Update number of connected Wintab digitizers */
  if (LOWORD(lParam) == WTI_INTERFACE && HIWORD(lParam) == IFC_NDEVICES) {
    m_wintab.info(WTI_INTERFACE, IFC_NDEVICES, &m_wintab.numDevices);
  }
}

void GHOST_WindowWin32::processWin32TabletEvent(WPARAM wParam, LPARAM lParam)
{
  if (!useTabletAPI(GHOST_kTabletWintab)) {
    return;
  }

  if (m_wintab.packet && m_wintab.tablet) {
    PACKET pkt;
    if (m_wintab.packet((HCTX)lParam, wParam, &pkt)) {
      switch (pkt.pkCursor % 3) { /* % 3 for multiple devices ("DualTrack") */
        case 0:
          m_tabletData.Active = GHOST_kTabletModeNone; /* puck - not yet supported */
          break;
        case 1:
          m_tabletData.Active = GHOST_kTabletModeStylus; /* stylus */
          break;
        case 2:
          m_tabletData.Active = GHOST_kTabletModeEraser; /* eraser */
          break;
      }

      if (m_wintab.maxPressure > 0) {
        m_tabletData.Pressure = (float)pkt.pkNormalPressure / (float)m_wintab.maxPressure;
      }
      else {
        m_tabletData.Pressure = 1.0f;
      }

      if ((m_wintab.maxAzimuth > 0) && (m_wintab.maxAltitude > 0)) {
        ORIENTATION ort = pkt.pkOrientation;
        float vecLen;
        float altRad, azmRad; /* in radians */

        /*
         * from the wintab spec:
         * orAzimuth    Specifies the clockwise rotation of the
         * cursor about the z axis through a full circular range.
         *
         * orAltitude   Specifies the angle with the x-y plane
         * through a signed, semicircular range.  Positive values
         * specify an angle upward toward the positive z axis;
         * negative values specify an angle downward toward the negative z axis.
         *
         * wintab.h defines .orAltitude as a UINT but documents .orAltitude
         * as positive for upward angles and negative for downward angles.
         * WACOM uses negative altitude values to show that the pen is inverted;
         * therefore we cast .orAltitude as an (int) and then use the absolute value.
         */

        /* convert raw fixed point data to radians */
        altRad = (float)((fabs((float)ort.orAltitude) / (float)m_wintab.maxAltitude) * M_PI / 2.0);
        azmRad = (float)(((float)ort.orAzimuth / (float)m_wintab.maxAzimuth) * M_PI * 2.0);

        /* find length of the stylus' projected vector on the XY plane */
        vecLen = cos(altRad);

        /* from there calculate X and Y components based on azimuth */
        m_tabletData.Xtilt = sin(azmRad) * vecLen;
        m_tabletData.Ytilt = (float)(sin(M_PI / 2.0 - azmRad) * vecLen);
      }
      else {
        m_tabletData.Xtilt = 0.0f;
        m_tabletData.Ytilt = 0.0f;
      }
    }
  }
}

void GHOST_WindowWin32::bringTabletContextToFront()
{
  if (!useTabletAPI(GHOST_kTabletWintab)) {
    return;
  }

  if (m_wintab.overlap && m_wintab.tablet) {
    m_wintab.overlap(m_wintab.tablet, TRUE);
  }
}

GHOST_TUns16 GHOST_WindowWin32::getDPIHint()
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

/** Reverse the bits in a GHOST_TUns8 */
static GHOST_TUns8 uns8ReverseBits(GHOST_TUns8 ch)
{
  ch = ((ch >> 1) & 0x55) | ((ch << 1) & 0xAA);
  ch = ((ch >> 2) & 0x33) | ((ch << 2) & 0xCC);
  ch = ((ch >> 4) & 0x0F) | ((ch << 4) & 0xF0);
  return ch;
}

#if 0 /* UNUSED */
/** Reverse the bits in a GHOST_TUns16 */
static GHOST_TUns16 uns16ReverseBits(GHOST_TUns16 shrt)
{
  shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
  shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
  shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
  shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
  return shrt;
}
#endif

GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                                             GHOST_TUns8 *mask,
                                                             int sizeX,
                                                             int sizeY,
                                                             int hotX,
                                                             int hotY,
                                                             bool canInvertColor)
{
  GHOST_TUns32 andData[32];
  GHOST_TUns32 xorData[32];
  GHOST_TUns32 fullBitRow, fullMaskRow;
  int x, y, cols;

  cols = sizeX / 8; /* Number of whole bytes per row (width of bm/mask). */
  if (sizeX % 8)
    cols++;

  if (m_customCursor) {
    DestroyCursor(m_customCursor);
    m_customCursor = NULL;
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
  if (m_Bar && S_OK == m_Bar->SetProgressValue(m_hWnd, 10000 * progress, 10000))
    return GHOST_kSuccess;

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::endProgressBar()
{
  if (m_Bar && S_OK == m_Bar->SetProgressState(m_hWnd, TBPF_NOPROGRESS))
    return GHOST_kSuccess;

  return GHOST_kFailure;
}

#ifdef WITH_INPUT_IME
void GHOST_WindowWin32::beginIME(
    GHOST_TInt32 x, GHOST_TInt32 y, GHOST_TInt32 w, GHOST_TInt32 h, int completed)
{
  m_imeInput.BeginIME(m_hWnd, GHOST_Rect(x, y - h, x, y), (bool)completed);
}

void GHOST_WindowWin32::endIME()
{
  m_imeInput.EndIME(m_hWnd);
}
#endif /* WITH_INPUT_IME */
