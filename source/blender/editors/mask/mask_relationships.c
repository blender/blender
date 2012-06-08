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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mask/mask_relationshops.c
 *  \ingroup edmask
 */


#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_mask.h"
#include "BKE_tracking.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"  /* SELECT */

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_mask.h"  /* own include */

#include "mask_intern.h"  /* own include */

static int mask_parent_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;
		int i;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (MASKPOINT_ISSEL_ANY(point)) {
					point->parent.id = NULL;
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
	DAG_id_tag_update(&mask->id, 0);

	return OPERATOR_FINISHED;
}

void MASK_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Parent";
	ot->description = "Clear the mask's parenting";
	ot->idname = "MASK_OT_parent_clear";

	/* api callbacks */
	ot->exec = mask_parent_clear_exec;

	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mask_parent_set_exec(bContext *C, wmOperator *UNUSED(op))
{
	Mask *mask = CTX_data_edit_mask(C);
	MaskLayer *masklay;

	/* parent info */
	SpaceClip *sc;
	MovieClip *clip;
	MovieTrackingTrack *track;
	MovieTrackingMarker *marker;
	MovieTrackingObject *tracking;
	/* done */

	float marker_pos_ofs[2];
	float parmask_pos[2];

	if ((NULL == (sc = CTX_wm_space_clip(C))) ||
	    (NULL == (clip = sc->clip)) ||
	    (NULL == (track = clip->tracking.act_track)) ||
	    (NULL == (marker = BKE_tracking_get_marker(track, sc->user.framenr))) ||
	    (NULL == (tracking = BKE_tracking_active_object(&clip->tracking))))
	{
		return OPERATOR_CANCELLED;
	}

	add_v2_v2v2(marker_pos_ofs, marker->pos, track->offset);

	BKE_mask_coord_from_movieclip(clip, &sc->user, parmask_pos, marker_pos_ofs);

	for (masklay = mask->masklayers.first; masklay; masklay = masklay->next) {
		MaskSpline *spline;
		int i;

		if (masklay->restrictflag & (MASK_RESTRICT_VIEW | MASK_RESTRICT_SELECT)) {
			continue;
		}

		for (spline = masklay->splines.first; spline; spline = spline->next) {
			for (i = 0; i < spline->tot_point; i++) {
				MaskSplinePoint *point = &spline->points[i];

				if (MASKPOINT_ISSEL_ANY(point)) {
					point->parent.id_type = ID_MC;
					point->parent.id = &clip->id;
					strcpy(point->parent.parent, tracking->name);
					strcpy(point->parent.sub_parent, track->name);

					copy_v2_v2(point->parent.parent_orig, parmask_pos);
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
	DAG_id_tag_update(&mask->id, 0);

	return OPERATOR_FINISHED;
}

/** based on #OBJECT_OT_parent_set */
void MASK_OT_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Parent";
	ot->description = "Set the mask's parenting";
	ot->idname = "MASK_OT_parent_set";

	/* api callbacks */
	//ot->invoke = mask_parent_set_invoke;
	ot->exec = mask_parent_set_exec;

	ot->poll = ED_maskedit_mask_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
