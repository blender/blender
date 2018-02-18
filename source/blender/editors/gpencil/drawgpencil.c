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

#include "BIF_gl.h"
#include "BIF_glutil.h"

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
#define GP_DRAWTHICKNESS_SPECIAL    3

/* ----- Tool Buffer Drawing ------ */
/* helper function to set color of buffer point */
static void gp_set_tpoint_color(tGPspoint *pt, float ink[4])
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	glColor4f(ink[0], ink[1], ink[2], alpha);
}

/* helper function to set color of point */
static void gp_set_point_color(bGPDspoint *pt, float ink[4])
{
	float alpha = ink[3] * pt->strength;
	CLAMP(alpha, GPENCIL_STRENGTH_MIN, 1.0f);
	glColor4f(ink[0], ink[1], ink[2], alpha);
}

/* helper function to set color and point */
static void gp_set_color_and_tpoint(tGPspoint *pt, float ink[4])
{
	gp_set_tpoint_color(pt, ink);
	glVertex2iv(&pt->x);
}

/* draw stroke defined in buffer (simple ogl lines/points for now, as dotted lines) */
static void gp_draw_stroke_buffer(tGPspoint *points, int totpoints, short thickness,
                                  short dflag, short sflag, float ink[4])
{
	tGPspoint *pt;
	int i;
	
	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;
	
	/* check if buffer can be drawn */
	if (dflag & (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_ONLYV2D))
		return;
	
	if (totpoints == 1) {
		/* if drawing a single point, draw it larger */
		glPointSize((float)(thickness + 2) * points->pressure);
		glBegin(GL_POINTS);

		gp_set_color_and_tpoint(points, ink);
		glEnd();
	}
	else if (sflag & GP_STROKE_ERASER) {
		/* don't draw stroke at all! */
	}
	else {
		float oldpressure = points[0].pressure;
		
		/* draw stroke curve */
		if (G.debug & G_DEBUG) setlinestyle(2);
		
		glLineWidth(max_ff(oldpressure * thickness, 1.0));
		glBegin(GL_LINE_STRIP);
		
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
			/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
			 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
			 */
			if (fabsf(pt->pressure - oldpressure) > 0.2f) {
				glEnd();
				glLineWidth(max_ff(pt->pressure * thickness, 1.0f));
				glBegin(GL_LINE_STRIP);
				
				/* need to roll-back one point to ensure that there are no gaps in the stroke */
				if (i != 0) { 
					gp_set_color_and_tpoint((pt - 1), ink);
				}
				
				/* now the point we want... */
				gp_set_color_and_tpoint(pt, ink);
				
				oldpressure = pt->pressure;
			}
			else {
				gp_set_color_and_tpoint(pt, ink);
			}
		}
		glEnd();

		if (G.debug & G_DEBUG) setlinestyle(0);
	}
}

/* --------- 2D Stroke Drawing Helpers --------- */
/* change in parameter list */
static void gp_calc_2d_stroke_fxy(float pt[3], short sflag, int offsx, int offsy, int winx, int winy, float r_co[2])
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
static void gp_draw_stroke_volumetric_buffer(tGPspoint *points, int totpoints, short thickness,
                                             short dflag, short UNUSED(sflag), float ink[4])
{
	GLUquadricObj *qobj = gluNewQuadric();
	float modelview[4][4];
	
	tGPspoint *pt;
	int i;
	
	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;
	
	/* check if buffer can be drawn */
	if (dflag & (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_ONLYV2D))
		return;
	
	/* get basic matrix - should be camera space (i.e "identity") */
	glGetFloatv(GL_MODELVIEW_MATRIX, (float *)modelview);
	
	/* draw points */
	glPushMatrix();
	
	for (i = 0, pt = points; i < totpoints; i++, pt++) {
		/* set the transformed position */
		// TODO: scale should change based on zoom level, which requires proper translation mult too!
		modelview[3][0] = pt->x;
		modelview[3][1] = pt->y;
		
		glLoadMatrixf((float *)modelview);
		
		/* draw the disk using the current state... */
		gp_set_tpoint_color(pt, ink);
		gluDisk(qobj, 0.0,  pt->pressure * thickness, 32, 1);
		
		
		modelview[3][0] = modelview[3][1] = 0.0f;
	}

	glPopMatrix();
	gluDeleteQuadric(qobj);
}

/* draw a 2D strokes in "volumetric" style */
static void gp_draw_stroke_volumetric_2d(bGPDspoint *points, int totpoints, short thickness,
                                         short dflag, short sflag,
                                         int offsx, int offsy, int winx, int winy,
                                         float diff_mat[4][4], float ink[4])
{
	GLUquadricObj *qobj = gluNewQuadric();
	float modelview[4][4];
	float baseloc[3];
	float scalefac = 1.0f;
	
	bGPDspoint *pt;
	int i;
	float fpt[3];
	
	/* HACK: We need a scale factor for the drawing in the image editor,
	 * which seems to use 1 unit as it's maximum size, whereas everything
	 * else assumes 1 unit = 1 pixel. Otherwise, we only get a massive blob.
	 */
	if ((dflag & GP_DRAWDATA_IEDITHACK) && (dflag & GP_DRAWDATA_ONLYV2D)) {
		scalefac = 0.001f;
	}
	
	/* get basic matrix */
	glGetFloatv(GL_MODELVIEW_MATRIX, (float *)modelview);
	copy_v3_v3(baseloc, modelview[3]);
	
	/* draw points */
	glPushMatrix();
	
	for (i = 0, pt = points; i < totpoints; i++, pt++) {
		/* color of point */
		gp_set_point_color(pt, ink);

		/* set the transformed position */
		float co[2];
		
		mul_v3_m4v3(fpt, diff_mat, &pt->x);
		gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);
		translate_m4(modelview, co[0], co[1], 0.0f);
		
		glLoadMatrixf((float *)modelview);
		
		/* draw the disk using the current state... */
		gluDisk(qobj, 0.0,  pt->pressure * thickness * scalefac, 32, 1);
		
		/* restore matrix */
		copy_v3_v3(modelview[3], baseloc);
	}
	
	glPopMatrix();
	gluDeleteQuadric(qobj);
}

/* draw a 3D stroke in "volumetric" style */
static void gp_draw_stroke_volumetric_3d(
        bGPDspoint *points, int totpoints, short thickness,
        short UNUSED(dflag), short UNUSED(sflag), float diff_mat[4][4], float ink[4])
{
	GLUquadricObj *qobj = gluNewQuadric();
	
	float base_modelview[4][4], modelview[4][4];
	float base_loc[3];
	
	bGPDspoint *pt;
	int i;
	float fpt[3];
	
	/* Get the basic modelview matrix we use for performing calculations */
	glGetFloatv(GL_MODELVIEW_MATRIX, (float *)base_modelview);
	copy_v3_v3(base_loc, base_modelview[3]);
	
	/* Create the basic view-aligned billboard matrix we're going to actually draw qobj with:
	 * - We need to knock out the rotation so that we are
	 *   simply left with a camera-facing billboard
	 * - The scale factors here are chosen so that the thickness
	 *   is relatively reasonable. Otherwise, it gets far too
	 *   large!
	 */
	scale_m4_fl(modelview, 0.1f);
	
	/* draw each point as a disk... */
	glPushMatrix();
	
	for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
		/* color of point */
		gp_set_point_color(pt, ink);

		mul_v3_m4v3(fpt, diff_mat, &pt->x);

		/* apply translation to base_modelview, so that the translated point is put in the right place */
		translate_m4(base_modelview, fpt[0], fpt[1], fpt[2]);
		
		/* copy the translation component to the billboard matrix we're going to use,
		 * then reset the base matrix to the original values so that we can do the same
		 * for the next point without accumulation/pollution effects
		 */
		copy_v3_v3(modelview[3], base_modelview[3]); /* copy offset value */
		copy_v3_v3(base_modelview[3], base_loc);     /* restore */
		
		/* apply our billboard matrix for drawing... */
		glLoadMatrixf((float *)modelview);
		
		/* draw the disk using the current state... */
		gluDisk(qobj, 0.0,  pt->pressure * thickness, 32, 1);
	}
	
	glPopMatrix();
	gluDeleteQuadric(qobj);
}


/* --------------- Stroke Fills ----------------- */

/* Get points of stroke always flat to view not affected by camera view or view position */
static void gp_stroke_2d_flat(bGPDspoint *points, int totpoints, float(*points2d)[2], int *r_direction)
{
	bGPDspoint *pt0 = &points[0];
	bGPDspoint *pt1 = &points[1];
	bGPDspoint *pt3 = &points[(int)(totpoints * 0.75)];
	
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
		bGPDspoint *pt = &points[i];
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
			bGPDtriangle *stroke_triangle = &gps->triangles[i];
			memcpy(stroke_triangle->verts, tmp_triangles[i], sizeof(uint[3]));
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
        int offsx, int offsy, int winx, int winy, float diff_mat[4][4])
{
	bGPDpalettecolor *palcolor;
	int i;
	float fpt[3];

	BLI_assert(gps->totpoints >= 3);

	palcolor = ED_gpencil_stroke_getcolor(gpd, gps);

	/* Triangulation fill if high quality flag is enabled */
	if (palcolor->flag & PC_COLOR_HQ_FILL) {
		bGPDtriangle *stroke_triangle;
		bGPDspoint *pt;

		/* Calculate triangles cache for filling area (must be done only after changes) */
		if ((gps->flag & GP_STROKE_RECALC_CACHES) || (gps->tot_triangles == 0) || (gps->triangles == NULL)) {
			gp_triangulate_stroke_fill(gps);
		}
		/* Draw all triangles for filling the polygon (cache must be calculated before) */
		BLI_assert(gps->tot_triangles >= 1);
		glBegin(GL_TRIANGLES);
		if (gps->flag & GP_STROKE_3DSPACE) {
			for (i = 0, stroke_triangle = gps->triangles; i < gps->tot_triangles; i++, stroke_triangle++) {
				for (int j = 0; j < 3; j++) {
					pt = &gps->points[stroke_triangle->verts[j]];
					mul_v3_m4v3(fpt, diff_mat, &pt->x);
					glVertex3fv(fpt);
				}
			}
		}
		else {
			for (i = 0, stroke_triangle = gps->triangles; i < gps->tot_triangles; i++, stroke_triangle++) {
				for (int j = 0; j < 3; j++) {
					float co[2];
					pt = &gps->points[stroke_triangle->verts[j]];
					mul_v3_m4v3(fpt, diff_mat, &pt->x);
					gp_calc_2d_stroke_fxy(fpt, gps->flag, offsx, offsy, winx, winy, co);
					glVertex2fv(co);
				}
			}
		}
		glEnd();
	}
	else {
		/* As an initial implementation, we use the OpenGL filled polygon drawing
		 * here since it's the easiest option to implement for this case. It does
		 * come with limitations (notably for concave shapes), though it shouldn't
		 * be much of an issue in most cases.
		 *
		 * We keep this legacy implementation around despite now having the high quality
		 * fills, as this is necessary for keeping everything working nicely for files
		 * created using old versions of Blender which may have depended on the artifacts
		 * the old fills created.
		 */
		bGPDspoint *pt;

		glBegin(GL_POLYGON);
		for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
			if (gps->flag & GP_STROKE_3DSPACE) {
				mul_v3_m4v3(fpt, diff_mat, &pt->x);
				glVertex3fv(fpt);
			}
			else {
				float co[2];
				mul_v3_m4v3(fpt, diff_mat, &pt->x);
				gp_calc_2d_stroke_fxy(fpt, gps->flag, offsx, offsy, winx, winy, co);
				glVertex2fv(co);
			}
		}

		glEnd();
	}
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void gp_draw_stroke_point(
        bGPDspoint *points, short thickness, short dflag, short sflag,
        int offsx, int offsy, int winx, int winy, float diff_mat[4][4], float ink[4])
{
	float fpt[3];
	bGPDspoint *pt = &points[0];

	/* color of point */
	gp_set_point_color(pt, ink);

	/* set point thickness (since there's only one of these) */
	glPointSize((float)(thickness + 2) * points->pressure);
	
	/* get final position using parent matrix */
	mul_v3_m4v3(fpt, diff_mat, &pt->x);

	/* draw point */
	if (sflag & GP_STROKE_3DSPACE) {
		glBegin(GL_POINTS);
		glVertex3fv(fpt);
		glEnd();
	}
	else {
		float co[2];
		
		/* get coordinates of point */
		gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);
		
		/* if thickness is less than GP_DRAWTHICKNESS_SPECIAL, simple dot looks ok
		 *  - also mandatory in if Image Editor 'image-based' dot
		 */
		if ((thickness < GP_DRAWTHICKNESS_SPECIAL) ||
		    ((dflag & GP_DRAWDATA_IEDITHACK) && (sflag & GP_STROKE_2DSPACE)))
		{
			glBegin(GL_POINTS);
			glVertex2fv(co);
			glEnd();
		}
		else {
			/* draw filled circle as is done in circf (but without the matrix push/pops which screwed things up) */
			GLUquadricObj *qobj = gluNewQuadric();
			
			gluQuadricDrawStyle(qobj, GLU_FILL);
			
			/* need to translate drawing position, but must reset after too! */
			glTranslate2fv(co);
			gluDisk(qobj, 0.0,  thickness, 32, 1);
			glTranslatef(-co[0], -co[1], 0.0);
			
			gluDeleteQuadric(qobj);
		}
	}
}

/* draw a given stroke in 3d (i.e. in 3d-space), using simple ogl lines */
static void gp_draw_stroke_3d(bGPDspoint *points, int totpoints, short thickness, bool debug,
                              short UNUSED(sflag), float diff_mat[4][4], float ink[4], bool cyclic)
{
	bGPDspoint *pt, *pt2;
	float curpressure = points[0].pressure;
	int i;
	float fpt[3];
	float cyclic_fpt[3];

	/* draw stroke curve */
	glLineWidth(max_ff(curpressure * thickness, 1.0f));
	glBegin(GL_LINE_STRIP);
	for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
		gp_set_point_color(pt, ink);

		/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
		 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
		 * Note: we want more visible levels of pressures when thickness is bigger.
		 */
		if (fabsf(pt->pressure - curpressure) > 0.2f / (float)thickness) {
			glEnd();
			curpressure = pt->pressure;
			glLineWidth(max_ff(curpressure * thickness, 1.0f));
			glBegin(GL_LINE_STRIP);
			
			/* need to roll-back one point to ensure that there are no gaps in the stroke */
			if (i != 0) { 
				pt2 = pt - 1;
				mul_v3_m4v3(fpt, diff_mat, &pt2->x);
				glVertex3fv(fpt);
			}
			
			/* now the point we want... */
			mul_v3_m4v3(fpt, diff_mat, &pt->x);
			glVertex3fv(fpt);
		}
		else {
			mul_v3_m4v3(fpt, diff_mat, &pt->x);
			glVertex3fv(fpt);
		}
		/* saves first point to use in cyclic */
		if (i == 0) {
			copy_v3_v3(cyclic_fpt, fpt);
		}
	}
	/* if cyclic draw line to first point */
	if (cyclic) {
		glVertex3fv(cyclic_fpt);
	}
	glEnd();

	/* draw debug points of curve on top? */
	/* XXX: for now, we represent "selected" strokes in the same way as debug, which isn't used anymore */
	if (debug) {
		glPointSize((float)(thickness + 2));
		
		glBegin(GL_POINTS);
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
			mul_v3_m4v3(fpt, diff_mat, &pt->x);
			glVertex3fv(fpt);
		}
		glEnd();

	}
}

/* ----- Fancy 2D-Stroke Drawing ------ */

/* draw a given stroke in 2d */
static void gp_draw_stroke_2d(bGPDspoint *points, int totpoints, short thickness_s, short dflag, short sflag,
                              bool debug, int offsx, int offsy, int winx, int winy, float diff_mat[4][4], float ink[4])
{
	/* otherwise thickness is twice that of the 3D view */
	float thickness = (float)thickness_s * 0.5f;
	
	/* strokes in Image Editor need a scale factor, since units there are not pixels! */
	float scalefac  = 1.0f;
	if ((dflag & GP_DRAWDATA_IEDITHACK) && (dflag & GP_DRAWDATA_ONLYV2D)) {
		scalefac = 0.001f;
	}
	
	/* tessellation code - draw stroke as series of connected quads with connection
	 * edges rotated to minimize shrinking artifacts, and rounded endcaps
	 */
	{
		bGPDspoint *pt1, *pt2;
		float s0[2], s1[2];     /* segment 'center' points */
		float pm[2];  /* normal from previous segment. */
		int i;
		float fpt[3];
		
		glShadeModel(GL_FLAT);
		glBegin(GL_QUADS);

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
			gp_set_point_color(pt1, ink);

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
				
				glVertex2fv(t0);
				glVertex2fv(t1);
				
				/* calculate points for start of segment */
				mt[0] = m2[0] * pthick;
				mt[1] = m2[1] * pthick;
				
				t0[0] = s0[0] - mt[0];
				t0[1] = s0[1] - mt[1];
				t1[0] = s0[0] + mt[0];
				t1[1] = s0[1] + mt[1];
				
				/* draw this line twice (first to finish off start cap, then for stroke) */
				glVertex2fv(t1);
				glVertex2fv(t0);
				glVertex2fv(t0);
				glVertex2fv(t1);
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
				
				/* draw this line twice (once for end of current segment, and once for start of next) */
				glVertex2fv(t1);
				glVertex2fv(t0);
				glVertex2fv(t0);
				glVertex2fv(t1);
			}
			
			/* if last segment, also draw end of segment (defined as segment's normal) */
			if (i == totpoints - 2) {
				/* for once, we use second point's pressure (otherwise it won't be drawn) */
				pthick = (pt2->pressure * thickness * scalefac);
				
				/* color of point */
				gp_set_point_color(pt2, ink);

				/* calculate points for end of segment */
				mt[0] = m2[0] * pthick;
				mt[1] = m2[1] * pthick;
				
				t0[0] = s1[0] - mt[0];
				t0[1] = s1[1] - mt[1];
				t1[0] = s1[0] + mt[0];
				t1[1] = s1[1] + mt[1];
				
				/* draw this line twice (once for end of stroke, and once for endcap)*/
				glVertex2fv(t1);
				glVertex2fv(t0);
				glVertex2fv(t0);
				glVertex2fv(t1);
				
				
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
				
				glVertex2fv(t1);
				glVertex2fv(t0);
			}
			
			/* store computed point2 coordinates as point1 ones of next segment. */
			copy_v2_v2(s0, s1);
			/* store stroke's 'natural' normal for next stroke to use */
			copy_v2_v2(pm, m2);
		}
		
		glEnd();
		glShadeModel(GL_SMOOTH);
	}
	
	/* draw debug points of curve on top? (original stroke points) */
	if (debug) {
		bGPDspoint *pt;
		int i;
		float fpt[3];

		glPointSize((float)(thickness_s + 2));
		
		glBegin(GL_POINTS);
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
			float co[2];
			mul_v3_m4v3(fpt, diff_mat, &pt->x);
			gp_calc_2d_stroke_fxy(fpt, sflag, offsx, offsy, winx, winy, co);
			glVertex2fv(co);
		}
		glEnd();
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
        bGPdata *gpd, bGPDframe *gpf, int offsx, int offsy, int winx, int winy, int dflag,
        bool debug, short lthick, const float opacity, const float tintcolor[4],
        const bool onion, const bool custonion, float diff_mat[4][4])
{
	bGPDstroke *gps;
	float tcolor[4];
	float tfill[4];
	short sthickness;
	float ink[4];

	for (gps = gpf->strokes.first; gps; gps = gps->next) {
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

		/* check which stroke-drawer to use */
		if (dflag & GP_DRAWDATA_ONLY3D) {
			const int no_xray = (dflag & GP_DRAWDATA_NO_XRAY);
			int mask_orig = 0;

			if (no_xray) {
				glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
				glDepthMask(0);
				glEnable(GL_DEPTH_TEST);

				/* first arg is normally rv3d->dist, but this isn't
				 * available here and seems to work quite well without */
				bglPolygonOffset(1.0f, 1.0f);
#if 0
				glEnable(GL_POLYGON_OFFSET_LINE);
				glPolygonOffset(-1.0f, -1.0f);
#endif
			}

			/* 3D Fill */
			//if ((dflag & GP_DRAWDATA_FILL) && (gps->totpoints >= 3)) {
			if (gps->totpoints >= 3) {
				/* set color using palette, tint color and opacity */
				interp_v3_v3v3(tfill, palcolor->fill, tintcolor, tintcolor[3]);
				tfill[3] = palcolor->fill[3] * opacity;
				if (tfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) {
					if (!onion) {
						glColor4fv(tfill);
					}
					else {
						if (custonion) {
							glColor4fv(tintcolor);
						}
						else {
							ARRAY_SET_ITEMS(tfill, UNPACK3(palcolor->fill), tintcolor[3]);
							glColor4fv(tfill);
						}
					}
					gp_draw_stroke_fill(gpd, gps, offsx, offsy, winx, winy, diff_mat);
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
				gp_draw_stroke_volumetric_3d(gps->points, gps->totpoints, sthickness, dflag, gps->flag, diff_mat, ink);
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
				glDisable(GL_DEPTH_TEST);

				bglPolygonOffset(0.0, 0.0);
#if 0
				glDisable(GL_POLYGON_OFFSET_LINE);
				glPolygonOffset(0, 0);
#endif
			}
		}
		else {
			/* 2D - Fill */
			if (gps->totpoints >= 3) {
				/* set color using palette, tint color and opacity */
				interp_v3_v3v3(tfill, palcolor->fill, tintcolor, tintcolor[3]);
				tfill[3] = palcolor->fill[3] * opacity;
				if (tfill[3] > GPENCIL_ALPHA_OPACITY_THRESH) {
					if (!onion) {
						glColor4fv(tfill);
					}
					else {
						if (custonion) {
							glColor4fv(tintcolor);
						}
						else {
							ARRAY_SET_ITEMS(tfill, palcolor->fill[0], palcolor->fill[1], palcolor->fill[2],
							                tintcolor[3]);
							glColor4fv(tfill);
						}
					}
					gp_draw_stroke_fill(gpd, gps, offsx, offsy, winx, winy, diff_mat);
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
}

/* Draw selected verts for strokes being edited */
static void gp_draw_strokes_edit(
        bGPdata *gpd, bGPDframe *gpf, int offsx, int offsy, int winx, int winy, short dflag,
        short lflag, float diff_mat[4][4], float alpha)
{
	bGPDstroke *gps;
	
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
			glEnable(GL_DEPTH_TEST);
			
			/* first arg is normally rv3d->dist, but this isn't
			 * available here and seems to work quite well without */
			bglPolygonOffset(1.0f, 1.0f);
#if 0
			glEnable(GL_POLYGON_OFFSET_LINE);
			glPolygonOffset(-1.0f, -1.0f);
#endif
		}
	}
	
	
	/* draw stroke verts */
	for (gps = gpf->strokes.first; gps; gps = gps->next) {
		bGPDspoint *pt;
		float vsize, bsize;
		int i;
		float fpt[3];

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
		bsize = UI_GetThemeValuef(TH_GP_VERTEX_SIZE);
		if ((int)bsize > 8) {
			vsize = 10.0f;
			bsize = 8.0f;
		}
		else {
			vsize = bsize + 2;
		}
		
		/* First Pass: Draw all the verts (i.e. these become the unselected state) */
		/* for now, we assume that the base color of the points is not too close to the real color */
		/* set color using palette */
		bGPDpalettecolor *palcolor = ED_gpencil_stroke_getcolor(gpd, gps);
		glColor3fv(palcolor->color);

		glPointSize(bsize);
		
		glBegin(GL_POINTS);
		for (i = 0, pt = gps->points; i < gps->totpoints && pt; i++, pt++) {
			if (gps->flag & GP_STROKE_3DSPACE) {
				mul_v3_m4v3(fpt, diff_mat, &pt->x);
				glVertex3fv(fpt);
			}
			else {
				float co[2];
				mul_v3_m4v3(fpt, diff_mat, &pt->x);
				gp_calc_2d_stroke_fxy(fpt, gps->flag, offsx, offsy, winx, winy, co);
				glVertex2fv(co);
			}
		}
		glEnd();
		
		
		/* Second Pass: Draw only verts which are selected */
		float curColor[4];
		UI_GetThemeColor3fv(TH_GP_VERTEX_SELECT, curColor);
		glColor4f(curColor[0], curColor[1], curColor[2], alpha);

		glPointSize(vsize);
		
		glBegin(GL_POINTS);
		for (i = 0, pt = gps->points; i < gps->totpoints && pt; i++, pt++) {
			if (pt->flag & GP_SPOINT_SELECT) {
				if (gps->flag & GP_STROKE_3DSPACE) {
					mul_v3_m4v3(fpt, diff_mat, &pt->x);
					glVertex3fv(fpt);
				}
				else {
					float co[2];
					
					mul_v3_m4v3(fpt, diff_mat, &pt->x);
					gp_calc_2d_stroke_fxy(fpt, gps->flag, offsx, offsy, winx, winy, co);
					glVertex2fv(co);
				}
			}
		}
		glEnd();

		/* Draw start and end point if enabled stroke direction hint */
		if ((gpd->flag & GP_DATA_SHOW_DIRECTION) && (gps->totpoints > 1)) {
			bGPDspoint *p;
			
			glPointSize(vsize + 4);
			glBegin(GL_POINTS);

			/* start point in green bigger */
			glColor3f(0.0f, 1.0f, 0.0f);
			p = &gps->points[0];
			mul_v3_m4v3(fpt, diff_mat, &p->x);
			glVertex3fv(fpt);
			glEnd();

			/* end point in red smaller */
			glPointSize(vsize + 1);
			glBegin(GL_POINTS);

			glColor3f(1.0f, 0.0f, 0.0f);
			p = &gps->points[gps->totpoints - 1];
			mul_v3_m4v3(fpt, diff_mat, &p->x);
			glVertex3fv(fpt);
			glEnd();
		}
	}
	
	
	/* clear depth mask */
	if (dflag & GP_DRAWDATA_ONLY3D) {
		if (no_xray) {
			glDepthMask(mask_orig);
			glDisable(GL_DEPTH_TEST);
			
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
        bGPdata *gpd, bGPDlayer *gpl, bGPDframe *gpf, int offsx, int offsy, int winx, int winy,
        int UNUSED(cfra), int dflag, bool debug, float diff_mat[4][4])
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
		bGPDframe *gf;
		float fac;
		
		/* draw previous frames first */
		for (gf = gpf->prev; gf; gf = gf->prev) {
			/* check if frame is drawable */
			if ((gpf->framenum - gf->framenum) <= gpl->gstep) {
				/* alpha decreases with distance from curframe index */
				fac = 1.0f - ((float)(gpf->framenum - gf->framenum) / (float)(gpl->gstep + 1));
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
		bGPDframe *gf;
		float fac;
		
		/* now draw next frames */
		for (gf = gpf->next; gf; gf = gf->next) {
			/* check if frame is drawable */
			if ((gf->framenum - gpf->framenum) <= gpl->gstep_next) {
				/* alpha decreases with distance from curframe index */
				fac = 1.0f - ((float)(gf->framenum - gpf->framenum) / (float)(gpl->gstep_next + 1));
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
	glEnable(GL_BLEND);
	for (tgpil = tgpi->ilayers.first; tgpil; tgpil = tgpil->next) {
		/* calculate parent position */
		ED_gpencil_parent_location(tgpil->gpl, diff_mat);
		if (tgpil->interFrame) {
			gp_draw_strokes(tgpi->gpd, tgpil->interFrame, offsx, offsy, winx, winy, dflag, false,
				tgpil->gpl->thickness, 1.0f, color, true, true, diff_mat);
		}
	}
	glDisable(GL_BLEND);
}

/* loop over gpencil data layers, drawing them */
static void gp_draw_data_layers(
        bGPDbrush *brush, float alpha, bGPdata *gpd,
        int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
	bGPDlayer *gpl;
	float diff_mat[4][4];

	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		bGPDframe *gpf;
		/* calculate parent position */
		ED_gpencil_parent_location(gpl, diff_mat);

		bool debug = (gpl->flag & GP_LAYER_DRAWDEBUG) ? true : false;
		short lthick = brush->thickness + gpl->thickness;
		
		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE)
			continue;
		
		/* get frame to draw */
		gpf = BKE_gpencil_layer_getframe(gpl, cfra, 0);
		if (gpf == NULL)
			continue;
		
		/* set basic stroke thickness */
		glLineWidth(lthick);
		
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
			/* Set color for drawing buffer stroke - since this may not be set yet */
			// glColor4fv(gpl->color);
			
			/* Buffer stroke needs to be drawn with a different linestyle
			 * to help differentiate them from normal strokes.
			 * 
			 * It should also be noted that sbuffer contains temporary point types
			 * i.e. tGPspoints NOT bGPDspoints
			 */
			if (gpd->sflag & PC_COLOR_VOLUMETRIC) {
				gp_draw_stroke_volumetric_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick,
				                                 dflag, gpd->sbuffer_sflag, gpd->scolor);
			}
			else {
				gp_draw_stroke_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag, gpd->scolor);
			}
		}
	}
}

/* draw a short status message in the top-right corner */
static void gp_draw_status_text(bGPdata *gpd, ARegion *ar)
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
		int xco, yco;
		
		BLF_width_and_height_default(printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);
		
		xco = (rect.xmax - U.widget_unit) - (int)printable_size[0];
		yco = (rect.ymax - U.widget_unit);
		
		/* text label */
		UI_ThemeColor(TH_TEXT_HI);
#ifdef WITH_INTERNATIONAL
		BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#else
		BLF_draw_default_ascii(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#endif
		
		/* grease pencil icon... */
		// XXX: is this too intrusive?
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
		xco -= U.widget_unit;
		yco -= (int)printable_size[1] / 2;

		UI_icon_draw(xco, yco, ICON_GREASEPENCIL);
		
		glDisable(GL_BLEND);
	}
}

/* draw grease-pencil datablock */
static void gp_draw_data(
        bGPDbrush *brush, float alpha, bGPdata *gpd,
        int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
	/* reset line drawing style (in case previous user didn't reset) */
	setlinestyle(0);
	
	/* turn on smooth lines (i.e. anti-aliasing) */
	glEnable(GL_LINE_SMOOTH);
	
	/* XXX: turn on some way of ensuring that the polygon edges get smoothed 
	 *      GL_POLYGON_SMOOTH is nasty and shouldn't be used, as it ends up
	 *      creating internal white rays due to the ways it accumulates stuff
	 */
	
	/* turn on alpha-blending */
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	/* draw! */
	gp_draw_data_layers(brush, alpha, gpd, offsx, offsy, winx, winy, cfra, dflag);
	
	/* turn off alpha blending, then smooth lines */
	glDisable(GL_BLEND); // alpha blending
	glDisable(GL_LINE_SMOOTH); // smooth lines
	
	/* restore initial gl conditions */
	glColor4f(0, 0, 0, 1);
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
	bGPdata *gpd;
	int offsx, offsy, sizex, sizey;
	int dflag = GP_DRAWDATA_NOSTATUS;
	
	gpd = ED_gpencil_data_get_active(C); // XXX
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
 * Note: this gets called twice - first time with onlyv2d=1 to draw 'canvas' strokes,
 * second time with onlyv2d=0 for screen-aligned strokes */
void ED_gpencil_draw_view2d(const bContext *C, bool onlyv2d)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd;
	int dflag = 0;
	
	/* check that we have grease-pencil stuff to draw */
	if (sa == NULL) return;
	gpd = ED_gpencil_data_get_active(C); // XXX
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
	if (onlyv2d == false) {
		gp_draw_status_text(gpd, ar);
	}
}

/* draw grease-pencil sketches to specified 3d-view assuming that matrices are already set correctly
 * Note: this gets called twice - first time with only3d=1 to draw 3d-strokes,
 * second time with only3d=0 for screen-aligned strokes */
void ED_gpencil_draw_view3d(wmWindowManager *wm, Scene *scene, View3D *v3d, ARegion *ar, bool only3d)
{
	bGPdata *gpd;
	int dflag = 0;
	RegionView3D *rv3d = ar->regiondata;
	int offsx,  offsy,  winx,  winy;
	
	/* check that we have grease-pencil stuff to draw */
	gpd = ED_gpencil_data_get_active_v3d(scene, v3d);
	if (gpd == NULL) return;
	
	/* when rendering to the offscreen buffer we don't want to
	 * deal with the camera border, otherwise map the coords to the camera border. */
	if ((rv3d->persp == RV3D_CAMOB) && !(G.f & G_RENDER_OGL)) {
		rctf rectf;
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &rectf, true); /* no shift */
		
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
