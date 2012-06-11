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
 * The Original Code is Copyright (C) 2008, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_buttons.c
 *  \ingroup edgpencil
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BLF_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_gpencil.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "gpencil_intern.h"

/* ************************************************** */
/* GREASE PENCIL PANEL-UI DRAWING */

/* Every space which implements Grease-Pencil functionality should have a panel
 * for the settings. All of the space-dependent parts should be coded in the panel
 * code for that space, but the rest is all handled by generic panel here.
 */

/* ------- Callbacks ----------- */
/* These are just 'dummy wrappers' around gpencil api calls */

/* make layer active one after being clicked on */
static void gp_ui_activelayer_cb(bContext *C, void *gpd, void *gpl)
{
	/* make sure the layer we want to remove is the active one */
	gpencil_layer_setactive(gpd, gpl);
	
	WM_event_add_notifier(C, NC_SCREEN | ND_GPENCIL | NA_EDITED, NULL); /* XXX please work! */
}

/* delete 'active' layer */
static void gp_ui_dellayer_cb(bContext *C, void *gpd, void *gpl)
{
	/* make sure the layer we want to remove is the active one */
	gpencil_layer_setactive(gpd, gpl);
	gpencil_layer_delactive(gpd);
	
	WM_event_add_notifier(C, NC_SCREEN | ND_GPENCIL | NA_EDITED, NULL); /* XXX please work! */
}


/* ------- Drawing Code ------- */

/* draw the controls for a given layer */
static void gp_drawui_layer(uiLayout *layout, bGPdata *gpd, bGPDlayer *gpl, const short is_v3d)
{
	uiLayout *box = NULL, *split = NULL;
	uiLayout *col = NULL;
	uiLayout *row = NULL, *sub = NULL;
	uiBlock *block;
	uiBut *but;
	PointerRNA ptr;
	int icon;
	
	/* make pointer to layer data */
	RNA_pointer_create((ID *)gpd, &RNA_GPencilLayer, gpl, &ptr);
	
	/* unless button has own callback, it adds this callback to button */
	block = uiLayoutGetBlock(layout);
	uiBlockSetFunc(block, gp_ui_activelayer_cb, gpd, gpl);
	
	/* draw header ---------------------------------- */
	/* get layout-row + UI-block for header */
	box = uiLayoutBox(layout);
	
	row = uiLayoutRow(box, 0);
	uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_EXPAND);
	block = uiLayoutGetBlock(row); /* err... */
	
	uiBlockSetEmboss(block, UI_EMBOSSN);
	
	/* left-align ............................... */
	sub = uiLayoutRow(row, 0);
	
	/* active */
	block = uiLayoutGetBlock(sub);
	icon = (gpl->flag & GP_LAYER_ACTIVE) ? ICON_RADIOBUT_ON : ICON_RADIOBUT_OFF;
	but = uiDefIconButBitI(block, TOG, GP_LAYER_ACTIVE, 0, icon, 0, 0, UI_UNIT_X, UI_UNIT_Y,
	                       &gpd->flag, 0.0, 0.0, 0.0, 0.0, TIP_("Set active layer"));
	uiButSetFunc(but, gp_ui_activelayer_cb, gpd, gpl);
	
	/* locked */
	icon = (gpl->flag & GP_LAYER_LOCKED) ? ICON_LOCKED : ICON_UNLOCKED;
	uiItemR(sub, &ptr, "lock", 0, "", icon);
	
	/* when layer is locked or hidden, only draw header */
	if (gpl->flag & (GP_LAYER_LOCKED | GP_LAYER_HIDE)) {
		char name[256]; /* gpl->info is 128, but we need space for 'locked/hidden' as well */
		
		/* visibility button (only if hidden but not locked!) */
		if ((gpl->flag & GP_LAYER_HIDE) && !(gpl->flag & GP_LAYER_LOCKED))
			uiItemR(sub, &ptr, "hide", 0, "", ICON_RESTRICT_VIEW_ON);
		
		/* name */
		if (gpl->flag & GP_LAYER_HIDE)
			BLI_snprintf(name, sizeof(name), IFACE_("%s (Hidden)"), gpl->info);
		else
			BLI_snprintf(name, sizeof(name), IFACE_("%s (Locked)"), gpl->info);
		uiItemL(sub, name, ICON_NONE);
		
		/* delete button (only if hidden but not locked!) */
		if ((gpl->flag & GP_LAYER_HIDE) && !(gpl->flag & GP_LAYER_LOCKED)) {
			/* right-align ............................... */
			sub = uiLayoutRow(row, 1);
			uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);
			block = uiLayoutGetBlock(sub); /* XXX... err... */
			
			but = uiDefIconBut(block, BUT, 0, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                   NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Delete layer"));
			uiButSetFunc(but, gp_ui_dellayer_cb, gpd, gpl);
		}
		uiBlockSetEmboss(block, UI_EMBOSS);
	}
	else {
		/* draw rest of header -------------------------------- */
		/* visibility button */
		uiItemR(sub, &ptr, "hide", 0, "", ICON_RESTRICT_VIEW_OFF);
		
		/* frame locking */
		/* TODO: this needs its own icons... */
		icon = (gpl->flag & GP_LAYER_FRAMELOCK) ? ICON_RENDER_STILL : ICON_RENDER_ANIMATION;
		uiItemR(sub, &ptr, "lock_frame", 0, "", icon);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		
		/* name */
		uiItemR(sub, &ptr, "info", 0, "", ICON_NONE);
		
		/* delete 'button' */
		uiBlockSetEmboss(block, UI_EMBOSSN);
		/* right-align ............................... */
		sub = uiLayoutRow(row, 1);
		uiLayoutSetAlignment(sub, UI_LAYOUT_ALIGN_RIGHT);
		block = uiLayoutGetBlock(sub); /* XXX... err... */
		
		but = uiDefIconBut(block, BUT, 0, ICON_X, 0, 0, UI_UNIT_X, UI_UNIT_Y,
		                   NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Delete layer"));
		uiButSetFunc(but, gp_ui_dellayer_cb, gpd, gpl);
		uiBlockSetEmboss(block, UI_EMBOSS);
		
		/* new backdrop ----------------------------------- */
		box = uiLayoutBox(layout);
		split = uiLayoutSplit(box, 0.5f, 0);
		
		/* draw settings ---------------------------------- */
		/* left column ..................... */
		col = uiLayoutColumn(split, 0);
		
		/* color */
		sub = uiLayoutColumn(col, 1);
		uiItemR(sub, &ptr, "color", 0, "", ICON_NONE);
		uiItemR(sub, &ptr, "alpha", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		
		/* stroke thickness */
		uiItemR(col, &ptr, "line_width", UI_ITEM_R_SLIDER, NULL, ICON_NONE);
		
		/* debugging options */
		if (G.debug & G_DEBUG) {
			uiItemR(col, &ptr, "show_points", 0, NULL, ICON_NONE);
		}
		
		/* right column ................... */
		col = uiLayoutColumn(split, 0);
		
		/* onion-skinning */
		sub = uiLayoutColumn(col, 1);
		uiItemR(sub, &ptr, "use_onion_skinning", 0, NULL, ICON_NONE);
		uiItemR(sub, &ptr, "ghost_range_max", 0, IFACE_("Frames"), ICON_NONE);
		
		/* 3d-view specific drawing options */
		if (is_v3d) {
			uiItemR(col, &ptr, "show_x_ray", 0, NULL, ICON_NONE);
		}
	}
}

/* stroke drawing options available */
typedef enum eGP_Stroke_Ops {
	STROKE_OPTS_NORMAL  = 0,
	STROKE_OPTS_V3D_OFF,
	STROKE_OPTS_V3D_ON,
} eGP_Stroke_Ops;

static void draw_gpencil_space_specials(const bContext *C, uiLayout *layout)
{
	uiLayout *col, *row;
	SpaceClip *sc = CTX_wm_space_clip(C);

	col = uiLayoutColumn(layout, 0);

	if (sc) {
		bScreen *screen = CTX_wm_screen(C);
		PointerRNA sc_ptr;

		RNA_pointer_create(&screen->id, &RNA_SpaceClipEditor, sc, &sc_ptr);
		row = uiLayoutRow(col, 1);
		uiItemR(row, &sc_ptr, "grease_pencil_source", UI_ITEM_R_EXPAND, NULL, ICON_NONE);
	}
}

/* Draw the contents for a grease-pencil panel*/
static void draw_gpencil_panel(bContext *C, uiLayout *layout, bGPdata *gpd, PointerRNA *ctx_ptr)
{
	PointerRNA gpd_ptr;
	bGPDlayer *gpl;
	uiLayout *col, *row;
	SpaceClip *sc = CTX_wm_space_clip(C);
	short v3d_stroke_opts = STROKE_OPTS_NORMAL;
	const short is_v3d = CTX_wm_view3d(C) != NULL;
	
	/* make new PointerRNA for Grease Pencil block */
	RNA_id_pointer_create((ID *)gpd, &gpd_ptr);
	
	/* draw gpd settings first ------------------------------------- */
	col = uiLayoutColumn(layout, 0);

	/* current Grease Pencil block */
	/* TODO: show some info about who owns this? */
	uiTemplateID(col, C, ctx_ptr, "grease_pencil", "GPENCIL_OT_data_add", NULL, "GPENCIL_OT_data_unlink");
	
	/* add new layer button - can be used even when no data, since it can add a new block too */
	uiItemO(col, IFACE_("New Layer"), ICON_NONE, "GPENCIL_OT_layer_add");
	row = uiLayoutRow(col, 1);
	uiItemO(row, IFACE_("Delete Frame"), ICON_NONE, "GPENCIL_OT_active_frame_delete");
	uiItemO(row, IFACE_("Convert"), ICON_NONE, "GPENCIL_OT_convert");
	
	/* sanity checks... */
	if (gpd == NULL)
		return;
	
	/* draw each layer --------------------------------------------- */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		col = uiLayoutColumn(layout, 1);
		gp_drawui_layer(col, gpd, gpl, is_v3d);
	}
	
	/* draw gpd drawing settings first ------------------------------------- */
	col = uiLayoutColumn(layout, 1);
	/* label */
	uiItemL(col, IFACE_("Drawing Settings:"), ICON_NONE);
		
	/* check whether advanced 3D-View drawing space options can be used */
	if (is_v3d) {
		if (gpd->flag & (GP_DATA_DEPTH_STROKE | GP_DATA_DEPTH_VIEW))
			v3d_stroke_opts = STROKE_OPTS_V3D_ON;
		else
			v3d_stroke_opts = STROKE_OPTS_V3D_OFF;
	}
		
	/* drawing space options */
	row = uiLayoutRow(col, 1);
	uiItemEnumR_string(row, &gpd_ptr, "draw_mode", "VIEW", NULL, ICON_NONE);
	uiItemEnumR_string(row, &gpd_ptr, "draw_mode", "CURSOR", NULL, ICON_NONE);

	if (sc == NULL) {
		row = uiLayoutRow(col, 1);
		uiLayoutSetActive(row, v3d_stroke_opts);
		uiItemEnumR_string(row, &gpd_ptr, "draw_mode", "SURFACE", NULL, ICON_NONE);
		uiItemEnumR_string(row, &gpd_ptr, "draw_mode", "STROKE", NULL, ICON_NONE);

		row = uiLayoutRow(col, 0);
		uiLayoutSetActive(row, v3d_stroke_opts == STROKE_OPTS_V3D_ON);
		uiItemR(row, &gpd_ptr, "use_stroke_endpoints", 0, NULL, ICON_NONE);
	}
}


/* Standard panel to be included wherever Grease Pencil is used... */
void gpencil_panel_standard(const bContext *C, Panel *pa)
{
	bGPdata **gpd_ptr = NULL;
	PointerRNA ptr;
	
	/* if (v3d->flag2 & V3D_DISPGP)... etc. */
	
	draw_gpencil_space_specials(C, pa->layout);
	
	/* get pointer to Grease Pencil Data */
	gpd_ptr = gpencil_data_get_pointers((bContext *)C, &ptr);
	
	if (gpd_ptr)
		draw_gpencil_panel((bContext *)C, pa->layout, *gpd_ptr, &ptr);
}

/* ************************************************** */
