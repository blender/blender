/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_sys_types.h"

#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMEditMesh;
struct BezTriple;
struct Curve;
struct Depsgraph;
struct GHash;
struct ListBase;
struct MDeformVert;
struct Main;
struct Nurb;
struct Object;
struct TextBox;
struct rctf;

typedef int eBezTriple_Flag__Alias;

typedef struct CurveCache {
  ListBase disp;
  ListBase bev;
  ListBase deformed_nurbs;
  /* This array contains the accumulative length of the curve segments.
   * So you can see this as a "total distance traveled" along the curve.
   * The first entry is the length between point 0 and 1 while the last is the
   * total length of the curve.
   *
   * Used by #BKE_where_on_path. */
  const float *anim_path_accum_length;
} CurveCache;

/* Definitions needed for shape keys */
typedef struct CVKeyIndex {
  void *orig_cv;
  int key_index, nu_index, pt_index, vertex_index;
  bool switched;
} CVKeyIndex;

typedef enum eNurbHandleTest_Mode {
  /** Read the selection from each handle. */
  NURB_HANDLE_TEST_EACH = 1,
  /**
   * When the knot (center point) is selected treat the handles as selected too.
   * Otherwise use the same behavior as #NURB_HANDLE_TEST_EACH.
   */
  NURB_HANDLE_TEST_KNOT_OR_EACH = 2,
  /**
   * When the knot is selected, treat all handles as selected, otherwise none.
   * \note Typically used when handles are hidden.
   */
  NURB_HANDLE_TEST_KNOT_ONLY = 3,
} eNurbHandleTest_Mode;

#define KNOTSU(nu) \
  ((nu)->orderu + (nu)->pntsu + (((nu)->flagu & CU_NURB_CYCLIC) ? ((nu)->orderu - 1) : 0))
#define KNOTSV(nu) \
  ((nu)->orderv + (nu)->pntsv + (((nu)->flagv & CU_NURB_CYCLIC) ? ((nu)->orderv - 1) : 0))

/* Non cyclic nurbs have 1 less segment */
#define SEGMENTSU(nu) (((nu)->flagu & CU_NURB_CYCLIC) ? (nu)->pntsu : (nu)->pntsu - 1)
#define SEGMENTSV(nu) (((nu)->flagv & CU_NURB_CYCLIC) ? (nu)->pntsv : (nu)->pntsv - 1)

#define CU_DO_RADIUS(cu, nu) \
  ((((cu)->flag & (CU_PATH_RADIUS | CU_3D)) || (cu)->bevobj || (cu)->extrude != 0.0f || \
    (cu)->bevel_radius != 0.0f) ? \
       1 : \
       0)

#define CU_IS_2D(cu) (((cu)->flag & CU_3D) == 0)

/* not 3d and not unfilled */
#define CU_DO_2DFILL(cu) (CU_IS_2D(cu) && (((cu)->flag & (CU_FRONT | CU_BACK)) != 0))

/* ** Curve ** */
/**
 * Frees edit-curve entirely.
 */
void BKE_curve_editfont_free(struct Curve *cu);
void BKE_curve_init(struct Curve *cu, short curve_type);
struct Curve *BKE_curve_add(struct Main *bmain, const char *name, int type);
short BKE_curve_type_get(const struct Curve *cu);
void BKE_curve_type_test(struct Object *ob);
void BKE_curve_dimension_update(struct Curve *cu);

struct BoundBox *BKE_curve_boundbox_get(struct Object *ob);

void BKE_curve_texspace_calc(struct Curve *cu);
void BKE_curve_texspace_ensure(struct Curve *cu);

/* Basic vertex data functions. */

bool BKE_curve_minmax(struct Curve *cu, bool use_radius, float min[3], float max[3]);
bool BKE_curve_center_median(struct Curve *cu, float cent[3]);
bool BKE_curve_center_bounds(struct Curve *cu, float cent[3]);
void BKE_curve_transform_ex(
    struct Curve *cu, const float mat[4][4], bool do_keys, bool do_props, float unit_scale);
void BKE_curve_transform(struct Curve *cu, const float mat[4][4], bool do_keys, bool do_props);
void BKE_curve_translate(struct Curve *cu, const float offset[3], bool do_keys);
void BKE_curve_material_index_remove(struct Curve *cu, int index);
bool BKE_curve_material_index_used(const struct Curve *cu, int index);
void BKE_curve_material_index_clear(struct Curve *cu);
bool BKE_curve_material_index_validate(struct Curve *cu);
void BKE_curve_material_remap(struct Curve *cu, const unsigned int *remap, unsigned int remap_len);

void BKE_curve_smooth_flag_set(struct Curve *cu, bool use_smooth);

/**
 * \return edit-nurbs or normal nurbs list.
 */
ListBase *BKE_curve_nurbs_get(struct Curve *cu);
const ListBase *BKE_curve_nurbs_get_for_read(const struct Curve *cu);

int BKE_curve_nurb_vert_index_get(const struct Nurb *nu, const void *vert);
void BKE_curve_nurb_active_set(struct Curve *cu, const struct Nurb *nu);
struct Nurb *BKE_curve_nurb_active_get(struct Curve *cu);
/**
 * Get active vert for curve.
 */
void *BKE_curve_vert_active_get(struct Curve *cu);
/**
 * Set active nurb and active vert for curve.
 */
void BKE_curve_nurb_vert_active_set(struct Curve *cu, const struct Nurb *nu, const void *vert);
/**
 * Get points to the active nurb and active vert for curve.
 */
bool BKE_curve_nurb_vert_active_get(struct Curve *cu, struct Nurb **r_nu, void **r_vert);
void BKE_curve_nurb_vert_active_validate(struct Curve *cu);

float (*BKE_curve_nurbs_vert_coords_alloc(const struct ListBase *lb, int *r_vert_len))[3];
void BKE_curve_nurbs_vert_coords_get(const struct ListBase *lb,
                                     float (*vert_coords)[3],
                                     int vert_len);

void BKE_curve_nurbs_vert_coords_apply_with_mat4(struct ListBase *lb,
                                                 const float (*vert_coords)[3],
                                                 const float mat[4][4],
                                                 bool constrain_2d);

void BKE_curve_nurbs_vert_coords_apply(struct ListBase *lb,
                                       const float (*vert_coords)[3],
                                       bool constrain_2d);

float (*BKE_curve_nurbs_key_vert_coords_alloc(const struct ListBase *lb,
                                              float *key,
                                              int *r_vert_len))[3];
void BKE_curve_nurbs_key_vert_tilts_apply(struct ListBase *lb, const float *key);

void BKE_curve_editNurb_keyIndex_delCV(struct GHash *keyindex, const void *cv);
void BKE_curve_editNurb_keyIndex_free(struct GHash **keyindex);
void BKE_curve_editNurb_free(struct Curve *cu);
/**
 * Get list of nurbs from edit-nurbs structure.
 */
struct ListBase *BKE_curve_editNurbs_get(struct Curve *cu);
const struct ListBase *BKE_curve_editNurbs_get_for_read(const struct Curve *cu);

void BKE_curve_bevelList_free(struct ListBase *bev);
void BKE_curve_bevelList_make(struct Object *ob, const struct ListBase *nurbs, bool for_render);
ListBase BKE_curve_bevel_make(const struct Curve *curve);

/**
 * Forward differencing method for bezier curve.
 */
void BKE_curve_forward_diff_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride);
/**
 * Forward differencing method for first derivative of cubic bezier curve.
 */
void BKE_curve_forward_diff_tangent_bezier(
    float q0, float q1, float q2, float q3, float *p, int it, int stride);

void BKE_curve_rect_from_textbox(const struct Curve *cu,
                                 const struct TextBox *tb,
                                 struct rctf *r_rect);

/**
 * This function is almost the same as #BKE_fcurve_correct_bezpart,
 * but doesn't allow as large a tangent.
 */
void BKE_curve_correct_bezpart(const float v1[2], float v2[2], float v3[2], const float v4[2]);

/* ** Nurbs ** */

bool BKE_nurbList_index_get_co(struct ListBase *editnurb, int index, float r_co[3]);

int BKE_nurbList_verts_count(const struct ListBase *nurb);
int BKE_nurbList_verts_count_without_handles(const struct ListBase *nurb);

void BKE_nurbList_free(struct ListBase *lb);
void BKE_nurbList_duplicate(struct ListBase *lb1, const struct ListBase *lb2);
/**
 * \param code:
 * - 1 (#HD_AUTO): set auto-handle.
 * - 2 (#HD_VECT): set vector-handle.
 * - 3 (#HD_ALIGN) it toggle, vector-handles become #HD_FREE.
 *
 * - 5: Set align, like 3 but no toggle.
 * - 6: Clear align (setting #HD_FREE), like 3 but no toggle.
 */
void BKE_nurbList_handles_set(struct ListBase *editnurb,
                              eNurbHandleTest_Mode handle_mode,
                              char code);
void BKE_nurbList_handles_recalculate(struct ListBase *editnurb, bool calc_length, uint8_t flag);

void BKE_nurbList_handles_autocalc(ListBase *editnurb, uint8_t flag);
void BKE_nurbList_flag_set(ListBase *editnurb, uint8_t flag, bool set);
/**
 * Set \a flag for every point that already has \a from_flag set.
 */
bool BKE_nurbList_flag_set_from_flag(ListBase *editnurb, uint8_t from_flag, uint8_t flag);

void BKE_nurb_free(struct Nurb *nu);
struct Nurb *BKE_nurb_duplicate(const struct Nurb *nu);
/**
 * Copy the nurb but allow for different number of points (to be copied after this).
 */
struct Nurb *BKE_nurb_copy(struct Nurb *src, int pntsu, int pntsv);

void BKE_nurb_project_2d(struct Nurb *nu);
/**
 * if use_radius is truth, minmax will take points' radius into account,
 * which will make bound-box closer to beveled curve.
 */
void BKE_nurb_minmax(const struct Nurb *nu, bool use_radius, float min[3], float max[3]);
float BKE_nurb_calc_length(const struct Nurb *nu, int resolution);

/**
 * \param coord_array: has to be `(3 * 4 * resolu * resolv)` in size, and zero-ed.
 */
void BKE_nurb_makeFaces(
    const struct Nurb *nu, float *coord_array, int rowstride, int resolu, int resolv);
/**
 * \param coord_array: Has to be `(3 * 4 * pntsu * resolu)` in size and zero-ed
 * \param tilt_array: set when non-NULL
 * \param radius_array: set when non-NULL
 */
void BKE_nurb_makeCurve(const struct Nurb *nu,
                        float *coord_array,
                        float *tilt_array,
                        float *radius_array,
                        float *weight_array,
                        int resolu,
                        int stride);

/**
 * Calculate the length for arrays filled in by #BKE_curve_calc_coords_axis.
 */
unsigned int BKE_curve_calc_coords_axis_len(unsigned int bezt_array_len,
                                            unsigned int resolu,
                                            bool is_cyclic,
                                            bool use_cyclic_duplicate_endpoint);
/**
 * Calculate an array for the entire curve (cyclic or non-cyclic).
 * \note Call for each axis.
 *
 * \param use_cyclic_duplicate_endpoint: Duplicate values at the beginning & end of the array.
 */
void BKE_curve_calc_coords_axis(const struct BezTriple *bezt_array,
                                unsigned int bezt_array_len,
                                unsigned int resolu,
                                bool is_cyclic,
                                bool use_cyclic_duplicate_endpoint,
                                /* array params */
                                unsigned int axis,
                                unsigned int stride,
                                float *r_points);

void BKE_nurb_knot_calc_u(struct Nurb *nu);
void BKE_nurb_knot_calc_v(struct Nurb *nu);

/* nurb checks if they can be drawn, also clamp order func */
bool BKE_nurb_check_valid_u(const struct Nurb *nu);
bool BKE_nurb_check_valid_v(const struct Nurb *nu);
bool BKE_nurb_check_valid_uv(const struct Nurb *nu);
bool BKE_nurb_valid_message(int pnts,
                            short order,
                            short flag,
                            short type,
                            bool is_surf,
                            int dir,
                            char *message_dst,
                            size_t maxncpy);

bool BKE_nurb_order_clamp_u(struct Nurb *nu);
bool BKE_nurb_order_clamp_v(struct Nurb *nu);

void BKE_nurb_direction_switch(struct Nurb *nu);
/**
 * \note caller must ensure active vertex remains valid.
 */
bool BKE_nurb_type_convert(struct Nurb *nu, short type, bool use_handles, const char **r_err_msg);

/**
 * Be sure to call #BKE_nurb_knot_calc_u / #BKE_nurb_knot_calc_v after this.
 */
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

/**
 * Recalculate the handles of a nurb bezier-triple. Acts based on handle selection with `SELECT`
 * flag. To use a different flag, use #BKE_nurb_handle_calc_ex().
 */
void BKE_nurb_handle_calc(struct BezTriple *bezt,
                          struct BezTriple *prev,
                          struct BezTriple *next,
                          bool is_fcurve,
                          char smoothing);
/**
 * Variant of #BKE_nurb_handle_calc() that allows calculating based on a different select flag.
 *
 * \param handle_sel_flag: The flag (bezt.f1/2/3) value to use to determine selection.
 * Usually #SELECT, but may want to use a different one at times
 * (if caller does not operate on selection).
 */
void BKE_nurb_handle_calc_ex(struct BezTriple *bezt,
                             struct BezTriple *prev,
                             struct BezTriple *next,
                             eBezTriple_Flag__Alias handle_sel_flag,
                             bool is_fcurve,
                             char smoothing);
/**
 * Similar to #BKE_nurb_handle_calc but for curves and figures out the previous and next for us.
 */
void BKE_nurb_handle_calc_simple(struct Nurb *nu, struct BezTriple *bezt);
void BKE_nurb_handle_calc_simple_auto(struct Nurb *nu, struct BezTriple *bezt);

void BKE_nurb_handle_smooth_fcurve(struct BezTriple *bezt, int total, bool cyclic);

void BKE_nurb_handles_calc(struct Nurb *nu);
void BKE_nurb_handles_autocalc(struct Nurb *nu, uint8_t flag);

/**
 * Return a flag for the handles to treat as "selected":
 * `1 << 0`, `1 << 1`, `1 << 2` map to handles 1 2 & 3.
 */
short BKE_nurb_bezt_handle_test_calc_flag(const BezTriple *bezt,
                                          const eBezTriple_Flag__Alias sel_flag,
                                          const eNurbHandleTest_Mode handle_mode);

/**
 * Update selected handle types to ensure valid state, e.g. deduce "Auto" types to concrete ones.
 * Thereby \a sel_flag defines what qualifies as selected.
 * Use when something has changed handle positions.
 *
 * The caller needs to recalculate handles.
 *
 * \param sel_flag: The flag (bezt.f1/2/3) value to use to determine selection. Usually `SELECT`,
 * but may want to use a different one at times (if caller does not operate on * selection).
 * \param handle_mode: Interpret the selection base on modes in #eNurbHandleTest_Mode.
 */
void BKE_nurb_bezt_handle_test(struct BezTriple *bezt,
                               eBezTriple_Flag__Alias sel_flag,
                               const eNurbHandleTest_Mode handle_mode,
                               bool use_around_local);
void BKE_nurb_handles_test(struct Nurb *nu,
                           eNurbHandleTest_Mode handle_mode,
                           bool use_around_local);

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
/** \name Decimate Curve (`curve_decimate.cc`)
 *
 * Simplify curve data.
 * \{ */

unsigned int BKE_curve_decimate_bezt_array(struct BezTriple *bezt_array,
                                           unsigned int bezt_array_len,
                                           unsigned int resolu,
                                           bool is_cyclic,
                                           char flag_test,
                                           char flag_set,
                                           float error_sq_max,
                                           unsigned int error_target_len);

void BKE_curve_decimate_nurb(struct Nurb *nu,
                             unsigned int resolu,
                             float error_sq_max,
                             unsigned int error_target_len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deform 3D Coordinates by Curve (`curve_deform.cc`)
 * \{ */

void BKE_curve_deform_coords(const struct Object *ob_curve,
                             const struct Object *ob_target,
                             float (*vert_coords)[3],
                             int vert_coords_len,
                             const struct MDeformVert *dvert,
                             int defgrp_index,
                             short flag,
                             short defaxis);

void BKE_curve_deform_coords_with_editmesh(const struct Object *ob_curve,
                                           const struct Object *ob_target,
                                           float (*vert_coords)[3],
                                           int vert_coords_len,
                                           int defgrp_index,
                                           short flag,
                                           short defaxis,
                                           struct BMEditMesh *em_target);

/**
 * \param orco: Input vec and orco = local coord in curve space
 * orco is original not-animated or deformed reference point.
 *
 * The result written to `vec` and `r_mat`.
 */
void BKE_curve_deform_co(const struct Object *ob_curve,
                         const struct Object *ob_target,
                         const float orco[3],
                         float vec[3],
                         int no_rot_axis,
                         float r_mat[3][3]);

/** \} */

/* `curve_convert.cc` */

/* Create a new curve from the given object at its current state. This only works for curve and
 * text objects, otherwise NULL is returned.
 *
 * If apply_modifiers is true and the object is a curve one, then spline deform modifiers are
 * applied on the control points of the splines.
 */
struct Curve *BKE_curve_new_from_object(struct Object *object,
                                        struct Depsgraph *depsgraph,
                                        bool apply_modifiers);

#ifdef __cplusplus
}
#endif
