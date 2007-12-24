/**
 * $Id: BIF_graphics.h 6596 2006-01-29 22:25:53Z broken $
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

#ifndef BIF_GRAPHICS_H
#define BIF_GRAPHICS_H

	/* XXX, should move somewhere else, with collected windowing
	 * stuff, to be done once the proper windowing stuff has
	 * been formed.
	 */
	
enum {
	CURSOR_VPAINT, 
	CURSOR_FACESEL, 
	CURSOR_WAIT, 
	CURSOR_EDIT, 
	CURSOR_X_MOVE, 
	CURSOR_Y_MOVE, 
	CURSOR_HELP, 
	CURSOR_STD, 
	CURSOR_NONE,
	CURSOR_PENCIL,
	CURSOR_TEXTEDIT
};

void set_cursor(int curs);
int get_cursor(void);

#endif /* BIF_GRAPHICS_H */

