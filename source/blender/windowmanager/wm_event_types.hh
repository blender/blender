/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

/*
 * These define have its origin at SGI, where all device defines were written down in device.h.
 * Blender copied the conventions quite some, and expanded it with internal new defines (ton)
 */

#pragma once

#include <cstdint>

/** #wmEvent.customdata type. */
enum {
  EVT_DATA_TIMER = 2,
  EVT_DATA_DRAGDROP = 3,
  EVT_DATA_NDOF_MOTION = 4,
  EVT_DATA_XR = 5,
};

/**
 * #wmTabletData.active tablet active, matches #GHOST_TTabletMode.
 *
 * Typically access via `event->tablet.active`.
 */
enum {
  EVT_TABLET_NONE = 0,
  EVT_TABLET_STYLUS = 1,
  EVT_TABLET_ERASER = 2,
};

/**
 * #wmEvent.type
 *
 * \note Also used for #wmKeyMapItem.type which is saved in key-map files,
 * do not change the values of existing values which can be used in key-maps.
 */
enum wmEventType : int16_t {
  /* Non-event, for example disabled timer. */
  EVENT_NONE = 0x0000,

/* ********** Start of Input devices. ********** */

/* Minimum mouse value (inclusive). */
#define _EVT_MOUSE_MIN 0x0001

  /* MOUSE: 0x000x, 0x001x. */
  LEFTMOUSE = 0x0001,   /* 1 */
  MIDDLEMOUSE = 0x0002, /* 2 */
  RIGHTMOUSE = 0x0003,  /* 3 */
  MOUSEMOVE = 0x0004,   /* 4 */
  /* Extra mouse buttons. */
  BUTTON4MOUSE = 0x0007, /* 7 */
  BUTTON5MOUSE = 0x0008, /* 8 */
  /* More mouse buttons - can't use 9 and 10 here (wheel). */
  BUTTON6MOUSE = 0x0012, /* 18 */
  BUTTON7MOUSE = 0x0013, /* 19 */
  /* Extra trackpad gestures (check #WM_EVENT_IS_CONSECUTIVE to detect motion events). */
  MOUSEPAN = 0x000e,       /* 14 */
  MOUSEZOOM = 0x000f,      /* 15 */
  MOUSEROTATE = 0x0010,    /* 13 */
  MOUSESMARTZOOM = 0x0017, /* 23 */

  /* Defaults from ghost. */
  WHEELUPMOUSE = 0x000a,   /* 10 */
  WHEELDOWNMOUSE = 0x000b, /* 11 */
  /* Mapped based on #USER_WHEELZOOMDIR. */
  WHEELINMOUSE = 0x000c,  /* 12 */
  WHEELOUTMOUSE = 0x000d, /* 13 */
  /* Successive MOUSEMOVE's are converted to this, so we can easily
   * ignore all but the most recent MOUSEMOVE (for better performance),
   * paint and drawing tools however will want to handle these. */
  INBETWEEN_MOUSEMOVE = 0x0011, /* 17 */
  /* Horizontal scrolling events. */
  WHEELLEFTMOUSE = 0x0014,  /* 20 */
  WHEELRIGHTMOUSE = 0x0015, /* 21 */

/* Maximum mouse value (inclusive). */
#define _EVT_MOUSE_MAX 0x0015 /* 21 */

  /* IME event, GHOST_kEventImeCompositionStart in ghost. */
  WM_IME_COMPOSITE_START = 0x0016, /* 22 */
  /* 0x0017 is MOUSESMARTZOOM. */
  /* IME event, GHOST_kEventImeComposition in ghost. */
  WM_IME_COMPOSITE_EVENT = 0x0018, /* 24 */
  /* IME event, GHOST_kEventImeCompositionEnd in ghost. */
  WM_IME_COMPOSITE_END = 0x0019, /* 25 */

  /* Tablet/Pen Specific Events. */
  TABLET_STYLUS = 0x001a, /* 26 */
  TABLET_ERASER = 0x001b, /* 27 */

/* *** Start of keyboard codes. *** */

/* Minimum keyboard value (inclusive). */
#define _EVT_KEYBOARD_MIN 0x0020 /* 32 */

  /* Standard keyboard.
   * - 0x0020 to 0x00ff [#_EVT_KEYBOARD_MIN to #_EVT_KEYBOARD_MAX] inclusive - for keys.
   * - 0x012c to 0x0143 [#EVT_F1KEY to #EVT_F24KEY] inclusive - for function keys. */

  EVT_ZEROKEY = 0x0030,  /* '0' (48). */
  EVT_ONEKEY = 0x0031,   /* '1' (49). */
  EVT_TWOKEY = 0x0032,   /* '2' (50). */
  EVT_THREEKEY = 0x0033, /* '3' (51). */
  EVT_FOURKEY = 0x0034,  /* '4' (52). */
  EVT_FIVEKEY = 0x0035,  /* '5' (53). */
  EVT_SIXKEY = 0x0036,   /* '6' (54). */
  EVT_SEVENKEY = 0x0037, /* '7' (55). */
  EVT_EIGHTKEY = 0x0038, /* '8' (56). */
  EVT_NINEKEY = 0x0039,  /* '9' (57). */

  EVT_AKEY = 0x0061, /* 'a' (97). */
  EVT_BKEY = 0x0062, /* 'b' (98). */
  EVT_CKEY = 0x0063, /* 'c' (99). */
  EVT_DKEY = 0x0064, /* 'd' (100). */
  EVT_EKEY = 0x0065, /* 'e' (101). */
  EVT_FKEY = 0x0066, /* 'f' (102). */
  EVT_GKEY = 0x0067, /* 'g' (103). */
  EVT_HKEY = 0x0068, /* 'h' (104). */
  EVT_IKEY = 0x0069, /* 'i' (105). */
  EVT_JKEY = 0x006a, /* 'j' (106). */
  EVT_KKEY = 0x006b, /* 'k' (107). */
  EVT_LKEY = 0x006c, /* 'l' (108). */
  EVT_MKEY = 0x006d, /* 'm' (109). */
  EVT_NKEY = 0x006e, /* 'n' (110). */
  EVT_OKEY = 0x006f, /* 'o' (111). */
  EVT_PKEY = 0x0070, /* 'p' (112). */
  EVT_QKEY = 0x0071, /* 'q' (113). */
  EVT_RKEY = 0x0072, /* 'r' (114). */
  EVT_SKEY = 0x0073, /* 's' (115). */
  EVT_TKEY = 0x0074, /* 't' (116). */
  EVT_UKEY = 0x0075, /* 'u' (117). */
  EVT_VKEY = 0x0076, /* 'v' (118). */
  EVT_WKEY = 0x0077, /* 'w' (119). */
  EVT_XKEY = 0x0078, /* 'x' (120). */
  EVT_YKEY = 0x0079, /* 'y' (121). */
  EVT_ZKEY = 0x007a, /* 'z' (122). */

  EVT_LEFTARROWKEY = 0x0089,  /* 137 */
  EVT_DOWNARROWKEY = 0x008a,  /* 138 */
  EVT_RIGHTARROWKEY = 0x008b, /* 139 */
  EVT_UPARROWKEY = 0x008c,    /* 140 */

  EVT_PAD0 = 0x0096, /* 150 */
  EVT_PAD1 = 0x0097, /* 151 */
  EVT_PAD2 = 0x0098, /* 152 */
  EVT_PAD3 = 0x0099, /* 153 */
  EVT_PAD4 = 0x009a, /* 154 */
  EVT_PAD5 = 0x009b, /* 155 */
  EVT_PAD6 = 0x009c, /* 156 */
  EVT_PAD7 = 0x009d, /* 157 */
  EVT_PAD8 = 0x009e, /* 158 */
  EVT_PAD9 = 0x009f, /* 159 */
  /* Key-pad keys. */
  EVT_PADASTERKEY = 0x00a0, /* 160 */
  EVT_PADSLASHKEY = 0x00a1, /* 161 */
  EVT_PADMINUS = 0x00a2,    /* 162 */
  EVT_PADENTER = 0x00a3,    /* 163 */
  EVT_PADPLUSKEY = 0x00a4,  /* 164 */

  EVT_PAUSEKEY = 0x00a5,    /* 165 */
  EVT_INSERTKEY = 0x00a6,   /* 166 */
  EVT_HOMEKEY = 0x00a7,     /* 167 */
  EVT_PAGEUPKEY = 0x00a8,   /* 168 */
  EVT_PAGEDOWNKEY = 0x00a9, /* 169 */
  EVT_ENDKEY = 0x00aa,      /* 170 */
  /* Note that 'PADPERIOD' is defined out-of-order. */
  EVT_UNKNOWNKEY = 0x00ab, /* 171 */
  /** OS modifier, see: #KM_OSKEY for details. */
  EVT_OSKEY = 0x00ac,     /* 172 */
  EVT_GRLESSKEY = 0x00ad, /* 173 */
  /* Media keys. */
  EVT_MEDIAPLAY = 0x00ae,  /* 174 */
  EVT_MEDIASTOP = 0x00af,  /* 175 */
  EVT_MEDIAFIRST = 0x00b0, /* 176 */
  EVT_MEDIALAST = 0x00b1,  /* 177 */
  /* Menu/App key. */
  EVT_APPKEY = 0x00b2, /* 178 */

  /** Additional modifier, see: #KM_HYPER for details. */
  EVT_HYPER = 0x00b3, /* 179 */

  EVT_PADPERIOD = 0x00c7, /* 199 */

  EVT_CAPSLOCKKEY = 0x00d3, /* 211 */

  /* Modifier keys. */
  EVT_LEFTCTRLKEY = 0x00d4,   /* 212 */
  EVT_LEFTALTKEY = 0x00d5,    /* 213 */
  EVT_RIGHTALTKEY = 0x00d6,   /* 214 */
  EVT_RIGHTCTRLKEY = 0x00d7,  /* 215 */
  EVT_RIGHTSHIFTKEY = 0x00d8, /* 216 */
  EVT_LEFTSHIFTKEY = 0x00d9,  /* 217 */
  /* Special characters. */
  EVT_ESCKEY = 0x00da,          /* 218 */
  EVT_TABKEY = 0x00db,          /* 219 */
  EVT_RETKEY = 0x00dc,          /* 220 */
  EVT_SPACEKEY = 0x00dd,        /* 221 */
  EVT_LINEFEEDKEY = 0x00de,     /* 222 */
  EVT_BACKSPACEKEY = 0x00df,    /* 223 */
  EVT_DELKEY = 0x00e0,          /* 224 */
  EVT_SEMICOLONKEY = 0x00e1,    /* 225 */
  EVT_PERIODKEY = 0x00e2,       /* 226 */
  EVT_COMMAKEY = 0x00e3,        /* 227 */
  EVT_QUOTEKEY = 0x00e4,        /* 228 */
  EVT_ACCENTGRAVEKEY = 0x00e5,  /* 229 */
  EVT_MINUSKEY = 0x00e6,        /* 230 */
  EVT_PLUSKEY = 0x00e7,         /* 231 */
  EVT_SLASHKEY = 0x00e8,        /* 232 */
  EVT_BACKSLASHKEY = 0x00e9,    /* 233 */
  EVT_EQUALKEY = 0x00ea,        /* 234 */
  EVT_LEFTBRACKETKEY = 0x00eb,  /* 235 */
  EVT_RIGHTBRACKETKEY = 0x00ec, /* 236 */

/* Maximum keyboard value (inclusive). */
#define _EVT_KEYBOARD_MAX 0x00ff /* 255 */

  /* WARNING: 0x010x are used for internal events
   * (but are still stored in the key-map). */

  EVT_F1KEY = 0x012c,  /* 300 */
  EVT_F2KEY = 0x012d,  /* 301 */
  EVT_F3KEY = 0x012e,  /* 302 */
  EVT_F4KEY = 0x012f,  /* 303 */
  EVT_F5KEY = 0x0130,  /* 304 */
  EVT_F6KEY = 0x0131,  /* 305 */
  EVT_F7KEY = 0x0132,  /* 306 */
  EVT_F8KEY = 0x0133,  /* 307 */
  EVT_F9KEY = 0x0134,  /* 308 */
  EVT_F10KEY = 0x0135, /* 309 */
  EVT_F11KEY = 0x0136, /* 310 */
  EVT_F12KEY = 0x0137, /* 311 */
  EVT_F13KEY = 0x0138, /* 312 */
  EVT_F14KEY = 0x0139, /* 313 */
  EVT_F15KEY = 0x013a, /* 314 */
  EVT_F16KEY = 0x013b, /* 315 */
  EVT_F17KEY = 0x013c, /* 316 */
  EVT_F18KEY = 0x013d, /* 317 */
  EVT_F19KEY = 0x013e, /* 318 */
  EVT_F20KEY = 0x013f, /* 319 */
  EVT_F21KEY = 0x0140, /* 320 */
  EVT_F22KEY = 0x0141, /* 321 */
  EVT_F23KEY = 0x0142, /* 322 */
  EVT_F24KEY = 0x0143, /* 323 */

  /* *** End of keyboard codes. *** */

  /* NDOF (from "Space Navigator" & friends)
   * These must be kept in sync with `GHOST_NDOFManager.hh`.
   * Ordering matters, exact values do not. */

  /**
   * Motion from 3D input (translation & rotation).
   * Check #WM_EVENT_IS_CONSECUTIVE to detect motion events.
   */
  NDOF_MOTION = 0x0190, /* 400 */

#define _NDOF_MIN NDOF_MOTION
#define _NDOF_BUTTON_MIN NDOF_BUTTON_MENU

  /* These two are available from any 3Dconnexion device. */

  NDOF_BUTTON_MENU = 0x0191, /* 401 */
  NDOF_BUTTON_FIT = 0x0192,  /* 402 */
  /* Standard views. */
  NDOF_BUTTON_TOP = 0x0193,    /* 403 */
  NDOF_BUTTON_BOTTOM = 0x0194, /* 404 */
  NDOF_BUTTON_LEFT = 0x0195,   /* 405 */
  NDOF_BUTTON_RIGHT = 0x0196,  /* 406 */
  NDOF_BUTTON_FRONT = 0x0197,  /* 407 */
  NDOF_BUTTON_BACK = 0x0198,   /* 408 */
  /* More views. */
  NDOF_BUTTON_ISO1 = 0x0199, /* 409 */
  NDOF_BUTTON_ISO2 = 0x019a, /* 410 */
  /* 90 degree rotations. */
  NDOF_BUTTON_ROLL_CW = 0x019b,  /* 411 */
  NDOF_BUTTON_ROLL_CCW = 0x019c, /* 412 */
  NDOF_BUTTON_SPIN_CW = 0x019d,  /* 413 */
  NDOF_BUTTON_SPIN_CCW = 0x019e, /* 414 */
  NDOF_BUTTON_TILT_CW = 0x019f,  /* 415 */
  NDOF_BUTTON_TILT_CCW = 0x01a0, /* 416 */
  /* Device control. */
  NDOF_BUTTON_ROTATE = 0x01a1,   /* 417 */
  NDOF_BUTTON_PANZOOM = 0x01a2,  /* 418 */
  NDOF_BUTTON_DOMINANT = 0x01a3, /* 419 */
  NDOF_BUTTON_PLUS = 0x01a4,     /* 420 */
  NDOF_BUTTON_MINUS = 0x01a5,    /* 421 */
  /* Restore views. */
  NDOF_BUTTON_V1 = 0x01a6, /* 422 */
  NDOF_BUTTON_V2 = 0x01a7, /* 423 */
  NDOF_BUTTON_V3 = 0x01a8, /* 424 */
  /* General-purpose buttons. */
  NDOF_BUTTON_1 = 0x01aa,  /* 426 */
  NDOF_BUTTON_2 = 0x01ab,  /* 427 */
  NDOF_BUTTON_3 = 0x01ac,  /* 428 */
  NDOF_BUTTON_4 = 0x01ad,  /* 429 */
  NDOF_BUTTON_5 = 0x01ae,  /* 430 */
  NDOF_BUTTON_6 = 0x01af,  /* 431 */
  NDOF_BUTTON_7 = 0x01b0,  /* 432 */
  NDOF_BUTTON_8 = 0x01b1,  /* 433 */
  NDOF_BUTTON_9 = 0x01b2,  /* 434 */
  NDOF_BUTTON_10 = 0x01b3, /* 435 */
  /* More general-purpose buttons. */
  NDOF_BUTTON_11 = 0x01b4, /* 436 */
  NDOF_BUTTON_12 = 0x01b5, /* 437 */

/* Disabled as GHOST converts these to keyboard events
 * which use regular keyboard event handling logic. */
#if 0
  /* Keyboard emulation. */
  NDOF_BUTTON_ESC = 0x01b6,    /* 438 */
  NDOF_BUTTON_ENTER = 0x01b7,  /* 439 */
  NDOF_BUTTON_DELETE = 0x01b8, /* 440 */
  NDOF_BUTTON_TAB = 0x01b9,    /* 441 */
  NDOF_BUTTON_SPACE = 0x01ba,  /* 442 */
  NDOF_BUTTON_ALT = 0x01bb,    /* 443 */
  NDOF_BUTTON_SHIFT = 0x01bc,  /* 444 */
  NDOF_BUTTON_CTRL = 0x01bd,   /* 445 */
#endif

  /* Store views. */
  NDOF_BUTTON_SAVE_V1 = 0x01be, /* 446 */
  NDOF_BUTTON_SAVE_V2 = 0x01bf, /* 447 */
  NDOF_BUTTON_SAVE_V3 = 0x01c0, /* 448 */

#define _NDOF_MAX NDOF_BUTTON_SAVE_V3
#define _NDOF_BUTTON_MAX NDOF_BUTTON_SAVE_V3

  /* ********** End of Input devices. ********** */

  /* ********** Start of Blender internal events. ********** */

  /* XXX Those are mixed inside keyboard 'area'! */
  /* System: 0x010x. */
  // INPUTCHANGE = 0x0103,   /* Input connected or disconnected, (259). */ /* UNUSED. */
  WINDEACTIVATE = 0x0104, /* Window is deactivated, focus lost, (260). */
  /* Timer: 0x011x. */
  TIMER = 0x0110,         /* Timer event, passed on to all queues (272). */
  TIMER0 = 0x0111,        /* Timer event, slot for internal use (273). */
  TIMER1 = 0x0112,        /* Timer event, slot for internal use (274). */
  TIMER2 = 0x0113,        /* Timer event, slot for internal use (275). */
  TIMERJOBS = 0x0114,     /* Timer event, jobs system (276). */
  TIMERAUTOSAVE = 0x0115, /* Timer event, autosave (277). */
  TIMERREPORT = 0x0116,   /* Timer event, reports (278). */
  TIMERREGION = 0x0117,   /* Timer event, region slide in/out (279). */
  TIMERNOTIFIER = 0x0118, /* Timer event, notifier sender (280). */
/* Timer max (287). */
#define _TIMER_MAX 0x011F

  /* Action-zones, tweak, gestures: 0x500x, 0x501x
   * Keep in sync with #IS_EVENT_ACTIONZONE(...). */
  EVT_ACTIONZONE_AREA = 0x5000,       /* 20480 */
  EVT_ACTIONZONE_REGION = 0x5001,     /* 20481 */
  EVT_ACTIONZONE_FULLSCREEN = 0x5011, /* 20497 */

  /* NOTE: these values are saved in key-map files, do not change them but just add new ones. */

  /* 0x5011 is taken, see #EVT_ACTIONZONE_FULLSCREEN. */

  /* Misc Blender internals: 0x502x. */
  EVT_FILESELECT = 0x5020, /* 20512 */
  EVT_BUT_OPEN = 0x5021,   /* 20513 */
  EVT_MODAL_MAP = 0x5022,  /* 20514 */
  EVT_DROP = 0x5023,       /* 20515 */
  /* When value is 0, re-activate, when 1, don't re-activate the button under the cursor. */
  EVT_BUT_CANCEL = 0x5024, /* 20516 */

  /* Could become gizmo callback. */
  EVT_GIZMO_UPDATE = 0x5025, /* 20517 */

  /* XR events: 0x503x. */
  EVT_XR_ACTION = 0x5030, /* 20528 */
  /* ********** End of Blender internal events. ********** */
};

/* -------------------------------------------------------------------- */
/** \name #wmEvent.type Helpers
 * \{ */

/** Test whether the event is timer event. */
#define ISTIMER(event_type) ((event_type) >= TIMER && (event_type) <= _TIMER_MAX)

/** Test whether the event is a key on the keyboard (including modifier keys). */
#define ISKEYBOARD(event_type) \
  (((event_type) >= _EVT_KEYBOARD_MIN && (event_type) <= _EVT_KEYBOARD_MAX) || \
   ((event_type) >= EVT_F1KEY && (event_type) <= EVT_F24KEY))

/**
 * Test whether the event is a key on the keyboard
 * or any other kind of button that supports press & release
 * (use for click & click-drag detection).
 *
 * \note Mouse wheel events are excluded from this macro, while they do generate press events it
 * doesn't make sense to have click & click-drag events for a mouse-wheel as it can't be held down.
 */
#define ISKEYBOARD_OR_BUTTON(event_type) \
  (ISMOUSE_BUTTON(event_type) || ISKEYBOARD(event_type) || ISNDOF_BUTTON(event_type))

/** Test whether the event is a modifier key. */
#define ISKEYMODIFIER(event_type) \
  (((event_type) >= EVT_LEFTCTRLKEY && (event_type) <= EVT_LEFTSHIFTKEY) || \
   ELEM((event_type), EVT_OSKEY, EVT_HYPER))

/**
 * Test whether the event is any kind:
 * #ISMOUSE_MOTION, #ISMOUSE_BUTTON, #ISMOUSE_WHEEL & #ISMOUSE_GESTURE.
 *
 * \note It's best to use more specific check if possible as mixing motion/buttons/gestures
 * is very broad and not necessarily obvious which kinds of events are important.
 */
#define ISMOUSE(event_type) ((event_type) >= _EVT_MOUSE_MIN && (event_type) <= _EVT_MOUSE_MAX)
/** Test whether the event is a mouse button (excluding mouse-wheel). */
#define ISMOUSE_MOTION(event_type) ELEM(event_type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)
/** Test whether the event is a mouse button (excluding mouse-wheel). */
#define ISMOUSE_BUTTON(event_type) \
  (ELEM(event_type, \
        LEFTMOUSE, \
        MIDDLEMOUSE, \
        RIGHTMOUSE, \
        BUTTON4MOUSE, \
        BUTTON5MOUSE, \
        BUTTON6MOUSE, \
        BUTTON7MOUSE))
/** Test whether the event is a mouse wheel. */
#define ISMOUSE_WHEEL(event_type) \
  (((event_type) >= WHEELUPMOUSE && (event_type) <= WHEELOUTMOUSE) || \
   ELEM((event_type), WHEELLEFTMOUSE, WHEELRIGHTMOUSE))
/** Test whether the event is a mouse (trackpad) gesture. */
#define ISMOUSE_GESTURE(event_type) ((event_type) >= MOUSEPAN && (event_type) <= MOUSESMARTZOOM)

/** Test whether the event is a NDOF event. */
#define ISNDOF(event_type) ((event_type) >= _NDOF_MIN && (event_type) <= _NDOF_MAX)
#define ISNDOF_BUTTON(event_type) \
  ((event_type) >= _NDOF_BUTTON_MIN && (event_type) <= _NDOF_BUTTON_MAX)

#define IS_EVENT_ACTIONZONE(event_type) \
  ELEM(event_type, EVT_ACTIONZONE_AREA, EVT_ACTIONZONE_REGION, EVT_ACTIONZONE_FULLSCREEN)

/** Test whether event type is acceptable as hotkey (excluding modifiers). */
#define ISHOTKEY(event_type) \
  ((ISKEYBOARD(event_type) || ISMOUSE_BUTTON(event_type) || ISMOUSE_WHEEL(event_type) || \
    ISNDOF_BUTTON(event_type)) && \
   (ISKEYMODIFIER(event_type) == false))

enum eEventType_Mask {
  /** #ISKEYMODIFIER. */
  EVT_TYPE_MASK_KEYBOARD_MODIFIER = (1 << 0),
  /** #ISKEYBOARD. */
  EVT_TYPE_MASK_KEYBOARD = (1 << 1),
  /** #ISMOUSE_WHEEL. */
  EVT_TYPE_MASK_MOUSE_WHEEL = (1 << 2),
  /** #ISMOUSE_BUTTON. */
  EVT_TYPE_MASK_MOUSE_GESTURE = (1 << 3),
  /** #ISMOUSE_GESTURE. */
  EVT_TYPE_MASK_MOUSE_BUTTON = (1 << 4),
  /** #ISMOUSE. */
  EVT_TYPE_MASK_MOUSE = (1 << 5),
  /** #ISNDOF. */
  EVT_TYPE_MASK_NDOF = (1 << 6),
  /** #IS_EVENT_ACTIONZONE. */
  EVT_TYPE_MASK_ACTIONZONE = (1 << 7),
};
#define EVT_TYPE_MASK_ALL \
  (EVT_TYPE_MASK_KEYBOARD | EVT_TYPE_MASK_MOUSE | EVT_TYPE_MASK_NDOF | EVT_TYPE_MASK_ACTIONZONE)

#define EVT_TYPE_MASK_HOTKEY_INCLUDE \
  (EVT_TYPE_MASK_KEYBOARD | EVT_TYPE_MASK_MOUSE | EVT_TYPE_MASK_NDOF)
#define EVT_TYPE_MASK_HOTKEY_EXCLUDE EVT_TYPE_MASK_KEYBOARD_MODIFIER

bool WM_event_type_mask_test(int event_type, enum eEventType_Mask mask);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmEvent.val Values
 * \{ */

/* Gestures. */

/**
 * File selector.
 * When #wmEvent::type is #EVT_FILESELECT, this value is stored in #wmEvent::val.
 */
enum {
  EVT_FILESELECT_FULL_OPEN = 1,
  EVT_FILESELECT_EXEC = 2,
  EVT_FILESELECT_CANCEL = 3,
  EVT_FILESELECT_EXTERNAL_CANCEL = 4,
};

/**
 * Gesture
 * Used in #wmEvent.val
 *
 * \note These values are saved in keymap files,
 * do not change them but just add new ones.
 */
enum {
  GESTURE_MODAL_CANCEL = 1,
  GESTURE_MODAL_CONFIRM = 2,

  /** Uses 'deselect' operator property. */
  GESTURE_MODAL_SELECT = 3,
  GESTURE_MODAL_DESELECT = 4,

  /** Circle select: when no mouse button is pressed. */
  GESTURE_MODAL_NOP = 5,

  /** Circle select: larger brush. */
  GESTURE_MODAL_CIRCLE_ADD = 6,
  /** Circle select: smaller brush. */
  GESTURE_MODAL_CIRCLE_SUB = 7,

  /** Box select/straight line, activate, use release to detect which button. */
  GESTURE_MODAL_BEGIN = 8,

  /** Uses 'zoom_out' operator property. */
  GESTURE_MODAL_IN = 9,
  GESTURE_MODAL_OUT = 10,

  /** Circle select: size brush (for trackpad event). */
  GESTURE_MODAL_CIRCLE_SIZE = 11,

  /** Move selection area. */
  GESTURE_MODAL_MOVE = 12,

  /** Toggle to activate snapping (angle snapping for straight line). */
  GESTURE_MODAL_SNAP = 13,

  /** Toggle to activate flip (flip the active side of a straight line). */
  GESTURE_MODAL_FLIP = 14,
};

/** \} */
