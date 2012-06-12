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
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_clip/clip_dopesheet_ops.c
 *  \ingroup spclip
 */

#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h"

#include "UI_interface.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_view2d.h"

#include "clip_intern.h"	// own include

/********************** select channel operator *********************/

static int dopesheet_select_channel_poll(bContext *C)
{
	SpaceClip *sc = CTX_wm_space_clip(C);

	if (sc && sc->clip)
		return sc->view == SC_VIEW_DOPESHEET;

	return FALSE;
}

static int dopesheet_select_channel_exec(bContext *C, wmOperator *op)
{
	SpaceClip *sc = CTX_wm_space_clip(C);
	MovieClip *clip = ED_space_clip(sc);
	MovieTracking *tracking = &clip->tracking;
	MovieTrackingObject *object = BKE_tracking_active_object(tracking);
	MovieTrackingDopesheet *dopesheet = &tracking->dopesheet;
	MovieTrackingDopesheetChannel *channel;
	ListBase *tracksbase = BKE_tracking_object_tracks(tracking, object);
	float location[2];
	int extend = RNA_boolean_get(op->ptr, "extend");
	int current_channel_index = 0, channel_index;

	RNA_float_get_array(op->ptr, "location", location);
	channel_index = -(location[1] - (CHANNEL_FIRST + CHANNEL_HEIGHT_HALF)) / CHANNEL_STEP;

	for (channel = dopesheet->channels.first; channel; channel = channel->next) {
		MovieTrackingTrack *track = channel->track;

		if (current_channel_index == channel_index) {
			if (extend)
				track->flag ^= TRACK_DOPE_SEL;
			else
				track->flag |= TRACK_DOPE_SEL;

			if (track->flag & TRACK_DOPE_SEL) {
				tracking->act_track = track;
				BKE_tracking_select_track(tracksbase, track, TRACK_AREA_ALL, TRUE);
			}
		}
		else if (!extend)
			track->flag &= ~TRACK_DOPE_SEL;

		current_channel_index++;
	}

	WM_event_add_notifier(C, NC_GEOM | ND_SELECT, NULL);

	return OPERATOR_FINISHED;
}

static int dopesheet_select_channel_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	float location[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &location[0], &location[1]);
	RNA_float_set_array(op->ptr, "location", location);

	return dopesheet_select_channel_exec(C, op);
}

void CLIP_OT_dopesheet_select_channel(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Channel";
	ot->description = "Select movie tracking channel";
	ot->idname = "CLIP_OT_dopesheet_select_channel";

	/* api callbacks */
	ot->invoke = dopesheet_select_channel_invoke;
	ot->exec = dopesheet_select_channel_exec;
	ot->poll = dopesheet_select_channel_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_float_vector(ot->srna, "location", 2, NULL, -FLT_MAX, FLT_MAX,
		"Location", "Mouse location to select channel", -100.0f, 100.0f);
	RNA_def_boolean(ot->srna, "extend", 0,
		"Extend", "Extend selection rather than clearing the existing selection");
}
