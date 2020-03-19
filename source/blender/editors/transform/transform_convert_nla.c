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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

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
#include "BKE_report.h"

#include "ED_anim_api.h"

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name NLA Transform Creation
 *
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
    /* only side on which center is gets transformed */
    float center[2];
    transform_convert_center_global_v2(t, center);
    t->frame_side = (center[0] > CFRA) ? 'R' : 'L';
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
      // TODO: we can make strips have handles later on...
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
        // TODO: we can make strips have handles later on...
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

              /* now, link the transform data up to this data */
              if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
                td->loc = tdn->h1;
                copy_v3_v3(td->iloc, tdn->h1);

                /* store all the other gunk that is required by transform */
                copy_v3_v3(td->center, center);
                memset(td->axismtx, 0, sizeof(td->axismtx));
                td->axismtx[2][2] = 1.0f;

                td->ext = NULL;
                td->val = NULL;

                td->flag |= TD_SELECTED;
                td->dist = 0.0f;

                unit_m3(td->mtx);
                unit_m3(td->smtx);
              }
              else {
                /* time scaling only needs single value */
                td->val = &tdn->h1[0];
                td->ival = tdn->h1[0];
              }

              td->extra = tdn;
              td++;
            }
            if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA)) {
              /* if tdn is already holding the start handle,
               * then we're doing both, otherwise, only end */
              tdn->handle = (tdn->handle) ? 2 : 1;

              /* now, link the transform data up to this data */
              if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
                td->loc = tdn->h2;
                copy_v3_v3(td->iloc, tdn->h2);

                /* store all the other gunk that is required by transform */
                copy_v3_v3(td->center, center);
                memset(td->axismtx, 0, sizeof(td->axismtx));
                td->axismtx[2][2] = 1.0f;

                td->ext = NULL;
                td->val = NULL;

                td->flag |= TD_SELECTED;
                td->dist = 0.0f;

                unit_m3(td->mtx);
                unit_m3(td->smtx);
              }
              else {
                /* time scaling only needs single value */
                td->val = &tdn->h2[0];
                td->ival = tdn->h2[0];
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

/** \} */
