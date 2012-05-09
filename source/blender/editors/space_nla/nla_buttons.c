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

/** \file blender/editors/space_nla/nla_buttons.c
 *  \ingroup spnla
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_anim_types.h"

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "BLF_translation.h"

#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_screen.h"


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_anim_api.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "nla_intern.h" // own include


/* ******************* nla editor space & buttons ************** */

#define B_NOP       1
#define B_REDR      2

/* -------------- */

static void do_nla_region_buttons(bContext *C, void *UNUSED(arg), int event)
{
	//Scene *scene= CTX_data_scene(C);
	
	switch (event) {

	}
	
	/* default for now */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_SCENE | ND_TRANSFORM, NULL);
}

static int nla_panel_context(const bContext *C, PointerRNA *adt_ptr, PointerRNA *nlt_ptr, PointerRNA *strip_ptr)
{
	bAnimContext ac;
	bAnimListElem *ale = NULL;
	ListBase anim_data = {NULL, NULL};
	short found = 0;
	int filter;
	
	/* for now, only draw if we could init the anim-context info (necessary for all animation-related tools) 
	 * to work correctly is able to be correctly retrieved. There's no point showing empty panels?
	 */
	if (ANIM_animdata_get_context(C, &ac) == 0) 
		return 0;
	
	/* extract list of active channel(s), of which we should only take the first one 
	 *	- we need the channels flag to get the active AnimData block when there are no NLA Tracks
	 */
	// XXX: double-check active!
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_ACTIVE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		switch (ale->type) {
			case ANIMTYPE_NLATRACK: /* NLA Track - The primary data type which should get caught */
			{
				NlaTrack *nlt = (NlaTrack *)ale->data;
				AnimData *adt = ale->adt;
				
				/* found it, now set the pointers */
				if (adt_ptr) {
					/* AnimData pointer */
					RNA_pointer_create(ale->id, &RNA_AnimData, adt, adt_ptr);
				}
				if (nlt_ptr) {
					/* NLA-Track pointer */
					RNA_pointer_create(ale->id, &RNA_NlaTrack, nlt, nlt_ptr);
				}
				if (strip_ptr) {
					/* NLA-Strip pointer */
					NlaStrip *strip = BKE_nlastrip_find_active(nlt);
					RNA_pointer_create(ale->id, &RNA_NlaStrip, strip, strip_ptr);
				}
				
				found = 1;
			}
			break;
				
			case ANIMTYPE_SCENE:    /* Top-Level Widgets doubling up as datablocks */
			case ANIMTYPE_OBJECT:
			case ANIMTYPE_FILLACTD: /* Action Expander */
			case ANIMTYPE_DSMAT:    /* Datablock AnimData Expanders */
			case ANIMTYPE_DSLAM:
			case ANIMTYPE_DSCAM:
			case ANIMTYPE_DSCUR:
			case ANIMTYPE_DSSKEY:
			case ANIMTYPE_DSWOR:
			case ANIMTYPE_DSNTREE:
			case ANIMTYPE_DSPART:
			case ANIMTYPE_DSMBALL:
			case ANIMTYPE_DSARM:
			case ANIMTYPE_DSSPK:
			{
				/* for these channels, we only do AnimData */
				if (ale->id && ale->adt) {
					if (adt_ptr) {
						/* AnimData pointer */
						RNA_pointer_create(ale->id, &RNA_AnimData, ale->adt, adt_ptr);
						
						/* set found status to -1, since setting to 1 would break the loop 
						 * and potentially skip an active NLA-Track in some cases...
						 */
						found = -1;
					}
				}
			}	
			break;
		}
		
		if (found > 0)
			break;
	}
	
	/* free temp data */
	BLI_freelistN(&anim_data);
	
	return found;
}

#if 0
static int nla_panel_poll(const bContext *C, PanelType *pt)
{
	return nla_panel_context(C, NULL, NULL);
}
#endif

static int nla_animdata_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	PointerRNA ptr;
	return (nla_panel_context(C, &ptr, NULL, NULL) && (ptr.data != NULL));
}

static int nla_track_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	PointerRNA ptr;
	return (nla_panel_context(C, NULL, &ptr, NULL) && (ptr.data != NULL));
}

static int nla_strip_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	PointerRNA ptr;
	return (nla_panel_context(C, NULL, NULL, &ptr) && (ptr.data != NULL));
}

static int nla_strip_actclip_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	PointerRNA ptr;
	NlaStrip *strip;
	
	if (!nla_panel_context(C, NULL, NULL, &ptr))
		return 0;
	if (ptr.data == NULL)
		return 0;
	
	strip = ptr.data;
	return (strip->type == NLASTRIP_TYPE_CLIP);
}

static int nla_strip_eval_panel_poll(const bContext *C, PanelType *UNUSED(pt))
{
	PointerRNA ptr;
	NlaStrip *strip;
	
	if (!nla_panel_context(C, NULL, NULL, &ptr))
		return 0;
	if (ptr.data == NULL)
		return 0;
	
	strip = ptr.data;
	
	if (strip->type == NLASTRIP_TYPE_SOUND)
		return 0;
		
	return 1;
}

/* -------------- */

/* active AnimData */
static void nla_panel_animdata(const bContext *C, Panel *pa)
{
	PointerRNA adt_ptr;
	/* AnimData *adt; */
	uiLayout *layout = pa->layout;
	uiLayout *row;
	uiBlock *block;
	
	/* check context and also validity of pointer */
	if (!nla_panel_context(C, &adt_ptr, NULL, NULL))
		return;

	/* adt= adt_ptr.data; */
	
	block = uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
	
	/* Active Action Properties ------------------------------------- */
	/* action */
	row = uiLayoutRow(layout, 1);
	uiTemplateID(row, (bContext *)C, &adt_ptr, "action", "ACTION_OT_new", NULL, NULL /*"ACTION_OT_unlink"*/);     // XXX: need to make these operators
	
	/* extrapolation */
	row = uiLayoutRow(layout, 1);
	uiItemR(row, &adt_ptr, "action_extrapolation", 0, NULL, ICON_NONE);
	
	/* blending */
	row = uiLayoutRow(layout, 1);
	uiItemR(row, &adt_ptr, "action_blend_type", 0, NULL, ICON_NONE);
		
	/* influence */
	row = uiLayoutRow(layout, 1);
	uiItemR(row, &adt_ptr, "action_influence", 0, NULL, ICON_NONE);
}

/* active NLA-Track */
static void nla_panel_track(const bContext *C, Panel *pa)
{
	PointerRNA nlt_ptr;
	uiLayout *layout = pa->layout;
	uiLayout *row;
	uiBlock *block;
	
	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, &nlt_ptr, NULL))
		return;
	
	block = uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
	
	/* Info - Active NLA-Context:Track ----------------------  */
	row = uiLayoutRow(layout, 1);
	uiItemR(row, &nlt_ptr, "name", 0, NULL, ICON_NLA);
}

/* generic settings for active NLA-Strip */
static void nla_panel_properties(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout = pa->layout;
	uiLayout *column, *row, *sub;
	uiBlock *block;
	short showEvalProps = 1;
	
	if (!nla_panel_context(C, NULL, NULL, &strip_ptr))
		return;
	
	block = uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
	
	/* Strip Properties ------------------------------------- */
	/* strip type */
	row = uiLayoutColumn(layout, 1);
	uiItemR(row, &strip_ptr, "name", 0, NULL, ICON_NLA);     // XXX icon?
	uiItemR(row, &strip_ptr, "type", 0, NULL, ICON_NONE);
	
	/* strip extents */
	column = uiLayoutColumn(layout, 1);
	uiItemL(column, "Strip Extents:", ICON_NONE);
	uiItemR(column, &strip_ptr, "frame_start", 0, NULL, ICON_NONE);
	uiItemR(column, &strip_ptr, "frame_end", 0, NULL, ICON_NONE);
	
	/* Evaluation-Related Strip Properties ------------------ */
	
	/* sound properties strips don't have these settings */
	if (RNA_enum_get(&strip_ptr, "type") == NLASTRIP_TYPE_SOUND)
		showEvalProps = 0;
	
	/* only show if allowed to... */
	if (showEvalProps) {
		/* extrapolation */
		row = uiLayoutRow(layout, 1);
		uiItemR(row, &strip_ptr, "extrapolation", 0, NULL, ICON_NONE);
		
		/* blending */
		row = uiLayoutRow(layout, 1);
		uiItemR(row, &strip_ptr, "blend_type", 0, NULL, ICON_NONE);
			
		/* blend in/out + autoblending
		 *	- blend in/out can only be set when autoblending is off
		 */
		column = uiLayoutColumn(layout, 1);
		uiLayoutSetActive(column, RNA_boolean_get(&strip_ptr, "use_animated_influence") == 0);
		uiItemR(column, &strip_ptr, "use_auto_blend", 0, NULL, ICON_NONE);     // XXX as toggle?

		sub = uiLayoutColumn(column, 1);
		uiLayoutSetActive(sub, RNA_boolean_get(&strip_ptr, "use_auto_blend") == 0);
		uiItemR(sub, &strip_ptr, "blend_in", 0, NULL, ICON_NONE);
		uiItemR(sub, &strip_ptr, "blend_out", 0, NULL, ICON_NONE);
			
		/* settings */
		column = uiLayoutColumn(layout, 1);
		uiLayoutSetActive(column, !(RNA_boolean_get(&strip_ptr, "use_animated_influence") || RNA_boolean_get(&strip_ptr, "use_animated_time")));
		uiItemL(column, "Playback Settings:", ICON_NONE);
		uiItemR(column, &strip_ptr, "mute", 0, NULL, ICON_NONE);
		uiItemR(column, &strip_ptr, "use_reverse", 0, NULL, ICON_NONE);
	}
}


/* action-clip only settings for active NLA-Strip */
static void nla_panel_actclip(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout = pa->layout;
	uiLayout *column, *row;
	uiBlock *block;

	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, NULL, &strip_ptr))
		return;
	
	block = uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
		
	/* Strip Properties ------------------------------------- */
	/* action pointer */
	row = uiLayoutRow(layout, 1);
	uiItemR(row, &strip_ptr, "action", 0, NULL, ICON_ACTION);
		
	/* action extents */
	// XXX custom names were used here (to avoid the prefixes)... probably not necessary in future?
	column = uiLayoutColumn(layout, 1);
	uiItemL(column, "Action Extents:", ICON_NONE);
	uiItemR(column, &strip_ptr, "action_frame_start", 0, "Start Frame", ICON_NONE);
	uiItemR(column, &strip_ptr, "action_frame_end", 0, "End Frame", ICON_NONE);
	uiItemO(column, NULL, ICON_NONE, "NLA_OT_action_sync_length");
		
	/* action usage */
	column = uiLayoutColumn(layout, 1);
	uiLayoutSetActive(column, RNA_boolean_get(&strip_ptr, "use_animated_time") == 0);
	uiItemL(column, "Playback Settings:", ICON_NONE);
	uiItemR(column, &strip_ptr, "scale", 0, NULL, ICON_NONE);
	uiItemR(column, &strip_ptr, "repeat", 0, NULL, ICON_NONE);
}

/* evaluation settings for active NLA-Strip */
static void nla_panel_evaluation(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	uiLayout *layout = pa->layout;
	uiLayout *col, *sub;
	uiBlock *block;

	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, NULL, &strip_ptr))
		return;
		
	block = uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
		
	col = uiLayoutColumn(layout, 1);
	uiItemR(col, &strip_ptr, "use_animated_influence", 0, NULL, ICON_NONE);
	
	sub = uiLayoutColumn(col, 1);
	uiLayoutSetEnabled(sub, RNA_boolean_get(&strip_ptr, "use_animated_influence"));	
	uiItemR(sub, &strip_ptr, "influence", 0, NULL, ICON_NONE);

	col = uiLayoutColumn(layout, 1);
	sub = uiLayoutRow(col, 0);
	uiItemR(sub, &strip_ptr, "use_animated_time", 0, NULL, ICON_NONE);
	uiItemR(sub, &strip_ptr, "use_animated_time_cyclic", 0, NULL, ICON_NONE);

	sub = uiLayoutRow(col, 0);
	uiLayoutSetEnabled(sub, RNA_boolean_get(&strip_ptr, "use_animated_time"));
	uiItemR(sub, &strip_ptr, "strip_time", 0, NULL, ICON_NONE);
}

/* F-Modifiers for active NLA-Strip */
static void nla_panel_modifiers(const bContext *C, Panel *pa)
{
	PointerRNA strip_ptr;
	NlaStrip *strip;
	FModifier *fcm;
	uiLayout *col, *row;
	uiBlock *block;

	/* check context and also validity of pointer */
	if (!nla_panel_context(C, NULL, NULL, &strip_ptr))
		return;
	strip = strip_ptr.data;
		
	block = uiLayoutGetBlock(pa->layout);
	uiBlockSetHandleFunc(block, do_nla_region_buttons, NULL);
	
	/* 'add modifier' button at top of panel */
	{
		row = uiLayoutRow(pa->layout, 0);
		block = uiLayoutGetBlock(row);
		
		// XXX for now, this will be a operator button which calls a temporary 'add modifier' operator
		// FIXME: we need to set the only-active property so that this will only add modifiers for the active strip (not all selected)
		uiDefButO(block, BUT, "NLA_OT_fmodifier_add", WM_OP_INVOKE_REGION_WIN, IFACE_("Add Modifier"), 10, 0, 150, 20,
		          TIP_("Adds a new F-Modifier for the active NLA Strip"));
		
		/* copy/paste (as sub-row)*/
		row = uiLayoutRow(row, 1);
		uiItemO(row, "", ICON_COPYDOWN, "NLA_OT_fmodifier_copy");
		uiItemO(row, "", ICON_PASTEDOWN, "NLA_OT_fmodifier_paste");
	}
	
	/* draw each modifier */
	for (fcm = strip->modifiers.first; fcm; fcm = fcm->next) {
		col = uiLayoutColumn(pa->layout, 1);
		
		ANIM_uiTemplate_fmodifier_draw(col, strip_ptr.id.data, &strip->modifiers, fcm);
	}
}

/* ******************* general ******************************** */


void nla_buttons_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel animdata");
	strcpy(pt->idname, "NLA_PT_animdata");
	strcpy(pt->label, "Animation Data");
	pt->draw = nla_panel_animdata;
	pt->poll = nla_animdata_panel_poll;
	pt->flag = PNL_DEFAULT_CLOSED;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel track");
	strcpy(pt->idname, "NLA_PT_track");
	strcpy(pt->label, "Active Track");
	pt->draw = nla_panel_track;
	pt->poll = nla_track_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
	strcpy(pt->idname, "NLA_PT_properties");
	strcpy(pt->label, "Active Strip");
	pt->draw = nla_panel_properties;
	pt->poll = nla_strip_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel properties");
	strcpy(pt->idname, "NLA_PT_actionclip");
	strcpy(pt->label, "Action Clip");
	pt->draw = nla_panel_actclip;
	pt->poll = nla_strip_actclip_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel evaluation");
	strcpy(pt->idname, "NLA_PT_evaluation");
	strcpy(pt->label, "Evaluation");
	pt->draw = nla_panel_evaluation;
	pt->poll = nla_strip_eval_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype nla panel modifiers");
	strcpy(pt->idname, "NLA_PT_modifiers");
	strcpy(pt->label, "Modifiers");
	pt->draw = nla_panel_modifiers;
	pt->poll = nla_strip_eval_panel_poll;
	BLI_addtail(&art->paneltypes, pt);
}

static int nla_properties(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = nla_has_buttons_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void NLA_OT_properties(wmOperatorType *ot)
{
	ot->name = "Properties";
	ot->idname = "NLA_OT_properties";
	ot->description = "Toggle display properties panel";
	
	ot->exec = nla_properties;
	ot->poll = ED_operator_nla_active;

	/* flags */
	ot->flag = 0;
}
