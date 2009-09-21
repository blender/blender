/*
 * $Id: wm_event_types.h
 *
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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
 *  These define have its origin at sgi, where all device defines were written down in device.h.
 *  Blender copied the conventions quite some, and expanded it with internal new defines (ton)
 *
 */ 


#ifndef WM_EVENT_TYPES_H
#define WM_EVENT_TYPES_H

/* customdata type */
#define EVT_DATA_TABLET		1
#define EVT_DATA_GESTURE	2
#define EVT_DATA_TIMER		3

/* tablet active, matches GHOST_TTabletMode */
#define EVT_TABLET_NONE		0
#define EVT_TABLET_STYLUS	1
#define EVT_TABLET_ERASER	2

#define MOUSEX		0x004	
#define MOUSEY		0x005	

/* MOUSE : 0x00x */
#define LEFTMOUSE		0x001	
#define MIDDLEMOUSE		0x002	
#define RIGHTMOUSE		0x003	
#define MOUSEMOVE		0x004	
		/* only use if you want user option switch possible */
#define ACTIONMOUSE		0x005
#define SELECTMOUSE		0x006
/* Extra mouse buttons */
#define BUTTON4MOUSE	0x007  
#define BUTTON5MOUSE	0x008
		/* defaults from ghost */
#define WHEELUPMOUSE	0x00a	
#define WHEELDOWNMOUSE	0x00b
		/* mapped with userdef */
#define WHEELINMOUSE	0x00c
#define WHEELOUTMOUSE	0x00d


/* SYSTEM : 0x01x */
#define	INPUTCHANGE		0x0103	/* input connected or disconnected */

#define TIMER			0x0110	/* timer event, passed on to all queues */
#define TIMER0			0x0111	/* timer event, slot for internal use */
#define TIMER1			0x0112	/* timer event, slot for internal use */
#define TIMER2			0x0113	/* timer event, slot for internal use */
#define TIMERJOBS		0x0114  /* timer event, internal use */

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

#define	PAUSEKEY	165
#define	INSERTKEY	166
#define	HOMEKEY 	167
#define	PAGEUPKEY 	168
#define	PAGEDOWNKEY	169
#define	ENDKEY		170

#define UNKNOWNKEY	171
#define COMMANDKEY	172
#define GRLESSKEY	173

/* for event checks */
	/* only used for KM_TEXTINPUT, so assume that we want all user-inputtable ascii codes included */
#define ISKEYBOARD(event)	(event >=' ' && event <=255)

/* test whether event type is acceptable as hotkey, excluding modifiers */
#define ISHOTKEY(event)	(event >=' ' && event <=320 && !(event>=LEFTCTRLKEY && event<=ESCKEY) && !(event>=UNKNOWNKEY && event<=GRLESSKEY))


/* **************** BLENDER GESTURE EVENTS ********************* */

#define EVT_ACTIONZONE_AREA		0x5000
#define EVT_ACTIONZONE_REGION	0x5001

		/* tweak events, for L M R mousebuttons */
#define EVT_TWEAK_L		0x5002
#define EVT_TWEAK_M		0x5003
#define EVT_TWEAK_R		0x5004
		/* tweak events for action or select mousebutton */
#define EVT_TWEAK_A		0x5005
#define EVT_TWEAK_S		0x5006

#define EVT_GESTURE		0x5010

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
#define EVT_FILESELECT_OPEN			1
#define EVT_FILESELECT_FULL_OPEN	2
#define EVT_FILESELECT_EXEC			3
#define EVT_FILESELECT_CANCEL		4	

/* event->type */
#define EVT_BUT_OPEN	0x5021
#define EVT_MODAL_MAP	0x5022



#endif	/* WM_EVENT_TYPES_H */

