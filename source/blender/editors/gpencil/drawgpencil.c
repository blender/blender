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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/drawgpencil.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_polyfill_2d.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"

#include "WM_api.h"

#include "BIF_glutil.h"

#include "GPU_immediate.h"
#include "GPU_draw.h"
#include "GPU_state.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_space_api.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"

/* ************************************************** */
/* GREASE PENCIL DRAWING */

/* ----- General Defines ------ */
/* flags for sflag */
typedef enum eDrawStrokeFlags {
	GP_DRAWDATA_NOSTATUS    = (1 << 0),   /* don't draw status info */
	GP_DRAWDATA_ONLY3D      = (1 << 1),   /* only draw 3d-strokes */
	GP_DRAWDATA_ONLYV2D     = (1 << 2),   /* only draw 'canvas' strokes */
	GP_DRAWDATA_ONLYI2D     = (1 << 3),   /* only draw 'image' strokes */
	GP_DRAWDATA_IEDITHACK   = (1 << 4),   /* special hack for drawing strokes in Image Editor (weird coordinates) */
	GP_DRAWDATA_NO_XRAY     = (1 << 5),   /* don't draw xray in 3D view (which is default) */
	GP_DRAWDATA_NO_ONIONS   = (1 << 6),	  /* no onionskins should be drawn (for animation playback) */
	GP_DRAWDATA_VOLUMETRIC	= (1 << 7),   /* draw strokes as "volumetric" circular billboards */
	GP_DRAWDATA_FILL        = (1 << 8),   /* fill insides/bounded-regions of strokes */
	GP_DRAWDATA_HQ_FILL     = (1 << 9)    /* Use high quality fill */
} eDrawStrokeFlags;



/* thickness above which we should use special drawing */
#if 0
#define GP_DRAWTHICKNESS_SPECIAL    3
#endif

/* conversion utility (float --> normalized unsigned byte) */
#define F2UB(x) (unsigned char)(255.0f * x)

/* ----- Tool Buffer Drawing ------ */
/* helper functions to set color of buffer point */

static void gp_set_tpoint_varying_color(const tGPspoint *pt, const float ink[4], unsigned attrib_id)
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	immAttrib4ub(attrib_id, F2UB(ink[0]), F2UB(ink[1]), F2UB(ink[2]), F2UB(alpha));
}

static void gp_set_point_uniform_color(const bGPDspoint *pt, const float ink[4])
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	immUniformColor3fvAlpha(ink, alpha);
}

static void gp_set_point_varying_color(const bGPDspoint *pt, const float ink[4], unsigned attrib_id)
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	immAttrib4ub(attrib_id, F2UB(ink[0]), F2UB(ink[1]), F2UB(ink[2]), F2UB(alpha));
}

/* draw fills for buffer stroke */
static void gp_draw_stroke_buffer_fill(const tGPspoint *points, int totpoints, float ink[4])
{
	if (totpoints < 3) {
		return;
	}
	int tot_triangles = totpoints - 2;
	/* allocate memory for temporary areas */
	unsigned int(*tmp_triangles)[3] = MEM_mallocN(sizeof(*tmp_triangles) * tot_triangles, "GP Stroke buffer temp triangulation");
	float(*points2d)[2] = MEM_mallocN(sizeof(*points2d) * totpoints, "GP Stroke buffer temp 2d points");

	/* Convert points to array and triangulate
	 * Here a cache is not used because while drawing the information changes all the time, so the cache
	 * would be recalculated constantly, so it is better to do direct calculation for each function call
	 */
	for (int i = 0; i < totpoints; i++) {
		const tGPspoint *pt = &points[i];
		points2d[i][0] = pt->x;
		points2d[i][1] = pt->y;
	}
	BLI_polyfill_calc((const float(*)[2])points2d, (unsigned int)totpoints, 0, (unsigned int(*)[3])tmp_triangles);

	/* draw triangulation data */
	if (tot_triangles > 0) {
		GPUVertFormat *format = immVertexFormat();
		uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
		uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

		immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

		/* Draw all triangles for filling the polygon */
		immBegin(GPU_PRIM_TRIS, tot_triangles * 3);
		/* TODO: use batch instead of immediate mode, to share vertices */

		const tGPspoint *pt;
		for (int i = 0; i < tot_triangles; i++) {
			/* vertex 1 */
			pt = &points[tmp_triangles[i][0]];
			gp_set_tpoint_varying_color(pt, ink, color);
			immVertex2iv(pos, &pt->x);
			/* vertex 2 */
			pt = &points[tmp_triangles[i][1]];
			gp_set_tpoint_varying_color(pt, ink, color);
			immVertex2iv(pos, &pt->x);
			/* vertex 3 */
			pt = &points[tmp_triangles[i][2]];
			gp_set_tpoint_varying_color(pt, ink, color);
			immVertex2iv(pos, &pt->x);
		}

		immEnd();
		immUnbindProgram();
	}

	/* clear memory */
	if (tmp_triangles) {
		MEM_freeN(tmp_triangles);
	}
	if (points2d) {
		MEM_freeN(points2d);
	}
}

/* draw stroke defined in buffer (simple ogl lines/points for now, as dotted lines) */
static void gp_draw_stroke_buffer(const tGPspoint *points, int totpoints, short thickness,
                                  short dflag, short sflag, float ink[4], float fill_ink[4])
{
	int draw_points = 0;

	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;

	/* check if buffer can be drawn */
	if (dflag & (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_ONLYV2D))
		return;

	if (sflag & GP_STROKE_ERASER) {
		/* don't draw stroke at all! */
		return;
	}

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
	uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	const tGPspoint *pt = points;

	if (totpoints == 1) {
		/* if drawing a single point, draw it larger */
		GPU_point_size((float)(thickness + 2) * points->pressure);
		immBindBuiltinProgram(GPU_SHADER_3D_POINT_FIXED_SIZE_VARYING_COLOR);
		immBegin(GPU_PRIM_POINTS, 1);
		gp_set_tpoint_varying_color(pt, ink, color);
		immVertex2iv(pos, &pt->x);
	}
	else {
		float oldpressure = points[0].pressure;

		/* draw stroke curve */
		GPU_line_width(max_ff(oldpressure * thickness, 1.0));
		immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);
		immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints);

		/* TODO: implement this with a geometry shader to draw one continuous tapered stroke */

		for (int i = 0; i < totpoints; i++, pt++) {
			/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
			 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
			 */
			if (fabsf(pt->pressure - oldpressure) > 0.2f) {
				/* need to have 2 points to avoid immEnd assert error */
				if (draw_points < 2) {
					gp_set_tpoint_varying_color(pt - 1, ink, color);
					immVertex2iv(pos, &(pt - 1)->x);
				}

				immEnd();
				draw_points = 0;

				GPU_line_width(max_ff(pt->pressure * thickness, 1.0f));
				immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints - i + 1);

				/* need to roll-back one point to ensure that there are no gaps in the stroke */
				if (i != 0) {
					gp_set_tpoint_varying_color(pt - 1, ink, color);
					immVertex2iv(pos, &(pt - 1)->x);
					++draw_points;
				}

				oldpressure = pt->pressure; /* reset our threshold */
			}

			/* now the point we want */
			gp_set_tpoint_varying_color(pt, ink, color);
			immVertex2iv(pos, &pt->x);
			++draw_points;
		}
		/* need to have 2 points to avoid immEnd assert error */
		if (draw_points < 2) {
			gp_set_tpoint_varying_color(pt - 1, ink, color);
			immVertex2iv(pos, &(pt - 1)->x);
		}
	}

	immEnd();
	immUnbindProgram();

	// draw fill
	if (fill_ink[3] > GPENCIL_ALPHA_OPACITY_THRESH) {
		gp_draw_stroke_buffer_fill(points, totpoints, fill_ink);
	}
}

/* --------- 2D Stroke Drawing Helpers --------- */
/* change in parameter list */
static void gp_calc_2d_stroke_fxy(const float pt[3], short sflag, int offsx, int offsy, int winx, int winy, float r_co[2])
{
	if (sflag & GP_STROKE_2DSPACE) {
		r_co[0] = pt[0];
		r_co[1] = pt[1];
	}
	else if (sflag & GP_STROKE_2DIMAGE) {
		const float x = (float)((pt[0] * winx) + offsx);
		const float y = (float)((pt[1] * winy) + offsy);

		r_co[0] = x;
		r_co[1] = y;
	}
	else {
		const float x = (float)(pt[0] / 100 * winx) + offsx;
		const float y = (float)(pt[1] / 100 * winy) + offsy;

		r_co[0] = x;
		r_co[1] = y;
	}
}
/* ----------- Volumetric Strokes --------------- */

/* draw a 2D buffer stroke in "volumetric" style
 * NOTE: the stroke buffer doesn't have any coordinate offsets/transforms
 */
static void gp_draw_stroke_volumetric_buffer(const tGPspoint *points, int totpoints, short thickness,
                                             short dflag, const float ink[4])
{
	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;

	/* check if buffer can be drawn */
	if (dflag & (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_ONLYV2D))
		return;

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
	GPU_enable_program_point_size();
	immBegin(GPU_PRIM_POINTS, totpoints);

	const tGPspoint *pt = points;
	for (int i = 0; i < totpoints; i++, pt++) {
		gp_set_tpoint_varying_color(pt, ink, color);
		immAttrib1f(size, pt->pressure * thickness); /* TODO: scale based on view transform (zoom level) */
		immVertex2f(pos, pt->x, pt->y);
	}

	immEnd();
	immUnbindProgram();
	GPU_disable_program_point_size();
}

/* draw a 2D strokes in "volumetric" style */
static void gp_draw_stroke_volumetric_2d(const bGPDspoint *points, int totpoints, short thickness,
                                         short UNUSED(dflag), short sflag,
                                         int offsx, int offsy, int winx, int winy,
                                         const float diff_mat[4][4], const float ink[4])
{
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
	uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
	GPU_enable_program_point_size();
	immBegin(GPU_PRIM_POINTS, totpoints);

	const bGPDspoint *pt = points;
	for (int i = 0; i < totpoints; i++, pt++) {
		/* transform position to 2D */
		float co[2];
		float fpt[3];

		mul_v3_m4v3(fpt, diff_mat, &pt->x);
		gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);

		gp_set_point_varying_color(pt, ink, color);
		immAttrib1f(size, pt->pressure * thickness); /* TODO: scale based on view transform */
		immVertex2f(pos, co[0], co[1]);
	}

	immEnd();
	immUnbindProgram();
	GPU_disable_program_point_size();
}

/* draw a 3D stroke in "volumetric" style */
static void gp_draw_stroke_volumetric_3d(
        const bGPDspoint *points, int totpoints, short thickness,
        const float ink[4])
{
	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
	uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
	GPU_enable_program_point_size();
	immBegin(GPU_PRIM_POINTS, totpoints);

	const bGPDspoint *pt = points;
	for (int i = 0; i < totpoints && pt; i++, pt++) {
		gp_set_point_varying_color(pt, ink, color);
		immAttrib1f(size, pt->pressure * thickness); /* TODO: scale based on view transform */
		immVertex3fv(pos, &pt->x);                   /* we can adjust size in vertex shader based on view/projection! */
	}

	immEnd();
	immUnbindProgram();
	GPU_disable_program_point_size();
}


/* --------------- Stroke Fills ----------------- */

/* Get points of stroke always flat to view not affected by camera view or view position */
static void gp_stroke_2d_flat(const bGPDspoint *points, int totpoints, float(*points2d)[2], int *r_direction)
{
	const bGPDspoint *pt0 = &points[0];
	const bGPDspoint *pt1 = &points[1];
	const bGPDspoint *pt3 = &points[(int)(totpoints * 0.75)];

	float locx[3];
	float locy[3];
	float loc3[3];
	float normal[3];

	/* local X axis (p0 -> p1) */
	sub_v3_v3v3(locx, &pt1->x, &pt0->x);

	/* point vector at 3/4 */
	sub_v3_v3v3(loc3, &pt3->x, &pt0->x);

	/* vector orthogonal to polygon plane */
	cross_v3_v3v3(normal, locx, loc3);

	/* local Y axis (cross to normal/x axis) */
	cross_v3_v3v3(locy, normal, locx);

	/* Normalize vectors */
	normalize_v3(locx);
	normalize_v3(locy);

	/* Get all points in local space */
	for (int i = 0; i < totpoints; i++) {
		const bGPDspoint *pt = &points[i];
		float loc[3];

		/* Get local space using first point as origin */
		sub_v3_v3v3(loc, &pt->x, &pt0->x);

		points2d[i][0] = dot_v3v3(loc, locx);
		points2d[i][1] = dot_v3v3(loc, locy);
	}

	/* Concave (-1), Convex (1), or Autodetect (0)? */
	*r_direction = (int)locy[2];
}


/* Triangulate stroke for high quality fill (this is done only if cache is null or stroke was modified) */
static void gp_triangulate_stroke_fill(bGPDstroke *gps)
{
	BLI_assert(gps->totpoints >= 3);

	/* allocate memory for temporary areas */
	gps->tot_triangles = gps->totpoints - 2;
	unsigned int (*tmp_triangles)[3] = MEM_mallocN(sizeof(*tmp_triangles) * gps->tot_triangles, "GP Stroke temp triangulation");
	float (*points2d)[2] = MEM_mallocN(sizeof(*points2d) * gps->totpoints, "GP Stroke temp 2d points");

	int direction = 0;

	/* convert to 2d and triangulate */
	gp_stroke_2d_flat(gps->points, gps->totpoints, points2d, &direction);
	BLI_polyfill_calc(points2d, (unsigned int)gps->totpoints, direction, tmp_triangles);

	/* Number of triangles */
	gps->tot_triangles = gps->totpoints - 2;
	/* save triangulation data in stroke cache */
	if (gps->tot_triangles > 0) {
		if (gps->triangles == NULL) {
			gps->triangles = MEM_callocN(sizeof(*gps->triangles) * gps->tot_triangles, "GP Stroke triangulation");
		}
		else {
			gps->triangles = MEM_recallocN(gps->triangles, sizeof(*gps->triangles) * gps->tot_triangles);
		}

		for (int i = 0; i < gps->tot_triangles; i++) {
			memcpy(gps->triangles[i].verts, tmp_triangles[i], sizeof(uint[3]));
		}
	}
	else {
		/* No triangles needed - Free anything allocated previously */
		if (gps->triangles)
			MEM_freeN(gps->triangles);

		gps->triangles = NULL;
	}

	/* disable recalculation flag */
	if (gps->flag & GP_STROKE_RECALC_CACHES) {
		gps->flag &= ~GP_STROKE_RECALC_CACHES;
	}

	/* clear memory */
	if (tmp_triangles) MEM_freeN(tmp_triangles);
	if (points2d) MEM_freeN(points2d);
}


/* draw fills for shapes */
static void gp_draw_stroke_fill(
        bGPdata *gpd, bGPDstroke *gps,
        int offsx, int offsy, int winx, int winy, const float diff_mat[4][4], const float color[4])
{
	float fpt[3];

	BLI_assert(gps->totpoints >= 3);

	bGPDpalettecolor *palcolor = ED_gpencil_stroke_getcolor(gpd, gps);

	/* Triangulation fill if high quality flag is enabled */
	if (palcolor->flag & PC_COLOR_HQ_FILL) {
		/* Calculate triangles cache for filling area (must be done only after changes) */
		if ((gps->flag & GP_STROKE_RECALC_CACHES) || (gps->tot_triangles == 0) || (gps->triangles == NULL)) {
			gp_triangulate_stroke_fill(gps);
		}
		BLI_assert(gps->tot_triangles >= 1);

		uint pos;
		if (gps->flag & GP_STROKE_3DSPACE) {
			pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
		}
		else {
			pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
		}

		immUniformColor4fv(color);

		/* Draw all triangles for filling the polygon (cache must be calculated before) */
		immBegin(GPU_PRIM_TRIS, gps->tot_triangles * 3);
		/* TODO: use batch instead of immediate mode, to share vertices */

		bGPDtriangle *stroke_triangle = gps->triangles;
		bGPDspoint *pt;

		if (gps->flag & GP_STROKE_3DSPACE) {
			for (int i = 0; i < gps->tot_triangles; i++, stroke_triangle++) {
				for (int j = 0; j < 3; j++) {
					pt = &gps->points[stroke_triangle->verts[j]];
					mul_v3_m4v3(fpt, diff_mat, &pt->x);
					immVertex3fv(pos, fpt);
				}
			}
		}
		else {
			for (int i = 0; i < gps->tot_triangles; i++, stroke_triangle++) {
				for (int j = 0; j < 3; j++) {
					float co[2];
					pt = &gps->points[stroke_triangle->verts[j]];
					mul_v3_m4v3(fpt, diff_mat, &pt->x);
					gp_calc_2d_stroke_fxy(fpt, gps->flag, offsx, offsy, winx, winy, co);
					immVertex3fv(pos, fpt);
				}
			}
		}

		immEnd();
		immUnbindProgram();
	}
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void gp_draw_stroke_point(
        const bGPDspoint *points, short thickness, short UNUSED(dflag), short sflag,
        int offsx, int offsy, int winx, int winy, const float diff_mat[4][4], const float ink[4])
{
	const bGPDspoint *pt = points;

	/* get final position using parent matrix */
	float fpt[3];
	mul_v3_m4v3(fpt, diff_mat, &pt->x);

	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

	if (sflag & GP_STROKE_3DSPACE) {
		immBindBuiltinProgram(GPU_SHADER_3D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);
	}
	else {
		immBindBuiltinProgram(GPU_SHADER_2D_POINT_UNIFORM_SIZE_UNIFORM_COLOR_AA);

		/* get 2D coordinates of point */
		float co[3] = { 0.0f };
		gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);
		copy_v3_v3(fpt, co);
	}

	gp_set_point_uniform_color(pt, ink);
	/* set point thickness (since there's only one of these) */
	immUniform1f("size", (float)(thickness + 2) * pt->pressure);

	immBegin(GPU_PRIM_POINTS, 1);
	immVertex3fv(pos, fpt);
	immEnd();

	immUnbindProgram();
}

/* draw a given stroke in 3d (i.e. in 3d-space), using simple ogl lines */
static void gp_draw_stroke_3d(const bGPDspoint *points, int totpoints, short thickness, bool UNUSED(debug),
                              short UNUSED(sflag), const float diff_mat[4][4], const float ink[4], bool cyclic)
{
	float curpressure = points[0].pressure;
	float fpt[3];
	float cyclic_fpt[3];
	int draw_points = 0;

	/* if cyclic needs one vertex more */
	int cyclic_add = 0;
	if (cyclic) {
		++cyclic_add;
	}


	GPUVertFormat *format = immVertexFormat();
	uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
	uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

	immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

	/* TODO: implement this with a geometry shader to draw one continuous tapered stroke */

	/* draw stroke curve */
	GPU_line_width(max_ff(curpressure * thickness, 1.0f));
	immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints + cyclic_add);
	const bGPDspoint *pt = points;
	for (int i = 0; i < totpoints; i++, pt++) {
		gp_set_point_varying_color(pt, ink, color);

		/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
		 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
		 * Note: we want more visible levels of pressures when thickness is bigger.
		 */
		if (fabsf(pt->pressure - curpressure) > 0.2f / (float)thickness) {
			/* if the pressure changes before get at least 2 vertices, need to repeat last point to avoid assert in immEnd() */
			if (draw_points < 2) {
				const bGPDspoint *pt2 = pt - 1;
				mul_v3_m4v3(fpt, diff_mat, &pt2->x);
				immVertex3fv(pos, fpt);
			}
			immEnd();
			draw_points = 0;

			curpressure = pt->pressure;
			GPU_line_width(max_ff(curpressure * thickness, 1.0f));
			immBeginAtMost(GPU_PRIM_LINE_STRIP, totpoints - i + 1 + cyclic_add);

			/* need to roll-back one point to ensure that there are no gaps in the stroke */
			if (i != 0) {
				const bGPDspoint *pt2 = pt - 1;
				mul_v3_m4v3(fpt, diff_mat, &pt2->x);
				gp_set_point_varying_color(pt2, ink, color);
				immVertex3fv(pos, fpt);
				++draw_points;
			}
		}

		/* now the point we want */
		mul_v3_m4v3(fpt, diff_mat, &pt->x);
		immVertex3fv(pos, fpt);
		++draw_points;

		if (cyclic && i == 0) {
			/* save first point to use in cyclic */
			copy_v3_v3(cyclic_fpt, fpt);
		}
	}

	if (cyclic) {
		/* draw line to first point to complete the cycle */
		immVertex3fv(pos, cyclic_fpt);
		++draw_points;
	}

	/* if less of two points, need to repeat last point to avoid assert in immEnd() */
	if (draw_points < 2) {
		const bGPDspoint *pt2 = pt - 1;
		mul_v3_m4v3(fpt, diff_mat, &pt2->x);
		gp_set_point_varying_color(pt2, ink, color);
		immVertex3fv(pos, fpt);
	}

	immEnd();
	immUnbindProgram();
}

/* ----- Fancy 2D-Stroke Drawing ------ */

/* draw a given stroke in 2d */
static void gp_draw_stroke_2d(const bGPDspoint *points, int totpoints, short thickness_s, short dflag, short sflag,
                              bool UNUSED(debug), int offsx, int offsy, int winx, int winy, const float diff_mat[4][4], const float ink[4])
{
	/* otherwise thickness is twice that of the 3D view */
	float thickness = (float)thickness_s * 0.5f;

	/* strokes in Image Editor need a scale factor, since units there are not pixels! */
	float scalefac  = 1.0f;
	if ((dflag & GP_DRAWDATA_IEDITHACK) && (dflag & GP_DRAWDATA_ONLYV2D)) {
		scalefac = 0.001f;
	}

	/* TODO: fancy++ with the magic of shaders */

	/* tessellation code - draw stroke as series of connected quads (triangle strips in fact) with connection
	 * edges rotated to minimize shrinking artifacts, and rounded endcaps
	 */
	{
		const bGPDspoint *pt1, *pt2;
		float s0[2], s1[2];     /* segment 'center' points */
		float pm[2];  /* normal from previous segment. */
		int i;
		float fpt[3];

		GPUVertFormat *format = immVertexFormat();
		uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
		uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_U8, 4, GPU_FETCH_INT_TO_FLOAT_UNIT);

		immBindBuiltinProgram(GPU_SHADER_2D_FLAT_COLOR);
		immBegin(GPU_PRIM_TRI_STRIP, totpoints * 2 + 4);

		/* get x and y coordinates from first point */
		mul_v3_m4v3(fpt, diff_mat, &points->x);
		gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, s0);

		for (i = 0, pt1 = points, pt2 = points + 1; i < (totpoints - 1); i++, pt1++, pt2++) {
			float t0[2], t1[2];     /* tessellated coordinates */
			float m1[2], m2[2];     /* gradient and normal */
			float mt[2], sc[2];     /* gradient for thickness, point for end-cap */
			float pthick;           /* thickness at segment point */

			/* get x and y coordinates from point2 (point1 has already been computed in previous iteration). */
			mul_v3_m4v3(fpt, diff_mat, &pt2->x);
			gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, s1);

			/* calculate gradient and normal - 'angle'=(ny/nx) */
			m1[1] = s1[1] - s0[1];
			m1[0] = s1[0] - s0[0];
			normalize_v2(m1);
			m2[1] = -m1[0];
			m2[0] = m1[1];

			/* always use pressure from first point here */
			pthick = (pt1->pressure * thickness * scalefac);

			/* color of point */
			gp_set_point_varying_color(pt1, ink, color);

			/* if the first segment, start of segment is segment's normal */
			if (i == 0) {
				/* draw start cap first
				 *	- make points slightly closer to center (about halfway across)
				 */
				mt[0] = m2[0] * pthick * 0.5f;
				mt[1] = m2[1] * pthick * 0.5f;
				sc[0] = s0[0] - (m1[0] * pthick * 0.75f);
				sc[1] = s0[1] - (m1[1] * pthick * 0.75f);

				t0[0] = sc[0] - mt[0];
				t0[1] = sc[1] - mt[1];
				t1[0] = sc[0] + mt[0];
				t1[1] = sc[1] + mt[1];

				/* First two points of cap. */
				immVertex2fv(pos, t0);
				immVertex2fv(pos, t1);

				/* calculate points for start of segment */
				mt[0] = m2[0] * pthick;
				mt[1] = m2[1] * pthick;

				t0[0] = s0[0] - mt[0];
				t0[1] = s0[1] - mt[1];
				t1[0] = s0[0] + mt[0];
				t1[1] = s0[1] + mt[1];

				/* Last two points of start cap (and first two points of first segment). */
				immVertex2fv(pos, t0);
				immVertex2fv(pos, t1);
			}
			/* if not the first segment, use bisector of angle between segments */
			else {
				float mb[2];         /* bisector normal */
				float athick, dfac;  /* actual thickness, difference between thicknesses */

				/* calculate gradient of bisector (as average of normals) */
				mb[0] = (pm[0] + m2[0]) / 2;
				mb[1] = (pm[1] + m2[1]) / 2;
				normalize_v2(mb);

				/* calculate gradient to apply
				 *  - as basis, use just pthick * bisector gradient
				 *	- if cross-section not as thick as it should be, add extra padding to fix it
				 */
				mt[0] = mb[0] * pthick;
				mt[1] = mb[1] * pthick;
				athick = len_v2(mt);
				dfac = pthick - (athick * 2);

				if (((athick * 2.0f) < pthick) && (IS_EQF(athick, pthick) == 0)) {
					mt[0] += (mb[0] * dfac);
					mt[1] += (mb[1] * dfac);
				}

				/* calculate points for start of segment */
				t0[0] = s0[0] - mt[0];
				t0[1] = s0[1] - mt[1];
				t1[0] = s0[0] + mt[0];
				t1[1] = s0[1] + mt[1];

				/* Last two points of previous segment, and first two points of current segment. */
				immVertex2fv(pos, t0);
				immVertex2fv(pos, t1);
			}

			/* if last segment, also draw end of segment (defined as segment's normal) */
			if (i == totpoints - 2) {
				/* for once, we use second point's pressure (otherwise it won't be drawn) */
				pthick = (pt2->pressure * thickness * scalefac);

				/* color of point */
				gp_set_point_varying_color(pt2, ink, color);

				/* calculate points for end of segment */
				mt[0] = m2[0] * pthick;
				mt[1] = m2[1] * pthick;

				t0[0] = s1[0] - mt[0];
				t0[1] = s1[1] - mt[1];
				t1[0] = s1[0] + mt[0];
				t1[1] = s1[1] + mt[1];

				/* Last two points of last segment (and first two points of end cap). */
				immVertex2fv(pos, t0);
				immVertex2fv(pos, t1);

				/* draw end cap as last step
				 *	- make points slightly closer to center (about halfway across)
				 */
				mt[0] = m2[0] * pthick * 0.5f;
				mt[1] = m2[1] * pthick * 0.5f;
				sc[0] = s1[0] + (m1[0] * pthick * 0.75f);
				sc[1] = s1[1] + (m1[1] * pthick * 0.75f);

				t0[0] = sc[0] - mt[0];
				t0[1] = sc[1] - mt[1];
				t1[0] = sc[0] + mt[0];
				t1[1] = sc[1] + mt[1];

				/* Last two points of end cap. */
				immVertex2fv(pos, t0);
				immVertex2fv(pos, t1);
			}

			/* store computed point2 coordinates as point1 ones of next segment. */
			copy_v2_v2(s0, s1);
			/* store stroke's 'natural' normal for next stroke to use */
			copy_v2_v2(pm, m2);
		}

		immEnd();
		immUnbindProgram();
	}
}

/* ----- Strokes Drawing ------ */

/* Helper for doing all the checks on whether a stroke can be drawn */
static bool gp_can_draw_stroke(const bGPDstroke *gps, const int dflag)
{
	/* skip stroke if it isn't in the right display space for this drawing context */
	/* 1) 3D Strokes */
	if ((dflag & GP_DRAWDATA_ONLY3D) && !(gps->flag & GP_STROKE_3DSPACE))
		return false;
	if (!(dflag & GP_DRAWDATA_ONLY3D) && (gps->flag & GP_STROKE_3DSPACE))
		return false;

	/* 2) Screen Space 2D Strokes */
	if ((dflag & GP_DRAWDATA_ONLYV2D) && !(gps->flag & GP_STROKE_2DSPACE))
		return false;
	if (!(dflag & GP_DRAWDATA_ONLYV2D) && (gps->flag & GP_STROKE_2DSPACE))
		return false;

	/* 3) Image Space (2D) */
	if ((dflag & GP_DRAWDATA_ONLYI2D) && !(gps->flag & GP_STROKE_2DIMAGE))
		return false;
	if (!(dflag & GP_DRAWDATA_ONLYI2D) && (gps->flag & GP_STROKE_2DIMAGE))
		return false;

	/* skip stroke if it doesn't have any valid data */
	if ((gps->points == NULL) || (gps->totpoints < 1))
		return false;

	/* stroke can be drawn */
	return true;
}

/* draw a set of strokes */
static void gp_draw_strokes(
        bGPdata *gpd, const bGPDframe *gpf, int offsx, int offsy, int winx, int winy, int dflag,
        bool debug, short lthick, const float opacity, const float tintcolor[4],
        const bool onion, const bool custonion, const float diff_mat[4][4])
{
	float tcolor[4];
	float tfill[4];
	short sthickness;
	float ink[4];

	GPU_enable_program_point_size();

	for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
		/* check if stroke can be drawn */
		if (gp_can_draw_stroke(gps, dflag) == false) {
			continue;
		}
		/* check if the color is visible */
		bGPDpalettecolor *palcolor = ED_gpencil_stroke_getcolor(gpd, gps);
		if ((palcolor == NULL) ||
		    (palcolor->flag & PC_COLOR_HIDE) ||
		    /* if onion and ghost flag do not draw*/
		    (onion && (palcolor->flag & PC_COLOR_ONIONSKIN)))
		{
			continue;
		}

		/* calculate thickness */
		sthickness = gps->thickness + lthick;

		if (sthickness <= 0) {
			continue;
		}

		/* check which stroke-drawer to use */
		if (dflag & GP_DRAWDATA_ONLY3D) {
			const int no_xray = (dflag & GP_DRAWDATA_NO_XRAY);
			int mask_orig = 0;

			if (no_xray) {
				glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
				glDepthMask(0);
				GPU_depth_test(true);

				/* first arg is normally rv3d->dist, but this isn't
				 * available here and seems to work quite well without */
				bglPolygonOffset(1.0f, 1.0f);
			}

			/* 3D Fill */
			//if ((dflag & GP_DRAWDATA_FILL) && (gps->totpoints >= 3)) {
			if (gps->totpoints >= 3) {
				/* set color using palette, tint color and opacity */
				interp_v3_v3v3(tfill, palcolor->fill, tintcolor, tintcolor[3]);
				tfill[3] = palcolor->fill[3] * opacity;
				if (tfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) {
					const float *color;
					if (!onion) {
						color = tfill;
					}
					else {
						if (custonion) {
							color = tintcolor;
						}
						else {
							ARRAY_SET_ITEMS(tfill, UNPACK3(palcolor->fill), tintcolor[3]);
							color = tfill;
						}
					}
					gp_draw_stroke_fill(gpd, gps, offsx, offsy, winx, winy, diff_mat, color);
				}
			}

			/* 3D Stroke */
			/* set color using palette, tint color and opacity */
			if (!onion) {
				interp_v3_v3v3(tcolor, palcolor->color, tintcolor, tintcolor[3]);
				tcolor[3] = palcolor->color[3] * opacity;
				copy_v4_v4(ink, tcolor);
			}
			else {
				if (custonion) {
					copy_v4_v4(ink, tintcolor);
				}
				else {
					ARRAY_SET_ITEMS(tcolor, palcolor->color[0], palcolor->color[1], palcolor->color[2], opacity);
					copy_v4_v4(ink, tcolor);
				}
			}
			if (palcolor->flag & PC_COLOR_VOLUMETRIC) {
				/* volumetric stroke drawing */
				gp_draw_stroke_volumetric_3d(gps->points, gps->totpoints, sthickness, ink);
			}
			else {
				/* 3D Lines - OpenGL primitives-based */
				if (gps->totpoints == 1) {
					gp_draw_stroke_point(gps->points, sthickness, dflag, gps->flag, offsx, offsy, winx, winy,
					                     diff_mat, ink);
				}
				else {
					gp_draw_stroke_3d(gps->points, gps->totpoints, sthickness, debug, gps->flag,
					                  diff_mat, ink, gps->flag & GP_STROKE_CYCLIC);
				}
			}
			if (no_xray) {
				glDepthMask(mask_orig);
				GPU_depth_test(false);

				bglPolygonOffset(0.0, 0.0);
			}
		}
		else {
			/* 2D - Fill */
			if (gps->totpoints >= 3) {
				/* set color using palette, tint color and opacity */
				interp_v3_v3v3(tfill, palcolor->fill, tintcolor, tintcolor[3]);
				tfill[3] = palcolor->fill[3] * opacity;
				if (tfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) {
					const float *color;
					if (!onion) {
						color = tfill;
					}
					else {
						if (custonion) {
							color = tintcolor;
						}
						else {
							ARRAY_SET_ITEMS(tfill, palcolor->fill[0], palcolor->fill[1], palcolor->fill[2],
							                tintcolor[3]);
							color = tfill;
						}
					}
					gp_draw_stroke_fill(gpd, gps, offsx, offsy, winx, winy, diff_mat, color);
				}
			}

			/* 2D Strokes... */
			/* set color using palette, tint color and opacity */
			if (!onion) {
				interp_v3_v3v3(tcolor, palcolor->color, tintcolor, tintcolor[3]);
				tcolor[3] = palcolor->color[3] * opacity;
				copy_v4_v4(ink, tcolor);
			}
			else {
				if (custonion) {
					copy_v4_v4(ink, tintcolor);
				}
				else {
					ARRAY_SET_ITEMS(tcolor, palcolor->color[0], palcolor->color[1], palcolor->color[2], opacity);
					copy_v4_v4(ink, tcolor);
				}
			}
			if (palcolor->flag & PC_COLOR_VOLUMETRIC) {
				/* blob/disk-based "volumetric" drawing */
				gp_draw_stroke_volumetric_2d(gps->points, gps->totpoints, sthickness, dflag, gps->flag,
				                             offsx, offsy, winx, winy, diff_mat, ink);
			}
			else {
				/* normal 2D strokes */
				if (gps->totpoints == 1) {
					gp_draw_stroke_point(gps->points, sthickness, dflag, gps->flag, offsx, offsy, winx, winy,
					                     diff_mat, ink);
				}
				else {
					gp_draw_stroke_2d(gps->points, gps->totpoints, sthickness, dflag, gps->flag, debug,
					                  offsx, offsy, winx, winy, diff_mat, ink);
				}
			}
		}
	}

	GPU_disable_program_point_size();
}

/* Draw selected verts for strokes being edited */
static void gp_draw_strokes_edit(
        bGPdata *gpd, const bGPDframe *gpf, int offsx, int offsy, int winx, int winy, short dflag,
        short lflag, const float diff_mat[4][4], float alpha)
{
	/* if alpha 0 do not draw */
	if (alpha == 0.0f)
		return;

	const bool no_xray = (dflag & GP_DRAWDATA_NO_XRAY) != 0;
	int mask_orig = 0;

	/* set up depth masks... */
	if (dflag & GP_DRAWDATA_ONLY3D) {
		if (no_xray) {
			glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
			glDepthMask(0);
			GPU_depth_test(true);

			/* first arg is normally rv3d->dist, but this isn't
			 * available here and seems to work quite well without */
			bglPolygonOffset(1.0f, 1.0f);
		}
	}

	GPU_enable_program_point_size();

	/* draw stroke verts */
	for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
		/* check if stroke can be drawn */
		if (gp_can_draw_stroke(gps, dflag) == false)
			continue;

		/* Optimisation: only draw points for selected strokes
		 * We assume that selected points can only occur in
		 * strokes that are selected too.
		 */
		if ((gps->flag & GP_STROKE_SELECT) == 0)
			continue;

		/* verify palette color lock */
		{
			bGPDpalettecolor *palcolor = ED_gpencil_stroke_getcolor(gpd, gps);
			if (palcolor != NULL) {
				if (palcolor->flag & PC_COLOR_HIDE) {
					continue;
				}
				if (((lflag & GP_LAYER_UNLOCK_COLOR) == 0) && (palcolor->flag & PC_COLOR_LOCKED)) {
					continue;
				}
			}
		}

		/* Get size of verts:
		 * - The selected state needs to be larger than the unselected state so that
		 *   they stand out more.
		 * - We use the theme setting for size of the unselected verts
		 */
		float bsize = UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
		float vsize;
		if ((int)bsize > 8) {
			vsize = 10.0f;
			bsize = 8.0f;
		}
		else {
			vsize = bsize + 2;
		}

		/* for now, we assume that the base color of the points is not too close to the real color */
		/* set color using palette */
		bGPDpalettecolor *palcolor = ED_gpencil_stroke_getcolor(gpd, gps);

		float selectColor[4];
		UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, selectColor);
		selectColor[3] = alpha;

		GPUVertFormat *format = immVertexFormat();
		uint pos; /* specified later */
		uint size = GPU_vertformat_attr_add(format, "size", GPU_COMP_F32, 1, GPU_FETCH_FLOAT);
		uint color = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

		if (gps->flag & GP_STROKE_3DSPACE) {
			pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_3D_POINT_VARYING_SIZE_VARYING_COLOR);
		}
		else {
			pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
			immBindBuiltinProgram(GPU_SHADER_2D_POINT_VARYING_SIZE_VARYING_COLOR);
		}

		immBegin(GPU_PRIM_POINTS, gps->totpoints);

		/* Draw start and end point differently if enabled stroke direction hint */
		bool show_direction_hint = (gpd->flag & GP_DATA_SHOW_DIRECTION) && (gps->totpoints > 1);

		/* Draw all the stroke points (selected or not) */
		bGPDspoint *pt = gps->points;
		float fpt[3];
		for (int i = 0; i < gps->totpoints; i++, pt++) {
			/* size and color first */
			if (show_direction_hint && i == 0) {
				/* start point in green bigger */
				immAttrib3f(color, 0.0f, 1.0f, 0.0f);
				immAttrib1f(size, vsize + 4);
			}
			else if (show_direction_hint && (i == gps->totpoints - 1)) {
				/* end point in red smaller */
				immAttrib3f(color, 1.0f, 0.0f, 0.0f);
				immAttrib1f(size, vsize + 1);
			}
			else if (pt->flag & GP_SPOINT_SELECT) {
				immAttrib3fv(color, selectColor);
				immAttrib1f(size, vsize);
			}
			else {
				immAttrib3fv(color, palcolor->color);
				immAttrib1f(size, bsize);
			}

			/* then position */
			if (gps->flag & GP_STROKE_3DSPACE) {
				mul_v3_m4v3(fpt, diff_mat, &pt->x);
				immVertex3fv(pos, fpt);
			}
			else {
				float co[2];
				mul_v3_m4v3(fpt, diff_mat, &pt->x);
				gp_calc_2d_stroke_fxy(fpt, gps->flag, offsx, offsy, winx, winy, co);
				immVertex2fv(pos, co);
			}
		}

		immEnd();
		immUnbindProgram();
	}

	GPU_disable_program_point_size();

	/* clear depth mask */
	if (dflag & GP_DRAWDATA_ONLY3D) {
		if (no_xray) {
			glDepthMask(mask_orig);
			GPU_depth_test(false);

			bglPolygonOffset(0.0, 0.0);
#if 0
			glDisable(GL_POLYGON_OFFSET_LINE);
			glPolygonOffset(0, 0);
#endif
		}
	}
}

/* ----- General Drawing ------ */

/* draw onion-skinning for a layer */
static void gp_draw_onionskins(
        bGPdata *gpd, const bGPDlayer *gpl, const bGPDframe *gpf, int offsx, int offsy, int winx, int winy,
        int UNUSED(cfra), int dflag, bool debug, const float diff_mat[4][4])
{
	const float default_color[3] = {UNPACK3(U.gpencil_new_layer_col)};
	const float alpha = 1.0f;
	float color[4];

	/* 1) Draw Previous Frames First */
	if (gpl->flag & GP_LAYER_GHOST_PREVCOL) {
		copy_v3_v3(color, gpl->gcolor_prev);
	}
	else {
		copy_v3_v3(color, default_color);
	}

	if (gpl->gstep > 0) {
		/* draw previous frames first */
		for (bGPDframe *gf = gpf->prev; gf; gf = gf->prev) {
			/* check if frame is drawable */
			if ((gpf->framenum - gf->framenum) <= gpl->gstep) {
				/* alpha decreases with distance from curframe index */
				float fac = 1.0f - ((float)(gpf->framenum - gf->framenum) / (float)(gpl->gstep + 1));
				color[3] = alpha * fac * 0.66f;
				gp_draw_strokes(gpd, gf, offsx, offsy, winx, winy, dflag, debug, gpl->thickness, 1.0f, color,
				                true, gpl->flag & GP_LAYER_GHOST_PREVCOL, diff_mat);
			}
			else
				break;
		}
	}
	else if (gpl->gstep == 0) {
		/* draw the strokes for the ghost frames (at half of the alpha set by user) */
		if (gpf->prev) {
			color[3] = (alpha / 7);
			gp_draw_strokes(gpd, gpf->prev, offsx, offsy, winx, winy, dflag, debug, gpl->thickness, 1.0f, color,
			                true, gpl->flag & GP_LAYER_GHOST_PREVCOL, diff_mat);
		}
	}
	else {
		/* don't draw - disabled */
	}

	/* 2) Now draw next frames */
	if (gpl->flag & GP_LAYER_GHOST_NEXTCOL) {
		copy_v3_v3(color, gpl->gcolor_next);
	}
	else {
		copy_v3_v3(color, default_color);
	}

	if (gpl->gstep_next > 0) {
		/* now draw next frames */
		for (bGPDframe *gf = gpf->next; gf; gf = gf->next) {
			/* check if frame is drawable */
			if ((gf->framenum - gpf->framenum) <= gpl->gstep_next) {
				/* alpha decreases with distance from curframe index */
				float fac = 1.0f - ((float)(gf->framenum - gpf->framenum) / (float)(gpl->gstep_next + 1));
				color[3] = alpha * fac * 0.66f;
				gp_draw_strokes(gpd, gf, offsx, offsy, winx, winy, dflag, debug, gpl->thickness, 1.0f, color,
				                true, gpl->flag & GP_LAYER_GHOST_NEXTCOL, diff_mat);
			}
			else
				break;
		}
	}
	else if (gpl->gstep_next == 0) {
		/* draw the strokes for the ghost frames (at half of the alpha set by user) */
		if (gpf->next) {
			color[3] = (alpha / 4);
			gp_draw_strokes(gpd, gpf->next, offsx, offsy, winx, winy, dflag, debug, gpl->thickness, 1.0f, color,
			                true, gpl->flag & GP_LAYER_GHOST_NEXTCOL, diff_mat);
		}
	}
	else {
		/* don't draw - disabled */
	}
}

/* draw interpolate strokes (used only while operator is running) */
void ED_gp_draw_interpolation(tGPDinterpolate *tgpi, const int type)
{
	tGPDinterpolate_layer *tgpil;
	float diff_mat[4][4];
	float color[4];

	int offsx = 0;
	int offsy = 0;
	int winx = tgpi->ar->winx;
	int winy = tgpi->ar->winy;

	UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, color);
	color[3] = 0.6f;
	int dflag = 0;
	/* if 3d stuff, enable flags */
	if (type == REGION_DRAW_POST_VIEW) {
		dflag |= (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_NOSTATUS);
	}

	/* turn on alpha-blending */
	GPU_blend(true);
	for (tgpil = tgpi->ilayers.first; tgpil; tgpil = tgpil->next) {
		/* calculate parent position */
		ED_gpencil_parent_location(tgpil->gpl, diff_mat);
		if (tgpil->interFrame) {
			gp_draw_strokes(tgpi->gpd, tgpil->interFrame, offsx, offsy, winx, winy, dflag, false,
				tgpil->gpl->thickness, 1.0f, color, true, true, diff_mat);
		}
	}
	GPU_blend(false);
}

/* loop over gpencil data layers, drawing them */
static void gp_draw_data_layers(
        const bGPDbrush *brush, float alpha, bGPdata *gpd,
        int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
	float diff_mat[4][4];

	for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		/* calculate parent position */
		ED_gpencil_parent_location(gpl, diff_mat);

		bool debug = (gpl->flag & GP_LAYER_DRAWDEBUG);
		short lthick = brush->thickness + gpl->thickness;

		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE)
			continue;

		/* get frame to draw */
		bGPDframe *gpf = BKE_gpencil_layer_getframe(gpl, cfra, 0);
		if (gpf == NULL)
			continue;

		/* set basic stroke thickness */
		GPU_line_width(lthick);

		/* Add layer drawing settings to the set of "draw flags"
		 * NOTE: If the setting doesn't apply, it *must* be cleared,
		 *       as dflag's carry over from the previous layer
		 */
#define GP_DRAWFLAG_APPLY(condition, draw_flag_value)     { \
			if (condition) dflag |= (draw_flag_value);      \
			else           dflag &= ~(draw_flag_value);     \
		} (void)0

		/* xray... */
		GP_DRAWFLAG_APPLY((gpl->flag & GP_LAYER_NO_XRAY), GP_DRAWDATA_NO_XRAY);

		/* volumetric strokes... */
		GP_DRAWFLAG_APPLY((gpl->flag & GP_LAYER_VOLUMETRIC), GP_DRAWDATA_VOLUMETRIC);

		/* HQ fills... */
		GP_DRAWFLAG_APPLY((gpl->flag & GP_LAYER_HQ_FILL), GP_DRAWDATA_HQ_FILL);

#undef GP_DRAWFLAG_APPLY

		/* Draw 'onionskins' (frame left + right)
		 *   - It is only possible to show these if the option is enabled
		 *   - The "no onions" flag prevents ghosts from appearing during animation playback/scrubbing
		 *     and in renders
		 *   - The per-layer "always show" flag however overrides the playback/render restriction,
		 *     allowing artists to selectively turn onionskins on/off during playback
		 */
		if ((gpl->flag & GP_LAYER_ONIONSKIN) &&
		    ((dflag & GP_DRAWDATA_NO_ONIONS) == 0 || (gpl->flag & GP_LAYER_GHOST_ALWAYS)))
		{
			/* Drawing method - only immediately surrounding (gstep = 0),
			 * or within a frame range on either side (gstep > 0)
			 */
			gp_draw_onionskins(gpd, gpl, gpf, offsx, offsy, winx, winy, cfra, dflag, debug, diff_mat);
		}

		/* draw the strokes already in active frame */
		gp_draw_strokes(gpd, gpf, offsx, offsy, winx, winy, dflag, debug, gpl->thickness,
		                gpl->opacity, gpl->tintcolor, false, false, diff_mat);

		/* Draw verts of selected strokes
		 *  - when doing OpenGL renders, we don't want to be showing these, as that ends up flickering
		 * 	- locked layers can't be edited, so there's no point showing these verts
		 *    as they will have no bearings on what gets edited
		 *  - only show when in editmode, since operators shouldn't work otherwise
		 *    (NOTE: doing it this way means that the toggling editmode shows visible change immediately)
		 */
		/* XXX: perhaps we don't want to show these when users are drawing... */
		if ((G.f & G_RENDER_OGL) == 0 &&
		    (gpl->flag & GP_LAYER_LOCKED) == 0 &&
		    (gpd->flag & GP_DATA_STROKE_EDITMODE))
		{
			gp_draw_strokes_edit(gpd, gpf, offsx, offsy, winx, winy, dflag, gpl->flag, diff_mat, alpha);
		}

		/* Check if may need to draw the active stroke cache, only if this layer is the active layer
		 * that is being edited. (Stroke buffer is currently stored in gp-data)
		 */
		if (ED_gpencil_session_active() && (gpl->flag & GP_LAYER_ACTIVE) &&
		    (gpf->flag & GP_FRAME_PAINT))
		{
			/* Buffer stroke needs to be drawn with a different linestyle
			 * to help differentiate them from normal strokes.
			 *
			 * It should also be noted that sbuffer contains temporary point types
			 * i.e. tGPspoints NOT bGPDspoints
			 */
			if (gpd->sflag & PC_COLOR_VOLUMETRIC) {
				gp_draw_stroke_volumetric_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick,
				                                 dflag, gpd->scolor);
			}
			else {
				gp_draw_stroke_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag, gpd->scolor, gpd->sfill);
			}
		}
	}
}

/* draw a short status message in the top-right corner */
static void gp_draw_status_text(const bGPdata *gpd, ARegion *ar)
{
	rcti rect;

	/* Cannot draw any status text when drawing OpenGL Renders */
	if (G.f & G_RENDER_OGL)
		return;

	/* Get bounds of region - Necessary to avoid problems with region overlap */
	ED_region_visible_rect(ar, &rect);

	/* for now, this should only be used to indicate when we are in stroke editmode */
	if (gpd->flag & GP_DATA_STROKE_EDITMODE) {
		const char *printable = IFACE_("GPencil Stroke Editing");
		float       printable_size[2];

		int font_id = BLF_default();

		BLF_width_and_height(font_id, printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);

		int xco = (rect.xmax - U.widget_unit) - (int)printable_size[0];
		int yco = (rect.ymax - U.widget_unit);

		/* text label */
		UI_FontThemeColor(font_id, TH_TEXT_HI);
#ifdef WITH_INTERNATIONAL
		BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#else
		BLF_draw_default_ascii(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#endif

		/* grease pencil icon... */
		// XXX: is this too intrusive?
		GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
		GPU_blend(true);

		xco -= U.widget_unit;
		yco -= (int)printable_size[1] / 2;

		UI_icon_draw(xco, yco, ICON_GREASEPENCIL);

		GPU_blend(false);
	}
}

/* draw grease-pencil datablock */
static void gp_draw_data(
        const bGPDbrush *brush, float alpha, bGPdata *gpd,
        int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
	/* turn on smooth lines (i.e. anti-aliasing) */
	GPU_line_smooth(true);

	/* XXX: turn on some way of ensuring that the polygon edges get smoothed
	 *      GL_POLYGON_SMOOTH is nasty and shouldn't be used, as it ends up
	 *      creating internal white rays due to the ways it accumulates stuff
	 */

	/* turn on alpha-blending */
	GPU_blend_set_func_separate(GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
	GPU_blend(true);

	/* draw! */
	gp_draw_data_layers(brush, alpha, gpd, offsx, offsy, winx, winy, cfra, dflag);

	/* turn off alpha blending, then smooth lines */
	GPU_blend(false); // alpha blending
	GPU_line_smooth(false); // smooth lines
}

/* if we have strokes for scenes (3d view)/clips (movie clip editor)
 * and objects/tracks, multiple data blocks have to be drawn */
static void gp_draw_data_all(Scene *scene, bGPdata *gpd, int offsx, int offsy, int winx, int winy,
                             int cfra, int dflag, const char spacetype)
{
	bGPdata *gpd_source = NULL;
	ToolSettings *ts;
	bGPDbrush *brush = NULL;
	if (scene) {
		ts = scene->toolsettings;
		brush = BKE_gpencil_brush_getactive(ts);
		/* if no brushes, create default set */
		if (brush == NULL) {
			BKE_gpencil_brush_init_presets(ts);
			brush = BKE_gpencil_brush_getactive(ts);
		}

		if (spacetype == SPACE_VIEW3D) {
			gpd_source = (scene->gpd ? scene->gpd : NULL);
		}
		else if (spacetype == SPACE_CLIP && scene->clip) {
			/* currently drawing only gpencil data from either clip or track, but not both - XXX fix logic behind */
			gpd_source = (scene->clip->gpd ? scene->clip->gpd : NULL);
		}

		if (gpd_source) {
			if (brush != NULL) {
				gp_draw_data(brush, ts->gp_sculpt.alpha, gpd_source,
				             offsx, offsy, winx, winy, cfra, dflag);
			}
		}
	}

	/* scene/clip data has already been drawn, only object/track data is drawn here
	 * if gpd_source == gpd, we don't have any object/track data and we can skip */
	if (gpd_source == NULL || (gpd_source && gpd_source != gpd)) {
		if (brush != NULL) {
			gp_draw_data(brush, ts->gp_sculpt.alpha, gpd,
			             offsx, offsy, winx, winy, cfra, dflag);
		}
	}
}

/* ----- Grease Pencil Sketches Drawing API ------ */

/* ............................
 * XXX
 *	We need to review the calls below, since they may be/are not that suitable for
 *	the new ways that we intend to be drawing data...
 * ............................ */

/* draw grease-pencil sketches to specified 2d-view that uses ibuf corrections */
void ED_gpencil_draw_2dimage(const bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);

	int offsx, offsy, sizex, sizey;
	int dflag = GP_DRAWDATA_NOSTATUS;

	bGPdata *gpd = ED_gpencil_data_get_active(C); // XXX
	if (gpd == NULL) return;

	/* calculate rect */
	switch (sa->spacetype) {
		case SPACE_IMAGE: /* image */
		case SPACE_CLIP: /* clip */
		{
			/* just draw using standard scaling (settings here are currently ignored anyways) */
			/* FIXME: the opengl poly-strokes don't draw at right thickness when done this way, so disabled */
			offsx = 0;
			offsy = 0;
			sizex = ar->winx;
			sizey = ar->winy;

			wmOrtho2(ar->v2d.cur.xmin, ar->v2d.cur.xmax, ar->v2d.cur.ymin, ar->v2d.cur.ymax);

			dflag |= GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_IEDITHACK;
			break;
		}
		case SPACE_SEQ: /* sequence */
		{
			/* just draw using standard scaling (settings here are currently ignored anyways) */
			offsx = 0;
			offsy = 0;
			sizex = ar->winx;
			sizey = ar->winy;

			/* NOTE: I2D was used in 2.4x, but the old settings for that have been deprecated
			 * and everything moved to standard View2d
			 */
			dflag |= GP_DRAWDATA_ONLYV2D;
			break;
		}
		default: /* for spacetype not yet handled */
			offsx = 0;
			offsy = 0;
			sizex = ar->winx;
			sizey = ar->winy;

			dflag |= GP_DRAWDATA_ONLYI2D;
			break;
	}

	if (ED_screen_animation_playing(wm)) {
		/* don't show onionskins during animation playback/scrub (i.e. it obscures the poses)
		 * OpenGL Renders (i.e. final output), or depth buffer (i.e. not real strokes)
		 */
		dflag |= GP_DRAWDATA_NO_ONIONS;
	}

	/* draw it! */
	gp_draw_data_all(scene, gpd, offsx, offsy, sizex, sizey, CFRA, dflag, sa->spacetype);
}

/* draw grease-pencil sketches to specified 2d-view assuming that matrices are already set correctly
 * Note: this gets called twice - first time with onlyv2d=true to draw 'canvas' strokes,
 * second time with onlyv2d=false for screen-aligned strokes */
void ED_gpencil_draw_view2d(const bContext *C, bool onlyv2d)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	int dflag = 0;

	/* check that we have grease-pencil stuff to draw */
	if (sa == NULL) return;
	bGPdata *gpd = ED_gpencil_data_get_active(C); // XXX
	if (gpd == NULL) return;

	/* special hack for Image Editor */
	/* FIXME: the opengl poly-strokes don't draw at right thickness when done this way, so disabled */
	if (ELEM(sa->spacetype, SPACE_IMAGE, SPACE_CLIP))
		dflag |= GP_DRAWDATA_IEDITHACK;

	/* draw it! */
	if (onlyv2d) dflag |= (GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_NOSTATUS);
	if (ED_screen_animation_playing(wm)) dflag |= GP_DRAWDATA_NO_ONIONS;

	gp_draw_data_all(scene, gpd, 0, 0, ar->winx, ar->winy, CFRA, dflag, sa->spacetype);

	/* draw status text (if in screen/pixel-space) */
	if (!onlyv2d) {
		gp_draw_status_text(gpd, ar);
	}
}

/* draw grease-pencil sketches to specified 3d-view assuming that matrices are already set correctly
 * Note: this gets called twice - first time with only3d=true to draw 3d-strokes,
 * second time with only3d=false for screen-aligned strokes */
void ED_gpencil_draw_view3d(wmWindowManager *wm,
                            Scene *scene,
                            ViewLayer *view_layer,
                            struct Depsgraph *depsgraph,
                            View3D *v3d,
                            ARegion *ar,
                            bool only3d)
{
	int dflag = 0;
	RegionView3D *rv3d = ar->regiondata;
	int offsx,  offsy,  winx,  winy;

	/* check that we have grease-pencil stuff to draw */
	bGPdata *gpd = ED_gpencil_data_get_active_v3d(scene, view_layer);
	if (gpd == NULL) return;

	/* when rendering to the offscreen buffer we don't want to
	 * deal with the camera border, otherwise map the coords to the camera border. */
	if ((rv3d->persp == RV3D_CAMOB) && !(G.f & G_RENDER_OGL)) {
		rctf rectf;
		ED_view3d_calc_camera_border(scene, depsgraph, ar, v3d, rv3d, &rectf, true); /* no shift */

		offsx = round_fl_to_int(rectf.xmin);
		offsy = round_fl_to_int(rectf.ymin);
		winx  = round_fl_to_int(rectf.xmax - rectf.xmin);
		winy  = round_fl_to_int(rectf.ymax - rectf.ymin);
	}
	else {
		offsx = 0;
		offsy = 0;
		winx  = ar->winx;
		winy  = ar->winy;
	}

	/* set flags */
	if (only3d) {
		/* 3D strokes/3D space:
		 * - only 3D space points
		 * - don't status text either (as it's the wrong space)
		 */
		dflag |= (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_NOSTATUS);
	}

	if (v3d->flag2 & V3D_RENDER_OVERRIDE) {
		/* don't draw status text when "only render" flag is set */
		dflag |= GP_DRAWDATA_NOSTATUS;
	}

	if ((wm == NULL) || ED_screen_animation_playing(wm)) {
		/* don't show onionskins during animation playback/scrub (i.e. it obscures the poses)
		 * OpenGL Renders (i.e. final output), or depth buffer (i.e. not real strokes)
		 */
		dflag |= GP_DRAWDATA_NO_ONIONS;
	}

	/* draw it! */
	gp_draw_data_all(scene, gpd, offsx, offsy, winx, winy, CFRA, dflag, v3d->spacetype);
}

void ED_gpencil_draw_ex(Scene *scene, bGPdata *gpd, int winx, int winy, const int cfra, const char spacetype)
{
	int dflag = GP_DRAWDATA_NOSTATUS | GP_DRAWDATA_ONLYV2D;

	gp_draw_data_all(scene, gpd, 0, 0, winx, winy, cfra, dflag, spacetype);
}

/* ************************************************** */
