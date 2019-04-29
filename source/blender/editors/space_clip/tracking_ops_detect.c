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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_movieclip.h"
#include "BKE_tracking.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_clip.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "clip_intern.h"
#include "tracking_ops_intern.h"

/********************** detect features operator *********************/

static bGPDlayer *detect_get_layer(MovieClip *clip)
{
  if (clip->gpd == NULL) {
    return NULL;
  }
  for (bGPDlayer *layer = clip->gpd->layers.first; layer != NULL; layer = layer->next) {
    if (layer->flag & GP_LAYER_ACTIVE) {
      return layer;
    }
  }
  return NULL;
}

static int detect_features_exec(bContext *C, wmOperator *op)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  MovieClip *clip = ED_space_clip_get_clip(sc);
  int clip_flag = clip->flag & MCLIP_TIMECODE_FLAGS;
  ImBuf *ibuf = BKE_movieclip_get_ibuf_flag(clip, &sc->user, clip_flag, MOVIECLIP_CACHE_SKIP);
  MovieTracking *tracking = &clip->tracking;
  ListBase *tracksbase = BKE_tracking_get_active_tracks(tracking);
  int placement = RNA_enum_get(op->ptr, "placement");
  int margin = RNA_int_get(op->ptr, "margin");
  int min_distance = RNA_int_get(op->ptr, "min_distance");
  float threshold = RNA_float_get(op->ptr, "threshold");
  int place_outside_layer = 0;
  int framenr = ED_space_clip_get_clip_frame_number(sc);
  bGPDlayer *layer = NULL;

  if (!ibuf) {
    BKE_report(op->reports, RPT_ERROR, "Feature detection requires valid clip frame");
    return OPERATOR_CANCELLED;
  }

  if (placement != 0) {
    layer = detect_get_layer(clip);
    place_outside_layer = placement == 2;
  }

  /* Deselect existing tracks. */
  ed_tracking_deselect_all_tracks(tracksbase);
  /* Run detector. */
  BKE_tracking_detect_harris(tracking,
                             tracksbase,
                             ibuf,
                             framenr,
                             margin,
                             threshold / 100000.0f,
                             min_distance,
                             layer,
                             place_outside_layer);

  IMB_freeImBuf(ibuf);

  BKE_tracking_dopesheet_tag_update(tracking);
  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

void CLIP_OT_detect_features(wmOperatorType *ot)
{
  static const EnumPropertyItem placement_items[] = {
      {0, "FRAME", 0, "Whole Frame", "Place markers across the whole frame"},
      {1,
       "INSIDE_GPENCIL",
       0,
       "Inside Grease Pencil",
       "Place markers only inside areas outlined with Grease Pencil"},
      {2,
       "OUTSIDE_GPENCIL",
       0,
       "Outside Grease Pencil",
       "Place markers only outside areas outlined with Grease Pencil"},
      {0, NULL, 0, NULL, NULL},
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
