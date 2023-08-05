/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include "DNA_gpencil_legacy_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_report.h"
#include "BKE_tracking.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_clip.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "clip_intern.h"
#include "tracking_ops_intern.h"

/********************** detect features operator *********************/

static bGPDlayer *detect_get_layer(MovieClip *clip)
{
  if (clip->gpd == nullptr) {
    return nullptr;
  }

  LISTBASE_FOREACH (bGPDlayer *, layer, &clip->gpd->layers) {
    if (layer->flag & GP_LAYER_ACTIVE) {
      return layer;
    }
  }
  return nullptr;
}

static int detect_features_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;
  ImBuf *ibuf = BKE_movieclip_get_ibuf_flag(clip, &sc->user, clip_flag, MOVIECLIP_CACHE_SKIP);
  MovieTracking *tracking = &clip->tracking;
  MovieTrackingObject *tracking_object = BKE_tracking_object_get_active(tracking);
  const int placement = RNA_enum_get(op->ptr, "placement");
  const int margin = RNA_int_get(op->ptr, "margin");
  const int min_distance = RNA_int_get(op->ptr, "min_distance");
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const int framenr = ED_space_clip_get_clip_frame_number(sc);
  bGPDlayer *layer = nullptr;
  int place_outside_layer = 0;

  if (!ibuf) {
    BKE_report(op->reports, RPT_ERROR, "Feature detection requires valid clip frame");
    return OPERATOR_CANCELLED;
  }

  if (placement != 0) {
    layer = detect_get_layer(clip);
    place_outside_layer = placement == 2;
  }

  /* Deselect existing tracks. */
  ed_tracking_deselect_all_tracks(&tracking_object->tracks);

  /* Run detector. */
  BKE_tracking_detect_harris(tracking,
                             &tracking_object->tracks,
                             ibuf,
                             framenr,
                             margin,
                             threshold / 100000.0f,
                             min_distance,
                             layer,
                             place_outside_layer);

  IMB_freeImBuf(ibuf);

  BKE_tracking_dopesheet_tag_update(tracking);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void CLIP_OT_detect_features(wmOperatorType *ot)
{
  static const EnumPropertyItem placement_items[] = {
      {0, "FRAME", 0, "Whole Frame", "Place markers across the whole frame"},
      {1,
       "INSIDE_GPENCIL",
       0,
       "Inside Annotated Area",
       "Place markers only inside areas outlined with the Annotation tool"},
      {2,
       "OUTSIDE_GPENCIL",
       0,
       "Outside Annotated Area",
       "Place markers only outside areas outlined with the Annotation tool"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Detect Features";
  ot->description = "Automatically detect features and place markers to track";
  ot->idname = "CLIP_OT_detect_features";

  /* api callbacks */
  ot->exec = detect_features_exec;
  ot->poll = ED_space_clip_tracking_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(
      ot->srna, "placement", placement_items, 0, "Placement", "Placement for detected features");
  RNA_def_int(ot->srna,
              "margin",
              16,
              0,
              INT_MAX,
              "Margin",
              "Only features further than margin pixels from the image "
              "edges are considered",
              0,
              300);
  RNA_def_float(ot->srna,
                "threshold",
                0.5f,
                0.0001f,
                FLT_MAX,
                "Threshold",
                "Threshold level to consider feature good enough for tracking",
                0.0001f,
                FLT_MAX);
  RNA_def_int(ot->srna,
              "min_distance",
              120,
              0,
              INT_MAX,
              "Distance",
              "Minimal distance accepted between two features",
              0,
              300);
}
