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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_draw.c
 *  \ingroup edmask
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"	/* SELECT */

#include "ED_mask.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"

#include "mask_intern.h"	// own include

typedef struct PixelSpaceContext {
	int width, height;
	float zoomx, zoomy;
	float aspx, aspy;
} PixelSpaceContext;

static void set_spline_color(MaskShape *shape, MaskSpline *spline)
{
	if (spline->flag & SELECT) {
		if (shape->act_spline == spline)
			glColor3f(1.0f, 1.0f, 1.0f);
		else
			glColor3f(1.0f, 0.0f, 0.0f);
	}
	else {
		glColor3f(0.5f, 0.0f, 0.0f);
	}
}

/* return non-zero if spline is selected */
static void draw_spline_points(MaskShape *shape, MaskSpline *spline, PixelSpaceContext *pixelspace)
{
	int i, hsize, tot_feather_point;
	float *feather_points, *fp;

	if (!spline->tot_point)
		return;

	hsize = UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE);

	glPointSize(hsize);

	/* feather points */
	feather_points = fp = BKE_mask_spline_feather_points(spline, pixelspace->aspx, pixelspace->aspy, &tot_feather_point);
	for (i = 0; i < spline->tot_point; i++) {
		int j;
		MaskSplinePoint *point = &spline->points[i];

		for (j = 0; j < point->tot_uw + 1; j++) {
			int sel = FALSE;

			if (j == 0) {
				sel = MASKPOINT_ISSEL(point);
			}
			else {
				sel = point->uw[j - 1].flag & SELECT;
			}

			if (sel) {
				if (point == shape->act_point)
					glColor3f(1.0f, 1.0f, 1.0f);
				else
					glColor3f(1.0f, 1.0f, 0.0f);
			} else
				glColor3f(0.5f, 0.5f, 0.0f);

			glBegin(GL_POINTS);
				glVertex2fv(fp);
			glEnd();

			fp += 2;
		}
	}
	MEM_freeN(feather_points);

	/* control points */
	for (i = 0; i < spline->tot_point; i++) {
		MaskSplinePoint *point = &spline->points[i];
		BezTriple *bezt = &point->bezt;
		float vert[2], handle[2];
		int has_handle = BKE_mask_point_has_handle(point);;

		copy_v2_v2(vert, bezt->vec[1]);
		BKE_mask_point_handle(point, pixelspace->aspx, pixelspace->aspy, handle);

		/* draw handle segment */
		if (has_handle) {
			set_spline_color(shape, spline);

			glBegin(GL_LINES);
				glVertex3fv(vert);
				glVertex3fv(handle);
			glEnd();
		}

		/* draw CV point */
		if (MASKPOINT_CV_ISSEL(point)) {
			if (point == shape->act_point)
				glColor3f(1.0f, 1.0f, 1.0f);
			else
				glColor3f(1.0f, 1.0f, 0.0f);
		} else
			glColor3f(0.5f, 0.5f, 0.0f);

		glBegin(GL_POINTS);
			glVertex3fv(vert);
		glEnd();

		/* draw handle points */
		if (has_handle) {
			if (MASKPOINT_HANDLE_ISSEL(point)) {
				if (point == shape->act_point)
					glColor3f(1.0f, 1.0f, 1.0f);
				else
					glColor3f(1.0f, 1.0f, 0.0f);
			} else
				glColor3f(0.5f, 0.5f, 0.0f);

			glBegin(GL_POINTS);
				glVertex3fv(handle);
			glEnd();
		}
	}

	glPointSize(1.0f);
}

static void draw_spline_curve_lines(float *points, int tot_point, int closed)
{
	int i;
	float *fp = points;

	if (closed)
		glBegin(GL_LINE_LOOP);
	else
		glBegin(GL_LINE_STRIP);

	for (i = 0; i < tot_point; i++, fp+=2) {
		glVertex3fv(fp);
	}
	glEnd();
}

static void draw_dashed_curve(MaskSpline *spline, float *points, int tot_point)
{
	glEnable(GL_COLOR_LOGIC_OP);
	glLogicOp(GL_OR);

	draw_spline_curve_lines(points, tot_point, spline->flag & MASK_SPLINE_CYCLIC);

	glDisable(GL_COLOR_LOGIC_OP);
	glLineStipple(3, 0xaaaa);
	glEnable(GL_LINE_STIPPLE);

	glColor3f(0.0f, 0.0f, 0.0f);
	draw_spline_curve_lines(points, tot_point, spline->flag & MASK_SPLINE_CYCLIC);

	glDisable(GL_LINE_STIPPLE);
}

static void draw_spline_curve(MaskShape *shape, MaskSpline *spline, PixelSpaceContext *pixelspace)
{
	float *diff_points, *feather_points;
	int tot_diff_point, tot_feather_point;

	diff_points = BKE_mask_spline_differentiate(spline, &tot_diff_point);

	if (!diff_points)
		return;

	feather_points = BKE_mask_spline_feather_differentiated_points(spline, pixelspace->aspx, pixelspace->aspy,
	                                                              &tot_feather_point);

	/* draw feather */
	if (spline->flag & SELECT)
		glColor3f(0.0f, 1.0f, 0.0f);
	else
		glColor3f(0.0f, 0.5f, 0.0f);
	draw_dashed_curve(spline, feather_points, tot_feather_point);

	/* draw main curve */
	set_spline_color(shape, spline);
	draw_dashed_curve(spline, diff_points, tot_diff_point);

	MEM_freeN(diff_points);
	MEM_freeN(feather_points);
}

static void draw_shapes(Mask *mask, PixelSpaceContext *pixelspace)
{
	MaskShape *shape = mask->shapes.first;

	while (shape) {
		MaskSpline *spline = shape->splines.first;

		while (spline) {
			/* draw curve itself first... */
			draw_spline_curve(shape, spline, pixelspace);

			/* ...and then handles over the curve so they're nicely visible */
			draw_spline_points(shape, spline, pixelspace);

			spline = spline->next;
		}

		shape = shape->next;
	}
}

void ED_mask_draw(bContext *C, int width, int height, float zoomx, float zoomy)
{
	Mask *mask = CTX_data_edit_mask(C);
	PixelSpaceContext pixelspace;
	float aspx, aspy;

	if (!mask)
		return;

	ED_mask_aspect(C, &aspx, &aspy);

	pixelspace.width = width;
	pixelspace.height = height;
	pixelspace.zoomx = zoomx;
	pixelspace.zoomy = zoomy;
	pixelspace.aspx = aspx;
	pixelspace.aspy = aspy;

	draw_shapes(mask, &pixelspace);
}
