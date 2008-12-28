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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
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

/* ************************************************************************** */
/* SETTINGS STUFF */

/* ************************************************************************** */
/* TRANSFORM STUFF */

/* ***************** Snap Current Frame Operator *********************** */

/* helper callback for actkeys_cfrasnap_exec() -> used to help get the average time of all selected beztriples */
// TODO: if some other code somewhere needs this, it'll be time to port this over to keyframes_edit.c!!!
static short bezt_calc_average(BeztEditData *bed, BezTriple *bezt)
{
	/* only if selected */
	if (bezt->f2 & SELECT) {
		/* store average time in float (only do rounding at last step */
		bed->f1 += bezt->vec[1][0];
		
		/* increment number of items */
		bed->i1++;
	}
	
	return 0;
}

/* snap current-frame indicator to 'average time' of selected keyframe */
static int actkeys_cfrasnap_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	ListBase anim_data= {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	BeztEditData bed;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* init edit data */
	memset(&bed, 0, sizeof(BeztEditData));
	
	/* loop over action data, averaging values */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_IPOKEYS);
	ANIM_animdata_filter(&anim_data, filter, ac.data, ac.datatype);
	
	for (ale= anim_data.first; ale; ale= ale->next)
		ipo_keys_bezier_loop(&bed, ale->key_data, NULL, bezt_calc_average, NULL);
	
	BLI_freelistN(&anim_data);
	
	/* set the new current frame value, based on the average time */
	if (bed.i1) {
		Scene *scene= ac.scene;
		CFRA= (int)floor((bed.f1 / bed.i1) + 0.5f);
	}
	
	/* set notifier tha things have changed */
	WM_event_add_notifier(C, NC_SCENE|ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}

void ACT_OT_keyframes_cfrasnap (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Snap Current Frame to Keys";
	ot->idname= "ACT_OT_keyframes_cfrasnap";
	
	/* api callbacks */
	ot->exec= actkeys_cfrasnap_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
}

/* ******************** Snap Keyframes Operator *********************** */

/* defines for snap keyframes tool */
EnumPropertyItem prop_actkeys_snap_types[] = {
	{ACTKEYS_SNAP_CFRA, "CFRA", "Current frame", ""},
	{ACTKEYS_SNAP_NEAREST_FRAME, "NEAREST_FRAME", "Nearest Frame", ""}, // XXX as single entry?
	{ACTKEYS_SNAP_NEAREST_SECOND, "NEAREST_SECOND", "Nearest Second", ""}, // XXX as single entry?
	{ACTKEYS_SNAP_NEAREST_MARKER, "NEAREST_MARKER", "Nearest Marker", ""},
	{0, NULL, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void snap_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc edit_cb;
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_IPOKEYS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing callbacks */
	edit_cb= ANIM_editkeyframes_snap(mode);
	
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.scene= ac->scene;
	
	/* snap keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		if (nob) {
			ANIM_nla_mapping_apply(nob, ale->key_data, 0, 1); 
			ipo_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_ipocurve);
			ANIM_nla_mapping_apply(nob, ale->key_data, 1, 1);
		}
		//else if (ale->type == ACTTYPE_GPLAYER)
		//	snap_gplayer_frames(ale->data, mode);
		else 
			ipo_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_ipocurve);
	}
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_snap_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get snapping mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* snap keyframes */
	snap_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_keyframes_snap (wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Snap Keys";
	ot->idname= "ACT_OT_keyframes_snap";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_snap_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* id-props */
	prop= RNA_def_property(ot->srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_actkeys_snap_types);
}

/* ******************** Mirror Keyframes Operator *********************** */

/* defines for snap keyframes tool */
EnumPropertyItem prop_actkeys_mirror_types[] = {
	{ACTKEYS_MIRROR_CFRA, "CFRA", "Current frame", ""},
	{ACTKEYS_MIRROR_YAXIS, "YAXIS", "Vertical Axis", ""},
	{ACTKEYS_MIRROR_XAXIS, "XAXIS", "Horizontal Axis", ""},
	{ACTKEYS_MIRROR_MARKER, "MARKER", "First Selected Marker", ""},
	{0, NULL, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void mirror_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BeztEditData bed;
	BeztEditFunc edit_cb;
	
	/* get beztriple editing callbacks */
	edit_cb= ANIM_editkeyframes_mirror(mode);
	
	memset(&bed, 0, sizeof(BeztEditData)); 
	bed.scene= ac->scene;
	
	/* for 'first selected marker' mode, need to find first selected marker first! */
	// XXX should this be made into a helper func in the API?
	if (mode == ACTKEYS_MIRROR_MARKER) {
		Scene *scene= ac->scene;
		TimeMarker *marker= NULL;
		
		/* find first selected marker */
		for (marker= scene->markers.first; marker; marker=marker->next) {
			if (marker->flag & SELECT) {
				break;
			}
		}
		
		/* store marker's time (if available) */
		if (marker)
			bed.f1= marker->frame;
		else
			return;
	}
	
	/* filter data */
	if (ac->datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_IPOKEYS);
	ANIM_animdata_filter(&anim_data, filter, ac->data, ac->datatype);
	
	/* mirror keyframes */
	for (ale= anim_data.first; ale; ale= ale->next) {
		Object *nob= ANIM_nla_mapping_get(ac, ale);
		
		if (nob) {
			ANIM_nla_mapping_apply(nob, ale->key_data, 0, 1); 
			ipo_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_ipocurve);
			ANIM_nla_mapping_apply(nob, ale->key_data, 1, 1);
		}
		//else if (ale->type == ACTTYPE_GPLAYER)
		//	snap_gplayer_frames(ale->data, mode);
		else 
			ipo_keys_bezier_loop(&bed, ale->key_data, NULL, edit_cb, calchandles_ipocurve);
	}
	BLI_freelistN(&anim_data);
}

/* ------------------- */

static int actkeys_mirror_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get snapping mode */
	mode= RNA_enum_get(op->ptr, "type");
	
	/* mirror keyframes */
	mirror_action_keys(&ac, mode);
	
	/* validate keyframes after editing */
	ANIM_editkeyframes_refresh(&ac);
	
	/* set notifier tha things have changed */
	ED_area_tag_redraw(CTX_wm_area(C)); // FIXME... should be updating 'keyframes' data context or so instead!
	
	return OPERATOR_FINISHED;
}
 
void ACT_OT_keyframes_mirror (wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name= "Mirror Keys";
	ot->idname= "ACT_OT_keyframes_mirror";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= actkeys_mirror_exec;
	ot->poll= ED_operator_areaactive;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER/*|OPTYPE_UNDO*/;
	
	/* id-props */
	prop= RNA_def_property(ot->srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_actkeys_mirror_types);
}

/* ************************************************************************** */
