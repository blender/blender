/*
 * $Id: multires.h 11480 2007-08-03 16:33:08Z campbellbarton $
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

struct MultiresLevel *multires_level_n(struct Multires *mr, int n);

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
void multires_set_level_cb(void *ob, void *me);
void multires_set_level(struct Object *ob, struct Mesh *me, const int render);
void multires_update_levels(struct Mesh *me, const int render);
void multires_level_to_mesh(struct Object *ob, struct Mesh *me, const int render);
void multires_edge_level_update(void *ob, void *me);
int multires_modifier_warning();

/* after adding or removing vcolor layers, run this */
void multires_load_cols(Mesh *me);

/* multires-firstlevel.c */
/* Generic */
void multires_update_first_level(struct Mesh *me, struct EditMesh *em);
void multires_update_customdata(struct MultiresLevel *lvl1, struct CustomData *src,
                                struct CustomData *dst, const int type);
void multires_customdata_to_mesh(struct Mesh *me, struct EditMesh *em, struct MultiresLevel *lvl,
                                 struct CustomData *src, struct CustomData *dst, const int type);
void multires_del_lower_customdata(struct Multires *mr, struct MultiresLevel *cr_lvl);

void multires_add_layer(struct Mesh *me, struct CustomData *cd, const int type, const int n);
void multires_delete_layer(struct Mesh *me, struct CustomData *cd, const int type, int n);

#endif
