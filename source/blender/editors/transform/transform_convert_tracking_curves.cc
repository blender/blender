/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_movieclip.h"
#include "BKE_node_tree_update.hh"
#include "BKE_tracking.h"
#include "BLI_math_matrix.h"

#include "ED_clip.hh"

#include "WM_api.hh"

#include "transform.hh"
#include "transform_convert.hh"

struct TransDataTrackingCurves {
  int flag;

  /* marker transformation from curves editor */
  float *prev_pos;
  float scale;
  short coord;

  MovieTrackingTrack *track;
};

/* -------------------------------------------------------------------- */
/** \name Clip Editor Motion Tracking Transform Creation
 * \{ */

static void markerToTransCurveDataInit(TransData *td,
                                       TransData2D *td2d,
                                       TransDataTrackingCurves *tdt,
                                       MovieTrackingTrack *track,
                                       MovieTrackingMarker *marker,
                                       MovieTrackingMarker *prev_marker,
                                       short coord,
                                       float size)
{
  float frames_delta = (marker->framenr - prev_marker->framenr);

  tdt->flag = marker->flag;
  marker->flag &= ~MARKER_TRACKED;

  tdt->coord = coord;
  tdt->scale = 1.0f / size * frames_delta;
  tdt->prev_pos = prev_marker->pos;
  tdt->track = track;

  /* calculate values depending on marker's speed */
  td2d->loc[0] = marker->framenr;
  td2d->loc[1] = (marker->pos[coord] - prev_marker->pos[coord]) * size / frames_delta;
  td2d->loc[2] = 0.0f;

  td2d->loc2d = marker->pos; /* current location */

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v3_v3(td->center, td->loc);
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = nullptr;
  td->val = nullptr;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

static void createTransTrackingCurvesData(bContext *C, TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  const MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(&clip->tracking);
  TransDataTrackingCurves *tdt;

  int width, height;
  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* count */
  tc->data_len = 0;

  if ((sc->flag & SC_SHOW_GRAPH_TRACKS_MOTION) == 0) {
    return;
  }

  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      for (int i = 1; i < track->markersnr; i++) {
        const MovieTrackingMarker *marker = &track->markers[i];
        const MovieTrackingMarker *prev_marker = &track->markers[i - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & MARKER_GRAPH_SEL_X) {
          tc->data_len += 1;
        }

        if (marker->flag & MARKER_GRAPH_SEL_Y) {
          tc->data_len += 1;
        }
      }
    }
  }

  if (tc->data_len == 0) {
    return;
  }

  td = tc->data = static_cast<TransData *>(
      MEM_callocN(tc->data_len * sizeof(TransData), "TransTracking TransData"));
  td2d = tc->data_2d = static_cast<TransData2D *>(
      MEM_callocN(tc->data_len * sizeof(TransData2D), "TransTracking TransData2D"));
  tc->custom.type.data = tdt = static_cast<TransDataTrackingCurves *>(MEM_callocN(
      tc->data_len * sizeof(TransDataTrackingCurves), "TransTracking TransDataTracking"));
  tc->custom.type.free_cb = nullptr;

  /* create actual data */
  LISTBASE_FOREACH (MovieTrackingTrack *, track, &tracking_object->tracks) {
    if (TRACK_VIEW_SELECTED(sc, track) && (track->flag & TRACK_LOCKED) == 0) {
      for (int i = 1; i < track->markersnr; i++) {
        MovieTrackingMarker *marker = &track->markers[i];
        MovieTrackingMarker *prev_marker = &track->markers[i - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & MARKER_GRAPH_SEL_X) {
          markerToTransCurveDataInit(
              td, td2d, tdt, track, marker, &track->markers[i - 1], 0, width);
          td += 1;
          td2d += 1;
          tdt += 1;
        }

        if (marker->flag & MARKER_GRAPH_SEL_Y) {
          markerToTransCurveDataInit(
              td, td2d, tdt, track, marker, &track->markers[i - 1], 1, height);

          td += 1;
          td2d += 1;
          tdt += 1;
        }
      }
    }
  }
}

static void createTransTrackingCurves(bContext *C, TransInfo *t)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int width, height;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!clip) {
    return;
  }

  BKE_movieclip_get_size(clip, &sc->user, &width, &height);

  if (width == 0 || height == 0) {
    return;
  }

  /* transformation was called from graph editor */
  BLI_assert(CTX_wm_region(C)->regiontype == RGN_TYPE_PREVIEW);
  createTransTrackingCurvesData(C, t);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name recalc Motion Tracking TransData
 * \{ */

static void cancelTransTrackingCurves(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransDataTrackingCurves *tdt_array = static_cast<TransDataTrackingCurves *>(
      tc->custom.type.data);

  int i = 0;
  while (i < tc->data_len) {
    TransDataTrackingCurves *tdt = &tdt_array[i];

    {
      MovieTrackingTrack *track = tdt->track;
      MovieTrackingMarker *marker, *prev_marker;
      int a;

      for (a = 1; a < track->markersnr; a++) {
        marker = &track->markers[a];
        prev_marker = &track->markers[a - 1];

        if ((marker->flag & MARKER_DISABLED) || (prev_marker->flag & MARKER_DISABLED)) {
          continue;
        }

        if (marker->flag & (MARKER_GRAPH_SEL_X | MARKER_GRAPH_SEL_Y)) {
          marker->flag = tdt->flag;
        }
      }
    }

    i++;
  }
}

static void flushTransTrackingCurves(TransInfo *t)
{
  TransData *td;
  TransData2D *td2d;
  TransDataTrackingCurves *tdt;
  int td_index;

  if (t->state == TRANS_CANCEL) {
    cancelTransTrackingCurves(t);
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* flush to 2d vector from internally used 3d vector */
  for (td_index = 0,
      td = tc->data,
      td2d = tc->data_2d,
      tdt = static_cast<TransDataTrackingCurves *>(tc->custom.type.data);
       td_index < tc->data_len;
       td_index++, td2d++, td++, tdt++)
  {
    {
      td2d->loc2d[tdt->coord] = tdt->prev_pos[tdt->coord] + td2d->loc[1] * tdt->scale;
    }
  }
}

static void recalcData_tracking_curves(TransInfo *t)
{
  SpaceClip *sc = static_cast<SpaceClip *>(t->area->spacedata.first);

  if (ED_space_clip_check_show_trackedit(sc)) {
    MovieClip *clip = ED_space_clip_get_clip(sc);

    flushTransTrackingCurves(t);

    DEG_id_tag_update(&clip->id, 0);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Tracking
 * \{ */

static void special_aftertrans_update__movieclip_for_curves(bContext *C, TransInfo *t)
{
  SpaceClip *sc = static_cast<SpaceClip *>(t->area->spacedata.first);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  if (t->scene->nodetree != nullptr) {
    /* Tracks can be used for stabilization nodes,
     * flush update for such nodes.
     */
    if (t->context != nullptr) {
      Main *bmain = CTX_data_main(C);
      BKE_ntree_update_tag_id_changed(bmain, &clip->id);
      BKE_ntree_update_main(bmain, nullptr);
      WM_event_add_notifier(C, NC_SCENE | ND_NODES, nullptr);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_TrackingCurves = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransTrackingCurves,
    /*recalc_data*/ recalcData_tracking_curves,
    /*special_aftertrans_update*/ special_aftertrans_update__movieclip_for_curves,
};
