/*
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
 */
#ifndef __BKE_CURVE_H__
#define __BKE_CURVE_H__

/** \file
 * \ingroup bke
 */

#include "DNA_scene_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BezTriple;
struct Curve;
struct Depsgraph;
struct GHash;
struct ListBase;
struct MDeformVert;
struct Main;
struct Nurb;
struct Object;
struct Path;
struct TextBox;
struct rctf;

typedef int eBezTriple_Flag__Alias;

typedef struct CurveCache {
  ListBase disp;
  ListBase bev;
  ListBase deformed_nurbs;
  struct Path *path;
} CurveCache;

/* Definitions needed for shape keys */
typedef struct CVKeyIndex {
  void *orig_cv;
  int key_index, nu_index, pt_index, vertex_index;
  bool switched;
} CVKeyIndex;

#define KNOTSU(nu) \
  ((nu)->orderu + (nu)->pntsu + (((nu)->flagu & CU_NURB_CYCLIC) ? ((nu)->orderu - 1) : 0))
#define KNOTSV(nu) \
  ((nu)->orderv + (nu)->pntsv + (((nu)->flagv & CU_NURB_CYCLIC) ? ((nu)->orderv - 1) : 0))

/* Non cyclic nurbs have 1 less segment */
#define SEGMENTSU(nu) (((nu)->flagu & CU_NURB_CYCLIC) ? (nu)->pntsu : (nu)->pntsu - 1)
#define SEGMENTSV(nu) (((nu)->flagv & CU_NURB_CYCLIC) ? (nu)->pntsv : (nu)->pntsv - 1)

#define CU_DO_TILT(cu, nu) ((((nu)->flag & CU_2D) && ((cu)->flag & CU_3D) == 0) ? 0 : 1)
#define CU_DO_RADIUS(cu, nu) \
  ((CU_DO_TILT(cu, nu) || ((cu)->flag & CU_PATH_RADIUS) || (cu)->bevobj || (cu)->ext1 != 0.0f || \
    (cu)->ext2 != 0.0f) ? \
       1 : \
       0)

/* not 3d and not unfilled */
#define CU_DO_2DFILL(cu) \
  ((((cu)->flag & CU_3D) == 0) && (((cu)->flag & (CU_FRONT | CU_BACK)) != 0))

/* ** Curve ** */
void BKE_curve_editfont_free(struct Curve *cu);
void BKE_curve_init(struct Curve *cu, const short curve_type);
struct Curve *BKE_curve_add(struct Main *bmain, const char *name, int type);
struct Curve *BKE_curve_copy(struct Main *bmain, const struct Curve *cu);
short BKE_curve_type_get(struct Curve *cu);
void BKE_curve_type_test(struct Object *ob);
void BKE_curve_curve_dimension_update(struct Curve *cu);

struct BoundBox *BKE_curve_boundbox_get(struct Object *ob);

void BKE_curve_texspace_calc(struct Curve *cu);
void BKE_curve_texspace_ensure(struct Curve *cu);
void BKE_curve_texspace_get(struct Curve *cu, float r_loc[3], float r_size[3]);

bool BKE_curve_minmax(struct Curve *cu, bool use_radius, float min[3], float max[3]);
bool BKE_curve_center_median(struct Curve *cu, float cent[3]);
bool BKE_curve_center_bounds(struct Curve *cu, float cent[3]);
void BKE_curve_transform_ex(struct Curve *cu,
                            float mat[4][4],
                            const bool do_keys,
                            const bool do_props,
                            const float unit_scale);
void BKE_curve_transform(struct Curve *cu,
                         float mat[4][4],
                         const bool do_keys,
                         const bool do_props);
void BKE_curve_translate(struct Curve *cu, float offset[3], const bool do_keys);
void BKE_curve_material_index_remove(struct Curve *cu, int index);
bool BKE_curve_material_index_used(struct Curve *cu, int index);
void BKE_curve_material_index_clear(struct Curve *cu);
bool BKE_curve_material_index_validate(struct Curve *cu);
void BKE_curve_material_remap(struct Curve *cu, const unsigned int *remap, unsigned int remap_len);

void BKE_curve_smooth_flag_set(struct Curve *cu, const bool use_smooth);

ListBase *BKE_curve_nurbs_get(struct Curve *cu);

int BKE_curve_nurb_vert_index_get(const struct Nurb *nu, const void *vert);
void BKE_curve_nurb_active_set(struct Curve *cu, const struct Nurb *nu);
struct Nurb *BKE_curve_nurb_active_get(struct Curve *cu);
void *BKE_curve_vert_active_get(struct Curve *cu);
void BKE_curve_nurb_vert_active_set(struct Curve *cu, const struct Nurb *nu, const void *vert);
bool BKE_curve_nurb_vert_active_get(struct Curve *cu, struct Nurb **r_nu, void **r_vert);
void BKE_curve_nurb_vert_active_validate(struct Curve *cu);

float (*BKE_curve_nurbs_vert_coords_alloc(struct ListBase *lb, int *r_vert_len))[3];
void BKE_curve_nurbs_vert_coords_get(struct ListBase *lb, float (*vert_coords)[3], int vert_len);

void BKE_curve_nurbs_vert_coords_apply_with_mat4(struct ListBase *lb,
                                                 const float (*vert_coords)[3],
                                                 const float mat[4][4],
                                                 const bool constrain_2d);

void BKE_curve_nurbs_vert_coords_apply(struct ListBase *lb,
                                       const float (*vert_coords)[3],
                                       const bool constrain_2d);

float (*BKE_curve_nurbs_key_vert_coords_alloc(struct ListBase *lb,
                                              float *key,
                                              int *r_vert_len))[3];
void BKE_curve_nurbs_key_vert_tilts_apply(struct ListBase *lb, float *key);

void BKE_curve_editNurb_keyIndex_delCV(struct GHash *keyindex, const void *cv);
void BKE_curve_editNurb_keyIndex_free(struct GHash **keyindex);
void BKE_curve_editNurb_free(struct Curve *cu);
struct ListBase *BKE_curve_editNurbs_get(struct Curve *cu);

void BKE_curve_bevelList_free(struct ListBase *bev);
void BKE_curve_bevelList_make(struct Object *ob, struct ListBase *nurbs, bool for_render);
void BKE_curve_bevel_make(struct Object *ob, struct ListBase *disp);

void BKE_curve_forward_diff_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride);
void BKE_curve_forward_diff_tangent_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride);

void BKE_curve_rect_from_textbox(const struct Curve *cu,
                                 const struct TextBox *tb,
                                 struct rctf *r_rect);

/* ** Nurbs ** */

bool BKE_nurbList_index_get_co(struct ListBase *editnurb, const int index, float r_co[3]);

int BKE_nurbList_verts_count(struct ListBase *nurb);
int BKE_nurbList_verts_count_without_handles(struct ListBase *nurb);

void BKE_nurbList_free(struct ListBase *lb);
void BKE_nurbList_duplicate(struct ListBase *lb1, const struct ListBase *lb2);
void BKE_nurbList_handles_set(struct ListBase *editnurb, const char code);
void BKE_nurbList_handles_recalculate(struct ListBase *editnurb,
                                      const bool calc_length,
                                      const char flag);

void BKE_nurbList_handles_autocalc(ListBase *editnurb, int flag);
void BKE_nurbList_flag_set(ListBase *editnurb, short flag, bool set);
bool BKE_nurbList_flag_set_from_flag(ListBase *editnurb, short from_flag, short flag);

void BKE_nurb_free(struct Nurb *nu);
struct Nurb *BKE_nurb_duplicate(const struct Nurb *nu);
struct Nurb *BKE_nurb_copy(struct Nurb *src, int pntsu, int pntsv);

void BKE_nurb_test_2d(struct Nurb *nu);
void BKE_nurb_minmax(struct Nurb *nu, bool use_radius, float min[3], float max[3]);
float BKE_nurb_calc_length(const struct Nurb *nu, int resolution);

void BKE_nurb_makeFaces(
    const struct Nurb *nu, float *coord_array, int rowstride, int resolu, int resolv);
void BKE_nurb_makeCurve(const struct Nurb *nu,
                        float *coord_array,
                        float *tilt_array,
                        float *radius_array,
                        float *weight_array,
                        int resolu,
                        int stride);

unsigned int BKE_curve_calc_coords_axis_len(const unsigned int bezt_array_len,
                                            const unsigned int resolu,
                                            const bool is_cyclic,
                                            const bool use_cyclic_duplicate_endpoint);
void BKE_curve_calc_coords_axis(const struct BezTriple *bezt_array,
                                const unsigned int bezt_array_len,
                                const unsigned int resolu,
                                const bool is_cyclic,
                                const bool use_cyclic_duplicate_endpoint,
                                /* array params */
                                const unsigned int axis,
                                const unsigned int stride,
                                float *r_points);

void BKE_nurb_knot_calc_u(struct Nurb *nu);
void BKE_nurb_knot_calc_v(struct Nurb *nu);

/* nurb checks if they can be drawn, also clamp order func */
bool BKE_nurb_check_valid_u(const struct Nurb *nu);
bool BKE_nurb_check_valid_v(const struct Nurb *nu);
bool BKE_nurb_check_valid_uv(const struct Nurb *nu);

bool BKE_nurb_order_clamp_u(struct Nurb *nu);
bool BKE_nurb_order_clamp_v(struct Nurb *nu);

void BKE_nurb_direction_switch(struct Nurb *nu);
bool BKE_nurb_type_convert(struct Nurb *nu,
                           const short type,
                           const bool use_handles,
                           const char **r_err_msg);

void BKE_nurb_points_add(struct Nurb *nu, int number);
void BKE_nurb_bezierPoints_add(struct Nurb *nu, int number);

int BKE_nurb_index_from_uv(struct Nurb *nu, int u, int v);
void BKE_nurb_index_to_uv(struct Nurb *nu, int index, int *r_u, int *r_v);

struct BezTriple *BKE_nurb_bezt_get_next(struct Nurb *nu, struct BezTriple *bezt);
struct BezTriple *BKE_nurb_bezt_get_prev(struct Nurb *nu, struct BezTriple *bezt);
struct BPoint *BKE_nurb_bpoint_get_next(struct Nurb *nu, struct BPoint *bp);
struct BPoint *BKE_nurb_bpoint_get_prev(struct Nurb *nu, struct BPoint *bp);

void BKE_nurb_bezt_calc_normal(struct Nurb *nu, struct BezTriple *bezt, float r_normal[3]);
void BKE_nurb_bezt_calc_plane(struct Nurb *nu, struct BezTriple *bezt, float r_plane[3]);

void BKE_nurb_bpoint_calc_normal(struct Nurb *nu, struct BPoint *bp, float r_normal[3]);
void BKE_nurb_bpoint_calc_plane(struct Nurb *nu, struct BPoint *bp, float r_plane[3]);

void BKE_nurb_handle_calc(struct BezTriple *bezt,
                          struct BezTriple *prev,
                          struct BezTriple *next,
                          const bool is_fcurve,
                          const char smoothing);
void BKE_nurb_handle_calc_ex(struct BezTriple *bezt,
                             struct BezTriple *prev,
                             struct BezTriple *next,
                             const eBezTriple_Flag__Alias handle_sel_flag,
                             const bool is_fcurve,
                             const char smoothing);
void BKE_nurb_handle_calc_simple(struct Nurb *nu, struct BezTriple *bezt);
void BKE_nurb_handle_calc_simple_auto(struct Nurb *nu, struct BezTriple *bezt);

void BKE_nurb_handle_smooth_fcurve(struct BezTriple *bezt, int total, bool cyclic);

void BKE_nurb_handles_calc(struct Nurb *nu);
void BKE_nurb_handles_autocalc(struct Nurb *nu, int flag);
void BKE_nurb_bezt_handle_test(struct BezTriple *bezt,
                               const eBezTriple_Flag__Alias sel_flag,
                               const bool use_handle,
                               const bool use_around_local);
void BKE_nurb_handles_test(struct Nurb *nu, const bool use_handles, const bool use_around_local);

/* **** Depsgraph evaluation **** */

void BKE_curve_eval_geometry(struct Depsgraph *depsgraph, struct Curve *curve);

/* Draw Cache */
enum {
  BKE_CURVE_BATCH_DIRTY_ALL = 0,
  BKE_CURVE_BATCH_DIRTY_SELECT,
};
void BKE_curve_batch_cache_dirty_tag(struct Curve *cu, int mode);
void BKE_curve_batch_cache_free(struct Curve *cu);

extern void (*BKE_curve_batch_cache_dirty_tag_cb)(struct Curve *cu, int mode);
extern void (*BKE_curve_batch_cache_free_cb)(struct Curve *cu);

/* -------------------------------------------------------------------- */
/** \name Decimate Curve (curve_decimate.c)
 *
 * Simplify curve data.
 * \{ */

unsigned int BKE_curve_decimate_bezt_array(struct BezTriple *bezt_array,
                                           const unsigned int bezt_array_len,
                                           const unsigned int resolu,
                                           const bool is_cyclic,
                                           const char flag_test,
                                           const char flag_set,
                                           const float error_sq_max,
                                           const unsigned int error_target_len);

void BKE_curve_decimate_nurb(struct Nurb *nu,
                             const unsigned int resolu,
                             const float error_sq_max,
                             const unsigned int error_target_len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deform 3D Coordinates by Curve (curve_deform.c)
 * \{ */

void BKE_curve_deform_coords(struct Object *ob_curve,
                             struct Object *ob_target,
                             float (*vert_coords)[3],
                             const int vert_coords_len,
                             const struct MDeformVert *dvert,
                             const int defgrp_index,
                             const short flag,
                             const short defaxis);
void BKE_curve_deform_co(struct Object *ob_curve,
                         struct Object *ob_target,
                         const float orco[3],
                         float vec[3],
                         float mat[3][3],
                         const int no_rot_axis);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __BKE_CURVE_H__ */
