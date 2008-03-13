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

#ifndef BIF_MESHTOOLS_H
#define BIF_MESHTOOLS_H

struct Object;
struct EditVert;

extern int join_mesh(void);

extern void sort_faces(void);
extern void objects_bake_render_menu(void);
extern void objects_bake_render_ui(short event);
extern void objects_bake_render(short event, char **error_msg);

extern long mesh_octree_table(struct Object *ob, float *co, char mode);
extern int mesh_get_x_mirror_vert(struct Object *ob, int index);
extern struct EditVert *editmesh_get_x_mirror_vert(struct Object *ob, float *co);
extern int *mesh_get_x_mirror_faces(struct Object *ob);

#endif

