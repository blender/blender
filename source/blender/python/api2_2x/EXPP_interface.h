/* 
 * $Id: EXPP_interface.h 7338 2006-04-30 16:22:31Z ianwill $
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
 * Contributor(s): Michel Selten, Willian P. Germano
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_INTERFACE_H
#define EXPP_INTERFACE_H

struct Object;
struct Script;
struct LinkNode;

extern struct LinkNode *bpy_pydriver_oblist;

void initBlenderApi2_2x( void );
char *bpy_gethome( int append_scriptsdir );
void discardFromBDict( char *key );
void EXPP_Library_Close( void );   /* in Library.c, used by BPY_end_python */

/* PyDrivers */

void bpy_pydriver_freeList(void);
void bpy_pydriver_appendToList(struct Object *ob);
struct Object **bpy_pydriver_obArrayFromList(void);

int bpy_during_pydriver(void);
void bpy_pydriver_running(int state);

#endif /* EXPP_INTERFACE_H */
