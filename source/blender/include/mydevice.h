
#ifndef __MYDEVICE_H__
#define __MYDEVICE_H__

/*
 * 
 *   mouse / timer / window: tot 0x020
 *   eigen codes: 0x4...
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* MOUSE : 0x00x */

#define LEFTMOUSE	0x001	
#define MIDDLEMOUSE	0x002	
#define RIGHTMOUSE	0x003	
#define MOUSEX		0x004	
#define MOUSEY		0x005	

/* timers */

#define TIMER0		0x006	
#define TIMER1		0x007	
#define TIMER2		0x008	
#define TIMER3		0x009	

/* SYSTEM : 0x01x */

#define KEYBD			0x010	/* keyboard */
#define RAWKEYBD		0x011	/* raw keyboard for keyboard manager */
#define REDRAW			0x012	/* used by port manager to signal redraws */
#define	INPUTCHANGE		0x013	/* input connected or disconnected */
#define	QFULL			0x014	/* queue was filled */
#define WINFREEZE		0x015	/* user wants process in this win to shut up */
#define WINTHAW			0x016	/* user wants process in this win to go again */
#define WINCLOSE		0x017	/* window close */
#define WINQUIT			0x018	/* signal from user that app is to go away */
#define Q_FIRSTTIME		0x019	/* on startup */

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

/* **************** BLENDER QUEUE EVENTS ********************* */

#define CHANGED				0x4000
#define DRAWEDGES			0x4001
#define AFTERQUEUE			0x4002
#define BACKBUFDRAW			0x4003
#define EXECUTE				0x4004
#define IGNORE_REDRAW		0x4005
#define LOAD_FILE			0x4006
#define RESHAPE				0x4007
#define UI_BUT_EVENT		0x4008
#define AUTOSAVE_FILE		0x4009

/* REDRAWVIEW3D moet de eerste zijn (laagste nummer) voor buttons! */
#define REDRAWVIEW3D		0x4010

#define REDRAWBUTSHEAD		0x4011
#define REDRAWBUTSALL		0x4012
#define REDRAWBUTSVIEW		0x4013
#define REDRAWBUTSLAMP		0x4014
#define REDRAWBUTSMAT		0x4015
#define REDRAWBUTSTEX		0x4016
#define REDRAWBUTSANIM		0x4017
#define REDRAWBUTSWORLD		0x4018
#define REDRAWBUTSRENDER	0x4019
#define REDRAWBUTSEDIT		0x401A
#define REDRAWVIEWCAM		0x401B
#define REDRAWHEADERS		0x401C
#define REDRAWBUTSGAME		0x401D
#define REDRAWBUTSRADIO		0x401E
#define REDRAWVIEW3D_Z		0x401F

#define REDRAWALL			0x4020
#define REDRAWINFO			0x4021
#define RENDERPREVIEW		0x4022
#define REDRAWIPO			0x4023
#define REDRAWDATASELECT	0x4024
#define REDRAWSEQ			0x4025
#define REDRAWIMAGE			0x4026
#define REDRAWOOPS			0x4027
#define REDRAWIMASEL        0x4028
#define AFTERIMASELIMA      0x4029
#define AFTERIMASELGET      0x402A
#define AFTERIMAWRITE       0x402B
#define IMALEFTMOUSE		0x402C
#define AFTERPIBREAD        0x402D
#define REDRAWTEXT	        0x402F
#define REDRAWBUTSSCRIPT	0x4030
#define REDRAWSOUND			0x4031
#define REDRAWBUTSSOUND		0x4032
#define REDRAWACTION		0x4033
#define REDRAWBUTSCONSTRAINT	0x4034
#define REDRAWNLA			0x4035

#endif	/* !__MYDEVICE_H__ */

