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
 * Contributor(s): Joshua Leung
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

#include "BLI_sys_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"

#include "WM_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_gpencil.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "gpencil_intern.h"

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
} eDrawStrokeFlags;



/* thickness above which we should use special drawing */
#define GP_DRAWTHICKNESS_SPECIAL    3

/* ----- Tool Buffer Drawing ------ */

/* draw stroke defined in buffer (simple ogl lines/points for now, as dotted lines) */
static void gp_draw_stroke_buffer(tGPspoint *points, int totpoints, short thickness, short dflag, short sflag)
{
	tGPspoint *pt;
	int i;
	
	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;
	
	/* check if buffer can be drawn */
	if (dflag & (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_ONLYV2D))
		return;
	
	/* if drawing a single point, draw it larger */
	if (totpoints == 1) {
		/* draw point */
		glBegin(GL_POINTS);
		glVertex2iv(&points->x);
		glEnd();
	}
	else if (sflag & GP_STROKE_ERASER) {
		/* don't draw stroke at all! */
	}
	else {
		float oldpressure = points[0].pressure;
		
		/* draw stroke curve */
		if (G.debug & G_DEBUG) setlinestyle(2);

		glLineWidth(oldpressure * thickness);
		glBegin(GL_LINE_STRIP);

		for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
			/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
			 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
			 */
			if (fabsf(pt->pressure - oldpressure) > 0.2f) {
				glEnd();
				glLineWidth(pt->pressure * thickness);
				glBegin(GL_LINE_STRIP);
				
				/* need to roll-back one point to ensure that there are no gaps in the stroke */
				if (i != 0) glVertex2iv(&(pt - 1)->x);

				/* now the point we want... */
				glVertex2iv(&pt->x);
				
				oldpressure = pt->pressure;
			}
			else
				glVertex2iv(&pt->x);
		}
		glEnd();

		/* reset for predictable OpenGL context */
		glLineWidth(1.0f);
		
		if (G.debug & G_DEBUG) setlinestyle(0);
	}
}

/* --------- 2D Stroke Drawing Helpers --------- */

/* helper function to calculate x-y drawing coordinates for 2D points */
static void gp_calc_2d_stroke_xy(bGPDspoint *pt, short sflag, int offsx, int offsy, int winx, int winy, float r_co[2])
{
	if (sflag & GP_STROKE_2DSPACE) {
		r_co[0] = pt->x;
		r_co[1] = pt->y;
	}
	else if (sflag & GP_STROKE_2DIMAGE) {
		const float x = (float)((pt->x * winx) + offsx);
		const float y = (float)((pt->y * winy) + offsy);
		
		r_co[0] = x;
		r_co[1] = y;
	}
	else {
		const float x = (float)(pt->x / 100 * winx) + offsx;
		const float y = (float)(pt->y / 100 * winy) + offsy;
		
		r_co[0] = x;
		r_co[1] = y;
	}
}

/* ----------- Volumetric Strokes --------------- */

/* draw a 2D buffer stroke in "volumetric" style
 * NOTE: the stroke buffer doesn't have any coordinate offsets/transforms
 */
static void gp_draw_stroke_volumetric_buffer(tGPspoint *points, int totpoints, short thickness,
                                             short dflag, short UNUSED(sflag))
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
		gluDisk(qobj, 0.0,  pt->pressure * thickness, 32, 1);
		
		
		modelview[3][0] = modelview[3][1] = 0.0f;
	}
	
	glPopMatrix();
	gluDeleteQuadric(qobj);
}

/* draw a 2D strokes in "volumetric" style */
static void gp_draw_stroke_volumetric_2d(bGPDspoint *points, int totpoints, short thickness,
                                         short dflag, short sflag,
                                         int offsx, int offsy, int winx, int winy)
{
	GLUquadricObj *qobj = gluNewQuadric();
	float modelview[4][4];
	float baseloc[3];
	float scalefac = 1.0f;
	
	bGPDspoint *pt;
	int i;
	
	
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
		/* set the transformed position */
		float co[2];
		
		gp_calc_2d_stroke_xy(pt, sflag, offsx, offsy, winx, winy, co);
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
static void gp_draw_stroke_volumetric_3d(bGPDspoint *points, int totpoints, short thickness,
                                         short UNUSED(dflag), short UNUSED(sflag))
{
	GLUquadricObj *qobj = gluNewQuadric();
	
	float base_modelview[4][4], modelview[4][4];
	float base_loc[3];
	
	bGPDspoint *pt;
	int i;
	
	
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
		/* apply translation to base_modelview, so that the translated point is put in the right place */
		translate_m4(base_modelview, pt->x, pt->y, pt->z);
		
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

/* draw fills for shapes */
static void gp_draw_stroke_fill(bGPDspoint *points, int totpoints, short UNUSED(thickness),
                                short UNUSED(dflag), short sflag,
                                int offsx, int offsy, int winx, int winy)
{
	bGPDspoint *pt;
	int i;
	
	BLI_assert(totpoints >= 3);
	
	/* As an initial implementation, we use the OpenGL filled polygon drawing 
	 * here since it's the easiest option to implement for this case. It does
	 * come with limitations (notably for concave shapes), though it shouldn't
	 * be much of an issue in most cases.
	 */
	glBegin(GL_POLYGON);
	
	for (i = 0, pt = points; i < totpoints; i++, pt++) {
		if (sflag & GP_STROKE_3DSPACE) {
			glVertex3fv(&pt->x);
		}
		else {
			float co[2];
			
			gp_calc_2d_stroke_xy(pt, sflag, offsx, offsy, winx, winy, co);
			glVertex2fv(co);
		}
	}
	
	glEnd();
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void gp_draw_stroke_point(bGPDspoint *points, short thickness, short dflag, short sflag,
                                 int offsx, int offsy, int winx, int winy)
{
	/* draw point */
	if (sflag & GP_STROKE_3DSPACE) {
		glBegin(GL_POINTS);
		glVertex3fv(&points->x);
		glEnd();
	}
	else {
		float co[2];
		
		/* get coordinates of point */
		gp_calc_2d_stroke_xy(points, sflag, offsx, offsy, winx, winy, co);
		
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
			glTranslatef(co[0], co[1], 0.0);
			gluDisk(qobj, 0.0,  thickness, 32, 1); 
			glTranslatef(-co[0], -co[1], 0.0);
			
			gluDeleteQuadric(qobj);
		}
	}
}

/* draw a given stroke in 3d (i.e. in 3d-space), using simple ogl lines */
static void gp_draw_stroke_3d(bGPDspoint *points, int totpoints, short thickness, bool debug, short UNUSED(sflag))
{
	bGPDspoint *pt;
	float curpressure = points[0].pressure;
	int i;
	
	/* draw stroke curve */
	glLineWidth(curpressure * thickness);
	glBegin(GL_LINE_STRIP);
	for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
		/* if there was a significant pressure change, stop the curve, change the thickness of the stroke,
		 * and continue drawing again (since line-width cannot change in middle of GL_LINE_STRIP)
		 * Note: we want more visible levels of pressures when thickness is bigger.
		 */
		if (fabsf(pt->pressure - curpressure) > 0.2f / (float)thickness) {
			glEnd();
			curpressure = pt->pressure;
			glLineWidth(curpressure * thickness);
			glBegin(GL_LINE_STRIP);
			
			/* need to roll-back one point to ensure that there are no gaps in the stroke */
			if (i != 0) glVertex3fv(&(pt - 1)->x);
			
			/* now the point we want... */
			glVertex3fv(&pt->x);
		}
		else {
			glVertex3fv(&pt->x);
		}
	}
	glEnd();
	
	/* draw debug points of curve on top? */
	/* XXX: for now, we represent "selected" strokes in the same way as debug, which isn't used anymore */
	if (debug) {
		glBegin(GL_POINTS);
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++)
			glVertex3fv(&pt->x);
		glEnd();
	}
}

/* ----- Fancy 2D-Stroke Drawing ------ */

/* draw a given stroke in 2d */
static void gp_draw_stroke_2d(bGPDspoint *points, int totpoints, short thickness_s, short dflag, short sflag,
                              bool debug, int offsx, int offsy, int winx, int winy)
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
		float pm[2];
		int i;
		
		glShadeModel(GL_FLAT);
		glBegin(GL_QUADS);
		
		for (i = 0, pt1 = points, pt2 = points + 1; i < (totpoints - 1); i++, pt1++, pt2++) {
			float s0[2], s1[2];     /* segment 'center' points */
			float t0[2], t1[2];     /* tessellated coordinates */
			float m1[2], m2[2];     /* gradient and normal */
			float mt[2], sc[2];     /* gradient for thickness, point for end-cap */
			float pthick;           /* thickness at segment point */
			
			/* get x and y coordinates from points */
			gp_calc_2d_stroke_xy(pt1, sflag, offsx, offsy, winx, winy, s0);
			gp_calc_2d_stroke_xy(pt2, sflag, offsx, offsy, winx, winy, s1);
			
			/* calculate gradient and normal - 'angle'=(ny/nx) */
			m1[1] = s1[1] - s0[1];
			m1[0] = s1[0] - s0[0];
			normalize_v2(m1);
			m2[1] = -m1[0];
			m2[0] = m1[1];
			
			/* always use pressure from first point here */
			pthick = (pt1->pressure * thickness * scalefac);
			
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
			
			/* store stroke's 'natural' normal for next stroke to use */
			copy_v2_v2(pm, m2);
		}
		
		glEnd();
	}
	
	/* draw debug points of curve on top? (original stroke points) */
	if (debug) {
		bGPDspoint *pt;
		int i;
		
		glBegin(GL_POINTS);
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
			float co[2];
			
			gp_calc_2d_stroke_xy(pt, sflag, offsx, offsy, winx, winy, co);
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
static void gp_draw_strokes(bGPDframe *gpf, int offsx, int offsy, int winx, int winy, int dflag,
                            bool debug, short lthick, const float color[4], const float fill_color[4])
{
	bGPDstroke *gps;
	
	for (gps = gpf->strokes.first; gps; gps = gps->next) {
		/* check if stroke can be drawn */
		if (gp_can_draw_stroke(gps, dflag) == false)
			continue;
		
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
			if ((dflag & GP_DRAWDATA_FILL) && (gps->totpoints >= 3)) {
				glColor4fv(fill_color);
				gp_draw_stroke_fill(gps->points, gps->totpoints, lthick, dflag, gps->flag, offsx, offsy, winx, winy);
			}
			
			/* 3D Stroke */
			glColor4fv(color);
			
			if (dflag & GP_DRAWDATA_VOLUMETRIC) {
				/* volumetric stroke drawing */
				gp_draw_stroke_volumetric_3d(gps->points, gps->totpoints, lthick, dflag, gps->flag);
			}
			else {
				/* 3D Lines - OpenGL primitives-based */
				if (gps->totpoints == 1) {
					gp_draw_stroke_point(gps->points, lthick, dflag, gps->flag, offsx, offsy, winx, winy);
				}
				else {
					gp_draw_stroke_3d(gps->points, gps->totpoints, lthick, debug, gps->flag);
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
			if ((dflag & GP_DRAWDATA_FILL) && (gps->totpoints >= 3)) {
				glColor4fv(fill_color);
				gp_draw_stroke_fill(gps->points, gps->totpoints, lthick, dflag, gps->flag, offsx, offsy, winx, winy);
			}
			
			/* 2D Strokes... */
			glColor4fv(color);
			
			if (dflag & GP_DRAWDATA_VOLUMETRIC) {
				/* blob/disk-based "volumetric" drawing */
				gp_draw_stroke_volumetric_2d(gps->points, gps->totpoints, lthick, dflag, gps->flag, offsx, offsy, winx, winy);
			}
			else {
				/* normal 2D strokes */
				if (gps->totpoints == 1) {
					gp_draw_stroke_point(gps->points, lthick, dflag, gps->flag, offsx, offsy, winx, winy);
				}
				else {
					gp_draw_stroke_2d(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, offsx, offsy, winx, winy);
				}
			}
		}
	}
}

/* Draw selected verts for strokes being edited */
static void gp_draw_strokes_edit(bGPDframe *gpf, int offsx, int offsy, int winx, int winy, short dflag, const float tcolor[3])
{
	bGPDstroke *gps;
	
	const int no_xray = (dflag & GP_DRAWDATA_NO_XRAY);
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
		
		/* check if stroke can be drawn */
		if (gp_can_draw_stroke(gps, dflag) == false)
			continue;
		
		/* Optimisation: only draw points for selected strokes
		 * We assume that selected points can only occur in
		 * strokes that are selected too.
		 */
		if ((gps->flag & GP_STROKE_SELECT) == 0)
			continue;
			
		/* Get size of verts:
		 * - The selected state needs to be larger than the unselected state so that
		 *   they stand out more.
		 * - We use the theme setting for size of the unselected verts
		 */
		bsize = UI_GetThemeValuef(TH_VERTEX_SIZE);
		if ((int)bsize > 8) {
			vsize = 10.0f;
			bsize = 8.0f;
		}
		else {
			vsize = bsize + 2;
		}
		
		/* First Pass: Draw all the verts (i.e. these become the unselected state) */
		if (tcolor != NULL) {
			/* for now, we assume that the base color of the points is not too close to the real color */
			glColor3fv(tcolor);
		}
		else {
			/* this doesn't work well with the default theme and black strokes... */
			UI_ThemeColor(TH_VERTEX);
		}
		glPointSize(bsize);
		
		glBegin(GL_POINTS);
		for (i = 0, pt = gps->points; i < gps->totpoints && pt; i++, pt++) {
			if (gps->flag & GP_STROKE_3DSPACE) {
				glVertex3fv(&pt->x);
			}
			else {
				float co[2];
				
				gp_calc_2d_stroke_xy(pt, gps->flag, offsx, offsy, winx, winy, co);
				glVertex2fv(co);
			}
		}
		glEnd();
		
		
		/* Second Pass: Draw only verts which are selected */
		UI_ThemeColor(TH_VERTEX_SELECT);
		glPointSize(vsize);
		
		glBegin(GL_POINTS);
		for (i = 0, pt = gps->points; i < gps->totpoints && pt; i++, pt++) {
			if (pt->flag & GP_SPOINT_SELECT) {
				if (gps->flag & GP_STROKE_3DSPACE) {
					glVertex3fv(&pt->x);
				}
				else {
					float co[2];
					
					gp_calc_2d_stroke_xy(pt, gps->flag, offsx, offsy, winx, winy, co);
					glVertex2fv(co);
				}
			}
		}
		glEnd();
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
static void gp_draw_onionskins(bGPDlayer *gpl, bGPDframe *gpf, int offsx, int offsy, int winx, int winy, 
                               int UNUSED(cfra), int dflag, short debug, short lthick)
{
	const float alpha = gpl->color[3];
	float color[4];
	
	/* 1) Draw Previous Frames First */
	if (gpl->flag & GP_LAYER_GHOST_PREVCOL) {
		copy_v3_v3(color, gpl->gcolor_prev);
	}
	else {
		copy_v3_v3(color, gpl->color);
	}
	
	if (gpl->gstep) {
		bGPDframe *gf;
		float fac;
		
		/* draw previous frames first */
		for (gf = gpf->prev; gf; gf = gf->prev) {
			/* check if frame is drawable */
			if ((gpf->framenum - gf->framenum) <= gpl->gstep) {
				/* alpha decreases with distance from curframe index */
				fac = 1.0f - ((float)(gpf->framenum - gf->framenum) / (float)(gpl->gstep + 1));
				color[3] = alpha * fac * 0.66f;
				gp_draw_strokes(gf, offsx, offsy, winx, winy, dflag, debug, lthick, color, color);
			}
			else 
				break;
		}
	}
	else {
		/* draw the strokes for the ghost frames (at half of the alpha set by user) */
		if (gpf->prev) {
			color[3] = (alpha / 7);
			gp_draw_strokes(gpf->prev, offsx, offsy, winx, winy, dflag, debug, lthick, color, color);
		}
	}
	
	
	/* 2) Now draw next frames */
	if (gpl->flag & GP_LAYER_GHOST_NEXTCOL) {
		copy_v3_v3(color, gpl->gcolor_next);
	}
	else {
		copy_v3_v3(color, gpl->color);
	}
	
	if (gpl->gstep_next) {
		bGPDframe *gf;
		float fac;
		
		/* now draw next frames */
		for (gf = gpf->next; gf; gf = gf->next) {
			/* check if frame is drawable */
			if ((gf->framenum - gpf->framenum) <= gpl->gstep_next) {
				/* alpha decreases with distance from curframe index */
				fac = 1.0f - ((float)(gf->framenum - gpf->framenum) / (float)(gpl->gstep_next + 1));
				color[3] = alpha * fac * 0.66f;
				gp_draw_strokes(gf, offsx, offsy, winx, winy, dflag, debug, lthick, color, color);
			}
			else 
				break;
		}
	}
	else {
		/* draw the strokes for the ghost frames (at half of the alpha set by user) */
		if (gpf->next) {
			color[3] = (alpha / 4);
			gp_draw_strokes(gpf->next, offsx, offsy, winx, winy, dflag, debug, lthick, color, color);
		}
	}
	
	/* 3) restore alpha */
	glColor4fv(gpl->color);
}

/* draw grease-pencil datablock */
static void gp_draw_data(bGPdata *gpd, int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
	bGPDlayer *gpl;
	
	/* reset line drawing style (in case previous user didn't reset) */
	setlinestyle(0);
	
	/* turn on smooth lines (i.e. anti-aliasing) */
	glEnable(GL_LINE_SMOOTH);
	
	glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	
	/* turn on alpha-blending */
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
		
	/* loop over layers, drawing them */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		bGPDframe *gpf;
		
		bool debug = (gpl->flag & GP_LAYER_DRAWDEBUG) ? true : false;
		short lthick = gpl->thickness;
		
		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE) 
			continue;
		
		/* get frame to draw */
		gpf = gpencil_layer_getframe(gpl, cfra, 0);
		if (gpf == NULL) 
			continue;
		
		/* set color, stroke thickness, and point size */
		glLineWidth(lthick);
		glPointSize((float)(gpl->thickness + 2));
		
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
		
		/* fill strokes... */
		// XXX: this is not a very good limit
		GP_DRAWFLAG_APPLY((gpl->fill[3] > 0.001f), GP_DRAWDATA_FILL);
#undef GP_DRAWFLAG_APPLY
		
		/* draw 'onionskins' (frame left + right) */
		if ((gpl->flag & GP_LAYER_ONIONSKIN) && !(dflag & GP_DRAWDATA_NO_ONIONS)) {
			/* Drawing method - only immediately surrounding (gstep = 0),
			 * or within a frame range on either side (gstep > 0)
			 */
			gp_draw_onionskins(gpl, gpf, offsx, offsy, winx, winy, cfra, dflag, debug, lthick);
		}
		
		/* draw the strokes already in active frame */
		gp_draw_strokes(gpf, offsx, offsy, winx, winy, dflag, debug, lthick, gpl->color, gpl->fill);
		
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
			gp_draw_strokes_edit(gpf, offsx, offsy, winx, winy, dflag, 
			                     (gpl->color[3] < 0.95f) ? gpl->color : NULL);
		}
		
		/* Check if may need to draw the active stroke cache, only if this layer is the active layer
		 * that is being edited. (Stroke buffer is currently stored in gp-data)
		 */
		if (ED_gpencil_session_active() && (gpl->flag & GP_LAYER_ACTIVE) &&
		    (gpf->flag & GP_FRAME_PAINT))
		{
			/* Set color for drawing buffer stroke - since this may not be set yet */
			glColor4fv(gpl->color);
			
			/* Buffer stroke needs to be drawn with a different linestyle
			 * to help differentiate them from normal strokes.
			 * 
			 * It should also be noted that sbuffer contains temporary point types
			 * i.e. tGPspoints NOT bGPDspoints
			 */
			if (gpl->flag & GP_LAYER_VOLUMETRIC) {
				gp_draw_stroke_volumetric_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag);
			}
			else {
				gp_draw_stroke_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag);
			}
		}
	}
	
	/* turn off alpha blending, then smooth lines */
	glDisable(GL_BLEND); // alpha blending
	glDisable(GL_LINE_SMOOTH); // smooth lines
	glDisable(GL_POLYGON_SMOOTH); // smooth poly lines
		
	/* restore initial gl conditions */
	glLineWidth(1.0);
	glPointSize(1.0);
	glColor4f(0, 0, 0, 1);
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
	
	
	/* draw it! */
	gp_draw_data(gpd, offsx, offsy, sizex, sizey, CFRA, dflag);
}

/* draw grease-pencil sketches to specified 2d-view assuming that matrices are already set correctly 
 * Note: this gets called twice - first time with onlyv2d=1 to draw 'canvas' strokes,
 * second time with onlyv2d=0 for screen-aligned strokes */
void ED_gpencil_draw_view2d(const bContext *C, bool onlyv2d)
{
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
	gp_draw_data(gpd, 0, 0, ar->winx, ar->winy, CFRA, dflag);
}

/* draw grease-pencil sketches to specified 3d-view assuming that matrices are already set correctly 
 * Note: this gets called twice - first time with only3d=1 to draw 3d-strokes,
 * second time with only3d=0 for screen-aligned strokes */
void ED_gpencil_draw_view3d(Scene *scene, View3D *v3d, ARegion *ar, bool only3d)
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

		offsx = iroundf(rectf.xmin);
		offsy = iroundf(rectf.ymin);
		winx  = iroundf(rectf.xmax - rectf.xmin);
		winy  = iroundf(rectf.ymax - rectf.ymin);
	}
	else {
		offsx = 0;
		offsy = 0;
		winx  = ar->winx;
		winy  = ar->winy;
	}
	
	/* draw it! */
	if (only3d) dflag |= (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_NOSTATUS);

	gp_draw_data(gpd, offsx, offsy, winx, winy, CFRA, dflag);
}

void ED_gpencil_draw_ex(bGPdata *gpd, int winx, int winy, const int cfra)
{
	int dflag = GP_DRAWDATA_NOSTATUS | GP_DRAWDATA_ONLYV2D;

	gp_draw_data(gpd, 0, 0, winx, winy, cfra, dflag);
}

/* ************************************************** */
