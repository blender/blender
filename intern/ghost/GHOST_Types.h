/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#include <stdint.h>

#ifdef WITH_VULKAN_BACKEND
#  ifdef __APPLE__
#    include <MoltenVK/vk_mvk_moltenvk.h>
#  else
#    include <vulkan/vulkan.h>
#  endif
#endif

/* This is used by `GHOST_C-api.h` too, cannot use C++ conventions. */
// NOLINTBEGIN: modernize-use-using

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#else
/* Convenience unsigned abbreviations (#WITH_CXX_GUARDEDALLOC defines these). */
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned char uchar;
#endif

#if defined(WITH_CXX_GUARDEDALLOC) && defined(__cplusplus)
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

/**
 * A reference to cursor bitmap data.
 */
typedef struct {
  /** `RGBA` bytes. */
  const uint8_t *data;
  int data_size[2];
  int hot_spot[2];
} GHOST_CursorBitmapRef;

typedef enum {
  GHOST_gpuStereoVisual = (1 << 0),
  GHOST_gpuDebugContext = (1 << 1),
} GHOST_GPUFlags;

typedef enum GHOST_DialogOptions {
  GHOST_DialogWarning = (1 << 0),
  GHOST_DialogError = (1 << 1),
} GHOST_DialogOptions;

typedef void *GHOST_TUserDataPtr;

typedef enum { GHOST_kFailure = 0, GHOST_kSuccess } GHOST_TSuccess;

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
  GHOST_kCapabilityPrimaryClipboard = (1 << 2),
  /**
   * Support for reading the front-buffer.
   */
  GHOST_kCapabilityGPUReadFrontBuffer = (1 << 3),
  /**
   * Set when there is support for system clipboard copy/paste.
   */
  GHOST_kCapabilityClipboardImages = (1 << 4),
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
} GHOST_TCapabilityFlag;

/**
 * Back-ends should use this, masking out features which are not supported
 * with notes as to why those features cannot be supported.
 */
#define GHOST_CAPABILITY_FLAG_ALL \
  (GHOST_kCapabilityCursorWarp | GHOST_kCapabilityWindowPosition | \
   GHOST_kCapabilityPrimaryClipboard | GHOST_kCapabilityGPUReadFrontBuffer | \
   GHOST_kCapabilityClipboardImages | GHOST_kCapabilityDesktopSample | \
   GHOST_kCapabilityInputIME | GHOST_kCapabilityTrackpadPhysicalDirection)

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
  float Xtilt; /* range 0.0 (upright) to 1.0 (tilted fully against the tablet surface) */
  float Ytilt; /* as above */
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
  GHOST_kButtonNum
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
   * Mouse wheel event.
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
  GHOST_kStandardCursorCrosshair,
  GHOST_kStandardCursorCrosshairA,
  GHOST_kStandardCursorCrosshairB,
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
#define _GHOST_KEY_MODIFIER_MAX GHOST_kKeyRightOS

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
#define GHOST_KEY_MODIFIER_TO_INDEX(key) ((unsigned int)(key)-_GHOST_KEY_MODIFIER_MIN)
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

typedef struct {
  /** Displacement of a mouse wheel. */
  int32_t z;
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
  GHOST_kDragnDropTypeString,    /* Unformatted text UTF-8 string. */
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
  /** size_t */
  GHOST_TUserDataPtr result_len, composite_len;
  /** char * utf8 encoding */
  GHOST_TUserDataPtr result, composite;
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

  /** The unicode character. if the length is 6, not nullptr terminated if all 6 are set. */
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

typedef struct {
  /** Number of pixels on a line. */
  uint32_t xPixels;
  /** Number of lines. */
  uint32_t yPixels;
  /** Number of bits per pixel. */
  uint32_t bpp;
  /** Refresh rate (in Hertz). */
  uint32_t frequency;
} GHOST_DisplaySetting;

typedef struct {
  int flags;
  GHOST_TDrawingContextType context_type;
} GHOST_GPUSettings;

#ifdef WITH_VULKAN_BACKEND
typedef struct {
  /** Image handle to the image that will be presented to the user. */
  VkImage image;
  /** Format of the image. */
  VkFormat format;
  /** Resolution of the image. */
  VkExtent2D extent;
} GHOST_VulkanSwapChainData;
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
#  ifdef WIN32
  GHOST_kXrGraphicsD3D11,
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
