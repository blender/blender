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
struct Object;
struct MDeformVert;
struct Mesh;
struct MultiresLevel;
struct Multires;
struct uiBlock;

void multires_draw_interface(struct uiBlock *block, unsigned short cx, unsigned short cy);
void multires_disp_map(void *, void*);

void multires_make(void *ob, void *me);
void multires_delete(void *ob, void *me);
struct Multires *multires_copy(struct Multires *orig);
void multires_free(struct Multires *mr);
void multires_free_level(struct MultiresLevel *lvl);
void multires_del_lower(void *ob, void *me);
void multires_del_higher(void *ob, void *me);
void multires_add_level(void *ob, void *me);
void multires_set_level(void *ob, void *me);
void multires_update_levels(struct Mesh *me);
void multires_level_to_mesh(struct Object *ob, struct Mesh *me);
void multires_calc_level_maps(struct MultiresLevel *lvl);
void multires_edge_level_update(void *ob, void *me);
int multires_modifier_warning();

/* multires-firstlevel.c */
/* Generic */
typedef enum FirstLevelType {
	FirstLevelType_Vert, FirstLevelType_Face
} FirstLevelType;

void multires_update_customdata(struct MultiresLevel *lvl1, struct CustomData *src,
                                struct CustomData *dst, const FirstLevelType type);
void multires_customdata_to_mesh(struct Mesh *me, struct EditMesh *em, struct MultiresLevel *lvl,
                                 struct CustomData *src, struct CustomData *dst, const FirstLevelType type);
void multires_del_lower_customdata(struct Multires *mr, struct MultiresLevel *cr_lvl);

#endif
