/**
 * $Id: drawgpencil.c 14881 2008-05-18 10:41:42Z aligorith $
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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BMF_Api.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_listBase.h"
#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_global.h"
#include "BKE_utildefines.h"
#include "BKE_blender.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_butspace.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"
#include "BIF_toets.h"

#include "BDR_gpencil.h"
#include "BIF_drawgpencil.h"

#include "BSE_drawipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_view.h"

#include "blendef.h"
#include "butspace.h"

#include "PIL_time.h"			/* sleep				*/
#include "mydevice.h"

/* ************************************************** */
/* GREASE PENCIL PANEL-UI DRAWING */

/* Every space which implements Grease-Pencil functionality should have a panel
 * for the settings. All of the space-dependent parts should be coded in the panel
 * code for that space, but the rest is all handled by generic panel here.
 */

/* ------- Callbacks ----------- */
/* These are just 'dummy wrappers' around gpencil api calls */

/* make layer active one after being clicked on */
void gp_ui_activelayer_cb (void *gpd, void *gpl)
{
	gpencil_layer_setactive(gpd, gpl);
	allqueue(REDRAWACTION, 0);
}

/* rename layer and set active */
void gp_ui_renamelayer_cb (void *gpd_arg, void *gpl_arg)
{
	bGPdata *gpd= (bGPdata *)gpd_arg;
	bGPDlayer *gpl= (bGPDlayer *)gpl_arg;
	
	BLI_uniquename(&gpd->layers, gpl, "GP_Layer", offsetof(bGPDlayer, info[0]), 128);
	gpencil_layer_setactive(gpd, gpl);
	allqueue(REDRAWACTION, 0);
}

/* add a new layer */
void gp_ui_addlayer_cb (void *gpd, void *dummy)
{
	gpencil_layer_addnew(gpd);
	allqueue(REDRAWACTION, 0);
}

/* delete active layer */
void gp_ui_dellayer_cb (void *gpd, void *dummy)
{
	gpencil_layer_delactive(gpd);
	allqueue(REDRAWACTION, 0);
}

/* delete last stroke of active layer */
void gp_ui_delstroke_cb (void *gpd, void *gpl)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	
	gpencil_layer_setactive(gpd, gpl);
	gpencil_frame_delete_laststroke(gpf);
}

/* delete active frame of active layer */
void gp_ui_delframe_cb (void *gpd, void *gpl)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	
	gpencil_layer_setactive(gpd, gpl);
	gpencil_layer_delframe(gpl, gpf);
	
	allqueue(REDRAWACTION, 0);
}

/* ------- Drawing Code ------- */

/* draw the controls for a given layer */
static void gp_drawui_layer (uiBlock *block, bGPdata *gpd, bGPDlayer *gpl, short *xco, short *yco)
{
	uiBut *but;
	short active= (gpl->flag & GP_LAYER_ACTIVE);
	short width= 314;
	short height;
	int rb_col;
	
	/* unless button has own callback, it adds this callback to button */
	uiBlockSetFunc(block, gp_ui_activelayer_cb, gpd, gpl);
	
	/* draw header */
	{
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* rounded header */
		if (active) uiBlockSetCol(block, TH_BUT_ACTION);
			rb_col= (active)?-20:20;
			uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-8, *yco-2, width, 24, NULL, 5.0, 0.0, 15 , rb_col-20, ""); 
		if (active) uiBlockSetCol(block, TH_AUTO);
		
		/* lock toggle */
		uiDefIconButBitI(block, ICONTOG, GP_LAYER_LOCKED, B_REDR, ICON_UNLOCKED,	*xco-7, *yco-1, 20, 20, &gpl->flag, 0.0, 0.0, 0, 0, "Layer cannot be modified");
	}
	
	/* when layer is locked or hidden, only draw header */
	if (gpl->flag & (GP_LAYER_LOCKED|GP_LAYER_HIDE)) {
		char name[256]; /* gpl->info is 128, but we need space for 'locked/hidden' as well */
		
		height= 26;
		
		/* visibility button (only if hidden but not locked!) */
		if ((gpl->flag & GP_LAYER_HIDE) && !(gpl->flag & GP_LAYER_LOCKED))
			uiDefIconButBitI(block, ICONTOG, GP_LAYER_HIDE, B_REDR, ICON_RESTRICT_VIEW_OFF,	*xco+12, *yco-1, 20, 20, &gpl->flag, 0.0, 0.0, 0, 0, "Visibility of layer");
		
		/* name */
		if (gpl->flag & GP_LAYER_HIDE)
			sprintf(name, "%s (Hidden)", gpl->info);
		else
			sprintf(name, "%s (Locked)", gpl->info);
		uiDefBut(block, LABEL, 1, name,	*xco+35, *yco, 240, 20, NULL, 0.0, 0.0, 0, 0, "Short description of what this layer is for (optional)");
			
		/* delete button (only if hidden but not locked!) */
		if ((gpl->flag & GP_LAYER_HIDE) & !(gpl->flag & GP_LAYER_LOCKED)) {
			but= uiDefIconBut(block, BUT, B_REDR, ICON_X, *xco+(width-30), *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Delete layer");
			uiButSetFunc(but, gp_ui_dellayer_cb, gpd, NULL);
		}	
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else {
		height= 97;
		
		/* draw rest of header */
		{
			/* visibility button */
			uiDefIconButBitI(block, ICONTOG, GP_LAYER_HIDE, B_REDR, ICON_RESTRICT_VIEW_OFF,	*xco+12, *yco-1, 20, 20, &gpl->flag, 0.0, 0.0, 0, 0, "Visibility of layer");
			
			uiBlockSetEmboss(block, UI_EMBOSS);
			
			/* name */
			but= uiDefButC(block, TEX, B_REDR, "Info:",	*xco+36, *yco, 240, 19, gpl->info, 0, 127, 0, 0, "Short description of what this layer is for (optional)");
			uiButSetFunc(but, gp_ui_renamelayer_cb, gpd, gpl);
			
			/* delete 'button' */
			uiBlockSetEmboss(block, UI_EMBOSSN);
			
			but= uiDefIconBut(block, BUT, B_REDR, ICON_X, *xco+(width-30), *yco, 19, 19, NULL, 0.0, 0.0, 0.0, 0.0, "Delete layer");
			uiButSetFunc(but, gp_ui_dellayer_cb, gpd, NULL);
			
			uiBlockSetEmboss(block, UI_EMBOSS);
		}
		
		/* draw backdrop */
		if (active) uiBlockSetCol(block, TH_BUT_ACTION);
			uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-8, *yco-height, width, height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
		if (active) uiBlockSetCol(block, TH_AUTO);
		
		/* draw settings */
		{
			/* color */
			uiBlockBeginAlign(block);
				uiDefButF(block, COL, B_REDR, "",		*xco, *yco-26, 150, 19, gpl->color, 0, 0, 0, 0, "Color to use for all strokes on this Grease Pencil Layer");
				uiDefButF(block, NUMSLI, B_REDR, "Opacity: ",		*xco,*yco-45,150,19, &gpl->color[3], 0.3, 1.0, 0, 0, "Visibility of stroke (0.3 to 1.0)");
			uiBlockEndAlign(block);
			
			/* stroke thickness */
			uiDefButS(block, NUMSLI, B_REDR, "Thickness:",	*xco, *yco-75, 150, 20, &gpl->thickness, 1, 10, 0, 0, "Thickness of strokes (in pixels)");
			
			/* debugging options */
			if (G.f & G_DEBUG) {
				uiDefButBitI(block, TOG, GP_LAYER_DRAWDEBUG, B_REDR, "Show Points", *xco, *yco-95, 150, 20, &gpl->flag, 0, 0, 0, 0, "Show points which form the strokes");
			}
			
			/* onion-skinning */
			uiBlockBeginAlign(block);
				uiDefButBitI(block, TOG, GP_LAYER_ONIONSKIN, B_REDR, "Onion-Skin", *xco+160, *yco-26, 140, 20, &gpl->flag, 0, 0, 0, 0, "Ghost frames on either side of frame");
				uiDefButS(block, NUMSLI, B_REDR, "GStep:",	*xco+160, *yco-46, 140, 20, &gpl->gstep, 0, 120, 0, 0, "Max number of frames on either side of active frame to show (0 = just 'first' available sketch on either side)");
			uiBlockEndAlign(block);
			
			/* options */
			uiBlockBeginAlign(block);
				but= uiDefBut(block, BUT, B_REDR, "Del Active Frame", *xco+160, *yco-75, 140, 20, NULL, 0, 0, 0, 0, "Erases the the active frame for this layer (Hotkey = Alt-XKEY/DEL)");
				uiButSetFunc(but, gp_ui_delframe_cb, gpd, gpl);
				
				but= uiDefBut(block, BUT, B_REDR, "Del Last Stroke", *xco+160, *yco-95, 140, 20, NULL, 0, 0, 0, 0, "Erases the last stroke from the active frame (Hotkey = Alt-XKEY/DEL)");
				uiButSetFunc(but, gp_ui_delstroke_cb, gpd, gpl);
			uiBlockEndAlign(block);
		}
	}
	
	/* adjust height for new to start */
	(*yco) -= (height + 27); 
} 

/* Draw the contents for a grease-pencil panel. This assumes several things:
 * 	- that panel has been created, is 318 x 204. max yco is 225
 *	- that a toggle for turning on/off gpencil drawing is 150 x 20, starting from (10,225)
 *		which is basically the top left-hand corner
 * It will return the amount of extra space to extend the panel by
 */
short draw_gpencil_panel (uiBlock *block, bGPdata *gpd, ScrArea *sa)
{
	uiBut *but;
	bGPDlayer *gpl;
	short xco= 10, yco= 170;
	
	/* draw gpd settings first */
	{
		/* add new layer buttons */
		but= uiDefBut(block, BUT, B_REDR, "Add New Layer", 10,205,150,20, 0, 0, 0, 0, 0, "Adds a new Grease Pencil Layer");
		uiButSetFunc(but, gp_ui_addlayer_cb, gpd, NULL);
		
		
		/* show override lmb-clicks button + painting lock */
		uiBlockBeginAlign(block);
			if ((gpd->flag & GP_DATA_EDITPAINT)==0) {
				uiDefButBitI(block, TOG, GP_DATA_EDITPAINT, B_REDR, "Draw Mode", 170, 225, 130, 20, &gpd->flag, 0, 0, 0, 0, "Interpret click-drag as new strokes");
				
				uiBlockSetCol(block, TH_BUT_SETTING);
					uiDefIconButBitI(block, ICONTOG, GP_DATA_LMBPLOCK, B_REDR, ICON_UNLOCKED,	300, 225, 20, 20, &gpd->flag, 0.0, 0.0, 0, 0, "Painting cannot occur with Shift-LMB (when making selections)");
				uiBlockSetCol(block, TH_AUTO);
			}
			else
				uiDefButBitI(block, TOG, GP_DATA_EDITPAINT, B_REDR, "Draw Mode", 170, 225, 150, 20, &gpd->flag, 0, 0, 0, 0, "Interpret click-drag as new strokes");
		uiBlockEndAlign(block);
		
		/* 'view align' button (naming depends on context) */
		if (sa->spacetype == SPACE_VIEW3D)
			uiDefButBitI(block, TOG, GP_DATA_VIEWALIGN, B_REDR, "Sketch in 3D", 170, 205, 150, 20, &gpd->flag, 0, 0, 0, 0, "New strokes are added in 3D-space");
		else if (sa->spacetype != SPACE_SEQ) /* not available for sequencer yet */
			uiDefButBitI(block, TOG, GP_DATA_VIEWALIGN, B_REDR, "Stick to View", 170, 205, 150, 20, &gpd->flag, 0, 0, 0, 0, "New strokes are added on 2d-canvas");
	}
	
	/* draw for each layer */
	for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
		gp_drawui_layer(block, gpd, gpl, &xco, &yco);
	}
	
	/* return new height if necessary */
	return (yco < 0) ? (204 - yco) : 204;
}	

/* ************************************************** */
/* GREASE PENCIL DRAWING */

/* ----- General Defines ------ */

/* flags for sflag */
enum {
	GP_DRAWDATA_NOSTATUS 	= (1<<0),	/* don't draw status info */
	GP_DRAWDATA_ONLY3D		= (1<<1),	/* only draw 3d-strokes */
	GP_DRAWDATA_ONLYV2D		= (1<<2),	/* only draw 'canvas' strokes */
	GP_DRAWDATA_ONLYI2D		= (1<<3),	/* only draw 'image' strokes */
};

/* ----- Tool Buffer Drawing ------ */

/* draw stroke defined in buffer (simple ogl lines/points for now, as dotted lines) */
static void gp_draw_stroke_buffer (tGPspoint *points, int totpoints, short thickness, short dflag, short sflag)
{
	tGPspoint *pt;
	int i;
	
	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;
	
	/* check if buffer can be drawn */
	if (dflag & (GP_DRAWDATA_ONLY3D|GP_DRAWDATA_ONLYV2D))
		return;
	
	/* if drawing a single point, draw it larger */	
	if (totpoints == 1) {		
		/* draw point */
		glBegin(GL_POINTS);
			glVertex2f(points->x, points->y);
		glEnd();
	}
	else if (sflag & GP_STROKE_ERASER) {
		/* draw stroke curve - just standard thickness */
		setlinestyle(4);
		glLineWidth(1.0f);
		
		glBegin(GL_LINE_STRIP);
		for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
			glVertex2f(pt->x, pt->y);
		}
		glEnd();
		
		setlinestyle(0);
	}
	else {
		float oldpressure = 0.0f;
		
		/* draw stroke curve */
		setlinestyle(2);
		
		glBegin(GL_LINE_STRIP);
		for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
			if (fabs(pt->pressure - oldpressure) > 0.2f) {
				glEnd();
				glLineWidth(pt->pressure * thickness);
				glBegin(GL_LINE_STRIP);
				
				glVertex2f(pt->x, pt->y);
				
				oldpressure = pt->pressure;
			}
			else
				glVertex2f(pt->x, pt->y);
		}
		glEnd();
		
		setlinestyle(0);
	}
}

/* ----- Existing Strokes Drawing (3D and Point) ------ */

/* draw a given stroke - just a single dot (only one point) */
static void gp_draw_stroke_point (bGPDspoint *points, short sflag, int winx, int winy)
{
	/* draw point */
	if (sflag & GP_STROKE_3DSPACE) {
		glBegin(GL_POINTS);
			glVertex3f(points->x, points->y, points->z);
		glEnd();
	}
	else if (sflag & GP_STROKE_2DSPACE) {
		glBegin(GL_POINTS);
			glVertex2f(points->x, points->y);
		glEnd();
	}
	else {
		const float x= (points->x / 1000 * winx);
		const float y= (points->y / 1000 * winy);
		
		glBegin(GL_POINTS);
			glVertex2f(x, y);
		glEnd();
	}
}

/* draw a given stroke in 3d (i.e. in 3d-space), using simple ogl lines */
static void gp_draw_stroke_3d (bGPDspoint *points, int totpoints, short thickness, short dflag, short sflag, short debug, int winx, int winy)
{
	bGPDspoint *pt;
	float oldpressure = 0.0f;
	int i;
	
	/* draw stroke curve */
	glBegin(GL_LINE_STRIP);
	for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
		if (fabs(pt->pressure - oldpressure) > 0.2f) {
			glEnd();
			glLineWidth(pt->pressure * thickness);
			glBegin(GL_LINE_STRIP);
			
			glVertex3f(pt->x, pt->y, pt->z);
			
			oldpressure = pt->pressure;
		}
		else
			glVertex3f(pt->x, pt->y, pt->z);
	}
	glEnd();
	
	/* draw debug points of curve on top? */
	if (debug) {
		glBegin(GL_POINTS);
		for (i=0, pt=points; i < totpoints && pt; i++, pt++)
			glVertex3f(pt->x, pt->y, pt->z);
		glEnd();
	}
}

/* ----- Fancy 2D-Stroke Drawing ------ */

/* draw a given stroke in 2d */
static void gp_draw_stroke (bGPDspoint *points, int totpoints, short thickness, short dflag, short sflag, short debug, int winx, int winy)
{	
	/* if thickness is less than 3, 'smooth' opengl lines look better */
	if ((thickness < 3) || (G.rt==0)) {
		bGPDspoint *pt;
		int i;
		
		glBegin(GL_LINE_STRIP);
		for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
			if (sflag & GP_STROKE_2DSPACE) {
				glVertex2f(pt->x, pt->y);
			}
			else {
				const float x= (pt->x / 1000 * winx);
				const float y= (pt->y / 1000 * winy);
				
				glVertex2f(x, y);
			}
		}
		glEnd();
	}
	else { /* tesselation code: currently only enabled with rt != 0 */
		bGPDspoint *pt1, *pt2;
		float pm[2];
		int i;
		short n;
		
		glShadeModel(GL_FLAT);
		
		glPointSize(3.0f); // temp
		
		for (n= 0; n < 2; n++) { // temp
		glBegin((n)?GL_POINTS:GL_QUADS);
		
		for (i=0, pt1=points, pt2=points+1; i < (totpoints-1); i++, pt1++, pt2++) {
			float s0[2], s1[2];		/* segment 'center' points */
			float t0[2], t1[2];		/* tesselated coordinates */
			float m1[2], m2[2];		/* gradient and normal */
			float pthick;			/* thickness at segment point */
			
			/* get x and y coordinates from points */
			if (sflag & GP_STROKE_2DSPACE) {
				s0[0]= pt1->x; 		s0[1]= pt1->y;
				s1[0]= pt2->x;		s1[1]= pt2->y;
			}
			else {
				s0[0]= (pt1->x / 1000 * winx);
				s0[1]= (pt1->y / 1000 * winy);
				s1[0]= (pt2->x / 1000 * winx);
				s1[1]= (pt2->y / 1000 * winy);
			}		
			
			/* calculate gradient and normal - 'angle'=(ny/nx) */
			m1[1]= s1[1] - s0[1];		
			m1[0]= s1[0] - s0[0];
			m2[1]= -m1[0];
			m2[0]= m1[1];
			Normalize2(m2);
			
			/* always use pressure from first point here */
			pthick= (pt1->pressure * thickness);
			
			/* if the first segment, start of segment is segment's normal */
			if (i == 0) {
				// TODO: also draw/do a round end-cap first
				
				/* calculate points for start of segment */
				t0[0]= s0[0] - (pthick * m2[0]);
				t0[1]= s0[1] - (pthick * m2[1]);
				t1[0]= s0[0] + (pthick * m2[0]);
				t1[1]= s0[1] + (pthick * m2[1]);
				
				/* draw this line only once */
				glVertex2fv(t0);
				glVertex2fv(t1);
			}
			/* if not the first segment, use bisector of angle between segments */
			else {
				float mb[2]; 	/* bisector normal */
				
				/* calculate gradient of bisector (as average of normals) */
				mb[0]= (pm[0] + m2[0]) / 2;
				mb[1]= (pm[1] + m2[1]) / 2;
				Normalize2(mb);
				
				/* calculate points for start of segment */
				// FIXME: do we need extra padding for acute angles?
				t0[0]= s0[0] - (pthick * mb[0]);
				t0[1]= s0[1] - (pthick * mb[1]);
				t1[0]= s0[0] + (pthick * mb[0]);
				t1[1]= s0[1] + (pthick * mb[1]);
				
				/* draw this line twice (once for end of current segment, and once for start of next) */
				glVertex2fv(t1);
				glVertex2fv(t0);
				glVertex2fv(t0);
				glVertex2fv(t1);
			}
			
			/* if last segment, also draw end of segment (defined as segment's normal) */
			if (i == totpoints-2) {
				/* for once, we use second point's pressure (otherwise it won't be drawn) */
				pthick= (pt2->pressure * thickness);
				
				/* calculate points for end of segment */
				t0[0]= s1[0] - (pthick * m2[0]);
				t0[1]= s1[1] - (pthick * m2[1]);
				t1[0]= s1[0] + (pthick * m2[0]);
				t1[1]= s1[1] + (pthick * m2[1]);
				
				/* draw this line only once */
				glVertex2fv(t1);
				glVertex2fv(t0);
				
				// TODO: draw end cap as last step 
			}
			
			/* store stroke's 'natural' normal for next stroke to use */
			Vec2Copyf(pm, m2);
		}
		
		glEnd();
		}
	}
	
	/* draw debug points of curve on top? (original stroke points) */
	if (debug) {
		bGPDspoint *pt;
		int i;
		
		glBegin(GL_POINTS);
		for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
			if (sflag & GP_STROKE_2DSPACE) {
				glVertex2f(pt->x, pt->y);
			}
			else {
				const float x= (pt->x / 1000 * winx);
				const float y= (pt->y / 1000 * winy);
				
				glVertex2f(x, y);
			}
		}
		glEnd();
	}
}

/* ----- General Drawing ------ */

/* draw a set of strokes */
static void gp_draw_strokes (bGPDframe *gpf, int winx, int winy, int dflag, short debug, 
							 short lthick, float color[4])
{
	bGPDstroke *gps;
	
	/* set color first (may need to reset it again later too) */
	glColor4f(color[0], color[1], color[2], color[3]);
	
	for (gps= gpf->strokes.first; gps; gps= gps->next) {
		/* check if stroke can be drawn */
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
		if ((gps->points == 0) || (gps->totpoints < 1))
			continue;
		
		/* check which stroke-drawer to use */
		if (gps->totpoints == 1)
			gp_draw_stroke_point(gps->points, gps->flag, winx, winy);
		else if (dflag & GP_DRAWDATA_ONLY3D)
			gp_draw_stroke_3d(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
		else if (gps->totpoints > 1)	
			gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
	}
}

/* draw grease-pencil datablock */
static void gp_draw_data (bGPdata *gpd, int winx, int winy, int dflag)
{
	bGPDlayer *gpl, *actlay=NULL;
	
	/* turn on smooth lines (i.e. anti-aliasing) */
	glEnable(GL_LINE_SMOOTH);
	
	/* turn on alpha-blending */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
		
	/* loop over layers, drawing them */
	for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
		bGPDframe *gpf;
		
		short debug = (gpl->flag & GP_LAYER_DRAWDEBUG) ? 1 : 0;
		short lthick= gpl->thickness;
		float color[4], tcolor[4];
		
		/* don't draw layer if hidden */
		if (gpl->flag & GP_LAYER_HIDE) 
			continue;
		
		/* if layer is active one, store pointer to it */
		if (gpl->flag & GP_LAYER_ACTIVE)
			actlay= gpl;
		
		/* get frame to draw */
		gpf= gpencil_layer_getframe(gpl, CFRA, 0);
		if (gpf == NULL) 
			continue;
		
		/* set color, stroke thickness, and point size */
		glLineWidth(lthick);
		QUATCOPY(color, gpl->color); // just for copying 4 array elements
		QUATCOPY(tcolor, gpl->color); // additional copy of color (for ghosting)
		glColor4f(color[0], color[1], color[2], color[3]);
		glPointSize(gpl->thickness + 2);
		
		/* draw 'onionskins' (frame left + right) */
		if (gpl->flag & GP_LAYER_ONIONSKIN) {
			/* drawing method - only immediately surrounding (gstep = 0), or within a frame range on either side (gstep > 0)*/			
			if (gpl->gstep) {
				bGPDframe *gf;
				short i;
				
				/* draw previous frames first */
				for (gf=gpf->prev, i=0; gf; gf=gf->prev, i++) {
					/* check if frame is drawable */
					if ((gpf->framenum - gf->framenum) <= gpl->gstep) {
						/* alpha decreases with distance from curframe index */
						tcolor[3] = color[3] - (i/gpl->gstep);
						gp_draw_strokes(gf, winx, winy, dflag, debug, lthick, tcolor);
					}
					else 
						break;
				}
				
				/* now draw next frames */
				for (gf= gpf->next, i=0; gf; gf=gf->next, i++) {
					/* check if frame is drawable */
					if ((gf->framenum - gpf->framenum) <= gpl->gstep) {
						/* alpha decreases with distance from curframe index */
						tcolor[3] = color[3] - (i/gpl->gstep);
						gp_draw_strokes(gf, winx, winy, dflag, debug, lthick, tcolor);
					}
					else 
						break;
				}	
				
				/* restore alpha */
				glColor4f(color[0], color[1], color[2], color[3]);
			}
			else {
				/* draw the strokes for the ghost frames (at half of the alpha set by user) */
				if (gpf->prev) {
					tcolor[3] = (color[3] / 7);
					gp_draw_strokes(gpf->prev, winx, winy, dflag, debug, lthick, tcolor);
				}
				
				if (gpf->next) {
					tcolor[3] = (color[3] / 4);
					gp_draw_strokes(gpf->next, winx, winy, dflag, debug, lthick, tcolor);
				}
				
				/* restore alpha */
				glColor4f(color[0], color[1], color[2], color[3]);
			}
		}
		
		/* draw the strokes already in active frame */
		tcolor[3]= color[3];
		gp_draw_strokes(gpf, winx, winy, dflag, debug, lthick, tcolor);
		
		/* Check if may need to draw the active stroke cache, only if this layer is the active layer
		 * that is being edited. (Stroke buffer is currently stored in gp-data)
		 */
		if ((G.f & G_GREASEPENCIL) && (gpl->flag & GP_LAYER_ACTIVE) &&
			(gpf->flag & GP_FRAME_PAINT)) 
		{
			/* Buffer stroke needs to be drawn with a different linestyle to help differentiate them from normal strokes. */
			gp_draw_stroke_buffer(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag);
		}
	}
	
	/* turn off alpha blending, then smooth lines */
	glDisable(GL_BLEND); // alpha blending
	glDisable(GL_LINE_SMOOTH); // smooth lines
	
	/* show info for debugging the status of gpencil */
	if ( ((dflag & GP_DRAWDATA_NOSTATUS)==0) && (gpd->flag & GP_DATA_DISPINFO) ) {
		char printable[256];
		short xmax;
		
		/* get text to display */
		if (actlay) {
			if (gpd->flag & GP_DATA_EDITPAINT)
				BIF_ThemeColor(TH_BONE_POSE); // should be blue-ish
			else if (actlay->actframe == NULL)
				BIF_ThemeColor(TH_REDALERT);
			else if (actlay->actframe->framenum == CFRA)
				BIF_ThemeColor(TH_VERTEX_SELECT); // should be yellow
			else
				BIF_ThemeColor(TH_TEXT_HI);
			
			if (actlay->actframe) {
				sprintf(printable, "GPencil: Layer ('%s'), Frame (%d) %s", 
					actlay->info, actlay->actframe->framenum,
					((gpd->flag & GP_DATA_EDITPAINT)?", Draw Mode On":"") );
			}
			else {
				sprintf(printable, "GPencil: Layer ('%s'), Frame <None> %s", 
					actlay->info, ((gpd->flag & GP_DATA_EDITPAINT)?", Draw Mode On":"") );
			}
		}
		else {
			BIF_ThemeColor(TH_REDALERT);
			sprintf(printable, "GPencil: Layer <None>");
		}
		xmax= GetButStringLength(printable);
		
		/* only draw it if view is wide enough (assume padding of 20 is enough for now) */
		if (winx > (xmax + 20)) { 
			glRasterPos2i(winx-xmax, winy-20);
			BMF_DrawString(G.fonts, printable);
		}
	}
	
	/* restore initial gl conditions */
	glLineWidth(1.0);
	glPointSize(1.0);
	glColor4f(0, 0, 0, 1);
}

/* ----- Grease Pencil Sketches Drawing API ------ */

/* draw grease-pencil sketches to specified 2d-view that uses ibuf corrections */
void draw_gpencil_2dimage (ScrArea *sa, ImBuf *ibuf)
{
	bGPdata *gpd;
	int dflag = 0;
	
	/* check that we have grease-pencil stuff to draw */
	if (ELEM(NULL, sa, ibuf)) return;
	gpd= gpencil_data_getactive(sa);
	if (gpd == NULL) return;
	
	/* draw it! */
	dflag = (GP_DRAWDATA_ONLYI2D|GP_DRAWDATA_NOSTATUS);
	gp_draw_data(gpd, sa->winx, sa->winy, dflag);
}

/* draw grease-pencil sketches to specified 2d-view assuming that matrices are already set correctly 
 * Note: this gets called twice - first time with onlyv2d=1 to draw 'canvas' strokes, second time with onlyv2d=0 for screen-aligned strokes
 */
void draw_gpencil_2dview (ScrArea *sa, short onlyv2d)
{
	bGPdata *gpd;
	int dflag = 0;
	
	/* check that we have grease-pencil stuff to draw */
	if (sa == NULL) return;
	gpd= gpencil_data_getactive(sa);
	if (gpd == NULL) return;
	
	/* draw it! */
	if (onlyv2d) dflag |= (GP_DRAWDATA_ONLYV2D|GP_DRAWDATA_NOSTATUS);
	gp_draw_data(gpd, sa->winx, sa->winy, dflag);
}

/* draw grease-pencil sketches to specified 3d-view assuming that matrices are already set correctly 
 * Note: this gets called twice - first time with only3d=1 to draw 3d-strokes, second time with only3d=0 for screen-aligned strokes
 */
void draw_gpencil_3dview (ScrArea *sa, short only3d)
{
	bGPdata *gpd;
	int dflag = 0;
	
	/* check that we have grease-pencil stuff to draw */
	gpd= gpencil_data_getactive(sa);
	if (gpd == NULL) return;
	
	/* draw it! */
	if (only3d) dflag |= (GP_DRAWDATA_ONLY3D|GP_DRAWDATA_NOSTATUS);
	gp_draw_data(gpd, sa->winx, sa->winy, dflag);
}

/* draw grease-pencil sketches to opengl render window assuming that matrices are already set correctly */
void draw_gpencil_oglrender (View3D *v3d, int winx, int winy)
{
	bGPdata *gpd;
	
	/* assume gpencil data comes from v3d */
	if (v3d == NULL) return;
	gpd= v3d->gpd;
	if (gpd == NULL) return;
	
	/* pass 1: draw 3d-strokes ------------ > */
	gp_draw_data(gpd, winx, winy, (GP_DRAWDATA_NOSTATUS|GP_DRAWDATA_ONLY3D));
	
	/* pass 2: draw 2d-strokes ------------ > */
		/* adjust view matrices */
	myortho2(-0.375, (float)(winx)-0.375, -0.375, (float)(winy)-0.375);
	glLoadIdentity();
	
		/* draw it! */
	gp_draw_data(gpd, winx, winy, GP_DRAWDATA_NOSTATUS);
}

/* ************************************************** */
