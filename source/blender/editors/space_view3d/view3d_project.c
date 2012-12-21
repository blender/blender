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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_project.c
 *  \ingroup spview3d
 */

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLO_sys_types.h"  /* int64_t */

#include "BIF_gl.h"  /* bglMats */
#include "BIF_glutil.h"  /* bglMats */

#include "BLI_math_vector.h"

#include "ED_view3d.h"  /* own include */

#define BL_NEAR_CLIP 0.001

/* Non Clipping Projection Functions
 * ********************************* */

/**
 * \note use #ED_view3d_ob_project_mat_get to get the projection matrix
 */
void ED_view3d_project_float_v2_m4(const ARegion *ar, const float co[3], float r_co[2], float mat[4][4])
{
	float vec4[4];
	
	copy_v3_v3(vec4, co);
	vec4[3] = 1.0;
	/* r_co[0] = IS_CLIPPED; */ /* always overwritten */
	
	mul_m4_v4(mat, vec4);
	
	if (vec4[3] > FLT_EPSILON) {
		r_co[0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * vec4[0] / vec4[3];
		r_co[1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * vec4[1] / vec4[3];
	}
	else {
		zero_v2(r_co);
	}
}

/**
 * \note use #ED_view3d_ob_project_mat_get to get projecting mat
 */
void ED_view3d_project_float_v3_m4(ARegion *ar, const float vec[3], float r_co[3], float mat[4][4])
{
	float vec4[4];
	
	copy_v3_v3(vec4, vec);
	vec4[3] = 1.0;
	/* r_co[0] = IS_CLIPPED; */ /* always overwritten */
	
	mul_m4_v4(mat, vec4);
	
	if (vec4[3] > FLT_EPSILON) {
		r_co[0] = (float)(ar->winx / 2.0f) + (ar->winx / 2.0f) * vec4[0] / vec4[3];
		r_co[1] = (float)(ar->winy / 2.0f) + (ar->winy / 2.0f) * vec4[1] / vec4[3];
		r_co[2] = vec4[2] / vec4[3];
	}
	else {
		zero_v3(r_co);
	}
}


/* Clipping Projection Functions
 * ***************************** */

eV3DProjStatus ED_view3d_project_base(struct ARegion *ar, struct Base *base)
{
	eV3DProjStatus ret = ED_view3d_project_short_global(ar, base->object->obmat[3], &base->sx, V3D_PROJ_TEST_CLIP_DEFAULT);

	if (ret != V3D_PROJ_RET_OK) {
		base->sx = IS_CLIPPED;
		base->sy = 0;
	}

	return ret;
}

/* perspmat is typically...
 * - 'rv3d->perspmat',   is_local == FALSE
 * - 'rv3d->perspmatob', is_local == TRUE
 */
static eV3DProjStatus ed_view3d_project__internal(ARegion *ar,
                                                  float perspmat[4][4], const int is_local,  /* normally hidden */
                                                  const float co[3], float r_co[2], const eV3DProjTest flag)
{
	float vec4[4];

	/* check for bad flags */
	BLI_assert((flag & V3D_PROJ_TEST_ALL) == flag);

	if (flag & V3D_PROJ_TEST_CLIP_BB) {
		RegionView3D *rv3d = ar->regiondata;
		if (rv3d->rflag & RV3D_CLIPPING) {
			if (ED_view3d_clipping_test(rv3d, co, is_local)) {
				return V3D_PROJ_RET_CLIP_BB;
			}
		}
	}

	copy_v3_v3(vec4, co);
	vec4[3] = 1.0;
	mul_m4_v4(perspmat, vec4);

	if (vec4[3] > (float)BL_NEAR_CLIP) {
		const float fx = ((float)ar->winx / 2.0f) * (1.0f + vec4[0] / vec4[3]);
		if (((flag & V3D_PROJ_TEST_CLIP_WIN) == 0) || (fx > 0 && fx < ar->winx)) {
			const float fy = ((float)ar->winy / 2.0f) * (1.0f + vec4[1] / vec4[3]);
			if (((flag & V3D_PROJ_TEST_CLIP_WIN) == 0) || (fy > 0.0f && fy < (float)ar->winy)) {
				r_co[0] = floorf(fx);
				r_co[1] = floorf(fy);
			}
			else {
				return V3D_PROJ_RET_CLIP_WIN;
			}
		}
		else {
			return V3D_PROJ_RET_CLIP_WIN;
		}
	}
	else {
		return V3D_PROJ_RET_CLIP_NEAR;
	}

	return V3D_PROJ_RET_OK;
}

eV3DProjStatus ED_view3d_project_short_ex(ARegion *ar, float perspmat[4][4], const int is_local,
                                          const float co[3], short r_co[2], const eV3DProjTest flag)
{
	float tvec[2];
	eV3DProjStatus ret = ed_view3d_project__internal(ar, perspmat, is_local, co, tvec, flag);
	if (ret == V3D_PROJ_RET_OK) {
		if ((tvec[0] > -32700.0f && tvec[0] < 32700.0f) &&
		    (tvec[1] > -32700.0f && tvec[1] < 32700.0f))
		{
			r_co[0] = (short)floor(tvec[0]);
			r_co[1] = (short)floor(tvec[1]);
		}
		else {
			ret = V3D_PROJ_RET_OVERFLOW;
		}
	}
	return ret;
}

eV3DProjStatus ED_view3d_project_int_ex(ARegion *ar, float perspmat[4][4], const int is_local,
                                        const float co[3], int r_co[2], const eV3DProjTest flag)
{
	float tvec[2];
	eV3DProjStatus ret = ed_view3d_project__internal(ar, perspmat, is_local, co, tvec, flag);
	if (ret == V3D_PROJ_RET_OK) {
		if ((tvec[0] > -2140000000.0f && tvec[0] < 2140000000.0f) &&
		    (tvec[1] > -2140000000.0f && tvec[1] < 2140000000.0f))
		{
			r_co[0] = (int)floor(tvec[0]);
			r_co[1] = (int)floor(tvec[1]);
		}
		else {
			ret = V3D_PROJ_RET_OVERFLOW;
		}
	}
	return ret;
}

eV3DProjStatus ED_view3d_project_float_ex(ARegion *ar, float perspmat[4][4], const int is_local,
                                        const float co[3], float r_co[2], const eV3DProjTest flag)
{
	float tvec[2];
	eV3DProjStatus ret = ed_view3d_project__internal(ar, perspmat, is_local, co, tvec, flag);
	if (ret == V3D_PROJ_RET_OK) {
		if (finite(tvec[0]) &&
		    finite(tvec[1]))
		{
			copy_v2_v2(r_co, tvec);
		}
		else {
			ret = V3D_PROJ_RET_OVERFLOW;
		}
	}
	return ret;
}

/* --- short --- */
eV3DProjStatus ED_view3d_project_short_global(ARegion *ar, const float co[3], short r_co[2], const eV3DProjTest flag)
{
	RegionView3D *rv3d = ar->regiondata;
	return ED_view3d_project_short_ex(ar, rv3d->persmat, FALSE, co, r_co, flag);
}
/* object space, use ED_view3d_init_mats_rv3d before calling */
eV3DProjStatus ED_view3d_project_short_object(ARegion *ar, const float co[3], short r_co[2], const eV3DProjTest flag)
{
	RegionView3D *rv3d = ar->regiondata;
	return ED_view3d_project_short_ex(ar, rv3d->persmatob, TRUE, co, r_co, flag);
}

/* --- int --- */
eV3DProjStatus ED_view3d_project_int_global(ARegion *ar, const float co[3], int r_co[2], const eV3DProjTest flag)
{
	RegionView3D *rv3d = ar->regiondata;
	return ED_view3d_project_int_ex(ar, rv3d->persmat, FALSE, co, r_co, flag);
}
/* object space, use ED_view3d_init_mats_rv3d before calling */
eV3DProjStatus ED_view3d_project_int_object(ARegion *ar, const float co[3], int r_co[2], const eV3DProjTest flag)
{
	RegionView3D *rv3d = ar->regiondata;
	return ED_view3d_project_int_ex(ar, rv3d->persmatob, TRUE, co, r_co, flag);
}

/* --- float --- */
eV3DProjStatus ED_view3d_project_float_global(ARegion *ar, const float co[3], float r_co[2], const eV3DProjTest flag)
{
	RegionView3D *rv3d = ar->regiondata;
	return ED_view3d_project_float_ex(ar, rv3d->persmat, FALSE, co, r_co, flag);
}
/* object space, use ED_view3d_init_mats_rv3d before calling */
eV3DProjStatus ED_view3d_project_float_object(ARegion *ar, const float co[3], float r_co[2], const eV3DProjTest flag)
{
	RegionView3D *rv3d = ar->regiondata;
	return ED_view3d_project_float_ex(ar, rv3d->persmatob, TRUE, co, r_co, flag);
}



/* More Generic Window/Ray/Vector projection functions
 * *************************************************** */

/* odd function, need to document better */
int initgrabz(RegionView3D *rv3d, float x, float y, float z)
{
	int flip = FALSE;
	if (rv3d == NULL) return flip;
	rv3d->zfac = rv3d->persmat[0][3] * x + rv3d->persmat[1][3] * y + rv3d->persmat[2][3] * z + rv3d->persmat[3][3];
	if (rv3d->zfac < 0.0f)
		flip = TRUE;
	/* if x,y,z is exactly the viewport offset, zfac is 0 and we don't want that
	 * (accounting for near zero values)
	 */
	if (rv3d->zfac < 1.e-6f && rv3d->zfac > -1.e-6f) rv3d->zfac = 1.0f;

	/* Negative zfac means x, y, z was behind the camera (in perspective).
	 * This gives flipped directions, so revert back to ok default case.
	 */
	/* NOTE: I've changed this to flip zfac to be positive again for now so that GPencil draws ok
	 * Aligorith, 2009Aug31 */
	//if (rv3d->zfac < 0.0f) rv3d->zfac = 1.0f;
	if (rv3d->zfac < 0.0f) rv3d->zfac = -rv3d->zfac;

	return flip;
}

/**
 * Calculate a 3d viewpoint and direction vector from 2d window coordinates.
 * This ray_start is located at the viewpoint, ray_normal is the direction towards mval.
 * ray_start is clipped by the view near limit so points in front of it are always in view.
 * In orthographic view the resulting ray_normal will match the view vector.
 * \param ar The region (used for the window width and height).
 * \param v3d The 3d viewport (used for near clipping value).
 * \param mval The area relative 2d location (such as event->mval, converted into float[2]).
 * \param ray_start The world-space starting point of the segment.
 * \param ray_normal The normalized world-space direction of towards mval.
 */
void ED_view3d_win_to_ray(ARegion *ar, View3D *v3d, const float mval[2], float ray_start[3], float ray_normal[3])
{
	float ray_end[3];
	
	ED_view3d_win_to_segment(ar, v3d, mval, ray_start, ray_end);
	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	normalize_v3(ray_normal);
}

/**
 * Calculate a normalized 3d direction vector from the viewpoint towards a global location.
 * In orthographic view the resulting vector will match the view vector.
 * \param rv3d The region (used for the window width and height).
 * \param coord The world-space location.
 * \param vec The resulting normalized vector.
 */
void ED_view3d_global_to_vector(RegionView3D *rv3d, const float coord[3], float vec[3])
{
	if (rv3d->is_persp) {
		float p1[4], p2[4];

		copy_v3_v3(p1, coord);
		p1[3] = 1.0f;
		copy_v3_v3(p2, p1);
		p2[3] = 1.0f;
		mul_m4_v4(rv3d->viewmat, p2);

		mul_v3_fl(p2, 2.0f);

		mul_m4_v4(rv3d->viewinv, p2);

		sub_v3_v3v3(vec, p1, p2);
	}
	else {
		copy_v3_v3(vec, rv3d->viewinv[2]);
	}
	normalize_v3(vec);
}

/**
 * Calculate a 3d location from 2d window coordinates.
 * \param ar The region (used for the window width and height).
 * \param depth_pt The reference location used to calculate the Z depth.
 * \param mval The area relative location (such as event->mval converted to floats).
 * \param out The resulting world-space location.
 */
void ED_view3d_win_to_3d(ARegion *ar, const float depth_pt[3], const float mval[2], float out[3])
{
	RegionView3D *rv3d = ar->regiondata;
	
	float line_sta[3];
	float line_end[3];

	if (rv3d->is_persp) {
		float mousevec[3];
		copy_v3_v3(line_sta, rv3d->viewinv[3]);
		ED_view3d_win_to_vector(ar, mval, mousevec);
		add_v3_v3v3(line_end, line_sta, mousevec);

		if (isect_line_plane_v3(out, line_sta, line_end, depth_pt, rv3d->viewinv[2], TRUE) == 0) {
			/* highly unlikely to ever happen, mouse vec paralelle with view plane */
			zero_v3(out);
		}
	}
	else {
		const float dx = (2.0f * mval[0] / (float)ar->winx) - 1.0f;
		const float dy = (2.0f * mval[1] / (float)ar->winy) - 1.0f;
		line_sta[0] = (rv3d->persinv[0][0] * dx) + (rv3d->persinv[1][0] * dy) + rv3d->viewinv[3][0];
		line_sta[1] = (rv3d->persinv[0][1] * dx) + (rv3d->persinv[1][1] * dy) + rv3d->viewinv[3][1];
		line_sta[2] = (rv3d->persinv[0][2] * dx) + (rv3d->persinv[1][2] * dy) + rv3d->viewinv[3][2];

		add_v3_v3v3(line_end, line_sta, rv3d->viewinv[2]);
		closest_to_line_v3(out, depth_pt, line_sta, line_end);
	}
}

/**
 * Calculate a 3d difference vector from 2d window offset.
 * note that initgrabz() must be called first to determine
 * the depth used to calculate the delta.
 * \param ar The region (used for the window width and height).
 * \param mval The area relative 2d difference (such as event->mval[0] - other_x).
 * \param out The resulting world-space delta.
 */
void ED_view3d_win_to_delta(ARegion *ar, const float mval[2], float out[3])
{
	RegionView3D *rv3d = ar->regiondata;
	float dx, dy;
	
	dx = 2.0f * mval[0] * rv3d->zfac / ar->winx;
	dy = 2.0f * mval[1] * rv3d->zfac / ar->winy;
	
	out[0] = (rv3d->persinv[0][0] * dx + rv3d->persinv[1][0] * dy);
	out[1] = (rv3d->persinv[0][1] * dx + rv3d->persinv[1][1] * dy);
	out[2] = (rv3d->persinv[0][2] * dx + rv3d->persinv[1][2] * dy);
}

/**
 * Calculate a 3d direction vector from 2d window coordinates.
 * This direction vector starts and the view in the direction of the 2d window coordinates.
 * In orthographic view all window coordinates yield the same vector.
 *
 * \note doesn't rely on initgrabz
 * for perspective view, get the vector direction to
 * the mouse cursor as a normalized vector.
 *
 * \param ar The region (used for the window width and height).
 * \param mval The area relative 2d location (such as event->mval converted to floats).
 * \param out The resulting normalized world-space direction vector.
 */
void ED_view3d_win_to_vector(ARegion *ar, const float mval[2], float out[3])
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d->is_persp) {
		out[0] = 2.0f * (mval[0] / ar->winx) - 1.0f;
		out[1] = 2.0f * (mval[1] / ar->winy) - 1.0f;
		out[2] = -0.5f;
		mul_project_m4_v3(rv3d->persinv, out);
		sub_v3_v3(out, rv3d->viewinv[3]);
	}
	else {
		copy_v3_v3(out, rv3d->viewinv[2]);
	}
	normalize_v3(out);
}

void ED_view3d_win_to_segment(ARegion *ar, View3D *v3d, const float mval[2], float ray_start[3], float ray_end[3])
{
	RegionView3D *rv3d = ar->regiondata;

	if (rv3d->is_persp) {
		float vec[3];
		ED_view3d_win_to_vector(ar, mval, vec);

		copy_v3_v3(ray_start, rv3d->viewinv[3]);
		madd_v3_v3v3fl(ray_start, rv3d->viewinv[3], vec, v3d->near);
		madd_v3_v3v3fl(ray_end, rv3d->viewinv[3], vec, v3d->far);
	}
	else {
		float vec[4];
		vec[0] = 2.0f * mval[0] / ar->winx - 1;
		vec[1] = 2.0f * mval[1] / ar->winy - 1;
		vec[2] = 0.0f;
		vec[3] = 1.0f;

		mul_m4_v4(rv3d->persinv, vec);

		madd_v3_v3v3fl(ray_start, vec, rv3d->viewinv[2],  1000.0f);
		madd_v3_v3v3fl(ray_end, vec, rv3d->viewinv[2], -1000.0f);
	}
}

/**
 * Calculate a 3d segment from 2d window coordinates.
 * This ray_start is located at the viewpoint, ray_end is a far point.
 * ray_start and ray_end are clipped by the view near and far limits
 * so points along this line are always in view.
 * In orthographic view all resulting segments will be parallel.
 * \param ar The region (used for the window width and height).
 * \param v3d The 3d viewport (used for near and far clipping range).
 * \param mval The area relative 2d location (such as event->mval, converted into float[2]).
 * \param ray_start The world-space starting point of the segment.
 * \param ray_end The world-space end point of the segment.
 * \return success, FALSE if the segment is totally clipped.
 */
int ED_view3d_win_to_segment_clip(ARegion *ar, View3D *v3d, const float mval[2], float ray_start[3], float ray_end[3])
{
	RegionView3D *rv3d = ar->regiondata;
	ED_view3d_win_to_segment(ar, v3d, mval, ray_start, ray_end);

	/* clipping */
	if (rv3d->rflag & RV3D_CLIPPING) {
		/* if the ray is totally clipped,
		 * restore the original values but return FALSE
		 * caller can choose what to do */
		float tray_start[3] = {UNPACK3(ray_start)};
		float tray_end[3]   = {UNPACK3(ray_end)};
		int a;
		for (a = 0; a < 4; a++) {
			if (clip_line_plane(tray_start, tray_end, rv3d->clip[a]) == FALSE) {
				return FALSE;
			}
		}

		/* copy in clipped values */
		copy_v3_v3(ray_start, tray_start);
		copy_v3_v3(ray_end, tray_end);
	}

	return TRUE;
}


/* Utility functions for projection
 * ******************************** */

void ED_view3d_ob_project_mat_get(RegionView3D *rv3d, Object *ob, float pmat[4][4])
{
	float vmat[4][4];

	mult_m4_m4m4(vmat, rv3d->viewmat, ob->obmat);
	mult_m4_m4m4(pmat, rv3d->winmat, vmat);
}

/**
 * Uses window coordinates (x,y) and depth component z to find a point in
 * modelspace */
void ED_view3d_unproject(bglMats *mats, float out[3], const float x, const float y, const float z)
{
	double ux, uy, uz;

	gluUnProject(x, y, z, mats->modelview, mats->projection,
	             (GLint *)mats->viewport, &ux, &uy, &uz);

	out[0] = ux;
	out[1] = uy;
	out[2] = uz;
}
