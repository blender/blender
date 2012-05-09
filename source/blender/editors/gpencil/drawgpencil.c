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

#include "BLO_sys_types.h"

#include "IMB_imbuf_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"



#include "WM_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_gpencil.h"
#include "ED_sequencer.h"
#include "ED_view3d.h"


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

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void gp_draw_stroke_point(bGPDspoint *points, short thickness, short dflag, short sflag, int offsx, int offsy, int winx, int winy)
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
		if (sflag & GP_STROKE_2DSPACE) {
			co[0] = points->x;
			co[1] = points->y;
		}
		else if (sflag & GP_STROKE_2DIMAGE) {
			co[0] = (points->x * winx) + offsx;
			co[1] = (points->y * winy) + offsy;
		}
		else {
			co[0] = (points->x / 100 * winx) + offsx;
			co[1] = (points->y / 100 * winy) + offsy;
		}
		
		/* if thickness is less than GP_DRAWTHICKNESS_SPECIAL, simple dot looks ok
		 *  - also mandatory in if Image Editor 'image-based' dot
		 */
		if ( (thickness < GP_DRAWTHICKNESS_SPECIAL) ||
		     ((dflag & GP_DRAWDATA_IEDITHACK) && (sflag & GP_STROKE_2DSPACE)) )
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
static void gp_draw_stroke_3d(bGPDspoint *points, int totpoints, short thickness, short debug)
{
	bGPDspoint *pt;
	float oldpressure = 0.0f;
	int i;
	
	/* draw stroke curve */
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
			if (i != 0) glVertex3fv(&(pt - 1)->x);

			/* now the point we want... */
			glVertex3fv(&pt->x);
			
			oldpressure = pt->pressure;
		}
		else {
			glVertex3fv(&pt->x);
		}
	}
	glEnd();
	
	/* draw debug points of curve on top? */
	if (debug) {
		glBegin(GL_POINTS);
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++)
			glVertex3fv(&pt->x);
		glEnd();
	}
}

/* ----- Fancy 2D-Stroke Drawing ------ */

/* draw a given stroke in 2d */
static void gp_draw_stroke(bGPDspoint *points, int totpoints, short thickness_s, short dflag, short sflag,
                           short debug, int offsx, int offsy, int winx, int winy)
{
	/* otherwise thickness is twice that of the 3D view */
	float thickness = (float)thickness_s * 0.5f;

	/* if thickness is less than GP_DRAWTHICKNESS_SPECIAL, 'smooth' opengl lines look better
	 *  - 'smooth' opengl lines are also required if Image Editor 'image-based' stroke
	 */
	if ( (thickness < GP_DRAWTHICKNESS_SPECIAL) || 
	     ((dflag & GP_DRAWDATA_IEDITHACK) && (dflag & GP_DRAWDATA_ONLYV2D)) )
	{
		bGPDspoint *pt;
		int i;
		
		glBegin(GL_LINE_STRIP);
		for (i = 0, pt = points; i < totpoints && pt; i++, pt++) {
			if (sflag & GP_STROKE_2DSPACE) {
				glVertex2f(pt->x, pt->y);
			}
			else if (sflag & GP_STROKE_2DIMAGE) {
				const float x = (pt->x * winx) + offsx;
				const float y = (pt->y * winy) + offsy;
				
				glVertex2f(x, y);
			}
			else {
				const float x = (pt->x / 100 * winx) + offsx;
				const float y = (pt->y / 100 * winy) + offsy;
				
				glVertex2f(x, y);
			}
		}
		glEnd();
	}
	
	/* tessellation code - draw stroke as series of connected quads with connection
	 * edges rotated to minimise shrinking artifacts, and rounded endcaps
	 */
	else {
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
			if (sflag & GP_STROKE_2DSPACE) {
				s0[0] = pt1->x;      s0[1] = pt1->y;
				s1[0] = pt2->x;      s1[1] = pt2->y;
			}
			else if (sflag & GP_STROKE_2DIMAGE) {
				s0[0] = (pt1->x * winx) + offsx;
				s0[1] = (pt1->y * winy) + offsy;
				s1[0] = (pt2->x * winx) + offsx;
				s1[1] = (pt2->y * winy) + offsy;
			}
			else {
				s0[0] = (pt1->x / 100 * winx) + offsx;
				s0[1] = (pt1->y / 100 * winy) + offsy;
				s1[0] = (pt2->x / 100 * winx) + offsx;
				s1[1] = (pt2->y / 100 * winy) + offsy;
			}		
			
			/* calculate gradient and normal - 'angle'=(ny/nx) */
			m1[1] = s1[1] - s0[1];
			m1[0] = s1[0] - s0[0];
			normalize_v2(m1);
			m2[1] = -m1[0];
			m2[0] = m1[1];
			
			/* always use pressure from first point here */
			pthick = (pt1->pressure * thickness);
			
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
				pthick = (pt2->pressure * thickness);
				
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
			if (sflag & GP_STROKE_2DSPACE) {
				glVertex2fv(&pt->x);
			}
			else if (sflag & GP_STROKE_2DIMAGE) {
				const float x = (float)((pt->x * winx) + offsx);
				const float y = (float)((pt->y * winy) + offsy);
				
				glVertex2f(x, y);
			}
			else {
				const float x = (float)(pt->x / 100 * winx) + offsx;
				const float y = (float)(pt->y / 100 * winy) + offsy;
				
				glVertex2f(x, y);
			}
		}
		glEnd();
	}
}

/* ----- General Drawing ------ */

/* draw a set of strokes */
static void gp_draw_strokes(bGPDframe *gpf, int offsx, int offsy, int winx, int winy, int dflag,
                            short debug, short lthick, float color[4])
{
	bGPDstroke *gps;
	
	/* set color first (may need to reset it again later too) */
	glColor4fv(color);
	
	for (gps = gpf->strokes.first; gps; gps = gps->next) {
		/* check if stroke can be drawn - checks here generally fall into pairs */
		if ((dflag & GP_DRAWDATA_ONLY3D) && !(gps->flag & GP_STROKE_3DSPACE))
			continue;
		if (!(dflag & GP_DRAWDATA_ONLY3D) && (gps->flag & GP_STROKE_3DSPACE))
			continue;
		if ((dflag & GP_DRAWDATA_ONLYV2D) && !(gps->flag & GP_STROKE_2DSPACE))
			continue;
		if (!(dflag & GP_DRAWDATA_ONLYV2D) && (gps->flag & GP_STROKE_2DSPACE))
			continue;
		if ((dflag & GP_DRAWDATA_ONLYI2D) && !(gps->flag & GP_STROKE_2DIMAGE))
			continue;
		if (!(dflag & GP_DRAWDATA_ONLYI2D) && (gps->flag & GP_STROKE_2DIMAGE))
			continue;
		if ((gps->points == NULL) || (gps->totpoints < 1))
			continue;
		
		/* check which stroke-drawer to use */
		if (gps->totpoints == 1)
			gp_draw_stroke_point(gps->points, lthick, dflag, gps->flag, offsx, offsy, winx, winy);
		else if (dflag & GP_DRAWDATA_ONLY3D) {
			const int no_xray = (dflag & GP_DRAWDATA_NO_XRAY);
			int mask_orig = 0;
			
			if (no_xray) {
				glGetIntegerv(GL_DEPTH_WRITEMASK, &mask_orig);
				glDepthMask(0);
				glEnable(GL_DEPTH_TEST);
				
				/* first arg is normally rv3d->dist, but this isn't available here and seems to work quite well without */
				bglPolygonOffset(1.0f, 1.0f);
#if 0
				glEnable(GL_POLYGON_OFFSET_LINE);
				glPolygonOffset(-1.0f, -1.0f);
#endif
			}
			
			gp_draw_stroke_3d(gps->points, gps->totpoints, lthick, debug);
			
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
		else if (gps->totpoints > 1)	
			gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, offsx, offsy, winx, winy);
	}
}

/* draw grease-pencil datablock */
static void gp_draw_data(bGPdata *gpd, int offsx, int offsy, int winx, int winy, int cfra, int dflag)
{
	bGPDlayer *gpl;
	
	/* reset line drawing style (in case previous user didn't reset) */
	setlinestyle(0);
	
	/* turn on smooth lines (i.e. anti-aliasing) */
	glEnable(GL_LINE_SMOOTH);
	
	/* turn on alpha-blending */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
		
	/* loop over layers, drawing them */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		bGPDframe *gpf;
		
		short debug = (gpl->flag & GP_LAYER_DRAWDEBUG) ? 1 : 0;
		short lthick = gpl->thickness;
		float color[4], tcolor[4];
		
		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE) 
			continue;
		
		/* get frame to draw */
		gpf = gpencil_layer_getframe(gpl, cfra, 0);
		if (gpf == NULL) 
			continue;
		
		/* set color, stroke thickness, and point size */
		glLineWidth(lthick);
		copy_v4_v4(color, gpl->color); // just for copying 4 array elements
		copy_v4_v4(tcolor, gpl->color); // additional copy of color (for ghosting)
		glColor4fv(color);
		glPointSize((float)(gpl->thickness + 2));
		
		/* apply xray layer setting */
		if (gpl->flag & GP_LAYER_NO_XRAY) dflag |=  GP_DRAWDATA_NO_XRAY;
		else dflag &= ~GP_DRAWDATA_NO_XRAY;
		
		/* draw 'onionskins' (frame left + right) */
		if (gpl->flag & GP_LAYER_ONIONSKIN) {
			/* drawing method - only immediately surrounding (gstep = 0), or within a frame range on either side (gstep > 0)*/			
			if (gpl->gstep) {
				bGPDframe *gf;
				float fac;
				
				/* draw previous frames first */
				for (gf = gpf->prev; gf; gf = gf->prev) {
					/* check if frame is drawable */
					if ((gpf->framenum - gf->framenum) <= gpl->gstep) {
						/* alpha decreases with distance from curframe index */
						fac = 1.0f - ((float)(gpf->framenum - gf->framenum) / (float)(gpl->gstep + 1));
						tcolor[3] = color[3] * fac * 0.66f;
						gp_draw_strokes(gf, offsx, offsy, winx, winy, dflag, debug, lthick, tcolor);
					}
					else 
						break;
				}
				
				/* now draw next frames */
				for (gf = gpf->next; gf; gf = gf->next) {
					/* check if frame is drawable */
					if ((gf->framenum - gpf->framenum) <= gpl->gstep) {
						/* alpha decreases with distance from curframe index */
						fac = 1.0f - ((float)(gf->framenum - gpf->framenum) / (float)(gpl->gstep + 1));
						tcolor[3] = color[3] * fac * 0.66f;
						gp_draw_strokes(gf, offsx, offsy, winx, winy, dflag, debug, lthick, tcolor);
					}
					else 
						break;
				}	
				
				/* restore alpha */
				glColor4fv(color);
			}
			else {
				/* draw the strokes for the ghost frames (at half of the alpha set by user) */
				if (gpf->prev) {
					tcolor[3] = (color[3] / 7);
					gp_draw_strokes(gpf->prev, offsx, offsy, winx, winy, dflag, debug, lthick, tcolor);
				}
				
				if (gpf->next) {
					tcolor[3] = (color[3] / 4);
					gp_draw_strokes(gpf->next, offsx, offsy, winx, winy, dflag, debug, lthick, tcolor);
				}
				
				/* restore alpha */
				glColor4fv(color);
			}
		}
		
		/* draw the strokes already in active frame */
		tcolor[3] = color[3];
		gp_draw_strokes(gpf, offsx, offsy, winx, winy, dflag, debug, lthick, tcolor);
		
		/* Check if may need to draw the active stroke cache, only if this layer is the active layer
		 * that is being edited. (Stroke buffer is currently stored in gp-data)
		 */
		if (ED_gpencil_session_active() && (gpl->flag & GP_LAYER_ACTIVE) &&
		    (gpf->flag & GP_FRAME_PAINT))
		{
			/* Buffer stroke needs to be drawn with a different linestyle to help differentiate them from normal strokes. */
			gp_draw_stroke_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag);
		}
	}
	
	/* turn off alpha blending, then smooth lines */
	glDisable(GL_BLEND); // alpha blending
	glDisable(GL_LINE_SMOOTH); // smooth lines
		
	/* restore initial gl conditions */
	glLineWidth(1.0);
	glPointSize(1.0);
	glColor4f(0, 0, 0, 1);
}

/* ----- Grease Pencil Sketches Drawing API ------ */

// ............................
// XXX 
//	We need to review the calls below, since they may be/are not that suitable for
//	the new ways that we intend to be drawing data...
// ............................

/* draw grease-pencil sketches to specified 2d-view that uses ibuf corrections */
void draw_gpencil_2dimage(bContext *C, ImBuf *ibuf)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd;
	int offsx, offsy, sizex, sizey;
	int dflag = GP_DRAWDATA_NOSTATUS;
	
	/* check that we have grease-pencil stuff to draw */
	if (ELEM(NULL, sa, ibuf)) return;
	gpd = gpencil_data_get_active(C); // XXX
	if (gpd == NULL) return;
	
	/* calculate rect */
	switch (sa->spacetype) {
		case SPACE_IMAGE: /* image */
		case SPACE_CLIP: /* clip */
		{
			
			/* just draw using standard scaling (settings here are currently ignored anyways) */
			// FIXME: the opengl poly-strokes don't draw at right thickness when done this way, so disabled
			offsx = 0;
			offsy = 0;
			sizex = ar->winx;
			sizey = ar->winy;
			
			wmOrtho2(ar->v2d.cur.xmin, ar->v2d.cur.xmax, ar->v2d.cur.ymin, ar->v2d.cur.ymax);
			
			dflag |= GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_IEDITHACK;
		}
		break;
#if 0   /* removed since 2.5x, needs to be added back */
		case SPACE_SEQ: /* sequence */
		{
			SpaceSeq *sseq = (SpaceSeq *)sa->spacedata.first;
			float zoom, zoomx, zoomy;
			
			/* calculate accessory values */
			zoom = (float)(SEQ_ZOOM_FAC(sseq->zoom));
			if (sseq->mainb == SEQ_DRAW_IMG_IMBUF) {
				/* XXX sequencer zoom should store it? */
				zoomx = zoom; //  * (G.scene->r.xasp / G.scene->r.yasp);
				zoomy = zoom;
			} 
			else
				zoomx = zoomy = zoom;
			
			/* calculate transforms (Note: we use ibuf here, as we have it) */
			sizex = (int)(zoomx * ibuf->x);
			sizey = (int)(zoomy * ibuf->y);
			offsx = (int)( (ar->winx - sizex) / 2 + sseq->xof);
			offsy = (int)( (ar->winy - sizey) / 2 + sseq->yof);
			
			dflag |= GP_DRAWDATA_ONLYI2D;
		}
		break;
#endif
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
 * Note: this gets called twice - first time with onlyv2d=1 to draw 'canvas' strokes, second time with onlyv2d=0 for screen-aligned strokes
 */
void draw_gpencil_view2d(bContext *C, short onlyv2d)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	bGPdata *gpd;
	int dflag = 0;
	
	/* check that we have grease-pencil stuff to draw */
	if (sa == NULL) return;
	gpd = gpencil_data_get_active(C); // XXX
	if (gpd == NULL) return;
	
	/* special hack for Image Editor */
	// FIXME: the opengl poly-strokes don't draw at right thickness when done this way, so disabled
	if (ELEM(sa->spacetype, SPACE_IMAGE, SPACE_CLIP))
		dflag |= GP_DRAWDATA_IEDITHACK;
	
	/* draw it! */
	if (onlyv2d) dflag |= (GP_DRAWDATA_ONLYV2D | GP_DRAWDATA_NOSTATUS);
	gp_draw_data(gpd, 0, 0, ar->winx, ar->winy, CFRA, dflag);
}

/* draw grease-pencil sketches to specified 3d-view assuming that matrices are already set correctly 
 * Note: this gets called twice - first time with only3d=1 to draw 3d-strokes, second time with only3d=0 for screen-aligned strokes
 */

void draw_gpencil_view3d(Scene *scene, View3D *v3d, ARegion *ar, short only3d)
{
	bGPdata *gpd;
	int dflag = 0;
	rcti rect;
	RegionView3D *rv3d = ar->regiondata;

	/* check that we have grease-pencil stuff to draw */
	gpd = gpencil_data_get_active_v3d(scene); // XXX
	if (gpd == NULL) return;

	/* when rendering to the offscreen buffer we don't want to
	 * deal with the camera border, otherwise map the coords to the camera border. */
	if ((rv3d->persp == RV3D_CAMOB) && !(G.f & G_RENDER_OGL)) {
		rctf rectf;
		ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &rectf, TRUE); /* no shift */
		BLI_copy_rcti_rctf(&rect, &rectf);
	}
	else {
		rect.xmin = 0;
		rect.ymin = 0;
		rect.xmax = ar->winx;
		rect.ymax = ar->winy;
	}
	
	/* draw it! */
	if (only3d) dflag |= (GP_DRAWDATA_ONLY3D | GP_DRAWDATA_NOSTATUS);

	gp_draw_data(gpd, rect.xmin, rect.ymin, rect.xmax, rect.ymax, CFRA, dflag);
}

/* ************************************************** */
