/**
 * $Id: 
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


#ifndef BIF_EDITMODE_UNDO_H
#define BIF_EDITMODE_UNDO_H

// Add this in your local code:

extern void undo_editmode_push(char *name, 
		void (*freedata)(void *), 			// pointer to function freeing data
		void (*to_editmode)(void *),        // data to editmode conversion
		void *(*from_editmode)(void),       // editmode to data conversion
		int  (*validate_undo)(void *));     // check if undo data is still valid


// Further exported for UI is:

struct uiBlock;

extern void undo_editmode_step(int step);	// undo and redo
extern void undo_editmode_clear(void);		// free & clear all data
extern void undo_editmode_menu(void);		// history menu
extern struct uiBlock *editmode_undohistorymenu(void *arg_unused);

/* Hack to avoid multires undo data taking up insane amounts of memory */
struct Object;
void *undo_editmode_get_prev(struct Object *ob);

#endif

