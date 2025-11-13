/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <stdint.h>
#include <string>

#ifdef WITH_VULKAN_BACKEND
#  include <vulkan/vulkan_core.h>
VK_DEFINE_HANDLE(VmaAllocator)
#endif

/* This is used by `GHOST_C-api.h` too, cannot use C++ conventions. */
// NOLINTBEGIN: modernize-use-using

#include "MEM_guardedalloc.h"

#if defined(__cplusplus)
#  define GHOST_DECLARE_HANDLE(name) \
    typedef struct name##__ { \
      int unused; \
      MEM_CXX_CLASS_ALLOC_FUNCS(#name) \
    } *name
#else
#  define GHOST_DECLARE_HANDLE(name) \
    typedef struct name##__ { \
      int unused; \
    } *name
#endif

/**
 * Creates a "handle" for a C++ GHOST object.
 * A handle is just an opaque pointer to an empty struct.
 * In the API the pointer is cast to the actual C++ class.
 * The 'name' argument to the macro is the name of the handle to create.
 */

GHOST_DECLARE_HANDLE(GHOST_SystemHandle);
GHOST_DECLARE_HANDLE(GHOST_TimerTaskHandle);
GHOST_DECLARE_HANDLE(GHOST_WindowHandle);
GHOST_DECLARE_HANDLE(GHOST_EventHandle);
GHOST_DECLARE_HANDLE(GHOST_RectangleHandle);
GHOST_DECLARE_HANDLE(GHOST_EventConsumerHandle);
GHOST_DECLARE_HANDLE(GHOST_ContextHandle);
GHOST_DECLARE_HANDLE(GHOST_XrContextHandle);

typedef void (*GHOST_TBacktraceFn)(void *file_handle);

typedef void *GHOST_TUserDataPtr;

typedef enum { GHOST_kFailure = 0, GHOST_kSuccess } GHOST_TSuccess;

/**
 * A reference to cursor bitmap data.
 */
typedef struct {
  /** `RGBA` bytes. */
  const uint8_t *data;
  int data_size[2];
  int hot_spot[2];
} GHOST_CursorBitmapRef;

/**
 * Pass this as an argument to GHOST so each ghost back-end
 * can generate cursors on demand.
 */
typedef struct GHOST_CursorGenerator {
  /**
   * The main cursor generation callback.
   *
   * \note only supports RGBA cursors.
   *
   * \param cursor_generator: Pass in to allow accessing the user argument.
   * \param cursor_size: The cursor size to generate.
   * \param cursor_size_max: The maximum dimension (width or height).
   * \param r_bitmap_size: The bitmap width & height in pixels.
   * The generator must guarantee the resulting size (dimensions written to `r_bitmap_size`)
   * never exceeds `cursor_size_max`.
   * \param r_hot_spot: The cursor hot-spot.
   * \param r_can_invert_color: When true, the call it can be inverted too much dark themes.
   *
   * \return the bitmap data or null if it could not be generated.
   * - The color is "straight" (alpha is not pre-multiplied).
   * - At least: `sizeof(uint8_t[4]) * r_bitmap_size[0] * r_bitmap_size[1]` allocated bytes.
   */
  uint8_t *(*generate_fn)(const struct GHOST_CursorGenerator *cursor_generator,
                          int cursor_size,
                          int cursor_size_max,
                          uint8_t *(*alloc_fn)(size_t size),
                          int r_bitmap_size[2],
                          int r_hot_spot[2],
                          bool *r_can_invert_color);
  /**
   * Called once GHOST has finished with this object,
   * Typically this would free `user_data`.
   */
  void (*free_fn)(struct GHOST_CursorGenerator *cursor_generator);
  /**
   * Implementation specific data used for rasterization
   * (could contain SVG data for example).
   */
  GHOST_TUserDataPtr user_data;

} GHOST_CursorGenerator;

typedef enum {
  GHOST_gpuStereoVisual = (1 << 0),
  GHOST_gpuDebugContext = (1 << 1),
  GHOST_gpuVSyncIsOverridden = (1 << 2),

} GHOST_GPUFlags;

typedef enum GHOST_DialogOptions {
  GHOST_DialogWarning = (1 << 0),
  GHOST_DialogError = (1 << 1),
} GHOST_DialogOptions;

/**
 * Static flag (relating to the back-ends support for features).
 *
 * \note When adding new capabilities, add to #GHOST_CAPABILITY_FLAG_ALL,
 * then mask out of from the `getCapabilities(..)` callback with an explanation for why
 * the feature is not supported.
 */
typedef enum {
  /**
   * Set when warping the cursor is supported (re-positioning the users cursor).
   */
  GHOST_kCapabilityCursorWarp = (1 << 0),
  /**
   * Set when getting/setting the window position is supported.
   */
  GHOST_kCapabilityWindowPosition = (1 << 1),
  /**
   * Set when a separate primary clipboard is supported.
   * This is a convention for X11/WAYLAND, select text & MMB to paste (without an explicit copy).
   */
  GHOST_kCapabilityClipboardPrimary = (1 << 2),
  /**
   * Support for reading the front-buffer.
   */
  GHOST_kCapabilityGPUReadFrontBuffer = (1 << 3),
  /**
   * Set when there is support for system clipboard copy/paste.
   */
  GHOST_kCapabilityClipboardImage = (1 << 4),
  /**
   * Support for sampling a color outside of the Blender windows.
   */
  GHOST_kCapabilityDesktopSample = (1 << 5),
  /**
   * Supports IME text input methods (when `WITH_INPUT_IME` is defined).
   */
  GHOST_kCapabilityInputIME = (1 << 6),
  /**
   * Support detecting the physical trackpad direction.
   */
  GHOST_kCapabilityTrackpadPhysicalDirection = (1 << 7),
  /**
   * Support for window decoration styles.
   */
  GHOST_kCapabilityWindowDecorationStyles = (1 << 8),
  /**
   * Support for the "Hyper" modifier key.
   */
  GHOST_kCapabilityKeyboardHyperKey = (1 << 9),
  /**
   * Support for creation of RGBA mouse cursors. This flag is likely
   * to be temporary as our intention is to implement on all platforms.
   */
  GHOST_kCapabilityCursorRGBA = (1 << 10),
  /**
   * Setting cursors via #GHOST_SetCursorGenerator is supported.
   */
  GHOST_kCapabilityCursorGenerator = (1 << 11),
  /**
   * Support accurately placing windows on multiple monitors.
   */
  GHOST_kCapabilityMultiMonitorPlacement = (1 << 12),
  /**
   * A "path" for a window is supported.
   * This indicates that #GHOST_IWindow::setPath can be used
   * without the need to include the windows file-path in its title.
   */
  GHOST_kCapabilityWindowPath = (1 << 13),
} GHOST_TCapabilityFlag;

/**
 * Back-ends should use this, masking out features which are not supported
 * with notes as to why those features cannot be supported.
 */
#define GHOST_CAPABILITY_FLAG_ALL \
  (GHOST_kCapabilityCursorWarp | GHOST_kCapabilityWindowPosition | \
   GHOST_kCapabilityClipboardPrimary | GHOST_kCapabilityGPUReadFrontBuffer | \
   GHOST_kCapabilityClipboardImage | GHOST_kCapabilityDesktopSample | GHOST_kCapabilityInputIME | \
   GHOST_kCapabilityTrackpadPhysicalDirection | GHOST_kCapabilityWindowDecorationStyles | \
   GHOST_kCapabilityKeyboardHyperKey | GHOST_kCapabilityCursorRGBA | \
   GHOST_kCapabilityCursorGenerator | GHOST_kCapabilityMultiMonitorPlacement | \
   GHOST_kCapabilityWindowPath)

/* Xtilt and Ytilt represent how much the pen is tilted away from
 * vertically upright in either the X or Y direction, with X and Y the
 * axes of the tablet surface.
 * In other words, Xtilt and Ytilt are components of a vector created by projecting
 * the pen's angle in 3D space vertically downwards on to the XY plane
 * --Matt
 */
typedef enum {
  GHOST_kTabletModeNone = 0,
  GHOST_kTabletModeStylus,
  GHOST_kTabletModeEraser
} GHOST_TTabletMode;

typedef enum {
  GHOST_kTabletAutomatic = 0,
  /* Show as Windows Ink to users to match "Use Windows Ink" in tablet utilities,
   * but we use the dependent Windows Pointer API. */
  GHOST_kTabletWinPointer,
  GHOST_kTabletWintab,
} GHOST_TTabletAPI;

typedef struct GHOST_TabletData {
  GHOST_TTabletMode Active; /* 0=None, 1=Stylus, 2=Eraser */
  float Pressure;           /* range 0.0 (not touching) to 1.0 (full pressure) */
  float Xtilt;              /* range -1.0 (left) to +1.0 (right) */
  float Ytilt;              /* range -1.0 (away from user) to +1.0 (toward user) */
} GHOST_TabletData;

static const GHOST_TabletData GHOST_TABLET_DATA_NONE = {
    GHOST_kTabletModeNone, /* No cursor in range */
    1.0f,                  /* Pressure */
    0.0f,                  /* Xtilt */
    0.0f};                 /* Ytilt */

typedef enum {
  GHOST_kNotVisible = 0,
  GHOST_kPartiallyVisible,
  GHOST_kFullyVisible
} GHOST_TVisibility;

typedef enum { GHOST_kFireTimeNever = 0xFFFFFFFF } GHOST_TFireTimeConstant;

typedef enum {
  GHOST_kModifierKeyLeftShift = 0,
  GHOST_kModifierKeyRightShift,
  GHOST_kModifierKeyLeftAlt,
  GHOST_kModifierKeyRightAlt,
  GHOST_kModifierKeyLeftControl,
  GHOST_kModifierKeyRightControl,
  GHOST_kModifierKeyLeftOS,
  GHOST_kModifierKeyRightOS,
  GHOST_kModifierKeyLeftHyper,
  GHOST_kModifierKeyRightHyper,
  GHOST_kModifierKeyNum
} GHOST_TModifierKey;

/**
 * \note these values are stored in #wmWindow::windowstate,
 * don't change, only add new values.
 */
typedef enum {
  GHOST_kWindowStateNormal = 0,
  GHOST_kWindowStateMaximized = 1,
  GHOST_kWindowStateMinimized = 2,
  GHOST_kWindowStateFullScreen = 3,
} GHOST_TWindowState;

typedef enum {
  GHOST_kConsoleWindowStateHide = 0,
  GHOST_kConsoleWindowStateShow,
  GHOST_kConsoleWindowStateToggle,
  GHOST_kConsoleWindowStateHideForNonConsoleLaunch
} GHOST_TConsoleWindowState;

typedef enum { GHOST_kWindowOrderTop = 0, GHOST_kWindowOrderBottom } GHOST_TWindowOrder;

typedef enum {
  GHOST_kDrawingContextTypeNone = 0,
#if defined(WITH_OPENGL_BACKEND)
  GHOST_kDrawingContextTypeOpenGL,
#endif
#ifdef WIN32
  GHOST_kDrawingContextTypeD3D,
#endif
#if defined(__APPLE__) && defined(WITH_METAL_BACKEND)
  GHOST_kDrawingContextTypeMetal,
#endif
#ifdef WITH_VULKAN_BACKEND
  GHOST_kDrawingContextTypeVulkan,
#endif
} GHOST_TDrawingContextType;

typedef enum {
  GHOST_kButtonMaskNone,
  GHOST_kButtonMaskLeft,
  GHOST_kButtonMaskMiddle,
  GHOST_kButtonMaskRight,
  GHOST_kButtonMaskButton4,
  GHOST_kButtonMaskButton5,
  /* Trackballs and programmable buttons. */
  GHOST_kButtonMaskButton6,
  GHOST_kButtonMaskButton7,

#define GHOST_kButtonNum (int(GHOST_kButtonMaskButton7) + 1)
} GHOST_TButton;

typedef enum {
  GHOST_kEventUnknown = 0,

  /** Mouse move event.
   *
   * \note #GHOST_GetEventData returns #GHOST_TEventCursorData.
   */
  GHOST_kEventCursorMove,
  /** Mouse button down event. */
  GHOST_kEventButtonDown,
  /** Mouse button up event. */
  GHOST_kEventButtonUp,
  /**
   * Vertical/Horizontal mouse wheel event.
   *
   * \note #GHOST_GetEventData returns #GHOST_TEventWheelData.
   */
  GHOST_kEventWheel,
  /**
   * Trackpad event.
   *
   * \note #GHOST_GetEventData returns #GHOST_TEventTrackpadData.
   */
  GHOST_kEventTrackpad,

#ifdef WITH_INPUT_NDOF
  /**
   * N degree of freedom device motion event.
   *
   * \note #GHOST_GetEventData returns #GHOST_TEventNDOFMotionData.
   */
  GHOST_kEventNDOFMotion,
  /**
   * N degree of freedom device button event.
   *
   * \note #GHOST_GetEventData returns #GHOST_TEventNDOFButtonData.
   */
  GHOST_kEventNDOFButton,
#endif

  /**
   * Keyboard up/down events.
   *
   * Includes repeat events, check #GHOST_TEventKeyData::is_repeat
   * if detecting repeat events is needed.
   *
   * \note #GHOST_GetEventData returns #GHOST_TEventKeyData.
   */
  GHOST_kEventKeyDown,
  GHOST_kEventKeyUp,

  GHOST_kEventQuitRequest,

  GHOST_kEventWindowClose,
  GHOST_kEventWindowActivate,
  GHOST_kEventWindowDeactivate,
  GHOST_kEventWindowUpdate,
  /** Client side window decorations have changed and need to be redrawn. */
  GHOST_kEventWindowUpdateDecor,
  GHOST_kEventWindowSize,
  GHOST_kEventWindowMove,
  GHOST_kEventWindowDPIHintChanged,

  GHOST_kEventDraggingEntered,
  GHOST_kEventDraggingUpdated,
  GHOST_kEventDraggingExited,
  GHOST_kEventDraggingDropDone,

  GHOST_kEventOpenMainFile, /* Needed for Cocoa to open double-clicked .blend file at startup. */
  GHOST_kEventNativeResolutionChange, /* Needed for Cocoa when window moves to other display. */

  GHOST_kEventImeCompositionStart,
  GHOST_kEventImeComposition,
  GHOST_kEventImeCompositionEnd,

#define GHOST_kNumEventTypes (GHOST_kEventImeCompositionEnd + 1)
} GHOST_TEventType;

typedef enum {
#define GHOST_kStandardCursorFirstCursor int(GHOST_kStandardCursorDefault)
  GHOST_kStandardCursorDefault = 0,
  GHOST_kStandardCursorRightArrow,
  GHOST_kStandardCursorLeftArrow,
  GHOST_kStandardCursorInfo,
  GHOST_kStandardCursorDestroy,
  GHOST_kStandardCursorHelp,
  GHOST_kStandardCursorWait,
  GHOST_kStandardCursorText,
  /** Crosshair: default. */
  GHOST_kStandardCursorCrosshair,
  /** Crosshair: with outline. */
  GHOST_kStandardCursorCrosshairA,
  /** Crosshair: a single "dot" (not really a crosshair). */
  GHOST_kStandardCursorCrosshairB,
  /** Crosshair: stippled/half-tone black/white. */
  GHOST_kStandardCursorCrosshairC,
  GHOST_kStandardCursorPencil,
  GHOST_kStandardCursorUpArrow,
  GHOST_kStandardCursorDownArrow,
  GHOST_kStandardCursorVerticalSplit,
  GHOST_kStandardCursorHorizontalSplit,
  GHOST_kStandardCursorEraser,
  GHOST_kStandardCursorKnife,
  GHOST_kStandardCursorEyedropper,
  GHOST_kStandardCursorZoomIn,
  GHOST_kStandardCursorZoomOut,
  GHOST_kStandardCursorMove,
  GHOST_kStandardCursorNSEWScroll,
  GHOST_kStandardCursorNSScroll,
  GHOST_kStandardCursorEWScroll,
  GHOST_kStandardCursorStop,
  GHOST_kStandardCursorUpDown,
  GHOST_kStandardCursorLeftRight,
  GHOST_kStandardCursorTopSide,
  GHOST_kStandardCursorBottomSide,
  GHOST_kStandardCursorLeftSide,
  GHOST_kStandardCursorRightSide,
  GHOST_kStandardCursorTopLeftCorner,
  GHOST_kStandardCursorTopRightCorner,
  GHOST_kStandardCursorBottomRightCorner,
  GHOST_kStandardCursorBottomLeftCorner,
  GHOST_kStandardCursorCopy,
  GHOST_kStandardCursorLeftHandle,
  GHOST_kStandardCursorRightHandle,
  GHOST_kStandardCursorBothHandles,
  GHOST_kStandardCursorHandOpen,
  GHOST_kStandardCursorHandClosed,
  GHOST_kStandardCursorHandPoint,
  GHOST_kStandardCursorBlade,
  GHOST_kStandardCursorSlip,
  GHOST_kStandardCursorCustom,

#define GHOST_kStandardCursorNumCursors (int(GHOST_kStandardCursorCustom) + 1)
} GHOST_TStandardCursor;

typedef enum {
  GHOST_kKeyUnknown = -1,
  GHOST_kKeyBackSpace,
  GHOST_kKeyTab,
  GHOST_kKeyLinefeed,
  GHOST_kKeyClear,
  GHOST_kKeyEnter = 0x0D,

  GHOST_kKeyEsc = 0x1B,
  GHOST_kKeySpace = ' ',
  GHOST_kKeyQuote = 0x27,
  GHOST_kKeyComma = ',',
  GHOST_kKeyMinus = '-',
  GHOST_kKeyPlus = '+',
  GHOST_kKeyPeriod = '.',
  GHOST_kKeySlash = '/',

  /* Number keys. */
  GHOST_kKey0 = '0',
  GHOST_kKey1,
  GHOST_kKey2,
  GHOST_kKey3,
  GHOST_kKey4,
  GHOST_kKey5,
  GHOST_kKey6,
  GHOST_kKey7,
  GHOST_kKey8,
  GHOST_kKey9,

  GHOST_kKeySemicolon = ';',
  GHOST_kKeyEqual = '=',

  /* Character keys. */
  GHOST_kKeyA = 'A',
  GHOST_kKeyB,
  GHOST_kKeyC,
  GHOST_kKeyD,
  GHOST_kKeyE,
  GHOST_kKeyF,
  GHOST_kKeyG,
  GHOST_kKeyH,
  GHOST_kKeyI,
  GHOST_kKeyJ,
  GHOST_kKeyK,
  GHOST_kKeyL,
  GHOST_kKeyM,
  GHOST_kKeyN,
  GHOST_kKeyO,
  GHOST_kKeyP,
  GHOST_kKeyQ,
  GHOST_kKeyR,
  GHOST_kKeyS,
  GHOST_kKeyT,
  GHOST_kKeyU,
  GHOST_kKeyV,
  GHOST_kKeyW,
  GHOST_kKeyX,
  GHOST_kKeyY,
  GHOST_kKeyZ,

  GHOST_kKeyLeftBracket = '[',
  GHOST_kKeyRightBracket = ']',
  GHOST_kKeyBackslash = 0x5C,
  GHOST_kKeyAccentGrave = '`',

#define _GHOST_KEY_MODIFIER_MIN GHOST_kKeyLeftShift
  /* Modifiers: See #GHOST_KEY_MODIFIER_CHECK. */
  GHOST_kKeyLeftShift = 0x100,
  GHOST_kKeyRightShift,
  GHOST_kKeyLeftControl,
  GHOST_kKeyRightControl,
  GHOST_kKeyLeftAlt,
  GHOST_kKeyRightAlt,
  GHOST_kKeyLeftOS, /* Command key on Apple, Windows key(s) on Windows. */
  GHOST_kKeyRightOS,

  GHOST_kKeyLeftHyper, /* Additional modifier on Wayland & X11, see !136340. */
  GHOST_kKeyRightHyper,
#define _GHOST_KEY_MODIFIER_MAX GHOST_kKeyRightHyper

  GHOST_kKeyGrLess, /* German PC only! */
  GHOST_kKeyApp,    /* Also known as menu key. */

  GHOST_kKeyCapsLock,
  GHOST_kKeyNumLock,
  GHOST_kKeyScrollLock,

  GHOST_kKeyLeftArrow,
  GHOST_kKeyRightArrow,
  GHOST_kKeyUpArrow,
  GHOST_kKeyDownArrow,

  GHOST_kKeyPrintScreen,
  GHOST_kKeyPause,

  GHOST_kKeyInsert,
  GHOST_kKeyDelete,
  GHOST_kKeyHome,
  GHOST_kKeyEnd,
  GHOST_kKeyUpPage,
  GHOST_kKeyDownPage,

  /* Numpad keys. */
  GHOST_kKeyNumpad0,
  GHOST_kKeyNumpad1,
  GHOST_kKeyNumpad2,
  GHOST_kKeyNumpad3,
  GHOST_kKeyNumpad4,
  GHOST_kKeyNumpad5,
  GHOST_kKeyNumpad6,
  GHOST_kKeyNumpad7,
  GHOST_kKeyNumpad8,
  GHOST_kKeyNumpad9,
  GHOST_kKeyNumpadPeriod,
  GHOST_kKeyNumpadEnter,
  GHOST_kKeyNumpadPlus,
  GHOST_kKeyNumpadMinus,
  GHOST_kKeyNumpadAsterisk,
  GHOST_kKeyNumpadSlash,

  /* Function keys. */
  GHOST_kKeyF1,
  GHOST_kKeyF2,
  GHOST_kKeyF3,
  GHOST_kKeyF4,
  GHOST_kKeyF5,
  GHOST_kKeyF6,
  GHOST_kKeyF7,
  GHOST_kKeyF8,
  GHOST_kKeyF9,
  GHOST_kKeyF10,
  GHOST_kKeyF11,
  GHOST_kKeyF12,
  GHOST_kKeyF13,
  GHOST_kKeyF14,
  GHOST_kKeyF15,
  GHOST_kKeyF16,
  GHOST_kKeyF17,
  GHOST_kKeyF18,
  GHOST_kKeyF19,
  GHOST_kKeyF20,
  GHOST_kKeyF21,
  GHOST_kKeyF22,
  GHOST_kKeyF23,
  GHOST_kKeyF24,

  /* Multimedia keypad buttons. */
  GHOST_kKeyMediaPlay,
  GHOST_kKeyMediaStop,
  GHOST_kKeyMediaFirst,
  GHOST_kKeyMediaLast
} GHOST_TKey;

#define GHOST_KEY_MODIFIER_NUM ((_GHOST_KEY_MODIFIER_MAX - _GHOST_KEY_MODIFIER_MIN) + 1)
#define GHOST_KEY_MODIFIER_TO_INDEX(key) ((unsigned int)(key) - _GHOST_KEY_MODIFIER_MIN)
#define GHOST_KEY_MODIFIER_FROM_INDEX(key) \
  (GHOST_TKey)(((unsigned int)(key) + _GHOST_KEY_MODIFIER_MIN))
#define GHOST_KEY_MODIFIER_CHECK(key) (GHOST_KEY_MODIFIER_TO_INDEX(key) < GHOST_KEY_MODIFIER_NUM)

typedef enum {
  /** Grab not set. */
  GHOST_kGrabDisable = 0,
  /** No cursor adjustments. */
  GHOST_kGrabNormal,
  /** Wrap the mouse location to prevent limiting screen bounds. */
  GHOST_kGrabWrap,
  /**
   * Hide the mouse while grabbing and restore the original location on release
   * (used for number buttons and some other draggable UI elements).
   */
  GHOST_kGrabHide,
} GHOST_TGrabCursorMode;

#define GHOST_GRAB_NEEDS_SOFTWARE_CURSOR_FOR_WARP(grab) ((grab) == GHOST_kGrabWrap)

typedef enum {
  /** Axis that cursor grab will wrap. */
  GHOST_kAxisNone = 0,
  GHOST_kAxisX = (1 << 0),
  GHOST_kAxisY = (1 << 1),
} GHOST_TAxisFlag;

typedef const void *GHOST_TEventDataPtr;

typedef struct {
  /** The x-coordinate of the cursor position. */
  int32_t x;
  /** The y-coordinate of the cursor position. */
  int32_t y;
  /** Associated tablet data. */
  GHOST_TabletData tablet;
} GHOST_TEventCursorData;

typedef struct {
  /** The mask of the mouse button. */
  GHOST_TButton button;
  /** Associated tablet data. */
  GHOST_TabletData tablet;
} GHOST_TEventButtonData;

typedef enum {
  GHOST_kEventWheelAxisVertical = 0,
  GHOST_kEventWheelAxisHorizontal = 1,
} GHOST_TEventWheelAxis;

typedef struct {
  /** Which mouse wheel is used. */
  GHOST_TEventWheelAxis axis;
  /** Displacement of a mouse wheel. */
  int32_t value;
} GHOST_TEventWheelData;

typedef enum {
  GHOST_kTrackpadEventUnknown = 0,
  GHOST_kTrackpadEventScroll,
  GHOST_kTrackpadEventRotate,
  GHOST_kTrackpadEventSwipe, /* Reserved, not used for now */
  GHOST_kTrackpadEventMagnify,
  GHOST_kTrackpadEventSmartMagnify
} GHOST_TTrackpadEventSubTypes;

typedef struct {
  /** The event subtype */
  GHOST_TTrackpadEventSubTypes subtype;
  /** The x-location of the trackpad event */
  int32_t x;
  /** The y-location of the trackpad event */
  int32_t y;
  /** The x-delta or value of the trackpad event */
  int32_t deltaX;
  /** The y-delta (currently only for scroll subtype) of the trackpad event */
  int32_t deltaY;
  /** The delta is inverted from the device due to system preferences. */
  char isDirectionInverted;
} GHOST_TEventTrackpadData;

typedef enum {
  GHOST_kDragnDropTypeUnknown = 0,
  GHOST_kDragnDropTypeFilenames, /* Array of strings representing file names (full path). */
  GHOST_kDragnDropTypeString,    /* Unformatted text UTF8 string. */
  GHOST_kDragnDropTypeBitmap     /* Bitmap image data. */
} GHOST_TDragnDropTypes;

typedef void *GHOST_TDragnDropDataPtr;

typedef struct {
  /** The x-coordinate of the cursor position. */
  int32_t x;
  /** The y-coordinate of the cursor position. */
  int32_t y;
  /** The dropped item type */
  GHOST_TDragnDropTypes dataType;
  /** The "dropped content" */
  GHOST_TDragnDropDataPtr data;
} GHOST_TEventDragnDropData;

/**
 * \warning this is a duplicate of #wmImeData.
 * All members must remain aligned and the struct size match!
 */
typedef struct {
  /** UTF8 encoded strings. */
  std::string result, composite;
  /** Cursor position in the IME composition. */
  int cursor_position;
  /** Represents the position of the beginning of the selection */
  int target_start;
  /** Represents the position of the end of the selection */
  int target_end;
} GHOST_TEventImeData;

typedef struct {
  int count;
  uint8_t **strings;
} GHOST_TStringArray;

/**
 * Keep in sync with #wmProgress.
 */
typedef enum {
  GHOST_kNotStarted = 0,
  GHOST_kStarting,
  GHOST_kInProgress,
  GHOST_kFinishing,
  GHOST_kFinished
} GHOST_TProgress;

#ifdef WITH_INPUT_NDOF
typedef struct {
  /** N-degree of freedom device data v3 [GSoC 2010] */
  /* Each component normally ranges from -1 to +1, but can exceed that.
   * These use blender standard view coordinates,
   * with positive rotations being CCW about the axis. */
  /* translation: */
  float tx, ty, tz;
  /* rotation:
   * - `axis = (rx,ry,rz).normalized`
   * - `amount = (rx,ry,rz).magnitude` [in revolutions, 1.0 = 360 deg]. */
  float rx, ry, rz;
  /** Time since previous NDOF Motion event */
  float dt;
  /** Starting, #GHOST_kInProgress or #GHOST_kFinishing (for modal handlers) */
  GHOST_TProgress progress;
} GHOST_TEventNDOFMotionData;

typedef enum { GHOST_kPress, GHOST_kRelease } GHOST_TButtonAction;
/* Good for mouse or other buttons too? */

typedef struct {
  GHOST_TButtonAction action;
  short button;
} GHOST_TEventNDOFButtonData;
#endif  // WITH_INPUT_NDOF

typedef struct {
  /** The key code. */
  GHOST_TKey key;

  /** The unicode character. if the length is 6, not null terminated if all 6 are set. */
  char utf8_buf[6];

  /**
   * Enabled when the key is held (auto-repeat).
   * In this case press events are sent without a corresponding release/up event.
   *
   * All back-ends must set this variable for correct behavior regarding repeatable keys.
   */
  char is_repeat;
} GHOST_TEventKeyData;

typedef enum {
  GHOST_kUserSpecialDirDesktop,
  GHOST_kUserSpecialDirDocuments,
  GHOST_kUserSpecialDirDownloads,
  GHOST_kUserSpecialDirMusic,
  GHOST_kUserSpecialDirPictures,
  GHOST_kUserSpecialDirVideos,
  GHOST_kUserSpecialDirCaches,
  /* Can be extended as needed. */
} GHOST_TUserSpecialDirTypes;

typedef enum {
  GHOST_kDecorationNone = 0,
  GHOST_kDecorationColoredTitleBar = (1 << 0),
} GHOST_TWindowDecorationStyleFlags;

typedef struct {
  /** Index of the GPU device in the list provided by the platform. */
  int index;
  /** (PCI) Vendor ID of the GPU. */
  uint vendor_id;
  /** Device ID of the GPU provided by the vendor. */
  uint device_id;
} GHOST_GPUDevice;

/**
 * Options for VSync.
 *
 * \note with the exception of #GHOST_kVSyncModeUnset,
 * these map to the OpenGL "swap interval" argument.
 */
typedef enum {
  /** Up to the GPU driver to choose. */
  GHOST_kVSyncModeUnset = -2,
  /** Adaptive sync (OpenGL only). */
  GHOST_kVSyncModeAuto = -1,
  /** Disable, useful for unclasped redraws for testing performance. */
  GHOST_kVSyncModeOff = 0,
  /** Force enable. */
  GHOST_kVSyncModeOn = 1,
} GHOST_TVSyncModes;

/**
 * Settings used to create a GPU context.
 *
 * \note Avoid adding values here unless they apply across multiple context implementations.
 * Otherwise the settings would be better added as extra arguments, only passed to that class.
 */
typedef struct {
  bool is_stereo_visual;
  bool is_debug;
  GHOST_TVSyncModes vsync;
} GHOST_ContextParams;

#define GHOST_CONTEXT_PARAMS_NONE \
  { \
      /*is_stereo_visual*/ false, \
      /*is_debug*/ false, \
      /*vsync*/ GHOST_kVSyncModeUnset, \
  }

#define GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS_OFFSCREEN(gpu_settings) \
  { \
      /*is_stereo_visual*/ false, \
      /*is_debug*/ (((gpu_settings).flags & GHOST_gpuDebugContext) != 0), \
      /*vsync*/ GHOST_kVSyncModeUnset, \
  }

#define GHOST_CONTEXT_PARAMS_FROM_GPU_SETTINGS(gpu_settings) \
  { \
      /*is_stereo_visual*/ (((gpu_settings).flags & GHOST_gpuStereoVisual) != 0), \
      /*is_debug*/ (((gpu_settings).flags & GHOST_gpuDebugContext) != 0), /*vsync*/ \
      (((gpu_settings).flags & GHOST_gpuVSyncIsOverridden) ? (gpu_settings).vsync : \
                                                             GHOST_kVSyncModeUnset), \
  }

typedef struct {
  int flags;
  /**
   * Use when `flags & GHOST_gpuVSyncIsOverridden` is set.
   * See #GHOST_ContextParams::vsync.
   */
  GHOST_TVSyncModes vsync;
  GHOST_TDrawingContextType context_type;
  GHOST_GPUDevice preferred_device;
} GHOST_GPUSettings;

typedef struct {
  float colored_titlebar_bg_color[3];
} GHOST_WindowDecorationStyleSettings;

typedef struct {
  /* Is HDR enabled for this Window? */
  bool hdr_enabled;
  /* Is wide gamut enabled for this Window? */
  bool wide_gamut_enabled;
  /* Scale factor to display SDR content in HDR. */
  float sdr_white_level;
} GHOST_WindowHDRInfo;

#define GHOST_WINDOW_HDR_INFO_NONE \
  { \
      /*hdr_enabled*/ false, \
      /*wide_gamut_enabled*/ false, \
      /*sdr_white_level*/ 1.0f, \
  }

#ifdef WITH_VULKAN_BACKEND
typedef struct {
  /** Image handle to the image that will be presented to the user. */
  VkImage image;
  /** Format of the swap-chain. */
  VkSurfaceFormatKHR surface_format;
  /** Resolution of the image. */
  VkExtent2D extent;
  /** Semaphore to wait before updating the image. */
  VkSemaphore acquire_semaphore;
  /** Semaphore to signal after the image has been updated. */
  VkSemaphore present_semaphore;
  /** Fence to signal after the image has been updated. */
  VkFence submission_fence;
  /* Factor to scale SDR content to HDR. */
  float sdr_scale;
} GHOST_VulkanSwapChainData;

typedef enum {
  /**
   * Use RAM to transfer the render result to the XR swapchain.
   *
   * Application renders a view, downloads the result to CPU RAM, GHOST_XrGraphicsBindingVulkan
   * will upload it to a GPU buffer and copy the buffer to the XR swapchain.
   */
  GHOST_kVulkanXRModeCPU,

  /**
   * Use Linux FD to transfer the render result to the XR swapchain.
   *
   * Application renders a view, export the memory in an FD handle. GHOST_XrGraphicsBindingVulkan
   * will import the memory and copy the image to the swapchain.
   */
  GHOST_kVulkanXRModeFD,

  /**
   * Use Win32 handle to transfer the render result to the XR swapchain.
   *
   * Application renders a view, export the memory in an win32 handle.
   * GHOST_XrGraphicsBindingVulkan will import the memory and copy the image to the swapchain.
   */
  GHOST_kVulkanXRModeWin32,
} GHOST_TVulkanXRModes;

typedef struct {
  /**
   * Mode to use for data transfer between the application rendered result and the OpenXR
   * swapchain. This is set by the GHOST and should be respected by the application.
   */
  GHOST_TVulkanXRModes data_transfer_mode;

  /**
   * Resolution of view render result.
   */
  VkExtent2D extent;

  union {
    struct {

      /**
       * Host accessible data containing the image data. Data is stored in the selected swapchain
       * format. Only used when data_transfer_mode == GHOST_kVulkanXRModeCPU.
       */
      void *image_data;
    } cpu;
    struct {
      /**
       * Vulkan handle of the image. When this is the same as last time the imported memory can be
       * reused.
       */
      VkImage vk_image_blender;

      /**
       * Did the memory address change and do we need to reimport the memory or can we still reuse
       * the previous imported memory.
       */
      bool new_handle;

      /**
       * Handle of the exported GPU memory. Depending on the data_transfer_mode the actual handle
       * type can be different (void-pointer/int/..).
       */
      uint64_t image_handle;

      /**
       * Data format of the image.
       */
      VkFormat image_format;

      /**
       * Allocation size of the exported memory.
       */
      VkDeviceSize memory_size;

      /**
       * Offset of the texture/buffer inside the allocated memory.
       */
      VkDeviceSize memory_offset;
    } gpu;
  };

} GHOST_VulkanOpenXRData;

/**
 * Return argument passed to #GHOST_IContext:::getVulkanHandles.
 *
 * The members of this struct are assigned values.
 */
typedef struct {
  /** The instance handle. */
  VkInstance instance;
  /** The physics device handle. */
  VkPhysicalDevice physical_device;
  /** The device handle. */
  VkDevice device;
  /** The graphic queue family id. */
  uint32_t graphic_queue_family;
  /** The queue handle. */
  VkQueue queue;
  /** The #std::mutex mutex. */
  void *queue_mutex;
  /** Vulkan memory allocator of the device. */
  VmaAllocator vma_allocator;
} GHOST_VulkanHandles;

#endif

typedef enum {
  /** Axis that cursor grab will wrap. */
  GHOST_kDebugDefault = (1 << 1),
  GHOST_kDebugWintab = (1 << 2),
} GHOST_TDebugFlags;

typedef struct {
  int flags;
} GHOST_Debug;

#ifdef _WIN32
typedef void *GHOST_TEmbedderWindowID;
#endif  // _WIN32

#ifndef _WIN32
/* I can't use "Window" from `X11/Xlib.h`
 * because it conflicts with Window defined in `winlay.h`. */
typedef int GHOST_TEmbedderWindowID;
#endif  // _WIN32

/**
 * A timer task callback routine.
 * \param task: The timer task object.
 * \param time: Time since this timer started (in milliseconds).
 */
#ifdef __cplusplus
class GHOST_ITimerTask;
typedef void (*GHOST_TimerProcPtr)(GHOST_ITimerTask *task, uint64_t time);
#else
struct GHOST_TimerTaskHandle__;
typedef void (*GHOST_TimerProcPtr)(struct GHOST_TimerTaskHandle__ *task, uint64_t time);
#endif

#ifdef WITH_XR_OPENXR

struct GHOST_XrDrawViewInfo;
struct GHOST_XrError;
/**
 * The XR view (i.e. the OpenXR runtime) may require a different graphics library than OpenGL.
 * An off-screen texture of the viewport will then be drawn into using OpenGL,
 * but the final texture draw call will happen through another library (say DirectX).
 *
 * This enum defines the possible graphics bindings to attempt to enable.
 */
typedef enum GHOST_TXrGraphicsBinding {
  GHOST_kXrGraphicsUnknown = 0,
  GHOST_kXrGraphicsOpenGL,
  GHOST_kXrGraphicsVulkan,
  GHOST_kXrGraphicsMetal,
#  ifdef WIN32
  GHOST_kXrGraphicsOpenGLD3D11,
  GHOST_kXrGraphicsVulkanD3D11,
#  endif
  /* For later */
  //  GHOST_kXrGraphicsVulkan,
} GHOST_TXrGraphicsBinding;

typedef void (*GHOST_XrErrorHandlerFn)(const struct GHOST_XrError *);

typedef void (*GHOST_XrSessionCreateFn)(void);
typedef void (*GHOST_XrSessionExitFn)(void *customdata);
typedef void (*GHOST_XrCustomdataFreeFn)(void *customdata);

typedef void *(*GHOST_XrGraphicsContextBindFn)(void);
typedef void (*GHOST_XrGraphicsContextUnbindFn)(GHOST_ContextHandle graphics_context);
typedef void (*GHOST_XrDrawViewFn)(const struct GHOST_XrDrawViewInfo *draw_view, void *customdata);
typedef bool (*GHOST_XrPassthroughEnabledFn)(void *customdata);
typedef void (*GHOST_XrDisablePassthroughFn)(void *customdata);

/**
 * An array of #GHOST_TXrGraphicsBinding items defining the candidate bindings to use.
 * The first available candidate will be chosen, so order defines priority.
 */
typedef const GHOST_TXrGraphicsBinding *GHOST_XrGraphicsBindingCandidates;

typedef struct {
  bool is_active;
  float position[3];
  /* Blender convention (w, x, y, z) */
  float orientation_quat[4];
} GHOST_XrPose;

enum {
  GHOST_kXrContextDebug = (1 << 0),
  GHOST_kXrContextDebugTime = (1 << 1),
#  ifdef WIN32
  /* Needed to avoid issues with the SteamVR OpenGL graphics binding
   * (use DirectX fallback instead). */
  GHOST_kXrContextGpuNVIDIA = (1 << 2),
#  endif
};

typedef struct {
  const GHOST_XrGraphicsBindingCandidates gpu_binding_candidates;
  unsigned int gpu_binding_candidates_count;

  unsigned int context_flag;
} GHOST_XrContextCreateInfo;

typedef struct {
  GHOST_XrPose base_pose;

  GHOST_XrSessionCreateFn create_fn;
  GHOST_XrSessionExitFn exit_fn;
  void *exit_customdata;
} GHOST_XrSessionBeginInfo;

/** Texture format for XR swapchain. */
typedef enum GHOST_TXrSwapchainFormat {
  GHOST_kXrSwapchainFormatRGBA8,
  GHOST_kXrSwapchainFormatRGBA16,
  GHOST_kXrSwapchainFormatRGBA16F,
  GHOST_kXrSwapchainFormatRGB10_A2,
} GHOST_TXrSwapchainFormat;

typedef struct GHOST_XrDrawViewInfo {
  int ofsx, ofsy;
  int width, height;

  GHOST_XrPose eye_pose;
  GHOST_XrPose local_pose;

  struct {
    float angle_left, angle_right;
    float angle_up, angle_down;
  } fov;

  GHOST_TXrSwapchainFormat swapchain_format;
  /** Set if the buffer should be submitted with a SRGB transfer applied. */
  char expects_srgb_buffer;

  /** The view that this info represents. Not necessarily the "eye index" (e.g. for quad view
   * systems, etc). */
  char view_idx;
} GHOST_XrDrawViewInfo;

typedef struct GHOST_XrError {
  const char *user_message;

  void *customdata;
} GHOST_XrError;

typedef struct GHOST_XrActionSetInfo {
  const char *name;

  GHOST_XrCustomdataFreeFn customdata_free_fn;
  void *customdata; /* wmXrActionSet */
} GHOST_XrActionSetInfo;

/** XR action type. Enum values match those in OpenXR's
 * XrActionType enum for consistency. */
typedef enum GHOST_XrActionType {
  GHOST_kXrActionTypeBooleanInput = 1,
  GHOST_kXrActionTypeFloatInput = 2,
  GHOST_kXrActionTypeVector2fInput = 3,
  GHOST_kXrActionTypePoseInput = 4,
  GHOST_kXrActionTypeVibrationOutput = 100,
} GHOST_XrActionType;

typedef struct GHOST_XrActionInfo {
  const char *name;
  GHOST_XrActionType type;
  uint32_t count_subaction_paths;
  const char **subaction_paths;
  /** States for each subaction path. */
  void *states;
  /** Input thresholds/regions for each subaction path. */
  float *float_thresholds;
  int16_t *axis_flags;

  GHOST_XrCustomdataFreeFn customdata_free_fn;
  void *customdata; /* wmXrAction */
} GHOST_XrActionInfo;

typedef struct GHOST_XrActionBindingInfo {
  const char *component_path;
  float float_threshold;
  int16_t axis_flag;
  GHOST_XrPose pose;
} GHOST_XrActionBindingInfo;

typedef struct GHOST_XrActionProfileInfo {
  const char *action_name;
  const char *profile_path;
  uint32_t count_subaction_paths;
  const char **subaction_paths;
  /** Bindings for each subaction path. */
  const GHOST_XrActionBindingInfo *bindings;
} GHOST_XrActionProfileInfo;

typedef struct GHOST_XrControllerModelVertex {
  float position[3];
  float normal[3];
} GHOST_XrControllerModelVertex;

typedef struct GHOST_XrControllerModelComponent {
  /** World space transform. */
  float transform[4][4];
  uint32_t vertex_offset;
  uint32_t vertex_count;
  uint32_t index_offset;
  uint32_t index_count;
} GHOST_XrControllerModelComponent;

typedef struct GHOST_XrControllerModelData {
  uint32_t count_vertices;
  const GHOST_XrControllerModelVertex *vertices;
  uint32_t count_indices;
  const uint32_t *indices;
  uint32_t count_components;
  const GHOST_XrControllerModelComponent *components;
} GHOST_XrControllerModelData;

#endif /* WITH_XR_OPENXR */

// NOLINTEND: modernize-use-using

/**
 * NDOF device button event types.
 *
 * SpaceMouse devices ship with an internal identifier number for each button.
 * Deprecated versions of the 3DxWare SDK have a `virtualkeys.h` header file
 * where some of these numbers are found but it is basically an arbitrary assignment
 * made by the vendor (3Dconnexion) since the application has the freedom to override as necessary.
 */
typedef enum {

  GHOST_NDOF_BUTTON_NONE = -1,
  /* Used internally, never sent or used as an index. */
  GHOST_NDOF_BUTTON_INVALID = 0,

  /* These two are available from any 3Dconnexion device. */
  GHOST_NDOF_BUTTON_MENU = 1,
  GHOST_NDOF_BUTTON_FIT = 2,

  /* Standard views. */
  GHOST_NDOF_BUTTON_TOP = 3,
  GHOST_NDOF_BUTTON_LEFT = 4,
  GHOST_NDOF_BUTTON_RIGHT = 5,
  GHOST_NDOF_BUTTON_FRONT = 6,
  GHOST_NDOF_BUTTON_BOTTOM = 7,
  GHOST_NDOF_BUTTON_BACK = 8,

  /* 90 degrees rotations. */
  GHOST_NDOF_BUTTON_ROLL_CW = 9,
  GHOST_NDOF_BUTTON_ROLL_CCW = 10,

  /* More views. */
  GHOST_NDOF_BUTTON_ISO1 = 11,
  GHOST_NDOF_BUTTON_ISO2 = 12,

  /* General-purpose buttons.
   * Users can assign functions via keymap editor. */
  GHOST_NDOF_BUTTON_1 = 13,
  GHOST_NDOF_BUTTON_2 = 14,
  GHOST_NDOF_BUTTON_3 = 15,
  GHOST_NDOF_BUTTON_4 = 16,
  GHOST_NDOF_BUTTON_5 = 17,
  GHOST_NDOF_BUTTON_6 = 18,
  GHOST_NDOF_BUTTON_7 = 19,
  GHOST_NDOF_BUTTON_8 = 20,
  GHOST_NDOF_BUTTON_9 = 21,
  GHOST_NDOF_BUTTON_10 = 22,

  /* Keyboard keys. */
  GHOST_NDOF_BUTTON_ESC = 23,
  GHOST_NDOF_BUTTON_ALT = 24,
  GHOST_NDOF_BUTTON_SHIFT = 25,
  GHOST_NDOF_BUTTON_CTRL = 26,

  /* Device control. */
  GHOST_NDOF_BUTTON_ROTATE = 27,
  GHOST_NDOF_BUTTON_PANZOOM = 28,
  GHOST_NDOF_BUTTON_DOMINANT = 29,
  GHOST_NDOF_BUTTON_PLUS = 30,
  GHOST_NDOF_BUTTON_MINUS = 31,

  /* New spin buttons. */
  GHOST_NDOF_BUTTON_SPIN_CW = 32,
  GHOST_NDOF_BUTTON_SPIN_CCW = 33,
  GHOST_NDOF_BUTTON_TILT_CW = 34,
  GHOST_NDOF_BUTTON_TILT_CCW = 35,

  /* Keyboard keys. */
  GHOST_NDOF_BUTTON_ENTER = 36,
  GHOST_NDOF_BUTTON_DELETE = 37,

  /* Keyboard Pro special buttons. */
  GHOST_NDOF_BUTTON_KBP_F1 = 41,
  GHOST_NDOF_BUTTON_KBP_F2 = 42,
  GHOST_NDOF_BUTTON_KBP_F3 = 43,
  GHOST_NDOF_BUTTON_KBP_F4 = 44,
  GHOST_NDOF_BUTTON_KBP_F5 = 45,
  GHOST_NDOF_BUTTON_KBP_F6 = 46,
  GHOST_NDOF_BUTTON_KBP_F7 = 47,
  GHOST_NDOF_BUTTON_KBP_F8 = 48,
  GHOST_NDOF_BUTTON_KBP_F9 = 49,
  GHOST_NDOF_BUTTON_KBP_F10 = 50,
  GHOST_NDOF_BUTTON_KBP_F11 = 51,
  GHOST_NDOF_BUTTON_KBP_F12 = 52,

  /* General-purpose buttons.
   * Users can assign functions via keymap editor. */
  GHOST_NDOF_BUTTON_11 = 77,
  GHOST_NDOF_BUTTON_12 = 78,

  /* Store views. */
  GHOST_NDOF_BUTTON_V1 = 103,
  GHOST_NDOF_BUTTON_V2 = 104,
  GHOST_NDOF_BUTTON_V3 = 105,
  GHOST_NDOF_BUTTON_SAVE_V1 = 139,
  GHOST_NDOF_BUTTON_SAVE_V2 = 140,
  GHOST_NDOF_BUTTON_SAVE_V3 = 141,

  /* Keyboard keys. */
  GHOST_NDOF_BUTTON_TAB = 175,
  GHOST_NDOF_BUTTON_SPACE = 176,

  /* Numpad Pro special buttons. */
  GHOST_NDOF_BUTTON_NP_F1 = 229,
  GHOST_NDOF_BUTTON_NP_F2 = 230,
  GHOST_NDOF_BUTTON_NP_F3 = 231,
  GHOST_NDOF_BUTTON_NP_F4 = 232,

  GHOST_NDOF_BUTTON_USER = 0x10000

} GHOST_NDOF_ButtonT;
