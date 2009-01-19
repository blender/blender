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

#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view2d_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_anim_api.h"
#include "ED_util.h"

#include "UI_resources.h"
#include "UI_view2d.h"

/* **************************** */
// XXX stubs to remove!

// NOTE: the code in this file has yet to be rewritten to get rid of the editipo system which is past its use-by-date - Aligorith

typedef struct EditIpo {
	IpoCurve *icu;
	short disptype;
	short flag;
	unsigned int col;
} EditIpo;

#define ISPOIN(a, b, c)                       ( (a->b) && (a->c) )  
#define ISPOIN3(a, b, c, d)           ( (a->b) && (a->c) && (a->d) )
#define ISPOIN4(a, b, c, d, e)        ( (a->b) && (a->c) && (a->d) && (a->e) )

/* *************************** */

/* helper func - draw keyframe vertices only for an IPO-curve */
static void draw_ipovertices_keyframes(IpoCurve *icu, View2D *v2d, short disptype, short edit, short sel)
{
	BezTriple *bezt= icu->bezt;
	float v1[2];
	int a, b;
	
	bglBegin(GL_POINTS);
	
	for (a = 0; a < icu->totvert; a++, bezt++) {
		/* IPO_DISPBITS is used for displaying curves for bitflag variables */
		if (disptype == IPO_DISPBITS) {
			/*if (v2d->cur.xmin < bezt->vec[1][0] < v2d->cur.xmax) {*/
			short ok= 0;
			
			if (edit) {
				if ((bezt->f2 & SELECT) == sel) 
					ok= 1;
			}
			else ok= 1;
			
			if (ok) {
				int val= (int)bezt->vec[1][1];
				v1[0]= bezt->vec[1][0];
				
				for (b= 0; b < 31; b++) {
					if (val & (1<<b)) {	
						v1[1]= b + 1;
						bglVertex3fv(v1);
					}
				}
			}
			/*}*/
		} 
		else { /* normal (non bit) curves */
			if (edit) {
				/* Only the vertex of the line, the
				 * handler are drawn later
				 */
				if ((bezt->f2 & SELECT) == sel) /* && G.v2d->cur.xmin < bezt->vec[1][0] < G.v2d->cur.xmax)*/
					bglVertex3fv(bezt->vec[1]);
			}
			else {
				/* draw only if in bounds */
				/*if (G.v2d->cur.xmin < bezt->vec[1][0] < G.v2d->cur.xmax)*/
				bglVertex3fv(bezt->vec[1]);
			}
		}
	}
	bglEnd();
}

/* helper func - draw handle vertex for an IPO-Curve as a round unfilled circle */
static void draw_ipohandle_control(float x, float y, float xscale, float yscale, float hsize)
{
	static GLuint displist=0;
	
	/* initialise round circle shape */
	if (displist == 0) {
		GLUquadricObj *qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
		
		qobj	= gluNewQuadric(); 
		gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
		gluDisk(qobj, 0.07,  0.8, 12, 1);
		gluDeleteQuadric(qobj);  
		
		glEndList();
	}
	
	/* adjust view transform before starting */
	glTranslatef(x, y, 0.0f);
	glScalef(1.0/xscale*hsize, 1.0/yscale*hsize, 1.0);
	
	/* draw! */
	glCallList(displist);
	
	/* restore view transform */
	glScalef(xscale/hsize, yscale/hsize, 1.0);
	glTranslatef(-x, -y, 0.0f);
}

/* helper func - draw handle vertices only for an IPO-curve (if it is in EditMode) */
static void draw_ipovertices_handles(IpoCurve *icu, View2D *v2d, short disptype, short sel)
{
	BezTriple *bezt= icu->bezt;
	BezTriple *prevbezt = NULL;
	float hsize, xscale, yscale;
	int a;
	
	/* get view settings */
	hsize= UI_GetThemeValuef(TH_HANDLE_VERTEX_SIZE);
	UI_view2d_getscale(v2d, &xscale, &yscale);
	
	/* set handle color */
	if (sel) UI_ThemeColor(TH_HANDLE_VERTEX_SELECT);
	else UI_ThemeColor(TH_HANDLE_VERTEX);
	
	for (a= 0; a < icu->totvert; a++, prevbezt=bezt, bezt++) {
		if (disptype != IPO_DISPBITS) {
			if (ELEM(icu->ipo, IPO_BEZ, IPO_MIXED)) {
				/* Draw the editmode handels for a bezier curve (others don't have handles) 
				 * if their selection status matches the selection status we're drawing for
				 *	- first handle only if previous beztriple was bezier-mode
				 *	- second handle only if current beztriple is bezier-mode
				 */
				if ( (!prevbezt && (bezt->ipo==IPO_BEZ)) || (prevbezt && (prevbezt->ipo==IPO_BEZ)) ) {
					if ((bezt->f1 & SELECT) == sel)/* && v2d->cur.xmin < bezt->vec[0][0] < v2d->cur.xmax)*/
						draw_ipohandle_control(bezt->vec[0][0], bezt->vec[0][1], xscale, yscale, hsize);
				}
				
				if (bezt->ipo==IPO_BEZ) {
					if ((bezt->f3 & SELECT) == sel)/* && v2d->cur.xmin < bezt->vec[2][0] < v2d->cur.xmax)*/
						draw_ipohandle_control(bezt->vec[2][0], bezt->vec[2][1], xscale, yscale, hsize);
				}
			}
		}
	}
}

static void draw_ipovertices(SpaceIpo *sipo, ARegion *ar, int sel)
{
	View2D *v2d= &ar->v2d;
	EditIpo *ei= sipo->editipo;
	int nr, val = 0;
	
	/* this shouldn't get called while drawing in selection-buffer anyway */
	//if (G.f & G_PICKSEL) return;
	
	glPointSize(UI_GetThemeValuef(TH_VERTEX_SIZE));
	
	for (nr=0; nr<sipo->totipo; nr++, ei++) {
		if ISPOIN(ei, flag & IPO_VISIBLE, icu) {
			/* select colors to use to draw keyframes */
			if (sipo->showkey) {
				if (sel) UI_ThemeColor(TH_TEXT_HI);
				else UI_ThemeColor(TH_TEXT);
			} 
			else if (ei->flag & IPO_EDIT) {
				if (sel) UI_ThemeColor(TH_VERTEX_SELECT); 
				else UI_ThemeColor(TH_VERTEX);
			} 
			else {
				if (sel) UI_ThemeColor(TH_TEXT_HI);
				else UI_ThemeColor(TH_TEXT);
				
				val= (ei->icu->flag & IPO_SELECT)!=0;
				if (sel != val) continue;
			}
			
			/* We can't change the color in the middle of
			 * GL_POINTS because then Blender will segfault
			 * on TNT2 / Linux with NVidia's drivers
			 * (at least up to ver. 4349) 
			 */		
			
			/* draw keyframes, then the handles (if in editmode) */
			draw_ipovertices_keyframes(ei->icu, v2d, ei->disptype, (ei->flag & IPO_EDIT), sel);
			
			/* Now draw the two vertex of the handles,
			 * This needs to be done after the keyframes, 
			 * because we can't call glPointSize
			 * in the middle of a glBegin/glEnd also the
			 * bug comment before.
			 */
			if ((ei->flag & IPO_EDIT) && (sipo->flag & SIPO_NOHANDLES)==0)
				draw_ipovertices_handles(ei->icu, v2d, ei->disptype, sel);
		}
	}
	
	glPointSize(1.0);
}

/* draw lines for IPO-curve handles only (this is only done in EditMode) */
static void draw_ipohandles(SpaceIpo *sipo, ARegion *ar, int sel)
{
	EditIpo *ei;
	extern unsigned int nurbcol[];
	unsigned int *col;
	int a, b;
	
	/* don't draw handle lines if handles are not shown */
	if (sipo->flag & SIPO_NOHANDLES)
		return;
	
	if (sel) col= nurbcol+4;
	else col= nurbcol;
	
	ei= sipo->editipo;
	for (a=0; a<sipo->totipo; a++, ei++) {
		if ISPOIN4(ei, flag & IPO_VISIBLE, flag & IPO_EDIT, icu, disptype!=IPO_DISPBITS) {
			if (ELEM(ei->icu->ipo, IPO_BEZ, IPO_MIXED)) {
				BezTriple *bezt=ei->icu->bezt, *prevbezt=NULL;
				float *fp;
				
				for (b= 0; b < ei->icu->totvert; b++, prevbezt=bezt, bezt++) {
					if ((bezt->f2 & SELECT)==sel) {
						fp= bezt->vec[0];
						
						/* only draw first handle if previous segment had handles */
						if ( (!prevbezt && (bezt->ipo==IPO_BEZ)) || (prevbezt && (prevbezt->ipo==IPO_BEZ)) ) 
						{
							cpack(col[bezt->h1]);
							glBegin(GL_LINE_STRIP); 
							glVertex2fv(fp); glVertex2fv(fp+3); 
							glEnd();
							
						}
						
						/* only draw second handle if this segment is bezier */
						if (bezt->ipo == IPO_BEZ) 
						{
							cpack(col[bezt->h2]);
							glBegin(GL_LINE_STRIP); 
							glVertex2fv(fp+3); glVertex2fv(fp+6); 
							glEnd();
						}
					}
					else {
						/* only draw first handle if previous segment was had handles, and selection is ok */
						if ( ((bezt->f1 & SELECT)==sel) && 
							 ( (!prevbezt && (bezt->ipo==IPO_BEZ)) || (prevbezt && (prevbezt->ipo==IPO_BEZ)) ) ) 
						{
							fp= bezt->vec[0];
							cpack(col[bezt->h1]);
							
							glBegin(GL_LINE_STRIP); 
							glVertex2fv(fp); glVertex2fv(fp+3); 
							glEnd();
						}
						
						/* only draw second handle if this segment is bezier, and selection is ok */
						if ( ((bezt->f3 & SELECT)==sel) &&
							 (bezt->ipo == IPO_BEZ) )
						{
							fp= bezt->vec[1];
							cpack(col[bezt->h2]);
							
							glBegin(GL_LINE_STRIP); 
							glVertex2fv(fp); glVertex2fv(fp+3); 
							glEnd();
						}
					}
				}
			}
		}
	}
}

/* helper func - draw one repeat of an ipo-curve: bitflag curve only (this is evil stuff to expose to user like this) */
static void draw_ipocurve_repeat_bits (IpoCurve *icu, View2D *v2d, float cycxofs)
{
	BezTriple *bezt= icu->bezt;
	int a;
	
	/* loop over each keyframe, drawing a line extending from that point */
	for (a=0, bezt=icu->bezt; a < icu->totvert; a++, bezt++) {
		int val= (int)bezt->vec[1][1];
		int b= 0;
		
		/* for each bit in the int, draw a line if the keyframe incorporates it */
		for (b = 0; b < 31; b++) {
			if (val & (1<<b)) {
				float v1[2];
				
				/* value stays constant */
				v1[1]= b+1;
				
				glBegin(GL_LINE_STRIP);
					/* extend left too if first keyframe, and not cyclic extrapolation */
					if ((a == 0) && !(icu->extrap & IPO_CYCL)) {
						v1[0]= v2d->cur.xmin+cycxofs;
						glVertex2fv(v1);
					}
					
					/* must pass through current keyframe */
					v1[0]= bezt->vec[1][0] + cycxofs;
					glVertex2fv(v1); 
					
					/* 1. if there is a next keyframe, extend until then OR
					 * 2. extend until 'infinity' if not cyclic extrapolation
					 */
					if ((a+1) < icu->totvert) v1[0]= (bezt+1)->vec[1][0]+cycxofs;
					else if ((icu->extrap & IPO_CYCL)==0) v1[0]= v2d->cur.xmax+cycxofs;
					
					glVertex2fv(v1);
				glEnd();
			}
		}
	}
}

/* helper func - draw one repeat of an ipo-curve: normal curve */
static void draw_ipocurve_repeat_normal (IpoCurve *icu, View2D *v2d, float cycxofs, float cycyofs, float *facp)
{
	BezTriple *prevbezt= icu->bezt;
	BezTriple *bezt= prevbezt+1;
	float v1[2], v2[2], v3[2], v4[2];
	float *fp, data[120];
	float fac= *(facp);
	int b= icu->totvert-1;
	int resol;
	
	glBegin(GL_LINE_STRIP);
	
	/* extrapolate to left? */
	if ((icu->extrap & IPO_CYCL)==0) {
		/* left-side of view comes before first keyframe, so need to extend as not cyclic */
		if (prevbezt->vec[1][0] > v2d->cur.xmin) {
			v1[0]= v2d->cur.xmin;
			
			/* y-value depends on the interpolation */
			if ((icu->extrap==IPO_HORIZ) || (prevbezt->ipo==IPO_CONST) || (icu->totvert==1)) {
				/* just extend across the first keyframe's value */
				v1[1]= prevbezt->vec[1][1];
			} 
			else if (prevbezt->ipo==IPO_LIN) {
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
	if (icu->totvert == 1) {
		v1[0]= prevbezt->vec[1][0] + cycxofs;
		v1[1]= prevbezt->vec[1][1] + cycyofs;
		glVertex2fv(v1);
	}
	
	/* draw curve between first and last keyframe (if there are enough to do so) */
	while (b--) {
		if (prevbezt->ipo==IPO_CONST) {
			/* Constant-Interpolation: draw segment between previous keyframe and next, but holding same value */
			v1[0]= prevbezt->vec[1][0]+cycxofs;
			v1[1]= prevbezt->vec[1][1]+cycyofs;
			glVertex2fv(v1);
			
			v1[0]= bezt->vec[1][0]+cycxofs;
			v1[1]= prevbezt->vec[1][1]+cycyofs;
			glVertex2fv(v1);
		}
		else if (prevbezt->ipo==IPO_LIN) {
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
			if (icu->driver) 
				resol= 32;
			else 
				resol= 3.0*sqrt(bezt->vec[1][0] - prevbezt->vec[1][0]);
			
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
				
// XXX old sys!				correct_bezpart(v1, v2, v3, v4);
				
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
	if ((icu->extrap & IPO_CYCL)==0) {
		if (prevbezt->vec[1][0] < v2d->cur.xmax) {
			v1[0]= v2d->cur.xmax;
			
			/* y-value depends on the interpolation */
			if ((icu->extrap==IPO_HORIZ) || (prevbezt->ipo==IPO_CONST) || (icu->totvert==1)) {
				/* based on last keyframe's value */
				v1[1]= prevbezt->vec[1][1];
			} 
			else if (prevbezt->ipo==IPO_LIN) {
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

void drawipospace(ScrArea *sa, ARegion *ar)
{
	SpaceIpo *sipo= sa->spacedata.first;
	//View2D *v2d= &ar->v2d;
	//	EditIpo *ei;

	
	if(sipo->editipo) {
		
		/* correct scale for degrees? */
		// XXX this should be calculated elsewhere
#if 0
		disptype= -1;
		ei= sipo->editipo;
		for(a=0; a<sipo->totipo; a++, ei++) {
			if(ei->flag & IPO_VISIBLE) {
				if(disptype== -1) disptype= ei->disptype;
				else if(disptype!=ei->disptype) disptype= 0;
			}
		}
#endif
		// now set grid size (done elsehwere now)
		
		/* ipokeys */
#if 0
		if(sipo->showkey) {
			//if(sipo->ipokey.first==0) make_ipokey();
			//else update_ipokey_val();
			make_ipokey();
			draw_ipokey(sipo);
		}
#endif
		
		/* map ipo-points for drawing if scaled ipo */
		//if (NLA_IPO_SCALED)
		//	ANIM_nla_mapping_apply_ipo(OBACT, sipo->ipo, 0, 0);

		/* draw deselect */
		draw_ipocurves(sipo, ar, 0);
		draw_ipohandles(sipo, ar, 0);
		draw_ipovertices(sipo, ar, 0);
		
		/* draw select */
		draw_ipocurves(sipo, ar, 1);
		draw_ipohandles(sipo, ar, 1);
		draw_ipovertices(sipo, ar, 1);
		
		/* undo mapping of ipo-points for drawing if scaled ipo */
		//if (NLA_IPO_SCALED)
		//	ANIM_nla_mapping_apply_ipo(OBACT, sipo->ipo, 1, 0);
		
	}
}
