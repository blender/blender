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
struct ListBase;
struct Nurb;
struct Object;
struct Scene;

#define KNOTSU(nu)	    ( (nu)->orderu+ (nu)->pntsu+ (((nu)->flagu & CU_NURB_CYCLIC) ? ((nu)->orderu-1) : 0) )
#define KNOTSV(nu)	    ( (nu)->orderv+ (nu)->pntsv+ (((nu)->flagv & CU_NURB_CYCLIC) ? ((nu)->orderv-1) : 0) )

/* Non cyclic nurbs have 1 less segment */
#define SEGMENTSU(nu)	    ( ((nu)->flagu & CU_NURB_CYCLIC) ? (nu)->pntsu : (nu)->pntsu-1 )
#define SEGMENTSV(nu)	    ( ((nu)->flagv & CU_NURB_CYCLIC) ? (nu)->pntsv : (nu)->pntsv-1 )

#define CU_DO_TILT(cu, nu) (((nu->flag & CU_2D) && (cu->flag & CU_3D)==0) ? 0 : 1)
#define CU_DO_RADIUS(cu, nu) ((CU_DO_TILT(cu, nu) || ((cu)->flag & CU_PATH_RADIUS) || (cu)->bevobj || (cu)->ext1!=0.0f || (cu)->ext2!=0.0f) ? 1:0)

/* ** Curve ** */
void BKE_curve_unlink(struct Curve *cu);
void BKE_curve_free(struct Curve *cu);
void BKE_curve_editfont_free(struct Curve *cu);
struct Curve *BKE_curve_add(const char *name, int type);
struct Curve *BKE_curve_copy(struct Curve *cu);
void BKE_curve_make_local(struct Curve *cu);
short BKE_curve_type_get(struct Curve *cu);
void BKE_curve_type_test(struct Object *ob);
void BKE_curve_curve_dimension_update(struct Curve *cu);
void BKE_curve_tex_space_calc(struct Curve *cu);

int BKE_curve_minmax(struct Curve *cu, float min[3], float max[3]);
int BKE_curve_center_median(struct Curve *cu, float cent[3]);
int BKE_curve_center_bounds(struct Curve *cu, float cent[3]);
void BKE_curve_translate(struct Curve *cu, float offset[3], int do_keys);
void BKE_curve_delete_material_index(struct Curve *cu, int index);

ListBase *BKE_curve_nurbs_get(struct Curve *cu);

float (*BKE_curve_vertexCos_get(struct Curve *cu, struct ListBase *lb, int *numVerts_r))[3];
void BK_curve_vertexCos_apply(struct Curve *cu, struct ListBase *lb, float (*vertexCos)[3]);

float (*BKE_curve_keyVertexCos_get(struct Curve *cu, struct ListBase *lb, float *key))[3];
void BKE_curve_keyVertexTilts_apply(struct Curve *cu, struct ListBase *lb, float *key);

void BKE_curve_editNurb_keyIndex_free(struct EditNurb *editnurb);
void BKE_curve_editNurb_free(struct Curve *cu);
struct ListBase *BKE_curve_editNurbs_get(struct Curve *cu);

float *BKE_curve_make_orco(struct Scene *scene, struct Object *ob);
float *BKE_curve_surf_make_orco(struct Object *ob);

void BKE_curve_bevelList_make(struct Object *ob);
void BKE_curve_bevel_make(struct Scene *scene, struct Object *ob,  struct ListBase *disp, int forRender);

void BKE_curve_forward_diff_bezier(float q0, float q1, float q2, float q3, float *p, int it, int stride);

/* ** Nurbs ** */

int BKE_nurbList_verts_count(struct ListBase *nurb);
int BKE_nurbList_verts_count_without_handles(struct ListBase *nurb);

void BKE_nurbList_free(struct ListBase *lb);
void BKE_nurbList_duplicate(struct ListBase *lb1,  struct ListBase *lb2);
void BKE_nurbList_handles_set(struct ListBase *editnurb, short code);

void BKE_nurbList_handles_autocalc(ListBase *editnurb, int flag);

void BKE_nurb_free(struct Nurb *nu);
struct Nurb *BKE_nurb_duplicate(struct Nurb *nu);

void BKE_nurb_test2D(struct Nurb *nu);
void BKE_nurb_minmax(struct Nurb *nu, float *min, float *max);

void BKE_nurb_makeFaces(struct Nurb *nu, float *coord_array, int rowstride, int resolu, int resolv);
void BKE_nurb_makeCurve(struct Nurb *nu, float *coord_array, float *tilt_array, float *radius_array, float *weight_array, int resolu, int stride);

void BKE_nurb_knot_calc_u(struct Nurb *nu);
void BKE_nurb_knot_calc_v(struct Nurb *nu);

/* nurb checks if they can be drawn, also clamp order func */
int BKE_nurb_check_valid_u(struct Nurb *nu);
int BKE_nurb_check_valid_v(struct Nurb *nu);

int BKE_nurb_order_clamp_u(struct Nurb *nu);
int BKE_nurb_order_clamp_v(struct Nurb *nu);

void BKE_nurb_direction_switch(struct Nurb *nu);

void BKE_nurb_points_add(struct Nurb *nu, int number);
void BKE_nurb_bezierPoints_add(struct Nurb *nu, int number);

void BKE_nurb_handle_calc(struct BezTriple *bezt, struct BezTriple *prev,  struct BezTriple *next, int mode);

void BKE_nurb_handles_calc(struct Nurb *nu);
void BKE_nurb_handles_autocalc(struct Nurb *nu, int flag);
void BKE_nurb_handles_test(struct Nurb *nu);

#endif
