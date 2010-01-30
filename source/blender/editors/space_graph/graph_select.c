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
#include "BLI_math.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
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
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_context.h"
#include "BKE_utildefines.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_keyframes_draw.h"
#include "ED_keyframes_edit.h"
#include "ED_markers.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_types.h"

#include "graph_intern.h"


/* ************************************************************************** */
/* KEYFRAMES STUFF */

/* ******************** Deselect All Operator ***************************** */
/* This operator works in one of three ways:
 *	1) (de)select all (AKEY) - test if select all or deselect all
 *	2) invert all (CTRL-IKEY) - invert selection of all keyframes
 *	3) (de)select all - no testing is done; only for use internal tools as normal function...
 */

/* Deselects keyframes in the Graph Editor
 *	- This is called by the deselect all operator, as well as other ones!
 *
 * 	- test: check if select or deselect all
 *	- sel: how to select keyframes 
 *		0 = deselect
 *		1 = select
 *		2 = invert
 */
static void deselect_graph_keys (bAnimContext *ac, short test, short sel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	BeztEditData bed;
	BeztEditFunc test_cb, sel_cb;
	
	/* determine type-based settings */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	
	/* filter data */
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* init BezTriple looping data */
	memset(&bed, 0, sizeof(BeztEditData));
	test_cb= ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	
	/* See if we should be selecting or deselecting */
	if (test) {
		for (ale= anim_data.first; ale; ale= ale->next) {
			if (ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, test_cb, NULL)) {
				sel= SELECT_SUBTRACT;
				break;
			}
		}
	}
	
	/* convert sel to selectmode, and use that to get editor */
	sel_cb= ANIM_editkeyframes_select(sel);
	
	/* Now set the flags */
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* Keyframes First */
		ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, sel_cb, NULL);
		
		/* only change selection of channel when the visibility of keyframes doesn't depend on this */
		if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0) {
			/* deactivate the F-Curve, and deselect if deselecting keyframes.
			 * otherwise select the F-Curve too since we've selected all the keyframes
			 */
			if (sel == SELECT_SUBTRACT) 
				fcu->flag &= ~FCURVE_SELECTED;
			else
				fcu->flag |= FCURVE_SELECTED;
		}
		
		/* always deactivate all F-Curves if we perform batch ops for selection */
		fcu->flag &= ~FCURVE_ACTIVE;
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_deselectall_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* 'standard' behaviour - check if selected, then apply relevant selection */
	if (RNA_boolean_get(op->ptr, "invert"))
		deselect_graph_keys(&ac, 0, SELECT_INVERT);
	else
		deselect_graph_keys(&ac, 1, SELECT_ADD);
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_select_all_toggle (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "GRAPH_OT_select_all_toggle";
	ot->description= "Toggle selection of all keyframes.";
	
	/* api callbacks */
	ot->exec= graphkeys_deselectall_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	RNA_def_boolean(ot->srna, "invert", 0, "Invert", "");
}

/* ******************** Border Select Operator **************************** */
/* This operator currently works in one of three ways:
 *	-> BKEY 	- 1) all keyframes within region are selected (validation with BEZT_OK_REGION)
 *	-> ALT-BKEY - depending on which axis of the region was larger...
 *		-> 2) x-axis, so select all frames within frame range (validation with BEZT_OK_FRAMERANGE)
 *		-> 3) y-axis, so select all frames within channels that region included (validation with BEZT_OK_VALUERANGE)
 */

/* Borderselect only selects keyframes now, as overshooting handles often get caught too,
 * which means that they may be inadvertantly moved as well.
 * Also, for convenience, handles should get same status as keyframe (if it was within bounds)
 */
static void borderselect_graphkeys (bAnimContext *ac, rcti rect, short mode, short selectmode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	BeztEditData bed;
	BeztEditFunc ok_cb, select_cb;
	View2D *v2d= &ac->ar->v2d;
	rctf rectf;
	
	/* convert mouse coordinates to frame ranges and channel coordinates corrected for view pan/zoom */
	UI_view2d_region_to_view(v2d, rect.xmin, rect.ymin, &rectf.xmin, &rectf.ymin);
	UI_view2d_region_to_view(v2d, rect.xmax, rect.ymax, &rectf.xmax, &rectf.ymax);
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVESONLY | ANIMFILTER_CURVEVISIBLE);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing/validation funcs  */
	select_cb= ANIM_editkeyframes_select(selectmode);
	ok_cb= ANIM_editkeyframes_ok(mode);
	
	/* init editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	bed.data= &rectf;
	
	/* loop over data, doing border select */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* apply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, ANIM_UNITCONV_ONLYKEYS);
		
		/* apply NLA mapping to all the keyframes, since it's easier than trying to
		 * guess when a callback might use something different
		 */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
		
		/* set horizontal range (if applicable) 
		 * NOTE: these values are only used for x-range and y-range but not region 
		 * 		(which uses bed.data, i.e. rectf)
		 */
		if (mode != BEZT_OK_VALUERANGE) {
			bed.f1= rectf.xmin;
			bed.f2= rectf.xmax;
		}
		else {
			bed.f1= rectf.ymin;
			bed.f2= rectf.ymax;
		}
		
		/* firstly, check if any keyframes will be hit by this */
		if (ANIM_fcurve_keys_bezier_loop(&bed, fcu, NULL, ok_cb, NULL)) {
			/* select keyframes that are in the appropriate places */
			ANIM_fcurve_keys_bezier_loop(&bed, fcu, ok_cb, select_cb, NULL);
			
			/* only change selection of channel when the visibility of keyframes doesn't depend on this */
			if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0) {
				/* select the curve too now that curve will be touched */
				if (selectmode == SELECT_ADD)
					fcu->flag |= FCURVE_SELECTED;
			}
		}
		
		/* un-apply NLA mapping from all the keyframes */
		if (adt)
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
			
		/* unapply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, ANIM_UNITCONV_RESTORE|ANIM_UNITCONV_ONLYKEYS);
	}
	
	/* cleanup */
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_borderselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	rcti rect;
	short mode=0, selectmode=0;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	if (RNA_int_get(op->ptr, "gesture_mode")==GESTURE_MODAL_SELECT)
		selectmode= SELECT_ADD;
	else
		selectmode= SELECT_SUBTRACT;

	/* get settings from operator */
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");
	
	/* selection 'mode' depends on whether borderselect region only matters on one axis */
	if (RNA_boolean_get(op->ptr, "axis_range")) {
		/* mode depends on which axis of the range is larger to determine which axis to use 
		 *	- checking this in region-space is fine, as it's fundamentally still going to be a different rect size
		 *	- the frame-range select option is favoured over the channel one (x over y), as frame-range one is often
		 *	  used for tweaking timing when "blocking", while channels is not that useful...
		 */
		if ((rect.xmax - rect.xmin) >= (rect.ymax - rect.ymin))
			mode= BEZT_OK_FRAMERANGE;
		else
			mode= BEZT_OK_VALUERANGE;
	}
	else 
		mode= BEZT_OK_REGION;
	
	/* apply borderselect action */
	borderselect_graphkeys(&ac, rect, mode, selectmode);
	
	/* send notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_SELECT, NULL);
	
	return OPERATOR_FINISHED;
} 

void GRAPH_OT_select_border(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Border Select";
	ot->idname= "GRAPH_OT_select_border";
	ot->description= "Select all keyframes within the specified region.";
	
	/* api callbacks */
	ot->invoke= WM_border_select_invoke;
	ot->exec= graphkeys_borderselect_exec;
	ot->modal= WM_border_select_modal;
	
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* rna */
	WM_operator_properties_gesture_border(ot, FALSE);
	
	RNA_def_boolean(ot->srna, "axis_range", 0, "Axis Range", "");
}

/* ******************** Column Select Operator **************************** */
/* This operator works in one of four ways:
 *	- 1) select all keyframes in the same frame as a selected one  (KKEY)
 *	- 2) select all keyframes in the same frame as the current frame marker (CTRL-KKEY)
 *	- 3) select all keyframes in the same frame as a selected markers (SHIFT-KKEY)
 *	- 4) select all keyframes that occur between selected markers (ALT-KKEY)
 */

/* defines for column-select mode */
static EnumPropertyItem prop_column_select_types[] = {
	{GRAPHKEYS_COLUMNSEL_KEYS, "KEYS", 0, "On Selected Keyframes", ""},
	{GRAPHKEYS_COLUMNSEL_CFRA, "CFRA", 0, "On Current Frame", ""},
	{GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN, "MARKERS_COLUMN", 0, "On Selected Markers", ""},
	{GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN, "MARKERS_BETWEEN", 0, "Between Min/Max Selected Markers", ""},
	{0, NULL, 0, NULL, NULL}
};

/* ------------------- */ 

/* Selects all visible keyframes between the specified markers */
static void markers_selectkeys_between (bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditFunc ok_cb, select_cb;
	BeztEditData bed;
	float min, max;
	
	/* get extreme markers */
	ED_markers_get_minmax(ac->markers, 1, &min, &max);
	min -= 0.5f;
	max += 0.5f;
	
	/* get editing funcs + data */
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb= ANIM_editkeyframes_select(SELECT_ADD);
	
	memset(&bed, 0, sizeof(BeztEditData));
	bed.f1= min; 
	bed.f2= max;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* select keys in-between */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {	
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else {
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
		}
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}


/* Selects all visible keyframes in the same frames as the specified elements */
static void columnselect_graph_keys (bAnimContext *ac, short mode)
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
		case GRAPHKEYS_COLUMNSEL_KEYS: /* list of selected keys */
			filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
			ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
			
			for (ale= anim_data.first; ale; ale= ale->next)
				ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_to_cfraelem, NULL);
			
			BLI_freelistN(&anim_data);
			break;
			
		case GRAPHKEYS_COLUMNSEL_CFRA: /* current frame */
			/* make a single CfraElem for storing this */
			ce= MEM_callocN(sizeof(CfraElem), "cfraElem");
			BLI_addtail(&bed.list, ce);
			
			ce->cfra= (float)CFRA;
			break;
			
		case GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN: /* list of selected markers */
			ED_markers_make_cfra_list(ac->markers, &bed.list, 1);
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
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* loop over cfraelems (stored in the BeztEditData->list)
		 *	- we need to do this here, as we can apply fewer NLA-mapping conversions
		 */
		for (ce= bed.list.first; ce; ce= ce->next) {
			/* set frame for validation callback to refer to */
			if (ale)
				bed.f1= BKE_nla_tweakedit_remap(adt, ce->cfra, NLATIME_CONVERT_UNMAP);
			else
				bed.f1= ce->cfra;
			
			/* select elements with frame number matching cfraelem */
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
		}
	}
	
	/* free elements */
	BLI_freelistN(&bed.list);
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int graphkeys_columnselect_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* action to take depends on the mode */
	mode= RNA_enum_get(op->ptr, "mode");
	
	if (mode == GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN)
		markers_selectkeys_between(&ac);
	else
		columnselect_graph_keys(&ac, mode);
	
	/* set notifier that keyframe selection has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}
 
void GRAPH_OT_select_column (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select All";
	ot->idname= "GRAPH_OT_select_column";
	ot->description= "Select all keyframes on the specified frame(s).";
	
	/* api callbacks */
	ot->exec= graphkeys_columnselect_exec;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* props */
	RNA_def_enum(ot->srna, "mode", prop_column_select_types, 0, "Mode", "");
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
static EnumPropertyItem prop_graphkeys_leftright_select_types[] = {
	{GRAPHKEYS_LRSEL_TEST, "CHECK", 0, "Check if Select Left or Right", ""},
	{GRAPHKEYS_LRSEL_NONE, "OFF", 0, "Don't select", ""},
	{GRAPHKEYS_LRSEL_LEFT, "LEFT", 0, "Before current frame", ""},
	{GRAPHKEYS_LRSEL_RIGHT, "RIGHT", 0, "After current frame", ""},
	{0, NULL, 0, NULL, NULL}
};

/* ------------------- */

enum {
	NEAREST_HANDLE_LEFT	= 0,
	NEAREST_HANDLE_KEY,
	NEAREST_HANDLE_RIGHT
} eHandleIndex; 
 
/* Find the vertex (either handle (0/2) or the keyframe (1)) that is nearest to the mouse cursor (in area coordinates)  
 * Selected verts get a disadvantage, to make it easier to select handles behind.
 * Returns eHandleIndex
 */
static short findnearest_fcurve_vert (bAnimContext *ac, int mval[2], FCurve **fcurve, BezTriple **bezt)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	View2D *v2d= &ac->ar->v2d;
	int hpoint=0, sco[3][2];
	int dist= 100, temp, i;
	
	/* clear pointers first */
	*fcurve= 0;
	*bezt= 0;
	
	/* get curves to search through 
	 *	- if the option to only show keyframes that belong to selected F-Curves is enabled,
	 *	  include the 'only selected' flag...
	 */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	if (sipo->flag & SIPO_SELCUVERTSONLY) 	// FIXME: this should really be check for by the filtering code...
		filter |= ANIMFILTER_SEL;
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* apply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, 0);
		
		/* try to progressively get closer to the right point... */
		if (fcu->bezt) {
			BezTriple *bezt1=fcu->bezt, *prevbezt=NULL;
			
			/* apply NLA mapping to all the keyframes */
			if (adt)
				ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			
			for (i=0; i < fcu->totvert; i++, prevbezt=bezt1, bezt1++) {
				/* convert beztriple points to screen-space */
				UI_view2d_to_region_no_clip(v2d, bezt1->vec[0][0], bezt1->vec[0][1], &sco[0][0], &sco[0][1]);
				UI_view2d_to_region_no_clip(v2d, bezt1->vec[1][0], bezt1->vec[1][1], &sco[1][0], &sco[1][1]);
				UI_view2d_to_region_no_clip(v2d, bezt1->vec[2][0], bezt1->vec[2][1], &sco[2][0], &sco[2][1]);
				
				/* keyframe - do select? */
				temp= abs(mval[0] - sco[1][0]) + abs(mval[1] - sco[1][1]);
				
				if (bezt1->f2 & SELECT) 
					temp += 5;
				
				if (temp < dist) { 
					hpoint= NEAREST_HANDLE_KEY; 
					*bezt= bezt1; 
					dist= temp; 
					*fcurve= fcu; 
				}
				
				/* handles - only do them if they're visible */
				// XXX also need to check for int-values only?
				if ((sipo->flag & SIPO_NOHANDLES)==0) {
					/* first handle only visible if previous segment had handles */
					if ( (!prevbezt && (bezt1->ipo==BEZT_IPO_BEZ)) || (prevbezt && (prevbezt->ipo==BEZT_IPO_BEZ)) )
					{
						temp= -3 + abs(mval[0] - sco[0][0]) + abs(mval[1] - sco[0][1]);
						if (bezt1->f1 & SELECT) 
							temp += 5;
							
						if (temp < dist) { 
							hpoint= NEAREST_HANDLE_LEFT; 
							*bezt= bezt1; 
							dist= temp; 
							*fcurve= fcu; 
						}
					}
					
					/* second handle only visible if this segment is bezier */
					if (bezt1->ipo == BEZT_IPO_BEZ) 
					{
						temp= abs(mval[0] - sco[2][0]) + abs(mval[1] - sco[2][1]);
						if (bezt1->f3 & SELECT) 
							temp += 5;
						
						if (temp < dist) { 
							hpoint= NEAREST_HANDLE_RIGHT; 
							*bezt=bezt1; 
							dist= temp; 
							*fcurve= fcu; 
						}
					}
				}
			}
			
			/* un-apply NLA mapping from all the keyframes */
			if (adt)
				ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		
		/* unapply unit corrections */
		ANIM_unit_mapping_apply_fcurve(ac->scene, ale->id, ale->key_data, ANIM_UNITCONV_RESTORE);
	}
	
	/* free channels */
	BLI_freelistN(&anim_data);
	
	/* return handle */
	return hpoint;
}
 
/* option 1) select keyframe directly under mouse */
static void mouse_graph_keys (bAnimContext *ac, int mval[], short select_mode, short curves_only)
{
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	FCurve *fcu;
	BezTriple *bezt;
	short handle;
	int filter;
	
	/* find the beztriple that we're selecting, and the handle that was clicked on */
	handle= findnearest_fcurve_vert(ac, mval, &fcu, &bezt);
	
	/* check if anything to select */
	if (fcu == NULL)	
		return;
	
	/* deselect all other curves? */
	if (select_mode == SELECT_REPLACE) {
		/* reset selection mode */
		select_mode= SELECT_ADD;
		
		/* deselect all other keyframes */
		deselect_graph_keys(ac, 0, SELECT_SUBTRACT);
		
		/* deselect other channels too, but only only do this if 
		 * selection of channel when the visibility of keyframes 
		 * doesn't depend on this 
		 */
		if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0)
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
	}
	
	/* if points can be selected on this F-Curve */
	// TODO: what about those with no keyframes?
	if ((curves_only == 0) && ((fcu->flag & FCURVE_PROTECTED)==0)) {
		/* only if there's keyframe */
		if (bezt) {
			/* depends on selection mode */
			if (select_mode == SELECT_INVERT) {
				/* keyframe - invert select of all */
				if (handle == NEAREST_HANDLE_KEY) {
					if (BEZSELECTED(bezt)) {
						BEZ_DESEL(bezt);
					}
					else {
						BEZ_SEL(bezt);
					}
				}
				
				/* handles - toggle selection of relevant handle */
				else if (handle == NEAREST_HANDLE_LEFT) {
					/* toggle selection */
					bezt->f1 ^= SELECT;
				}
				else {
					/* toggle selection */
					bezt->f3 ^= SELECT;
				}
			}
			else {
				/* if the keyframe was clicked on, select all verts of given beztriple */
				if (handle == NEAREST_HANDLE_KEY) {
					BEZ_SEL(bezt);
				}
				/* otherwise, select the handle that applied */
				else if (handle == NEAREST_HANDLE_LEFT) 
					bezt->f1 |= SELECT;
				else 
					bezt->f3 |= SELECT;
			}
		}
	}
	else {
		BeztEditFunc select_cb;
		BeztEditData bed;
		
		/* initialise keyframe editing data */
		memset(&bed, 0, sizeof(BeztEditData));
		
		/* set up BezTriple edit callbacks */
		select_cb= ANIM_editkeyframes_select(select_mode);
		
		/* select all keyframes */
		ANIM_fcurve_keys_bezier_loop(&bed, fcu, NULL, select_cb, NULL);
	}
	
	/* only change selection of channel when the visibility of keyframes doesn't depend on this */
	if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0) {
		/* select or deselect curve? */
		if (select_mode == SELECT_INVERT)
			fcu->flag ^= FCURVE_SELECTED;
		else if (select_mode == SELECT_ADD)
			fcu->flag |= FCURVE_SELECTED;
			
		/* set active F-Curve (NOTE: sync the filter flags with findnearest_fcurve_vert) */
		if (fcu->flag & FCURVE_SELECTED) {
			filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
			ANIM_set_active_channel(ac, ac->data, ac->datatype, filter, fcu, ANIMTYPE_FCURVE);
		}
	}
}

/* Option 2) Selects all the keyframes on either side of the current frame (depends on which side the mouse is on) */
static void graphkeys_mselect_leftright (bAnimContext *ac, short leftright, short select_mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	BeztEditFunc ok_cb, select_cb;
	BeztEditData bed;
	Scene *scene= ac->scene;
	
	/* if select mode is replace, deselect all keyframes (and channels) first */
	if (select_mode==SELECT_REPLACE) {
		/* reset selection mode to add to selection */
		select_mode= SELECT_ADD;
		
		/* deselect all other keyframes */
		deselect_graph_keys(ac, 0, SELECT_SUBTRACT);
		
		/* deselect other channels too, but only only do this if 
		 * selection of channel when the visibility of keyframes 
		 * doesn't depend on this 
		 */
		if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0)
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
	}
	
	/* set callbacks and editing data */
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAMERANGE);
	select_cb= ANIM_editkeyframes_select(select_mode);
	
	memset(&bed, 0, sizeof(BeztEditFunc));
	if (leftright == GRAPHKEYS_LRSEL_LEFT) {
		bed.f1 = MINAFRAMEF;
		bed.f2 = (float)(CFRA + 0.1f);
	} 
	else {
		bed.f1 = (float)(CFRA - 0.1f);
		bed.f2 = MAXFRAMEF;
	}
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
		
	/* select keys on the side where most data occurs */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else
			ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
	}
	
	/* Cleanup */
	BLI_freelistN(&anim_data);
}

/* Option 3) Selects all visible keyframes in the same frame as the mouse click */
static void graphkeys_mselect_column (bAnimContext *ac, int mval[2], short select_mode)
{
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	SpaceIpo *sipo= (SpaceIpo *)ac->sa->spacedata.first;
	BeztEditFunc select_cb, ok_cb;
	BeztEditData bed;
	FCurve *fcu;
	BezTriple *bezt;
	float selx = (float)ac->scene->r.cfra;
	
	/* find the beztriple that occurs on this frame, and use his as the frame number we're using */
	findnearest_fcurve_vert(ac, mval, &fcu, &bezt);
	
	/* check if anything to select */
	if (ELEM(NULL, fcu, bezt))	
		return;
	selx= bezt->vec[1][0];
	
	/* if select mode is replace, deselect all keyframes (and channels) first */
	if (select_mode==SELECT_REPLACE) {
		/* reset selection mode to add to selection */
		select_mode= SELECT_ADD;
		
		/* deselect all other keyframes */
		deselect_graph_keys(ac, 0, SELECT_SUBTRACT);
		
		/* deselect other channels too, but only only do this if 
		 * selection of channel when the visibility of keyframes 
		 * doesn't depend on this 
		 */
		if ((sipo->flag & SIPO_SELCUVERTSONLY) == 0)
			ANIM_deselect_anim_channels(ac, ac->data, ac->datatype, 0, ACHANNEL_SETFLAG_CLEAR);
	}
	
	/* initialise keyframe editing data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* set up BezTriple edit callbacks */
	select_cb= ANIM_editkeyframes_select(select_mode);
	ok_cb= ANIM_editkeyframes_ok(BEZT_OK_FRAME);
	
	/* loop through all of the keys and select additional keyframes
	 * based on the keys found to be selected above
	 */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_CURVEVISIBLE | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);
		
		/* set frame for validation callback to refer to */
		if (adt)
			bed.f1= BKE_nla_tweakedit_remap(adt, selx, NLATIME_CONVERT_UNMAP);
		else
			bed.f1= selx;
		
		/* select elements with frame number matching cfra */
		ANIM_fcurve_keys_bezier_loop(&bed, ale->key_data, ok_cb, select_cb, NULL);
	}
	
	/* free elements */
	BLI_freelistN(&bed.list);
	BLI_freelistN(&anim_data);
}
 
/* ------------------- */

/* handle clicking */
static int graphkeys_clickselect_invoke(bContext *C, wmOperator *op, wmEvent *event)
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
	if (RNA_boolean_get(op->ptr, "extend"))
		selectmode= SELECT_INVERT;
	else
		selectmode= SELECT_REPLACE;
	
	/* figure out action to take */
	if (RNA_enum_get(op->ptr, "left_right")) {
		/* select all keys on same side of current frame as mouse */
		float x;
		
		UI_view2d_region_to_view(v2d, mval[0], mval[1], &x, NULL);
		if (x < CFRA)
			RNA_int_set(op->ptr, "left_right", GRAPHKEYS_LRSEL_LEFT);
		else 	
			RNA_int_set(op->ptr, "left_right", GRAPHKEYS_LRSEL_RIGHT);
		
		graphkeys_mselect_leftright(&ac, RNA_enum_get(op->ptr, "left_right"), selectmode);
	}
	else if (RNA_boolean_get(op->ptr, "column")) {
		/* select all keyframes in the same frame as the one that was under the mouse */
		graphkeys_mselect_column(&ac, mval, selectmode);
	}
	else if (RNA_boolean_get(op->ptr, "curves")) {
		/* select all keyframes in F-Curve under mouse */
		mouse_graph_keys(&ac, mval, selectmode, 1);
	}
	else {
		/* select keyframe under mouse */
		mouse_graph_keys(&ac, mval, selectmode, 0);
	}
	
	/* set notifier that keyframe selection (and also channel selection in some cases) has changed */
	WM_event_add_notifier(C, NC_ANIMATION|ND_KEYFRAME_SELECT|ND_ANIMCHAN_SELECT, NULL);
	
	/* for tweak grab to work */
	return OPERATOR_FINISHED|OPERATOR_PASS_THROUGH;
}
 
void GRAPH_OT_clickselect (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Mouse Select Keys";
	ot->idname= "GRAPH_OT_clickselect";
	ot->description= "Select keyframes by clicking on them.";
	
	/* api callbacks */
	ot->invoke= graphkeys_clickselect_invoke;
	ot->poll= graphop_visible_keyframes_poll;
	
	/* id-props */
	// XXX should we make this into separate operators?
	RNA_def_enum(ot->srna, "left_right", prop_graphkeys_leftright_select_types, 0, "Left Right", ""); // CTRLKEY
	RNA_def_boolean(ot->srna, "extend", 0, "Extend Select", ""); // SHIFTKEY
	RNA_def_boolean(ot->srna, "column", 0, "Column Select", "Select all keyframes that occur on the same frame as the one under the mouse"); // ALTKEY
	RNA_def_boolean(ot->srna, "curves", 0, "Only Curves", "Select all the keyframes in the curve"); // CTRLKEY + ALTKEY
}

/* ************************************************************************** */
