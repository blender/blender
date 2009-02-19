/**
 * $Id: drawipo.c 17512 2008-11-20 05:55:42Z aligorith $
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_world_types.h"

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
#include "ED_util.h"

#include "graph_intern.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"
#include "UI_text.h"

/* XXX */
extern void ui_rasterpos_safe(float x, float y, float aspect);
extern void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad);

/* *************************** */
/* Curve Drawing */

/* Points ---------------- */

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_vertices_keyframes (FCurve *fcu, View2D *v2d, short edit, short sel)
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
	
	bglEnd();
}


/* helper func - draw handle vertex for an F-Curve as a round unfilled circle */
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
	
	/* anti-aliased lines for more consistent appearance */
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	
	/* draw! */
	glCallList(displist);
	
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	
	/* restore view transform */
	glScalef(xscale/hsize, yscale/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

/* helper func - draw handle vertices only for an F-Curve (if it is not protected) */
static void draw_fcurve_vertices_handles (FCurve *fcu, View2D *v2d, short sel)
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
	
	for (i=0; i < fcu->totvert; i++, prevbezt=bezt, bezt++) {
		/* Draw the editmode handels for a bezier curve (others don't have handles) 
		 * if their selection status matches the selection status we're drawing for
		 *	- first handle only if previous beztriple was bezier-mode
		 *	- second handle only if current beztriple is bezier-mode
		 */
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

/* helper func - set color to draw F-Curve data with */
static void set_fcurve_vertex_color (SpaceIpo *sipo, FCurve *fcu, short sel)
{
#if 0
		if (sipo->showkey) {
			if (sel) UI_ThemeColor(TH_TEXT_HI);
			else UI_ThemeColor(TH_TEXT);
		} 
#endif
		if ((fcu->flag & FCURVE_PROTECTED)==0) {
			/* Curve's points are being edited */
			if (sel) UI_ThemeColor(TH_VERTEX_SELECT); 
			else UI_ThemeColor(TH_VERTEX);
		} 
		else {
			/* Curve's points cannot be edited */
			if (sel) UI_ThemeColor(TH_TEXT_HI);
			else UI_ThemeColor(TH_TEXT);
		}
}


void draw_fcurve_vertices (SpaceIpo *sipo, ARegion *ar, FCurve *fcu)
{
	View2D *v2d= &ar->v2d;
	
	/* only draw points if curve is visible 
	 * 	- draw unselected points before selected points as separate passes to minimise color-changing overhead
	 *	   (XXX dunno if this is faster than drawing all in one pass though) 
	 * 	   and also to make sure in the case of overlapping points that the selected is always visible
	 *	- draw handles before keyframes, so that keyframes will overlap handles (keyframes are more important for users)
	 */
	
	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
	
	/* draw the two handles first (if they're shown, and if curve is being edited) */
	if ((fcu->flag & FCURVE_PROTECTED)==0 && (fcu->flag & FCURVE_INT_VALUES)==0 && (sipo->flag & SIPO_NOHANDLES)==0) {
		set_fcurve_vertex_color(sipo, fcu, 0);
		draw_fcurve_vertices_handles(fcu, v2d, 0);
		
		set_fcurve_vertex_color(sipo, fcu, 1);
		draw_fcurve_vertices_handles(fcu, v2d, 1);
	}
		
	/* draw keyframes over the handles */
	set_fcurve_vertex_color(sipo, fcu, 0);
	draw_fcurve_vertices_keyframes(fcu, v2d, !(fcu->flag & FCURVE_PROTECTED), 0);
	
	set_fcurve_vertex_color(sipo, fcu, 1);
	draw_fcurve_vertices_keyframes(fcu, v2d, !(fcu->flag & FCURVE_PROTECTED), 1);
	
	glPointSize(1.0f);
}

/* Handles ---------------- */

/* draw lines for F-Curve handles only (this is only done in EditMode) */
static void draw_fcurve_handles (SpaceIpo *sipo, ARegion *ar, FCurve *fcu)
{
	extern unsigned int nurbcol[];
	unsigned int *col;
	int sel, b;
	
	/* don't draw handle lines if handles are not shown */
	if ((sipo->flag & SIPO_NOHANDLES) || (fcu->flag & FCURVE_PROTECTED) || (fcu->flag & FCURVE_INT_VALUES))
		return;
	
	/* slightly hacky, but we want to draw unselected points before selected ones*/
	for (sel= 0; sel < 2; sel++) {
		BezTriple *bezt=fcu->bezt, *prevbezt=NULL;
		float *fp;
		
		if (sel) col= nurbcol+4;
		else col= nurbcol;
			
		for (b= 0; b < fcu->totvert; b++, prevbezt=bezt, bezt++) {
			if ((bezt->f2 & SELECT)==sel) {
				fp= bezt->vec[0];
				
				/* only draw first handle if previous segment had handles */
				if ( (!prevbezt && (bezt->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) ) 
				{
					cpack(col[(unsigned char)bezt->h1]);
					glBegin(GL_LINE_STRIP); 
						glVertex2fv(fp); glVertex2fv(fp+3); 
					glEnd();
					
				}
				
				/* only draw second handle if this segment is bezier */
				if (bezt->ipo == BEZT_IPO_BEZ) 
				{
					cpack(col[(unsigned char)bezt->h2]);
					glBegin(GL_LINE_STRIP); 
						glVertex2fv(fp+3); glVertex2fv(fp+6); 
					glEnd();
				}
			}
			else {
				/* only draw first handle if previous segment was had handles, and selection is ok */
				if ( ((bezt->f1 & SELECT)==sel) && 
					 ( (!prevbezt && (bezt->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) ) ) 
				{
					fp= bezt->vec[0];
					cpack(col[(unsigned char)bezt->h1]);
					
					glBegin(GL_LINE_STRIP); 
						glVertex2fv(fp); glVertex2fv(fp+3); 
					glEnd();
				}
				
				/* only draw second handle if this segment is bezier, and selection is ok */
				if ( ((bezt->f3 & SELECT)==sel) &&
					 (bezt->ipo == BEZT_IPO_BEZ) )
				{
					fp= bezt->vec[1];
					cpack(col[(unsigned char)bezt->h2]);
					
					glBegin(GL_LINE_STRIP); 
						glVertex2fv(fp); glVertex2fv(fp+3); 
					glEnd();
				}
			}
		}
	}
}

/* Samples ---------------- */

/* helper func - draw sample-range marker for an F-Curve as a cross */
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
	
	/* anti-aliased lines for more consistent appearance */
		// XXX needed here?
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	
	/* draw! */
	glCallList(displist);
	
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
	
	/* restore view transform */
	glScalef(xscale/hsize, yscale/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

/* helper func - draw keyframe vertices only for an F-Curve */
static void draw_fcurve_samples (SpaceIpo *sipo, ARegion *ar, FCurve *fcu)
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
	last= (first) ? (first + fcu->totvert) : (NULL);
	
	/* draw */
	if (first && last) {
		draw_fcurve_sample_control(first->vec[0], first->vec[1], xscale, yscale, hsize);
		draw_fcurve_sample_control(last->vec[0], last->vec[1], xscale, yscale, hsize);
	}
}

/* Curve ---------------- */

/* helper func - draw one repeat of an F-Curve */
static void draw_fcurve_repeat (FCurve *fcu, View2D *v2d, float cycxofs, float cycyofs, float *facp)
{
	BezTriple *prevbezt= fcu->bezt;
	BezTriple *bezt= prevbezt+1;
	float v1[2], v2[2], v3[2], v4[2];
	float *fp, data[120];
	float fac= *(facp);
	int b= fcu->totvert-1;
	int resol;
	
	glBegin(GL_LINE_STRIP);
	
	/* extrapolate to left? */
	if ( (fcu->modifiers.first == NULL)/* || ( ((FModifier *)fcu->modifiers.first)->type != FMODIFIER_TYPE_CYCLES) */) {
		/* left-side of view comes before first keyframe, so need to extend as not cyclic */
		if (prevbezt->vec[1][0] > v2d->cur.xmin) {
			v1[0]= v2d->cur.xmin;
			
			/* y-value depends on the interpolation */
			if ((fcu->extend==FCURVE_EXTRAPOLATE_CONSTANT) || (fcu->flag & FCURVE_INT_VALUES) || (prevbezt->ipo==BEZT_IPO_CONST) || (fcu->totvert==1)) {
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
	}
	
	/* if only one keyframe, add it now */
	if (fcu->totvert == 1) {
		v1[0]= prevbezt->vec[1][0] + cycxofs;
		v1[1]= prevbezt->vec[1][1] + cycyofs;
		glVertex2fv(v1);
	}
	
	/* draw curve between first and last keyframe (if there are enough to do so) */
	// XXX this doesn't take into account modifiers, or sample data
	while (b--) {
		if ((fcu->flag & FCURVE_INT_VALUES) || (prevbezt->ipo==BEZT_IPO_CONST)) {
			/* Constant-Interpolation: draw segment between previous keyframe and next, but holding same value */
			v1[0]= prevbezt->vec[1][0]+cycxofs;
			v1[1]= prevbezt->vec[1][1]+cycyofs;
			glVertex2fv(v1);
			
			v1[0]= bezt->vec[1][0]+cycxofs;
			v1[1]= prevbezt->vec[1][1]+cycyofs;
			glVertex2fv(v1);
		}
		else if (prevbezt->ipo==BEZT_IPO_LIN) {
			/* Linear interpolation: just add one point (which should add a new line segment) */
			v1[0]= prevbezt->vec[1][0]+cycxofs;
			v1[1]= prevbezt->vec[1][1]+cycyofs;
			glVertex2fv(v1);
		}
		else {
			/* Bezier-Interpolation: draw curve as series of segments between keyframes 
			 *	- resol determines number of points to sample in between keyframes
			 */
			
			/* resol not depending on horizontal resolution anymore, drivers for example... */
			// XXX need to take into account the scale
			if (fcu->driver) 
				resol= 32;
			else 
				resol= (int)(3.0*sqrt(bezt->vec[1][0] - prevbezt->vec[1][0]));
			
			if (resol < 2) {
				/* only draw one */
				v1[0]= prevbezt->vec[1][0]+cycxofs;
				v1[1]= prevbezt->vec[1][1]+cycyofs;
				glVertex2fv(v1);
			}
			else {
				/* clamp resolution to max of 32 */
				if (resol > 32) resol= 32;
				
				v1[0]= prevbezt->vec[1][0]+cycxofs;
				v1[1]= prevbezt->vec[1][1]+cycyofs;
				v2[0]= prevbezt->vec[2][0]+cycxofs;
				v2[1]= prevbezt->vec[2][1]+cycyofs;
				
				v3[0]= bezt->vec[0][0]+cycxofs;
				v3[1]= bezt->vec[0][1]+cycyofs;
				v4[0]= bezt->vec[1][0]+cycxofs;
				v4[1]= bezt->vec[1][1]+cycyofs;
				
				correct_bezpart(v1, v2, v3, v4);
				
				forward_diff_bezier(v1[0], v2[0], v3[0], v4[0], data, resol, 3);
				forward_diff_bezier(v1[1], v2[1], v3[1], v4[1], data+1, resol, 3);
				
				for (fp= data; resol; resol--, fp+= 3)
					glVertex2fv(fp);
			}
		}
		
		/* get next pointers */
		prevbezt= bezt; 
		bezt++;
		
		/* last point? */
		if (b == 0) {
			v1[0]= prevbezt->vec[1][0]+cycxofs;
			v1[1]= prevbezt->vec[1][1]+cycyofs;
			glVertex2fv(v1);
		}
	}
	
	/* extrapolate to right? (see code for left-extrapolation above too) */
	if ( (fcu->modifiers.first == NULL)/* || ( ((FModifier *)fcu->modifiers.first)->type != FMODIFIER_TYPE_CYCLES) */) {
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
	}
	
	glEnd();
	
	/* return fac, as we alter it */
	*(facp) = fac;
} 

#if 0 // XXX old animation system unconverted code!

/* draw all ipo-curves */
static void draw_ipocurves(SpaceIpo *sipo, ARegion *ar, int sel)
{
	View2D *v2d= &ar->v2d;
	EditIpo *ei;
	int nr, val/*, pickselcode=0*/;
	
	/* if we're drawing for GL_SELECT, reset pickselcode first 
	 * 	- there's only one place that will do this, so it should be fine
	 */
	//if (G.f & G_PICKSEL)
	//	pickselcode= 1;
	
	ei= sipo->editipo;
	for (nr=0; nr<sipo->totipo; nr++, ei++) {
		if ISPOIN3(ei, flag & IPO_VISIBLE, icu, icu->bezt) {
			/* val is used to indicate if curve can be edited */
			//if (G.f & G_PICKSEL) {
			//	/* when using OpenGL to select stuff (on mouseclick) */
			//	glLoadName(pickselcode++);
			//	val= 1;
			//}
			//else {
				/* filter to only draw those that are selected or unselected (based on drawing mode */
				val= (ei->flag & (IPO_SELECT+IPO_EDIT)) != 0;
				val= (val==sel);
			//}
			
			/* only draw those curves that we can draw */
			if (val) {
				IpoCurve *icu= ei->icu;
				float cycdx=0.0f, cycdy=0.0f, cycxofs=0.0f, cycyofs=0.0f;
				const int lastindex= (icu->totvert-1);
				float fac= 0.0f;
				int cycount=1;
				
				/* set color for curve curve:
				 *	- bitflag curves (evil) must always be drawn coloured as they cannot work with IPO-Keys
				 *	- when IPO-Keys are shown, individual curves are not editable, so we show by drawing them all black
				 */
				if ((sipo->showkey) && (ei->disptype!=IPO_DISPBITS)) UI_ThemeColor(TH_TEXT); 
				else cpack(ei->col);
				
				/* cyclic curves - get offset and number of repeats to display */
				if (icu->extrap & IPO_CYCL) {
					BezTriple *bezt= icu->bezt;
					BezTriple *lastbezt= bezt + lastindex;
					
					/* calculate cycle length and amplitude  */
					cycdx= lastbezt->vec[1][0] - bezt->vec[1][0];
					cycdy= lastbezt->vec[1][1] - bezt->vec[1][1];
					
					/* check that the cycle does have some length */
					if (cycdx > 0.01f) {
						/* count cycles before first frame  */
						while (icu->bezt->vec[1][0]+cycxofs > v2d->cur.xmin) {
							cycxofs -= cycdx;
							if (icu->extrap & IPO_DIR) cycyofs-= cycdy;
							cycount++;
						}
						
						/* count cycles after last frame (and adjust offset) */
						fac= 0.0f;
						while (lastbezt->vec[1][0]+fac < v2d->cur.xmax) {
							cycount++;
							fac += cycdx;
						}
					}
				}
				
				/* repeat process for each repeat */
				while (cycount--) {
					/* bitflag curves are drawn differently to normal curves */
					if (ei->disptype==IPO_DISPBITS)
						draw_ipocurve_repeat_bits(icu, v2d, cycxofs);
					else
						draw_ipocurve_repeat_normal(icu, v2d, cycxofs, cycyofs, &fac);
					
					/* prepare for next cycle by adjusing offsets */
					cycxofs += cycdx;
					if (icu->extrap & IPO_DIR) cycyofs += cycdy;
				}
				
				/* vertical line that indicates the end of a speed curve */
				if ((sipo->blocktype==ID_CU) && (icu->adrcode==CU_SPEED)) {
					int b= icu->totvert-1;
					
					if (b) {
						BezTriple *bezt= icu->bezt+b;
						
						glColor3ub(0, 0, 0);
						
						glBegin(GL_LINES);
							glVertex2f(bezt->vec[1][0], 0.0f);
							glVertex2f(bezt->vec[1][0], bezt->vec[1][1]);
						glEnd();
					}
				}
			}
		}
	}
}
#endif // XXX old animation system unconverted code

#if 0
static void draw_ipokey(SpaceIpo *sipo, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	IpoKey *ik;
	
	glBegin(GL_LINES);
	for (ik= sipo->ipokey.first; ik; ik= ik->next) {
		if (ik->flag & SELECT) glColor3ub(0xFF, 0xFF, 0x99);
		else glColor3ub(0xAA, 0xAA, 0x55);
		
		glVertex2f(ik->val, v2d->cur.ymin);
		glVertex2f(ik->val, v2d->cur.ymax);
	}
	glEnd();
}
#endif

void graph_draw_curves (bAnimContext *ac, SpaceIpo *sipo, ARegion *ar)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* build list of curves to draw */
	filter= (ANIMFILTER_VISIBLE|ANIMFILTER_CURVESONLY|ANIMFILTER_CURVEVISIBLE);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
		
	/* for each curve:
	 *	draw curve, then handle-lines, and finally vertices in this order so that 
	 * 	the data will be layered correctly
	 */
	for (ale=anim_data.first; ale; ale=ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		float fac=0.0f; // dummy var
		
		/* map keyframes for drawing if scaled F-Curve */
		if (nob)
			ANIM_nla_mapping_apply_fcurve(nob, ale->key_data, 0, 0); 
		
		/* draw curve - we currently calculate colour on the fly, but that should probably be done in advance instead */
		if ( ((fcu->bezt) || (fcu->fpt)) && (fcu->totvert) ) { 
			/* set color/drawing style for curve itself */
			if ( ((fcu->grp) && (fcu->grp->flag & AGRP_PROTECTED)) || (fcu->flag & FCURVE_PROTECTED) ) {
				/* protected curves (non editable) are drawn with dotted lines */
				setlinestyle(2);
			}
			if (fcu->flag & FCURVE_MUTED) {
				/* muted curves are drawn in a greyish hue */
				// XXX should we have some variations?
				UI_ThemeColorShade(TH_HEADER, 50);
			}
			else {
				/* set whatever color the curve has set */
				glColor3fv(fcu->color);
			}
			
			/* draw F-Curve */
			draw_fcurve_repeat(fcu, &ar->v2d, 0, 0, &fac); // XXX this call still needs a lot more work
			
			/* restore settings */
			setlinestyle(0);
			
			
			/* draw handles and vertices as appropriate */
			if (fcu->bezt) {
				/* only draw handles/vertices on keyframes */
				draw_fcurve_handles(sipo, ar, fcu);
				draw_fcurve_vertices(sipo, ar, fcu);
			}
			else {
				/* samples: should we only draw two indicators at either end as indicators? */
				draw_fcurve_samples(sipo, ar, fcu);
			}
		}
		
		/* undo mapping of keyframes for drawing if scaled F-Curve */
		if (nob)
			ANIM_nla_mapping_apply_fcurve(nob, ale->key_data, 1, 0); 
	}
	
	/* free list of curves */
	BLI_freelistN(&anim_data);
}

/* ************************************************************************* */
/* Channel List */

// XXX quite a few of these need to be kept in sync with their counterparts in Action Editor
// as they're the same. We have 2 separate copies of this for now to make it easier to develop
// the diffences between the two editors, but one day these should be merged!

/* left hand part */
void graph_draw_channel_names(bAnimContext *ac, SpaceIpo *sipo, ARegion *ar) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	View2D *v2d= &ar->v2d;
	float x= 0.0f, y= 0.0f, height;
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
	
	if (height > (v2d->mask.ymax - v2d->mask.ymin)) {
		/* don't use totrect set, as the width stays the same 
		 * (NOTE: this is ok here, the configuration is pretty straightforward) 
		 */
		v2d->tot.ymin= (float)(-height);
	}
	
	/* XXX I would call the below line! (ton) */
	UI_view2d_totRect_set(v2d, ar->winx, height);
	
	/* loop through channels, and set up drawing depending on their type  */	
	y= (float)ACHANNEL_FIRST;
	
	for (ale= anim_data.first, i=0; ale; ale= ale->next, i++) {
		const float yminc= (float)(y - ACHANNEL_HEIGHT_HALF);
		const float ymaxc= (float)(y + ACHANNEL_HEIGHT_HALF);
		
		/* check if visible */
		if ( IN_RANGE(yminc, v2d->cur.ymin, v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, v2d->cur.ymin, v2d->cur.ymax) ) 
		{
			bActionGroup *grp = NULL;
			short indent= 0, offset= 0, sel= 0, group= 0;
			int expand= -1, protect = -1, special= -1, mute = -1;
			char name[128];
			
			/* determine what needs to be drawn */
			switch (ale->type) {
				case ANIMTYPE_SCENE: /* scene */
				{
					Scene *sce= (Scene *)ale->data;
					
					group= 4;
					indent= 0;
					
					special= ICON_SCENE_DATA;
					
					/* only show expand if there are any channels */
					if (EXPANDED_SCEC(sce))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_SCEC(sce);
					strcpy(name, sce->id.name+2);
				}
					break;
				case ANIMTYPE_OBJECT: /* object */
				{
					Base *base= (Base *)ale->data;
					Object *ob= base->object;
					
					group= 4;
					indent= 0;
					
					/* icon depends on object-type */
					if (ob->type == OB_ARMATURE)
						special= ICON_ARMATURE_DATA;
					else	
						special= ICON_OBJECT_DATA;
						
					/* only show expand if there are any channels */
					if (EXPANDED_OBJC(ob))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_OBJC(base);
					strcpy(name, ob->id.name+2);
				}
					break;
				case ANIMTYPE_FILLACTD: /* action widget */
				{
					bAction *act= (bAction *)ale->data;
					
					group = 4;
					indent= 1;
					special= ICON_ACTION;
					
					if (EXPANDED_ACTC(act))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_ACTC(act);
					strcpy(name, "Action");
				}
					break;
				case ANIMTYPE_FILLDRIVERS: /* drivers widget */
				{
					AnimData *adt= (AnimData *)ale->data;
					
					group = 4;
					indent= 1;
					special= ICON_ANIM_DATA;
					
					if (EXPANDED_DRVD(adt))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					strcpy(name, "Drivers");
				}
					break;
				case ANIMTYPE_FILLMATD: /* object materials (dopesheet) expand widget */
				{
					Object *ob = (Object *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_MATERIAL_DATA;
					
					if (FILTER_MAT_OBJC(ob))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					strcpy(name, "Materials");
				}
					break;
				
				
				case ANIMTYPE_DSMAT: /* single material (dopesheet) expand widget */
				{
					Material *ma = (Material *)ale->data;
					
					group = 0;
					indent = 0;
					special = ICON_MATERIAL_DATA;
					offset = 21;
					
					if (FILTER_MAT_OBJD(ma))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, ma->id.name+2);
				}
					break;
				case ANIMTYPE_DSLAM: /* lamp (dopesheet) expand widget */
				{
					Lamp *la = (Lamp *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_LAMP_DATA;
					
					if (FILTER_LAM_OBJD(la))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, la->id.name+2);
				}
					break;
				case ANIMTYPE_DSCAM: /* camera (dopesheet) expand widget */
				{
					Camera *ca = (Camera *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CAMERA_DATA;
					
					if (FILTER_CAM_OBJD(ca))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, ca->id.name+2);
				}
					break;
				case ANIMTYPE_DSCUR: /* curve (dopesheet) expand widget */
				{
					Curve *cu = (Curve *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CURVE_DATA;
					
					if (FILTER_CUR_OBJD(cu))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, cu->id.name+2);
				}
					break;
				case ANIMTYPE_DSSKEY: /* shapekeys (dopesheet) expand widget */
				{
					Key *key= (Key *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_SHAPEKEY_DATA;
					
					if (FILTER_SKE_OBJD(key))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					//sel = SEL_OBJC(base);
					strcpy(name, "Shape Keys");
				}
					break;
				case ANIMTYPE_DSWOR: /* world (dopesheet) expand widget */
				{
					World *wo= (World *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_WORLD_DATA;
					
					if (FILTER_WOR_SCED(wo))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					strcpy(name, wo->id.name+2);
				}
					break;
				
				
				case ANIMTYPE_GROUP: /* action group */
				{
					bActionGroup *agrp= (bActionGroup *)ale->data;
					
					group= 2;
					indent= 0;
					special= -1;
					
					offset= (ale->id) ? 16 : 0;
					
					/* only show expand if there are any channels */
					if (agrp->channels.first) {
						if (EXPANDED_AGRP(agrp))
							expand = ICON_TRIA_DOWN;
						else
							expand = ICON_TRIA_RIGHT;
					}
					
					if (EDITABLE_AGRP(agrp))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					sel = SEL_AGRP(agrp);
					strcpy(name, agrp->name);
				}
					break;
				case ANIMTYPE_FCURVE: /* F-Curve channel */
				{
					FCurve *fcu = (FCurve *)ale->data;
					
					indent = 0;
					
					group= (fcu->grp) ? 1 : 0;
					grp= fcu->grp;
					
					switch (ale->ownertype) {
						case ANIMTYPE_NONE:	/* no owner */
						case ANIMTYPE_FCURVE: 
							offset= 0;
							break;
							
						case ANIMTYPE_DSMAT: /* for now, this is special case for materials */
							offset= 21;
							indent= 1;
							break;
							
						default:
							offset= 14;
							break;
					}
					
					/* for now, 'special' (i.e. in front of name) is used to show visibility status */
					if (fcu->flag & FCURVE_VISIBLE)
						special= ICON_CHECKBOX_HLT;
					else
						special= ICON_CHECKBOX_DEHLT;
					
					if (fcu->flag & FCURVE_MUTED)
						mute = ICON_MUTE_IPO_ON;
					else	
						mute = ICON_MUTE_IPO_OFF;
						
					if (EDITABLE_FCU(fcu))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
					
					sel = SEL_FCU(fcu);
					
					getname_anim_fcurve(name, ale->id, fcu);
				}
					break;
					
				case ANIMTYPE_SHAPEKEY: /* shapekey channel */
				{
					KeyBlock *kb = (KeyBlock *)ale->data;
					
					indent = 0;
					special = -1;
					
					offset= (ale->id) ? 21 : 0;
					
					if (kb->name[0] == '\0')
						sprintf(name, "Key %d", ale->index);
					else
						strcpy(name, kb->name);
				}
					break;
			}	
			
			/* now, start drawing based on this information */
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			
			/* draw backing strip behind channel name */
			if (group == 4) {
				/* only used in dopesheet... */
				if (ELEM(ale->type, ANIMTYPE_SCENE, ANIMTYPE_OBJECT)) {
					/* object channel - darker */
					UI_ThemeColor(TH_DOPESHEET_CHANNELOB);
					uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
					gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
				}
				else {
					/* sub-object folders - lighter */
					UI_ThemeColor(TH_DOPESHEET_CHANNELSUBOB);
					
					offset += 7 * indent;
					glBegin(GL_QUADS);
						glVertex2f(x+offset, yminc);
						glVertex2f(x+offset, ymaxc);
						glVertex2f((float)ACHANNEL_NAMEWIDTH, ymaxc);
						glVertex2f((float)ACHANNEL_NAMEWIDTH, yminc);
					glEnd();
					
					/* clear group value, otherwise we cause errors... */
					group = 0;
				}
			}
			else if (group == 3) {
				/* only for gp-data channels */
				UI_ThemeColorShade(TH_GROUP, 20);
				uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
			}
			else if (group == 2) {
				/* only for action group channels */
				if (ale->flag & AGRP_ACTIVE)
					UI_ThemeColorShade(TH_GROUP_ACTIVE, 10);
				else
					UI_ThemeColorShade(TH_GROUP, 20);
				uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)ACHANNEL_NAMEWIDTH, ymaxc, 8);
			}
			else {
				/* most of the time, only F-Curves are going to be drawn here */
				if (ale->type == ANIMTYPE_FCURVE) {
					/* F-Curve channels are colored with whatever color the curve has stored  */
					FCurve *fcu= (FCurve *)ale->data;
					glColor3fv(fcu->color);
				}
				else
					UI_ThemeColorShade(TH_HEADER, ((indent==0)?20: (indent==1)?-20: -40));
				
				indent += group;
				offset += 7 * indent;
				glBegin(GL_QUADS);
					glVertex2f(x+offset, yminc);
					glVertex2f(x+offset, ymaxc);
					glVertex2f((float)ACHANNEL_NAMEWIDTH, ymaxc);
					glVertex2f((float)ACHANNEL_NAMEWIDTH, yminc);
				glEnd();
			}
			
			/* draw expand/collapse triangle */
			if (expand > 0) {
				UI_icon_draw(x+offset, yminc, expand);
				offset += 17;
			}
			
			/* draw special icon indicating certain data-types */
			if (special > -1) {
				if (ELEM(group, 3, 4)) {
					/* for gpdatablock channels */
					UI_icon_draw(x+offset, yminc, special);
					offset += 17;
				}
				else {
					/* for normal channels */
					UI_icon_draw(x+offset, yminc, special);
					offset += 17;
				}
			}
			glDisable(GL_BLEND);
			
			/* draw name */
			if (sel)
				UI_ThemeColor(TH_TEXT_HI);
			else
				UI_ThemeColor(TH_TEXT);
			offset += 3;
			ui_rasterpos_safe(x+offset, y-4, 1.0f);
			UI_DrawString(G.font, name, 0);
			
			/* reset offset - for RHS of panel */
			offset = 0;
			
			/* set blending again, as text drawing may clear it */
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			
			/* draw protect 'lock' */
			if (protect > -1) {
				offset = 16;
				UI_icon_draw((float)ACHANNEL_NAMEWIDTH-offset, yminc, protect);
			}
			
			/* draw mute 'eye' */
			if (mute > -1) {
				offset += 16;
				UI_icon_draw((float)(ACHANNEL_NAMEWIDTH-offset), yminc, mute);
			}
			glDisable(GL_BLEND);
		}
		
		/* adjust y-position for next one */
		y -= ACHANNEL_STEP;
	}
	
	/* free tempolary channels */
	BLI_freelistN(&anim_data);
}
