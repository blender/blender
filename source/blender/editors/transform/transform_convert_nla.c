/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include <stdio.h>

#include "DNA_anim_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_anim_data.h"
#include "BKE_context.h"
#include "BKE_nla.h"

#include "ED_anim_api.h"
#include "ED_markers.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_mode.h"
#include "transform_snap.h"

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

static bool is_overlap(const float left_bound_a,
                       const float right_bound_a,
                       const float left_bound_b,
                       const float right_bound_b)
{
  return (left_bound_a < right_bound_b) && (right_bound_a > left_bound_b);
}

static bool nlastrip_is_overlap(const NlaStrip *strip_a,
                                const float offset_a,
                                const NlaStrip *strip_b,
                                const float offset_b)
{
  return is_overlap(strip_a->start + offset_a,
                    strip_a->end + offset_a,
                    strip_b->start + offset_b,
                    strip_b->end + offset_b);
}

/**
 * Assumes strips to horizontally translate (shuffle) are tagged with
 * #NLASTRIP_FLAG_INVALID_LOCATION.
 *
 * \returns The total sided offset that results in no overlaps between tagged strips and non-tagged
 * strips.
 */
static float transdata_get_time_shuffle_offset_side(ListBase *trans_datas, const bool shuffle_left)
{
  float total_offset = 0;

  float offset;
  do {
    offset = 0;

    LISTBASE_FOREACH (LinkData *, link, trans_datas) {
      TransDataNla *trans_data = (TransDataNla *)link->data;
      NlaStrip *xformed_strip = trans_data->strip;

      LISTBASE_FOREACH (NlaStrip *, non_xformed_strip, &trans_data->nlt->strips) {

        if (non_xformed_strip->flag & NLASTRIP_FLAG_INVALID_LOCATION) {
          continue;
        }

        /* Allow overlap with transitions. */
        if (non_xformed_strip->type & NLASTRIP_TYPE_TRANSITION) {
          continue;
        }

        if (!nlastrip_is_overlap(non_xformed_strip, 0, xformed_strip, total_offset)) {
          continue;
        }

        offset = shuffle_left ?
                     fmin(offset, non_xformed_strip->start - (xformed_strip->end + total_offset)) :
                     fmax(offset, non_xformed_strip->end - (xformed_strip->start + total_offset));
      }
    }

    total_offset += offset;
  } while (!IS_EQF(offset, 0.0f));

  return total_offset;
}

/**
 * Assumes strips to horizontally translate (shuffle) are tagged with
 * #NLASTRIP_FLAG_INVALID_LOCATION.
 *
 * \returns The minimal total signed offset that results in no overlaps between tagged strips and
 * non-tagged strips.
 */
static float transdata_get_time_shuffle_offset(ListBase *trans_datas)
{
  const float offset_left = transdata_get_time_shuffle_offset_side(trans_datas, true);
  const float offset_right = transdata_get_time_shuffle_offset_side(trans_datas, false);
  BLI_assert(offset_left <= 0);
  BLI_assert(offset_right >= 0);

  return -offset_left < offset_right ? offset_left : offset_right;
}

/* -------------------------------------------------------------------- */
/** \name Transform application to NLA strips
 * \{ */

/**
 * \brief Applies a translation to the given #NlaStrip.
 * \param strip_rna_ptr: The RNA pointer of the NLA strip to modify.
 * \param transdata: The transformation info structure.
 */
static void applyTransformNLA_translation(PointerRNA *strip_rna_ptr, const TransDataNla *transdata)
{
  /* NOTE: we write these twice to avoid truncation errors which can arise when
   * moving the strips a large distance using numeric input #33852.
   */
  RNA_float_set(strip_rna_ptr, "frame_start", transdata->h1[0]);
  RNA_float_set(strip_rna_ptr, "frame_end", transdata->h2[0]);

  RNA_float_set(strip_rna_ptr, "frame_start", transdata->h1[0]);
  RNA_float_set(strip_rna_ptr, "frame_end", transdata->h2[0]);
}

static void applyTransformNLA_timeScale(PointerRNA *strip_rna_ptr, const float value)
{
  RNA_float_set(strip_rna_ptr, "scale", value);
}

/** Reorder strips for proper nla stack evaluation while dragging. */
static void nlastrip_overlap_reorder(TransDataNla *tdn, NlaStrip *strip)
{
  while (strip->prev != NULL && tdn->h1[0] < strip->prev->start) {
    BLI_listbase_swaplinks(&tdn->nlt->strips, strip, strip->prev);
  }
  while (strip->next != NULL && tdn->h1[0] > strip->next->start) {
    BLI_listbase_swaplinks(&tdn->nlt->strips, strip, strip->next);
  }
}

/** Flag overlaps with adjacent strips.
 *
 * Since the strips are re-ordered as they're transformed, we only have to check adjacent
 * strips for overlap instead of all of them. */
static void nlastrip_flag_overlaps(NlaStrip *strip)
{

  NlaStrip *adj_strip = strip->prev;
  if (adj_strip != NULL && !(adj_strip->flag & NLASTRIP_FLAG_SELECT) &&
      nlastrip_is_overlap(strip, 0, adj_strip, 0))
  {
    strip->flag |= NLASTRIP_FLAG_INVALID_LOCATION;
  }
  adj_strip = strip->next;
  if (adj_strip != NULL && !(adj_strip->flag & NLASTRIP_FLAG_SELECT) &&
      nlastrip_is_overlap(strip, 0, adj_strip, 0))
  {
    strip->flag |= NLASTRIP_FLAG_INVALID_LOCATION;
  }
}

/**
 * Check the Transformation data for the given Strip, and fix any overlap. Then
 * apply the Transformation.
 */
static void nlastrip_fix_overlapping(TransInfo *t, TransDataNla *tdn, NlaStrip *strip)
{
  /* firstly, check if the proposed transform locations would overlap with any neighboring
   * strips (barring transitions) which are absolute barriers since they are not being moved
   *
   * this is done as a iterative procedure (done 5 times max for now). */
  short iter_max = 4;
  NlaStrip *prev = BKE_nlastrip_prev_in_track(strip, true);
  NlaStrip *next = BKE_nlastrip_next_in_track(strip, true);

  PointerRNA strip_ptr;

  for (short iter = 0; iter <= iter_max; iter++) {
    const bool p_exceeded = (prev != NULL) && (tdn->h1[0] < prev->end);
    const bool n_exceeded = (next != NULL) && (tdn->h2[0] > next->start);

    if ((p_exceeded && n_exceeded) || (iter == iter_max)) {
      /* Both endpoints exceeded (or iteration ping-pong'd meaning that we need a compromise).
       * - Simply crop strip to fit within the bounds of the strips bounding it.
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
    else if (n_exceeded) {
      /* move backwards */
      float offset = tdn->h2[0] - next->start;

      tdn->h1[0] -= offset;
      tdn->h2[0] -= offset;
    }
    else if (p_exceeded) {
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
   * (e.g. for transition strips, the values are taken from the neighbors). */
  RNA_pointer_create(NULL, &RNA_NlaStrip, strip, &strip_ptr);

  switch (t->mode) {
    case TFM_TIME_EXTEND:
    case TFM_TIME_SCALE: {
      /* The final scale is the product of the original strip scale (from before the
       * transform operation started) and the current scale value of this transform operation. */
      const float originalStripScale = tdn->h1[2];
      const float newStripScale = originalStripScale * t->values_final[0];
      applyTransformNLA_timeScale(&strip_ptr, newStripScale);
      applyTransformNLA_translation(&strip_ptr, tdn);
      break;
    }
    case TFM_TRANSLATION:
      applyTransformNLA_translation(&strip_ptr, tdn);
      break;
    default:
      printf("recalcData_nla: unsupported NLA transformation mode %d\n", t->mode);
      break;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NLA Transform Creation
 * \{ */

static void createTransNlaData(bContext *C, TransInfo *t)
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
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_FOREDIT |
            ANIMFILTER_FCURVESONLY);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  if (t->mode == TFM_TIME_EXTEND) {
    t->frame_side = transform_convert_frame_side_dir_get(t, (float)scene->r.cfra);
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
          if (FrameOnMouseSide(t->frame_side, strip->start, (float)scene->r.cfra)) {
            count++;
          }
          if (FrameOnMouseSide(t->frame_side, strip->end, (float)scene->r.cfra)) {
            count++;
          }
        }
      }
    }
  }

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    /* clear temp metas that may have been created but aren't needed now
     * because they fell on the wrong side of scene->r.cfra
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
            /* Our transform data is constructed as follows:
             * - Only the handles on the right side of the current-frame get included.
             * - `td` structs are transform-elements operated on by the transform system and
             *   represent a single handle. The storage/pointer used (`val` or `loc`) depends
             *   on whether we're scaling or transforming. Ultimately though, the handles the `td`
             *   writes to will simply be a dummy in `tdn`.
             * - For each strip being transformed, a single `tdn` struct is used, so in some
             *   cases, there will need to be 1 of these `tdn` elements in the array skipped.
             */
            float center[3], yval;

            /* Firstly, initialize `tdn` settings. */
            tdn->id = ale->id;
            tdn->oldTrack = tdn->nlt = nlt;
            tdn->strip = strip;
            tdn->trackIndex = BLI_findindex(&adt->nla_tracks, nlt);

            yval = (float)(tdn->trackIndex * NLACHANNEL_STEP(snla));

            tdn->h1[0] = strip->start;
            tdn->h1[1] = yval;
            tdn->h2[0] = strip->end;
            tdn->h2[1] = yval;
            tdn->h1[2] = tdn->h2[2] = strip->scale;

            center[0] = (float)scene->r.cfra;
            center[1] = yval;
            center[2] = 0.0f;

            /* set td's based on which handles are applicable */
            if (FrameOnMouseSide(t->frame_side, strip->start, (float)scene->r.cfra)) {
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
            if (FrameOnMouseSide(t->frame_side, strip->end, (float)scene->r.cfra)) {
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

static void recalcData_nla(TransInfo *t)
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
    int delta_y1, delta_y2;

    /* if this tdn has no handles, that means it is just a dummy that should be skipped */
    if (tdn->handle == 0) {
      continue;
    }
    strip->flag &= ~NLASTRIP_FLAG_INVALID_LOCATION;

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

      strip->scale = tdn->h1[2];

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

    const bool nlatrack_isliboverride = BKE_nlatrack_is_nonlocal_in_liboverride(tdn->id, tdn->nlt);
    const bool allow_overlap = !nlatrack_isliboverride && ELEM(t->mode, TFM_TRANSLATION);

    if (allow_overlap) {
      nlastrip_overlap_reorder(tdn, strip);

      /* Directly flush. */
      strip->start = tdn->h1[0];
      strip->end = tdn->h2[0];
    }
    else {
      nlastrip_fix_overlapping(t, tdn, strip);
    }

    /* flush transforms to child strips (since this should be a meta) */
    BKE_nlameta_flush_transforms(strip);

    /* Now, check if we need to try and move track:
     * - we need to calculate both,
     *   as only one may have been altered by transform if only 1 handle moved.
     */
    /* In LibOverride case, we cannot move strips across tracks that come from the linked data.
     */
    const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(tdn->id);
    if (nlatrack_isliboverride) {
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
              !BKE_nlatrack_is_nonlocal_in_liboverride(tdn->id, track))
          {
            /* move strip to this track */
            BKE_nlatrack_remove_strip(tdn->nlt, strip);
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
              !BKE_nlatrack_is_nonlocal_in_liboverride(tdn->id, track))
          {
            /* move strip to this track */
            BKE_nlatrack_remove_strip(tdn->nlt, strip);
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

    nlastrip_flag_overlaps(strip);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform NLA
 * \{ */

typedef struct IDGroupedTransData {
  struct IDGroupedTransData *next, *prev;

  ID *id;
  ListBase trans_datas;
} IDGroupedTransData;

/** horizontally translate (shuffle) the transformed strip to a non-overlapping state. */
static void nlastrip_shuffle_transformed(TransDataContainer *tc, TransDataNla *first_trans_data)
{
  /* Element: #IDGroupedTransData. */
  ListBase grouped_trans_datas = {NULL, NULL};

  /* Flag all non-library-override transformed strips so we can distinguish them when shuffling.
   *
   * Group trans_datas by ID so shuffling is unique per ID. */
  {
    TransDataNla *tdn = first_trans_data;
    for (int i = 0; i < tc->data_len; i++, tdn++) {

      /* Skip dummy handles. */
      if (tdn->handle == 0) {
        continue;
      }

      /* For strips within library override tracks, don't do any shuffling at all. Unsure how
       * library overrides should behave so, for now, they're treated as mostly immutable. */
      if ((tdn->nlt->flag & NLATRACK_OVERRIDELIBRARY_LOCAL) == 0) {
        continue;
      }

      tdn->strip->flag |= NLASTRIP_FLAG_INVALID_LOCATION;

      IDGroupedTransData *dst_group = NULL;
      /* Find dst_group with matching ID. */
      LISTBASE_FOREACH (IDGroupedTransData *, group, &grouped_trans_datas) {
        if (group->id == tdn->id) {
          dst_group = group;
          break;
        }
      }
      if (dst_group == NULL) {
        dst_group = MEM_callocN(sizeof(IDGroupedTransData), __func__);
        dst_group->id = tdn->id;
        BLI_addhead(&grouped_trans_datas, dst_group);
      }

      BLI_addtail(&dst_group->trans_datas, BLI_genericNodeN(tdn));
    }
  }

  /* Apply shuffling. */
  LISTBASE_FOREACH (IDGroupedTransData *, group, &grouped_trans_datas) {
    ListBase *trans_datas = &group->trans_datas;

    /* Apply horizontal shuffle. */
    const float minimum_time_offset = transdata_get_time_shuffle_offset(trans_datas);
    LISTBASE_FOREACH (LinkData *, link, trans_datas) {
      TransDataNla *trans_data = (TransDataNla *)link->data;
      NlaStrip *strip = trans_data->strip;

      strip->start += minimum_time_offset;
      strip->end += minimum_time_offset;
      BKE_nlameta_flush_transforms(strip);
    }
  }

  /* Memory cleanup. */
  LISTBASE_FOREACH (IDGroupedTransData *, group, &grouped_trans_datas) {
    BLI_freelistN(&group->trans_datas);
  }
  BLI_freelistN(&grouped_trans_datas);
}

static void special_aftertrans_update__nla(bContext *C, TransInfo *t)
{
  bAnimContext ac;

  /* initialize relevant anim-context 'context' data */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  if (!ac.datatype) {
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  TransDataNla *first_trans_data = tc->custom.type.data;

  /* Shuffle transformed strips. */
  if (ELEM(t->mode, TFM_TRANSLATION)) {
    nlastrip_shuffle_transformed(tc, first_trans_data);
  }

  /* Clear NLASTRIP_FLAG_INVALID_LOCATION flag. */
  TransDataNla *tdn = first_trans_data;
  for (int i = 0; i < tc->data_len; i++, tdn++) {
    if (tdn->strip == NULL) {
      continue;
    }

    tdn->strip->flag &= ~NLASTRIP_FLAG_INVALID_LOCATION;
  }

  ListBase anim_data = {NULL, NULL};
  short filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_FCURVESONLY);

  /* get channels to work on */
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
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

/** \} */

TransConvertTypeInfo TransConvertType_NLA = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*createTransData*/ createTransNlaData,
    /*recalcData*/ recalcData_nla,
    /*special_aftertrans_update*/ special_aftertrans_update__nla,
};
