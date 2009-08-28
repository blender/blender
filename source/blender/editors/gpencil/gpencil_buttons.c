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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
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

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_gpencil_types.h"
#include "DNA_listBase.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_utildefines.h"

#include "PIL_time.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_gpencil.h"
#include "ED_sequencer.h"
#include "ED_util.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "gpencil_intern.h"

/* ************************************************** */
/* GREASE PENCIL PANEL-UI DRAWING */

/* Every space which implements Grease-Pencil functionality should have a panel
 * for the settings. All of the space-dependent parts should be coded in the panel
 * code for that space, but the rest is all handled by generic panel here.
 */

/* ------- Callbacks ----------- */
/* These are just 'dummy wrappers' around gpencil api calls */

#if 0
// XXX
/* make layer active one after being clicked on */
void gp_ui_activelayer_cb (void *gpd, void *gpl)
{
	gpencil_layer_setactive(gpd, gpl);
	
	scrarea_queue_winredraw(curarea);
	allqueue(REDRAWACTION, 0);
}

/* rename layer and set active */
void gp_ui_renamelayer_cb (void *gpd_arg, void *gpl_arg)
{
	bGPdata *gpd= (bGPdata *)gpd_arg;
	bGPDlayer *gpl= (bGPDlayer *)gpl_arg;
	
	BLI_uniquename(&gpd->layers, gpl, "GP_Layer", '.', offsetof(bGPDlayer, info[0]), 128);
	gpencil_layer_setactive(gpd, gpl);
	
	scrarea_queue_winredraw(curarea);
	allqueue(REDRAWACTION, 0);
}

/* add a new layer */
void gp_ui_addlayer_cb (void *gpd, void *dummy)
{
	gpencil_layer_addnew(gpd);
	
	scrarea_queue_winredraw(curarea);
	allqueue(REDRAWACTION, 0);
}

/* delete active layer */
void gp_ui_dellayer_cb (void *gpd, void *dummy)
{
	gpencil_layer_delactive(gpd);
	
	scrarea_queue_winredraw(curarea);
	allqueue(REDRAWACTION, 0);
}

/* delete last stroke of active layer */
void gp_ui_delstroke_cb (void *gpd, void *gpl)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	
	if (gpf) {
		if (gpf->framenum != CFRA) return;

		gpencil_layer_setactive(gpd, gpl);
		gpencil_frame_delete_laststroke(gpl, gpf);
		
		scrarea_queue_winredraw(curarea);
	}
}

/* delete active frame of active layer */
void gp_ui_delframe_cb (void *gpd, void *gpl)
{
	bGPDframe *gpf= gpencil_layer_getframe(gpl, CFRA, 0);
	
	gpencil_layer_setactive(gpd, gpl);
	gpencil_layer_delframe(gpl, gpf);
	
	scrarea_queue_winredraw(curarea);
	allqueue(REDRAWACTION, 0);
}

/* convert the active layer to geometry */
void gp_ui_convertlayer_cb (void *gpd, void *gpl)
{
	gpencil_layer_setactive(gpd, gpl);
	gpencil_convert_menu();
	
	scrarea_queue_winredraw(curarea);
}
#endif

/* ------- Drawing Code ------- */

#if 0
/* XXX */
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
			uiDefBut(block, ROUNDBOX, B_REDR, "", *xco-8, *yco-2, width, 24, NULL, 5.0, 0.0, 15.0, (float)(rb_col-20), ""); 
		if (active) uiBlockSetCol(block, TH_AUTO);
		
		/* lock toggle */
		uiDefIconButBitI(block, ICONTOG, GP_LAYER_LOCKED, B_REDR, ICON_UNLOCKED,	*xco-7, *yco-1, 20, 20, &gpl->flag, 0.0, 0.0, 0, 0, "Layer cannot be modified");
	}
	
	/* when layer is locked or hidden, only draw header */
	if (gpl->flag & (GP_LAYER_LOCKED|GP_LAYER_HIDE)) {
		char name[256]; /* gpl->info is 128, but we need space for 'locked/hidden' as well */
		
		height= 0;
		
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
			uiDefBut(block, ROUNDBOX, B_DIFF, "", *xco-8, *yco-height, width, height-1, NULL, 5.0, 0.0, 12.0, (float)rb_col, ""); 
		if (active) uiBlockSetCol(block, TH_AUTO);
		
		/* draw settings */
		{
			/* color */
			uiBlockBeginAlign(block);
				uiDefButF(block, COL, B_REDR, "",		*xco, *yco-26, 150, 19, gpl->color, 0, 0, 0, 0, "Color to use for all strokes on this Grease Pencil Layer");
				uiDefButF(block, NUMSLI, B_REDR, "Opacity: ",		*xco,*yco-45,150,19, &gpl->color[3], 0.3f, 1.0f, 0, 0, "Visibility of stroke (0.3 to 1.0)");
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
				if (curarea->spacetype == SPACE_VIEW3D) {
					but= uiDefBut(block, BUT, B_REDR, "Convert to...", *xco+160, *yco-75, 140, 20, NULL, 0, 0, 0, 0, "Converts this layer's strokes to geometry (Hotkey = Alt-Shift-C)");
					uiButSetFunc(but, gp_ui_convertlayer_cb, gpd, gpl);
				}
				else {
					but= uiDefBut(block, BUT, B_REDR, "Del Active Frame", *xco+160, *yco-75, 140, 20, NULL, 0, 0, 0, 0, "Erases the the active frame for this layer (Hotkey = Alt-XKEY/DEL)");
					uiButSetFunc(but, gp_ui_delframe_cb, gpd, gpl);
				}
				
				but= uiDefBut(block, BUT, B_REDR, "Del Last Stroke", *xco+160, *yco-95, 140, 20, NULL, 0, 0, 0, 0, "Erases the last stroke from the active frame (Hotkey = Alt-XKEY/DEL)");
				uiButSetFunc(but, gp_ui_delstroke_cb, gpd, gpl);
			uiBlockEndAlign(block);
		}
	}
	
	/* adjust height for new to start */
	(*yco) -= (height + 27); 
} 
#endif
/* Draw the contents for a grease-pencil panel. This assumes several things:
 * 	- that panel has been created, is 318 x 204. max yco is 225
 *	- that a toggle for turning on/off gpencil drawing is 150 x 20, starting from (10,225)
 *		which is basically the top left-hand corner
 * It will return the amount of extra space to extend the panel by
 */
short draw_gpencil_panel (uiBlock *block, bGPdata *gpd, ScrArea *sa)
{
#if 0
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
		else
			uiDefButBitI(block, TOG, GP_DATA_VIEWALIGN, B_REDR, "Stick to View", 170, 205, 150, 20, &gpd->flag, 0, 0, 0, 0, "New strokes are added on 2d-canvas");
	}
	
	/* draw for each layer */
	for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
		gp_drawui_layer(block, gpd, gpl, &xco, &yco);
	}
	
	/* return new height if necessary */
	return (yco < 0) ? (204 - yco) : 204;
#endif
	return 0;
}	

/* ************************************************** */
