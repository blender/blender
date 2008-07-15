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

#include "stdio.h"
#include "ctype.h"
#include "string.h"

#include "BKE_global.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BIF_keyval.h"
#include "blendef.h"
#include "mydevice.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


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

/* 
 * Decodes key combination strings [qual1+[qual2+[...]]]keyname
 * The '+'s may be replaced by '-' or ' ' characters to support different
 * formats. No additional whitespace is allowed. The keyname may be an internal
 * name, like "RETKEY", or a more common name, like "Return". Decoding is case-
 * insensitive.
 *
 * Example strings: "Ctrl+L", "ALT-ESC", "Shift A"
 *
 * Returns 1 if successful. 
 */
int decode_key_string(char *str, unsigned short *key, unsigned short *qual)
{
	int i, prev, len, invalid=0;

	len= strlen(str);
	*key= *qual= 0;

	/* Convert to upper case */
	for (i=0; i<len; i++) {
		str[i]= toupper(str[i]);
	}

	/* Handle modifiers */
	for (prev=i=0; i<len; i++) {
		if (str[i]==' ' || str[i]=='+' || str[i]=='-') {
			if (!strncmp(str+prev, "CTRL", i-prev)) *qual |= LR_CTRLKEY;
			else if (!strncmp(str+prev, "ALT", i-prev)) *qual |= LR_ALTKEY;
			else if (!strncmp(str+prev, "SHIFT", i-prev)) *qual |= LR_SHIFTKEY;
			else if (!strncmp(str+prev, "COMMAND", i-prev)) *qual |= LR_COMMANDKEY;
			prev=i+1;
		}
	}

	/* Compare last part against key names */
	if (len-prev==1 || len-prev==4 && !strncmp(str+prev, "KEY", 3)) {
		
		if (str[prev]>='A' && str[prev]<='Z') {
			*key= str[prev]-'A'+AKEY;
		} else if (str[prev]>='0' && str[prev]<='9') {
			*key= str[prev]-'0'+ZEROKEY;
		} else {
			invalid= 1;
		}
	
	} else if (!strncmp(str+prev, "ZEROKEY", len-prev) || !strncmp(str+prev, "ZERO", len-prev)) {
		*key= ZEROKEY;
	} else if (!strncmp(str+prev, "ONEKEY", len-prev) || !strncmp(str+prev, "ONE", len-prev)) {
		*key= ONEKEY;
	} else if (!strncmp(str+prev, "TWOKEY", len-prev) || !strncmp(str+prev, "TWO", len-prev)) {
		*key= TWOKEY;
	} else if (!strncmp(str+prev, "THREEKEY", len-prev) || !strncmp(str+prev, "THREE", len-prev)) {
		*key= THREEKEY;
	} else if (!strncmp(str+prev, "FOURKEY", len-prev) || !strncmp(str+prev, "FOUR", len-prev)) {
		*key= FOURKEY;
	} else if (!strncmp(str+prev, "FIVEKEY", len-prev) || !strncmp(str+prev, "FIVE", len-prev)) {
		*key= FIVEKEY;
	} else if (!strncmp(str+prev, "SIZEKEY", len-prev) || !strncmp(str+prev, "SIX", len-prev)) {
		*key= SIXKEY;
	} else if (!strncmp(str+prev, "SEVENKEY", len-prev) || !strncmp(str+prev, "SEVEN", len-prev)) {
		*key= SEVENKEY;
	} else if (!strncmp(str+prev, "EIGHTKEY", len-prev) || !strncmp(str+prev, "EIGHT", len-prev)) {
		*key= EIGHTKEY;
	} else if (!strncmp(str+prev, "NINEKEY", len-prev) || !strncmp(str+prev, "NINE", len-prev)) {
		*key= NINEKEY;

	} else if (!strncmp(str+prev, "ESCKEY", len-prev) || !strncmp(str+prev, "ESC", len-prev)) {
		*key= ESCKEY;
	} else if (!strncmp(str+prev, "TABKEY", len-prev) || !strncmp(str+prev, "TAB", len-prev)) {
		*key= TABKEY;
	} else if (!strncmp(str+prev, "RETKEY", len-prev) || !strncmp(str+prev, "RETURN", len-prev) || !strncmp(str+prev, "ENTER", len-prev)) {
		*key= RETKEY;
	} else if (!strncmp(str+prev, "SPACEKEY", len-prev) || !strncmp(str+prev, "SPACE", len-prev)) {
		*key= SPACEKEY;
	} else if (!strncmp(str+prev, "LINEFEEDKEY", len-prev) || !strncmp(str+prev, "LINEFEED", len-prev)) {
		*key= LINEFEEDKEY;
	} else if (!strncmp(str+prev, "BACKSPACEKEY", len-prev) || !strncmp(str+prev, "BACKSPACE", len-prev)) {
		*key= BACKSPACEKEY;
	} else if (!strncmp(str+prev, "DELKEY", len-prev) || !strncmp(str+prev, "DELETE", len-prev)) {
		*key= DELKEY;
	
	} else if (!strncmp(str+prev, "SEMICOLONKEY", len-prev) || !strncmp(str+prev, "SEMICOLON", len-prev)) {
		*key= SEMICOLONKEY;
	} else if (!strncmp(str+prev, "PERIODKEY", len-prev) || !strncmp(str+prev, "PERIOD", len-prev)) {
		*key= PERIODKEY;
	} else if (!strncmp(str+prev, "COMMAKEY", len-prev) || !strncmp(str+prev, "COMMA", len-prev)) {
		*key= COMMAKEY;
	} else if (!strncmp(str+prev, "QUOTEKEY", len-prev) || !strncmp(str+prev, "QUOTE", len-prev)) {
		*key= QUOTEKEY;
	} else if (!strncmp(str+prev, "ACCENTGRAVEKEY", len-prev) || !strncmp(str+prev, "ACCENTGRAVE", len-prev)) {
		*key= ACCENTGRAVEKEY;
	} else if (!strncmp(str+prev, "MINUSKEY", len-prev) || !strncmp(str+prev, "MINUS", len-prev)) {
		*key= MINUSKEY;
	} else if (!strncmp(str+prev, "SLASHKEY", len-prev) || !strncmp(str+prev, "SLASH", len-prev)) {
		*key= SLASHKEY;
	} else if (!strncmp(str+prev, "BACKSLASHKEY", len-prev) || !strncmp(str+prev, "BACKSLASH", len-prev)) {
		*key= BACKSLASHKEY;
	} else if (!strncmp(str+prev, "EQUALKEY", len-prev) || !strncmp(str+prev, "EQUAL", len-prev)) {
		*key= EQUALKEY;
	} else if (!strncmp(str+prev, "LEFTBRACKETKEY", len-prev) || !strncmp(str+prev, "LEFTBRACKET", len-prev)) {
		*key= LEFTBRACKETKEY;
	} else if (!strncmp(str+prev, "RIGHTBRACKETKEY", len-prev) || !strncmp(str+prev, "RIGHTBRACKET", len-prev)) {
		*key= RIGHTBRACKETKEY;
	} else if (!strncmp(str+prev, "DELKEY", len-prev) || !strncmp(str+prev, "DELETE", len-prev)) {
		*key= DELKEY;
	
	} else if (!strncmp(str+prev, "LEFTARROWKEY", len-prev) || !strncmp(str+prev, "LEFTARROW", len-prev)) {
		*key= LEFTARROWKEY;
	} else if (!strncmp(str+prev, "DOWNARROWKEY", len-prev) || !strncmp(str+prev, "DOWNARROW", len-prev)) {
		*key= DOWNARROWKEY;
	} else if (!strncmp(str+prev, "RIGHTARROWKEY", len-prev) || !strncmp(str+prev, "RIGHTARROW", len-prev)) {
		*key= RIGHTARROWKEY;
	} else if (!strncmp(str+prev, "UPARROWKEY", len-prev) || !strncmp(str+prev, "UPARROW", len-prev)) {
		*key= UPARROWKEY;

	} else if (!strncmp(str+prev, "PAD", 3)) {
		
		if (len-prev<=4) {
		
			if (str[prev]>='0' && str[prev]<='9') {
				*key= str[prev]-'0'+ZEROKEY;
			} else {
				invalid= 1;
			}
		
		} else if (!strncmp(str+prev+3, "PERIODKEY", len-prev-3) || !strncmp(str+prev+3, "PERIOD", len-prev-3)) {
			*key= PADPERIOD;
		} else if (!strncmp(str+prev+3, "SLASHKEY", len-prev-3) || !strncmp(str+prev+3, "SLASH", len-prev-3)) {
			*key= PADSLASHKEY;
		} else if (!strncmp(str+prev+3, "ASTERKEY", len-prev-3) || !strncmp(str+prev+3, "ASTERISK", len-prev-3)) {
			*key= PADASTERKEY;
		} else if (!strncmp(str+prev+3, "MINUSKEY", len-prev-3) || !strncmp(str+prev+3, "MINUS", len-prev-3)) {
			*key= PADMINUS;
		} else if (!strncmp(str+prev+3, "ENTERKEY", len-prev-3) || !strncmp(str+prev+3, "ENTER", len-prev-3)) {
			*key= PADENTER;
		} else if (!strncmp(str+prev+3, "PLUSKEY", len-prev-3) || !strncmp(str+prev+3, "PLUS", len-prev-3)) {
			*key= PADPLUSKEY;
		} else {
			invalid= 1;
		}

	} else if (!strncmp(str+prev, "F1KEY", len-prev) || !strncmp(str+prev, "F1", len-prev)) {
		*key= F1KEY;
	} else if (!strncmp(str+prev, "F2KEY", len-prev) || !strncmp(str+prev, "F2", len-prev)) {
		*key= F2KEY;
	} else if (!strncmp(str+prev, "F3KEY", len-prev) || !strncmp(str+prev, "F3", len-prev)) {
		*key= F3KEY;
	} else if (!strncmp(str+prev, "F4KEY", len-prev) || !strncmp(str+prev, "F4", len-prev)) {
		*key= F4KEY;
	} else if (!strncmp(str+prev, "F5KEY", len-prev) || !strncmp(str+prev, "F5", len-prev)) {
		*key= F5KEY;
	} else if (!strncmp(str+prev, "F6KEY", len-prev) || !strncmp(str+prev, "F6", len-prev)) {
		*key= F6KEY;
	} else if (!strncmp(str+prev, "F7KEY", len-prev) || !strncmp(str+prev, "F7", len-prev)) {
		*key= F7KEY;
	} else if (!strncmp(str+prev, "F8KEY", len-prev) || !strncmp(str+prev, "F8", len-prev)) {
		*key= F8KEY;
	} else if (!strncmp(str+prev, "F9KEY", len-prev) || !strncmp(str+prev, "F9", len-prev)) {
		*key= F9KEY;
	} else if (!strncmp(str+prev, "F10KEY", len-prev) || !strncmp(str+prev, "F10", len-prev)) {
		*key= F10KEY;
	} else if (!strncmp(str+prev, "F11KEY", len-prev) || !strncmp(str+prev, "F11", len-prev)) {
		*key= F11KEY;
	} else if (!strncmp(str+prev, "F12KEY", len-prev) || !strncmp(str+prev, "F12", len-prev)) {
		*key= F12KEY;

	} else if (!strncmp(str+prev, "PAUSEKEY", len-prev) || !strncmp(str+prev, "PAUSE", len-prev)) {
		*key= PAUSEKEY;
	} else if (!strncmp(str+prev, "INSERTKEY", len-prev) || !strncmp(str+prev, "INSERT", len-prev)) {
		*key= INSERTKEY;
	} else if (!strncmp(str+prev, "HOMEKEY", len-prev) || !strncmp(str+prev, "HOME", len-prev)) {
		*key= HOMEKEY;
	} else if (!strncmp(str+prev, "PAGEUPKEY", len-prev) || !strncmp(str+prev, "PAGEUP", len-prev)) {
		*key= PAGEUPKEY;
	} else if (!strncmp(str+prev, "PAGEDOWNKEY", len-prev) || !strncmp(str+prev, "PAGEDOWN", len-prev)) {
		*key= PAGEDOWNKEY;
	} else if (!strncmp(str+prev, "ENDKEY", len-prev) || !strncmp(str+prev, "END", len-prev)) {
		*key= ENDKEY;
	
	} else {
		invalid= 1;
	}

	if (!invalid && *key) {
		return 1;
	}
	
	return 0;
}
