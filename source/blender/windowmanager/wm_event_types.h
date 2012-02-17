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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
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
#define EVT_DATA_TABLET		1
#define EVT_DATA_GESTURE	2
#define EVT_DATA_TIMER		3
#define EVT_DATA_LISTBASE	4
#define EVT_DATA_NDOF_MOTION 5

/* tablet active, matches GHOST_TTabletMode */
#define EVT_TABLET_NONE		0
#define EVT_TABLET_STYLUS	1
#define EVT_TABLET_ERASER	2

#define MOUSEX		4
#define MOUSEY		5

/* MOUSE : 0x00x */
#define LEFTMOUSE		1
#define MIDDLEMOUSE		2
#define RIGHTMOUSE		3
#define MOUSEMOVE		4
		/* only use if you want user option switch possible */
#define ACTIONMOUSE		5
#define SELECTMOUSE		6
		/* Extra mouse buttons */
#define BUTTON4MOUSE	7
#define BUTTON5MOUSE	8
		/* Extra trackpad gestures */
#define MOUSEPAN		14
#define MOUSEZOOM		15
#define MOUSEROTATE		16
		/* defaults from ghost */
#define WHEELUPMOUSE	10
#define WHEELDOWNMOUSE	11
		/* mapped with userdef */
#define WHEELINMOUSE	12
#define WHEELOUTMOUSE	13
#define INBETWEEN_MOUSEMOVE	17


/* NDOF (from SpaceNavigator & friends)
   These should be kept in sync with GHOST_NDOFManager.h
   Ordering matters, exact values do not. */

#define NDOF_MOTION 400

enum {
	// used internally, never sent
	NDOF_BUTTON_NONE = NDOF_MOTION,
	// these two are available from any 3Dconnexion device
	NDOF_BUTTON_MENU,
	NDOF_BUTTON_FIT,
	// standard views
	NDOF_BUTTON_TOP,
	NDOF_BUTTON_BOTTOM,
	NDOF_BUTTON_LEFT,
	NDOF_BUTTON_RIGHT,
	NDOF_BUTTON_FRONT,
	NDOF_BUTTON_BACK,
	// more views
	NDOF_BUTTON_ISO1,
	NDOF_BUTTON_ISO2,
	// 90 degree rotations
	NDOF_BUTTON_ROLL_CW,
	NDOF_BUTTON_ROLL_CCW,
	NDOF_BUTTON_SPIN_CW,
	NDOF_BUTTON_SPIN_CCW,
	NDOF_BUTTON_TILT_CW,
	NDOF_BUTTON_TILT_CCW,
	// device control
	NDOF_BUTTON_ROTATE,
	NDOF_BUTTON_PANZOOM,
	NDOF_BUTTON_DOMINANT,
	NDOF_BUTTON_PLUS,
	NDOF_BUTTON_MINUS,
	// general-purpose buttons
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
	NDOF_LAST
	};


/* SYSTEM : 0x01xx */
#define	INPUTCHANGE		0x0103	/* input connected or disconnected */
#define WINDEACTIVATE	0x0104	/* window is deactivated, focus lost */

#define TIMER			0x0110	/* timer event, passed on to all queues */
#define TIMER0			0x0111	/* timer event, slot for internal use */
#define TIMER1			0x0112	/* timer event, slot for internal use */
#define TIMER2			0x0113	/* timer event, slot for internal use */
#define TIMERJOBS		0x0114  /* timer event, jobs system */
#define TIMERAUTOSAVE	0x0115  /* timer event, autosave */
#define TIMERREPORT		0x0116	/* timer event, reports */
#define TIMERF			0x011F	/* last timer */

/* test whether the event is timer event */
#define ISTIMER(event)	(event >= TIMER && event <= TIMERF)


/* standard keyboard */
#define AKEY		'a'
#define BKEY		'b'
#define CKEY		'c'
#define DKEY		'd'
#define EKEY		'e'
#define FKEY		'f'
#define GKEY		'g'
#define HKEY		'h'
#define IKEY		'i'
#define JKEY		'j'
#define KKEY		'k'
#define LKEY		'l'
#define MKEY		'm'
#define NKEY		'n'
#define OKEY		'o'
#define PKEY		'p'
#define QKEY		'q'
#define RKEY		'r'
#define SKEY		's'
#define TKEY		't'
#define UKEY		'u'
#define VKEY		'v'
#define WKEY		'w'
#define XKEY		'x'
#define YKEY		'y'
#define ZKEY		'z'

#define ZEROKEY		'0'
#define ONEKEY		'1'
#define TWOKEY		'2'
#define THREEKEY	'3'
#define FOURKEY		'4'
#define FIVEKEY		'5'
#define SIXKEY		'6'
#define SEVENKEY	'7'
#define EIGHTKEY	'8'
#define NINEKEY		'9'

#define CAPSLOCKKEY		211

#define LEFTCTRLKEY		212
#define LEFTALTKEY 		213
#define	RIGHTALTKEY 	214
#define	RIGHTCTRLKEY 	215
#define RIGHTSHIFTKEY	216
#define LEFTSHIFTKEY	217

#define ESCKEY			218
#define TABKEY			219
#define RETKEY			220
#define SPACEKEY		221
#define LINEFEEDKEY		222
#define BACKSPACEKEY	223
#define DELKEY			224
#define SEMICOLONKEY	225
#define PERIODKEY		226
#define COMMAKEY		227
#define QUOTEKEY		228
#define ACCENTGRAVEKEY	229
#define MINUSKEY		230
#define SLASHKEY		232
#define BACKSLASHKEY	233
#define EQUALKEY		234
#define LEFTBRACKETKEY	235
#define RIGHTBRACKETKEY	236

#define LEFTARROWKEY	137
#define DOWNARROWKEY	138
#define RIGHTARROWKEY	139
#define UPARROWKEY		140

#define PAD0			150
#define PAD1			151
#define PAD2			152
#define PAD3			153
#define PAD4			154
#define PAD5			155
#define PAD6			156
#define PAD7			157
#define PAD8			158
#define PAD9			159


#define PADPERIOD		199
#define	PADSLASHKEY 	161
#define PADASTERKEY 	160

#define PADMINUS		162
#define PADENTER		163
#define PADPLUSKEY 		164

#define	F1KEY 		300
#define	F2KEY 		301
#define	F3KEY 		302
#define	F4KEY 		303
#define	F5KEY 		304
#define	F6KEY 		305
#define	F7KEY 		306
#define	F8KEY 		307
#define	F9KEY 		308
#define	F10KEY		309
#define	F11KEY		310
#define	F12KEY		311
#define	F13KEY		312
#define	F14KEY		313
#define	F15KEY		314
#define	F16KEY		315
#define	F17KEY		316
#define	F18KEY		317
#define	F19KEY		318

#define	PAUSEKEY	165
#define	INSERTKEY	166
#define	HOMEKEY 	167
#define	PAGEUPKEY 	168
#define	PAGEDOWNKEY	169
#define	ENDKEY		170

#define UNKNOWNKEY	171
#define OSKEY		172
#define GRLESSKEY	173

// XXX: are these codes ok?
#define MEDIAPLAY	174
#define MEDIASTOP	175
#define MEDIAFIRST	176
#define MEDIALAST	177

/* for event checks */
	/* only used for KM_TEXTINPUT, so assume that we want all user-inputtable ascii codes included */
#define ISTEXTINPUT(event)	(event >=' ' && event <=255)

	/* test whether the event is a key on the keyboard */
#define ISKEYBOARD(event)	(event >=' ' && event <=320)

	/* test whether the event is a modifier key */
#define ISKEYMODIFIER(event)	((event >= LEFTCTRLKEY && event <= LEFTSHIFTKEY) || event == OSKEY)

	/* test whether the event is a mouse button */
#define ISMOUSE(event)	(event >= LEFTMOUSE && event <= MOUSEROTATE)

	/* test whether the event is tweak event */
#define ISTWEAK(event)	(event >= EVT_TWEAK_L && event <= EVT_GESTURE)

	/* test whether the event is a NDOF event */
#define ISNDOF(event)	(event >= NDOF_MOTION && event < NDOF_LAST)

/* test whether event type is acceptable as hotkey, excluding modifiers */
#define ISHOTKEY(event)	((ISKEYBOARD(event) || ISMOUSE(event) || ISNDOF(event)) && event!=ESCKEY && !(event>=LEFTCTRLKEY && event<=LEFTSHIFTKEY) && !(event>=UNKNOWNKEY && event<=GRLESSKEY))

/* **************** BLENDER GESTURE EVENTS (0x5000) **************** */

#define EVT_ACTIONZONE_AREA		20480
#define EVT_ACTIONZONE_REGION	20481

		/* tweak events, for L M R mousebuttons */
#define EVT_TWEAK_L		20482
#define EVT_TWEAK_M		20483
#define EVT_TWEAK_R		20484
		/* tweak events for action or select mousebutton */
#define EVT_TWEAK_A		20485
#define EVT_TWEAK_S		20486

#define EVT_GESTURE		20496

/* value of tweaks and line gestures, note, KM_ANY (-1) works for this case too */
#define EVT_GESTURE_N		1
#define EVT_GESTURE_NE		2
#define EVT_GESTURE_E		3
#define EVT_GESTURE_SE		4
#define EVT_GESTURE_S		5
#define EVT_GESTURE_SW		6
#define EVT_GESTURE_W		7
#define EVT_GESTURE_NW		8
/* value of corner gestures */
#define EVT_GESTURE_N_E		9
#define EVT_GESTURE_N_W		10
#define EVT_GESTURE_E_N		11
#define EVT_GESTURE_E_S		12
#define EVT_GESTURE_S_E		13
#define EVT_GESTURE_S_W		14
#define EVT_GESTURE_W_S		15
#define EVT_GESTURE_W_N		16

/* **************** OTHER BLENDER EVENTS ********************* */

/* event->type */
#define EVT_FILESELECT	0x5020

/* event->val */
#define EVT_FILESELECT_OPEN					1
#define EVT_FILESELECT_FULL_OPEN			2
#define EVT_FILESELECT_EXEC					3
#define EVT_FILESELECT_CANCEL				4
#define EVT_FILESELECT_EXTERNAL_CANCEL		5

/* event->type */
#define EVT_BUT_OPEN	0x5021
#define EVT_MODAL_MAP	0x5022
#define EVT_DROP		0x5023
#define EVT_BUT_CANCEL	0x5024

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
#define GESTURE_MODAL_CANCEL		1
#define GESTURE_MODAL_CONFIRM		2

#define GESTURE_MODAL_SELECT		3
#define GESTURE_MODAL_DESELECT		4

#define GESTURE_MODAL_NOP			5 /* circle select when no mouse button is pressed */

#define GESTURE_MODAL_CIRCLE_ADD	6 /* circle sel: larger brush */
#define GESTURE_MODAL_CIRCLE_SUB	7 /* circle sel: smaller brush */

#define GESTURE_MODAL_BEGIN			8 /* border select/straight line, activate, use release to detect which button */

#define GESTURE_MODAL_IN			9
#define GESTURE_MODAL_OUT			10


#endif	/* __WM_EVENT_TYPES_H__ */

