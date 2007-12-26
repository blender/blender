/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef MULTIRES_H
#define MULTIRES_H

struct CustomData;
struct EditMesh;
struct Object;
struct MDeformVert;
struct Mesh;
struct MultiresLevel;
struct Multires;
struct uiBlock;

/* For canceling operations that don't work with multires on or on a non-base level */
int multires_test();
int multires_level1_test();

void multires_draw_interface(struct uiBlock *block, unsigned short cx, unsigned short cy);

void multires_make(void *ob, void *me);
void multires_delete(void *ob, void *me);
void multires_level_to_editmesh(struct Object *ob, struct Mesh *me, const int render);
void multires_subdivide(void *ob, void *me);
void multires_del_lower(void *ob, void *me);
void multires_del_higher(void *ob, void *me);
void multires_set_level_cb(void *ob, void *me);
void multires_edge_level_update_cb(void *ob, void *me);
int multires_modifier_warning();

#endif
