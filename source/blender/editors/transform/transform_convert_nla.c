/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include "DNA_anim_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_nla.h"

#include "ED_anim_api.h"
#include "ED_markers.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "transform.h"
#include "transform_snap.h"

#include "transform_convert.h"
#include "transform_snap.h"

#include "transform_mode.h"

/** Used for NLA transform (stored in #TransData.extra pointer). */
typedef struct TransDataNla {
  /** ID-block NLA-data is attached to. */
  ID *id;

  /** Original NLA-Track that the strip belongs to. */
  struct NlaTrack *oldTrack;
  /** Current NLA-Track that the strip belongs to. */
  struct NlaTrack *nlt;

  /** NLA-strip this data represents. */
  struct NlaStrip *strip;

  /* dummy values for transform to write in - must have 3 elements... */
  /** start handle. */
  float h1[3];
  /** end handle. */
  float h2[3];

  /** index of track that strip is currently in. */
  int trackIndex;
  /** handle-index: 0 for dummy entry, -1 for start, 1 for end, 2 for both ends. */
  int handle;
} TransDataNla;

/* -------------------------------------------------------------------- */
/** \name NLA Transform Creation
 * \{ */

void createTransNlaData(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  SpaceNla *snla = NULL;
  TransData *td = NULL;
  TransDataNla *tdn = NULL;

  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  int count = 0;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* determine what type of data we are operating on */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }
  snla = (SpaceNla *)ac.sl;

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  if (t->mode == TFM_TIME_EXTEND) {
    t->frame_side = transform_convert_frame_side_dir_get(t, (float)CFRA);
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

  /* loop 1: count how many strips are selected (consider each strip as 2 points) */
  for (ale = anim_data.first; ale; ale = ale->next) {
    NlaTrack *nlt = (NlaTrack *)ale->data;
    NlaStrip *strip;

    /* make some meta-strips for chains of selected strips */
    BKE_nlastrips_make_metas(&nlt->strips, 1);

    /* only consider selected strips */
    for (strip = nlt->strips.first; strip; strip = strip->next) {
      /* TODO: we can make strips have handles later on. */
      /* transition strips can't get directly transformed */
      if (strip->type != NLASTRIP_TYPE_TRANSITION) {
        if (strip->flag & NLASTRIP_FLAG_SELECT) {
          if (FrameOnMouseSide(t->frame_side, strip->start, (float)CFRA)) {
            count++;
          }
          if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA)) {
            count++;
          }
        }
      }
    }
  }

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    /* clear temp metas that may have been created but aren't needed now
     * because they fell on the wrong side of CFRA
     */
    for (ale = anim_data.first; ale; ale = ale->next) {
      NlaTrack *nlt = (NlaTrack *)ale->data;
      BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
    }

    /* cleanup temp list */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  /* allocate memory for data */
  tc->data_len = count;

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(NLA Editor)");
  td = tc->data;
  tc->custom.type.data = tdn = MEM_callocN(tc->data_len * sizeof(TransDataNla),
                                           "TransDataNla (NLA Editor)");
  tc->custom.type.use_free = true;

  /* loop 2: build transdata array */
  for (ale = anim_data.first; ale; ale = ale->next) {
    /* only if a real NLA-track */
    if (ale->type == ANIMTYPE_NLATRACK) {
      AnimData *adt = ale->adt;
      NlaTrack *nlt = (NlaTrack *)ale->data;
      NlaStrip *strip;

      /* only consider selected strips */
      for (strip = nlt->strips.first; strip; strip = strip->next) {
        /* TODO: we can make strips have handles later on. */
        /* transition strips can't get directly transformed */
        if (strip->type != NLASTRIP_TYPE_TRANSITION) {
          if (strip->flag & NLASTRIP_FLAG_SELECT) {
            /* our transform data is constructed as follows:
             * - only the handles on the right side of the current-frame get included
             * - td structs are transform-elements operated on by the transform system
             *   and represent a single handle. The storage/pointer used (val or loc) depends on
             *   whether we're scaling or transforming. Ultimately though, the handles
             *   the td writes to will simply be a dummy in tdn
             * - for each strip being transformed, a single tdn struct is used, so in some
             *   cases, there will need to be 1 of these tdn elements in the array skipped...
             */
            float center[3], yval;

            /* firstly, init tdn settings */
            tdn->id = ale->id;
            tdn->oldTrack = tdn->nlt = nlt;
            tdn->strip = strip;
            tdn->trackIndex = BLI_findindex(&adt->nla_tracks, nlt);

            yval = (float)(tdn->trackIndex * NLACHANNEL_STEP(snla));

            tdn->h1[0] = strip->start;
            tdn->h1[1] = yval;
            tdn->h2[0] = strip->end;
            tdn->h2[1] = yval;

            center[0] = (float)CFRA;
            center[1] = yval;
            center[2] = 0.0f;

            /* set td's based on which handles are applicable */
            if (FrameOnMouseSide(t->frame_side, strip->start, (float)CFRA)) {
              /* just set tdn to assume that it only has one handle for now */
              tdn->handle = -1;

              /* Now, link the transform data up to this data. */
              td->loc = tdn->h1;
              copy_v3_v3(td->iloc, tdn->h1);

              if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
                /* Store all the other gunk that is required by transform. */
                copy_v3_v3(td->center, center);
                td->axismtx[2][2] = 1.0f;
                td->flag |= TD_SELECTED;
                unit_m3(td->mtx);
                unit_m3(td->smtx);
              }

              td->extra = tdn;
              td++;
            }
            if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA)) {
              /* if tdn is already holding the start handle,
               * then we're doing both, otherwise, only end */
              tdn->handle = (tdn->handle) ? 2 : 1;

              /* Now, link the transform data up to this data. */
              td->loc = tdn->h2;
              copy_v3_v3(td->iloc, tdn->h2);

              if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
                /* Store all the other gunk that is required by transform. */
                copy_v3_v3(td->center, center);
                td->axismtx[2][2] = 1.0f;
                td->flag |= TD_SELECTED;
                unit_m3(td->mtx);
                unit_m3(td->smtx);
              }

              td->extra = tdn;
              td++;
            }

            /* If both handles were used, skip the next tdn (i.e. leave it blank)
             * since the counting code is dumb.
             * Otherwise, just advance to the next one.
             */
            if (tdn->handle == 2) {
              tdn += 2;
            }
            else {
              tdn++;
            }
          }
        }
      }
    }
  }

  /* cleanup temp list */
  ANIM_animdata_freelist(&anim_data);
}

void recalcData_nla(TransInfo *t)
{
  SpaceNla *snla = (SpaceNla *)t->area->spacedata.first;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* handle auto-snapping
   * NOTE: only do this when transform is still running, or we can't restore
   */
  if (t->state != TRANS_CANCEL) {
    const short autosnap = getAnimEdit_SnapMode(t);
    if (autosnap != SACTSNAP_OFF) {
      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
        transform_snap_anim_flush_data(t, td, autosnap, td->loc);
      }
    }
  }

  /* For each strip we've got, perform some additional validation of the values
   * that got set before using RNA to set the value (which does some special
   * operations when setting these values to make sure that everything works ok).
   */
  TransDataNla *tdn = tc->custom.type.data;
  for (int i = 0; i < tc->data_len; i++, tdn++) {
    NlaStrip *strip = tdn->strip;
    PointerRNA strip_ptr;
    int delta_y1, delta_y2;

    /* if this tdn has no handles, that means it is just a dummy that should be skipped */
    if (tdn->handle == 0) {
      continue;
    }

    /* set refresh tags for objects using this animation,
     * BUT only if realtime updates are enabled
     */
    if ((snla->flag & SNLA_NOREALTIMEUPDATES) == 0) {
      ANIM_id_update(CTX_data_main(t->context), tdn->id);
    }

    /* if canceling transform, just write the values without validating, then move on */
    if (t->state == TRANS_CANCEL) {
      /* clear the values by directly overwriting the originals, but also need to restore
       * endpoints of neighboring transition-strips
       */

      /* start */
      strip->start = tdn->h1[0];

      if ((strip->prev) && (strip->prev->type == NLASTRIP_TYPE_TRANSITION)) {
        strip->prev->end = tdn->h1[0];
      }

      /* end */
      strip->end = tdn->h2[0];

      if ((strip->next) && (strip->next->type == NLASTRIP_TYPE_TRANSITION)) {
        strip->next->start = tdn->h2[0];
      }

      /* flush transforms to child strips (since this should be a meta) */
      BKE_nlameta_flush_transforms(strip);

      /* restore to original track (if needed) */
      if (tdn->oldTrack != tdn->nlt) {
        /* Just append to end of list for now,
         * since strips get sorted in special_aftertrans_update(). */
        BLI_remlink(&tdn->nlt->strips, strip);
        BLI_addtail(&tdn->oldTrack->strips, strip);
      }

      continue;
    }

    /* firstly, check if the proposed transform locations would overlap with any neighboring strips
     * (barring transitions) which are absolute barriers since they are not being moved
     *
     * this is done as a iterative procedure (done 5 times max for now)
     */
    NlaStrip *prev = strip->prev;
    while (prev != NULL && (prev->type & NLASTRIP_TYPE_TRANSITION)) {
      prev = prev->prev;
    }

    NlaStrip *next = strip->next;
    while (next != NULL && (next->type & NLASTRIP_TYPE_TRANSITION)) {
      next = next->next;
    }

    for (short iter = 0; iter < 5; iter++) {
      const bool pExceeded = (prev != NULL) && (tdn->h1[0] < prev->end);
      const bool nExceeded = (next != NULL) && (tdn->h2[0] > next->start);

      if ((pExceeded && nExceeded) || (iter == 4)) {
        /* both endpoints exceeded (or iteration ping-pong'd meaning that we need a
         * compromise)
         * - Simply crop strip to fit within the bounds of the strips bounding it
         * - If there were no neighbors, clear the transforms
         *   (make it default to the strip's current values).
         */
        if (prev && next) {
          tdn->h1[0] = prev->end;
          tdn->h2[0] = next->start;
        }
        else {
          tdn->h1[0] = strip->start;
          tdn->h2[0] = strip->end;
        }
      }
      else if (nExceeded) {
        /* move backwards */
        float offset = tdn->h2[0] - next->start;

        tdn->h1[0] -= offset;
        tdn->h2[0] -= offset;
      }
      else if (pExceeded) {
        /* more forwards */
        float offset = prev->end - tdn->h1[0];

        tdn->h1[0] += offset;
        tdn->h2[0] += offset;
      }
      else { /* all is fine and well */
        break;
      }
    }

    /* Use RNA to write the values to ensure that constraints on these are obeyed
     * (e.g. for transition strips, the values are taken from the neighbors)
     *
     * NOTE: we write these twice to avoid truncation errors which can arise when
     * moving the strips a large distance using numeric input T33852.
     */
    RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);

    RNA_float_set(&strip_ptr, "frame_start", tdn->h1[0]);
    RNA_float_set(&strip_ptr, "frame_end", tdn->h2[0]);

    RNA_float_set(&strip_ptr, "frame_start", tdn->h1[0]);
    RNA_float_set(&strip_ptr, "frame_end", tdn->h2[0]);

    /* flush transforms to child strips (since this should be a meta) */
    BKE_nlameta_flush_transforms(strip);

    /* Now, check if we need to try and move track:
     * - we need to calculate both,
     *   as only one may have been altered by transform if only 1 handle moved.
     */
    /* In LibOverride case, we cannot move strips across tracks that come from the linked data. */
    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(tdn->id);
    if (BKE_nlatrack_is_nonlocal_in_liboverride(tdn->id, tdn->nlt)) {
      continue;
    }

    delta_y1 = ((int)tdn->h1[1] / NLACHANNEL_STEP(snla) - tdn->trackIndex);
    delta_y2 = ((int)tdn->h2[1] / NLACHANNEL_STEP(snla) - tdn->trackIndex);

    if (delta_y1 || delta_y2) {
      NlaTrack *track;
      int delta = (delta_y2) ? delta_y2 : delta_y1;
      int n;

      /* Move in the requested direction,
       * checking at each layer if there's space for strip to pass through,
       * stopping on the last track available or that we're able to fit in.
       */
      if (delta > 0) {
        for (track = tdn->nlt->next, n = 0; (track) && (n < delta); track = track->next, n++) {
          /* check if space in this track for the strip */
          if (BKE_nlatrack_has_space(track, strip->start, strip->end) &&
              !BKE_nlatrack_is_nonlocal_in_liboverride(tdn->id, track)) {
            /* move strip to this track */
            BLI_remlink(&tdn->nlt->strips, strip);
            BKE_nlatrack_add_strip(track, strip, is_liboverride);

            tdn->nlt = track;
            tdn->trackIndex++;
          }
          else { /* can't move any further */
            break;
          }
        }
      }
      else {
        /* make delta 'positive' before using it, since we now know to go backwards */
        delta = -delta;

        for (track = tdn->nlt->prev, n = 0; (track) && (n < delta); track = track->prev, n++) {
          /* check if space in this track for the strip */
          if (BKE_nlatrack_has_space(track, strip->start, strip->end) &&
              !BKE_nlatrack_is_nonlocal_in_liboverride(tdn->id, track)) {
            /* move strip to this track */
            BLI_remlink(&tdn->nlt->strips, strip);
            BKE_nlatrack_add_strip(track, strip, is_liboverride);

            tdn->nlt = track;
            tdn->trackIndex--;
          }
          else { /* can't move any further */
            break;
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform NLA
 * \{ */

void special_aftertrans_update__nla(bContext *C, TransInfo *UNUSED(t))
{
  bAnimContext ac;

  /* initialize relevant anim-context 'context' data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  if (ac.datatype) {
    ListBase anim_data = {NULL, NULL};
    bAnimListElem *ale;
    short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT);

    /* get channels to work on */
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    for (ale = anim_data.first; ale; ale = ale->next) {
      NlaTrack *nlt = (NlaTrack *)ale->data;

      /* make sure strips are in order again */
      BKE_nlatrack_sort_strips(nlt);

      /* remove the temp metas */
      BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
    }

    /* General refresh for the outliner because the following might have happened:
     * - strips moved between tracks
     * - strips swapped order
     * - duplicate-move moves to different track. */
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, NULL);

    /* free temp memory */
    ANIM_animdata_freelist(&anim_data);

    /* Perform after-transform validation. */
    ED_nla_postop_refresh(&ac);
  }
}

/** \} */
