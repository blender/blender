/**
 * $Id$
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GPW_KeyboardDevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// Key code values not found in winuser.h
#ifndef VK_MINUS
#define VK_MINUS 0xBD
#endif // VK_MINUS
#ifndef VK_SEMICOLON
#define VK_SEMICOLON 0xBA
#endif // VK_SEMICOLON
#ifndef VK_PERIOD
#define VK_PERIOD 0xBE
#endif // VK_PERIOD
#ifndef VK_COMMA
#define VK_COMMA 0xBC
#endif // VK_COMMA
#ifndef VK_QUOTE
#define VK_QUOTE 0xDE
#endif // VK_QUOTE
#ifndef VK_BACK_QUOTE
#define VK_BACK_QUOTE 0xC0
#endif // VK_BACK_QUOTE
#ifndef VK_SLASH
#define VK_SLASH 0xBF
#endif // VK_SLASH
#ifndef VK_BACK_SLASH
#define VK_BACK_SLASH 0xDC
#endif // VK_BACK_SLASH
#ifndef VK_EQUALS
#define VK_EQUALS 0xBB
#endif // VK_EQUALS
#ifndef VK_OPEN_BRACKET
#define VK_OPEN_BRACKET 0xDB
#endif // VK_OPEN_BRACKET
#ifndef VK_CLOSE_BRACKET
#define VK_CLOSE_BRACKET 0xDD
#endif // VK_CLOSE_BRACKET



GPW_KeyboardDevice::GPW_KeyboardDevice(void)
{
	m_seperateLeftRight = false;
	m_seperateLeftRightInitialized = false;

	m_reverseKeyTranslateTable['A'                             ] = KX_AKEY                    ;                  
	m_reverseKeyTranslateTable['B'                             ] = KX_BKEY                    ;                  
	m_reverseKeyTranslateTable['C'                             ] = KX_CKEY                    ;                  
	m_reverseKeyTranslateTable['D'                             ] = KX_DKEY                    ;                  
	m_reverseKeyTranslateTable['E'                             ] = KX_EKEY                    ;                  
	m_reverseKeyTranslateTable['F'                             ] = KX_FKEY                    ;                  
	m_reverseKeyTranslateTable['G'                             ] = KX_GKEY                    ;                  
	m_reverseKeyTranslateTable['H'                             ] = KX_HKEY                    ;                  
	m_reverseKeyTranslateTable['I'                             ] = KX_IKEY                    ;                  
	m_reverseKeyTranslateTable['J'                             ] = KX_JKEY                    ;                  
	m_reverseKeyTranslateTable['K'                             ] = KX_KKEY                    ;                  
	m_reverseKeyTranslateTable['L'                             ] = KX_LKEY                    ;                  
	m_reverseKeyTranslateTable['M'                             ] = KX_MKEY                    ;                  
	m_reverseKeyTranslateTable['N'                             ] = KX_NKEY                    ;                  
	m_reverseKeyTranslateTable['O'                             ] = KX_OKEY                    ;                  
	m_reverseKeyTranslateTable['P'                             ] = KX_PKEY                    ;                  
	m_reverseKeyTranslateTable['Q'                             ] = KX_QKEY                    ;                  
	m_reverseKeyTranslateTable['R'                             ] = KX_RKEY                    ;                  
	m_reverseKeyTranslateTable['S'                             ] = KX_SKEY                    ;                  
	m_reverseKeyTranslateTable['T'                             ] = KX_TKEY                    ;                  
	m_reverseKeyTranslateTable['U'                             ] = KX_UKEY                    ;                  
	m_reverseKeyTranslateTable['V'                             ] = KX_VKEY                    ;                  
	m_reverseKeyTranslateTable['W'                             ] = KX_WKEY                    ;                  
	m_reverseKeyTranslateTable['X'                             ] = KX_XKEY                    ;                  
	m_reverseKeyTranslateTable['Y'                             ] = KX_YKEY                    ;                  
	m_reverseKeyTranslateTable['Z'                             ] = KX_ZKEY                    ;                  

	m_reverseKeyTranslateTable['0'                             ] = KX_ZEROKEY                 ;                  
	m_reverseKeyTranslateTable['1'                             ] = KX_ONEKEY                  ;                  
	m_reverseKeyTranslateTable['2'                             ] = KX_TWOKEY                  ;                  
	m_reverseKeyTranslateTable['3'                             ] = KX_THREEKEY                ;                  
	m_reverseKeyTranslateTable['4'                             ] = KX_FOURKEY                 ;                  
	m_reverseKeyTranslateTable['5'                             ] = KX_FIVEKEY                 ;                  
	m_reverseKeyTranslateTable['6'                             ] = KX_SIXKEY                  ;                  
	m_reverseKeyTranslateTable['7'                             ] = KX_SEVENKEY                ;                  
	m_reverseKeyTranslateTable['8'                             ] = KX_EIGHTKEY                ;                  
	m_reverseKeyTranslateTable['9'                             ] = KX_NINEKEY                 ;
	
	// Middle keyboard area keys
	m_reverseKeyTranslateTable[VK_PAUSE                        ] = KX_PAUSEKEY                ;                  
	m_reverseKeyTranslateTable[VK_INSERT                       ] = KX_INSERTKEY               ;                  
	m_reverseKeyTranslateTable[VK_DELETE                       ] = KX_DELKEY                  ;                  
	m_reverseKeyTranslateTable[VK_HOME                         ] = KX_HOMEKEY                 ;                  
	m_reverseKeyTranslateTable[VK_END                          ] = KX_ENDKEY                  ;                  
	m_reverseKeyTranslateTable[VK_PRIOR                        ] = KX_PAGEUPKEY               ;                  
	m_reverseKeyTranslateTable[VK_NEXT                         ] = KX_PAGEDOWNKEY             ;                  
	
	// Arrow keys
	m_reverseKeyTranslateTable[VK_UP                           ] = KX_UPARROWKEY              ;                  
	m_reverseKeyTranslateTable[VK_DOWN                         ] = KX_DOWNARROWKEY            ;                  
	m_reverseKeyTranslateTable[VK_LEFT                         ] = KX_LEFTARROWKEY            ;                  
	m_reverseKeyTranslateTable[VK_RIGHT                        ] = KX_RIGHTARROWKEY           ;                  

	// Function keys
	m_reverseKeyTranslateTable[VK_F1                           ] = KX_F1KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F2                           ] = KX_F2KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F3                           ] = KX_F3KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F4                           ] = KX_F4KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F5                           ] = KX_F5KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F6                           ] = KX_F6KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F7                           ] = KX_F7KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F8                           ] = KX_F8KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F9                           ] = KX_F9KEY                   ;                  
	m_reverseKeyTranslateTable[VK_F10                          ] = KX_F10KEY                  ;                  
	m_reverseKeyTranslateTable[VK_F11                          ] = KX_F11KEY                  ;                  
	m_reverseKeyTranslateTable[VK_F12                          ] = KX_F12KEY                  ;                  

	// Numpad keys
	m_reverseKeyTranslateTable[VK_NUMPAD0	                   ] = KX_PAD0	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD1	                   ] = KX_PAD1	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD2	                   ] = KX_PAD2	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD3	                   ] = KX_PAD3	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD4	                   ] = KX_PAD4	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD5	                   ] = KX_PAD5	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD6	                   ] = KX_PAD6	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD7	                   ] = KX_PAD7	                   ;                  
	m_reverseKeyTranslateTable[VK_NUMPAD8	                   ] = KX_PAD8	                   ;                                                                                                       
	m_reverseKeyTranslateTable[VK_NUMPAD9	                   ] = KX_PAD9	                   ;                  
	m_reverseKeyTranslateTable[VK_MULTIPLY                     ] = KX_PADASTERKEY              ;                                                                                                                     	                                                                                                       
	m_reverseKeyTranslateTable[VK_ADD                          ] = KX_PADPLUSKEY               ;                  
	m_reverseKeyTranslateTable[VK_DECIMAL                      ] = KX_PADPERIOD                ;                  
	m_reverseKeyTranslateTable[VK_SUBTRACT                     ] = KX_PADMINUS                 ;                  
	m_reverseKeyTranslateTable[VK_DIVIDE                       ] = KX_PADSLASHKEY            ;                 
	m_reverseKeyTranslateTable[VK_SEPARATOR                    ] = KX_PADENTER                 ;                  

	// Other keys
	m_reverseKeyTranslateTable[VK_CAPITAL                      ] = KX_CAPSLOCKKEY              ;                  
	m_reverseKeyTranslateTable[VK_ESCAPE                       ] = KX_ESCKEY                   ;                  
	m_reverseKeyTranslateTable[VK_TAB                          ] = KX_TABKEY                   ;                  
	//m_reverseKeyTranslateTable[VK_RETURN                       ] = KX_RETKEY                   ;                  
	m_reverseKeyTranslateTable[VK_SPACE                        ] = KX_SPACEKEY                 ;                  
	m_reverseKeyTranslateTable[VK_RETURN		               ] = KX_LINEFEEDKEY		       ;                  
	m_reverseKeyTranslateTable[VK_BACK                         ] = KX_BACKSPACEKEY             ;                  
	m_reverseKeyTranslateTable[VK_SEMICOLON                    ] = KX_SEMICOLONKEY             ;                  
	m_reverseKeyTranslateTable[VK_PERIOD                       ] = KX_PERIODKEY                ;                  
	m_reverseKeyTranslateTable[VK_COMMA                        ] = KX_COMMAKEY                 ;                  
	m_reverseKeyTranslateTable[VK_QUOTE                        ] = KX_QUOTEKEY                 ;                  
	m_reverseKeyTranslateTable[VK_BACK_QUOTE                   ] = KX_ACCENTGRAVEKEY           ;                  
	m_reverseKeyTranslateTable[VK_MINUS	                       ] = KX_MINUSKEY                 ;                  
	m_reverseKeyTranslateTable[VK_SLASH		                   ] = KX_SLASHKEY                 ;                  
	m_reverseKeyTranslateTable[VK_BACK_SLASH                   ] = KX_BACKSLASHKEY             ;                  
	m_reverseKeyTranslateTable[VK_EQUALS		               ] = KX_EQUALKEY                 ;                  
	m_reverseKeyTranslateTable[VK_OPEN_BRACKET	               ] = KX_LEFTBRACKETKEY           ;                  
	m_reverseKeyTranslateTable[VK_CLOSE_BRACKET	               ] = KX_RIGHTBRACKETKEY          ;                  

	/* 
	 * Need to handle Ctrl, Alt and Shift keys differently.
	 * Win32 messages do not discriminate left and right keys.
	 */
	m_reverseKeyTranslateTable[VK_LCONTROL	                   ] = KX_LEFTCTRLKEY	           ;                  
	m_reverseKeyTranslateTable[VK_RCONTROL 	                   ] = KX_RIGHTCTRLKEY 	           ;                  
	m_reverseKeyTranslateTable[VK_LMENU                        ] = KX_LEFTALTKEY               ;                  
	m_reverseKeyTranslateTable[VK_RMENU                        ] = KX_RIGHTALTKEY              ;                  
	m_reverseKeyTranslateTable[VK_RSHIFT	                   ] = KX_RIGHTSHIFTKEY            ;                  
	m_reverseKeyTranslateTable[VK_LSHIFT                       ] = KX_LEFTSHIFTKEY             ;                  
}


GPW_KeyboardDevice::~GPW_KeyboardDevice(void)
{
}


void GPW_KeyboardDevice::ConvertWinEvent(WPARAM wParam, bool isDown)
{
	if ((wParam == VK_SHIFT) || (wParam == VK_MENU) || (wParam == VK_CONTROL)) {
		ConvertModifierKey(wParam, isDown);
	}
	else {
		ConvertEvent(wParam, isDown);
	}
}


void GPW_KeyboardDevice::ConvertModifierKey(WPARAM wParam, bool isDown)
{
	/*
	GetKeyState and GetAsyncKeyState only work with Win95, Win98, NT4,
	Terminal Server and Windows 2000.
	But on WinME it always returns zero. These two functions are simply
	skipped by Millenium Edition!

	Official explanation from Microsoft:
	Intentionally disabled.
	It didn't work all that well on some newer hardware, and worked less 
	well with the passage of time, so it was fully disabled in ME.
	*/
	if (!m_seperateLeftRightInitialized && isDown) {
		CheckForSeperateLeftRight(wParam);
	}
	if (m_seperateLeftRight) {
		bool down = HIBYTE(::GetKeyState(VK_LSHIFT)) != 0;
		ConvertEvent(VK_LSHIFT, down);
		down = HIBYTE(::GetKeyState(VK_RSHIFT)) != 0;
		ConvertEvent(VK_RSHIFT, down);
		down = HIBYTE(::GetKeyState(VK_LMENU)) != 0;
		ConvertEvent(VK_LMENU, down);
		down = HIBYTE(::GetKeyState(VK_RMENU)) != 0;
		ConvertEvent(VK_RMENU, down);
		down = HIBYTE(::GetKeyState(VK_LCONTROL)) != 0;
		ConvertEvent(VK_LCONTROL, down);
		down = HIBYTE(::GetKeyState(VK_RCONTROL)) != 0;
		ConvertEvent(VK_RCONTROL, down);
	}
	else {
		bool down = HIBYTE(::GetKeyState(VK_SHIFT)) != 0;
		ConvertEvent(VK_LSHIFT, down);
		ConvertEvent(VK_RSHIFT, down);
		down = HIBYTE(::GetKeyState(VK_MENU)) != 0;
		ConvertEvent(VK_LMENU, down);
		ConvertEvent(VK_RMENU, down);
		down = HIBYTE(::GetKeyState(VK_CONTROL)) != 0;
		ConvertEvent(VK_LCONTROL, down);
		ConvertEvent(VK_RCONTROL, down);
	}
}


void GPW_KeyboardDevice::CheckForSeperateLeftRight(WPARAM wParam)
{
	// Check whether this system supports seperate left and right keys
	switch (wParam) {
		case VK_SHIFT:
			m_seperateLeftRight = 
				(HIBYTE(::GetKeyState(VK_LSHIFT)) != 0) ||
				(HIBYTE(::GetKeyState(VK_RSHIFT)) != 0) ?
				true : false;
			break;
		case VK_CONTROL:
			m_seperateLeftRight = 
				(HIBYTE(::GetKeyState(VK_LCONTROL)) != 0) ||
				(HIBYTE(::GetKeyState(VK_RCONTROL)) != 0) ?
				true : false;
			break;
		case VK_MENU:
			m_seperateLeftRight = 
				(HIBYTE(::GetKeyState(VK_LMENU)) != 0) ||
				(HIBYTE(::GetKeyState(VK_RMENU)) != 0) ?
				true : false;
			break;
	}
	m_seperateLeftRightInitialized = true;
}
