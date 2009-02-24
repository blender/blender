/**
 * $Id:
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "ED_screen.h"

#include "BIF_transform.h"

#include "graph_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* ************************** view-based operators **********************************/
// XXX this probably shouldn't be here..

/* Toggle Handles ----------------------------------------------------------------- */

static int view_toggle_handles_exec (bContext *C, wmOperator *op)
{
	SpaceIpo *sipo= (SpaceIpo *)CTX_wm_space_data(C);
	ARegion *ar= CTX_wm_region(C);
	
	if (sipo == NULL)
		return OPERATOR_CANCELLED;
	
	/* toggle flag to hide handles */
	sipo->flag ^= SIPO_NOHANDLES;
	
	/* request refresh of keys area */
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void GRAPHEDIT_OT_view_togglehandles (wmOperatorType *ot)
{
	/* identification */
	ot->name= "Show/Hide All Handles";
	ot->idname= "GRAPHEDIT_OT_view_toggle_handles";
	
	/* callbacks */
	ot->exec= view_toggle_handles_exec;
	ot->poll= ED_operator_areaactive;
}

/* ************************** registration - operator types **********************************/

void graphedit_operatortypes(void)
{
	/* view */
	WM_operatortype_append(GRAPHEDIT_OT_view_togglehandles);
	WM_operatortype_append(GRAPHEDIT_OT_set_previewrange);
	WM_operatortype_append(GRAPHEDIT_OT_view_all);
	WM_operatortype_append(GRAPHEDIT_OT_properties);
	
	/* keyframes */
		/* selection */
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_clickselect);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_deselectall);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_borderselect);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_columnselect);
	
		/* editing */
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_snap);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_mirror);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_cfrasnap);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_handletype);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_interpolation_type);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_extrapolation_type);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_sample);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_smooth);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_clean);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_delete);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_duplicate);
	
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_copy);
	WM_operatortype_append(GRAPHEDIT_OT_keyframes_paste);
	
	//TODO: insertkey...
}

/* ************************** registration - keymaps **********************************/

static void graphedit_keymap_keyframes (wmWindowManager *wm, ListBase *keymap)
{
	/* view */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_view_toggle_handles", HKEY, KM_PRESS, KM_CTRL, 0);
	
	/* graph_select.c - selection tools */
		/* click-select */
		// TODO: column to alt, left-right to ctrl (for select-linked consistency)
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, KM_CTRL, 0)->ptr, "column_select", 1);
	RNA_boolean_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0)->ptr, "extend_select", 1);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_clickselect", SELECTMOUSE, KM_PRESS, KM_ALT, 0)->ptr, "left_right", GRAPHKEYS_LRSEL_TEST);
	
		/* deselect all */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_deselectall", AKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_deselectall", IKEY, KM_PRESS, KM_CTRL, 0)->ptr, "invert", 1);
	
		/* borderselect */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_borderselect", BKEY, KM_PRESS, 0, 0);
	RNA_boolean_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_borderselect", BKEY, KM_PRESS, KM_ALT, 0)->ptr, "axis_range", 1);
	
		/* column select */
		// XXX KKEY would be nice to keep for 'keyframe' lines
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_columnselect", KKEY, KM_PRESS, 0, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_KEYS);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_columnselect", KKEY, KM_PRESS, KM_CTRL, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_CFRA);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_columnselect", KKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN);
	RNA_enum_set(WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_columnselect", KKEY, KM_PRESS, KM_ALT, 0)->ptr, "mode", GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN);
	
	
	/* graph_edit.c */
		/* snap - current frame to selected keys */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_cfrasnap", SKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
		
		/* menu + single-step transform */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_snap", SKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_mirror", MKEY, KM_PRESS, KM_SHIFT, 0);
	
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_handletype", HKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_interpolation_type", TKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_extrapolation_type", EKEY, KM_PRESS, KM_SHIFT, 0);
	
	
		/* destructive */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_clean", OKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_sample", OKEY, KM_PRESS, KM_SHIFT, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_smooth", OKEY, KM_PRESS, KM_ALT, 0);
	
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_delete", XKEY, KM_PRESS, 0, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_delete", DELKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_duplicate", DKEY, KM_PRESS, KM_SHIFT, 0);
	
		/* insertkey */
	// TODO..
	
		/* copy/paste */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_copy", CKEY, KM_PRESS, KM_CTRL, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_keyframes_paste", VKEY, KM_PRESS, KM_CTRL, 0);
	
		/* auto-set range */
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_set_previewrange", PKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_view_all", HOMEKEY, KM_PRESS, 0, 0);
	
	/* transform system */
	transform_keymap_for_space(wm, keymap, SPACE_IPO);
}

/* --------------- */

void graphedit_keymap(wmWindowManager *wm)
{
	ListBase *keymap;
	
	/* keymap for all regions */
	keymap= WM_keymap_listbase(wm, "GraphEdit Generic", SPACE_IPO, 0);
	WM_keymap_add_item(keymap, "GRAPHEDIT_OT_properties", NKEY, KM_PRESS, 0, 0);

	/* channels */
	/* Channels are not directly handled by the Graph Editor module, but are inherited from the Animation module. 
	 * All the relevant operations, keymaps, drawing, etc. can therefore all be found in that module instead, as these
	 * are all used for the Graph Editor too.
	 */
	
	/* keyframes */
	keymap= WM_keymap_listbase(wm, "GraphEdit Keys", SPACE_IPO, 0);
	graphedit_keymap_keyframes(wm, keymap);
}

