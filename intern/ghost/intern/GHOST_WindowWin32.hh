/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WindowWin32 class.
 */

#pragma once

#ifndef WIN32
#  error WIN32 only!
#endif /* WIN32 */

#include "GHOST_TaskbarWin32.hh"
#include "GHOST_TrackpadWin32.hh"
#include "GHOST_Window.hh"
#include "GHOST_Wintab.hh"
#ifdef WITH_INPUT_IME
#  include "GHOST_ImeWin32.hh"
#endif

#include <vector>

class GHOST_SystemWin32;
class GHOST_DropTargetWin32;

/* typedefs for user32 functions to allow dynamic loading of Windows 10 DPI scaling functions. */
typedef UINT(API *GHOST_WIN32_GetDpiForWindow)(HWND);

typedef BOOL(API *GHOST_WIN32_AdjustWindowRectExForDpi)(
    LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);

struct GHOST_PointerInfoWin32 {
  int32_t pointerId;
  int32_t isPrimary;
  GHOST_TButton buttonMask;
  POINT pixelLocation;
  uint64_t time;
  GHOST_TabletData tabletData;
};

typedef enum {
  MousePressed,
  MouseReleased,
  OperatorGrab,
  OperatorUngrab
} GHOST_MouseCaptureEventWin32;

/**
 * GHOST window on MS Windows OSs.
 */
class GHOST_WindowWin32 : public GHOST_Window {
 public:
  /**
   * Constructor.
   * Creates a new window and opens it.
   * To check if the window was created properly, use the #getValid() method.
   * \param title: The text shown in the title bar of the window.
   * \param left: The coordinate of the left edge of the window.
   * \param top: The coordinate of the top edge of the window.
   * \param width: The width the window.
   * \param height: The height the window.
   * \param state: The state the window is initially opened with.
   * \param type: The type of drawing context installed in this window.
   * \param wantStereoVisual: Stereo visual for quad buffered stereo.
   * \param parentWindowHwnd: TODO.
   */
  GHOST_WindowWin32(GHOST_SystemWin32 *system,
                    const char *title,
                    int32_t left,
                    int32_t top,
                    uint32_t width,
                    uint32_t height,
                    GHOST_TWindowState state,
                    GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
                    bool wantStereoVisual = false,
                    bool alphaBackground = false,
                    GHOST_WindowWin32 *parentWindow = 0,
                    bool is_debug = false,
                    bool dialog = false);

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowWin32();

  /**
   * Adjusts a requested window rect to fit and position correctly in monitor.
   * \param win_rect: pointer to rectangle that will be modified.
   * \param dwStyle: The Window Style of the window whose required size is to be calculated.
   * \param dwExStyle: The Extended Window Style of the window.
   */
  void adjustWindowRectForClosestMonitor(LPRECT win_rect, DWORD dwStyle, DWORD dwExStyle);

  /**
   * Returns indication as to whether the window is valid.
   * \return The validity of the window.
   */
  bool getValid() const;

  /**
   * Access to the handle of the window.
   * \return The handle of the window.
   */
  HWND getHWND() const;

  /**
   * Sets the title displayed in the title bar.
   * \param title: The title to display in the title bar.
   */
  void setTitle(const char *title);

  /**
   * Returns the title displayed in the title bar.
   * \return The title displayed in the title bar.
   */
  std::string getTitle() const;

  /**
   * Returns the window rectangle dimensions.
   * The dimensions are given in screen coordinates that are
   * relative to the upper-left corner of the screen.
   * \param bounds: The bounding rectangle of the window.
   */
  void getWindowBounds(GHOST_Rect &bounds) const;

  /**
   * Returns the client rectangle dimensions.
   * The left and top members of the rectangle are always zero.
   * \param bounds: The bounding rectangle of the client area of the window.
   */
  void getClientBounds(GHOST_Rect &bounds) const;

  /**
   * Resizes client rectangle width.
   * \param width: The new width of the client area of the window.
   */
  GHOST_TSuccess setClientWidth(uint32_t width);

  /**
   * Resizes client rectangle height.
   * \param height: The new height of the client area of the window.
   */
  GHOST_TSuccess setClientHeight(uint32_t height);

  /**
   * Resizes client rectangle.
   * \param width: The new width of the client area of the window.
   * \param height: The new height of the client area of the window.
   */
  GHOST_TSuccess setClientSize(uint32_t width, uint32_t height);

  /**
   * Returns the state of the window (normal, minimized, maximized).
   * \return The state of the window.
   */
  GHOST_TWindowState getState() const;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX: The x-coordinate on the screen.
   * \param inY: The y-coordinate on the screen.
   * \param outX: The x-coordinate in the client rectangle.
   * \param outY: The y-coordinate in the client rectangle.
   */
  void screenToClient(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

  /**
   * Converts a point in client rectangle coordinates to screen coordinates.
   * \param inX: The x-coordinate in the client rectangle.
   * \param inY: The y-coordinate in the client rectangle.
   * \param outX: The x-coordinate on the screen.
   * \param outY: The y-coordinate on the screen.
   */
  void clientToScreen(int32_t inX, int32_t inY, int32_t &outX, int32_t &outY) const;

  /**
   * Sets the state of the window (normal, minimized, maximized).
   * \param state: The state of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setState(GHOST_TWindowState state);

  /**
   * Sets the order of the window (bottom, top).
   * \param order: The order of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setOrder(GHOST_TWindowOrder order);

  /**
   * Invalidates the contents of this window.
   */
  GHOST_TSuccess invalidate();

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress: The progress percentage (0.0 to 1.0).
   */
  GHOST_TSuccess setProgressBar(float progress);

  /**
   * Hides the progress bar in the icon
   */
  GHOST_TSuccess endProgressBar();

  /**
   * Set or Release mouse capture (should be called for any real button press).
   *
   * \param event: Whether mouse was pressed and released,
   * or an operator grabbed or ungrabbed the mouse.
   */
  void updateMouseCapture(GHOST_MouseCaptureEventWin32 event);

  /**
   * Inform the window that it has lost mouse capture, called in response to native window system
   * messages (WA_INACTIVE, WM_CAPTURECHANGED) or if ReleaseCapture() is explicitly called (for new
   * window creation).
   */
  void lostMouseCapture();

  bool isDialog() const;

  /**
   * Loads the windows equivalent of a standard GHOST cursor.
   * \param visible: Flag for cursor visibility.
   * \param cursorShape: The cursor shape.
   */
  HCURSOR getStandardCursor(GHOST_TStandardCursor shape) const;
  void loadCursor(bool visible, GHOST_TStandardCursor cursorShape) const;

  /**
   * Query whether given tablet API should be used.
   * \param api: Tablet API to test.
   */
  bool usingTabletAPI(GHOST_TTabletAPI api) const;

  /**
   * Translate WM_POINTER events into GHOST_PointerInfoWin32 structs.
   * \param outPointerInfo: Storage to return resulting GHOST_PointerInfoWin32 structs.
   * \param wParam: WPARAM of the event.
   * \param lParam: LPARAM of the event.
   * \return True if #outPointerInfo was updated.
   */
  GHOST_TSuccess getPointerInfo(std::vector<GHOST_PointerInfoWin32> &outPointerInfo,
                                WPARAM wParam,
                                LPARAM lParam);

  /**
   * Resets pointer pen tablet state.
   */
  void resetPointerPenInfo();

  /**
   * Retrieves pointer to Wintab if Wintab is the set Tablet API.
   * \return Pointer to Wintab member.
   */
  GHOST_Wintab *getWintab() const;

  /**
   * Loads Wintab context for the window.
   * \param enable: True if Wintab should be enabled after loading. Wintab should not be enabled if
   * the window is minimized.
   */
  void loadWintab(bool enable);

  /**
   * Closes Wintab for the window.
   */
  void closeWintab();

  /**
   * Get the most recent Windows Pointer tablet data.
   * \return Most recent pointer tablet data.
   */
  GHOST_TabletData getTabletData();

  GHOST_TSuccess beginFullScreen() const
  {
    return GHOST_kFailure;
  }

  GHOST_TSuccess endFullScreen() const
  {
    return GHOST_kFailure;
  }

  void updateDPI();

  uint16_t getDPIHint() override;

  /** True if the mouse is either over or captured by the window. */
  bool m_mousePresent;

  /** True if the window currently resizing. */
  bool m_inLiveResize;

  /** Called when OS colors change and when the window is created. */
  void ThemeRefresh();

#ifdef WITH_INPUT_IME
  GHOST_ImeWin32 *getImeInput()
  {
    return &m_imeInput;
  }

  void beginIME(int32_t x, int32_t y, int32_t w, int32_t h, bool completed);

  void endIME();
#endif /* WITH_INPUT_IME */

  /*
   * Drive DirectManipulation context.
   */
  void updateDirectManipulation();

  /*
   * Handle DM_POINTERHITTEST events.
   * \param wParam: wParam from the event.
   */
  void onPointerHitTest(WPARAM wParam);

  GHOST_TTrackpadInfo getTrackpadInfo();

 private:
  /**
   * \param type: The type of rendering context create.
   * \return Indication of success.
   */
  GHOST_Context *newDrawingContext(GHOST_TDrawingContextType type);

  /**
   * Sets the cursor visibility on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorVisibility(bool visible);

  /**
   * Sets the cursor grab on the window using native window system calls.
   * Using registerMouseClickEvent.
   * \param mode: GHOST_TGrabCursorMode.
   */
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode);

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape);
  GHOST_TSuccess hasCursorShape(GHOST_TStandardCursor shape);

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCustomCursorShape(uint8_t *bitmap,
                                            uint8_t *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor);

  /* Registration of the AppModel Properties that govern the taskbar button and jump lists. */
  void registerWindowAppUserModelProperties();
  void unregisterWindowAppUserModelProperties();

  /** Pointer to system. */
  GHOST_SystemWin32 *m_system;
  /** Pointer to COM #IDropTarget implementer. */
  GHOST_DropTargetWin32 *m_dropTarget;
  /** Window handle. */
  HWND m_hWnd;
  /** Device context handle. */
  HDC m_hDC;

  bool m_isDialog;

  /** Flag for if window has captured the mouse. */
  bool m_hasMouseCaptured;
  /**
   * Flag if an operator grabs the mouse with #WM_cursor_grab_enable, #WM_cursor_grab_disable
   * Multiple grabs must be released with a single un-grab.
   */
  bool m_hasGrabMouse;
  /** Count of number of pressed buttons. */
  int m_nPressedButtons;
  /** HCURSOR structure of the custom cursor. */
  HCURSOR m_customCursor;
  /** Request GL context with alpha channel. */
  bool m_wantAlphaBackground;

  /** ITaskbarList3 structure for progress bar. */
  ITaskbarList3 *m_Bar;

  static const wchar_t *s_windowClassName;
  static const int s_maxTitleLength;

  /** Pointer to Wintab manager if Wintab is loaded. */
  GHOST_Wintab *m_wintab;

  /** Most recent tablet data. */
  GHOST_TabletData m_lastPointerTabletData;

  GHOST_TWindowState m_normal_state;

  /** `user32.dll` handle */
  HMODULE m_user32;

  HWND m_parentWindowHwnd;

  GHOST_DirectManipulationHelper *m_directManipulationHelper;

#ifdef WITH_INPUT_IME
  /** Handle input method editors event */
  GHOST_ImeWin32 m_imeInput;
#endif
  bool m_debug_context;
};
