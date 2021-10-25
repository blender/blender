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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_action/action_edit.c
 *  \ingroup spaction
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mask_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_gpencil.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_report.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_markers.h"
#include "ED_mask.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "action_intern.h"


/* ************************************************************************** */
/* POSE MARKERS STUFF */

/* *************************** Localise Markers ***************************** */

/* ensure that there is:
 *  1) an active action editor
 *  2) that the mode will have an active action available
 *  3) that the set of markers being shown are the scene markers, not the list we're merging
 *	4) that there are some selected markers
 */
static int act_markers_make_local_poll(bContext *C)
{
	SpaceAction *sact = CTX_wm_space_action(C);
	
	/* 1) */
	if (sact == NULL)
		return 0;
	
	/* 2) */
	if (ELEM(sact->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY) == 0)
		return 0;
	if (sact->action == NULL)
		return 0;
		
	/* 3) */
	if (sact->flag & SACTION_POSEMARKERS_SHOW)
		return 0;
		
	/* 4) */
	return ED_markers_get_first_selected(ED_context_get_markers(C)) != NULL;
}

static int act_markers_make_local_exec(bContext *C, wmOperator *UNUSED(op))
{	
	ListBase *markers = ED_context_get_markers(C);
	
	SpaceAction *sact = CTX_wm_space_action(C);
	bAction *act = (sact) ? sact->action : NULL;
	
	TimeMarker *marker, *markern = NULL;
	
	/* sanity checks */
	if (ELEM(NULL, markers, act))
		return OPERATOR_CANCELLED;
		
	/* migrate markers */
	for (marker = markers->first; marker; marker = markern) {
		markern = marker->next;
		
		/* move if marker is selected */
		if (marker->flag & SELECT) {
			BLI_remlink(markers, marker);
			BLI_addtail(&act->markers, marker);
		}
	}
	
	/* now enable the "show posemarkers only" setting, so that we can see that something did happen */
	sact->flag |= SACTION_POSEMARKERS_SHOW;
	
	/* notifiers - both sets, as this change affects both */
	WM_event_add_notifier(C, NC_SCENE | ND_MARKERS, NULL);
	WM_event_add_notifier(C, NC_ANIMATION | ND_MARKERS, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_markers_make_local(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Markers Local";
	ot->idname = "ACTION_OT_markers_make_local";
	ot->description = "Move selected scene markers to the active Action as local 'pose' markers";
	
	/* callbacks */
	ot->exec = act_markers_make_local_exec;
	ot->poll = act_markers_make_local_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************** */
/* KEYFRAME-RANGE STUFF */

/* *************************** Calculate Range ************************** */

/* Get the min/max keyframes*/
static bool get_keyframe_extents(bAnimContext *ac, float *min, float *max, const short onlySel)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	bool found = false;
	
	/* get data to filter, from Action or Dopesheet */
	/* XXX: what is sel doing here?!
	 *      Commented it, was breaking things (eg. the "auto preview range" tool). */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_SEL *//*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* set large values to try to override */
	*min = 999999999.0f;
	*max = -999999999.0f;
	
	/* check if any channels to set range with */
	if (anim_data.first) {
		/* go through channels, finding max extents */
		for (ale = anim_data.first; ale; ale = ale->next) {
			AnimData *adt = ANIM_nla_mapping_get(ac, ale);
			if (ale->datatype == ALE_GPFRAME) {
				bGPDlayer *gpl = ale->data;
				bGPDframe *gpf;

				/* find gp-frame which is less than or equal to cframe */
				for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
					const float framenum = (float)gpf->framenum;
					*min = min_ff(*min, framenum);
					*max = max_ff(*max, framenum);
					found = true;
				}
			}
			else if (ale->datatype == ALE_MASKLAY) {
				MaskLayer *masklay = ale->data;
				MaskLayerShape *masklay_shape;

				/* find mask layer which is less than or equal to cframe */
				for (masklay_shape = masklay->splines_shapes.first;
				     masklay_shape;
				     masklay_shape = masklay_shape->next)
				{
					const float framenum = (float)masklay_shape->frame;
					*min = min_ff(*min, framenum);
					*max = max_ff(*max, framenum);
					found = true;
				}
			}
			else {
				FCurve *fcu = (FCurve *)ale->key_data;
				float tmin, tmax;

				/* get range and apply necessary scaling before processing */
				if (calc_fcurve_range(fcu, &tmin, &tmax, onlySel, false)) {

					if (adt) {
						tmin = BKE_nla_tweakedit_remap(adt, tmin, NLATIME_CONVERT_MAP);
						tmax = BKE_nla_tweakedit_remap(adt, tmax, NLATIME_CONVERT_MAP);
					}

					/* try to set cur using these values, if they're more extreme than previously set values */
					*min = min_ff(*min, tmin);
					*max = max_ff(*max, tmax);
					found = true;
				}
			}
		}

		if (fabsf(*max - *min) < 0.001f) {
			*min -= 0.0005f;
			*max += 0.0005f;
		}

		/* free memory */
		ANIM_animdata_freelist(&anim_data);
	}
	else {
		/* set default range */
		if (ac->scene) {
			*min = (float)ac->scene->r.sfra;
			*max = (float)ac->scene->r.efra;
		}
		else {
			*min = -5;
			*max = 100;
		}
	}

	return found;
}

/* ****************** Automatic Preview-Range Operator ****************** */

static int actkeys_previewrange_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	Scene *scene;
	float min, max;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	if (ac.scene == NULL)
		return OPERATOR_CANCELLED;
	else
		scene = ac.scene;
	
	/* set the range directly */
	get_keyframe_extents(&ac, &min, &max, false);
	scene->r.flag |= SCER_PRV_RANGE;
	scene->r.psfra = floorf(min);
	scene->r.pefra = ceilf(max);

	if (scene->r.psfra == scene->r.pefra) {
		scene->r.pefra = scene->r.psfra + 1;
	}
	
	/* set notifier that things have changed */
	// XXX err... there's nothing for frame ranges yet, but this should do fine too
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_previewrange_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Auto-Set Preview Range";
	ot->idname = "ACTION_OT_previewrange_set";
	ot->description = "Set Preview Range based on extents of selected Keyframes";
	
	/* api callbacks */
	ot->exec = actkeys_previewrange_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** View-All Operator ****************** */

/**
 * Find the extents of the active channel
 *
 * \param[out] min Bottom y-extent of channel
 * \param[out] max Top y-extent of channel
 * \return Success of finding a selected channel
 */
static bool actkeys_channels_get_selected_extents(bAnimContext *ac, float *min, float *max)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	short found = 0; /* NOTE: not bool, since we want prioritise individual channels over expanders */
	float y;
	
	/* get all items - we need to do it this way */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_LIST_CHANNELS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through all channels, finding the first one that's selected */
	y = (float)ACHANNEL_FIRST(ac);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		const bAnimChannelType *acf = ANIM_channel_get_typeinfo(ale);
		
		/* must be selected... */
		if (acf && acf->has_setting(ac, ale, ACHANNEL_SETTING_SELECT) && 
		    ANIM_channel_setting_get(ac, ale, ACHANNEL_SETTING_SELECT))
		{
			/* update best estimate */
			*min = (float)(y - ACHANNEL_HEIGHT_HALF(ac));
			*max = (float)(y + ACHANNEL_HEIGHT_HALF(ac));
			
			/* is this high enough priority yet? */
			found = acf->channel_role;
			
			/* only stop our search when we've found an actual channel
			 * - datablock expanders get less priority so that we don't abort prematurely
			 */
			if (found == ACHANNEL_ROLE_CHANNEL) {
				break;
			}
		}
		
		/* adjust y-position for next one */
		y -= ACHANNEL_STEP(ac);
	}
	
	/* free all temp data */
	ANIM_animdata_freelist(&anim_data);
	
	return (found != 0);
}

static int actkeys_viewall(bContext *C, const bool only_sel)
{
	bAnimContext ac;
	View2D *v2d;
	float extra, min, max;
	bool found;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	v2d = &ac.ar->v2d;
	
	/* set the horizontal range, with an extra offset so that the extreme keys will be in view */
	found = get_keyframe_extents(&ac, &min, &max, only_sel);

	if (only_sel && (found == false))
		return OPERATOR_CANCELLED;

	v2d->cur.xmin = min;
	v2d->cur.xmax = max;

	extra = 0.1f * BLI_rctf_size_x(&v2d->cur);
	v2d->cur.xmin -= extra;
	v2d->cur.xmax += extra;
	
	/* set vertical range */
	if (only_sel == false) {
		/* view all -> the summary channel is usually the shows everything, and resides right at the top... */
		v2d->cur.ymax = 0.0f;
		v2d->cur.ymin = (float)-BLI_rcti_size_y(&v2d->mask);
	}
	else {
		/* locate first selected channel (or the active one), and frame those */
		float ymin = v2d->cur.ymin;
		float ymax = v2d->cur.ymax;
		
		if (actkeys_channels_get_selected_extents(&ac, &ymin, &ymax)) {
			/* recenter the view so that this range is in the middle */
			float ymid = (ymax - ymin) / 2.0f + ymin;
			float x_center;
			
			UI_view2d_center_get(v2d, &x_center, NULL);
			UI_view2d_center_set(v2d, x_center, ymid);
		}
	}
	
	/* do View2D syncing */
	UI_view2d_sync(CTX_wm_screen(C), CTX_wm_area(C), v2d, V2D_LOCK_COPY);
	
	/* just redraw this view */
	ED_area_tag_redraw(CTX_wm_area(C));
	
	return OPERATOR_FINISHED;
}

/* ......... */

static int actkeys_viewall_exec(bContext *C, wmOperator *UNUSED(op))
{	
	/* whole range */
	return actkeys_viewall(C, false);
}

static int actkeys_viewsel_exec(bContext *C, wmOperator *UNUSED(op))
{
	/* only selected */
	return actkeys_viewall(C, true);
}

/* ......... */

void ACTION_OT_view_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View All";
	ot->idname = "ACTION_OT_view_all";
	ot->description = "Reset viewable area to show full keyframe range";
	
	/* api callbacks */
	ot->exec = actkeys_viewall_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ACTION_OT_view_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Selected";
	ot->idname = "ACTION_OT_view_selected";
	ot->description = "Reset viewable area to show selected keyframes range";
	
	/* api callbacks */
	ot->exec = actkeys_viewsel_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** View-All Operator ****************** */

static int actkeys_view_frame_exec(bContext *C, wmOperator *op)
{
	const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
	ANIM_center_frame(C, smooth_viewtx);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_view_frame(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "View Frame";
	ot->idname = "ACTION_OT_view_frame";
	ot->description = "Reset viewable area to show range around current frame";
	
	/* api callbacks */
	ot->exec = actkeys_view_frame_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************** */
/* GENERAL STUFF */

/* ******************** Copy/Paste Keyframes Operator ************************* */
/* NOTE: the backend code for this is shared with the graph editor */

static short copy_action_keys(bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok = 0;
	
	/* clear buffer first */
	ANIM_fcurves_copybuf_free();
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* copy keyframes */
	ok = copy_animedit_keys(ac, &anim_data);
	
	/* clean up */
	ANIM_animdata_freelist(&anim_data);

	return ok;
}


static short paste_action_keys(bAnimContext *ac,
                               const eKeyPasteOffset offset_mode, const eKeyMergeMode merge_mode, bool flip)
{	
	ListBase anim_data = {NULL, NULL};
	int filter, ok = 0;
	
	/* filter data 
	 * - First time we try to filter more strictly, allowing only selected channels 
	 *   to allow copying animation between channels
	 * - Second time, we loosen things up if nothing was found the first time, allowing
	 *   users to just paste keyframes back into the original curve again [#31670]
	 */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	
	if (ANIM_animdata_filter(ac, &anim_data, filter | ANIMFILTER_SEL, ac->data, ac->datatype) == 0)
		ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* paste keyframes */
	ok = paste_animedit_keys(ac, &anim_data, offset_mode, merge_mode, flip);

	/* clean up */
	ANIM_animdata_freelist(&anim_data);

	return ok;
}

/* ------------------- */

static int actkeys_copy_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;

	/* copy keyframes */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		if (ED_gpencil_anim_copybuf_copy(&ac) == false) {
			/* Nothing got copied - An error about this should be been logged already */
			return OPERATOR_CANCELLED;
		}
	}
	else if (ac.datatype == ANIMCONT_MASK) {
		/* FIXME... */
		BKE_report(op->reports, RPT_ERROR, "Keyframe pasting is not available for mask mode");
		return OPERATOR_CANCELLED;
	}
	else {
		if (copy_action_keys(&ac)) {
			BKE_report(op->reports, RPT_ERROR, "No keyframes copied to keyframes copy/paste buffer");
			return OPERATOR_CANCELLED;
		}
	}
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Keyframes";
	ot->idname = "ACTION_OT_copy";
	ot->description = "Copy selected keyframes to the copy/paste buffer";
	
	/* api callbacks */
	ot->exec = actkeys_copy_exec;
	ot->poll = ED_operator_action_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int actkeys_paste_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;

	const eKeyPasteOffset offset_mode = RNA_enum_get(op->ptr, "offset");
	const eKeyMergeMode merge_mode = RNA_enum_get(op->ptr, "merge");
	const bool flipped = RNA_boolean_get(op->ptr, "flipped");
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* ac.reports by default will be the global reports list, which won't show warnings */
	ac.reports = op->reports;
	
	/* paste keyframes */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		if (ED_gpencil_anim_copybuf_paste(&ac, offset_mode) == false) {
			/* An error occurred - Reports should have been fired already */
			return OPERATOR_CANCELLED;
		}
	}
	else if (ac.datatype == ANIMCONT_MASK) {
		/* FIXME... */
		BKE_report(op->reports, RPT_ERROR, "Keyframe pasting is not available for grease pencil or mask mode");
		return OPERATOR_CANCELLED;
	}
	else {
		/* non-zero return means an error occurred while trying to paste */
		if (paste_action_keys(&ac, offset_mode, merge_mode, flipped)) {
			return OPERATOR_CANCELLED;
		}
	}

	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_paste(wmOperatorType *ot)
{
	PropertyRNA *prop;
	/* identifiers */
	ot->name = "Paste Keyframes";
	ot->idname = "ACTION_OT_paste";
	ot->description = "Paste keyframes from copy/paste buffer for the selected channels, starting on the current frame";
	
	/* api callbacks */
//	ot->invoke = WM_operator_props_popup; // better wait for action redo panel
	ot->exec = actkeys_paste_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "offset", rna_enum_keyframe_paste_offset_items, KEYFRAME_PASTE_OFFSET_CFRA_START, "Offset", "Paste time offset of keys");
	RNA_def_enum(ot->srna, "merge", rna_enum_keyframe_paste_merge_items, KEYFRAME_PASTE_MERGE_MIX, "Type", "Method of merging pasted keys and existing");
	prop = RNA_def_boolean(ot->srna, "flipped", false, "Flipped", "Paste keyframes from mirrored bones if they exist");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* ******************** Insert Keyframes Operator ************************* */

/* defines for insert keyframes tool */
static EnumPropertyItem prop_actkeys_insertkey_types[] = {
	{1, "ALL", 0, "All Channels", ""},
	{2, "SEL", 0, "Only Selected Channels", ""},
	{3, "GROUP", 0, "In Active Group", ""},  /* XXX not in all cases */
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for inserting new keyframes */
static void insert_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	ReportList *reports = ac->reports;
	Scene *scene = ac->scene;
	ToolSettings *ts = scene->toolsettings;
	short flag = 0;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	if (mode == 2) filter |= ANIMFILTER_SEL;
	else if (mode == 3) filter |= ANIMFILTER_ACTGROUPED;
	
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* init keyframing flag */
	flag = ANIM_get_keyframing_flags(scene, 1);
	
	/* insert keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		FCurve *fcu = (FCurve *)ale->key_data;
		float cfra;
		
		/* adjust current frame for NLA-scaling */
		if (adt)
			cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
		else 
			cfra = (float)CFRA;
			
		/* read value from property the F-Curve represents, or from the curve only?
		 * - ale->id != NULL:    Typically, this means that we have enough info to try resolving the path
		 * - ale->owner != NULL: If this is set, then the path may not be resolvable from the ID alone,
		 *                       so it's easier for now to just read the F-Curve directly.
		 *                       (TODO: add the full-blown PointerRNA relative parsing case here...)
		 */
		if (ale->id && !ale->owner) {
			insert_keyframe(reports, ale->id, NULL, ((fcu->grp) ? (fcu->grp->name) : (NULL)), fcu->rna_path, fcu->array_index, cfra, ts->keyframe_type, flag);
		}
		else {
			const float curval = evaluate_fcurve(fcu, cfra);
			insert_vert_fcurve(fcu, cfra, curval, ts->keyframe_type, 0);
		}
		
		ale->update |= ANIM_UPDATE_DEFAULT;
	}
	
	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* this function is for inserting new grease pencil frames */
static void insert_gpencil_keys(bAnimContext *ac, short mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	Scene *scene = ac->scene;
	ToolSettings *ts = scene->toolsettings;
	eGP_GetFrame_Mode add_frame_mode;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	if (mode == 2) filter |= ANIMFILTER_SEL;
	
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	
	/* add a copy or a blank frame? */
	if (ts->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST)
		add_frame_mode = GP_GETFRAME_ADD_COPY; /* XXX: actframe may not be what we want? */
	else
		add_frame_mode = GP_GETFRAME_ADD_NEW;
	
	
	/* insert gp frames */
	for (ale = anim_data.first; ale; ale = ale->next) {
		bGPDlayer *gpl = (bGPDlayer *)ale->data;
		BKE_gpencil_layer_getframe(gpl, CFRA, add_frame_mode);
	}
	
	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_insertkey_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ac.datatype == ANIMCONT_MASK) {
		BKE_report(op->reports, RPT_ERROR, "Insert Keyframes is not yet implemented for this mode");
		return OPERATOR_CANCELLED;
	}
		
	/* what channels to affect? */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* insert keyframes */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		insert_gpencil_keys(&ac, mode);
	}
	else {
		insert_action_keys(&ac, mode);
	}

	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_keyframe_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Insert Keyframes";
	ot->idname = "ACTION_OT_keyframe_insert";
	ot->description = "Insert keyframes for the specified channels";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_insertkey_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_insertkey_types, 0, "Type", "");
}

/* ******************** Duplicate Keyframes Operator ************************* */

static void duplicate_action_keys(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and delete selected keys */
	for (ale = anim_data.first; ale; ale = ale->next) {
		if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE))
			duplicate_fcurve_keys((FCurve *)ale->key_data);
		else if (ale->type == ANIMTYPE_GPLAYER)
			ED_gplayer_frames_duplicate((bGPDlayer *)ale->data);
		else if (ale->type == ANIMTYPE_MASKLAYER)
			ED_masklayer_frames_duplicate((MaskLayer *)ale->data);
		else
			BLI_assert(0);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_duplicate_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* duplicate keyframes */
	duplicate_action_keys(&ac);

	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Keyframes";
	ot->idname = "ACTION_OT_duplicate";
	ot->description = "Make a copy of all selected keyframes";
	
	/* api callbacks */
	ot->exec = actkeys_duplicate_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Delete Keyframes Operator ************************* */

static bool delete_action_keys(bAnimContext *ac)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	bool changed_final = false;

	/* filter data */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);

	/* loop through filtered data and delete selected keys */
	for (ale = anim_data.first; ale; ale = ale->next) {
		bool changed = false;

		if (ale->type == ANIMTYPE_GPLAYER) {
			changed = ED_gplayer_frames_delete((bGPDlayer *)ale->data);
		}
		else if (ale->type == ANIMTYPE_MASKLAYER) {
			changed = ED_masklayer_frames_delete((MaskLayer *)ale->data);
		}
		else {
			FCurve *fcu = (FCurve *)ale->key_data;
			AnimData *adt = ale->adt;
			
			/* delete selected keyframes only */
			changed = delete_fcurve_keys(fcu);
			
			/* Only delete curve too if it won't be doing anything anymore */
			if ((fcu->totvert == 0) && (list_has_suitable_fmodifier(&fcu->modifiers, 0, FMI_TYPE_GENERATE_CURVE) == 0)) {
				ANIM_fcurve_delete_from_animdata(ac, adt, fcu);
				ale->key_data = NULL;
			}
		}
		
		if (changed) {
			ale->update |= ANIM_UPDATE_DEFAULT;
			changed_final = true;
		}
	}
	
	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
	
	return changed_final;
}

/* ------------------- */

static int actkeys_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* delete keyframes */
	if (!delete_action_keys(&ac))
		return OPERATOR_CANCELLED;
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_REMOVED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Keyframes";
	ot->idname = "ACTION_OT_delete";
	ot->description = "Remove all selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = actkeys_delete_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Clean Keyframes Operator ************************* */

static void clean_action_keys(bAnimContext *ac, float thresh, bool clean_chan)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_SEL /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and clean curves */
	for (ale = anim_data.first; ale; ale = ale->next) {
		clean_fcurve(ac, ale, thresh, clean_chan);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_clean_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	float thresh;
	bool clean_chan;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented");
		return OPERATOR_PASS_THROUGH;
	}
		
	/* get cleaning threshold */
	thresh = RNA_float_get(op->ptr, "threshold");
	clean_chan = RNA_boolean_get(op->ptr, "channels");
	
	/* clean keyframes */
	clean_action_keys(&ac, thresh, clean_chan);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_clean(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clean Keyframes";
	ot->idname = "ACTION_OT_clean";
	ot->description = "Simplify F-Curves by removing closely spaced keyframes";
	
	/* api callbacks */
	//ot->invoke =  // XXX we need that number popup for this! 
	ot->exec = actkeys_clean_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_float(ot->srna, "threshold", 0.001f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 1000.0f);
	RNA_def_boolean(ot->srna, "channels", false, "Channels", "");
}

/* ******************** Sample Keyframes Operator *********************** */

/* Evaluates the curves between each selected keyframe on each frame, and keys the value  */
static void sample_action_keys(bAnimContext *ac)
{	
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through filtered data and add keys between selected keyframes on every frame  */
	for (ale = anim_data.first; ale; ale = ale->next) {
		sample_fcurve((FCurve *)ale->key_data);

		ale->update |= ANIM_UPDATE_DEPS;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_sample_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented");
		return OPERATOR_PASS_THROUGH;
	}
	
	/* sample keyframes */
	sample_action_keys(&ac);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_sample(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Sample Keyframes";
	ot->idname = "ACTION_OT_sample";
	ot->description = "Add keyframes on every frame between the selected keyframes";
	
	/* api callbacks */
	ot->exec = actkeys_sample_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************************** */
/* SETTINGS STUFF */

/* ******************** Set Extrapolation-Type Operator *********************** */

/* defines for make/clear cyclic extrapolation tools */
#define MAKE_CYCLIC_EXPO    -1
#define CLEAR_CYCLIC_EXPO   -2

/* defines for set extrapolation-type for selected keyframes tool */
static EnumPropertyItem prop_actkeys_expo_types[] = {
	{FCURVE_EXTRAPOLATE_CONSTANT, "CONSTANT", 0, "Constant Extrapolation", "Values on endpoint keyframes are held"},
	{FCURVE_EXTRAPOLATE_LINEAR, "LINEAR", 0, "Linear Extrapolation", "Straight-line slope of end segments are extended past the endpoint keyframes"},
	
	{MAKE_CYCLIC_EXPO, "MAKE_CYCLIC", 0, "Make Cyclic (F-Modifier)", "Add Cycles F-Modifier if one doesn't exist already"},
	{CLEAR_CYCLIC_EXPO, "CLEAR_CYCLIC", 0, "Clear Cyclic (F-Modifier)", "Remove Cycles F-Modifier if not needed anymore"},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for setting extrapolation mode for keyframes */
static void setexpo_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_SEL /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting mode per F-Curve */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->data;
		
		if (mode >= 0) {
			/* just set mode setting */
			fcu->extend = mode;
		}
		else {
			/* shortcuts for managing Cycles F-Modifiers to make it easier to toggle cyclic animation 
			 * without having to go through FModifier UI in Graph Editor to do so
			 */
			if (mode == MAKE_CYCLIC_EXPO) {
				/* only add if one doesn't exist */
				if (list_has_suitable_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES, -1) == 0) {
					/* TODO: add some more preset versions which set different extrapolation options? */
					add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CYCLES);
				}
			}
			else if (mode == CLEAR_CYCLIC_EXPO) {
				/* remove all the modifiers fitting this description */
				FModifier *fcm, *fcn = NULL;
				
				for (fcm = fcu->modifiers.first; fcm; fcm = fcn) {
					fcn = fcm->next;
					
					if (fcm->type == FMODIFIER_TYPE_CYCLES)
						remove_fmodifier(&fcu->modifiers, fcm);
				}
			}
		}

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_expo_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented");
		return OPERATOR_PASS_THROUGH;
	}
		
	/* get handle setting mode */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setexpo_action_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_extrapolation_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Extrapolation";
	ot->idname = "ACTION_OT_extrapolation_type";
	ot->description = "Set extrapolation mode for selected F-Curves";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_expo_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_expo_types, 0, "Type", "");
}

/* ******************** Set Interpolation-Type Operator *********************** */

/* this function is responsible for setting interpolation mode for keyframes */
static void setipo_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	KeyframeEditFunc set_cb = ANIM_editkeyframes_ipo(mode);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple interpolation
	 * Note: we do not supply KeyframeEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale = anim_data.first; ale; ale = ale->next) {
		ANIM_fcurve_keyframes_loop(NULL, ale->key_data, NULL, set_cb, calchandles_fcurve);

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_ipo_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented");
		return OPERATOR_PASS_THROUGH;
	}
		
	/* get handle setting mode */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	setipo_action_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_interpolation_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Interpolation";
	ot->idname = "ACTION_OT_interpolation_type";
	ot->description = "Set interpolation mode for the F-Curve segments starting from the selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_ipo_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_beztriple_interpolation_mode_items, 0, "Type", "");
}

/* ******************** Set Handle-Type Operator *********************** */

/* this function is responsible for setting handle-type of selected keyframes */
static void sethandles_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditFunc edit_cb = ANIM_editkeyframes_handles(mode);
	KeyframeEditFunc sel_cb = ANIM_editkeyframes_ok(BEZT_OK_SELECTED);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting flags for handles 
	 * Note: we do not supply KeyframeEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale = anim_data.first; ale; ale = ale->next) {
		FCurve *fcu = (FCurve *)ale->key_data;
		
		/* any selected keyframes for editing? */
		if (ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, sel_cb, NULL)) {
			/* change type of selected handles */
			ANIM_fcurve_keyframes_loop(NULL, fcu, NULL, edit_cb, calchandles_fcurve);

			ale->update |= ANIM_UPDATE_DEFAULT;
		}
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_handletype_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented");
		return OPERATOR_PASS_THROUGH;
	}
		
	/* get handle setting mode */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	sethandles_action_keys(&ac, mode);
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_handle_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Handle Type";
	ot->idname = "ACTION_OT_handle_type";
	ot->description = "Set type of handle for selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_handletype_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_keyframe_handle_type_items, 0, "Type", "");
}

/* ******************** Set Keyframe-Type Operator *********************** */

/* this function is responsible for setting keyframe type for keyframes */
static void setkeytype_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	KeyframeEditFunc set_cb = ANIM_editkeyframes_keytype(mode);
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through setting BezTriple interpolation
	 * Note: we do not supply KeyframeEditData to the looper yet. Currently that's not necessary here...
	 */
	for (ale = anim_data.first; ale; ale = ale->next) {
		ANIM_fcurve_keyframes_loop(NULL, ale->key_data, NULL, set_cb, NULL);

		ale->update |= ANIM_UPDATE_DEPS | ANIM_UPDATE_HANDLES;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* this function is responsible for setting the keyframe type for Grease Pencil frames */
static void setkeytype_gpencil_keys(bAnimContext *ac, short mode)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* filter data */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* loop through each layer */
	for (ale = anim_data.first; ale; ale = ale->next) {
		if (ale->type == ANIMTYPE_GPLAYER) {
			ED_gplayer_frames_keytype_set(ale->data, mode);
			ale->update |= ANIM_UPDATE_DEPS;
		}
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_keytype_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	if (ac.datatype == ANIMCONT_MASK) {
		BKE_report(op->reports, RPT_ERROR, "Not implemented for Masks");
		return OPERATOR_PASS_THROUGH;
	}
		
	/* get handle setting mode */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* set handle type */
	if (ac.datatype == ANIMCONT_GPENCIL) {
		setkeytype_gpencil_keys(&ac, mode);
	}
	else {
		setkeytype_action_keys(&ac, mode);
	}
	
	/* set notifier that keyframe properties have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME_PROP, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_keyframe_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Keyframe Type";
	ot->idname = "ACTION_OT_keyframe_type";
	ot->description = "Set type of keyframe for the selected keyframes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_keytype_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_beztriple_keyframe_type_items, 0, "Type", "");
}

/* ************************************************************************** */
/* TRANSFORM STUFF */

/* ***************** Jump to Selected Frames Operator *********************** */

static int actkeys_framejump_poll(bContext *C)
{
	/* prevent changes during render */
	if (G.is_rendering)
		return 0;

	return ED_operator_action_active(C);
}

/* snap current-frame indicator to 'average time' of selected keyframe */
static int actkeys_framejump_exec(bContext *C, wmOperator *UNUSED(op))
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	KeyframeEditData ked = {{NULL}};
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
	
	/* init edit data */
	/* loop over action data, averaging values */
	filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE /*| ANIMFILTER_CURVESONLY */ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1); 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, bezt_calc_average, NULL);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, bezt_calc_average, NULL);
	}
	
	ANIM_animdata_freelist(&anim_data);
	
	/* set the new current frame value, based on the average time */
	if (ked.i1) {
		Scene *scene = ac.scene;
		CFRA = iroundf(ked.f1 / ked.i1);
		SUBFRA = 0.f;
	}
	
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_SCENE | ND_FRAME, ac.scene);
	
	return OPERATOR_FINISHED;
}

void ACTION_OT_frame_jump(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Jump to Keyframes";
	ot->idname = "ACTION_OT_frame_jump";
	ot->description = "Set the current frame to the average frame value of selected keyframes";
	
	/* api callbacks */
	ot->exec = actkeys_framejump_exec;
	ot->poll = actkeys_framejump_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Snap Keyframes Operator *********************** */

/* defines for snap keyframes tool */
static EnumPropertyItem prop_actkeys_snap_types[] = {
	{ACTKEYS_SNAP_CFRA, "CFRA", 0, "Current frame",
	 "Snap selected keyframes to the current frame"},
	{ACTKEYS_SNAP_NEAREST_FRAME, "NEAREST_FRAME", 0, "Nearest Frame",
	 "Snap selected keyframes to the nearest (whole) frame (use to fix accidental sub-frame offsets)"},
	{ACTKEYS_SNAP_NEAREST_SECOND, "NEAREST_SECOND", 0, "Nearest Second",
	 "Snap selected keyframes to the nearest second"},
	{ACTKEYS_SNAP_NEAREST_MARKER, "NEAREST_MARKER", 0, "Nearest Marker",
	 "Snap selected keyframes to the nearest marker"},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for snapping keyframes to frame-times */
static void snap_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked = {{NULL}};
	KeyframeEditFunc edit_cb;
	
	/* filter data */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* get beztriple editing callbacks */
	edit_cb = ANIM_editkeyframes_snap(mode);

	ked.scene = ac->scene;
	if (mode == ACTKEYS_SNAP_NEAREST_MARKER) {
		ked.list.first = (ac->markers) ? ac->markers->first : NULL;
		ked.list.last = (ac->markers) ? ac->markers->last : NULL;
	}
	
	/* snap keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		if (ale->type == ANIMTYPE_GPLAYER) {
			ED_gplayer_snap_frames(ale->data, ac->scene, mode);
		}
		else if (ale->type == ANIMTYPE_MASKLAYER) {
			ED_masklayer_snap_frames(ale->data, ac->scene, mode);
		}
		else if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 0); 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0);
		}
		else {
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);
		}

		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
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
	mode = RNA_enum_get(op->ptr, "type");
	
	/* snap keyframes */
	snap_action_keys(&ac, mode);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_snap(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Keys";
	ot->idname = "ACTION_OT_snap";
	ot->description = "Snap selected keyframes to the times specified";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_snap_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_snap_types, 0, "Type", "");
}

/* ******************** Mirror Keyframes Operator *********************** */

/* defines for mirror keyframes tool */
static EnumPropertyItem prop_actkeys_mirror_types[] = {
	{ACTKEYS_MIRROR_CFRA, "CFRA", 0, "By Times over Current frame",
	 "Flip times of selected keyframes using the current frame as the mirror line"},
	{ACTKEYS_MIRROR_XAXIS, "XAXIS", 0, "By Values over Value=0",
	 "Flip values of selected keyframes (i.e. negative values become positive, and vice versa)"},
	{ACTKEYS_MIRROR_MARKER, "MARKER", 0, "By Times over First Selected Marker",
	 "Flip times of selected keyframes using the first selected marker as the reference point"},
	{0, NULL, 0, NULL, NULL}
};

/* this function is responsible for mirroring keyframes */
static void mirror_action_keys(bAnimContext *ac, short mode) 
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	KeyframeEditData ked = {{NULL}};
	KeyframeEditFunc edit_cb;
	
	/* get beztriple editing callbacks */
	edit_cb = ANIM_editkeyframes_mirror(mode);

	ked.scene = ac->scene;
	
	/* for 'first selected marker' mode, need to find first selected marker first! */
	/* XXX should this be made into a helper func in the API? */
	if (mode == ACTKEYS_MIRROR_MARKER) {
		TimeMarker *marker = ED_markers_get_first_selected(ac->markers);
		
		if (marker)
			ked.f1 = (float)marker->frame;
		else
			return;
	}
	
	/* filter data */
	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK))
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NODUPLIS);
	else
		filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/ | ANIMFILTER_NODUPLIS);
	ANIM_animdata_filter(ac, &anim_data, filter, ac->data, ac->datatype);
	
	/* mirror keyframes */
	for (ale = anim_data.first; ale; ale = ale->next) {
		AnimData *adt = ANIM_nla_mapping_get(ac, ale);
		
		if (ale->type == ANIMTYPE_GPLAYER) {
			ED_gplayer_mirror_frames(ale->data, ac->scene, mode);
		}
		else if (ale->type == ANIMTYPE_MASKLAYER) {
			/* TODO */
		}
		else if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 0); 
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 0);
		}
		else {
			ANIM_fcurve_keyframes_loop(&ked, ale->key_data, NULL, edit_cb, calchandles_fcurve);
		}
		
		ale->update |= ANIM_UPDATE_DEFAULT;
	}

	ANIM_animdata_update(ac, &anim_data);
	ANIM_animdata_freelist(&anim_data);
}

/* ------------------- */

static int actkeys_mirror_exec(bContext *C, wmOperator *op)
{
	bAnimContext ac;
	short mode;
	
	/* get editor data */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return OPERATOR_CANCELLED;
		
	/* get mirroring mode */
	mode = RNA_enum_get(op->ptr, "type");
	
	/* mirror keyframes */
	mirror_action_keys(&ac, mode);
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Mirror Keys";
	ot->idname = "ACTION_OT_mirror";
	ot->description = "Flip selected keyframes over the selected mirror line";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = actkeys_mirror_exec;
	ot->poll = ED_operator_action_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* id-props */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_actkeys_mirror_types, 0, "Type", "");
}

/* ************************************************************************** */
