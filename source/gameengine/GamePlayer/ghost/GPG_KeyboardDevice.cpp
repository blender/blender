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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * GHOST Blender Player keyboard device implementation.
 */

/** \file gameengine/GamePlayer/ghost/GPG_KeyboardDevice.cpp
 *  \ingroup player
 */


#include "GPG_KeyboardDevice.h"

GPG_KeyboardDevice::GPG_KeyboardDevice(void)
{
	m_reverseKeyTranslateTable[GHOST_kKeyA                     ] = KX_AKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyB                     ] = KX_BKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyC                     ] = KX_CKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyD                     ] = KX_DKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyE                     ] = KX_EKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyF                     ] = KX_FKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyG                     ] = KX_GKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyH                     ] = KX_HKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyI                     ] = KX_IKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyJ                     ] = KX_JKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyK                     ] = KX_KKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyL                     ] = KX_LKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyM                     ] = KX_MKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyN                     ] = KX_NKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyO                     ] = KX_OKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyP                     ] = KX_PKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyQ                     ] = KX_QKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyR                     ] = KX_RKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyS                     ] = KX_SKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyT                     ] = KX_TKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyU                     ] = KX_UKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyV                     ] = KX_VKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyW                     ] = KX_WKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyX                     ] = KX_XKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyY                     ] = KX_YKEY                    ;
	m_reverseKeyTranslateTable[GHOST_kKeyZ                     ] = KX_ZKEY                    ;

	m_reverseKeyTranslateTable[GHOST_kKey0                     ] = KX_ZEROKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKey1                     ] = KX_ONEKEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKey2                     ] = KX_TWOKEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKey3                     ] = KX_THREEKEY                ;
	m_reverseKeyTranslateTable[GHOST_kKey4                     ] = KX_FOURKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKey5                     ] = KX_FIVEKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKey6                     ] = KX_SIXKEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKey7                     ] = KX_SEVENKEY                ;
	m_reverseKeyTranslateTable[GHOST_kKey8                     ] = KX_EIGHTKEY                ;
	m_reverseKeyTranslateTable[GHOST_kKey9                     ] = KX_NINEKEY                 ;

	// Middle keyboard area keys
	m_reverseKeyTranslateTable[GHOST_kKeyPause                 ] = KX_PAUSEKEY                ;
	m_reverseKeyTranslateTable[GHOST_kKeyInsert                ] = KX_INSERTKEY               ;
	m_reverseKeyTranslateTable[GHOST_kKeyDelete                ] = KX_DELKEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyHome                  ] = KX_HOMEKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyEnd                   ] = KX_ENDKEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyUpPage                ] = KX_PAGEUPKEY               ;
	m_reverseKeyTranslateTable[GHOST_kKeyDownPage              ] = KX_PAGEDOWNKEY             ;

	// Arrow keys
	m_reverseKeyTranslateTable[GHOST_kKeyUpArrow               ] = KX_UPARROWKEY              ;
	m_reverseKeyTranslateTable[GHOST_kKeyDownArrow             ] = KX_DOWNARROWKEY            ;
	m_reverseKeyTranslateTable[GHOST_kKeyLeftArrow             ] = KX_LEFTARROWKEY            ;
	m_reverseKeyTranslateTable[GHOST_kKeyRightArrow            ] = KX_RIGHTARROWKEY           ;

	// Function keys
	m_reverseKeyTranslateTable[GHOST_kKeyF1                    ] = KX_F1KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF2                    ] = KX_F2KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF3                    ] = KX_F3KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF4                    ] = KX_F4KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF5                    ] = KX_F5KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF6                    ] = KX_F6KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF7                    ] = KX_F7KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF8                    ] = KX_F8KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF9                    ] = KX_F9KEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyF10                   ] = KX_F10KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF11                   ] = KX_F11KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF12                   ] = KX_F12KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF13                   ] = KX_F13KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF14                   ] = KX_F14KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF15                   ] = KX_F15KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF16                   ] = KX_F16KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF17                   ] = KX_F17KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF18                   ] = KX_F18KEY                  ;
	m_reverseKeyTranslateTable[GHOST_kKeyF19                   ] = KX_F19KEY                  ;


	// Numpad keys
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad0               ] = KX_PAD0                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad1               ] = KX_PAD1                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad2               ] = KX_PAD2                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad3               ] = KX_PAD3                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad4               ] = KX_PAD4                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad5               ] = KX_PAD5                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad6               ] = KX_PAD6                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad7               ] = KX_PAD7                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad8               ] = KX_PAD8                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpad9               ] = KX_PAD9                     ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpadAsterisk        ] = KX_PADASTERKEY              ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpadPlus            ] = KX_PADPLUSKEY               ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpadPeriod          ] = KX_PADPERIOD                ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpadMinus           ] = KX_PADMINUS                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpadSlash           ] = KX_PADSLASHKEY              ;
	m_reverseKeyTranslateTable[GHOST_kKeyNumpadEnter           ] = KX_PADENTER                 ;

	// Other keys
	m_reverseKeyTranslateTable[GHOST_kKeyCapsLock              ] = KX_CAPSLOCKKEY              ;
	m_reverseKeyTranslateTable[GHOST_kKeyEsc                   ] = KX_ESCKEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyTab                   ] = KX_TABKEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeySpace                 ] = KX_SPACEKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyEnter                 ] = KX_RETKEY                   ;
	m_reverseKeyTranslateTable[GHOST_kKeyBackSpace             ] = KX_BACKSPACEKEY             ;
	m_reverseKeyTranslateTable[GHOST_kKeySemicolon             ] = KX_SEMICOLONKEY             ;
	m_reverseKeyTranslateTable[GHOST_kKeyPeriod                ] = KX_PERIODKEY                ;
	m_reverseKeyTranslateTable[GHOST_kKeyComma                 ] = KX_COMMAKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyQuote                 ] = KX_QUOTEKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyAccentGrave           ] = KX_ACCENTGRAVEKEY           ;
	m_reverseKeyTranslateTable[GHOST_kKeyMinus                 ] = KX_MINUSKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeySlash                 ] = KX_SLASHKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyBackslash             ] = KX_BACKSLASHKEY             ;
	m_reverseKeyTranslateTable[GHOST_kKeyEqual                 ] = KX_EQUALKEY                 ;
	m_reverseKeyTranslateTable[GHOST_kKeyLeftBracket           ] = KX_LEFTBRACKETKEY           ;
	m_reverseKeyTranslateTable[GHOST_kKeyRightBracket          ] = KX_RIGHTBRACKETKEY          ;

	m_reverseKeyTranslateTable[GHOST_kKeyOS                    ] = KX_OSKEY                    ;

	// Modifier keys.
	m_reverseKeyTranslateTable[GHOST_kKeyLeftControl           ] = KX_LEFTCTRLKEY              ;
	m_reverseKeyTranslateTable[GHOST_kKeyRightControl          ] = KX_RIGHTCTRLKEY             ;
	m_reverseKeyTranslateTable[GHOST_kKeyLeftAlt               ] = KX_LEFTALTKEY               ;
	m_reverseKeyTranslateTable[GHOST_kKeyRightAlt              ] = KX_RIGHTALTKEY              ;
	m_reverseKeyTranslateTable[GHOST_kKeyLeftShift             ] = KX_LEFTSHIFTKEY             ;
	m_reverseKeyTranslateTable[GHOST_kKeyRightShift            ] = KX_RIGHTSHIFTKEY            ;
}


GPG_KeyboardDevice::~GPG_KeyboardDevice(void)
{
}
