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



#ifndef WM_CURSORS_H
#define WM_CURSORS_H

void wm_init_cursor_data(void);

#define BC_GHOST_CURSORS 1000

/* old cursors */
enum {
	CURSOR_FACESEL=BC_GHOST_CURSORS, 
	CURSOR_WAIT, 
	CURSOR_EDIT, 
	CURSOR_X_MOVE, 
	CURSOR_Y_MOVE, 
	CURSOR_HELP, 
	CURSOR_STD, 
	CURSOR_NONE,
	CURSOR_PENCIL,
};


//typedef struct BCursor_s BCursor;
typedef struct BCursor {

	char *small_bm;
	char *small_mask;

	char small_sizex; 
	char small_sizey; 
	char small_hotx; 
	char small_hoty; 

	char *big_bm; 
	char *big_mask;

	char big_sizex; 
	char big_sizey; 
	char big_hotx; 
	char big_hoty; 

	char fg_color; 
	char bg_color; 

} BCursor;

#define SYSCURSOR 1
enum {
	BC_NW_ARROWCURSOR=2, 
	BC_NS_ARROWCURSOR,
	BC_EW_ARROWCURSOR,
	BC_WAITCURSOR,
	BC_CROSSCURSOR,
	BC_EDITCROSSCURSOR,
	BC_BOXSELCURSOR,
	BC_KNIFECURSOR,
	BC_VLOOPCURSOR,
	BC_TEXTEDITCURSOR,
	BC_PAINTBRUSHCURSOR,
	BC_HANDCURSOR,
	BC_NSEW_SCROLLCURSOR,
	BC_NS_SCROLLCURSOR,
	BC_EW_SCROLLCURSOR,
	BC_EYEDROPPER_CURSOR,
	BC_SWAPAREA_CURSOR,
/* --- ALWAYS LAST ----- */
	BC_NUMCURSORS,
};


enum {
	BC_BLACK=0, 
	BC_WHITE, 
	BC_RED,
	BC_BLUE,
	BC_GREEN,
	BC_YELLOW
};

#define SMALL_CURSOR 	0
#define BIG_CURSOR 		1

#endif /* WM_CURSORS_H */

