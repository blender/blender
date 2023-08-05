/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmask
 */

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_mask.h"
#include "BKE_tracking.h"

#include "DEG_depsgraph.h"

#include "DNA_mask_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh" /* frame remapping functions */
#include "ED_mask.hh"
#include "ED_screen.hh"

#include "mask_intern.h" /* own include */

static int mask_parent_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Mask *mask = CTX_data_edit_mask(C);

  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      for (int i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          point->parent.id = nullptr;
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

  ot->poll = ED_maskedit_mask_visible_splines_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int mask_parent_set_exec(bContext *C, wmOperator * /*op*/)
{
  Mask *mask = CTX_data_edit_mask(C);

  /* parent info */
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  MovieTrackingTrack *track;
  MovieTrackingPlaneTrack *plane_track;
  MovieTrackingObject *tracking_object;
  /* done */

  int framenr, parent_type;
  float parmask_pos[2], orig_corners[4][2];
  const char *sub_parent_name;

  if (ELEM(nullptr, sc, clip)) {
    return OPERATOR_CANCELLED;
  }

  framenr = ED_space_clip_get_clip_frame_number(sc);

  tracking_object = BKE_tracking_object_get_active(&clip->tracking);

  if (tracking_object == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if ((track = tracking_object->active_track) != nullptr) {
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, framenr);
    float marker_pos_ofs[2];

    add_v2_v2v2(marker_pos_ofs, marker->pos, track->offset);

    BKE_mask_coord_from_movieclip(clip, &sc->user, parmask_pos, marker_pos_ofs);

    sub_parent_name = track->name;
    parent_type = MASK_PARENT_POINT_TRACK;
    memset(orig_corners, 0, sizeof(orig_corners));
  }
  else if ((plane_track = tracking_object->active_plane_track) != nullptr) {
    MovieTrackingPlaneMarker *plane_marker = BKE_tracking_plane_marker_get(plane_track, framenr);

    zero_v2(parmask_pos);
    sub_parent_name = plane_track->name;
    parent_type = MASK_PARENT_PLANE_TRACK;
    memcpy(orig_corners, plane_marker->corners, sizeof(orig_corners));
  }
  else {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (MaskLayer *, mask_layer, &mask->masklayers) {
    if (mask_layer->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    LISTBASE_FOREACH (MaskSpline *, spline, &mask_layer->splines) {
      for (int i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          point->parent.id_type = ID_MC;
          point->parent.id = &clip->id;
          point->parent.type = parent_type;
          STRNCPY(point->parent.parent, tracking_object->name);
          STRNCPY(point->parent.sub_parent, sub_parent_name);

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
