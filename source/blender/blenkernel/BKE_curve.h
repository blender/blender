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
#ifndef __BKE_CURVE_H__
#define __BKE_CURVE_H__

/** \file BKE_curve.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

struct BevList;
struct BezTriple;
struct Curve;
struct EditNurb;
struct ListBase;
struct Main;
struct Nurb;
struct Object;
struct Scene;
struct Path;
struct TextBox;
struct rctf;

typedef struct CurveCache {
	ListBase disp;
	ListBase bev;
	struct Path *path;
} CurveCache;

#define KNOTSU(nu)      ( (nu)->orderu + (nu)->pntsu + (((nu)->flagu & CU_NURB_CYCLIC) ? ((nu)->orderu - 1) : 0) )
#define KNOTSV(nu)      ( (nu)->orderv + (nu)->pntsv + (((nu)->flagv & CU_NURB_CYCLIC) ? ((nu)->orderv - 1) : 0) )

/* Non cyclic nurbs have 1 less segment */
#define SEGMENTSU(nu)       ( ((nu)->flagu & CU_NURB_CYCLIC) ? (nu)->pntsu : (nu)->pntsu - 1)
#define SEGMENTSV(nu)       ( ((nu)->flagv & CU_NURB_CYCLIC) ? (nu)->pntsv : (nu)->pntsv - 1)

#define CU_DO_TILT(cu, nu) (((nu->flag & CU_2D) && (cu->flag & CU_3D) == 0) ? 0 : 1)
#define CU_DO_RADIUS(cu, nu) ((CU_DO_TILT(cu, nu) || ((cu)->flag & CU_PATH_RADIUS) || (cu)->bevobj || (cu)->ext1 != 0.0f || (cu)->ext2 != 0.0f) ? 1 : 0)

/* not 3d and not unfilled */
#define CU_DO_2DFILL(cu)  ((((cu)->flag & CU_3D) == 0) && (((cu)->flag & (CU_FRONT | CU_BACK)) != 0))

/* ** Curve ** */
void BKE_curve_unlink(struct Curve *cu);
void BKE_curve_free(struct Curve *cu);
void BKE_curve_editfont_free(struct Curve *cu);
struct Curve *BKE_curve_add(struct Main *bmain, const char *name, int type);
struct Curve *BKE_curve_copy(struct Curve *cu);
void BKE_curve_make_local(struct Curve *cu);
short BKE_curve_type_get(struct Curve *cu);
void BKE_curve_type_test(struct Object *ob);
void BKE_curve_curve_dimension_update(struct Curve *cu);

void BKE_curve_boundbox_calc(struct Curve *cu, float r_loc[3], float r_size[3]);
struct BoundBox *BKE_curve_boundbox_get(struct Object *ob);
void BKE_curve_texspace_calc(struct Curve *cu);
void BKE_curve_texspace_get(struct Curve *cu, float r_loc[3], float r_rot[3], float r_size[3]);

bool BKE_curve_minmax(struct Curve *cu, bool use_radius, float min[3], float max[3]);
bool BKE_curve_center_median(struct Curve *cu, float cent[3]);
bool BKE_curve_center_bounds(struct Curve *cu, float cent[3]);
void BKE_curve_translate(struct Curve *cu, float offset[3], const bool do_keys);
void BKE_curve_material_index_remove(struct Curve *cu, int index);
void BKE_curve_material_index_clear(struct Curve *cu);

ListBase    *BKE_curve_nurbs_get(struct Curve *cu);

void         BKE_curve_nurb_active_set(struct Curve *cu, struct Nurb *nu);
struct Nurb *BKE_curve_nurb_active_get(struct Curve *cu);
void        *BKE_curve_vert_active_get(struct Curve *cu);
void         BKE_curve_nurb_vert_active_set(struct Curve *cu, struct Nurb *nu,    void *vert);
bool         BKE_curve_nurb_vert_active_get(struct Curve *cu, struct Nurb **r_nu, void **r_vert);
void         BKE_curve_nurb_vert_active_validate(struct Curve *cu);

float (*BKE_curve_nurbs_vertexCos_get(struct ListBase *lb, int *r_numVerts))[3];
void BK_curve_nurbs_vertexCos_apply(struct ListBase *lb, float (*vertexCos)[3]);

float (*BKE_curve_nurbs_keyVertexCos_get(struct ListBase *lb, float *key))[3];
void BKE_curve_nurbs_keyVertexTilts_apply(struct ListBase *lb, float *key);

void BKE_curve_editNurb_keyIndex_free(struct EditNurb *editnurb);
void BKE_curve_editNurb_free(struct Curve *cu);
struct ListBase *BKE_curve_editNurbs_get(struct Curve *cu);

float *BKE_curve_make_orco(struct Scene *scene, struct Object *ob, int *r_numVerts);
float *BKE_curve_surf_make_orco(struct Object *ob);

void BKE_curve_bevelList_make(struct Object *ob, struct ListBase *nurbs, bool for_render);
void BKE_curve_bevel_make(struct Scene *scene, struct Object *ob,  struct ListBase *disp,
                          const bool for_render, const bool use_render_resolution);

void BKE_curve_forward_diff_bezier(float q0, float q1, float q2, float q3, float *p, int it, int stride);

void BKE_curve_rect_from_textbox(const struct Curve *cu, const struct TextBox *tb, struct rctf *r_rect);

/* ** Nurbs ** */

bool BKE_nurbList_index_get_co(struct ListBase *editnurb, const int index, float r_co[3]);

int BKE_nurbList_verts_count(struct ListBase *nurb);
int BKE_nurbList_verts_count_without_handles(struct ListBase *nurb);

void BKE_nurbList_free(struct ListBase *lb);
void BKE_nurbList_duplicate(struct ListBase *lb1,  struct ListBase *lb2);
void BKE_nurbList_handles_set(struct ListBase *editnurb, const char code);
void BKE_nurbList_handles_recalculate(struct ListBase *editnurb, const bool calc_length, const char flag);

void BKE_nurbList_handles_autocalc(ListBase *editnurb, int flag);
void BKE_nurbList_flag_set(ListBase *editnurb, short flag);

void BKE_nurb_free(struct Nurb *nu);
struct Nurb *BKE_nurb_duplicate(struct Nurb *nu);
struct Nurb *BKE_nurb_copy(struct Nurb *src, int pntsu, int pntsv);

void BKE_nurb_test2D(struct Nurb *nu);
void BKE_nurb_minmax(struct Nurb *nu, bool use_radius, float min[3], float max[3]);

void BKE_nurb_makeFaces(struct Nurb *nu, float *coord_array, int rowstride, int resolu, int resolv);
void BKE_nurb_makeCurve(struct Nurb *nu, float *coord_array, float *tilt_array, float *radius_array, float *weight_array, int resolu, int stride);

void BKE_nurb_knot_calc_u(struct Nurb *nu);
void BKE_nurb_knot_calc_v(struct Nurb *nu);

/* nurb checks if they can be drawn, also clamp order func */
bool BKE_nurb_check_valid_u(struct Nurb *nu);
bool BKE_nurb_check_valid_v(struct Nurb *nu);
bool BKE_nurb_check_valid_uv(struct Nurb *nu);

bool BKE_nurb_order_clamp_u(struct Nurb *nu);
bool BKE_nurb_order_clamp_v(struct Nurb *nu);

void BKE_nurb_direction_switch(struct Nurb *nu);
bool BKE_nurb_type_convert(struct Nurb *nu, const short type, const bool use_handles);

void BKE_nurb_points_add(struct Nurb *nu, int number);
void BKE_nurb_bezierPoints_add(struct Nurb *nu, int number);

struct BezTriple *BKE_nurb_bezt_get_next(struct Nurb *nu, struct BezTriple *bezt);
struct BezTriple *BKE_nurb_bezt_get_prev(struct Nurb *nu, struct BezTriple *bezt);
struct BPoint    *BKE_nurb_bpoint_get_next(struct Nurb *nu, struct BPoint *bp);
struct BPoint    *BKE_nurb_bpoint_get_prev(struct Nurb *nu, struct BPoint *bp);

void BKE_nurb_bezt_calc_normal(struct Nurb *nu, struct BezTriple *bezt, float r_normal[3]);
void BKE_nurb_bezt_calc_plane(struct Nurb *nu, struct BezTriple *bezt, float r_plane[3]);

void BKE_nurb_handle_calc(struct BezTriple *bezt, struct BezTriple *prev,  struct BezTriple *next,
                          const bool is_fcurve);
void BKE_nurb_handle_calc_simple(struct Nurb *nu, struct BezTriple *bezt);

void BKE_nurb_handles_calc(struct Nurb *nu);
void BKE_nurb_handles_autocalc(struct Nurb *nu, int flag);
void BKE_nurb_bezt_handle_test(struct BezTriple *bezt, const bool use_handle);
void BKE_nurb_handles_test(struct Nurb *nu, const bool use_handles);

#endif  /* __BKE_CURVE_H__ */
