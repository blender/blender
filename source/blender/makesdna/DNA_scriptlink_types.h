/**
 * blenlib/DNA_object_types.h (mar-2001 nzc)
 *	
 * Scriptlink is hard-coded in object for some reason.
 *
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
#ifndef DNA_SCRIPTLINK_TYPES_H
#define DNA_SCRIPTLINK_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

struct ID;

typedef struct ScriptLink {
	struct ID **scripts;
	short *flag;
	
	short actscript, totscript;
	int pad;
} ScriptLink;

/* **************** SCRIPTLINKS ********************* */

#define SCRIPT_FRAMECHANGED	1
#define SCRIPT_ONLOAD 2
#define SCRIPT_REDRAW	4
#define SCRIPT_ONSAVE	8
#define SCRIPT_RENDER 16
/* POSTRENDER is not meant for the UI, it simply calls the
 * RENDER script links for clean-up actions */
#define SCRIPT_POSTRENDER 32

/* **************** SPACE HANDLERS ********************* */
/* these are special scriptlinks that can be assigned to
 * a given space in a given ScrArea to:
 * - (EVENT type) handle events sent to that space;
 * - (DRAW type) draw on the space after its own drawing function finishes
 */
#define SPACEHANDLER_VIEW3D_EVENT 1
#define SPACEHANDLER_VIEW3D_DRAW 2


#ifdef __cplusplus
}
#endif
#endif
