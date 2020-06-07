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
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_nla.h"
#include "BKE_report.h"

#include "ED_anim_api.h"

#include "transform.h"
#include "transform_convert.h"

/* helper struct for gp-frame transforms */
typedef struct tGPFtransdata {
  float val;  /* where transdata writes transform */
  int *sdata; /* pointer to gpf->framenum */
} tGPFtransdata;

/* -------------------------------------------------------------------- */
/** \name Action Transform Creation
 *
 * \{ */

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_fcurve_keys(FCurve *fcu, char side, float cfra, bool is_prop_edit)
{
  BezTriple *bezt;
  int i, count = 0, count_all = 0;

  if (ELEM(NULL, fcu, fcu->bezt)) {
    return count;
  }

  /* only include points that occur on the right side of cfra */
  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
      /* no need to adjust the handle selection since they are assumed
       * selected (like graph editor with SIPO_NOHANDLES) */
      if (bezt->f2 & SELECT) {
        count++;
      }

      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  else {
    return count;
  }
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_gplayer_frames(bGPDlayer *gpl, char side, float cfra, bool is_prop_edit)
{
  bGPDframe *gpf;
  int count = 0, count_all = 0;

  if (gpl == NULL) {
    return count;
  }

  /* only include points that occur on the right side of cfra */
  for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
    if (FrameOnMouseSide(side, (float)gpf->framenum, cfra)) {
      if (gpf->flag & GP_FRAME_SELECT) {
        count++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  else {
    return count;
  }
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_masklayer_frames(MaskLayer *masklay, char side, float cfra, bool is_prop_edit)
{
  MaskLayerShape *masklayer_shape;
  int count = 0, count_all = 0;

  if (masklay == NULL) {
    return count;
  }

  /* only include points that occur on the right side of cfra */
  for (masklayer_shape = masklay->splines_shapes.first; masklayer_shape;
       masklayer_shape = masklayer_shape->next) {
    if (FrameOnMouseSide(side, (float)masklayer_shape->frame, cfra)) {
      if (masklayer_shape->flag & MASK_SHAPE_SELECT) {
        count++;
      }
      count_all++;
    }
  }

  if (is_prop_edit && count > 0) {
    return count_all;
  }
  else {
    return count;
  }
}

/* This function assigns the information to transdata */
static void TimeToTransData(TransData *td, float *time, AnimData *adt, float ypos)
{
  /* memory is calloc'ed, so that should zero everything nicely for us */
  td->val = time;
  td->ival = *(time);

  td->center[0] = td->ival;
  td->center[1] = ypos;

  /* store the AnimData where this keyframe exists as a keyframe of the
   * active action as td->extra.
   */
  td->extra = adt;
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = IcuToTransData(td, icu, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used.
 */
static TransData *ActionFCurveToTransData(TransData *td,
                                          TransData2D **td2dv,
                                          FCurve *fcu,
                                          AnimData *adt,
                                          char side,
                                          float cfra,
                                          bool is_prop_edit,
                                          float ypos)
{
  BezTriple *bezt;
  TransData2D *td2d = *td2dv;
  int i;

  if (ELEM(NULL, fcu, fcu->bezt)) {
    return td;
  }

  for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
    /* only add selected keyframes (for now, proportional edit is not enabled) */
    if (is_prop_edit || (bezt->f2 & SELECT)) { /* note this MUST match count_fcurve_keys(),
                                                * so can't use BEZT_ISSEL_ANY() macro */
      /* only add if on the right 'side' of the current frame */
      if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
        TimeToTransData(td, bezt->vec[1], adt, ypos);

        if (bezt->f2 & SELECT) {
          td->flag |= TD_SELECTED;
        }

        /*set flags to move handles as necessary*/
        td->flag |= TD_MOVEHANDLE1 | TD_MOVEHANDLE2;
        td2d->h1 = bezt->vec[0];
        td2d->h2 = bezt->vec[2];

        copy_v2_v2(td2d->ih1, td2d->h1);
        copy_v2_v2(td2d->ih2, td2d->h2);

        td++;
        td2d++;
      }
    }
  }

  *td2dv = td2d;

  return td;
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = GPLayerToTransData(td, ipo, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used.
 */
static int GPLayerToTransData(TransData *td,
                              tGPFtransdata *tfd,
                              bGPDlayer *gpl,
                              char side,
                              float cfra,
                              bool is_prop_edit,
                              float ypos)
{
  bGPDframe *gpf;
  int count = 0;

  /* check for select frames on right side of current frame */
  for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
    if (is_prop_edit || (gpf->flag & GP_FRAME_SELECT)) {
      if (FrameOnMouseSide(side, (float)gpf->framenum, cfra)) {
        /* memory is calloc'ed, so that should zero everything nicely for us */
        td->val = &tfd->val;
        td->ival = (float)gpf->framenum;

        td->center[0] = td->ival;
        td->center[1] = ypos;

        tfd->val = (float)gpf->framenum;
        tfd->sdata = &gpf->framenum;

        /* advance td now */
        td++;
        tfd++;
        count++;
      }
    }
  }

  return count;
}

/* refer to comment above #GPLayerToTransData, this is the same but for masks */
static int MaskLayerToTransData(TransData *td,
                                tGPFtransdata *tfd,
                                MaskLayer *masklay,
                                char side,
                                float cfra,
                                bool is_prop_edit,
                                float ypos)
{
  MaskLayerShape *masklay_shape;
  int count = 0;

  /* check for select frames on right side of current frame */
  for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
       masklay_shape = masklay_shape->next) {
    if (is_prop_edit || (masklay_shape->flag & MASK_SHAPE_SELECT)) {
      if (FrameOnMouseSide(side, (float)masklay_shape->frame, cfra)) {
        /* memory is calloc'ed, so that should zero everything nicely for us */
        td->val = &tfd->val;
        td->ival = (float)masklay_shape->frame;

        td->center[0] = td->ival;
        td->center[1] = ypos;

        tfd->val = (float)masklay_shape->frame;
        tfd->sdata = &masklay_shape->frame;

        /* advance td now */
        td++;
        tfd++;
        count++;
      }
    }
  }

  return count;
}

void createTransActionData(bContext *C, TransInfo *t)
{
  Scene *scene = t->scene;
  TransData *td = NULL;
  TransData2D *td2d = NULL;
  tGPFtransdata *tfd = NULL;

  rcti *mask = &t->region->v2d.mask;
  rctf *datamask = &t->region->v2d.cur;

  float xsize = BLI_rctf_size_x(datamask);
  float ysize = BLI_rctf_size_y(datamask);
  float xmask = BLI_rcti_size_x(mask);
  float ymask = BLI_rcti_size_y(mask);

  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;

  int count = 0;
  float cfra;
  float ypos = 1.0f / ((ysize / xsize) * (xmask / ymask)) * BLI_rctf_cent_y(&t->region->v2d.cur);

  /* determine what type of data we are operating on */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  /* filter data */
  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT);
  }
  else {
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT /*| ANIMFILTER_CURVESONLY*/);
  }
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  if (t->mode == TFM_TIME_EXTEND) {
    t->frame_side = transform_convert_frame_side_dir_get(t, (float)CFRA);
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

  /* loop 1: fully select ipo-keys and count how many BezTriples are selected */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    int adt_count = 0;
    /* convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points)
     */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = (float)CFRA;
    }

    if (ELEM(ale->type, ANIMTYPE_FCURVE, ANIMTYPE_NLACURVE)) {
      adt_count = count_fcurve_keys(ale->key_data, t->frame_side, cfra, is_prop_edit);
    }
    else if (ale->type == ANIMTYPE_GPLAYER) {
      adt_count = count_gplayer_frames(ale->data, t->frame_side, cfra, is_prop_edit);
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      adt_count = count_masklayer_frames(ale->data, t->frame_side, cfra, is_prop_edit);
    }
    else {
      BLI_assert(0);
    }

    if (adt_count > 0) {
      count += adt_count;
      ale->tag = true;
    }
  }

  /* stop if trying to build list if nothing selected */
  if (count == 0) {
    /* cleanup temp list */
    ANIM_animdata_freelist(&anim_data);
    return;
  }

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* allocate memory for data */
  tc->data_len = count;

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(Action Editor)");
  tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "transdata2d");
  td = tc->data;
  td2d = tc->data_2d;

  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    tc->custom.type.data = tfd = MEM_callocN(sizeof(tGPFtransdata) * count, "tGPFtransdata");
    tc->custom.type.use_free = true;
  }

  /* loop 2: build transdata array */
  for (ale = anim_data.first; ale; ale = ale->next) {

    if (is_prop_edit && !ale->tag) {
      continue;
    }

    cfra = (float)CFRA;

    {
      AnimData *adt;
      adt = ANIM_nla_mapping_get(&ac, ale);
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
      }
    }

    if (ale->type == ANIMTYPE_GPLAYER) {
      bGPDlayer *gpl = (bGPDlayer *)ale->data;
      int i;

      i = GPLayerToTransData(td, tfd, gpl, t->frame_side, cfra, is_prop_edit, ypos);
      td += i;
      tfd += i;
    }
    else if (ale->type == ANIMTYPE_MASKLAYER) {
      MaskLayer *masklay = (MaskLayer *)ale->data;
      int i;

      i = MaskLayerToTransData(td, tfd, masklay, t->frame_side, cfra, is_prop_edit, ypos);
      td += i;
      tfd += i;
    }
    else {
      AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
      FCurve *fcu = (FCurve *)ale->key_data;

      td = ActionFCurveToTransData(td, &td2d, fcu, adt, t->frame_side, cfra, is_prop_edit, ypos);
    }
  }

  /* calculate distances for proportional editing */
  if (is_prop_edit) {
    td = tc->data;

    for (ale = anim_data.first; ale; ale = ale->next) {
      AnimData *adt;

      /* F-Curve may not have any keyframes */
      if (!ale->tag) {
        continue;
      }

      adt = ANIM_nla_mapping_get(&ac, ale);
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
      }
      else {
        cfra = (float)CFRA;
      }

      if (ale->type == ANIMTYPE_GPLAYER) {
        bGPDlayer *gpl = (bGPDlayer *)ale->data;
        bGPDframe *gpf;

        for (gpf = gpl->frames.first; gpf; gpf = gpf->next) {
          if (gpf->flag & GP_FRAME_SELECT) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            bGPDframe *gpf_iter;
            int min = INT_MAX;
            for (gpf_iter = gpl->frames.first; gpf_iter; gpf_iter = gpf_iter->next) {
              if (gpf_iter->flag & GP_FRAME_SELECT) {
                if (FrameOnMouseSide(t->frame_side, (float)gpf_iter->framenum, cfra)) {
                  int val = abs(gpf->framenum - gpf_iter->framenum);
                  if (val < min) {
                    min = val;
                  }
                }
              }
            }
            td->dist = td->rdist = min;
          }
          td++;
        }
      }
      else if (ale->type == ANIMTYPE_MASKLAYER) {
        MaskLayer *masklay = (MaskLayer *)ale->data;
        MaskLayerShape *masklay_shape;

        for (masklay_shape = masklay->splines_shapes.first; masklay_shape;
             masklay_shape = masklay_shape->next) {
          if (FrameOnMouseSide(t->frame_side, (float)masklay_shape->frame, cfra)) {
            if (masklay_shape->flag & MASK_SHAPE_SELECT) {
              td->dist = td->rdist = 0.0f;
            }
            else {
              MaskLayerShape *masklay_iter;
              int min = INT_MAX;
              for (masklay_iter = masklay->splines_shapes.first; masklay_iter;
                   masklay_iter = masklay_iter->next) {
                if (masklay_iter->flag & MASK_SHAPE_SELECT) {
                  if (FrameOnMouseSide(t->frame_side, (float)masklay_iter->frame, cfra)) {
                    int val = abs(masklay_shape->frame - masklay_iter->frame);
                    if (val < min) {
                      min = val;
                    }
                  }
                }
              }
              td->dist = td->rdist = min;
            }
            td++;
          }
        }
      }
      else {
        FCurve *fcu = (FCurve *)ale->key_data;
        BezTriple *bezt;
        int i;

        for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
          if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
            if (bezt->f2 & SELECT) {
              td->dist = td->rdist = 0.0f;
            }
            else {
              BezTriple *bezt_iter;
              int j;
              float min = FLT_MAX;
              for (j = 0, bezt_iter = fcu->bezt; j < fcu->totvert; j++, bezt_iter++) {
                if (bezt_iter->f2 & SELECT) {
                  if (FrameOnMouseSide(t->frame_side, (float)bezt_iter->vec[1][0], cfra)) {
                    float val = fabs(bezt->vec[1][0] - bezt_iter->vec[1][0]);
                    if (val < min) {
                      min = val;
                    }
                  }
                }
              }
              td->dist = td->rdist = min;
            }
            td++;
          }
        }
      }
    }
  }

  /* cleanup temp list */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Action Transform Flush
 *
 * \{ */

/* This function helps flush transdata written to tempdata into the gp-frames  */
static void flushTransIntFrameActionData(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tGPFtransdata *tfd = tc->custom.type.data;

  /* flush data! */
  for (int i = 0; i < tc->data_len; i++, tfd++) {
    *(tfd->sdata) = round_fl_to_int(tfd->val);
  }
}

/* helper for recalcData() - for Action Editor transforms */
void recalcData_actedit(TransInfo *t)
{
  ViewLayer *view_layer = t->view_layer;
  SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;

  bAnimContext ac = {NULL};
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  /* initialize relevant anim-context 'context' data from TransInfo data */
  /* NOTE: sync this with the code in ANIM_animdata_get_context() */
  ac.bmain = CTX_data_main(t->context);
  ac.scene = t->scene;
  ac.view_layer = t->view_layer;
  ac.obact = OBACT(view_layer);
  ac.area = t->area;
  ac.region = t->region;
  ac.sl = (t->area) ? t->area->spacedata.first : NULL;
  ac.spacetype = (t->area) ? t->area->spacetype : 0;
  ac.regiontype = (t->region) ? t->region->regiontype : 0;

  ANIM_animdata_context_getdata(&ac);

  /* perform flush */
  if (ELEM(ac.datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
    /* flush transform values back to actual coordinates */
    flushTransIntFrameActionData(t);
  }

  if (ac.datatype != ANIMCONT_MASK) {
    /* Get animdata blocks visible in editor,
     * assuming that these will be the ones where things changed. */
    filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_ANIMDATA);
    ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

    /* just tag these animdata-blocks to recalc, assuming that some data there changed
     * BUT only do this if realtime updates are enabled
     */
    if ((saction->flag & SACTION_NOREALTIMEUPDATES) == 0) {
      for (ale = anim_data.first; ale; ale = ale->next) {
        /* set refresh tags for objects using this animation */
        ANIM_list_elem_update(CTX_data_main(t->context), t->scene, ale);
      }
    }

    /* now free temp channels */
    ANIM_animdata_freelist(&anim_data);
  }
}

/** \} */
