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
#include "mydevice.h"
#include "blendef.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

char *event_to_string(short evt) {
#define smap(evt)	case evt: return #evt
	switch (evt) {
	default: return "<unknown>";
	smap(CHANGED);
	smap(DRAWEDGES);
	smap(BACKBUFDRAW);
	smap(EXECUTE);
	smap(LOAD_FILE);
	smap(RESHAPE);
	smap(UI_BUT_EVENT);
	smap(REDRAWVIEW3D);
	smap(REDRAWBUTSHEAD);
	smap(REDRAWBUTSALL);
	smap(REDRAWBUTSVIEW);
	smap(REDRAWBUTSLAMP);
	smap(REDRAWBUTSMAT);
	smap(REDRAWBUTSTEX);
	smap(REDRAWBUTSANIM);
	smap(REDRAWBUTSWORLD);
	smap(REDRAWBUTSRENDER);
	smap(REDRAWBUTSEDIT);
	smap(REDRAWVIEWCAM);
	smap(REDRAWHEADERS);
	smap(REDRAWBUTSGAME);
	smap(REDRAWBUTSRADIO);
	smap(REDRAWVIEW3D_Z);
	smap(REDRAWALL);
	smap(REDRAWINFO);
	smap(RENDERPREVIEW);
	smap(REDRAWIPO);
	smap(REDRAWDATASELECT);
	smap(REDRAWSEQ);
	smap(REDRAWIMAGE);
	smap(REDRAWOOPS);
	smap(REDRAWIMASEL);
	smap(AFTERIMASELIMA);
	smap(AFTERIMASELGET);
	smap(AFTERIMAWRITE);
	smap(IMALEFTMOUSE);
	smap(AFTERPIBREAD);
	smap(REDRAWTEXT);
	smap(REDRAWBUTSSCRIPT);
	smap(REDRAWSOUND);
	smap(REDRAWBUTSSOUND);
	smap(REDRAWACTION);
	smap(LEFTMOUSE);
	smap(MIDDLEMOUSE);
	smap(RIGHTMOUSE);
	smap(MOUSEX);
	smap(MOUSEY);
	smap(TIMER0);
	smap(TIMER1);
	smap(TIMER2);
	smap(TIMER3);
	smap(KEYBD);
	smap(RAWKEYBD);
	smap(REDRAW);
	smap(INPUTCHANGE);
	smap(QFULL);
	smap(WINFREEZE);
	smap(WINTHAW);
	smap(WINCLOSE);
	smap(WINQUIT);
	smap(Q_FIRSTTIME);
	smap(AKEY);
	smap(BKEY);
	smap(CKEY);
	smap(DKEY);
	smap(EKEY);
	smap(FKEY);
	smap(GKEY);
	smap(HKEY);
	smap(IKEY);
	smap(JKEY);
	smap(KKEY);
	smap(LKEY);
	smap(MKEY);
	smap(NKEY);
	smap(OKEY);
	smap(PKEY);
	smap(QKEY);
	smap(RKEY);
	smap(SKEY);
	smap(TKEY);
	smap(UKEY);
	smap(VKEY);
	smap(WKEY);
	smap(XKEY);
	smap(YKEY);
	smap(ZKEY);
	smap(ZEROKEY);
	smap(ONEKEY);
	smap(TWOKEY);
	smap(THREEKEY);
	smap(FOURKEY);
	smap(FIVEKEY);
	smap(SIXKEY);
	smap(SEVENKEY);
	smap(EIGHTKEY);
	smap(NINEKEY);
	smap(CAPSLOCKKEY);
	smap(LEFTCTRLKEY);
	smap(LEFTALTKEY);
	smap(RIGHTALTKEY);
	smap(RIGHTCTRLKEY);
	smap(RIGHTSHIFTKEY);
	smap(LEFTSHIFTKEY);
	smap(ESCKEY);
	smap(TABKEY);
	smap(RETKEY);
	smap(SPACEKEY);
	smap(LINEFEEDKEY);
	smap(BACKSPACEKEY);
	smap(DELKEY);
	smap(SEMICOLONKEY);
	smap(PERIODKEY);
	smap(COMMAKEY);
	smap(QUOTEKEY);
	smap(ACCENTGRAVEKEY);
	smap(MINUSKEY);
	smap(SLASHKEY);
	smap(BACKSLASHKEY);
	smap(EQUALKEY);
	smap(LEFTBRACKETKEY);
	smap(RIGHTBRACKETKEY);
	smap(LEFTARROWKEY);
	smap(DOWNARROWKEY);
	smap(RIGHTARROWKEY);
	smap(UPARROWKEY);
	smap(PAD0);
	smap(PAD1);
	smap(PAD2);
	smap(PAD3);
	smap(PAD4);
	smap(PAD5);
	smap(PAD6);
	smap(PAD7);
	smap(PAD8);
	smap(PAD9);
	smap(PADPERIOD);
	smap(PADSLASHKEY);
	smap(PADASTERKEY);
	smap(PADMINUS);
	smap(PADENTER);
	smap(PADPLUSKEY);
	smap(F1KEY);
	smap(F2KEY);
	smap(F3KEY);
	smap(F4KEY);
	smap(F5KEY);
	smap(F6KEY);
	smap(F7KEY);
	smap(F8KEY);
	smap(F9KEY);
	smap(F10KEY);
	smap(F11KEY);
	smap(F12KEY);
	smap(PAUSEKEY);
	smap(INSERTKEY);
	smap(HOMEKEY);
	smap(PAGEUPKEY);
	smap(PAGEDOWNKEY);
	smap(ENDKEY);
	smap(REDRAWBUTSCONSTRAINT);
	smap(REDRAWNLA);
	}
	#undef smap
}
