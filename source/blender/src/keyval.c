/**
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

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"


#include "BIF_keyval.h"

#include "blendef.h"

#include "mydevice.h"

char *key_event_to_string(unsigned short event)
{

	switch(event) {
	case AKEY:
		return "A";
		break;
	case BKEY:
		return "B";
		break;
	case CKEY:
		return "C";
		break;
	case DKEY:
		return "D";
		break;
	case EKEY:
		return "E";
		break;
	case FKEY:
		return "F";
		break;
	case GKEY:
		return "G";
		break;
	case HKEY:
		return "H";
		break;
	case IKEY:
		return "I";
		break;
	case JKEY:
		return "J";
		break;
	case KKEY:
		return "K";
		break;
	case LKEY:
		return "L";
		break;
	case MKEY:
		return "M";
		break;
	case NKEY:
		return "N";
		break;
	case OKEY:
		return "O";
		break;
	case PKEY:
		return "P";
		break;
	case QKEY:
		return "Q";
		break;
	case RKEY:
		return "R";
		break;
	case SKEY:
		return "S";
		break;
	case TKEY:
		return "T";
		break;
	case UKEY:
		return "U";
		break;
	case VKEY:
		return "V";
		break;
	case WKEY:
		return "W";
		break;
	case XKEY:
		return "X";
		break;
	case YKEY:
		return "Y";
		break;
	case ZKEY:
		return "Z";
		break;

	case ZEROKEY:
		return "Zero";
		break;
	case ONEKEY:
		return "One";
		break;
	case TWOKEY:
		return "Two";
		break;
	case THREEKEY:
		return "Three";
		break;
	case FOURKEY:
		return "Four";
		break;
	case FIVEKEY:
		return "Five";
		break;
	case SIXKEY:
		return "Six";
		break;
	case SEVENKEY:
		return "Seven";
		break;
	case EIGHTKEY:
		return "Eight";
		break;
	case NINEKEY:
		return "Nine";
		break;

	case LEFTCTRLKEY:
		return "Leftctrl";
		break;
	case LEFTALTKEY:
		return "Leftalt";
		break;
	case	RIGHTALTKEY:
		return "Rightalt";
		break;
	case	RIGHTCTRLKEY:
		return "Rightctrl";
		break;
	case RIGHTSHIFTKEY:
		return "Rightshift";
		break;
	case LEFTSHIFTKEY:
		return "Leftshift";
		break;

	case ESCKEY:
		return "Esc";
		break;
	case TABKEY:
		return "Tab";
		break;
	case RETKEY:
		return "Ret";
		break;
	case SPACEKEY:
		return "Space";
		break;
	case LINEFEEDKEY:
		return "Linefeed";
		break;
	case BACKSPACEKEY:
		return "Backspace";
		break;
	case DELKEY:
		return "Del";
		break;
	case SEMICOLONKEY:
		return "Semicolon";
		break;
	case PERIODKEY:
		return "Period";
		break;
	case COMMAKEY:
		return "Comma";
		break;
	case QUOTEKEY:
		return "Quote";
		break;
	case ACCENTGRAVEKEY:
		return "Accentgrave";
		break;
	case MINUSKEY:
		return "Minus";
		break;
	case SLASHKEY:
		return "Slash";
		break;
	case BACKSLASHKEY:
		return "Backslash";
		break;
	case EQUALKEY:
		return "Equal";
		break;
	case LEFTBRACKETKEY:
		return "Leftbracket";
		break;
	case RIGHTBRACKETKEY:
		return "Rightbracket";
		break;

	case LEFTARROWKEY:
		return "Leftarrow";
		break;
	case DOWNARROWKEY:
		return "Downarrow";
		break;
	case RIGHTARROWKEY:
		return "Rightarrow";
		break;
	case UPARROWKEY:
		return "Uparrow";
		break;

	case PAD2:
		return "Pad2";
		break;
	case PAD4:
		return "Pad4";
		break;
	case PAD6:
		return "Pad6";
		break;
	case PAD8:
		return "Pad8";
		break;
	case PAD1:
		return "Pad1";
		break;
	case PAD3:
		return "Pad3";
		break;
	case PAD5:
		return "Pad5";
		break;
	case PAD7:
		return "Pad7";
		break;
	case PAD9:
		return "Pad9";
		break;

	case PADPERIOD:
		return "Padperiod";
		break;
	case PADSLASHKEY:
		return "Padslash";
		break;
	case PADASTERKEY:
		return "Padaster";
		break;

	case PAD0:
		return "Pad0";
		break;
	case PADMINUS:
		return "Padminus";
		break;
	case PADENTER:
		return "Padenter";
		break;
	case PADPLUSKEY:
		return "Padplus";
		break;

	case	F1KEY:
		return "F1";
		break;
	case	F2KEY:
		return "F2";
		break;
	case	F3KEY:
		return "F3";
		break;
	case	F4KEY:
		return "F4";
		break;
	case	F5KEY:
		return "F5";
		break;
	case	F6KEY:
		return "F6";
		break;
	case	F7KEY:
		return "F7";
		break;
	case	F8KEY:
		return "F8";
		break;
	case	F9KEY:
		return "F9";
		break;
	case	F10KEY:
		return "F10";
		break;
	case	F11KEY:
		return "F11";
		break;
	case	F12KEY:
		return "F12";
		break;

	case	PAUSEKEY:
		return "Pause";
		break;
	case	INSERTKEY:
		return "Insert";
		break;
	case	HOMEKEY:
		return "Home";
		break;
	case	PAGEUPKEY:
		return "Pageup";
		break;
	case	PAGEDOWNKEY:
		return "Pagedown";
		break;
	case	ENDKEY:
		return "End";
		break;
	}
	
	return "";
}
