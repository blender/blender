/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/wm_event_types.h
 *  \ingroup wm
 */


/*
 *  These define have its origin at sgi, where all device defines were written down in device.h.
 *  Blender copied the conventions quite some, and expanded it with internal new defines (ton)
 *
 */ 


#ifndef __WM_EVENT_TYPES_H__
#define __WM_EVENT_TYPES_H__

/* customdata type */
enum {
	EVT_DATA_GESTURE     = 1,
	EVT_DATA_TIMER       = 2,
	EVT_DATA_DRAGDROP    = 3,
	EVT_DATA_NDOF_MOTION = 4,
};

/* tablet active, matches GHOST_TTabletMode */
enum {
	EVT_TABLET_NONE   = 0,
	EVT_TABLET_STYLUS = 1,
	EVT_TABLET_ERASER = 2,
};

/* ********** wmEvent.type ********** */
enum {
	/* non-event, for example disabled timer */
	EVENT_NONE = 0x0000,

	/* ********** Start of Input devices. ********** */

	/* MOUSE : 0x000x, 0x001x */
	LEFTMOUSE           = 0x0001,
	MIDDLEMOUSE         = 0x0002,
	RIGHTMOUSE          = 0x0003,
	MOUSEMOVE           = 0x0004,
	/* only use if you want user option switch possible */
	ACTIONMOUSE         = 0x0005,
	SELECTMOUSE         = 0x0006,
	/* Extra mouse buttons */
	BUTTON4MOUSE        = 0x0007,
	BUTTON5MOUSE        = 0x0008,
	/* More mouse buttons - can't use 9 and 10 here (wheel) */
	BUTTON6MOUSE        = 0x0012,
	BUTTON7MOUSE        = 0x0013,
	/* Extra trackpad gestures */
	MOUSEPAN            = 0x000e,
	MOUSEZOOM           = 0x000f,
	MOUSEROTATE         = 0x0010,
	/* defaults from ghost */
	WHEELUPMOUSE        = 0x000a,
	WHEELDOWNMOUSE      = 0x000b,
	/* mapped with userdef */
	WHEELINMOUSE        = 0x000c,
	WHEELOUTMOUSE       = 0x000d,
	/* Successive MOUSEMOVE's are converted to this, so we can easily
	 * ignore all but the most recent MOUSEMOVE (for better performance),
	 * paint and drawing tools however will want to handle these. */
	INBETWEEN_MOUSEMOVE = 0x0011,

	/* *** Start of keyboard codes. *** */

	/* standard keyboard.
	 * XXX from 0x0020 to 0x00ff, and 0x012c to 0x013f for function keys! */
	AKEY            = 0x0061,  /* 'a' */
	BKEY            = 0x0062,  /* 'b' */
	CKEY            = 0x0063,  /* 'c' */
	DKEY            = 0x0064,  /* 'd' */
	EKEY            = 0x0065,  /* 'e' */
	FKEY            = 0x0066,  /* 'f' */
	GKEY            = 0x0067,  /* 'g' */
	HKEY            = 0x0068,  /* 'h' */
	IKEY            = 0x0069,  /* 'i' */
	JKEY            = 0x006a,  /* 'j' */
	KKEY            = 0x006b,  /* 'k' */
	LKEY            = 0x006c,  /* 'l' */
	MKEY            = 0x006d,  /* 'm' */
	NKEY            = 0x006e,  /* 'n' */
	OKEY            = 0x006f,  /* 'o' */
	PKEY            = 0x0070,  /* 'p' */
	QKEY            = 0x0071,  /* 'q' */
	RKEY            = 0x0072,  /* 'r' */
	SKEY            = 0x0073,  /* 's' */
	TKEY            = 0x0074,  /* 't' */
	UKEY            = 0x0075,  /* 'u' */
	VKEY            = 0x0076,  /* 'v' */
	WKEY            = 0x0077,  /* 'w' */
	XKEY            = 0x0078,  /* 'x' */
	YKEY            = 0x0079,  /* 'y' */
	ZKEY            = 0x007a,  /* 'z' */

	ZEROKEY         = 0x0030,  /* '0' */
	ONEKEY          = 0x0031,  /* '1' */
	TWOKEY          = 0x0032,  /* '2' */
	THREEKEY        = 0x0033,  /* '3' */
	FOURKEY         = 0x0034,  /* '4' */
	FIVEKEY         = 0x0035,  /* '5' */
	SIXKEY          = 0x0036,  /* '6' */
	SEVENKEY        = 0x0037,  /* '7' */
	EIGHTKEY        = 0x0038,  /* '8' */
	NINEKEY         = 0x0039,  /* '9' */

	CAPSLOCKKEY     = 0x00d3,  /* 211 */

	LEFTCTRLKEY     = 0x00d4,  /* 212 */
	LEFTALTKEY      = 0x00d5,  /* 213 */
	RIGHTALTKEY     = 0x00d6,  /* 214 */
	RIGHTCTRLKEY    = 0x00d7,  /* 215 */
	RIGHTSHIFTKEY   = 0x00d8,  /* 216 */
	LEFTSHIFTKEY    = 0x00d9,  /* 217 */

	ESCKEY          = 0x00da,  /* 218 */
	TABKEY          = 0x00db,  /* 219 */
	RETKEY          = 0x00dc,  /* 220 */
	SPACEKEY        = 0x00dd,  /* 221 */
	LINEFEEDKEY     = 0x00de,  /* 222 */
	BACKSPACEKEY    = 0x00df,  /* 223 */
	DELKEY          = 0x00e0,  /* 224 */
	SEMICOLONKEY    = 0x00e1,  /* 225 */
	PERIODKEY       = 0x00e2,  /* 226 */
	COMMAKEY        = 0x00e3,  /* 227 */
	QUOTEKEY        = 0x00e4,  /* 228 */
	ACCENTGRAVEKEY  = 0x00e5,  /* 229 */
	MINUSKEY        = 0x00e6,  /* 230 */
	SLASHKEY        = 0x00e8,  /* 232 */
	BACKSLASHKEY    = 0x00e9,  /* 233 */
	EQUALKEY        = 0x00ea,  /* 234 */
	LEFTBRACKETKEY  = 0x00eb,  /* 235 */
	RIGHTBRACKETKEY = 0x00ec,  /* 236 */

	LEFTARROWKEY    = 0x0089,  /* 137 */
	DOWNARROWKEY    = 0x008a,  /* 138 */
	RIGHTARROWKEY   = 0x008b,  /* 139 */
	UPARROWKEY      = 0x008c,  /* 140 */

	PAD0            = 0x0096,  /* 150 */
	PAD1            = 0x0097,  /* 151 */
	PAD2            = 0x0098,  /* 152 */
	PAD3            = 0x0099,  /* 153 */
	PAD4            = 0x009a,  /* 154 */
	PAD5            = 0x009b,  /* 155 */
	PAD6            = 0x009c,  /* 156 */
	PAD7            = 0x009d,  /* 157 */
	PAD8            = 0x009e,  /* 158 */
	PAD9            = 0x009f,  /* 159 */

	PADPERIOD       = 0x00c7,  /* 199 */
	PADASTERKEY     = 0x00a0,  /* 160 */
	PADSLASHKEY     = 0x00a1,  /* 161 */
	PADMINUS        = 0x00a2,  /* 162 */
	PADENTER        = 0x00a3,  /* 163 */
	PADPLUSKEY      = 0x00a4,  /* 164 */

	PAUSEKEY        = 0x00a5,  /* 165 */
	INSERTKEY       = 0x00a6,  /* 166 */
	HOMEKEY         = 0x00a7,  /* 167 */
	PAGEUPKEY       = 0x00a8,  /* 168 */
	PAGEDOWNKEY     = 0x00a9,  /* 169 */
	ENDKEY          = 0x00aa,  /* 170 */

	UNKNOWNKEY      = 0x00ab,  /* 171 */
	OSKEY           = 0x00ac,  /* 172 */
	GRLESSKEY       = 0x00ad,  /* 173 */

	/* XXX: are these codes ok? */
	MEDIAPLAY       = 0x00ae,  /* 174 */
	MEDIASTOP       = 0x00af,  /* 175 */
	MEDIAFIRST      = 0x00b0,  /* 176 */
	MEDIALAST       = 0x00b1,  /* 177 */

	F1KEY           = 0x012c,  /* 300 */
	F2KEY           = 0x012d,  /* 301 */
	F3KEY           = 0x012e,  /* 302 */
	F4KEY           = 0x012f,  /* 303 */
	F5KEY           = 0x0130,  /* 304 */
	F6KEY           = 0x0131,  /* 305 */
	F7KEY           = 0x0132,  /* 306 */
	F8KEY           = 0x0133,  /* 307 */
	F9KEY           = 0x0134,  /* 308 */
	F10KEY          = 0x0135,  /* 309 */
	F11KEY          = 0x0136,  /* 310 */
	F12KEY          = 0x0137,  /* 311 */
	F13KEY          = 0x0138,  /* 312 */
	F14KEY          = 0x0139,  /* 313 */
	F15KEY          = 0x013a,  /* 314 */
	F16KEY          = 0x013b,  /* 315 */
	F17KEY          = 0x013c,  /* 316 */
	F18KEY          = 0x013d,  /* 317 */
	F19KEY          = 0x013e,  /* 318 */

	/* *** End of keyboard codes. *** */

	/* NDOF (from SpaceNavigator & friends)
	 * These should be kept in sync with GHOST_NDOFManager.h
	 * Ordering matters, exact values do not. */
	NDOF_MOTION = 0x0190,
	/* used internally, never sent */
	NDOF_BUTTON_NONE = NDOF_MOTION,
	/* these two are available from any 3Dconnexion device */
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_FIT,
	/* standard views */
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_BOTTOM,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_BACK,
	/* more views */
	NDOF_BUTTON_ISO1,
	NDOF_BUTTON_ISO2,
	/* 90 degree rotations */
	NDOF_BUTTON_ROLL_CW,
	NDOF_BUTTON_ROLL_CCW,
	NDOF_BUTTON_SPIN_CW,
	NDOF_BUTTON_SPIN_CCW,
	NDOF_BUTTON_TILT_CW,
	NDOF_BUTTON_TILT_CCW,
	/* device control */
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_PANZOOM,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	/* keyboard emulation */
	NDOF_BUTTON_ESC,
	NDOF_BUTTON_ALT,
	NDOF_BUTTON_SHIFT,
	NDOF_BUTTON_CTRL,
	/* general-purpose buttons */
	NDOF_BUTTON_1,
	NDOF_BUTTON_2,
	NDOF_BUTTON_3,
	NDOF_BUTTON_4,
	NDOF_BUTTON_5,
	NDOF_BUTTON_6,
	NDOF_BUTTON_7,
	NDOF_BUTTON_8,
	NDOF_BUTTON_9,
	NDOF_BUTTON_10,
	/* more general-purpose buttons */
	NDOF_BUTTON_A,
	NDOF_BUTTON_B,
	NDOF_BUTTON_C,
	/* the end */
	NDOF_LAST,

	/* ********** End of Input devices. ********** */

	/* ********** Start of Blender internal events. ********** */

	/* XXX Those are mixed inside keyboard 'area'! */
	/* System: 0x010x */
	INPUTCHANGE           = 0x0103,  /* input connected or disconnected */
	WINDEACTIVATE         = 0x0104,  /* window is deactivated, focus lost */
	/* Timer: 0x011x */
	TIMER                 = 0x0110,  /* timer event, passed on to all queues */
	TIMER0                = 0x0111,  /* timer event, slot for internal use */
	TIMER1                = 0x0112,  /* timer event, slot for internal use */
	TIMER2                = 0x0113,  /* timer event, slot for internal use */
	TIMERJOBS             = 0x0114,  /* timer event, jobs system */
	TIMERAUTOSAVE         = 0x0115,  /* timer event, autosave */
	TIMERREPORT           = 0x0116,  /* timer event, reports */
	TIMERREGION           = 0x0117,  /* timer event, region slide in/out */
	TIMERF                = 0x011F,  /* last timer */

	/* Tweak, gestures: 0x500x, 0x501x */
	EVT_ACTIONZONE_AREA   = 0x5000,
	EVT_ACTIONZONE_REGION = 0x5001,
	/* tweak events, for L M R mousebuttons */
	EVT_TWEAK_L           = 0x5002,
	EVT_TWEAK_M           = 0x5003,
	EVT_TWEAK_R           = 0x5004,
	/* tweak events for action or select mousebutton */
	EVT_TWEAK_A           = 0x5005,
	EVT_TWEAK_S           = 0x5006,
	EVT_GESTURE           = 0x5010,

	/* Misc Blender internals: 0x502x */
	EVT_FILESELECT        = 0x5020,
	EVT_BUT_OPEN          = 0x5021,
	EVT_MODAL_MAP         = 0x5022,
	EVT_DROP              = 0x5023,
	EVT_BUT_CANCEL        = 0x5024,

	/* ********** End of Blender internal events. ********** */
};


/* *********** wmEvent.type helpers. ********** */

/* test whether the event is timer event */
#define ISTIMER(event_type)	(event_type >= TIMER && event_type <= TIMERF)

/* for event checks */
/* only used for KM_TEXTINPUT, so assume that we want all user-inputtable ascii codes included */
/* UNUSED - see wm_eventmatch - BUG [#30479] */
/* #define ISTEXTINPUT(event_type)  (event_type >= ' ' && event_type <= 255) */
/* note, an alternative could be to check 'event->utf8_buf' */

/* test whether the event is a key on the keyboard */
#define ISKEYBOARD(event_type)                          \
	((event_type >= 0x0020 && event_type <= 0x00ff) ||  \
	 (event_type >= 0x012c && event_type <= 0x013f))

/* test whether the event is a modifier key */
#define ISKEYMODIFIER(event_type)  ((event_type >= LEFTCTRLKEY && event_type <= LEFTSHIFTKEY) || event_type == OSKEY)

/* test whether the event is a mouse button */
#define ISMOUSE(event_type)  (event_type >= LEFTMOUSE && event_type <= BUTTON7MOUSE)

/* test whether the event is tweak event */
#define ISTWEAK(event_type)  (event_type >= EVT_TWEAK_L && event_type <= EVT_GESTURE)

/* test whether the event is a NDOF event */
#define ISNDOF(event_type)  (event_type >= NDOF_MOTION && event_type < NDOF_LAST)

/* test whether event type is acceptable as hotkey, excluding modifiers */
#define ISHOTKEY(event_type)                                                  \
	((ISKEYBOARD(event_type) || ISMOUSE(event_type) || ISNDOF(event_type)) && \
	 (event_type != ESCKEY) &&                                                \
	 (event_type >= LEFTCTRLKEY && event_type <= LEFTSHIFTKEY) == false &&    \
	 (event_type >= UNKNOWNKEY  && event_type <= GRLESSKEY) == false)


/* ********** wmEvent.val ********** */

/* Gestures */
enum {
	/* value of tweaks and line gestures, note, KM_ANY (-1) works for this case too */
	EVT_GESTURE_N   = 1,
	EVT_GESTURE_NE  = 2,
	EVT_GESTURE_E   = 3,
	EVT_GESTURE_SE  = 4,
	EVT_GESTURE_S   = 5,
	EVT_GESTURE_SW  = 6,
	EVT_GESTURE_W   = 7,
	EVT_GESTURE_NW  = 8,
	/* value of corner gestures */
	EVT_GESTURE_N_E = 9,
	EVT_GESTURE_N_W = 10,
	EVT_GESTURE_E_N = 11,
	EVT_GESTURE_E_S = 12,
	EVT_GESTURE_S_E = 13,
	EVT_GESTURE_S_W = 14,
	EVT_GESTURE_W_S = 15,
	EVT_GESTURE_W_N = 16,
};

/* File select */
enum {
	EVT_FILESELECT_OPEN             = 1,
	EVT_FILESELECT_FULL_OPEN        = 2,
	EVT_FILESELECT_EXEC             = 3,
	EVT_FILESELECT_CANCEL           = 4,
	EVT_FILESELECT_EXTERNAL_CANCEL  = 5,
};

/* Gesture */
/* NOTE: these values are saved in keymap files, do not change them but just add new ones */
enum {
	GESTURE_MODAL_CANCEL      = 1,
	GESTURE_MODAL_CONFIRM     = 2,

	GESTURE_MODAL_SELECT      = 3,
	GESTURE_MODAL_DESELECT    = 4,

	GESTURE_MODAL_NOP         = 5, /* circle select when no mouse button is pressed */

	GESTURE_MODAL_CIRCLE_ADD  = 6, /* circle sel: larger brush */
	GESTURE_MODAL_CIRCLE_SUB  = 7, /* circle sel: smaller brush */

	GESTURE_MODAL_BEGIN       = 8, /* border select/straight line, activate, use release to detect which button */

	GESTURE_MODAL_IN          = 9,
	GESTURE_MODAL_OUT         = 10,

	GESTURE_MODAL_CIRCLE_SIZE = 11, /* circle sel: size brush (for trackpad event) */
};


#endif	/* __WM_EVENT_TYPES_H__ */

