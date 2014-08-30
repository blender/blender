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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_graph/graph_buttons.c
 *  \ingroup spgraph
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_screen.h"
#include "BKE_unit.h"


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "graph_intern.h"   // own include

/* ******************* graph editor space & buttons ************** */

#define B_REDR 1

/* -------------- */

static int graph_panel_context(const bContext *C, bAnimListElem **ale, FCurve **fcu)
{
	bAnimContext ac;
	bAnimListElem *elem = NULL;
	
	/* for now, only draw if we could init the anim-context info (necessary for all animation-related tools) 
	 * to work correctly is able to be correctly retrieved. There's no point showing empty panels?
	 */
	if (ANIM_animdata_get_context(C, &ac) == 0) 
		return 0;
	
	/* try to find 'active' F-Curve */
	elem = get_active_fcurve_channel(&ac);
	if (elem == NULL) 
		return 0;
	
	if (fcu)
		*fcu = (FCurve *)elem->data;
	if (ale)
		*ale = elem;
	else
		MEM_freeN(elem);
	
	return 1;
}

static int graph_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	return graph_panel_context(C, NULL, NULL);
}

/* -------------- */

/* Graph Editor View Settings */
static void graph_panel_view(const bContext *C, Panel *pa)
{
	bScreen *sc = CTX_wm_screen(C);
	SpaceIpo *sipo = CTX_wm_space_graph(C);
	Scene *scene = CTX_data_scene(C);
	PointerRNA spaceptr, sceneptr;
	uiLayout *col, *sub, *row;
	
	/* get RNA pointers for use when creating the UI elements */
	RNA_id_pointer_create(&scene->id, &sceneptr);
	RNA_pointer_create(&sc->id, &RNA_SpaceGraphEditor, sipo, &spaceptr);

	/* 2D-Cursor */
	col = uiLayoutColumn(pa->layout, false);
	uiItemR(col, &spaceptr, "show_cursor", 0, NULL, ICON_NONE);
		
	sub = uiLayoutColumn(col, true);
	uiLayoutSetActive(sub, RNA_boolean_get(&spaceptr, "show_cursor"));
	uiItemO(sub, IFACE_("Cursor from Selection"), ICON_NONE, "GRAPH_OT_frame_jump");

	sub = uiLayoutColumn(col, true);
	uiLayoutSetActive(sub, RNA_boolean_get(&spaceptr, "show_cursor"));
	row = uiLayoutSplit(sub, 0.7f, true);
	uiItemR(row, &sceneptr, "frame_current", 0, IFACE_("Cursor X"), ICON_NONE);
	uiItemEnumO(row, "GRAPH_OT_snap", IFACE_("To Keys"), 0, "type", GRAPHKEYS_SNAP_CFRA);
	
	row = uiLayoutSplit(sub, 0.7f, true);
	uiItemR(row, &spaceptr, "cursor_position_y", 0, IFACE_("Cursor Y"), ICON_NONE);
	uiItemEnumO(row, "GRAPH_OT_snap", IFACE_("To Keys"), 0, "type", GRAPHKEYS_SNAP_VALUE);
}

/* ******************* active F-Curve ************** */

static void graph_panel_properties(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	PointerRNA fcu_ptr;
	uiLayout *layout = pa->layout;
	uiLayout *col, *row, *sub;
	// uiBlock *block;  // UNUSED
	char name[256];
	int icon = 0;

	if (!graph_panel_context(C, &ale, &fcu))
		return;
	
	// UNUSED
	// block = uiLayoutGetBlock(layout);
	// uiBlockSetHandleFunc(block, do_graph_region_buttons, NULL);
	
	/* F-Curve pointer */
	RNA_pointer_create(ale->id, &RNA_FCurve, fcu, &fcu_ptr);
	
	/* user-friendly 'name' for F-Curve */
	/* TODO: only show the path if this is invalid? */
	col = uiLayoutColumn(layout, false);
	icon = getname_anim_fcurve(name, ale->id, fcu);
	uiItemL(col, name, icon);
		
	/* RNA-Path Editing - only really should be enabled when things aren't working */
	col = uiLayoutColumn(layout, true);
	uiLayoutSetEnabled(col, (fcu->flag & FCURVE_DISABLED) != 0);
	uiItemR(col, &fcu_ptr, "data_path", 0, "", ICON_RNA);
	uiItemR(col, &fcu_ptr, "array_index", 0, NULL, ICON_NONE);
		
	/* color settings */
	col = uiLayoutColumn(layout, true);
	uiItemL(col, IFACE_("Display Color:"), ICON_NONE);
		
	row = uiLayoutRow(col, true);
	uiItemR(row, &fcu_ptr, "color_mode", 0, "", ICON_NONE);
			
	sub = uiLayoutRow(row, true);
	uiLayoutSetEnabled(sub, (fcu->color_mode == FCURVE_COLOR_CUSTOM));
	uiItemR(sub, &fcu_ptr, "color", 0, "", ICON_NONE);
	
	MEM_freeN(ale);
}

/* ******************* active Keyframe ************** */

/* get 'active' keyframe for panel editing */
static short get_active_fcurve_keyframe_edit(FCurve *fcu, BezTriple **bezt, BezTriple **prevbezt)
{
	BezTriple *b;
	int i;
	
	/* zero the pointers */
	*bezt = *prevbezt = NULL;
	
	/* sanity checks */
	if ((fcu->bezt == NULL) || (fcu->totvert == 0))
		return 0;
		
	/* find first selected keyframe for now, and call it the active one 
	 *	- this is a reasonable assumption, given that whenever anyone 
	 *	  wants to edit numerically, there is likely to only be 1 vert selected
	 */
	for (i = 0, b = fcu->bezt; i < fcu->totvert; i++, b++) {
		if (BEZSELECTED(b)) {
			/* found 
			 *	- 'previous' is either the one before, of the keyframe itself (which is still fine)
			 *		XXX: we can just make this null instead if needed
			 */
			*prevbezt = (i > 0) ? b - 1 : b;
			*bezt = b;
			
			return 1;
		}
	}
	
	/* not found */
	return 0;
}

/* update callback for active keyframe properties - base updates stuff */
static void graphedit_activekey_update_cb(bContext *UNUSED(C), void *fcu_ptr, void *UNUSED(bezt_ptr))
{
	FCurve *fcu = (FCurve *)fcu_ptr;
	
	/* make sure F-Curve and its handles are still valid after this editing */
	sort_time_fcurve(fcu);
	calchandles_fcurve(fcu);
}

/* update callback for active keyframe properties - handle-editing wrapper */
static void graphedit_activekey_handles_cb(bContext *C, void *fcu_ptr, void *bezt_ptr)
{
	BezTriple *bezt = (BezTriple *)bezt_ptr;
	
	/* since editing the handles, make sure they're set to types which are receptive to editing 
	 * see transform_conversions.c :: createTransGraphEditData(), last step in second loop
	 */
	if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM) && ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM)) {
		/* by changing to aligned handles, these can now be moved... */
		bezt->h1 = HD_ALIGN;
		bezt->h2 = HD_ALIGN;
	}
	else {
		BKE_nurb_bezt_handle_test(bezt, true);
	}
	
	/* now call standard updates */
	graphedit_activekey_update_cb(C, fcu_ptr, bezt_ptr);
}

/* update callback for editing coordinates of right handle in active keyframe properties
 * NOTE: we cannot just do graphedit_activekey_handles_cb() due to "order of computation"
 *       weirdness (see calchandleNurb_intern() and T39911)
 */
static void graphedit_activekey_left_handle_coord_cb(bContext *C, void *fcu_ptr, void *bezt_ptr)
{
	BezTriple *bezt = (BezTriple *)bezt_ptr;

	const char f1 = bezt->f1;
	const char f3 = bezt->f3;

	bezt->f1 |= SELECT;
	bezt->f3 &= ~SELECT;

	/* perform normal updates NOW */
	graphedit_activekey_handles_cb(C, fcu_ptr, bezt_ptr);

	/* restore selection state so that no-one notices this hack */
	bezt->f1 = f1;
	bezt->f3 = f3;
}

static void graphedit_activekey_right_handle_coord_cb(bContext *C, void *fcu_ptr, void *bezt_ptr)
{
	BezTriple *bezt = (BezTriple *)bezt_ptr;
	
	/* original state of handle selection - to be restored after performing the recalculation */
	const char f1 = bezt->f1;
	const char f3 = bezt->f3;
	
	/* temporarily make it so that only the right handle is selected, so that updates go correctly 
	 * (i.e. it now acts as if we've just transforming the vert when it is selected by itself)
	 */
	bezt->f1 &= ~SELECT;
	bezt->f3 |= SELECT;
	
	/* perform normal updates NOW */
	graphedit_activekey_handles_cb(C, fcu_ptr, bezt_ptr);
	
	/* restore selection state so that no-one notices this hack */
	bezt->f1 = f1;
	bezt->f3 = f3;
}

static void graph_panel_key_properties(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	BezTriple *bezt, *prevbezt;
	
	uiLayout *layout = pa->layout;
	uiLayout *col;
	uiBlock *block;

	if (!graph_panel_context(C, &ale, &fcu))
		return;
	
	block = uiLayoutGetBlock(layout);
	/* uiBlockSetHandleFunc(block, do_graph_region_buttons, NULL); */
	
	/* only show this info if there are keyframes to edit */
	if (get_active_fcurve_keyframe_edit(fcu, &bezt, &prevbezt)) {
		PointerRNA bezt_ptr, id_ptr, fcu_prop_ptr;
		PropertyRNA *fcu_prop = NULL;
		uiBut *but;
		int unit = B_UNIT_NONE;
		
		/* RNA pointer to keyframe, to allow editing */
		RNA_pointer_create(ale->id, &RNA_Keyframe, bezt, &bezt_ptr);
		
		/* get property that F-Curve affects, for some unit-conversion magic */
		RNA_id_pointer_create(ale->id, &id_ptr);
		if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &fcu_prop_ptr, &fcu_prop)) {
			/* determine the unit for this property */
			unit = RNA_SUBTYPE_UNIT(RNA_property_subtype(fcu_prop));
		}
		
		/* interpolation */
		col = uiLayoutColumn(layout, false);
		uiItemR(col, &bezt_ptr, "interpolation", 0, NULL, ICON_NONE);
		
		/* easing type */
		if (bezt->ipo > BEZT_IPO_BEZ)
			uiItemR(col, &bezt_ptr, "easing", 0, NULL, 0);

		/* easing extra */
		switch (bezt->ipo) {
			case BEZT_IPO_BACK:
				col = uiLayoutColumn(layout, 1);
				uiItemR(col, &bezt_ptr, "back", 0, NULL, 0);
				break;
			case BEZT_IPO_ELASTIC:
				col = uiLayoutColumn(layout, 1);
				uiItemR(col, &bezt_ptr, "amplitude", 0, NULL, 0);
				uiItemR(col, &bezt_ptr, "period", 0, NULL, 0);
				break;
			default:
				break;
		}
		
		/* numerical coordinate editing 
		 *  - we use the button-versions of the calls so that we can attach special update handlers
		 *    and unit conversion magic that cannot be achieved using a purely RNA-approach
		 */
		col = uiLayoutColumn(layout, true);
		/* keyframe itself */
		{
			uiItemL(col, IFACE_("Key:"), ICON_NONE);
			
			but = uiDefButR(block, NUM, B_REDR, IFACE_("Frame:"), 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "co", 0, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, graphedit_activekey_update_cb, fcu, bezt);
			
			but = uiDefButR(block, NUM, B_REDR, IFACE_("Value:"), 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "co", 1, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, graphedit_activekey_update_cb, fcu, bezt);
			uiButSetUnitType(but, unit);
		}
		
		/* previous handle - only if previous was Bezier interpolation */
		if ((prevbezt) && (prevbezt->ipo == BEZT_IPO_BEZ)) {
			uiItemL(col, IFACE_("Left Handle:"), ICON_NONE);
			
			but = uiDefButR(block, NUM, B_REDR, "X:", 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "handle_left", 0, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, graphedit_activekey_left_handle_coord_cb, fcu, bezt);
			
			but = uiDefButR(block, NUM, B_REDR, "Y:", 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "handle_left", 1, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, graphedit_activekey_left_handle_coord_cb, fcu, bezt);
			uiButSetUnitType(but, unit);
			
			/* XXX: with label? */
			but = uiDefButR(block, MENU, B_REDR, NULL, 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "handle_left_type", 0, 0, 0, -1, -1, "Type of left handle");
			uiButSetFunc(but, graphedit_activekey_handles_cb, fcu, bezt);
		}
		
		/* next handle - only if current is Bezier interpolation */
		if (bezt->ipo == BEZT_IPO_BEZ) {
			/* NOTE: special update callbacks are needed on the coords here due to T39911 */
			uiItemL(col, IFACE_("Right Handle:"), ICON_NONE);
			
			but = uiDefButR(block, NUM, B_REDR, "X:", 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "handle_right", 0, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, graphedit_activekey_right_handle_coord_cb, fcu, bezt);
			
			but = uiDefButR(block, NUM, B_REDR, "Y:", 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "handle_right", 1, 0, 0, -1, -1, NULL);
			uiButSetFunc(but, graphedit_activekey_right_handle_coord_cb, fcu, bezt);
			uiButSetUnitType(but, unit);
			
			/* XXX: with label? */
			but = uiDefButR(block, MENU, B_REDR, NULL, 0, 0, UI_UNIT_X, UI_UNIT_Y,
			                &bezt_ptr, "handle_right_type", 0, 0, 0, -1, -1, "Type of right handle");
			uiButSetFunc(but, graphedit_activekey_handles_cb, fcu, bezt);
		}
	}
	else {
		if ((fcu->bezt == NULL) && (fcu->modifiers.first)) {
			/* modifiers only - so no keyframes to be active */
			uiItemL(layout, IFACE_("F-Curve only has F-Modifiers"), ICON_NONE);
			uiItemL(layout, IFACE_("See Modifiers panel below"), ICON_INFO);
		}
		else if (fcu->fpt) {
			/* samples only */
			uiItemL(layout, IFACE_("F-Curve doesn't have any keyframes as it only contains sampled points"),
			        ICON_NONE);
		}
		else
			uiItemL(layout, IFACE_("No active keyframe on F-Curve"), ICON_NONE);
	}
	
	MEM_freeN(ale);
}

/* ******************* drivers ******************************** */

#define B_IPO_DEPCHANGE     10

static void do_graph_region_driver_buttons(bContext *C, void *UNUSED(arg), int event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	
	switch (event) {
		case B_IPO_DEPCHANGE:
		{
			/* rebuild depsgraph for the new deps */
			DAG_relations_tag_update(bmain);
			break;
		}
	}
	
	/* default for now */
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, scene); // XXX could use better notifier
}

/* callback to remove the active driver */
static void driver_remove_cb(bContext *C, void *ale_v, void *UNUSED(arg))
{
	bAnimListElem *ale = (bAnimListElem *)ale_v;
	ID *id = ale->id;
	FCurve *fcu = ale->data;
	ReportList *reports = CTX_wm_reports(C);
	
	/* try to get F-Curve that driver lives on, and ID block which has this AnimData */
	if (ELEM(NULL, id, fcu))
		return;
	
	/* call API method to remove this driver  */
	ANIM_remove_driver(reports, id, fcu->rna_path, fcu->array_index, 0);
}

/* callback to add a target variable to the active driver */
static void driver_add_var_cb(bContext *UNUSED(C), void *driver_v, void *UNUSED(arg))
{
	ChannelDriver *driver = (ChannelDriver *)driver_v;
	
	/* add a new variable */
	driver_add_new_variable(driver);
}

/* callback to remove target variable from active driver */
static void driver_delete_var_cb(bContext *UNUSED(C), void *driver_v, void *dvar_v)
{
	ChannelDriver *driver = (ChannelDriver *)driver_v;
	DriverVar *dvar = (DriverVar *)dvar_v;
	
	/* remove the active variable */
	driver_free_variable(driver, dvar);
}

/* callback to reset the driver's flags */
static void driver_update_flags_cb(bContext *UNUSED(C), void *fcu_v, void *UNUSED(arg))
{
	FCurve *fcu = (FCurve *)fcu_v;
	ChannelDriver *driver = fcu->driver;
	
	/* clear invalid flags */
	fcu->flag &= ~FCURVE_DISABLED;
	driver->flag &= ~DRIVER_FLAG_INVALID;
}

/* drivers panel poll */
static int graph_panel_drivers_poll(const bContext *C, PanelType *UNUSED(pt))
{
	SpaceIpo *sipo = CTX_wm_space_graph(C);

	if (sipo->mode != SIPO_MODE_DRIVERS)
		return 0;

	return graph_panel_context(C, NULL, NULL);
}

/* settings for 'single property' driver variable type */
static void graph_panel_driverVar__singleProp(uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar = &dvar->targets[0];
	PointerRNA dtar_ptr;
	uiLayout *row, *col;
	
	/* initialize RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar, &dtar_ptr); 
	
	/* Target ID */
	row = uiLayoutRow(layout, false);
	uiLayoutSetRedAlert(row, ((dtar->flag & DTAR_FLAG_INVALID) && !dtar->id));
	uiTemplateAnyID(row, &dtar_ptr, "id", "id_type", IFACE_("Prop:"));
	
	/* Target Property */
	if (dtar->id) {
		PointerRNA root_ptr;
		
		/* get pointer for resolving the property selected */
		RNA_id_pointer_create(dtar->id, &root_ptr);
		
		/* rna path */
		col = uiLayoutColumn(layout, true);
		uiLayoutSetRedAlert(col, (dtar->flag & DTAR_FLAG_INVALID));
		uiTemplatePathBuilder(col, &dtar_ptr, "data_path", &root_ptr, IFACE_("Path"));
	}
}

/* settings for 'rotation difference' driver variable type */
static void graph_panel_driverVar__rotDiff(uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar = &dvar->targets[0];
	DriverTarget *dtar2 = &dvar->targets[1];
	Object *ob1 = (Object *)dtar->id;
	Object *ob2 = (Object *)dtar2->id;
	PointerRNA dtar_ptr, dtar2_ptr;
	uiLayout *col;
	
	/* initialize RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar, &dtar_ptr); 
	RNA_pointer_create(id, &RNA_DriverTarget, dtar2, &dtar2_ptr); 
	
	/* Bone 1 */
	col = uiLayoutColumn(layout, true);
	uiLayoutSetRedAlert(col, (dtar->flag & DTAR_FLAG_INVALID)); /* XXX: per field... */
	uiTemplateAnyID(col, &dtar_ptr, "id", "id_type", IFACE_("Bone 1:"));
	
	if (dtar->id && GS(dtar->id->name) == ID_OB && ob1->pose) {
		PointerRNA tar_ptr;
		
		RNA_pointer_create(dtar->id, &RNA_Pose, ob1->pose, &tar_ptr);
		uiItemPointerR(col, &dtar_ptr, "bone_target", &tar_ptr, "bones", "", ICON_BONE_DATA);
	}
	
	col = uiLayoutColumn(layout, true);
	uiLayoutSetRedAlert(col, (dtar2->flag & DTAR_FLAG_INVALID)); /* XXX: per field... */
	uiTemplateAnyID(col, &dtar2_ptr, "id", "id_type", IFACE_("Bone 2:"));
		
	if (dtar2->id && GS(dtar2->id->name) == ID_OB && ob2->pose) {
		PointerRNA tar_ptr;
		
		RNA_pointer_create(dtar2->id, &RNA_Pose, ob2->pose, &tar_ptr);
		uiItemPointerR(col, &dtar2_ptr, "bone_target", &tar_ptr, "bones", "", ICON_BONE_DATA);
	}
}

/* settings for 'location difference' driver variable type */
static void graph_panel_driverVar__locDiff(uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar  = &dvar->targets[0];
	DriverTarget *dtar2 = &dvar->targets[1];
	Object *ob1 = (Object *)dtar->id;
	Object *ob2 = (Object *)dtar2->id;
	PointerRNA dtar_ptr, dtar2_ptr;
	uiLayout *col;
	
	/* initialize RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar,  &dtar_ptr); 
	RNA_pointer_create(id, &RNA_DriverTarget, dtar2, &dtar2_ptr); 
	
	/* Bone 1 */
	col = uiLayoutColumn(layout, true);
	uiLayoutSetRedAlert(col, (dtar->flag & DTAR_FLAG_INVALID)); /* XXX: per field... */
	uiTemplateAnyID(col, &dtar_ptr, "id", "id_type", IFACE_("Ob/Bone 1:"));
		
	if (dtar->id && GS(dtar->id->name) == ID_OB && ob1->pose) {
		PointerRNA tar_ptr;
		
		RNA_pointer_create(dtar->id, &RNA_Pose, ob1->pose, &tar_ptr);
		uiItemPointerR(col, &dtar_ptr, "bone_target", &tar_ptr, "bones", "", ICON_BONE_DATA);
	}
	
	uiLayoutSetRedAlert(col, false); /* we can clear it again now - it's only needed when creating the ID/Bone fields */
	uiItemR(col, &dtar_ptr, "transform_space", 0, NULL, ICON_NONE);
	
	col = uiLayoutColumn(layout, true);
	uiLayoutSetRedAlert(col, (dtar2->flag & DTAR_FLAG_INVALID)); /* XXX: per field... */
	uiTemplateAnyID(col, &dtar2_ptr, "id", "id_type", IFACE_("Ob/Bone 2:"));
		
	if (dtar2->id && GS(dtar2->id->name) == ID_OB && ob2->pose) {
		PointerRNA tar_ptr;
		
		RNA_pointer_create(dtar2->id, &RNA_Pose, ob2->pose, &tar_ptr);
		uiItemPointerR(col, &dtar2_ptr, "bone_target", &tar_ptr, "bones", "", ICON_BONE_DATA);
	}
		
	uiLayoutSetRedAlert(col, false); /* we can clear it again now - it's only needed when creating the ID/Bone fields */
	uiItemR(col, &dtar2_ptr, "transform_space", 0, NULL, ICON_NONE);
}

/* settings for 'transform channel' driver variable type */
static void graph_panel_driverVar__transChan(uiLayout *layout, ID *id, DriverVar *dvar)
{
	DriverTarget *dtar = &dvar->targets[0];
	Object *ob = (Object *)dtar->id;
	PointerRNA dtar_ptr;
	uiLayout *col, *sub;
	
	/* initialize RNA pointer to the target */
	RNA_pointer_create(id, &RNA_DriverTarget, dtar, &dtar_ptr); 
	
	/* properties */
	col = uiLayoutColumn(layout, true);
	uiLayoutSetRedAlert(col, (dtar->flag & DTAR_FLAG_INVALID)); /* XXX: per field... */
	uiTemplateAnyID(col, &dtar_ptr, "id", "id_type", IFACE_("Ob/Bone:"));
		
	if (dtar->id && GS(dtar->id->name) == ID_OB && ob->pose) {
		PointerRNA tar_ptr;
		
		RNA_pointer_create(dtar->id, &RNA_Pose, ob->pose, &tar_ptr);
		uiItemPointerR(col, &dtar_ptr, "bone_target", &tar_ptr, "bones", "", ICON_BONE_DATA);
	}
		
	sub = uiLayoutColumn(layout, true);
	uiItemR(sub, &dtar_ptr, "transform_type", 0, NULL, ICON_NONE);
	uiItemR(sub, &dtar_ptr, "transform_space", 0, IFACE_("Space"), ICON_NONE);
}

/* driver settings for active F-Curve (only for 'Drivers' mode) */
static void graph_panel_drivers(const bContext *C, Panel *pa)
{
	bAnimListElem *ale;
	FCurve *fcu;
	ChannelDriver *driver;
	DriverVar *dvar;
	
	PointerRNA driver_ptr;
	uiLayout *col;
	uiBlock *block;
	uiBut *but;
	
	/* Get settings from context */
	if (!graph_panel_context(C, &ale, &fcu))
		return;
	driver = fcu->driver;
	
	/* set event handler for panel */
	block = uiLayoutGetBlock(pa->layout); // xxx?
	uiBlockSetHandleFunc(block, do_graph_region_driver_buttons, NULL);
	
	/* general actions - management */
	col = uiLayoutColumn(pa->layout, false);
	block = uiLayoutGetBlock(col);
	but = uiDefIconTextBut(block, BUT, B_IPO_DEPCHANGE, ICON_FILE_REFRESH, IFACE_("Update Dependencies"),
	               0, 0, 10 * UI_UNIT_X, 22,
	               NULL, 0.0, 0.0, 0, 0,
	               TIP_("Force updates of dependencies"));
	uiButSetFunc(but, driver_update_flags_cb, fcu, NULL);

	but = uiDefIconTextBut(block, BUT, B_IPO_DEPCHANGE, ICON_ZOOMOUT, IFACE_("Remove Driver"),
	               0, 0, 10 * UI_UNIT_X, 18,
	               NULL, 0.0, 0.0, 0, 0,
	               TIP_("Remove this driver"));
	uiButSetNFunc(but, driver_remove_cb, MEM_dupallocN(ale), NULL);
		
	/* driver-level settings - type, expressions, and errors */
	RNA_pointer_create(ale->id, &RNA_Driver, driver, &driver_ptr);
	
	col = uiLayoutColumn(pa->layout, true);
	block = uiLayoutGetBlock(col);
	uiItemR(col, &driver_ptr, "type", 0, NULL, ICON_NONE);

	/* show expression box if doing scripted drivers, and/or error messages when invalid drivers exist */
	if (driver->type == DRIVER_TYPE_PYTHON) {
		bool bpy_data_expr_error = (strstr(driver->expression, "bpy.data.") != NULL);
		bool bpy_ctx_expr_error  = (strstr(driver->expression, "bpy.context.") != NULL);
		
		/* expression */
		uiItemR(col, &driver_ptr, "expression", 0, IFACE_("Expr"), ICON_NONE);
		
		/* errors? */
		if ((G.f & G_SCRIPT_AUTOEXEC) == 0) {
			uiItemL(col, IFACE_("ERROR: Python auto-execution disabled"), ICON_CANCEL);
		}
		else if (driver->flag & DRIVER_FLAG_INVALID) {
			uiItemL(col, IFACE_("ERROR: Invalid Python expression"), ICON_CANCEL);
		}
		
		/* Explicit bpy-references are evil. Warn about these to prevent errors */
		/* TODO: put these in a box? */
		if (bpy_data_expr_error || bpy_ctx_expr_error) {
			uiItemL(col, IFACE_("WARNING: Driver expression may not work correctly"), ICON_HELP);
			
			if (bpy_data_expr_error) {
				uiItemL(col, IFACE_("TIP: Use variables instead of bpy.data paths (see below)"), ICON_ERROR);
			}
			if (bpy_ctx_expr_error) {
				uiItemL(col, IFACE_("TIP: bpy.context is not safe for renderfarm usage"), ICON_ERROR);
			}
		}
	}
	else {
		/* errors? */
		if (driver->flag & DRIVER_FLAG_INVALID)
			uiItemL(col, IFACE_("ERROR: Invalid target channel(s)"), ICON_ERROR);
			
		/* Warnings about a lack of variables
		 * NOTE: The lack of variables is generally a bad thing, since it indicates
		 *       that the driver doesn't work at all. This particular scenario arises
		 *       primarily when users mistakenly try to use drivers for procedural
		 *       property animation
		 */
		if (BLI_listbase_is_empty(&driver->variables)) {
			uiItemL(col, IFACE_("ERROR: Driver is useless without any inputs"), ICON_ERROR);
			
			if (!BLI_listbase_is_empty(&fcu->modifiers)) {
				uiItemL(col, IFACE_("TIP: Use F-Curves for procedural animation instead"), ICON_INFO);
				uiItemL(col, IFACE_("F-Modifiers can generate curves for those too"), ICON_INFO);
			}
		}
	}
		
	col = uiLayoutColumn(pa->layout, true);
	/* debug setting */
	uiItemR(col, &driver_ptr, "show_debug_info", 0, NULL, ICON_NONE);
		
	/* value of driver */
	if (driver->flag & DRIVER_FLAG_SHOWDEBUG) {
		uiLayout *row = uiLayoutRow(col, true);
		char valBuf[32];
			
		uiItemL(row, IFACE_("Driver Value:"), ICON_NONE);
			
		BLI_snprintf(valBuf, sizeof(valBuf), "%.3f", driver->curval);
		uiItemL(row, valBuf, ICON_NONE);
	}
	
	/* add driver variables */
	col = uiLayoutColumn(pa->layout, false);
	block = uiLayoutGetBlock(col);
	but = uiDefIconTextBut(block, BUT, B_IPO_DEPCHANGE, ICON_ZOOMIN, IFACE_("Add Variable"),
	               0, 0, 10 * UI_UNIT_X, UI_UNIT_Y,
	               NULL, 0.0, 0.0, 0, 0,
	               TIP_("Driver variables ensure that all dependencies will be accounted for and that drivers will update correctly"));
	uiButSetFunc(but, driver_add_var_cb, driver, NULL);
	
	/* loop over targets, drawing them */
	for (dvar = driver->variables.first; dvar; dvar = dvar->next) {
		PointerRNA dvar_ptr;
		uiLayout *box, *row;
		
		/* sub-layout column for this variable's settings */
		col = uiLayoutColumn(pa->layout, true);
		
		/* header panel */
		box = uiLayoutBox(col);
		/* first row context info for driver */
		RNA_pointer_create(ale->id, &RNA_DriverVariable, dvar, &dvar_ptr);
		
		row = uiLayoutRow(box, false);
		block = uiLayoutGetBlock(row);
		/* variable name */
		uiItemR(row, &dvar_ptr, "name", 0, "", ICON_NONE);
		
		/* remove button */
		uiBlockSetEmboss(block, UI_EMBOSSN);
		but = uiDefIconBut(block, BUT, B_IPO_DEPCHANGE, ICON_X, 290, 0, UI_UNIT_X, UI_UNIT_Y,
		                   NULL, 0.0, 0.0, 0.0, 0.0, IFACE_("Delete target variable"));
		uiButSetFunc(but, driver_delete_var_cb, driver, dvar);
		uiBlockSetEmboss(block, UI_EMBOSS);
		
		/* variable type */
		row = uiLayoutRow(box, false);
		uiItemR(row, &dvar_ptr, "type", 0, "", ICON_NONE);
				
		/* variable type settings */
		box = uiLayoutBox(col);
		/* controls to draw depends on the type of variable */
		switch (dvar->type) {
			case DVAR_TYPE_SINGLE_PROP:     /* single property */
				graph_panel_driverVar__singleProp(box, ale->id, dvar);
				break;
			case DVAR_TYPE_ROT_DIFF:     /* rotational difference */
				graph_panel_driverVar__rotDiff(box, ale->id, dvar);
				break;
			case DVAR_TYPE_LOC_DIFF:     /* location difference */
				graph_panel_driverVar__locDiff(box, ale->id, dvar);
				break;
			case DVAR_TYPE_TRANSFORM_CHAN:     /* transform channel */
				graph_panel_driverVar__transChan(box, ale->id, dvar);
				break;
		}
		
		/* value of variable */
		if (driver->flag & DRIVER_FLAG_SHOWDEBUG) {
			char valBuf[32];
			
			box = uiLayoutBox(col);
			row = uiLayoutRow(box, true);
			uiItemL(row, IFACE_("Value:"), ICON_NONE);
			
			if ((dvar->type == DVAR_TYPE_ROT_DIFF) ||
			    (dvar->type == DVAR_TYPE_TRANSFORM_CHAN &&
			     dvar->targets[0].transChan >= DTAR_TRANSCHAN_ROTX &&
			     dvar->targets[0].transChan < DTAR_TRANSCHAN_SCALEX))
			{
				BLI_snprintf(valBuf, sizeof(valBuf), "%.3f (%4.1f°)", dvar->curval, RAD2DEGF(dvar->curval));
			}
			else {
				BLI_snprintf(valBuf, sizeof(valBuf), "%.3f", dvar->curval);
			}
			
			uiItemL(row, valBuf, ICON_NONE);
		}
	}
	
	/* cleanup */
	MEM_freeN(ale);
}

/* ******************* F-Modifiers ******************************** */
/* All the drawing code is in editors/animation/fmodifier_ui.c */

#define B_FMODIFIER_REDRAW      20

static void do_graph_region_modifier_buttons(bContext *C, void *UNUSED(arg), int event)
{
	switch (event) {
		case B_FMODIFIER_REDRAW: // XXX this should send depsgraph updates too
			WM_event_add_notifier(C, NC_ANIMATION, NULL); // XXX need a notifier specially for F-Modifiers
			break;
	}
}

static void graph_panel_modifiers(const bContext *C, Panel *pa)	
{
	bAnimListElem *ale;
	FCurve *fcu;
	FModifier *fcm;
	uiLayout *col, *row;
	uiBlock *block;
	
	if (!graph_panel_context(C, &ale, &fcu))
		return;
	
	block = uiLayoutGetBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_graph_region_modifier_buttons, NULL);
	
	/* 'add modifier' button at top of panel */
	{
		row = uiLayoutRow(pa->layout, false);
		block = uiLayoutGetBlock(row);
		
		/* this is an operator button which calls a 'add modifier' operator... 
		 * a menu might be nicer but would be tricky as we need some custom filtering
		 */
		uiDefButO(block, BUT, "GRAPH_OT_fmodifier_add", WM_OP_INVOKE_REGION_WIN, IFACE_("Add Modifier"),
		          0.5 * UI_UNIT_X, 0, 7.5 * UI_UNIT_X, UI_UNIT_Y, TIP_("Adds a new F-Curve Modifier for the active F-Curve"));
		
		/* copy/paste (as sub-row)*/
		row = uiLayoutRow(row, true);
		uiItemO(row, "", ICON_COPYDOWN, "GRAPH_OT_fmodifier_copy");
		uiItemO(row, "", ICON_PASTEDOWN, "GRAPH_OT_fmodifier_paste");
	}
	
	/* draw each modifier */
	for (fcm = fcu->modifiers.first; fcm; fcm = fcm->next) {
		col = uiLayoutColumn(pa->layout, true);
		
		ANIM_uiTemplate_fmodifier_draw(col, ale->id, &fcu->modifiers, fcm);
	}

	MEM_freeN(ale);
}

/* ******************* general ******************************** */

void graph_buttons_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype graph panel view");
	strcpy(pt->idname, "GRAPH_PT_view");
	strcpy(pt->label, N_("View Properties"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = graph_panel_view;
	pt->flag |= PNL_DEFAULT_CLOSED;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype graph panel properties");
	strcpy(pt->idname, "GRAPH_PT_properties");
	strcpy(pt->label, N_("Active F-Curve"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = graph_panel_properties;
	pt->poll = graph_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype graph panel properties");
	strcpy(pt->idname, "GRAPH_PT_key_properties");
	strcpy(pt->label, N_("Active Keyframe"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = graph_panel_key_properties;
	pt->poll = graph_panel_poll;
	BLI_addtail(&art->paneltypes, pt);


	pt = MEM_callocN(sizeof(PanelType), "spacetype graph panel drivers");
	strcpy(pt->idname, "GRAPH_PT_drivers");
	strcpy(pt->label, N_("Drivers"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = graph_panel_drivers;
	pt->poll = graph_panel_drivers_poll;
	BLI_addtail(&art->paneltypes, pt);

	pt = MEM_callocN(sizeof(PanelType), "spacetype graph panel modifiers");
	strcpy(pt->idname, "GRAPH_PT_modifiers");
	strcpy(pt->label, N_("Modifiers"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = graph_panel_modifiers;
	pt->poll = graph_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int graph_properties_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = graph_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void GRAPH_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->idname = "GRAPH_OT_properties";
	ot->description = "Toggle display properties panel";
	
	ot->exec = graph_properties_toggle_exec;
	ot->poll = ED_operator_graphedit_active;

	/* flags */
	ot->flag = 0;
}
