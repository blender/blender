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
 * Declaration of GHOST_WindowWin32 class.
 */

#ifndef __GHOST_WINDOWWIN32_H__
#define __GHOST_WINDOWWIN32_H__

#ifndef WIN32
#  error WIN32 only!
#endif  // WIN32

#include "GHOST_Window.h"
#include "GHOST_TaskbarWin32.h"
#ifdef WITH_INPUT_IME
#  include "GHOST_ImeWin32.h"
#endif

#include <wintab.h>
#define PACKETDATA (PK_BUTTONS | PK_NORMAL_PRESSURE | PK_ORIENTATION | PK_CURSOR)
#define PACKETMODE PK_BUTTONS
#include <pktdef.h>

class GHOST_SystemWin32;
class GHOST_DropTargetWin32;

// typedefs for WinTab functions to allow dynamic loading
typedef UINT(API *GHOST_WIN32_WTInfo)(UINT, UINT, LPVOID);
typedef HCTX(API *GHOST_WIN32_WTOpen)(HWND, LPLOGCONTEXTA, BOOL);
typedef BOOL(API *GHOST_WIN32_WTClose)(HCTX);
typedef BOOL(API *GHOST_WIN32_WTPacket)(HCTX, UINT, LPVOID);
typedef BOOL(API *GHOST_WIN32_WTEnable)(HCTX, BOOL);
typedef BOOL(API *GHOST_WIN32_WTOverlap)(HCTX, BOOL);

// typedef to user32 functions to disable gestures on windows
typedef BOOL(API *GHOST_WIN32_RegisterTouchWindow)(HWND hwnd, ULONG ulFlags);

// typedefs for user32 functions to allow dynamic loading of Windows 10 DPI scaling functions
typedef UINT(API *GHOST_WIN32_GetDpiForWindow)(HWND);
#ifndef USER_DEFAULT_SCREEN_DPI
#  define USER_DEFAULT_SCREEN_DPI 96
#endif  // USER_DEFAULT_SCREEN_DPI

// typedefs for user32 functions to allow pointer functions
enum tagPOINTER_INPUT_TYPE {
  PT_POINTER = 1,  // Generic pointer
  PT_TOUCH = 2,    // Touch
  PT_PEN = 3,      // Pen
  PT_MOUSE = 4,    // Mouse
#if (WINVER >= 0x0603)
  PT_TOUCHPAD = 5,  // Touchpad
#endif              /* WINVER >= 0x0603 */
};

typedef enum tagPOINTER_BUTTON_CHANGE_TYPE {
  POINTER_CHANGE_NONE,
  POINTER_CHANGE_FIRSTBUTTON_DOWN,
  POINTER_CHANGE_FIRSTBUTTON_UP,
  POINTER_CHANGE_SECONDBUTTON_DOWN,
  POINTER_CHANGE_SECONDBUTTON_UP,
  POINTER_CHANGE_THIRDBUTTON_DOWN,
  POINTER_CHANGE_THIRDBUTTON_UP,
  POINTER_CHANGE_FOURTHBUTTON_DOWN,
  POINTER_CHANGE_FOURTHBUTTON_UP,
  POINTER_CHANGE_FIFTHBUTTON_DOWN,
  POINTER_CHANGE_FIFTHBUTTON_UP,
} POINTER_BUTTON_CHANGE_TYPE;

typedef DWORD POINTER_INPUT_TYPE;
typedef UINT32 POINTER_FLAGS;

typedef struct tagPOINTER_INFO {
  POINTER_INPUT_TYPE pointerType;
  UINT32 pointerId;
  UINT32 frameId;
  POINTER_FLAGS pointerFlags;
  HANDLE sourceDevice;
  HWND hwndTarget;
  POINT ptPixelLocation;
  POINT ptHimetricLocation;
  POINT ptPixelLocationRaw;
  POINT ptHimetricLocationRaw;
  DWORD dwTime;
  UINT32 historyCount;
  INT32 InputData;
  DWORD dwKeyStates;
  UINT64 PerformanceCount;
  POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
} POINTER_INFO;

typedef UINT32 PEN_FLAGS;
#define PEN_FLAG_NONE 0x00000000      // Default
#define PEN_FLAG_BARREL 0x00000001    // The barrel button is pressed
#define PEN_FLAG_INVERTED 0x00000002  // The pen is inverted
#define PEN_FLAG_ERASER 0x00000004    // The eraser button is pressed

typedef UINT32 PEN_MASK;
#define PEN_MASK_NONE 0x00000000      // Default - none of the optional fields are valid
#define PEN_MASK_PRESSURE 0x00000001  // The pressure field is valid
#define PEN_MASK_ROTATION 0x00000002  // The rotation field is valid
#define PEN_MASK_TILT_X 0x00000004    // The tiltX field is valid
#define PEN_MASK_TILT_Y 0x00000008    // The tiltY field is valid

typedef struct tagPOINTER_PEN_INFO {
  POINTER_INFO pointerInfo;
  PEN_FLAGS penFlags;
  PEN_MASK penMask;
  UINT32 pressure;
  UINT32 rotation;
  INT32 tiltX;
  INT32 tiltY;
} POINTER_PEN_INFO;

/*
 * Flags that appear in pointer input message parameters
 */
#define POINTER_MESSAGE_FLAG_NEW 0x00000001           // New pointer
#define POINTER_MESSAGE_FLAG_INRANGE 0x00000002       // Pointer has not departed
#define POINTER_MESSAGE_FLAG_INCONTACT 0x00000004     // Pointer is in contact
#define POINTER_MESSAGE_FLAG_FIRSTBUTTON 0x00000010   // Primary action
#define POINTER_MESSAGE_FLAG_SECONDBUTTON 0x00000020  // Secondary action
#define POINTER_MESSAGE_FLAG_THIRDBUTTON 0x00000040   // Third button
#define POINTER_MESSAGE_FLAG_FOURTHBUTTON 0x00000080  // Fourth button
#define POINTER_MESSAGE_FLAG_FIFTHBUTTON 0x00000100   // Fifth button
#define POINTER_MESSAGE_FLAG_PRIMARY 0x00002000       // Pointer is primary
#define POINTER_MESSAGE_FLAG_CONFIDENCE \
  0x00004000  // Pointer is considered unlikely to be accidental
#define POINTER_MESSAGE_FLAG_CANCELED 0x00008000  // Pointer is departing in an abnormal manner

typedef UINT32 TOUCH_FLAGS;
#define TOUCH_FLAG_NONE 0x00000000  // Default

typedef UINT32 TOUCH_MASK;
#define TOUCH_MASK_NONE 0x00000000         // Default - none of the optional fields are valid
#define TOUCH_MASK_CONTACTAREA 0x00000001  // The rcContact field is valid
#define TOUCH_MASK_ORIENTATION 0x00000002  // The orientation field is valid
#define TOUCH_MASK_PRESSURE 0x00000004     // The pressure field is valid

typedef struct tagPOINTER_TOUCH_INFO {
  POINTER_INFO pointerInfo;
  TOUCH_FLAGS touchFlags;
  TOUCH_MASK touchMask;
  RECT rcContact;
  RECT rcContactRaw;
  UINT32 orientation;
  UINT32 pressure;
} POINTER_TOUCH_INFO;

/*
 * Macros to retrieve information from pointer input message parameters
 */
#define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))
#define IS_POINTER_FLAG_SET_WPARAM(wParam, flag) (((DWORD)HIWORD(wParam) & (flag)) == (flag))
#define IS_POINTER_NEW_WPARAM(wParam) IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_NEW)
#define IS_POINTER_INRANGE_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_INRANGE)
#define IS_POINTER_INCONTACT_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_INCONTACT)
#define IS_POINTER_FIRSTBUTTON_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FIRSTBUTTON)
#define IS_POINTER_SECONDBUTTON_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_SECONDBUTTON)
#define IS_POINTER_THIRDBUTTON_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_THIRDBUTTON)
#define IS_POINTER_FOURTHBUTTON_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FOURTHBUTTON)
#define IS_POINTER_FIFTHBUTTON_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FIFTHBUTTON)
#define IS_POINTER_PRIMARY_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_PRIMARY)
#define HAS_POINTER_CONFIDENCE_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_CONFIDENCE)
#define IS_POINTER_CANCELED_WPARAM(wParam) \
  IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_CANCELED)

typedef BOOL(API *GHOST_WIN32_GetPointerInfo)(UINT32 pointerId, POINTER_INFO *pointerInfo);
typedef BOOL(API *GHOST_WIN32_GetPointerPenInfo)(UINT32 pointerId, POINTER_PEN_INFO *penInfo);
typedef BOOL(API *GHOST_WIN32_GetPointerTouchInfo)(UINT32 pointerId, POINTER_TOUCH_INFO *penInfo);

struct GHOST_PointerInfoWin32 {
  GHOST_TInt32 pointerId;
  GHOST_TInt32 isInContact;
  GHOST_TInt32 isPrimary;
  GHOST_TSuccess hasButtonMask;
  GHOST_TButtonMask buttonMask;
  POINT pixelLocation;
  GHOST_TabletData tabletData;
};

/**
 * GHOST window on M$ Windows OSs.
 */
class GHOST_WindowWin32 : public GHOST_Window {
 public:
  /**
   * Constructor.
   * Creates a new window and opens it.
   * To check if the window was created properly, use the getValid() method.
   * \param title     The text shown in the title bar of the window.
   * \param left      The coordinate of the left edge of the window.
   * \param top       The coordinate of the top edge of the window.
   * \param width     The width the window.
   * \param height    The height the window.
   * \param state     The state the window is initially opened with.
   * \param type      The type of drawing context installed in this window.
   * \param wantStereoVisual   Stereo visual for quad buffered stereo.
   * \param parentWindowHwnd
   */
  GHOST_WindowWin32(GHOST_SystemWin32 *system,
                    const STR_String &title,
                    GHOST_TInt32 left,
                    GHOST_TInt32 top,
                    GHOST_TUns32 width,
                    GHOST_TUns32 height,
                    GHOST_TWindowState state,
                    GHOST_TDrawingContextType type = GHOST_kDrawingContextTypeNone,
                    bool wantStereoVisual = false,
                    bool alphaBackground = false,
                    GHOST_TEmbedderWindowID parentWindowHwnd = 0,
                    bool is_debug = false);

  /**
   * Destructor.
   * Closes the window and disposes resources allocated.
   */
  ~GHOST_WindowWin32();

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
   * \param title The title to display in the title bar.
   */
  void setTitle(const STR_String &title);

  /**
   * Returns the title displayed in the title bar.
   * \param title The title displayed in the title bar.
   */
  void getTitle(STR_String &title) const;

  /**
   * Returns the window rectangle dimensions.
   * The dimensions are given in screen coordinates that are
   * relative to the upper-left corner of the screen.
   * \param bounds The bounding rectangle of the window.
   */
  void getWindowBounds(GHOST_Rect &bounds) const;

  /**
   * Returns the client rectangle dimensions.
   * The left and top members of the rectangle are always zero.
   * \param bounds The bounding rectangle of the client area of the window.
   */
  void getClientBounds(GHOST_Rect &bounds) const;

  /**
   * Resizes client rectangle width.
   * \param width The new width of the client area of the window.
   */
  GHOST_TSuccess setClientWidth(GHOST_TUns32 width);

  /**
   * Resizes client rectangle height.
   * \param height The new height of the client area of the window.
   */
  GHOST_TSuccess setClientHeight(GHOST_TUns32 height);

  /**
   * Resizes client rectangle.
   * \param width     The new width of the client area of the window.
   * \param height    The new height of the client area of the window.
   */
  GHOST_TSuccess setClientSize(GHOST_TUns32 width, GHOST_TUns32 height);

  /**
   * Returns the state of the window (normal, minimized, maximized).
   * \return The state of the window.
   */
  GHOST_TWindowState getState() const;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX   The x-coordinate on the screen.
   * \param inY   The y-coordinate on the screen.
   * \param outX  The x-coordinate in the client rectangle.
   * \param outY  The y-coordinate in the client rectangle.
   */
  void screenToClient(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const;

  /**
   * Converts a point in screen coordinates to client rectangle coordinates
   * \param inX   The x-coordinate in the client rectangle.
   * \param inY   The y-coordinate in the client rectangle.
   * \param outX  The x-coordinate on the screen.
   * \param outY  The y-coordinate on the screen.
   */
  void clientToScreen(GHOST_TInt32 inX,
                      GHOST_TInt32 inY,
                      GHOST_TInt32 &outX,
                      GHOST_TInt32 &outY) const;

  /**
   * Sets the state of the window (normal, minimized, maximized).
   * \param state The state of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setState(GHOST_TWindowState state);

  /**
   * Sets the order of the window (bottom, top).
   * \param order The order of the window.
   * \return Indication of success.
   */
  GHOST_TSuccess setOrder(GHOST_TWindowOrder order);

  /**
   * Invalidates the contents of this window.
   */
  GHOST_TSuccess invalidate();

  /**
   * Sets the progress bar value displayed in the window/application icon
   * \param progress The progress %
   */
  GHOST_TSuccess setProgressBar(float progress);

  /**
   * Hides the progress bar in the icon
   */
  GHOST_TSuccess endProgressBar();

  /**
   * Register a mouse click event (should be called
   * for any real button press, controls mouse
   * capturing).
   *
   * \param press
   *      0 - mouse pressed
   *      1 - mouse released
   *      2 - operator grab
   *      3 - operator ungrab
   */
  void registerMouseClickEvent(int press);

  /**
   * Inform the window that it has lost mouse capture,
   * called in response to native window system messages.
   */
  void lostMouseCapture();

  /**
   * Loads the windows equivalent of a standard GHOST cursor.
   * \param visible       Flag for cursor visibility.
   * \param cursorShape   The cursor shape.
   */
  void loadCursor(bool visible, GHOST_TStandardCursor cursorShape) const;

  const GHOST_TabletData *GetTabletData()
  {
    return &m_tabletData;
  }

  void setTabletData(GHOST_TabletData *tabletData);
  bool useTabletAPI(GHOST_TTabletAPI api) const;
  void getPointerInfo(WPARAM wParam);

  void processWin32PointerEvent(WPARAM wParam);
  void processWin32TabletActivateEvent(WORD state);
  void processWin32TabletInitEvent();
  void processWin32TabletEvent(WPARAM wParam, LPARAM lParam);
  void bringTabletContextToFront();

  GHOST_TSuccess beginFullScreen() const
  {
    return GHOST_kFailure;
  }

  GHOST_TSuccess endFullScreen() const
  {
    return GHOST_kFailure;
  }

  GHOST_TUns16 getDPIHint() override;

  GHOST_TSuccess getPointerInfo(GHOST_PointerInfoWin32 *pointerInfo, WPARAM wParam, LPARAM lParam);

  /** if the window currently resizing */
  bool m_inLiveResize;

#ifdef WITH_INPUT_IME
  GHOST_ImeWin32 *getImeInput()
  {
    return &m_imeInput;
  }

  void beginIME(GHOST_TInt32 x, GHOST_TInt32 y, GHOST_TInt32 w, GHOST_TInt32 h, int completed);

  void endIME();
#endif /* WITH_INPUT_IME */

 private:
  /**
   * \param type  The type of rendering context create.
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
   * \param mode  GHOST_TGrabCursorMode.
   */
  GHOST_TSuccess setWindowCursorGrab(GHOST_TGrabCursorMode mode);

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCursorShape(GHOST_TStandardCursor shape);

  /**
   * Sets the cursor shape on the window using
   * native window system calls.
   */
  GHOST_TSuccess setWindowCustomCursorShape(GHOST_TUns8 *bitmap,
                                            GHOST_TUns8 *mask,
                                            int sizex,
                                            int sizey,
                                            int hotX,
                                            int hotY,
                                            bool canInvertColor);

  /** Pointer to system */
  GHOST_SystemWin32 *m_system;
  /** Pointer to COM IDropTarget implementor */
  GHOST_DropTargetWin32 *m_dropTarget;
  /** Window handle. */
  HWND m_hWnd;
  /** Device context handle. */
  HDC m_hDC;

  /** Flag for if window has captured the mouse */
  bool m_hasMouseCaptured;
  /** Flag if an operator grabs the mouse with WM_cursor_grab_enable/ungrab()
   * Multiple grabs must be released with a single ungrab */
  bool m_hasGrabMouse;
  /** Count of number of pressed buttons */
  int m_nPressedButtons;
  /** HCURSOR structure of the custom cursor */
  HCURSOR m_customCursor;
  /** request GL context aith alpha channel */
  bool m_wantAlphaBackground;

  /** ITaskbarList3 structure for progress bar*/
  ITaskbarList3 *m_Bar;

  static const wchar_t *s_windowClassName;
  static const int s_maxTitleLength;

  /** Tablet data for GHOST */
  GHOST_TabletData m_tabletData;

  /* Wintab API */
  struct {
    /** WinTab dll handle */
    HMODULE handle;

    /** API functions */
    GHOST_WIN32_WTInfo info;
    GHOST_WIN32_WTOpen open;
    GHOST_WIN32_WTClose close;
    GHOST_WIN32_WTPacket packet;
    GHOST_WIN32_WTEnable enable;
    GHOST_WIN32_WTOverlap overlap;

    /** Stores the Tablet context if detected Tablet features using WinTab.dll */
    HCTX tablet;
    LONG maxPressure;
    LONG maxAzimuth, maxAltitude;
  } m_wintab;

  GHOST_TWindowState m_normal_state;

  /** user32 dll handle*/
  HMODULE m_user32;
  GHOST_WIN32_GetPointerInfo m_fpGetPointerInfo;
  GHOST_WIN32_GetPointerPenInfo m_fpGetPointerPenInfo;
  GHOST_WIN32_GetPointerTouchInfo m_fpGetPointerTouchInfo;

  /** Hwnd to parent window */
  GHOST_TEmbedderWindowID m_parentWindowHwnd;

#ifdef WITH_INPUT_IME
  /** Handle input method editors event */
  GHOST_ImeWin32 m_imeInput;
#endif
  bool m_debug_context;
};

#endif  // __GHOST_WINDOWWIN32_H__
