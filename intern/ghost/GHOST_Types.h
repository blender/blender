/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef _GHOST_TYPES_H_
#define _GHOST_TYPES_H_

typedef	char				GHOST_TInt8;
typedef	unsigned char		GHOST_TUns8;
typedef short				GHOST_TInt16;
typedef unsigned short		GHOST_TUns16;
typedef	int					GHOST_TInt32;
typedef	unsigned int		GHOST_TUns32;

#ifdef WIN32
#define WM_BLND_NDOF_AXIS	WM_USER + 1
#define WM_BLND_NDOF_BTN 	WM_USER + 2
#endif

#if defined(WIN32) && !defined(FREE_WINDOWS)
typedef __int64				GHOST_TInt64;
typedef unsigned __int64	GHOST_TUns64;
#else
typedef long long			GHOST_TInt64;
typedef unsigned long long	GHOST_TUns64;
#endif

typedef void*				GHOST_TUserDataPtr;

typedef enum
{
	GHOST_kFailure = 0,
	GHOST_kSuccess
} GHOST_TSuccess;

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

typedef struct GHOST_TabletData {
	GHOST_TTabletMode Active; /* 0=None, 1=Stylus, 2=Eraser */
	float Pressure;	/* range 0.0 (not touching) to 1.0 (full pressure) */
	float Xtilt;	/* range 0.0 (upright) to 1.0 (tilted fully against the tablet surface) */
	float Ytilt;	/* as above */
} GHOST_TabletData;


typedef enum {
	GHOST_kNotVisible = 0,
	GHOST_kPartiallyVisible,
	GHOST_kFullyVisible
} GHOST_TVisibility;


typedef enum {
	GHOST_kFireTimeNever = 0xFFFFFFFF
} GHOST_TFireTimeConstant;

typedef enum {
    GHOST_kModifierKeyLeftShift = 0,
    GHOST_kModifierKeyRightShift,
    GHOST_kModifierKeyLeftAlt,
    GHOST_kModifierKeyRightAlt,
    GHOST_kModifierKeyLeftControl,
    GHOST_kModifierKeyRightControl,
    GHOST_kModifierKeyCommand,		// APPLE only
    GHOST_kModifierKeyNumMasks
} GHOST_TModifierKeyMask;


typedef enum {
	GHOST_kWindowStateNormal = 0,
	GHOST_kWindowStateMaximized,
	GHOST_kWindowStateMinimized,
	GHOST_kWindowStateFullScreen,
	GHOST_kWindowStateEmbedded,
	GHOST_kWindowState8Normal = 8,
	GHOST_kWindowState8Maximized,
	GHOST_kWindowState8Minimized,
	GHOST_kWindowState8FullScreen,
	GHOST_kWindowStateModified,
	GHOST_kWindowStateUnModified
} GHOST_TWindowState;


/** Constants for the answer to the blender exit request */
typedef enum {
	GHOST_kExitCancel = 0,
	GHOST_kExitNow
} GHOST_TExitRequestResponse;

typedef enum {
	GHOST_kWindowOrderTop = 0,
	GHOST_kWindowOrderBottom
} GHOST_TWindowOrder;


typedef enum {
	GHOST_kDrawingContextTypeNone = 0,
	GHOST_kDrawingContextTypeOpenGL
} GHOST_TDrawingContextType;


typedef enum {
	GHOST_kButtonMaskLeft = 0,
	GHOST_kButtonMaskMiddle,
	GHOST_kButtonMaskRight,
	GHOST_kButtonMaskButton4,
	GHOST_kButtonMaskButton5,
	GHOST_kButtonNumMasks
} GHOST_TButtonMask;


typedef enum {
	GHOST_kEventUnknown = 0,

	GHOST_kEventCursorMove,		/// Mouse move event
	GHOST_kEventButtonDown,		/// Mouse button event
	GHOST_kEventButtonUp,		/// Mouse button event
	GHOST_kEventWheel,			/// Mouse wheel event
	GHOST_kEventTrackpad,		/// Trackpad event

	GHOST_kEventNDOFMotion,		/// N degree of freedom device motion event
	GHOST_kEventNDOFButton,		/// N degree of freedom device button event

	GHOST_kEventKeyDown,
	GHOST_kEventKeyUp,
//	GHOST_kEventKeyAuto,

	GHOST_kEventQuit,

	GHOST_kEventWindowClose,
	GHOST_kEventWindowActivate,
	GHOST_kEventWindowDeactivate,
	GHOST_kEventWindowUpdate,
	GHOST_kEventWindowSize,
	GHOST_kEventWindowMove,
	
	GHOST_kEventDraggingEntered,
	GHOST_kEventDraggingUpdated,
	GHOST_kEventDraggingExited,
	GHOST_kEventDraggingDropDone,
	
	GHOST_kEventOpenMainFile, // Needed for Cocoa to open double-clicked .blend file at startup

	GHOST_kEventTimer,

	GHOST_kNumEventTypes
} GHOST_TEventType;


typedef enum {
	GHOST_kStandardCursorFirstCursor = 0,
	GHOST_kStandardCursorDefault = 0,
	GHOST_kStandardCursorRightArrow,
	GHOST_kStandardCursorLeftArrow,
	GHOST_kStandardCursorInfo, 
	GHOST_kStandardCursorDestroy,
	GHOST_kStandardCursorHelp,    
	GHOST_kStandardCursorCycle,
	GHOST_kStandardCursorSpray,
	GHOST_kStandardCursorWait,
	GHOST_kStandardCursorText,
	GHOST_kStandardCursorCrosshair,
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
	GHOST_kStandardCursorNumCursors, 
	GHOST_kStandardCursorPencil
} GHOST_TStandardCursor;


typedef enum {
	GHOST_kKeyUnknown = -1,
	GHOST_kKeyBackSpace,
	GHOST_kKeyTab,
	GHOST_kKeyLinefeed,
	GHOST_kKeyClear,
	GHOST_kKeyEnter  = 0x0D,
	
	GHOST_kKeyEsc    = 0x1B,
	GHOST_kKeySpace  = ' ',
	GHOST_kKeyQuote  = 0x27,
	GHOST_kKeyComma  = ',',
	GHOST_kKeyMinus  = '-',
	GHOST_kKeyPeriod = '.',
	GHOST_kKeySlash  = '/',

	// Number keys
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
	GHOST_kKeyEqual     = '=',

	// Character keys
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

	GHOST_kKeyLeftBracket  = '[',
	GHOST_kKeyRightBracket = ']',
	GHOST_kKeyBackslash    = 0x5C,
	GHOST_kKeyAccentGrave  = '`',

	
	GHOST_kKeyLeftShift = 0x100,
	GHOST_kKeyRightShift,
	GHOST_kKeyLeftControl,
	GHOST_kKeyRightControl,
	GHOST_kKeyLeftAlt,
	GHOST_kKeyRightAlt,
    GHOST_kKeyCommand,				// APPLE only!
    GHOST_kKeyGrLess ,		// German PC only!

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

	// Numpad keys
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

	// Function keys
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
	GHOST_kKeyF24
} GHOST_TKey;

typedef enum {
	GHOST_kGrabDisable = 0,	/* grab not set */
	GHOST_kGrabNormal,	/* no cursor adjustments */
	GHOST_kGrabWrap,		/* wrap the mouse location to prevent limiting screen bounds */
	GHOST_kGrabHide,		/* hide the mouse while grabbing and restore the original location on release (numbuts) */
} GHOST_TGrabCursorMode;

typedef void* GHOST_TEventDataPtr;

typedef struct {
	/** The x-coordinate of the cursor position. */
	GHOST_TInt32 x;
	/** The y-coordinate of the cursor position. */
	GHOST_TInt32 y;
} GHOST_TEventCursorData;

typedef struct {
	/** The mask of the mouse button. */
	GHOST_TButtonMask button;
} GHOST_TEventButtonData;

typedef struct {
	/** Displacement of a mouse wheel. */
	GHOST_TInt32 z;	
} GHOST_TEventWheelData;

typedef enum {
	GHOST_kTrackpadEventUnknown =0,
	GHOST_kTrackpadEventScroll,
	GHOST_kTrackpadEventRotate,
	GHOST_kTrackpadEventSwipe, /* Reserved, not used for now */
	GHOST_kTrackpadEventMagnify
} GHOST_TTrackpadEventSubTypes;
	

typedef struct {
	/** The event subtype */
	GHOST_TTrackpadEventSubTypes subtype;
	/** The x-location of the trackpad event */
	GHOST_TInt32 x;
	/** The y-location of the trackpad event */
	GHOST_TInt32 y;
	/** The x-delta or value of the trackpad event */
	GHOST_TInt32 deltaX;
	/** The y-delta (currently only for scroll subtype) of the trackpad event */
	GHOST_TInt32 deltaY;
} GHOST_TEventTrackpadData;


typedef enum {
	GHOST_kDragnDropTypeUnknown =0,
	GHOST_kDragnDropTypeFilenames, /*Array of strings representing file names (full path) */
	GHOST_kDragnDropTypeString, /* Unformatted text UTF-8 string */
	GHOST_kDragnDropTypeBitmap /*Bitmap image data */
} GHOST_TDragnDropTypes;

typedef struct {
	/** The x-coordinate of the cursor position. */
	GHOST_TInt32 x;
	/** The y-coordinate of the cursor position. */
	GHOST_TInt32 y;
	/** The dropped item type */
	GHOST_TDragnDropTypes dataType;
	/** The "dropped content" */
	GHOST_TEventDataPtr data;
} GHOST_TEventDragnDropData;

typedef struct {
	int count;
	GHOST_TUns8 **strings;
} GHOST_TStringArray;


/* original patch used floats, but the driver return ints and uns. We will calibrate in view, no sense on doing conversions twice */
/* as all USB device controls are likely to use ints, this is also more future proof */
//typedef struct {
//   /** N-degree of freedom device data */
//   float tx, ty, tz;   /** -x left, +y up, +z forward */
//   float rx, ry, rz;
//   float dt;
//} GHOST_TEventNDOFData;

typedef struct {
   /** N-degree of freedom device data v2*/
   int changed;
   GHOST_TUns64 client;
   GHOST_TUns64 address;
   GHOST_TInt16 tx, ty, tz;   /** -x left, +y up, +z forward */
   GHOST_TInt16 rx, ry, rz;
   GHOST_TInt16 buttons;
   GHOST_TUns64 time;
   GHOST_TUns64 delta;
} GHOST_TEventNDOFData;

typedef int     (*GHOST_NDOFLibraryInit_fp)();
typedef void    (*GHOST_NDOFLibraryShutdown_fp)(void* deviceHandle);
typedef void*   (*GHOST_NDOFDeviceOpen_fp)(void* platformData);

// original patch windows callback. In mac os X version the callback is internal to the plug-in and post an event to main thead.
// not necessary faster, but better integration with other events. 

//typedef int     (*GHOST_NDOFEventHandler_fp)(float* result7, void* deviceHandle, unsigned int message, unsigned int* wParam, unsigned long* lParam);
//typedef void     (*GHOST_NDOFCallBack_fp)(GHOST_TEventNDOFDataV2 *VolDatas);

typedef struct {
	/** The key code. */
	GHOST_TKey		key;
	/** The ascii code for the key event ('\0' if none). */
	char			ascii;
} GHOST_TEventKeyData;

typedef struct {
	/** Number of pixels on a line. */
	GHOST_TUns32	xPixels;
	/** Number of lines. */
	GHOST_TUns32	yPixels;
	/** Numberof bits per pixel. */
	GHOST_TUns32	bpp;
	/** Refresh rate (in Hertz). */
	GHOST_TUns32	frequency;
} GHOST_DisplaySetting;


#ifdef _WIN32
typedef long GHOST_TEmbedderWindowID;
#endif // _WIN32

#ifndef _WIN32
// I can't use "Window" from "<X11/Xlib.h>" because it conflits with Window defined in winlay.h
typedef int GHOST_TEmbedderWindowID;
#endif // _WIN32

/**
 * A timer task callback routine.
 * @param task The timer task object.
 * @param time The current time.
 */
#ifdef __cplusplus
class GHOST_ITimerTask;
typedef void (*GHOST_TimerProcPtr)(GHOST_ITimerTask* task, GHOST_TUns64 time);
#else
struct GHOST_TimerTaskHandle__;
typedef void (*GHOST_TimerProcPtr)(struct GHOST_TimerTaskHandle__* task, GHOST_TUns64 time);
#endif

#endif // _GHOST_TYPES_H_

