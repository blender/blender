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
 * The Original Code is Copyright (C) 2014, Blender Foundation
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_utils.c
 *  \ingroup edgpencil
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "gpencil_intern.h"

/* ******************************************************** */

/* Check if part of stroke occurs within last segment drawn by eraser */
bool gp_stroke_inside_circle(const int mval[2], const int UNUSED(mvalo[2]),
                             int rad, int x0, int y0, int x1, int y1)
{
	/* simple within-radius check for now */
	const float mval_fl[2]     = {mval[0], mval[1]};
	const float screen_co_a[2] = {x0, y0};
	const float screen_co_b[2] = {x1, y1};
	
	if (edge_inside_circle(mval_fl, rad, screen_co_a, screen_co_b)) {
		return true;
	}
	
	/* not inside */
	return false;
}

/* ******************************************************** */

/* Check whether given stroke can be edited given the supplied context */
// XXX: do we need additional flags for screenspace vs dataspace?
bool ED_gpencil_stroke_can_use_direct(const ScrArea *sa, const bGPDstroke *gps)
{
	/* sanity check */
	if (ELEM(NULL, sa, gps))
		return false;

	/* filter stroke types by flags + spacetype */
	if (gps->flag & GP_STROKE_3DSPACE) {
		/* 3D strokes - only in 3D view */
		return (sa->spacetype == SPACE_VIEW3D);
	}
	else if (gps->flag & GP_STROKE_2DIMAGE) {
		/* Special "image" strokes - only in Image Editor */
		return (sa->spacetype == SPACE_IMAGE);
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		/* 2D strokes (dataspace) - for any 2D view (i.e. everything other than 3D view) */
		return (sa->spacetype != SPACE_VIEW3D);
	}
	else {
		/* view aligned - anything goes */
		return true;
	}
}

/* Check whether given stroke can be edited in the current context */
bool ED_gpencil_stroke_can_use(const bContext *C, const bGPDstroke *gps)
{
	ScrArea *sa = CTX_wm_area(C);
	return ED_gpencil_stroke_can_use_direct(sa, gps);
}

/* ******************************************************** */

/* Init handling for space-conversion function (from passed-in parameters) */
void gp_point_conversion_init(bContext *C, GP_SpaceConversion *r_gsc)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	
	/* zero out the storage (just in case) */
	memset(r_gsc, 0, sizeof(GP_SpaceConversion));
	unit_m4(r_gsc->mat);
	
	/* store settings */
	r_gsc->sa = sa;
	r_gsc->ar = ar;
	r_gsc->v2d = &ar->v2d;
	
	/* init region-specific stuff */
	if (sa->spacetype == SPACE_VIEW3D) {
		wmWindow *win = CTX_wm_window(C);
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = (View3D *)CTX_wm_space_data(C);
		RegionView3D *rv3d = ar->regiondata;
		
		/* init 3d depth buffers */
		view3d_operator_needs_opengl(C);
		
		view3d_region_operator_needs_opengl(win, ar);
		ED_view3d_autodist_init(scene, ar, v3d, 0);
		
		/* for camera view set the subrect */
		if (rv3d->persp == RV3D_CAMOB) {
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &r_gsc->subrect_data, true); /* no shift */
			r_gsc->subrect = &r_gsc->subrect_data;
		}
	}
}


/* Convert Grease Pencil points to screen-space values
 * WARNING: This assumes that the caller has already checked whether the stroke in question can be drawn
 */
void gp_point_to_xy(GP_SpaceConversion *gsc, bGPDstroke *gps, bGPDspoint *pt,
                    int *r_x, int *r_y)
{
	ARegion *ar = gsc->ar;
	View2D *v2d = gsc->v2d;
	rctf *subrect = gsc->subrect;
	int xyval[2];
	
	/* sanity checks */
	BLI_assert(!(gps->flag & GP_STROKE_3DSPACE) || (gsc->sa->spacetype == SPACE_VIEW3D));
	BLI_assert(!(gps->flag & GP_STROKE_2DSPACE) || (gsc->sa->spacetype != SPACE_VIEW3D));
	
	
	if (gps->flag & GP_STROKE_3DSPACE) {
		if (ED_view3d_project_int_global(ar, &pt->x, xyval, V3D_PROJ_TEST_NOP) == V3D_PROJ_RET_OK) {
			*r_x = xyval[0];
			*r_y = xyval[1];
		}
		else {
			*r_x = V2D_IS_CLIPPED;
			*r_y = V2D_IS_CLIPPED;
		}
	}
	else if (gps->flag & GP_STROKE_2DSPACE) {
		float vec[3] = {pt->x, pt->y, 0.0f};
		mul_m4_v3(gsc->mat, vec);
		UI_view2d_view_to_region_clip(v2d, vec[0], vec[1], r_x, r_y);
	}
	else {
		if (subrect == NULL) {
			/* normal 3D view (or view space) */
			*r_x = (int)(pt->x / 100 * ar->winx);
			*r_y = (int)(pt->y / 100 * ar->winy);
		}
		else {
			/* camera view, use subrect */
			*r_x = (int)((pt->x / 100) * BLI_rctf_size_x(subrect)) + subrect->xmin;
			*r_y = (int)((pt->y / 100) * BLI_rctf_size_y(subrect)) + subrect->ymin;
		}
	}
}

/* ******************************************************** */
