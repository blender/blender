/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <algorithm>

#include "GHOST_ContextD3D.hh"
#include "GHOST_ContextNone.hh"
#include "GHOST_DropTargetWin32.hh"
#include "GHOST_SystemWin32.hh"
#include "GHOST_WindowManager.hh"
#include "GHOST_WindowWin32.hh"
#include "utf_winfunc.hh"
#include "utfconv.hh"

#ifdef WITH_OPENGL_BACKEND
#  include "GHOST_ContextWGL.hh"
#endif
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#ifdef WIN32
#  include "BLI_path_utils.hh"
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

/* force NVidia OPTIMUS to used dedicated graphics */
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
                                     const GHOST_ContextParams &context_params,
                                     GHOST_WindowWin32 *parent_window,
                                     bool dialog,
                                     const GHOST_GPUDevice &preferred_device)
    : GHOST_Window(width, height, state, context_params, false),
      mouse_present_(false),
      in_live_resize_(false),
      system_(system),
      drop_target_(nullptr),
      h_wnd_(0),
      h_DC_(0),
      is_dialog_(dialog),
      preferred_device_(preferred_device),
      has_mouse_captured_(false),
      has_grab_mouse_(false),
      n_pressed_buttons_(0),
      custom_cursor_(0),
      bar_(nullptr),
      wintab_(nullptr),
      last_pointer_tablet_data_(GHOST_TABLET_DATA_NONE),
      normal_state_(GHOST_kWindowStateNormal),
      user32_(::LoadLibrary("user32.dll")),
      parent_window_hwnd_(parent_window ? parent_window->h_wnd_ : HWND_DESKTOP),
      direct_manipulation_helper_(nullptr)
{
  DWORD style = parent_window ?
                    WS_POPUPWINDOW | WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SIZEBOX :
                    WS_OVERLAPPEDWINDOW;

  if (state == GHOST_kWindowStateFullScreen) {
    style |= WS_MAXIMIZE;
  }

  /* Forces owned windows onto taskbar and allows minimization. */
  DWORD extended_style = parent_window ? WS_EX_APPWINDOW : 0;

  if (dialog) {
    /* When we are ready to make windows of this type:
     * style = WS_POPUPWINDOW | WS_CAPTION
     * extended_style = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST
     */
  }

  RECT win_rect = {left, top, long(left + width), long(top + height)};
  adjustWindowRectForClosestMonitor(&win_rect, style, extended_style);

  wchar_t *title_16 = alloc_utf16_from_8((char *)title, 0);
  h_wnd_ = ::CreateWindowExW(extended_style,                 /* window extended style */
                             s_windowClassName,              /* pointer to registered class name */
                             title_16,                       /* pointer to window name */
                             style,                          /* window style */
                             win_rect.left,                  /* horizontal position of window */
                             win_rect.top,                   /* vertical position of window */
                             win_rect.right - win_rect.left, /* window width */
                             win_rect.bottom - win_rect.top, /* window height */
                             parent_window_hwnd_,            /* handle to parent or owner window */
                             0,                    /* handle to menu or child-window identifier */
                             ::GetModuleHandle(0), /* handle to application instance */
                             0);                   /* pointer to window-creation data */
  free(title_16);

  if (h_wnd_ == nullptr) {
    return;
  }

  registerWindowAppUserModelProperties();

  /*  Store the device context. */
  h_DC_ = ::GetDC(h_wnd_);

  if (!setDrawingContextType(type)) {
    const char *title = "Blender - Unsupported Graphics Card Configuration";
    const char *text = "";
#if defined(WIN32)
    if (strncmp(BLI_getenv("PROCESSOR_IDENTIFIER"), "ARM", 3) == 0 &&
        strstr(BLI_getenv("PROCESSOR_IDENTIFIER"), "Qualcomm") != NULL)
    {
      text =
          "A driver with support for OpenGL 4.3 or higher is required.\n\n"
          "Qualcomm devices require the \"OpenCL™, OpenGL®, and Vulkan® Compatibility Pack\" "
          "from the Microsoft Store.\n\n"
          "Devices using processors older than a Qualcomm Snapdragon 8cx Gen3 are incompatible, "
          "but may be able to run an emulated x64 copy of Blender, such as a 3.x LTS release.";
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
    MessageBox(h_wnd_, text, title, MB_OK | MB_ICONERROR);
    ::ReleaseDC(h_wnd_, h_DC_);
    ::DestroyWindow(h_wnd_);
    h_wnd_ = nullptr;
    if (!parent_window) {
      exit(0);
    }
    return;
  }

  RegisterTouchWindow(h_wnd_, 0);

  /* Register as drop-target. #OleInitialize(0) required first, done in GHOST_SystemWin32. */
  drop_target_ = new GHOST_DropTargetWin32(this, system_);
  ::RegisterDragDrop(h_wnd_, drop_target_);

  /* Store a pointer to this class in the window structure. */
  ::SetWindowLongPtr(h_wnd_, GWLP_USERDATA, (LONG_PTR)this);

  if (!system_->window_focus_) {
    /* If we don't want focus then lower to bottom. */
    ::SetWindowPos(h_wnd_, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }

  if (parent_window) {
    /* Release any parent capture to allow immediate interaction (#90110). */
    ::ReleaseCapture();
    parent_window->lostMouseCapture();
  }

  /* Show the window. */
  int nCmdShow;
  switch (state) {
    case GHOST_kWindowStateFullScreen:
    case GHOST_kWindowStateMaximized:
      nCmdShow = SW_SHOWMAXIMIZED;
      break;
    case GHOST_kWindowStateMinimized:
      nCmdShow = (system_->window_focus_) ? SW_SHOWMINIMIZED : SW_SHOWMINNOACTIVE;
      break;
    case GHOST_kWindowStateNormal:
    default:
      nCmdShow = (system_->window_focus_) ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE;
      break;
  }

  ThemeRefresh();

  ::ShowWindow(h_wnd_, nCmdShow);

  /* Initialize WINTAB. */
  if (system->getTabletAPI() != GHOST_kTabletWinPointer) {
    loadWintab(GHOST_kWindowStateMinimized != state);
  }

  /* Allow the showing of a progress bar on the taskbar. */
  CoCreateInstance(
      CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, (LPVOID *)&bar_);

  /* Initialize Direct Manipulation. */
  direct_manipulation_helper_ = GHOST_DirectManipulationHelper::create(h_wnd_, getDPIHint());

  /* Initialize HDR info. */
  updateHDRInfo();
}

void GHOST_WindowWin32::updateDirectManipulation()
{
  if (!direct_manipulation_helper_) {
    return;
  }

  direct_manipulation_helper_->update();
}

void GHOST_WindowWin32::onPointerHitTest(WPARAM wParam)
{
  /* Only #DM_POINTERHITTEST can be the first message of input sequence of touch-pad input. */

  if (!direct_manipulation_helper_) {
    return;
  }

  UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
  POINTER_INPUT_TYPE pointerType;
  if (GetPointerType(pointerId, &pointerType) && pointerType == PT_TOUCHPAD) {
    direct_manipulation_helper_->onPointerHitTest(pointerId);
  }
}

GHOST_TTrackpadInfo GHOST_WindowWin32::getTrackpadInfo()
{
  if (!direct_manipulation_helper_) {
    return {0, 0, 0};
  }

  return direct_manipulation_helper_->getTrackpadInfo();
}

GHOST_WindowWin32::~GHOST_WindowWin32()
{
  if (h_wnd_) {
    unregisterWindowAppUserModelProperties();
  }

  if (bar_) {
    bar_->SetProgressState(h_wnd_, TBPF_NOPROGRESS);
    bar_->Release();
    bar_ = nullptr;
  }

  closeWintab();

  if (user32_) {
    FreeLibrary(user32_);
    user32_ = nullptr;
  }

  if (custom_cursor_) {
    DestroyCursor(custom_cursor_);
    custom_cursor_ = nullptr;
  }

  if (h_wnd_ != nullptr && h_DC_ != nullptr && releaseNativeHandles()) {
    ::ReleaseDC(h_wnd_, h_DC_);
    h_DC_ = nullptr;
  }

  if (h_wnd_) {
    /* If this window is referenced by others as parent, clear that relation or windows will free
     * the handle while we still reference it. */
    for (GHOST_IWindow *iter_win : system_->getWindowManager()->getWindows()) {
      GHOST_WindowWin32 *iter_winwin = (GHOST_WindowWin32 *)iter_win;
      if (iter_winwin->parent_window_hwnd_ == h_wnd_) {
        ::SetWindowLongPtr(iter_winwin->h_wnd_, GWLP_HWNDPARENT, 0);
        iter_winwin->parent_window_hwnd_ = 0;
      }
    }

    if (drop_target_) {
      /* Disable DragDrop. */
      RevokeDragDrop(h_wnd_);
      /* Release our reference of the DropTarget and it will delete itself eventually. */
      drop_target_->Release();
      drop_target_ = nullptr;
    }
    ::SetWindowLongPtr(h_wnd_, GWLP_USERDATA, 0);
    ::DestroyWindow(h_wnd_);
    h_wnd_ = 0;
  }

  delete direct_manipulation_helper_;
  direct_manipulation_helper_ = nullptr;
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
  if (user32_) {
    fpAdjustWindowRectExForDpi = (GHOST_WIN32_AdjustWindowRectExForDpi)::GetProcAddress(
        user32_, "AdjustWindowRectExForDpi");
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
  return GHOST_Window::getValid() && h_wnd_ != 0 && h_DC_ != 0;
}

HWND GHOST_WindowWin32::getHWND() const
{
  return h_wnd_;
}

void *GHOST_WindowWin32::getOSWindow() const
{
  return (void *)h_wnd_;
}

void GHOST_WindowWin32::setTitle(const char *title)
{
  wchar_t *title_16 = alloc_utf16_from_8((char *)title, 0);
  ::SetWindowTextW(h_wnd_, (wchar_t *)title_16);
  free(title_16);
}

std::string GHOST_WindowWin32::getTitle() const
{
  std::wstring wtitle(::GetWindowTextLengthW(h_wnd_) + 1, L'\0');
  ::GetWindowTextW(h_wnd_, &wtitle[0], wtitle.capacity());

  std::string title(count_utf_8_from_16(wtitle.c_str()) + 1, '\0');
  conv_utf_16_to_8(wtitle.c_str(), &title[0], title.capacity());

  return title;
}

GHOST_TSuccess GHOST_WindowWin32::applyWindowDecorationStyle()
{
  /* DWMWINDOWATTRIBUTE::DWMWA_CAPTION_COLOR */
  constexpr DWORD caption_color_attr = 35;

  if (window_decoration_style_flags_ & GHOST_kDecorationColoredTitleBar) {
    const float *color = window_decoration_style_settings_.colored_titlebar_bg_color;
    const COLORREF colorref = RGB(
        char(color[0] * 255.0f), char(color[1] * 255.0f), char(color[2] * 255.0f));
    if (!SUCCEEDED(DwmSetWindowAttribute(h_wnd_, caption_color_attr, &colorref, sizeof(colorref))))
    {
      return GHOST_kFailure;
    }
  }
  return GHOST_kSuccess;
}

void GHOST_WindowWin32::getWindowBounds(GHOST_Rect &bounds) const
{
  RECT rect;
  ::GetWindowRect(h_wnd_, &rect);
  bounds.b_ = rect.bottom;
  bounds.l_ = rect.left;
  bounds.r_ = rect.right;
  bounds.t_ = rect.top;
}

void GHOST_WindowWin32::getClientBounds(GHOST_Rect &bounds) const
{
  RECT rect;
  POINT coord;
  if (!IsIconic(h_wnd_)) {
    ::GetClientRect(h_wnd_, &rect);

    coord.x = rect.left;
    coord.y = rect.top;
    ::ClientToScreen(h_wnd_, &coord);

    bounds.l_ = coord.x;
    bounds.t_ = coord.y;

    coord.x = rect.right;
    coord.y = rect.bottom;
    ::ClientToScreen(h_wnd_, &coord);

    bounds.r_ = coord.x;
    bounds.b_ = coord.y;
  }
  else {
    bounds.b_ = 0;
    bounds.l_ = 0;
    bounds.r_ = 0;
    bounds.t_ = 0;
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
    success = ::SetWindowPos(h_wnd_, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
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
    success = ::SetWindowPos(h_wnd_, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
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
    success = ::SetWindowPos(h_wnd_, HWND_TOP, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER) ?
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
  if (::IsIconic(h_wnd_)) {
    return GHOST_kWindowStateMinimized;
  }
  else if (::IsZoomed(h_wnd_)) {
    LONG_PTR result = ::GetWindowLongPtr(h_wnd_, GWL_STYLE);
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
  ::ScreenToClient(h_wnd_, &point);
  outX = point.x;
  outY = point.y;
}

void GHOST_WindowWin32::clientToScreen(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  POINT point = {inX, inY};
  ::ClientToScreen(h_wnd_, &point);
  outX = point.x;
  outY = point.y;
}

GHOST_TSuccess GHOST_WindowWin32::setState(GHOST_TWindowState state)
{
  GHOST_TWindowState curstate = getState();
  LONG_PTR style = GetWindowLongPtr(h_wnd_, GWL_STYLE) | WS_CAPTION;
  WINDOWPLACEMENT wp;
  wp.length = sizeof(WINDOWPLACEMENT);
  ::GetWindowPlacement(h_wnd_, &wp);

  switch (state) {
    case GHOST_kWindowStateMinimized:
      wp.showCmd = SW_MINIMIZE;
      break;
    case GHOST_kWindowStateMaximized:
      wp.showCmd = SW_SHOWMAXIMIZED;
      break;
    case GHOST_kWindowStateFullScreen:
      if (curstate != state && curstate != GHOST_kWindowStateMinimized) {
        normal_state_ = curstate;
      }
      wp.showCmd = SW_SHOWMAXIMIZED;
      wp.ptMaxPosition.x = 0;
      wp.ptMaxPosition.y = 0;
      style &= ~(WS_CAPTION | WS_MAXIMIZE);
      break;
    case GHOST_kWindowStateNormal:
    default:
      if (curstate == GHOST_kWindowStateFullScreen && normal_state_ == GHOST_kWindowStateMaximized)
      {
        wp.showCmd = SW_SHOWMAXIMIZED;
        normal_state_ = GHOST_kWindowStateNormal;
      }
      else {
        wp.showCmd = SW_SHOWNORMAL;
      }
      break;
  }
  ::SetWindowLongPtr(h_wnd_, GWL_STYLE, style);
  /* #SetWindowLongPtr Docs:
   * Frame changes not visible until #SetWindowPos with #SWP_FRAMECHANGED. */
  ::SetWindowPos(h_wnd_, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
  return ::SetWindowPlacement(h_wnd_, &wp) == TRUE ? GHOST_kSuccess : GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::setOrder(GHOST_TWindowOrder order)
{
  HWND hWndInsertAfter, hWndToRaise;

  if (order == GHOST_kWindowOrderBottom) {
    hWndInsertAfter = HWND_BOTTOM;
    hWndToRaise = ::GetWindow(h_wnd_, GW_HWNDNEXT); /* the window to raise */
  }
  else {
    if (getState() == GHOST_kWindowStateMinimized) {
      setState(GHOST_kWindowStateNormal);
    }
    hWndInsertAfter = HWND_TOP;
    hWndToRaise = nullptr;
  }

  if (::SetWindowPos(h_wnd_, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE) == FALSE) {
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
  if (h_wnd_) {
    success = ::InvalidateRect(h_wnd_, 0, FALSE) != 0 ? GHOST_kSuccess : GHOST_kFailure;
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
      GHOST_Context *context = new GHOST_ContextVK(
          want_context_params_, h_wnd_, 1, 2, preferred_device_, &hdr_info_);
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
            want_context_params_,
            false,
            h_wnd_,
            h_DC_,
            WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            4,
            minor,
            (want_context_params_.is_debug ? WGL_CONTEXT_DEBUG_BIT_ARB : 0),
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
      GHOST_Context *context = new GHOST_ContextD3D(want_context_params_, h_wnd_);

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
  if (has_mouse_captured_) {
    has_grab_mouse_ = false;
    n_pressed_buttons_ = 0;
    has_mouse_captured_ = false;
  }
}

bool GHOST_WindowWin32::isDialog() const
{
  return is_dialog_;
}

void GHOST_WindowWin32::updateMouseCapture(GHOST_MouseCaptureEventWin32 event)
{
  switch (event) {
    case MousePressed:
      n_pressed_buttons_++;
      break;
    case MouseReleased:
      if (n_pressed_buttons_) {
        n_pressed_buttons_--;
      }
      break;
    case OperatorGrab:
      has_grab_mouse_ = true;
      break;
    case OperatorUngrab:
      has_grab_mouse_ = false;
      break;
  }

  if (!n_pressed_buttons_ && !has_grab_mouse_ && has_mouse_captured_) {
    ::ReleaseCapture();
    has_mouse_captured_ = false;
  }
  else if ((n_pressed_buttons_ || has_grab_mouse_) && !has_mouse_captured_) {
    ::SetCapture(h_wnd_);
    has_mouse_captured_ = true;
  }
}

HCURSOR GHOST_WindowWin32::getStandardCursor(GHOST_TStandardCursor shape) const
{
  /* Convert GHOST cursor to Windows OEM cursor. */
  HANDLE cursor = nullptr;
  uint32_t flags = LR_SHARED | LR_DEFAULTSIZE;
  int cx = 0, cy = 0;

  switch (shape) {
    case GHOST_kStandardCursorCustom:
      if (custom_cursor_) {
        return custom_cursor_;
      }
      else {
        return nullptr;
      }
    case GHOST_kStandardCursorMove:
      cursor = ::LoadImage(nullptr, IDC_SIZEALL, IMAGE_CURSOR, cx, cy, flags);
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
  if (::GetForegroundWindow() == h_wnd_) {
    loadCursor(visible, getCursorShape());
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  if (mode != GHOST_kGrabDisable) {
    if (mode != GHOST_kGrabNormal) {
      system_->getCursorPosition(cursor_grab_init_pos_[0], cursor_grab_init_pos_[1]);
      setCursorGrabAccum(0, 0);

      if (mode == GHOST_kGrabHide) {
        setWindowCursorVisibility(false);
      }
    }
    updateMouseCapture(OperatorGrab);
  }
  else {
    if (cursor_grab_ == GHOST_kGrabHide) {
      system_->setCursorPosition(cursor_grab_init_pos_[0], cursor_grab_init_pos_[1]);
      setWindowCursorVisibility(true);
    }
    if (cursor_grab_ != GHOST_kGrabNormal) {
      /* Use to generate a mouse move event, otherwise the last event
       * blender gets can be outside the screen causing menus not to show
       * properly unless the user moves the mouse. */
      int32_t pos[2];
      system_->getCursorPosition(pos[0], pos[1]);
      system_->setCursorPosition(pos[0], pos[1]);
    }

    /* Almost works without but important otherwise the mouse GHOST location
     * can be incorrect on exit. */
    setCursorGrabAccum(0, 0);
    cursor_grab_bounds_.l_ = cursor_grab_bounds_.r_ = -1; /* disable */
    updateMouseCapture(OperatorUngrab);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setWindowCursorShape(GHOST_TStandardCursor cursor_shape)
{
  if (::GetForegroundWindow() == h_wnd_) {
    loadCursor(getCursorVisibility(), cursor_shape);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::hasCursorShape(GHOST_TStandardCursor cursor_shape)
{
  return (getStandardCursor(cursor_shape)) ? GHOST_kSuccess : GHOST_kFailure;
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
      /* Input value is a range of -90 to +90, with a positive value
       * indicating a tilt to the right. Convert to what Blender
       * expects: -1.0f (left) to +1.0f (right). */
      outPointerInfo[i].tabletData.Xtilt = std::clamp(
          pointerPenInfo[i].tiltX / 90.0f, -1.0f, 1.0f);
    }

    if (pointerPenInfo[i].penMask & PEN_MASK_TILT_Y) {
      /* Input value is a range of -90 to +90, with a positive value
       * indicating a tilt toward the user. Convert to what Blender
       * expects: -1.0f (away from user) to +1.0f (toward user). */
      outPointerInfo[i].tabletData.Ytilt = std::clamp(
          pointerPenInfo[i].tiltY / 90.0f, -1.0f, 1.0f);
    }
  }

  if (!outPointerInfo.empty()) {
    last_pointer_tablet_data_ = outPointerInfo.back().tabletData;
  }

  return GHOST_kSuccess;
}

void GHOST_WindowWin32::resetPointerPenInfo()
{
  last_pointer_tablet_data_ = GHOST_TABLET_DATA_NONE;
}

GHOST_Wintab *GHOST_WindowWin32::getWintab() const
{
  return wintab_;
}

void GHOST_WindowWin32::loadWintab(bool enable)
{
  if (!wintab_) {
    WINTAB_PRINTF("Loading Wintab for window %p\n", h_wnd_);
    if (wintab_ = GHOST_Wintab::loadWintab(h_wnd_)) {
      if (enable) {
        wintab_->enable();

        /* Focus Wintab if cursor is inside this window. This ensures Wintab is enabled when the
         * tablet is used to change the Tablet API. */
        int32_t x, y;
        if (system_->getCursorPosition(x, y)) {
          GHOST_Rect rect;
          getClientBounds(rect);

          if (rect.isInside(x, y)) {
            wintab_->gainFocus();
          }
        }
      }
    }
  }
}

void GHOST_WindowWin32::closeWintab()
{
  WINTAB_PRINTF("Closing Wintab for window %p\n", h_wnd_);
  delete wintab_;
  wintab_ = nullptr;
}

bool GHOST_WindowWin32::usingTabletAPI(GHOST_TTabletAPI api) const
{
  if (system_->getTabletAPI() == api) {
    return true;
  }
  else if (system_->getTabletAPI() == GHOST_kTabletAutomatic) {
    if (wintab_ && wintab_->devicesPresent()) {
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
    return wintab_ ? wintab_->getLastTabletData() : GHOST_TABLET_DATA_NONE;
  }
  else {
    return last_pointer_tablet_data_;
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
    DwmSetWindowAttribute(this->h_wnd_, 20, &DarkMode, sizeof(DarkMode));
  }
}

void GHOST_WindowWin32::updateDPI()
{
  if (direct_manipulation_helper_) {
    direct_manipulation_helper_->setDPI(getDPIHint());
  }
}

uint16_t GHOST_WindowWin32::getDPIHint()
{
  if (user32_) {
    GHOST_WIN32_GetDpiForWindow fpGetDpiForWindow = (GHOST_WIN32_GetDpiForWindow)::GetProcAddress(
        user32_, "GetDpiForWindow");

    if (fpGetDpiForWindow) {
      return fpGetDpiForWindow(this->h_wnd_);
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

GHOST_TSuccess GHOST_WindowWin32::setWindowCustomCursorShape(const uint8_t *bitmap,
                                                             const uint8_t *mask,
                                                             const int size[2],
                                                             const int hot_spot[2],
                                                             bool /*can_invert_color*/)
{
  if (mask) {
    /* Old 1bpp XBitMap bitmap and mask. */
    uint32_t andData[32];
    uint32_t xorData[32];
    uint32_t fullBitRow, fullMaskRow;
    int x, y, cols;

    cols = size[0] / 8; /* Number of whole bytes per row (width of bitmap/mask). */
    if (size[0] % 8) {
      cols++;
    }

    HCURSOR previous_cursor = custom_cursor_;

    memset(&andData, 0xFF, sizeof(andData));
    memset(&xorData, 0, sizeof(xorData));

    for (y = 0; y < size[1]; y++) {
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

    custom_cursor_ = ::CreateCursor(
        ::GetModuleHandle(0), hot_spot[0], hot_spot[1], 32, 32, andData, xorData);

    if (!custom_cursor_) {
      return GHOST_kFailure;
    }

    if (::GetForegroundWindow() == h_wnd_) {
      loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
    }

    if (previous_cursor) {
      DestroyCursor(previous_cursor);
    }

    return GHOST_kSuccess;
  }

  /* RGBA bitmap, size up to 255x255. This limit may differ on other
   * platforms. Requesting larger does not give an error, just results
   * in a smaller, empty result. */

  BITMAPV5HEADER header;
  memset(&header, 0, sizeof(BITMAPV5HEADER));
  header.bV5Size = sizeof(BITMAPV5HEADER);
  header.bV5Width = (LONG)size[0];
  header.bV5Height = (LONG)size[1];
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;

  HDC hdc = GetDC(h_wnd_);
  void *bits = nullptr;
  HBITMAP bmp = CreateDIBSection(
      hdc, (BITMAPINFO *)&header, DIB_RGB_COLORS, (void **)&bits, NULL, (DWORD)0);
  ReleaseDC(NULL, hdc);

  uint32_t *ptr = (uint32_t *)bits;
  char w = size[0];
  char h = size[1];
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      int i = (y * w * 4) + (x * 4);
      uint32_t r = bitmap[i];
      uint32_t g = bitmap[i + 1];
      uint32_t b = bitmap[i + 2];
      uint32_t a = bitmap[i + 3];
      *ptr++ = (a << 24) | (r << 16) | (g << 8) | b;
    }
  }

  HBITMAP empty_mask = CreateBitmap(size[0], size[1], 1, 1, NULL);
  ICONINFO icon_info;
  icon_info.fIcon = FALSE;
  icon_info.xHotspot = (DWORD)hot_spot[0];
  icon_info.yHotspot = (DWORD)hot_spot[1];
  icon_info.hbmMask = empty_mask;
  icon_info.hbmColor = bmp;

  HCURSOR previous_cursor = custom_cursor_;

  custom_cursor_ = CreateIconIndirect(&icon_info);
  DeleteObject(bmp);
  DeleteObject(empty_mask);

  if (!custom_cursor_) {
    return GHOST_kFailure;
  }

  if (::GetForegroundWindow() == h_wnd_) {
    loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
  }

  if (previous_cursor) {
    DestroyCursor(previous_cursor);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowWin32::setProgressBar(float progress)
{
  /* #SetProgressValue sets state to #TBPF_NORMAL automatically. */
  if (bar_ && S_OK == bar_->SetProgressValue(h_wnd_, 10000 * progress, 10000)) {
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_WindowWin32::endProgressBar()
{
  if (bar_ && S_OK == bar_->SetProgressState(h_wnd_, TBPF_NOPROGRESS)) {
    return GHOST_kSuccess;
  }

  return GHOST_kFailure;
}

#ifdef WITH_INPUT_IME
void GHOST_WindowWin32::beginIME(int32_t x, int32_t y, int32_t /*w*/, int32_t h, bool completed)
{
  ime_input_.BeginIME(h_wnd_, GHOST_Rect(x, y - h, x, y), completed);
}

void GHOST_WindowWin32::endIME()
{
  ime_input_.EndIME(h_wnd_);
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

  HRESULT hr = SHGetPropertyStoreForWindow(h_wnd_, IID_PPV_ARGS(&pstore));
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
  HRESULT hr = SHGetPropertyStoreForWindow(h_wnd_, IID_PPV_ARGS(&pstore));
  if (SUCCEEDED(hr)) {
    PROPVARIANT value;
    PropVariantInit(&value);
    pstore->SetValue(PKEY_AppUserModel_ID, value);
    pstore->SetValue(PKEY_AppUserModel_RelaunchCommand, value);
    pstore->SetValue(PKEY_AppUserModel_RelaunchDisplayNameResource, value);
    pstore->Release();
  }
}

/* We call this from a few window event changes, but actually none of them immediately
 * respond to SDR white level changes in the system. That requires using the WinRT API,
 * which we don't do so far. */
void GHOST_WindowWin32::updateHDRInfo()
{
  /* Get monitor from window. */
  HMONITOR hmonitor = ::MonitorFromWindow(h_wnd_, MONITOR_DEFAULTTONEAREST);
  if (!hmonitor) {
    return;
  }

  MONITORINFOEXW monitor_info = {};
  monitor_info.cbSize = sizeof(MONITORINFOEXW);
  if (!::GetMonitorInfoW(hmonitor, &monitor_info)) {
    return;
  }

  /* Get active display paths and modes. */
  UINT32 path_count = 0;
  UINT32 mode_count = 0;
  if (::GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) !=
      ERROR_SUCCESS)
  {
    return;
  }

  std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
  std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
  if (::QueryDisplayConfig(
          QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count, modes.data(), nullptr) !=
      ERROR_SUCCESS)
  {
    return;
  }

  GHOST_WindowHDRInfo info = GHOST_WINDOW_HDR_INFO_NONE;

  /* Find the display path matching the monitor. */
  for (const DISPLAYCONFIG_PATH_INFO &path : paths) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name = {};
    device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    device_name.header.size = sizeof(device_name);
    device_name.header.adapterId = path.sourceInfo.adapterId;
    device_name.header.id = path.sourceInfo.id;

    if (::DisplayConfigGetDeviceInfo(&device_name.header) != ERROR_SUCCESS) {
      continue;
    }
    if (wcscmp(monitor_info.szDevice, device_name.viewGdiDeviceName) != 0) {
      continue;
    }

    /* Query HDR status. */
    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO color_info = {};
    color_info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
    color_info.header.size = sizeof(color_info);
    color_info.header.adapterId = path.targetInfo.adapterId;
    color_info.header.id = path.targetInfo.id;

    if (::DisplayConfigGetDeviceInfo(&color_info.header) == ERROR_SUCCESS) {
      /* This particular combination indicates HDR mode is enabled. This is undocumented but
       * used by WinRT. When wideColorEnforced is true we are in SDR mode with advanced color. */
      info.wide_gamut_enabled = color_info.advancedColorSupported &&
                                color_info.advancedColorEnabled;
      info.hdr_enabled = info.wide_gamut_enabled && !color_info.wideColorEnforced;
    }

    if (info.hdr_enabled) {
      /* Query SDR white level. */
      DISPLAYCONFIG_SDR_WHITE_LEVEL white_level = {};
      white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
      white_level.header.size = sizeof(white_level);
      white_level.header.adapterId = path.targetInfo.adapterId;
      white_level.header.id = path.targetInfo.id;

      if (::DisplayConfigGetDeviceInfo(&white_level.header) == ERROR_SUCCESS) {
        if (white_level.SDRWhiteLevel > 0) {
          /* Windows assumes 1.0 = 80 nits, so multipley by that to get the absolute
           * value in nits if we need it in the future. */
          info.sdr_white_level = static_cast<float>(white_level.SDRWhiteLevel) / 1000.0f;
        }
      }
    }

    hdr_info_ = info;
  }
}
