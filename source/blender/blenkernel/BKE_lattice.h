/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#ifndef __BKE_LATTICE_H__
#define __BKE_LATTICE_H__

/** \file BKE_lattice.h
 *  \ingroup bke
 *  \author Ton Roosendaal
 *  \since June 2001
 */

#include "BLI_compiler_attrs.h"

struct Lattice;
struct Main;
struct Mesh;
struct Object;
struct Scene;
struct BPoint;
struct MDeformVert;
struct Depsgraph;

void BKE_lattice_resize(struct Lattice *lt, int u, int v, int w, struct Object *ltOb);
void BKE_lattice_init(struct Lattice *lt);
struct Lattice *BKE_lattice_add(struct Main *bmain, const char *name);
void BKE_lattice_copy_data(struct Main *bmain, struct Lattice *lt_dst, const struct Lattice *lt_src, const int flag);
struct Lattice *BKE_lattice_copy(struct Main *bmain, const struct Lattice *lt);
void BKE_lattice_free(struct Lattice *lt);
void BKE_lattice_make_local(struct Main *bmain, struct Lattice *lt, const bool lib_local);
void calc_lat_fudu(int flag, int res, float *r_fu, float *r_du);

struct LatticeDeformData *init_latt_deform(struct Object *oblatt, struct Object *ob) ATTR_WARN_UNUSED_RESULT;
void calc_latt_deform(struct LatticeDeformData *lattice_deform_data, float co[3], float weight);
void end_latt_deform(struct LatticeDeformData *lattice_deform_data);

bool object_deform_mball(struct Object *ob, struct ListBase *dispbase);
void outside_lattice(struct Lattice *lt);

void curve_deform_verts(struct Object *cuOb, struct Object *target,
        struct Mesh *mesh, float (*vertexCos)[3],
        int numVerts, const char *vgroup, short defaxis);
void curve_deform_vector(struct Object *cuOb, struct Object *target,
                         float orco[3], float vec[3], float mat[3][3], int no_rot_axis);

void lattice_deform_verts(struct Object *laOb, struct Object *target,
                          struct Mesh *mesh, float (*vertexCos)[3],
                          int numVerts, const char *vgroup, float influence);
void armature_deform_verts(struct Object *armOb, struct Object *target,
                           const struct Mesh *mesh, float (*vertexCos)[3],
                           float (*defMats)[3][3], int numVerts, int deformflag,
                           float (*prevCos)[3], const char *defgrp_name);

float (*BKE_lattice_vertexcos_get(struct Object *ob, int *r_numVerts))[3];
void    BKE_lattice_vertexcos_apply(struct Object *ob, float (*vertexCos)[3]);
void    BKE_lattice_modifiers_calc(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);

struct MDeformVert *BKE_lattice_deform_verts_get(struct Object *lattice);
struct BPoint *BKE_lattice_active_point_get(struct Lattice *lt);

struct BoundBox *BKE_lattice_boundbox_get(struct Object *ob);
void BKE_lattice_minmax_dl(struct Object *ob, struct Lattice *lt, float min[3], float max[3]);
void BKE_lattice_minmax(struct Lattice *lt, float min[3], float max[3]);
void BKE_lattice_center_median(struct Lattice *lt, float cent[3]);
void BKE_lattice_center_bounds(struct Lattice *lt, float cent[3]);
void BKE_lattice_translate(struct Lattice *lt, float offset[3], bool do_keys);
void BKE_lattice_transform(struct Lattice *lt, float mat[4][4], bool do_keys);

bool BKE_lattice_is_any_selected(const struct Lattice *lt);

int  BKE_lattice_index_from_uvw(struct Lattice *lt, const int u, const int v, const int w);
void BKE_lattice_index_to_uvw(struct Lattice *lt, const int index, int *r_u, int *r_v, int *r_w);
int  BKE_lattice_index_flip(struct Lattice *lt, const int index,
                            const bool flip_u, const bool flip_v, const bool flip_w);
void BKE_lattice_bitmap_from_flag(struct Lattice *lt, unsigned int *bitmap, const short flag,
                                  const bool clear, const bool respecthide);

/* **** Depsgraph evaluation **** */

struct Depsgraph;

void BKE_lattice_eval_geometry(struct Depsgraph *depsgraph,
                               struct Lattice *latt);

/* Draw Cache */
enum {
	BKE_LATTICE_BATCH_DIRTY_ALL = 0,
	BKE_LATTICE_BATCH_DIRTY_SELECT,
};
void BKE_lattice_batch_cache_dirty(struct Lattice *lt, int mode);
void BKE_lattice_batch_cache_free(struct Lattice *lt);

#endif  /* __BKE_LATTICE_H__ */
