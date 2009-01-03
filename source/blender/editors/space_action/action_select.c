/**
 * $Id: editaction.c 17746 2008-12-08 11:19:44Z aligorith $
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
 * The Original Code is Copyright (C) 2008 Blender Foundation
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "action_intern.h"

/* ************************************************************************** */
/* GENERAL STUFF */

/* used only by mouse_action. It is used to find the location of the nearest 
 * keyframe to where the mouse clicked, 
 */
// XXX port this to new listview code...
static void *get_nearest_action_key (bAnimContext *ac, int mval[2], float *selx, short *sel, short *ret_type, bActionChannel **par)
{
	ListBase anim_data = {NULL, NULL};
	ListBase anim_keys = {NULL, NULL};
	bAnimListElem *ale;
	ActKeyColumn *ak;
	View2D *v2d= &ac->ar->v2d;
	int filter;
	
	rctf rectf;
	void *data = NULL;
	float xmin, xmax, x, y;
	int clickmin, clickmax;
	short found = 0;
	
	/* action-channel */
	*par= NULL;
	
	UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, &y);
    clickmin = (int) ((-y) / (ACHANNEL_STEP));
	clickmax = clickmin;
	
	/* x-range to check is +/- 7 on either side of mouse click (size of keyframe icon) */
	UI_view2d_region_to_view(v2d, mval[0]-7, mval[1], &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, mval[0]+7, mval[1], &rectf.xmax, &rectf.ymax);
	
	if (clickmax < 0) {
		*ret_type= ANIMTYPE_NONE;
		return NULL;
	}
	
	/* filter data */
	filter= (ANIMFILTER_FORDRAWING | ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		if (clickmax < 0) 
			break;
		if (clickmin <= 0) {
			/* found match - must return here... */
			Object *nob= ANIM_nla_mapping_get(ac, ale);
			ActKeysInc *aki= init_aki_data(ac, ale);
			
			/* apply NLA-scaling correction? */
			if (nob) {
				xmin= get_action_frame(nob, rectf.xmin);
				xmax= get_action_frame(nob, rectf.xmax);
			}
			else {
				xmin= rectf.xmin;
				xmax= rectf.xmax;
			}
			
			/* make list of keyframes */
			if (ale->key_data) {
				switch (ale->datatype) {
					case ALE_OB:
					{
						Object *ob= (Object *)ale->key_data;
						ob_to_keylist(ob, &anim_keys, NULL, aki);
					}
						break;
					case ALE_ACT:
					{
						bAction *act= (bAction *)ale->key_data;
						action_to_keylist(act, &anim_keys, NULL, aki);
					}
						break;
					case ALE_IPO:
					{
						Ipo *ipo= (Ipo *)ale->key_data;
						ipo_to_keylist(ipo, &anim_keys, NULL, aki);
					}
						break;
					case ALE_ICU:
					{
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						icu_to_keylist(icu, &anim_keys, NULL, aki);
					}
						break;
				}
			}
			else if (ale->type == ANIMTYPE_GROUP) {
				bActionGroup *agrp= (bActionGroup *)ale->data;
				agroup_to_keylist(agrp, &anim_keys, NULL, aki);
			}
			else if (ale->type == ANIMTYPE_GPDATABLOCK) {
				/* cleanup */
				BLI_freelistN(&anim_data);
				
				/* this channel currently doens't have any keyframes... must ignore! */
				*ret_type= ANIMTYPE_NONE;
				return NULL;
			}
			else if (ale->type == ANIMTYPE_GPLAYER) {
				bGPDlayer *gpl= (bGPDlayer *)ale->data;
				gpl_to_keylist(gpl, &anim_keys, NULL, aki);
			}
			
			/* loop through keyframes, finding one that was clicked on */
			for (ak= anim_keys.first; ak; ak= ak->next) {
				if (IN_RANGE(ak->cfra, xmin, xmax)) {
					*selx= ak->cfra;
					found= 1;
					break;
				}
			}
			/* no matching keyframe found - set to mean frame value so it doesn't actually select anything */
			if (found == 0)
				*selx= ((xmax+xmin) / 2);
			
			/* figure out what to return */
			if (ac->datatype == ANIMCONT_ACTION) {
				*par= ale->owner; /* assume that this is an action channel */
				*ret_type= ale->type;
				data = ale->data;
			}
			else if (ac->datatype == ANIMCONT_SHAPEKEY) {
				data = ale->key_data;
				*ret_type= ANIMTYPE_ICU;
			}
			else if (ac->datatype == ANIMCONT_DOPESHEET) {
				data = ale->data;
				*ret_type= ale->type;
			}
			else if (ac->datatype == ANIMCONT_GPENCIL) {
				data = ale->data;
				*ret_type= ANIMTYPE_GPLAYER;
			}
			
			/* cleanup tempolary lists */
			BLI_freelistN(&anim_keys);
			anim_keys.first = anim_keys.last = NULL;
			
			BLI_freelistN(&anim_data);
			
			return data;
		}
		--clickmin;
		--clickmax;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
	
	*ret_type= ANIMTYPE_NONE;
	return NULL;
}

/* ************************************************************************** */
/* KEYFRAMES STUFF */

/* ******************** Deselect All Operator ***************************** */
/* This operator works in one of three ways:
 *	1) (de)select all (AKEY) - test if select all or deselect all
 *	2) invert all (CTRL-IKEY) - invert selection of all keyframes
 *	3) (de)select all - no testing is done; only for use internal tools as normal function...
 */

/* Deselects keyframes in the action editor
 *	- This is called by the deselect all operator, as well as other ones!
 *
 * 	- test: check if select or deselect all
 *	- sel: how to select keyframes 
 *		0 = deselect
 *		1 = select
 *		2 = invert
 */
static void deselect_action_keys (bAnimContext *ac, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* determine type-based settings */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
	
	/* filter data */
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			if (ale->type == ANIMTYPE_GPLAYER) {
				//if (is_gplayer_frame_selected(ale->data)) {
				//	sel= 0;
				//	break;
				//}
			}
			else {
				if (is_ipo_key_selected(ale->key_data)) {
					sel= 0;
					break;
				}
			}
		}
	}
		
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		//if (ale->type == ACTTYPE_GPLAYER)
		//	set_gplayer_frame_selection(ale->data, sel);
		//else
			set_ipo_key_selection(ale->key_data, sel);
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behaviour - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		deselect_action_keys(&ac, 0, 2);
	else
		deselect_action_keys(&ac, 1, 1);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_keyframes_deselectall (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "ACT_OT_keyframes_deselectall";
	
	/* api callbacks */
	ot->exec= actkeys_deselectall_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	RNA_def_property(ot->srna, "invert", PROP_BOOLEAN, PROP_NONE);
}

/* ******************** Border Select Operator **************************** */
/* This operator currently works in one of three ways:
 *	-> BKEY 	- 1) all keyframes within region are selected (ACTKEYS_BORDERSEL_ALLKEYS)
 *	-> ALT-BKEY - depending on which axis of the region was larger...
 *		-> 2) x-axis, so select all frames within frame range (ACTKEYS_BORDERSEL_FRAMERANGE)
 *		-> 3) y-axis, so select all frames within channels that region included (ACTKEYS_BORDERSEL_CHANNELS)
 */

/* defines for borderselect mode */
enum {
	ACTKEYS_BORDERSEL_ALLKEYS	= 0,
	ACTKEYS_BORDERSEL_FRAMERANGE,
	ACTKEYS_BORDERSEL_CHANNELS,
} eActKeys_BorderSelect_Mode;


static void borderselect_action (bAnimContext *ac, rcti rect, short mode, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc ok_cb, select_cb;
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	float ymin=0, ymax=(float)(-ACHANNEL_HEIGHT);
	
	/* convert mouse coordinates to frame ranges and channel coordinates corrected for view pan/zoom */
	UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin+2, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax-2, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CHANNELS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing/validation funcs  */
	select_cb= ANIM_editkeyframes_select(selectmode);
	
	if (ELEM(mode, ACTKEYS_BORDERSEL_FRAMERANGE, ACTKEYS_BORDERSEL_ALLKEYS))
		ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	else
		ok_cb= NULL;
		
	/* init editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		/* get new vertical minimum extent of channel */
		ymin= ymax - ACHANNEL_STEP;
		
		/* set horizontal range (if applicable) */
		if (ELEM(mode, ACTKEYS_BORDERSEL_FRAMERANGE, ACTKEYS_BORDERSEL_ALLKEYS)) {
			/* if channel is mapped in NLA, apply correction */
			if (nob) {
				bed.f1= get_action_frame(nob, rectf.xmin);
				bed.f2= get_action_frame(nob, rectf.xmax);
			}
			else {
				bed.f1= rectf.xmin;
				bed.f2= rectf.xmax;
			}
		}
		
		/* perform vertical suitability check (if applicable) */
		if ( (mode == ACTKEYS_BORDERSEL_FRAMERANGE) || 
			!((ymax < rectf.ymin) || (ymin > rectf.ymax)) )
		{
			/* loop over data selecting */
			if (ale->key_data) {
				if (ale->datatype == ALE_IPO)
					ANIM_ipo_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
				else if (ale->datatype == ALE_ICU)
					ANIM_icu_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
			}
			else if (ale->type == ANIMTYPE_GROUP) {
				bActionGroup *agrp= ale->data;
				bActionChannel *achan;
				bConstraintChannel *conchan;
				
				for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
					ANIM_ipo_keys_bezier_loop(&bed, achan->ipo, ok_cb, select_cb, NULL);
					
					for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
						ANIM_ipo_keys_bezier_loop(&bed, conchan->ipo, ok_cb, select_cb, NULL);
				}
			}
			//else if (ale->type == ANIMTYPE_GPLAYER) {
			//	borderselect_gplayer_frames(ale->data, rectf.xmin, rectf.xmax, selectmode);
			//}
		}
		
		/* set minimum extent to be the maximum of the next channel */
		ymax=ymin;
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short mode=0, selectmode=0;
	int event;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
		
	event= RNA_int_get(op->ptr, "event_type");
	if (event == LEFTMOUSE) // FIXME... hardcoded
		selectmode = SELECT_ADD;
	else
		selectmode = SELECT_SUBTRACT;
	
	/* selection 'mode' depends on whether borderselect region only matters on one axis */
	if (RNA_boolean_get(op->ptr, "axis_range")) {
		/* mode depends on which axis of the range is larger to determine which axis to use 
		 *	- checking this in region-space is fine, as it's fundamentally still going to be a different rect size
		 *	- the frame-range select option is favoured over the channel one (x over y), as frame-range one is often
		 *	  used for tweaking timing when "blocking", while channels is not that useful...
		 */
		if ((rect.xmax - rect.xmin) >= (rect.ymax - rect.ymin))
			mode= ACTKEYS_BORDERSEL_FRAMERANGE;
		else
			mode= ACTKEYS_BORDERSEL_CHANNELS;
	}
	else 
		mode= ACTKEYS_BORDERSEL_ALLKEYS;
	
	/* apply borderselect action */
	borderselect_action(&ac, rect, mode, selectmode);
	
	return OPERATOR_FINISHED;
} 

void ACT_OT_keyframes_borderselect(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "ACT_OT_keyframes_borderselect";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= actkeys_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* rna */
	RNA_def_property(ot->srna, "event_type", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "xmin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "xmax", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymin", PROP_INT, PROP_NONE);
	RNA_def_property(ot->srna, "ymax", PROP_INT, PROP_NONE);
	
	RNA_def_property(ot->srna, "axis_range", PROP_BOOLEAN, PROP_NONE);
}

/* ******************** Column Select Operator **************************** */
/* This operator works in one of four ways:
 *	- 1) select all keyframes in the same frame as a selected one  (KKEY)
 *	- 2) select all keyframes in the same frame as the current frame marker (CTRL-KKEY)
 *	- 3) select all keyframes in the same frame as a selected markers (SHIFT-KKEY)
 *	- 4) select all keyframes that occur between selected markers (ALT-KKEY)
 */

/* defines for column-select mode */
EnumPropertyItem prop_column_select_types[] = {
	{ACTKEYS_COLUMNSEL_KEYS, "KEYS", "On Selected Keyframes", ""},
	{ACTKEYS_COLUMNSEL_CFRA, "CFRA", "On Current Frame", ""},
	{ACTKEYS_COLUMNSEL_MARKERS_COLUMN, "MARKERS_COLUMN", "On Selected Markers", ""},
	{ACTKEYS_COLUMNSEL_MARKERS_BETWEEN, "MARKERS_BETWEEN", "Between Min/Max Selected Markers", ""},
	{0, NULL, NULL, NULL}
};

/* ------------------- */ 

/* Selects all visible keyframes between the specified markers */
static void markers_selectkeys_between (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditFunc select_cb;
	BeztEditData bed;
	float min, max;
	
	/* get extreme markers */
	//get_minmax_markers(1, &min, &max); // FIXME... add back markers api!
	min= (float)ac->scene->r.sfra; // xxx temp code
	max= (float)ac->scene->r.efra; // xxx temp code
	
	if (min==max) return;
	min -= 0.5f;
	max += 0.5f;
	
	/* get editing funcs + data */
	select_cb= ANIM_editkeyframes_select(SELECT_ADD);
	memset(&bed, 0, sizeof(BeztEditData));
	bed.f1= min; 
	bed.f2= max;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* select keys in-between */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		if (nob) {	
			ANIM_nla_mapping_apply_ipo(nob, ale->key_data, 0, 1);
			ANIM_ipo_keys_bezier_loop(&bed, ale->key_data, NULL, select_cb, NULL);
			ANIM_nla_mapping_apply_ipo(nob, ale->key_data, 1, 1);
		}
		else {
			ANIM_ipo_keys_bezier_loop(&bed, ale->key_data, NULL, select_cb, NULL);
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}


/* helper callback for columnselect_action_keys() -> populate list CfraElems with frame numbers from selected beztriples */
// TODO: if some other code somewhere needs this, it'll be time to port this over to keyframes_edit.c!!!
static short bezt_to_cfraelem(BeztEditData *bed, BezTriple *bezt)
{
	/* only if selected */
	if (bezt->f2 & SELECT) {
		CfraElem *ce= MEM_callocN(sizeof(CfraElem), "cfraElem");
		BLI_addtail(&bed->list, ce);
		
		ce->cfra= bezt->vec[1][0];
	}
	
	return 0;
}

/* Selects all visible keyframes in the same frames as the specified elements */
static void columnselect_action_keys (bAnimContext *ac, short mode)
{
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene= ac->scene;
	CfraElem *ce;
	BeztEditFunc select_cb, ok_cb;
	BeztEditData bed;
	
	/* initialise keyframe editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* build list of columns */
	switch (mode) {
		case ACTKEYS_COLUMNSEL_KEYS: /* list of selected keys */
			if (ac->datatype == ANIMCONT_GPENCIL) {
				filter= (ANIMFILTER_VISIBLE);
				ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
				
				//for (ale= anim_data.first; ale; ale= ale->next)
				//	gplayer_make_cfra_list(ale->data, &elems, 1);
			}
			else {
				filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
				ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
				
				for (ale= anim_data.first; ale; ale= ale->next)
					ANIM_ipo_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_to_cfraelem, NULL);
			}
			BLI_freelistN(&anim_data);
			break;
			
		case ACTKEYS_COLUMNSEL_CFRA: /* current frame */
			/* make a single CfraElem for storing this */
			ce= MEM_callocN(sizeof(CfraElem), "cfraElem");
			BLI_addtail(&bed.list, ce);
			
			ce->cfra= (float)CFRA;
			break;
			
		case ACTKEYS_COLUMNSEL_MARKERS_COLUMN: /* list of selected markers */
			// FIXME: markers api needs to be improved for this first!
			//make_marker_cfra_list(&elems, 1);
			return; // XXX currently, this does nothing!
			break;
			
		default: /* invalid option */
			return;
	}
	
	/* set up BezTriple edit callbacks */
	select_cb= ANIM_editkeyframes_select(SELECT_ADD);
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ONLYICU);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		/* loop over cfraelems (stored in the BeztEditData->list)
		 *	- we need to do this here, as we can apply fewer NLA-mapping conversions
		 */
		for (ce= bed.list.first; ce; ce= ce->next) {
			/* set frame for validation callback to refer to */
			if (nob)
				bed.f1= get_action_frame(nob, ce->cfra);
			else
				bed.f1= ce->cfra;
			
			/* select elements with frame number matching cfraelem */
			ANIM_icu_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
			
#if 0 // XXX reenable when Grease Pencil stuff is back
			if (ale->type == ANIMTYPE_GPLAYER) {
				bGPDlayer *gpl= (bGPDlayer *)ale->data;
				bGPDframe *gpf;
				
				for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
					if (ecfra == gpf->framenum) 
						gpf->flag |= GP_FRAME_SELECT;
				}
			}
			//else... 
#endif // XXX reenable when Grease Pencil stuff is back
		}
	}
	
	/* free elements */
	BLI_freelistN(&bed.list);
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_columnselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* action to take depends on the mode */
	mode= RNA_enum_get(op->ptr, "mode");
	
	if (mode == ACTKEYS_COLUMNSEL_MARKERS_BETWEEN)
		markers_selectkeys_between(&ac);
	else
		columnselect_action_keys(&ac, mode);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_keyframes_columnselect (wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "ACT_OT_keyframes_columnselect";
	
	/* api callbacks */
	ot->exec= actkeys_columnselect_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	prop= RNA_def_property(ot->srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_column_select_types);
}

/* ******************** Mouse-Click Select Operator *********************** */
/* This operator works in one of three ways:
 *	- 1) keyframe under mouse - no special modifiers
 *	- 2) all keyframes on the same side of current frame indicator as mouse - ALT modifier
 *	- 3) column select all keyframes in frame under mouse - CTRL modifier
 *
 * In addition to these basic options, the SHIFT modifier can be used to toggle the 
 * selection mode between replacing the selection (without) and inverting the selection (with).
 */

/* defines for left-right select tool */
EnumPropertyItem prop_leftright_select_types[] = {
	{ACTKEYS_LRSEL_TEST, "CHECK", "Check if Select Left or Right", ""},
	{ACTKEYS_LRSEL_NONE, "OFF", "Don't select", ""},
	{ACTKEYS_LRSEL_LEFT, "LEFT", "Before current frame", ""},
	{ACTKEYS_LRSEL_RIGHT, "RIGHT", "After current frame", ""},
	{0, NULL, NULL, NULL}
};

/* ------------------- */
 
/* option 1) select keyframe directly under mouse */
static void mouse_action_keys (bAnimContext *ac, int mval[2], short selectmode)
{
	Object *ob= NULL;
	bDopeSheet *ads= NULL;
	bAction	*act= NULL;
	bActionGroup *agrp= NULL;
	bActionChannel *achan= NULL;
	bConstraintChannel *conchan= NULL;
	Ipo *ipo= NULL;
	IpoCurve *icu= NULL;
	bGPdata *gpd = NULL;
	bGPDlayer *gpl = NULL;
	
	BeztEditData bed;
	BeztEditFunc select_cb, ok_cb;
	void *anim_channel;
	short sel, chan_type = 0;
	float selx = 0.0f, selxa;
	
	/* determine what type of data we are operating on */
	if (ac->datatype == ANIMCONT_ACTION) 
		act= (bAction *)ac->data;
	else if (ac->datatype == ANIMCONT_DOPESHEET) 
		ads= (bDopeSheet *)ac->data;
	else if (ac->datatype == ANIMCONT_GPENCIL) 
		gpd= (bGPdata *)ac->data;
	
	/* get channel and selection info */
	anim_channel= get_nearest_action_key(ac, mval, &selx, &sel, &chan_type, &achan);
	if (anim_channel == NULL) 
		return;
	
	switch (chan_type) {
		case ANIMTYPE_ICU:
			icu= (IpoCurve *)anim_channel;
			break;
		case ANIMTYPE_CONCHAN:
			conchan= (bConstraintChannel *)anim_channel;
			break;
		case ANIMTYPE_ACHAN:
			achan= (bActionChannel *)anim_channel;
			break;
		case ANIMTYPE_GROUP:
			agrp= (bActionGroup *)anim_channel;
			break;
		case ANIMTYPE_DSMAT:
			ipo= ((Material *)anim_channel)->ipo;
			break;
		case ANIMTYPE_DSLAM:
			ipo= ((Lamp *)anim_channel)->ipo;
			break;
		case ANIMTYPE_DSCAM:
			ipo= ((Camera *)anim_channel)->ipo;
			break;
		case ANIMTYPE_DSCUR:
			ipo= ((Curve *)anim_channel)->ipo;
			break;
		case ANIMTYPE_DSSKEY:
			ipo= ((Key *)anim_channel)->ipo;
			break;
		case ANIMTYPE_FILLACTD:
			act= (bAction *)anim_channel;
			break;
		case ANIMTYPE_FILLIPOD:
			ipo= ((Object *)anim_channel)->ipo;
			break;
		case ANIMTYPE_OBJECT:
			ob= ((Base *)anim_channel)->object;
			break;
		case ANIMTYPE_GPLAYER:
			gpl= (bGPDlayer *)anim_channel;
			break;
		default:
			return;
	}
	
	/* for replacing selection, firstly need to clear existing selection */
	if (selectmode == SELECT_REPLACE) {
		selectmode = SELECT_ADD;
		
		deselect_action_keys(ac, 0, 0);
		
		if (ELEM(ac->datatype, ANIMCONT_ACTION, ANIMCONT_DOPESHEET)) {
			ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
			
			/* Highlight either an Action-Channel or Action-Group */
			if (achan) {
				achan->flag |= ACHAN_SELECTED;
				//hilight_channel(act, achan, 1);
				//select_poseelement_by_name(achan->name, 2);	/* 2 is activate */
			}
			else if (agrp) {
				agrp->flag |= AGRP_SELECTED;
				//set_active_actiongroup(act, agrp, 1);
			}
		}
		else if (ac->datatype == ANIMCONT_GPENCIL) {
			ANIM_deselect_anim_channels(ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
			
			/* Highlight gpencil layer */
			gpl->flag |= GP_LAYER_SELECT;
			//gpencil_layer_setactive(gpd, gpl);
		}
	}
	
	/* get functions for selecting keyframes */
	select_cb= ANIM_editkeyframes_select(selectmode);
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.f1= selx;
	
	/* apply selection to keyframes */
	if (icu)
		ANIM_icu_keys_bezier_loop(&bed, icu, ok_cb, select_cb, NULL);
	else if (ipo)
		ANIM_ipo_keys_bezier_loop(&bed, ipo, ok_cb, select_cb, NULL);
	else if (conchan)
		ANIM_ipo_keys_bezier_loop(&bed, conchan->ipo, ok_cb, select_cb, NULL);
	else if (achan)
		ANIM_ipo_keys_bezier_loop(&bed, achan->ipo, ok_cb, select_cb, NULL);
	else if (agrp) {
		for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
			ANIM_ipo_keys_bezier_loop(&bed, achan->ipo, ok_cb, select_cb, NULL);
			
			for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
				ANIM_ipo_keys_bezier_loop(&bed, conchan->ipo, ok_cb, select_cb, NULL);
		}
	}
	else if (act) {
		for (achan= act->chanbase.first; achan; achan= achan->next) {
			ANIM_ipo_keys_bezier_loop(&bed, achan->ipo, ok_cb, select_cb, NULL);
			
			for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
				ANIM_ipo_keys_bezier_loop(&bed, conchan->ipo, ok_cb, select_cb, NULL);
		}
	}
	else if (ob) {
		if (ob->ipo) {
			bed.f1= selx;
			ANIM_ipo_keys_bezier_loop(&bed, ob->ipo, ok_cb, select_cb, NULL);
		}
		
		if (ob->action) {
			selxa= get_action_frame(ob, selx);
			bed.f1= selxa;
			
			for (achan= ob->action->chanbase.first; achan; achan= achan->next) {
				ANIM_ipo_keys_bezier_loop(&bed, achan->ipo, ok_cb, select_cb, NULL);
				
				for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next)
					ANIM_ipo_keys_bezier_loop(&bed, conchan->ipo, ok_cb, select_cb, NULL);
			}
		}
		
		if (ob->constraintChannels.first) {
			bed.f1= selx;
			
			for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next)
				ANIM_ipo_keys_bezier_loop(&bed, conchan->ipo, ok_cb, select_cb, NULL);
		}
		
		// FIXME: add data ipos too...
	}
	//else if (gpl)
	//	select_gpencil_frame(gpl, (int)selx, selectmode);
}

/* Option 2) Selects all the keyframes on either side of the current frame (depends on which side the mouse is on) */
static void selectkeys_leftright (bAnimContext *ac, short leftright, short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditFunc ok_cb, select_cb;
	BeztEditData bed;
	Scene *scene= ac->scene;
	
	/* if select mode is replace, deselect all keyframes first */
	if (select_mode==SELECT_REPLACE) {
		select_mode=SELECT_ADD;
		deselect_action_keys(ac, 0, 0);
	}
	
	/* set callbacks and editing data */
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb= ANIM_editkeyframes_select(select_mode);
	
	memset(&bed, 0, sizeof(BeztEditFunc));
	if (leftright == ACTKEYS_LRSEL_LEFT) {
		bed.f1 = -MAXFRAMEF;
		bed.f2 = (float)(CFRA + 0.1f);
	} 
	else {
		bed.f1 = (float)(CFRA - 0.1f);
		bed.f2 = MAXFRAMEF;
	}
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
		
	/* select keys on the side where most data occurs */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		if (nob) {
			ANIM_nla_mapping_apply_ipo(nob, ale->key_data, 0, 1);
			ANIM_ipo_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_ipo(nob, ale->key_data, 1, 1);
		}
		//else if (ale->type == ANIMTYPE_GPLAYER)
		//	borderselect_gplayer_frames(ale->data, min, max, SELECT_ADD);
		else
			ANIM_ipo_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* Option 3) Selects all visible keyframes in the same frame as the mouse click */
static void mouse_columnselect_action_keys (bAnimContext *ac, float selx)
{
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditFunc select_cb, ok_cb;
	BeztEditData bed;
	
	/* initialise keyframe editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* set up BezTriple edit callbacks */
	select_cb= ANIM_editkeyframes_select(SELECT_ADD);
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_ONLYICU);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		/* set frame for validation callback to refer to */
		if (nob)
			bed.f1= get_action_frame(nob, selx);
		else
			bed.f1= selx;
		
		/* select elements with frame number matching cfraelem */
		ANIM_icu_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
			
#if 0 // XXX reenable when Grease Pencil stuff is back
			if (ale->type == ANIMTYPE_GPLAYER) {
				bGPDlayer *gpl= (bGPDlayer *)ale->data;
				bGPDframe *gpf;
				
				for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
					if (ecfra == gpf->framenum) 
						gpf->flag |= GP_FRAME_SELECT;
				}
			}
			//else... 
#endif // XXX reenable when Grease Pencil stuff is back
	}
	
	/* free elements */
	BLI_freelistN(&bed.list);
	BLI_freelistN(&anim_data);
}
 
/* ------------------- */

/* handle clicking */
static int actkeys_clickselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bAnimContext ac;
	Scene *scene;
	ARegion *ar;
	View2D *v2d;
	short selectmode;
	int mval[2];
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get useful pointers from animation context data */
	scene= ac.scene;
	ar= ac.ar;
	v2d= &ar->v2d;
	
	/* get mouse coordinates (in region coordinates) */
	mval[0]= (event->x - ar->winrct.xmin);
	mval[1]= (event->y - ar->winrct.ymin);
	
	/* select mode is either replace (deselect all, then add) or add/extend */
	// XXX this is currently only available for normal select only
	if (RNA_boolean_get(op->ptr, "extend_select"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
	
	/* figure out action to take */
	if (RNA_enum_get(op->ptr, "left_right")) {
		/* select all keys on same side of current frame as mouse */
		float x;
		
		UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, NULL);
		if (x < CFRA)
			RNA_int_set(op->ptr, "left_right", ACTKEYS_LRSEL_LEFT);
		else 	
			RNA_int_set(op->ptr, "left_right", ACTKEYS_LRSEL_RIGHT);
		
		selectkeys_leftright(&ac, RNA_enum_get(op->ptr, "left_right"), selectmode);
	}
	else if (RNA_boolean_get(op->ptr, "column_select")) {
		/* select all the keyframes that occur on the same frame as where the mouse clicked */
		float x;
		
		/* figure out where (the frame) the mouse clicked, and set all keyframes in that frame */
		UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, NULL);
		mouse_columnselect_action_keys(&ac, x);
	}
	else {
		/* select keyframe under mouse */
		mouse_action_keys(&ac, mval, selectmode);
		// XXX activate transform...
	}
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_keyframes_clickselect (wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Mouse Select Keys";
	ot->idname= "ACT_OT_keyframes_clickselect";
	
	/* api callbacks */
	ot->invoke= actkeys_clickselect_invoke;
	ot->poll= ED_operator_areaactive;
	
	/* id-props */
	// XXX should we make this into separate operators?
	prop= RNA_def_property(ot->srna, "left_right", PROP_ENUM, PROP_NONE); // ALTKEY
		//RNA_def_property_enum_items(prop, prop_actkeys_clickselect_items);
	prop= RNA_def_property(ot->srna, "extend_select", PROP_BOOLEAN, PROP_NONE); // SHIFTKEY
	prop= RNA_def_property(ot->srna, "column_select", PROP_BOOLEAN, PROP_NONE); // CTRLKEY
}

/* ************************************************************************** */
