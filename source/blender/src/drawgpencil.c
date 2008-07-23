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
	force_draw_plus(SPACE_ACTION, 0);
}

/* rename layer and set active */
void gp_ui_renamelayer_cb (void *gpd_arg, void *gpl_arg)
{
	bGPdata *gpd= (bGPdata *)gpd_arg;
	bGPDlayer *gpl= (bGPDlayer *)gpl_arg;
	
	BLI_uniquename(&gpd->layers, gpl, "GP_Layer", offsetof(bGPDlayer, info[0]), 128);
	gpencil_layer_setactive(gpd, gpl);
	force_draw_plus(SPACE_ACTION, 0);
}

/* add a new layer */
void gp_ui_addlayer_cb (void *gpd, void *dummy)
{
	gpencil_layer_addnew(gpd);
	force_draw_plus(SPACE_ACTION, 0);
}

/* delete active layer */
void gp_ui_dellayer_cb (void *gpd, void *dummy)
{
	gpencil_layer_delactive(gpd);
	force_draw_plus(SPACE_ACTION, 0);
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
	
	force_draw_plus(SPACE_ACTION, 0);
}

/* ------- Drawing Code ------- */

/* draw the controls for a given layer */
static void gp_drawui_layer (uiBlock *block, bGPdata *gpd, bGPDlayer *gpl, short *xco, short *yco)
{
	uiBut *but;
	short width= 314;
	short height;
	int rb_col;
	
	/* unless button has own callback, it adds this callback to button */
	uiBlockSetFunc(block, gp_ui_activelayer_cb, gpd, gpl);
	
	/* draw header */
	{
		uiBlockSetEmboss(block, UI_EMBOSSN);
		
		/* rounded header */
		//uiBlockSetCol(block, TH_BUT_SETTING1); // FIXME: maybe another color
			rb_col= (gpl->flag & GP_LAYER_ACTIVE)?50:20;
			uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-8, *yco-2, width, 24, NULL, 5.0, 0.0, 15 , rb_col-20, ""); 
		//uiBlockSetCol(block, TH_AUTO);
		
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
			
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else {
		height= 100;
		
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
		//uiBlockSetCol(block, TH_BUT_SETTING1); // fixme: maybe another color
			uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-8, *yco-height, width, height-1, NULL, 5.0, 0.0, 12, rb_col, ""); 
		//uiBlockSetCol(block, TH_AUTO);
		
		/* draw settings */
		{
			/* color */
			uiBlockBeginAlign(block);
				uiDefButF(block, COL, B_REDR, "",		*xco, *yco-26, 150, 19, gpl->color, 0, 0, 0, 0, "Color to use for all strokes on this Grease Pencil Layer");
				uiDefButF(block, NUMSLI, B_REDR, "Opacity: ",		*xco,*yco-45,150,19, &gpl->color[3], 0.3, 1.0, 0, 0, "Visibility of stroke (0.3 to 1.0)");
			uiBlockEndAlign(block);
			
			/* stroke thickness */
			uiDefButS(block, NUMSLI, B_REDR, "Thickness:",	*xco, *yco-75, 150, 20, &gpl->thickness, 1, 10, 0, 0, "Thickness of strokes (in pixels)");
			
			
			/* onion-skinning */
			uiBlockBeginAlign(block);
				uiDefButBitI(block, TOG, GP_LAYER_ONIONSKIN, B_REDR, "Onion-Skin", *xco+160, *yco-26, 140, 20, &gpl->flag, 0, 0, 0, 0, "Ghost frames on either side of frame");
				uiDefButS(block, NUMSLI, B_REDR, "GStep:",	*xco+160, *yco-46, 140, 20, &gpl->gstep, 0, 120, 0, 0, "Max number of frames on either side of active frame to show (0 = just 'first' available sketch on either side)");
			uiBlockEndAlign(block);
			
			/* options */
			but= uiDefBut(block, BUT, B_REDR, "Del Active Frame", *xco+160, *yco-75, 140, 20, NULL, 0, 0, 0, 0, "Erases the the active frame for this layer");
			uiButSetFunc(but, gp_ui_delframe_cb, gpd, gpl);
			
			but= uiDefBut(block, BUT, B_REDR, "Del Last Stroke", *xco+160, *yco-95, 140, 20, NULL, 0, 0, 0, 0, "Erases the last stroke from the active frame");
			uiButSetFunc(but, gp_ui_delstroke_cb, gpd, gpl);
			
			//uiDefButBitI(block, TOG, GP_LAYER_DRAWDEBUG, B_REDR, "Show Points", *xco+160, *yco-75, 130, 20, &gpl->flag, 0, 0, 0, 0, "Show points which form the strokes");
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
		
		
		/* show override lmb-clicks button */
		uiDefButBitI(block, TOG, GP_DATA_EDITPAINT, B_REDR, "Draw Mode", 170, 225, 150, 20, &gpd->flag, 0, 0, 0, 0, "Interpret LMB-click as new strokes (same as holding Shift-Key per stroke)");
		
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

/* flags for sflag */
enum {
	GP_DRAWDATA_NOSTATUS 	= (1<<0),	/* don't draw status info */
	GP_DRAWDATA_ONLY3D		= (1<<1),	/* only draw 3d-strokes */
	GP_DRAWDATA_ONLYV2D		= (1<<2),	/* only draw 'canvas' strokes */
};

/* draw a given stroke */
static void gp_draw_stroke (bGPDspoint *points, int totpoints, short thickness, short dflag, short sflag, short debug, int winx, int winy)
{
	bGPDspoint *pt;
	int i;
	
	/* error checking */
	if ((points == NULL) || (totpoints <= 0))
		return;
	
	/* check if stroke can be drawn */
	if ((dflag & GP_DRAWDATA_ONLY3D) && !(sflag & GP_STROKE_3DSPACE))
		return;
	if (!(dflag & GP_DRAWDATA_ONLY3D) && (sflag & GP_STROKE_3DSPACE))
		return;
	if ((dflag & GP_DRAWDATA_ONLYV2D) && !(sflag & GP_STROKE_2DSPACE))
		return;
	if (!(dflag & GP_DRAWDATA_ONLYV2D) && (sflag & GP_STROKE_2DSPACE))
		return;
	
	/* if drawing a single point, draw it larger */
	if (totpoints == 1) {		
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
	else {
		float oldpressure = 0.0f;
		
		/* draw stroke curve */
		glBegin(GL_LINE_STRIP);
		for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
			float x, y, z;
			
			if (sflag & GP_STROKE_3DSPACE) {
				x= pt->x;
				y= pt->y;
				z= pt->z;
			}
			else if (sflag & GP_STROKE_2DSPACE) {
				x= pt->x;
				y= pt->y;
				z= 0;
			}
			else {
				x= (pt->x / 1000 * winx);
				y= (pt->y / 1000 * winy);
				z= 0;
			}
			
			if (fabs(pt->pressure - oldpressure) > 0.2f) {
				glEnd();
				glLineWidth(pt->pressure * thickness);
				glBegin(GL_LINE_STRIP);
				
				if (sflag & GP_STROKE_3DSPACE) 
					glVertex3f(x, y, z);
				else
					glVertex2f(x, y);
				
				oldpressure = pt->pressure;
			}
			else {
				if (sflag & GP_STROKE_3DSPACE) 
					glVertex3f(x, y, z);
				else
					glVertex2f(x, y);
			}
		}
		glEnd();
		
		/* draw debug points of curve on top? */
		if (debug) {
			glBegin(GL_POINTS);
			for (i=0, pt=points; i < totpoints && pt; i++, pt++) {
				if (sflag & GP_STROKE_3DSPACE) {
					glVertex3f(pt->x, pt->y, pt->z);
				}
				else if (sflag & GP_STROKE_2DSPACE) {
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
		bGPDstroke *gps;
		
		short debug = (gpl->flag & GP_LAYER_DRAWDEBUG) ? 1 : 0;
		short lthick= gpl->thickness;
		float color[4];
		
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
						glColor4f(color[0], color[1], color[2], (color[3]-(i*0.7)));
						
						for (gps= gf->strokes.first; gps; gps= gps->next) {	
							gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
						}
					}
					else 
						break;
				}
				
				/* now draw next frames */
				for (gf= gpf->next, i=0; gf; gf=gf->next, i++) {
					/* check if frame is drawable */
					if ((gf->framenum - gpf->framenum) <= gpl->gstep) {
						/* alpha decreases with distance from curframe index */
						glColor4f(color[0], color[1], color[2], (color[3]-(i*0.7)));
						
						for (gps= gf->strokes.first; gps; gps= gps->next) {								
							gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
						}
					}
					else 
						break;
				}	
				
				/* restore alpha */
				glColor4f(color[0], color[1], color[2], color[3]);
			}
			else {
				/* draw the strokes for the ghost frames (at half of the alpha set by user) */
				glColor4f(color[0], color[1], color[2], (color[3] / 7));
				
				if (gpf->prev) {
					for (gps= gpf->prev->strokes.first; gps; gps= gps->next) {
						gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
					}
				}
				
				glColor4f(color[0], color[1], color[2], (color[3] / 4));
				if (gpf->next) {
					for (gps= gpf->next->strokes.first; gps; gps= gps->next) {	
						gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
					}
				}
				
				/* restore alpha */
				glColor4f(color[0], color[1], color[2], color[3]);
			}
		}
		
		/* draw the strokes already in active frame */
		for (gps= gpf->strokes.first; gps; gps= gps->next) {	
			gp_draw_stroke(gps->points, gps->totpoints, lthick, dflag, gps->flag, debug, winx, winy);
		}
		
		/* Check if may need to draw the active stroke cache, only if this layer is the active layer
		 * that is being edited. (Stroke cache is currently stored in gp-data)
		 */
		if ((G.f & G_GREASEPENCIL) && (gpl->flag & GP_LAYER_ACTIVE) &&
			(gpf->flag & GP_FRAME_PAINT)) 
		{
			/* Buffer stroke needs to be drawn with a different linestyle to help differentiate them from normal strokes. */
			setlinestyle(2);
			gp_draw_stroke(gpd->sbuffer, gpd->sbuffer_size, lthick, dflag, gpd->sbuffer_sflag, debug, winx, winy);
			setlinestyle(0);
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

/* ----------- */

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
