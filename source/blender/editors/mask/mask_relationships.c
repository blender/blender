/*
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
 */

/** \file
 * \ingroup edmask
 */

#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_mask.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.h"

#include "DNA_mask_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_clip.h" /* frame remapping functions */

#include "mask_intern.h" /* own include */

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
  DEG_id_tag_update(&mask->id, 0);

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
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTracking *tracking;
  MovieTrackingTrack *track;
  MovieTrackingPlaneTrack *plane_track;
  MovieTrackingObject *tracking_object;
  /* done */

  int framenr, parent_type;
  float parmask_pos[2], orig_corners[4][2];
  const char *sub_parent_name;

  if (ELEM(NULL, sc, clip)) {
    return OPERATOR_CANCELLED;
  }

  framenr = ED_space_clip_get_clip_frame_number(sc);

  tracking = &clip->tracking;
  tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  if (tracking_object == NULL) {
    return OPERATOR_CANCELLED;
  }

  if ((track = BKE_tracking_track_get_active(tracking)) != NULL) {
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
    float marker_pos_ofs[2];

    add_v2_v2v2(marker_pos_ofs, marker->pos, track->offset);

    BKE_mask_coord_from_movieclip(clip, &sc->user, parmask_pos, marker_pos_ofs);

    sub_parent_name = track->name;
    parent_type = MASK_PARENT_POINT_TRACK;
    memset(orig_corners, 0, sizeof(orig_corners));
  }
  else if ((plane_track = BKE_tracking_plane_track_get_active(tracking)) != NULL) {
    MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

    zero_v2(parmask_pos);
    sub_parent_name = plane_track->name;
    parent_type = MASK_PARENT_PLANE_TRACK;
    memcpy(orig_corners, plane_marker->corners, sizeof(orig_corners));
  }
  else {
    return OPERATOR_CANCELLED;
  }

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
          point->parent.type = parent_type;
          BLI_strncpy(point->parent.parent, tracking_object->name, sizeof(point->parent.parent));
          BLI_strncpy(point->parent.sub_parent, sub_parent_name, sizeof(point->parent.sub_parent));

          copy_v2_v2(point->parent.parent_orig, parmask_pos);
          memcpy(point->parent.parent_corners_orig,
                 orig_corners,
                 sizeof(point->parent.parent_corners_orig));
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_MASK | ND_DATA, mask);
  DEG_id_tag_update(&mask->id, 0);

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
  // ot->invoke = mask_parent_set_invoke;
  ot->exec = mask_parent_set_exec;

  ot->poll = ED_space_clip_maskedit_mask_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
