/**
 * include/BPI_script.h (jan-2004 ianwill)
 *	
 * $Id: BPI_script.h 4590 2005-06-11 05:30:14Z ianwill $
 *
 * Header for BPython's script structure. BPI: Blender Python external include
 * file.
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BPI_SCRIPT_H
#define BPI_SCRIPT_H

//#include "DNA_listBase.h"
#include "DNA_ID.h"

typedef struct Script {
	ID id;

	void *py_draw;
	void *py_event;
	void *py_button;
	void *py_browsercallback;
	void *py_globaldict;

	int flags, lastspace;

} Script;

/* Note: a script that registers callbacks in the script->py_* pointers
 * above (or calls the file or image selectors) needs to keep its global
 * dictionary until Draw.Exit() is called and the callbacks removed.
 * Unsetting SCRIPT_RUNNING means the interpreter reached the end of the
 * script and returned control to Blender, but we can't get rid of its
 * namespace (global dictionary) while SCRIPT_GUI or SCRIPT_FILESEL is set,
 * because of the callbacks.  The flags and the script name are saved in
 * each running script's global dictionary, under '__script__'. */

/* Flags */
#define SCRIPT_RUNNING	0x01
#define SCRIPT_GUI			0x02
#define SCRIPT_FILESEL	0x04

#endif /* BPI_SCRIPT_H */
