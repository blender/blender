/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BLI_bounds_types.hh"
#include "BLI_compiler_attrs.h"
#include "BLI_math_vector_types.hh"
#include "BLI_sys_types.h"

struct BMEditMesh;
struct BPoint;
struct Depsgraph;
struct Lattice;
struct LatticeDeformData;
struct Main;
struct MDeformVert;
struct Mesh;
struct Object;
struct Scene;

void BKE_lattice_resize(Lattice *lt, int u_new, int v_new, int w_new, Object *lt_ob);
Lattice *BKE_lattice_add(Main *bmain, const char *name);
void calc_lat_fudu(int flag, int res, float *r_fu, float *r_du);

void outside_lattice(Lattice *lt);

float (*BKE_lattice_vert_coords_alloc(const Lattice *lt, int *r_vert_len))[3];
void BKE_lattice_vert_coords_get(const Lattice *lt, float (*vert_coords)[3]);
void BKE_lattice_vert_coords_apply_with_mat4(Lattice *lt,
                                             const float (*vert_coords)[3],
                                             const float mat[4][4]);
void BKE_lattice_vert_coords_apply(Lattice *lt, const float (*vert_coords)[3]);
void BKE_lattice_modifiers_calc(Depsgraph *depsgraph, Scene *scene, Object *ob);

MDeformVert *BKE_lattice_deform_verts_get(const Object *oblatt);
BPoint *BKE_lattice_active_point_get(Lattice *lt);

std::optional<blender::Bounds<blender::float3>> BKE_lattice_minmax(const Lattice *lt);
void BKE_lattice_center_median(Lattice *lt, float cent[3]);
void BKE_lattice_translate(Lattice *lt, const float offset[3], bool do_keys);
void BKE_lattice_transform(Lattice *lt, const float mat[4][4], bool do_keys);

bool BKE_lattice_is_any_selected(const Lattice *lt);

int BKE_lattice_index_from_uvw(const Lattice *lt, int u, int v, int w);
void BKE_lattice_index_to_uvw(const Lattice *lt, int index, int *r_u, int *r_v, int *r_w);
int BKE_lattice_index_flip(const Lattice *lt, int index, bool flip_u, bool flip_v, bool flip_w);
void BKE_lattice_bitmap_from_flag(
    const Lattice *lt, unsigned int *bitmap, uint8_t flag, bool clear, bool respecthide);

/* **** Depsgraph evaluation **** */

void BKE_lattice_eval_geometry(Depsgraph *depsgraph, Lattice *latt);

/* Draw Cache */
enum {
  BKE_LATTICE_BATCH_DIRTY_ALL = 0,
  BKE_LATTICE_BATCH_DIRTY_SELECT,
};
void BKE_lattice_batch_cache_dirty_tag(Lattice *lt, int mode);
void BKE_lattice_batch_cache_free(Lattice *lt);

extern void (*BKE_lattice_batch_cache_dirty_tag_cb)(Lattice *lt, int mode);
extern void (*BKE_lattice_batch_cache_free_cb)(Lattice *lt);

/* -------------------------------------------------------------------- */
/** \name Deform 3D Coordinates by Lattice (`lattice_deform.cc`)
 * \{ */

LatticeDeformData *BKE_lattice_deform_data_create(const Object *oblatt,
                                                  const Object *ob) ATTR_WARN_UNUSED_RESULT;
void BKE_lattice_deform_data_eval_co(LatticeDeformData *lattice_deform_data,
                                     float co[3],
                                     float weight);
void BKE_lattice_deform_data_destroy(LatticeDeformData *lattice_deform_data);

void BKE_lattice_deform_coords(const Object *ob_lattice,
                               const Object *ob_target,
                               float (*vert_coords)[3],
                               int vert_coords_len,
                               short flag,
                               const char *defgrp_name,
                               float fac);

void BKE_lattice_deform_coords_with_mesh(const Object *ob_lattice,
                                         const Object *ob_target,
                                         float (*vert_coords)[3],
                                         int vert_coords_len,
                                         short flag,
                                         const char *defgrp_name,
                                         float fac,
                                         const Mesh *me_target);

void BKE_lattice_deform_coords_with_editmesh(const Object *ob_lattice,
                                             const Object *ob_target,
                                             float (*vert_coords)[3],
                                             int vert_coords_len,
                                             short flag,
                                             const char *defgrp_name,
                                             float fac,
                                             BMEditMesh *em_target);

/** \} */
