/**
 * $Id$
 *
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
 * The Original Code is Copyright (C) Blender Foundation
 *
 * Contributor(s): Joshua Leung (2009 Recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <float.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_screen.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_anim_api.h"

#include "graph_intern.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

/* *************************** */
/* Utility Drawing Defines */

/* determine the alpha value that should be used when 
 * drawing components for some F-Curve (fcu)
 *	- selected F-Curves should be more visible than partially visible ones
 */
#define drawFCurveFade(fcu) ( ((fcu)->flag & FCURVE_SELECTED)? 1.0f : 0.25f )

/* set the colour for some point from some value given packed into an int 
 *	- intV: integer value containing color info packed into an int
 *	- alpha: float value describing the 
 */
#define cpackA(intVC, alpha) \
	{ \
		float _cpackCol[3]; \
		cpack_to_rgb(intVC, &_cpackCol[0], &_cpackCol[1], &_cpackCol[2]); \
		glColor4f(_cpackCol[0], _cpackCol[1], _cpackCol[2], alpha); \
	}

/* *************************** */
/* F-Curve Modifier Drawing */

/* Envelope -------------- */

// TODO: draw a shaded poly showing the region of influence too!!!
static void draw_fcurve_modifier_controls_envelope (FCurve *fcu, FModifier *fcm, View2D *v2d)
{
	FMod_Envelope *env= (FMod_Envelope *)fcm->data;
	FCM_EnvelopeData *fed;
	const float fac= 0.05f * (v2d->cur.xmax - v2d->cur.xmin);
	int i;
	
	/* draw two black lines showing the standard reference levels */
	glColor3f(0.0f, 0.0f, 0.0f);
	setlinestyle(5);
	
	glBegin(GL_LINES);
		glVertex2f(v2d->cur.xmin, env->midval+env->min);
		glVertex2f(v2d->cur.xmax, env->midval+env->min);
		
		glVertex2f(v2d->cur.xmin, env->midval+env->max);
		glVertex2f(v2d->cur.xmax, env->midval+env->max);
	glEnd(); // GL_LINES
	setlinestyle(0);
	
	/* set size of vertices (non-adjustable for now) */
	glPointSize(2.0f);
	
	// for now, point color is fixed, and is white
	glColor3f(1.0f, 1.0f, 1.0f);
	
	/* we use bgl points not standard gl points, to workaround vertex 
	 * drawing bugs that some drivers have (probably legacy ones only though)
	 */
	bglBegin(GL_POINTS);
	for (i=0, fed=env->data; i < env->totvert; i++, fed++) {
		/* only draw if visible
		 *	- min/max here are fixed, not relative
		 */
		if IN_RANGE(fed->time, (v2d->cur.xmin - fac), (v2d->cur.xmax + fac)) {
			glVertex2f(fed->time, fed->min);
			glVertex2f(fed->time, fed->max);
		}
	}
	bglEnd(); // GL_POINTS
	
	glPointSize(1.0f);
}

/* *************************** */
/* F-Curve Drawing */

/* Points ---------------- */

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_vertices_keyframes (bAnimContext *ac, FCurve *fcu, View2D *v2d, short edit, short sel)
{
	BezTriple *bezt= fcu->bezt;
	const float fac= 0.05f * (v2d->cur.xmax - v2d->cur.xmin);
	int i;
	
	/* we use bgl points not standard gl points, to workaround vertex 
	 * drawing bugs that some drivers have (probably legacy ones only though)
	 */
	bglBegin(GL_POINTS);
	
	for (i = 0; i < fcu->totvert; i++, bezt++) {
		/* as an optimisation step, only draw those in view 
		 *	- we apply a correction factor to ensure that points don't pop in/out due to slight twitches of view size
		 */
		if IN_RANGE(bezt->vec[1][0], (v2d->cur.xmin - fac), (v2d->cur.xmax + fac)) {
			if (edit) {
				/* 'Keyframe' vertex only, as handle lines and handles have already been drawn
				 *	- only draw those with correct selection state for the current drawing color
				 *	- 
				 */
				if ((bezt->f2 & SELECT) == sel)
					bglVertex3fv(bezt->vec[1]);
			}
			else {
				/* no check for selection here, as curve is not editable... */
				// XXX perhaps we don't want to even draw points?   maybe add an option for that later
				bglVertex3fv(bezt->vec[1]);
			}
		}
	}
	
	bglEnd(); // GL_POINTS
}


/* helper func - draw handle vertex for an F-Curve as a round unfilled circle 
 * NOTE: the caller MUST HAVE GL_LINE_SMOOTH & GL_BLEND ENABLED, otherwise, the controls don't 
 * have a consistent appearance (due to off-pixel alignments)...
 */
static void draw_fcurve_handle_control (float x, float y, float xscale, float yscale, float hsize)
{
	static GLuint displist=0;
	
	/* initialise round circle shape */
	if (displist == 0) {
		GLUquadricObj *qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE);
		
		qobj	= gluNewQuadric(); 
		gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
		gluDisk(qobj, 0,  0.7, 8, 1);
		gluDeleteQuadric(qobj);  
		
		glEndList();
	}
	
	/* adjust view transform before starting */
	glTranslatef(x, y, 0.0f);
	glScalef(1.0f/xscale*hsize, 1.0f/yscale*hsize, 1.0f);
	
	/* draw! */
	glCallList(displist);
	
	/* restore view transform */
	glScalef(xscale/hsize, yscale/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

/* helper func - draw handle vertices only for an F-Curve (if it is not protected) */
static void draw_fcurve_vertices_handles (bAnimContext *ac, SpaceIpo *sipo, FCurve *fcu, View2D *v2d, short sel)
{
	BezTriple *bezt= fcu->bezt;
	BezTriple *prevbezt = NULL;
	float hsize, xscale, yscale;
	int i;
	
	/* get view settings */
	hsize= UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE);
	UI_view2d_getscale(v2d, &xscale, &yscale);
	
	/* set handle color */
	if (sel) UI_ThemeColor(TH_HANDLE_VERTEX_SELECT);
	else UI_ThemeColor(TH_HANDLE_VERTEX);
	
	/* anti-aliased lines for more consistent appearance */
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	
	for (i=0; i < fcu->totvert; i++, prevbezt=bezt, bezt++) {
		/* Draw the editmode handles for a bezier curve (others don't have handles) 
		 * if their selection status matches the selection status we're drawing for
		 *	- first handle only if previous beztriple was bezier-mode
		 *	- second handle only if current beztriple is bezier-mode
		 *
		 * Also, need to take into account whether the keyframe was selected
		 * if a Graph Editor option to only show handles of selected keys is on.
		 */
		if ( !(sipo->flag & SIPO_SELVHANDLESONLY) || BEZSELECTED(bezt) ) {
			if ( (!prevbezt && (bezt->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) ) {
				if ((bezt->f1 & SELECT) == sel)/* && v2d->cur.xmin < bezt->vec[0][0] < v2d->cur.xmax)*/
					draw_fcurve_handle_control(bezt->vec[0][0], bezt->vec[0][1], xscale, yscale, hsize);
			}
			
			if (bezt->ipo==BEZT_IPO_BEZ) {
				if ((bezt->f3 & SELECT) == sel)/* && v2d->cur.xmin < bezt->vec[2][0] < v2d->cur.xmax)*/
					draw_fcurve_handle_control(bezt->vec[2][0], bezt->vec[2][1], xscale, yscale, hsize);
			}
		}
	}
	
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

/* helper func - set color to draw F-Curve data with */
static void set_fcurve_vertex_color (SpaceIpo *sipo, FCurve *fcu, short sel)
{
	/* Fade the 'intensity' of the vertices based on the selection of the curves too */
	int alphaOffset= (int)((drawFCurveFade(fcu) - 1.0f) * 255);
	
	/* Set color of curve vertex based on state of curve (i.e. 'Edit' Mode) */
	if ((fcu->flag & FCURVE_PROTECTED)==0) {
		/* Curve's points ARE BEING edited */
		if (sel) UI_ThemeColorShadeAlpha(TH_VERTEX_SELECT, 0, alphaOffset); 
		else UI_ThemeColorShadeAlpha(TH_VERTEX, 0, alphaOffset);
	} 
	else {
		/* Curve's points CANNOT BE edited */
		if (sel) UI_ThemeColorShadeAlpha(TH_TEXT_HI, 0, alphaOffset);
		else UI_ThemeColorShadeAlpha(TH_TEXT, 0, alphaOffset);
	}
}


static void draw_fcurve_vertices (bAnimContext *ac, SpaceIpo *sipo, ARegion *ar, FCurve *fcu, int do_handles)
{
	View2D *v2d= &ar->v2d;
	
	/* only draw points if curve is visible 
	 * 	- draw unselected points before selected points as separate passes to minimise color-changing overhead
	 *	   (XXX dunno if this is faster than drawing all in one pass though) 
	 * 	   and also to make sure in the case of overlapping points that the selected is always visible
	 *	- draw handles before keyframes, so that keyframes will overlap handles (keyframes are more important for users)
	 */
	
	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
	
	/* draw the two handles first (if they're shown, the curve doesn't have just a single keyframe, and the curve is being edited) */
	if (do_handles)
	{
		set_fcurve_vertex_color(sipo, fcu, 0);
		draw_fcurve_vertices_handles(ac, sipo, fcu, v2d, 0);
		
		set_fcurve_vertex_color(sipo, fcu, 1);
		draw_fcurve_vertices_handles(ac, sipo, fcu, v2d, 1);
	}
		
	/* draw keyframes over the handles */
	set_fcurve_vertex_color(sipo, fcu, 0);
	draw_fcurve_vertices_keyframes(ac, fcu, v2d, !(fcu->flag & FCURVE_PROTECTED), 0);
	
	set_fcurve_vertex_color(sipo, fcu, 1);
	draw_fcurve_vertices_keyframes(ac, fcu, v2d, !(fcu->flag & FCURVE_PROTECTED), 1);
	
	glPointSize(1.0f);
}

/* Handles ---------------- */

static int draw_fcurve_handles_check(SpaceIpo *sipo, FCurve *fcu)
{
	/* don't draw handle lines if handles are not to be shown */
	if (	(sipo->flag & SIPO_NOHANDLES) || /* handles shouldn't be shown anywhere */
			(fcu->flag & FCURVE_PROTECTED) || /* keyframes aren't editable */
			(fcu->flag & FCURVE_INT_VALUES) || /* editing the handles here will cause weird/incorrect interpolation issues */
			((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) || /* group that curve belongs to is not editable */
			(fcu->totvert <= 1) /* do not show handles if there is only 1 keyframe, otherwise they all clump together in an ugly ball */
		) 
	{
		return 0;
	} 
	else 
	{
		return 1;
	}
}

/* draw lines for F-Curve handles only (this is only done in EditMode)
 * note: draw_fcurve_handles_check must be checked before running this. */
static void draw_fcurve_handles (bAnimContext *ac, SpaceIpo *sipo, ARegion *ar, FCurve *fcu)
{
	int sel, b;
	
	/* a single call to GL_LINES here around these calls should be sufficient to still
	 * get separate line segments, but which aren't wrapped with GL_LINE_STRIP everytime we
	 * want a single line
	 */
	glBegin(GL_LINES);
	
	/* slightly hacky, but we want to draw unselected points before selected ones 
	 * so that selected points are clearly visible
	 */
	for (sel= 0; sel < 2; sel++) {
		BezTriple *bezt=fcu->bezt, *prevbezt=NULL;
		int basecol= (sel)? TH_HANDLE_SEL_FREE : TH_HANDLE_FREE;
		float *fp;
		char col[4];
		
		/* if only selected keyframes have handles shown, skip the first round */
		if ((sel == 0) && (sipo->flag & SIPO_SELVHANDLESONLY))
			continue;
		
		for (b= 0; b < fcu->totvert; b++, prevbezt=bezt, bezt++) {
			/* if only selected keyframes can get their handles shown, 
			 * check that keyframe is selected
			 */
			if (sipo->flag & SIPO_SELVHANDLESONLY) {
				if (BEZSELECTED(bezt) == 0)
					continue;
			}
			
			/* draw handle with appropriate set of colors if selection is ok */
			if ((bezt->f2 & SELECT)==sel) {
				fp= bezt->vec[0];

				/* only draw first handle if previous segment had handles */
				if ( (!prevbezt && (bezt->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) ) 
				{
					UI_GetThemeColor3ubv(basecol + bezt->h1, col);
					col[3]= drawFCurveFade(fcu) * 255;
					glColor4ubv((GLubyte *)col);
					
					glVertex2fv(fp); glVertex2fv(fp+3); 
				}

				/* only draw second handle if this segment is bezier */
				if (bezt->ipo == BEZT_IPO_BEZ) 
				{
					UI_GetThemeColor3ubv(basecol + bezt->h2, col);
					col[3]= drawFCurveFade(fcu) * 255;
					glColor4ubv((GLubyte *)col);

					glVertex2fv(fp+3); glVertex2fv(fp+6); 
				}
			}
			else {
				/* only draw first handle if previous segment was had handles, and selection is ok */
				if ( ((bezt->f1 & SELECT)==sel) && 
					 ( (!prevbezt && (bezt->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) ) ) 
				{
					fp= bezt->vec[0];
					UI_GetThemeColor3ubv(basecol + bezt->h1, col);
					col[3]= drawFCurveFade(fcu) * 255;
					glColor4ubv((GLubyte *)col);

					glVertex2fv(fp); glVertex2fv(fp+3); 
				}
				
				/* only draw second handle if this segment is bezier, and selection is ok */
				if ( ((bezt->f3 & SELECT)==sel) &&
					 (bezt->ipo == BEZT_IPO_BEZ) )
				{
					fp= bezt->vec[1];
					UI_GetThemeColor3ubv(basecol + bezt->h2, col);
					col[3]= drawFCurveFade(fcu) * 255;
					glColor4ubv((GLubyte *)col);

					glVertex2fv(fp); glVertex2fv(fp+3); 
				}
			}
		}
	}
	
	glEnd(); // GL_LINES 
}

/* Samples ---------------- */

/* helper func - draw sample-range marker for an F-Curve as a cross 
 * NOTE: the caller MUST HAVE GL_LINE_SMOOTH & GL_BLEND ENABLED, otherwise, the controls don't 
 * have a consistent appearance (due to off-pixel alignments)...
 */
static void draw_fcurve_sample_control (float x, float y, float xscale, float yscale, float hsize)
{
	static GLuint displist=0;
	
	/* initialise X shape */
	if (displist == 0) {
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE);
		
		glBegin(GL_LINES);
			glVertex2f(-0.7f, -0.7f);
			glVertex2f(+0.7f, +0.7f);
			
			glVertex2f(-0.7f, +0.7f);
			glVertex2f(+0.7f, -0.7f);
		glEnd(); // GL_LINES
		
		glEndList();
	}
	
	/* adjust view transform before starting */
	glTranslatef(x, y, 0.0f);
	glScalef(1.0f/xscale*hsize, 1.0f/yscale*hsize, 1.0f);
	
	/* draw! */
	glCallList(displist);
	
	/* restore view transform */
	glScalef(xscale/hsize, yscale/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_samples (bAnimContext *ac, SpaceIpo *sipo, ARegion *ar, FCurve *fcu)
{
	FPoint *first, *last;
	float hsize, xscale, yscale;
	
	/* get view settings */
	hsize= UI_GetThemeValuef(TH_VERTEX_SIZE);
	UI_view2d_getscale(&ar->v2d, &xscale, &yscale);
	
	/* set vertex color */
	if (fcu->flag & (FCURVE_ACTIVE|FCURVE_SELECTED)) UI_ThemeColor(TH_TEXT_HI);
	else UI_ThemeColor(TH_TEXT);
	
	/* get verts */
	first= fcu->fpt;
	last= (first) ? (first + (fcu->totvert-1)) : (NULL);
	
	/* draw */
	if (first && last) {
		/* anti-aliased lines for more consistent appearance */
		glEnable(GL_LINE_SMOOTH);
		glEnable(GL_BLEND);
		
		draw_fcurve_sample_control(first->vec[0], first->vec[1], xscale, yscale, hsize);
		draw_fcurve_sample_control(last->vec[0], last->vec[1], xscale, yscale, hsize);
		
		glDisable(GL_BLEND);
		glDisable(GL_LINE_SMOOTH);
	}
}

/* Curve ---------------- */

/* helper func - just draw the F-Curve by sampling the visible region (for drawing curves with modifiers) */
static void draw_fcurve_curve (bAnimContext *ac, ID *id, FCurve *fcu, SpaceIpo *sipo, View2D *v2d, View2DGrid *grid)
{
	ChannelDriver *driver;
	float samplefreq, ctime;
	float stime, etime;
	float unitFac;
	float dx, dy;

	/* when opening a blend file on a different sized screen or while dragging the toolbar this can happen
	 * best just bail out in this case */
	UI_view2d_grid_size(grid, &dx, &dy);
	if(dx <= 0.0f)
		return;


	/* disable any drivers temporarily */
	driver= fcu->driver;
	fcu->driver= NULL;
	
	/* compute unit correction factor */
	unitFac= ANIM_unit_mapping_get_factor(ac->scene, id, fcu, 0);
	
	/* Note about sampling frequency:
	 * 	Ideally, this is chosen such that we have 1-2 pixels = 1 segment
	 *	which means that our curves can be as smooth as possible. However,
	 * 	this does mean that curves may not be fully accurate (i.e. if they have
	 * 	sudden spikes which happen at the sampling point, we may have problems).
	 * 	Also, this may introduce lower performance on less densely detailed curves,'
	 *	though it is impossible to predict this from the modifiers!
	 *
	 *	If the automatically determined sampling frequency is likely to cause an infinite
	 *	loop (i.e. too close to 0), then clamp it to a determined "safe" value. The value
	 * 	chosen here is just the coarsest value which still looks reasonable...
	 */
		/* grid->dx represents the number of 'frames' between gridlines, but we divide by U.v2d_min_gridsize to get pixels-steps */
		// TODO: perhaps we should have 1.0 frames as upper limit so that curves don't get too distorted?
	samplefreq= dx / U.v2d_min_gridsize;
	if (samplefreq < 0.00001f) samplefreq= 0.00001f;
	
	
	/* the start/end times are simply the horizontal extents of the 'cur' rect */
	stime= v2d->cur.xmin;
	etime= v2d->cur.xmax + samplefreq; /* + samplefreq here so that last item gets included... */
	
	
	/* at each sampling interval, add a new vertex 
	 *	- apply the unit correction factor to the calculated values so that 
	 *	  the displayed values appear correctly in the viewport
	 */
	glBegin(GL_LINE_STRIP);
	
	for (ctime= stime; ctime <= etime; ctime += samplefreq)
		glVertex2f( ctime, evaluate_fcurve(fcu, ctime)*unitFac );
	
	glEnd();
	
	/* restore driver */
	fcu->driver= driver;
}

/* helper func - draw a samples-based F-Curve */
static void draw_fcurve_curve_samples (bAnimContext *ac, ID *id, FCurve *fcu, View2D *v2d)
{
	FPoint *prevfpt= fcu->fpt;
	FPoint *fpt= prevfpt + 1;
	float fac, v[2];
	int b= fcu->totvert-1;
	
	glBegin(GL_LINE_STRIP);
	
	/* apply unit mapping */
	ANIM_unit_mapping_apply_fcurve(ac->scene, id, fcu, 0);
	
	/* extrapolate to left? - left-side of view comes before first keyframe? */
	if (prevfpt->vec[0] > v2d->cur.xmin) {
		v[0]= v2d->cur.xmin;
		
		/* y-value depends on the interpolation */
		if ((fcu->extend==FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) || (fcu->totvert==1)) {
			/* just extend across the first keyframe's value */
			v[1]= prevfpt->vec[1];
		} 
		else {
			/* extrapolate linear dosnt use the handle, use the next points center instead */
			fac= (prevfpt->vec[0]-fpt->vec[0])/(prevfpt->vec[0]-v[0]);
			if (fac) fac= 1.0f/fac;
			v[1]= prevfpt->vec[1]-fac*(prevfpt->vec[1]-fpt->vec[1]);
		}
		
		glVertex2fv(v);
	}
	
	/* if only one sample, add it now */
	if (fcu->totvert == 1)
		glVertex2fv(prevfpt->vec);
	
	/* loop over samples, drawing segments */
	/* draw curve between first and last keyframe (if there are enough to do so) */
	while (b--) {
		/* Linear interpolation: just add one point (which should add a new line segment) */
		glVertex2fv(prevfpt->vec);
		
		/* get next pointers */
		prevfpt= fpt; 
		fpt++;
		
		/* last point? */
		if (b == 0)
			glVertex2fv(prevfpt->vec);
	}
	
	/* extrapolate to right? (see code for left-extrapolation above too) */
	if (prevfpt->vec[0] < v2d->cur.xmax) {
		v[0]= v2d->cur.xmax;
		
		/* y-value depends on the interpolation */
		if ((fcu->extend==FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) || (fcu->totvert==1)) {
			/* based on last keyframe's value */
			v[1]= prevfpt->vec[1];
		} 
		else {
			/* extrapolate linear dosnt use the handle, use the previous points center instead */
			fpt = prevfpt-1;
			fac= (prevfpt->vec[0]-fpt->vec[0])/(prevfpt->vec[0]-v[0]);
			if (fac) fac= 1.0f/fac;
			v[1]= prevfpt->vec[1]-fac*(prevfpt->vec[1]-fpt->vec[1]);
		}
		
		glVertex2fv(v);
	}
	
	/* unapply unit mapping */
	ANIM_unit_mapping_apply_fcurve(ac->scene, id, fcu, ANIM_UNITCONV_RESTORE);
	
	glEnd();
}

/* helper func - draw one repeat of an F-Curve */
static void draw_fcurve_curve_bezts (bAnimContext *ac, ID *id, FCurve *fcu, View2D *v2d, View2DGrid *grid)
{
	BezTriple *prevbezt= fcu->bezt;
	BezTriple *bezt= prevbezt+1;
	float v1[2], v2[2], v3[2], v4[2];
	float *fp, data[120];
	float fac= 0.0f;
	int b= fcu->totvert-1;
	int resol;
	
	glBegin(GL_LINE_STRIP);
	
	/* apply unit mapping */
	ANIM_unit_mapping_apply_fcurve(ac->scene, id, fcu, 0);
	
	/* extrapolate to left? */
	if (prevbezt->vec[1][0] > v2d->cur.xmin) {
		/* left-side of view comes before first keyframe, so need to extend as not cyclic */
		v1[0]= v2d->cur.xmin;
		
		/* y-value depends on the interpolation */
		if ((fcu->extend==FCURVE_EXTRAPOLATE_CONSTANT) || (prevbezt->ipo==BEZT_IPO_CONST) || (fcu->totvert==1)) {
			/* just extend across the first keyframe's value */
			v1[1]= prevbezt->vec[1][1];
		} 
		else if (prevbezt->ipo==BEZT_IPO_LIN) {
			/* extrapolate linear dosnt use the handle, use the next points center instead */
			fac= (prevbezt->vec[1][0]-bezt->vec[1][0])/(prevbezt->vec[1][0]-v1[0]);
			if (fac) fac= 1.0f/fac;
			v1[1]= prevbezt->vec[1][1]-fac*(prevbezt->vec[1][1]-bezt->vec[1][1]);
		} 
		else {
			/* based on angle of handle 1 (relative to keyframe) */
			fac= (prevbezt->vec[0][0]-prevbezt->vec[1][0])/(prevbezt->vec[1][0]-v1[0]);
			if (fac) fac= 1.0f/fac;
			v1[1]= prevbezt->vec[1][1]-fac*(prevbezt->vec[0][1]-prevbezt->vec[1][1]);
		}
		
		glVertex2fv(v1);
	}
	
	/* if only one keyframe, add it now */
	if (fcu->totvert == 1) {
		v1[0]= prevbezt->vec[1][0];
		v1[1]= prevbezt->vec[1][1];
		glVertex2fv(v1);
	}
	
	/* draw curve between first and last keyframe (if there are enough to do so) */
	while (b--) {
		if (prevbezt->ipo==BEZT_IPO_CONST) {
			/* Constant-Interpolation: draw segment between previous keyframe and next, but holding same value */
			v1[0]= prevbezt->vec[1][0];
			v1[1]= prevbezt->vec[1][1];
			glVertex2fv(v1);
			
			v1[0]= bezt->vec[1][0];
			v1[1]= prevbezt->vec[1][1];
			glVertex2fv(v1);
		}
		else if (prevbezt->ipo==BEZT_IPO_LIN) {
			/* Linear interpolation: just add one point (which should add a new line segment) */
			v1[0]= prevbezt->vec[1][0];
			v1[1]= prevbezt->vec[1][1];
			glVertex2fv(v1);
		}
		else {
			/* Bezier-Interpolation: draw curve as series of segments between keyframes 
			 *	- resol determines number of points to sample in between keyframes
			 */
			
			/* resol not depending on horizontal resolution anymore, drivers for example... */
			// TODO: would be nice to make this depend on the scale of the graph too...
			if (fcu->driver) 
				resol= 32;
			else 
				resol= (int)(3.0*sqrt(bezt->vec[1][0] - prevbezt->vec[1][0]));
			
			if (resol < 2) {
				/* only draw one */
				v1[0]= prevbezt->vec[1][0];
				v1[1]= prevbezt->vec[1][1];
				glVertex2fv(v1);
			}
			else {
				/* clamp resolution to max of 32 */
				if (resol > 32) resol= 32;
				
				v1[0]= prevbezt->vec[1][0];
				v1[1]= prevbezt->vec[1][1];
				v2[0]= prevbezt->vec[2][0];
				v2[1]= prevbezt->vec[2][1];
				
				v3[0]= bezt->vec[0][0];
				v3[1]= bezt->vec[0][1];
				v4[0]= bezt->vec[1][0];
				v4[1]= bezt->vec[1][1];
				
				correct_bezpart(v1, v2, v3, v4);
				
				forward_diff_bezier(v1[0], v2[0], v3[0], v4[0], data, resol, sizeof(float)*3);
				forward_diff_bezier(v1[1], v2[1], v3[1], v4[1], data+1, resol, sizeof(float)*3);
				
				for (fp= data; resol; resol--, fp+= 3)
					glVertex2fv(fp);
			}
		}
		
		/* get next pointers */
		prevbezt= bezt; 
		bezt++;
		
		/* last point? */
		if (b == 0) {
			v1[0]= prevbezt->vec[1][0];
			v1[1]= prevbezt->vec[1][1];
			glVertex2fv(v1);
		}
	}
	
	/* extrapolate to right? (see code for left-extrapolation above too) */
	if (prevbezt->vec[1][0] < v2d->cur.xmax) {
		v1[0]= v2d->cur.xmax;
		
		/* y-value depends on the interpolation */
		if ((fcu->extend==FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) || (prevbezt->ipo==BEZT_IPO_CONST) || (fcu->totvert==1)) {
			/* based on last keyframe's value */
			v1[1]= prevbezt->vec[1][1];
		} 
		else if (prevbezt->ipo==BEZT_IPO_LIN) {
			/* extrapolate linear dosnt use the handle, use the previous points center instead */
			bezt = prevbezt-1;
			fac= (prevbezt->vec[1][0]-bezt->vec[1][0])/(prevbezt->vec[1][0]-v1[0]);
			if (fac) fac= 1.0f/fac;
			v1[1]= prevbezt->vec[1][1]-fac*(prevbezt->vec[1][1]-bezt->vec[1][1]);
		} 
		else {
			/* based on angle of handle 1 (relative to keyframe) */
			fac= (prevbezt->vec[2][0]-prevbezt->vec[1][0])/(prevbezt->vec[1][0]-v1[0]);
			if (fac) fac= 1.0f/fac;
			v1[1]= prevbezt->vec[1][1]-fac*(prevbezt->vec[2][1]-prevbezt->vec[1][1]);
		}
		
		glVertex2fv(v1);
	}
	
	/* unapply unit mapping */
	ANIM_unit_mapping_apply_fcurve(ac->scene, id, fcu, ANIM_UNITCONV_RESTORE);
	
	glEnd();
} 

/* Public Curve-Drawing API  ---------------- */

/* Draw the 'ghost' F-Curves (i.e. snapshots of the curve) 
 * NOTE: unit mapping has already been applied to the values, so do not try and apply again
 */
void graph_draw_ghost_curves (bAnimContext *ac, SpaceIpo *sipo, ARegion *ar, View2DGrid *grid)
{
	FCurve *fcu;
	
	/* draw with thick dotted lines */
	setlinestyle(10);
	glLineWidth(3.0f);
	
	/* anti-aliased lines for less jagged appearance */
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	
	/* the ghost curves are simply sampled F-Curves stored in sipo->ghostCurves */
	for (fcu= sipo->ghostCurves.first; fcu; fcu= fcu->next) {
		/* set whatever color the curve has set 
		 * 	- this is set by the function which creates these
		 *	- draw with a fixed opacity of 2
		 */
		glColor4f(fcu->color[0], fcu->color[1], fcu->color[2], 0.5f);
		
		/* simply draw the stored samples */
		draw_fcurve_curve_samples(ac, NULL, fcu, &ar->v2d);
	}
	
	/* restore settings */
	setlinestyle(0);
	glLineWidth(1.0f);
	
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
}

/* This is called twice from space_graph.c -> graph_main_area_draw()
 * Unselected then selected F-Curves are drawn so that they do not occlude each other.
 */
void graph_draw_curves (bAnimContext *ac, SpaceIpo *sipo, ARegion *ar, View2DGrid *grid, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* build list of curves to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CURVESONLY|ANIMFILTER_CURVEVISIBLE);
	filter |= ((sel) ? (ANIMFILTER_SEL) : (ANIMFILTER_UNSEL));
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
		
	/* for each curve:
	 *	draw curve, then handle-lines, and finally vertices in this order so that 
	 * 	the data will be layered correctly
	 */
	for (ale=anim_data.first; ale; ale=ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		FModifier *fcm= find_active_fmodifier(&fcu->modifiers);
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* map keyframes for drawing if scaled F-Curve */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 0); 
		
		/* draw curve:
		 *	- curve line may be result of one or more destructive modifiers or just the raw data,
		 *	  so we need to check which method should be used
		 *	- controls from active modifier take precidence over keyframes
		 *	  (XXX! editing tools need to take this into account!)
		 */
		 
		/* 1) draw curve line */
		{
			/* set color/drawing style for curve itself */
			if ( ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) || (fcu->flag & FCURVE_PROTECTED) ) {
				/* protected curves (non editable) are drawn with dotted lines */
				setlinestyle(2);
			}
			if ( ((fcu->grp) && (fcu->grp->flag & AGRP_MUTED)) || (fcu->flag & FCURVE_MUTED) ) {
				/* muted curves are drawn in a greyish hue */
				// XXX should we have some variations?
				UI_ThemeColorShade(TH_HEADER, 50);
			}
			else {
				/* set whatever color the curve has set 
				 *	- unselected curves draw less opaque to help distinguish the selected ones
				 */
				glColor4f(fcu->color[0], fcu->color[1], fcu->color[2], drawFCurveFade(fcu));
			}
			
			/* anti-aliased lines for less jagged appearance */
			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_BLEND);
			
			/* draw F-Curve */
			if ((fcu->modifiers.first) || (fcu->flag & FCURVE_INT_VALUES)) {
				/* draw a curve affected by modifiers or only allowed to have integer values 
				 * by sampling it at various small-intervals over the visible region 
				 */
				draw_fcurve_curve(ac, ale->id, fcu, sipo, &ar->v2d, grid);
			}
			else if ( ((fcu->bezt) || (fcu->fpt)) && (fcu->totvert) ) { 
				/* just draw curve based on defined data (i.e. no modifiers) */
				if (fcu->bezt)
					draw_fcurve_curve_bezts(ac, ale->id, fcu, &ar->v2d, grid);
				else if (fcu->fpt)
					draw_fcurve_curve_samples(ac, ale->id, fcu, &ar->v2d);
			}
			
			/* restore settings */
			setlinestyle(0);
			
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_BLEND);
		}
		
		/* 2) draw handles and vertices as appropriate based on active 
		 *	- if the option to only show controls if the F-Curve is selected is enabled, we must obey this
		 */
		if (!(sipo->flag & SIPO_SELCUVERTSONLY) || (fcu->flag & FCURVE_SELECTED)) {
			if (fcurve_needs_draw_fmodifier_controls(fcu, fcm)) {
				/* only draw controls if this is the active modifier */
				if ((fcu->flag & FCURVE_ACTIVE) && (fcm)) {
					switch (fcm->type) {
						case FMODIFIER_TYPE_ENVELOPE: /* envelope */
							draw_fcurve_modifier_controls_envelope(fcu, fcm, &ar->v2d);
							break;
					}
				}
			}
			else if ( ((fcu->bezt) || (fcu->fpt)) && (fcu->totvert) ) { 
				/* apply unit mapping */
				ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, fcu, 0);
				
				if (fcu->bezt) {
					int do_handles = draw_fcurve_handles_check(sipo, fcu);
					
					if (do_handles) {
						/* only draw handles/vertices on keyframes */
						glEnable(GL_BLEND);
						draw_fcurve_handles(ac, sipo, ar, fcu);
						glDisable(GL_BLEND);
					}
					
					draw_fcurve_vertices(ac, sipo, ar, fcu, do_handles);
				}
				else {
					/* samples: only draw two indicators at either end as indicators */
					draw_fcurve_samples(ac, sipo, ar, fcu);
				}
				
				/* unapply unit mapping */
				ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, fcu, ANIM_UNITCONV_RESTORE);
			}
		}
		
		/* undo mapping of keyframes for drawing if scaled F-Curve */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0); 
	}
	
	/* free list of curves */
	BLI_freelistN(&anim_data);
}

/* ************************************************************************* */
/* Channel List */

/* left hand part */
void graph_draw_channel_names(bContext *C, bAnimContext *ac, SpaceIpo *sipo, ARegion *ar) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	float y= 0.0f, height;
	int items, i=0;
	
	/* build list of channels to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CHANNELS);
	items= ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* Update max-extent of channels here (taking into account scrollers):
	 * 	- this is done to allow the channel list to be scrollable, but must be done here
	 * 	  to avoid regenerating the list again and/or also because channels list is drawn first
	 *	- offset of ACHANNEL_HEIGHT*2 is added to the height of the channels, as first is for 
	 *	  start of list offset, and the second is as a correction for the scrollers.
	 */
	height= (float)((items*ACHANNEL_STEP) + (ACHANNEL_HEIGHT*2));
	UI_view2d_totRect_set(v2d, ar->winx, height);
	
	/* loop through channels, and set up drawing depending on their type  */	
	{	/* first pass: just the standard GL-drawing for backdrop + text */
		y= (float)ACHANNEL_FIRST;
		
		for (ale= anim_data.first, i=0; ale; ale= ale->next, i++) {
			const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
			const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
			
			/* check if visible */
			if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw(ac, ale, yminc, ymaxc);
			}
			
			/* adjust y-position for next one */
			y -= ACHANNEL_STEP;
		}
	}
	{	/* second pass: widgets */
		uiBlock *block= uiBeginBlock(C, ar, "graph channel buttons", UI_EMBOSS);
		
		y= (float)ACHANNEL_FIRST;
		
		/* set blending again, as may not be set in previous step */
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		
		for (ale= anim_data.first, i=0; ale; ale= ale->next, i++) {
			const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
			const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
			
			/* check if visible */
			if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
			{
				/* draw all channels using standard channel-drawing API */
				ANIM_channel_draw_widgets(ac, ale, block, yminc, ymaxc);
			}
			
			/* adjust y-position for next one */
			y -= ACHANNEL_STEP;
		}
		
		uiEndBlock(C, block);
		uiDrawBlock(C, block);
		
		glDisable(GL_BLEND);
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}
