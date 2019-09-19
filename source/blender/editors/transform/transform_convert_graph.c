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

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_fcurve.h"
#include "BKE_nla.h"
#include "BKE_report.h"

#include "ED_anim_api.h"
#include "ED_markers.h"

#include "UI_view2d.h"

#include "transform.h"
#include "transform_convert.h"

typedef struct TransDataGraph {
  float unit_scale;
  float offset;
} TransDataGraph;

/* -------------------------------------------------------------------- */
/** \name Graph Editor Transform Creation
 *
 * \{ */

/* Helper function for createTransGraphEditData, which is responsible for associating
 * source data with transform data
 */
static void bezt_to_transdata(TransData *td,
                              TransData2D *td2d,
                              TransDataGraph *tdg,
                              AnimData *adt,
                              BezTriple *bezt,
                              int bi,
                              bool selected,
                              bool ishandle,
                              bool intvals,
                              const float mtx[3][3],
                              const float smtx[3][3],
                              float unit_scale,
                              float offset)
{
  float *loc = bezt->vec[bi];
  const float *cent = bezt->vec[1];

  /* New location from td gets dumped onto the old-location of td2d, which then
   * gets copied to the actual data at td2d->loc2d (bezt->vec[n])
   *
   * Due to NLA mapping, we apply NLA mapping to some of the verts here,
   * and then that mapping will be undone after transform is done.
   */

  if (adt) {
    td2d->loc[0] = BKE_nla_tweakedit_remap(adt, loc[0], NLATIME_CONVERT_MAP);
    td2d->loc[1] = (loc[1] + offset) * unit_scale;
    td2d->loc[2] = 0.0f;
    td2d->loc2d = loc;

    td->loc = td2d->loc;
    td->center[0] = BKE_nla_tweakedit_remap(adt, cent[0], NLATIME_CONVERT_MAP);
    td->center[1] = (cent[1] + offset) * unit_scale;
    td->center[2] = 0.0f;

    copy_v3_v3(td->iloc, td->loc);
  }
  else {
    td2d->loc[0] = loc[0];
    td2d->loc[1] = (loc[1] + offset) * unit_scale;
    td2d->loc[2] = 0.0f;
    td2d->loc2d = loc;

    td->loc = td2d->loc;
    copy_v3_v3(td->center, cent);
    td->center[1] = (td->center[1] + offset) * unit_scale;
    copy_v3_v3(td->iloc, td->loc);
  }

  if (!ishandle) {
    td2d->h1 = bezt->vec[0];
    td2d->h2 = bezt->vec[2];
    copy_v2_v2(td2d->ih1, td2d->h1);
    copy_v2_v2(td2d->ih2, td2d->h2);
  }
  else {
    td2d->h1 = NULL;
    td2d->h2 = NULL;
  }

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  /* store AnimData info in td->extra, for applying mapping when flushing */
  td->extra = adt;

  if (selected) {
    td->flag |= TD_SELECTED;
    td->dist = 0.0f;
  }
  else {
    td->dist = FLT_MAX;
  }

  if (ishandle) {
    td->flag |= TD_NOTIMESNAP;
  }
  if (intvals) {
    td->flag |= TD_INTVALUES;
  }

  /* copy space-conversion matrices for dealing with non-uniform scales */
  copy_m3_m3(td->mtx, mtx);
  copy_m3_m3(td->smtx, smtx);

  tdg->unit_scale = unit_scale;
  tdg->offset = offset;
}

static bool graph_edit_is_translation_mode(TransInfo *t)
{
  return ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_TRANSLATE, TFM_TIME_SLIDE, TFM_TIME_DUPLICATE);
}

static bool graph_edit_use_local_center(TransInfo *t)
{
  return ((t->around == V3D_AROUND_LOCAL_ORIGINS) && (graph_edit_is_translation_mode(t) == false));
}

static void graph_key_shortest_dist(
    TransInfo *t, FCurve *fcu, TransData *td_start, TransData *td, int cfra, bool use_handle)
{
  int j = 0;
  TransData *td_iter = td_start;

  td->dist = FLT_MAX;
  for (; j < fcu->totvert; j++) {
    BezTriple *bezt = fcu->bezt + j;
    if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
      const bool sel2 = (bezt->f2 & SELECT) != 0;
      const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
      const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

      if (sel1 || sel2 || sel3) {
        td->dist = td->rdist = min_ff(td->dist, fabs(td_iter->center[0] - td->center[0]));
      }

      td_iter += 3;
    }
  }
}

void createTransGraphEditData(bContext *C, TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  Scene *scene = t->scene;
  ARegion *ar = t->ar;
  View2D *v2d = &ar->v2d;

  TransData *td = NULL;
  TransData2D *td2d = NULL;
  TransDataGraph *tdg = NULL;

  bAnimContext ac;
  ListBase anim_data = {NULL, NULL};
  bAnimListElem *ale;
  int filter;

  BezTriple *bezt;
  int count = 0, i;
  float mtx[3][3], smtx[3][3];
  const bool is_translation_mode = graph_edit_is_translation_mode(t);
  const bool use_handle = !(sipo->flag & SIPO_NOHANDLES);
  const bool use_local_center = graph_edit_use_local_center(t);
  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  short anim_map_flag = ANIM_UNITCONV_ONLYSEL | ANIM_UNITCONV_SELVERTS;

  /* determine what type of data we are operating on */
  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  anim_map_flag |= ANIM_get_normalization_flags(&ac);

  /* filter data */
  filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVE_VISIBLE);
  ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);

  /* which side of the current frame should be allowed */
  // XXX we still want this mode, but how to get this using standard transform too?
  if (t->mode == TFM_TIME_EXTEND) {
    /* only side on which mouse is gets transformed */
    float xmouse, ymouse;

    UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[1], &xmouse, &ymouse);
    t->frame_side = (xmouse > CFRA) ? 'R' : 'L';  // XXX use t->frame_side
  }
  else {
    /* normal transform - both sides of current frame are considered */
    t->frame_side = 'B';
  }

  /* Loop 1: count how many BezTriples (specifically their verts)
   * are selected (or should be edited). */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    FCurve *fcu = (FCurve *)ale->key_data;
    float cfra;
    int curvecount = 0;
    bool selected = false;

    /* F-Curve may not have any keyframes */
    if (fcu->bezt == NULL) {
      continue;
    }

    /* convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points)
     */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = (float)CFRA;
    }

    /* Only include BezTriples whose 'keyframe'
     * occurs on the same side of the current frame as mouse. */
    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
        const bool sel2 = (bezt->f2 & SELECT) != 0;
        const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
        const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

        if (is_prop_edit) {
          curvecount += 3;
          if (sel2 || sel1 || sel3) {
            selected = true;
          }
        }
        else {
          if (!is_translation_mode || !(sel2)) {
            if (sel1) {
              count++;
            }

            if (sel3) {
              count++;
            }
          }

          /* only include main vert if selected */
          if (sel2 && !use_local_center) {
            count++;
          }
        }
      }
    }

    if (is_prop_edit) {
      if (selected) {
        count += curvecount;
        ale->tag = true;
      }
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

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData (Graph Editor)");
  /* For each 2d vert a 3d vector is allocated,
   * so that they can be treated just as if they were 3d verts. */
  tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransData2D (Graph Editor)");
  tc->custom.type.data = MEM_callocN(tc->data_len * sizeof(TransDataGraph), "TransDataGraph");
  tc->custom.type.use_free = true;

  td = tc->data;
  td2d = tc->data_2d;
  tdg = tc->custom.type.data;

  /* precompute space-conversion matrices for dealing with non-uniform scaling of Graph Editor */
  unit_m3(mtx);
  unit_m3(smtx);

  if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
    float xscale, yscale;

    /* apply scale factors to x and y axes of space-conversion matrices */
    UI_view2d_scale_get(v2d, &xscale, &yscale);

    /* mtx is data to global (i.e. view) conversion */
    mul_v3_fl(mtx[0], xscale);
    mul_v3_fl(mtx[1], yscale);

    /* smtx is global (i.e. view) to data conversion */
    if (IS_EQF(xscale, 0.0f) == 0) {
      mul_v3_fl(smtx[0], 1.0f / xscale);
    }
    if (IS_EQF(yscale, 0.0f) == 0) {
      mul_v3_fl(smtx[1], 1.0f / yscale);
    }
  }

  /* loop 2: build transdata arrays */
  for (ale = anim_data.first; ale; ale = ale->next) {
    AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
    FCurve *fcu = (FCurve *)ale->key_data;
    bool intvals = (fcu->flag & FCURVE_INT_VALUES) != 0;
    float unit_scale, offset;
    float cfra;

    /* F-Curve may not have any keyframes */
    if (fcu->bezt == NULL || (is_prop_edit && ale->tag == 0)) {
      continue;
    }

    /* convert current-frame to action-time (slightly less accurate, especially under
     * higher scaling ratios, but is faster than converting all points)
     */
    if (adt) {
      cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
    }
    else {
      cfra = (float)CFRA;
    }

    unit_scale = ANIM_unit_mapping_get_factor(
        ac.scene, ale->id, ale->key_data, anim_map_flag, &offset);

    /* only include BezTriples whose 'keyframe' occurs on the same side
     * of the current frame as mouse (if applicable) */
    for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
      if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
        const bool sel2 = (bezt->f2 & SELECT) != 0;
        const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
        const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

        TransDataCurveHandleFlags *hdata = NULL;
        /* short h1=1, h2=1; */ /* UNUSED */

        if (is_prop_edit) {
          bool is_sel = (sel2 || sel1 || sel3);
          /* we always select all handles for proportional editing if central handle is selected */
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            adt,
                            bezt,
                            0,
                            is_sel,
                            true,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            adt,
                            bezt,
                            1,
                            is_sel,
                            false,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
          initTransDataCurveHandles(td, bezt);
          bezt_to_transdata(td++,
                            td2d++,
                            tdg++,
                            adt,
                            bezt,
                            2,
                            is_sel,
                            true,
                            intvals,
                            mtx,
                            smtx,
                            unit_scale,
                            offset);
        }
        else {
          /* only include handles if selected, irrespective of the interpolation modes.
           * also, only treat handles specially if the center point isn't selected.
           */
          if (!is_translation_mode || !(sel2)) {
            if (sel1) {
              hdata = initTransDataCurveHandles(td, bezt);
              bezt_to_transdata(td++,
                                td2d++,
                                tdg++,
                                adt,
                                bezt,
                                0,
                                sel1,
                                true,
                                intvals,
                                mtx,
                                smtx,
                                unit_scale,
                                offset);
            }
            else {
              /* h1 = 0; */ /* UNUSED */
            }

            if (sel3) {
              if (hdata == NULL) {
                hdata = initTransDataCurveHandles(td, bezt);
              }
              bezt_to_transdata(td++,
                                td2d++,
                                tdg++,
                                adt,
                                bezt,
                                2,
                                sel3,
                                true,
                                intvals,
                                mtx,
                                smtx,
                                unit_scale,
                                offset);
            }
            else {
              /* h2 = 0; */ /* UNUSED */
            }
          }

          /* only include main vert if selected */
          if (sel2 && !use_local_center) {
            /* move handles relative to center */
            if (is_translation_mode) {
              if (sel1) {
                td->flag |= TD_MOVEHANDLE1;
              }
              if (sel3) {
                td->flag |= TD_MOVEHANDLE2;
              }
            }

            /* if handles were not selected, store their selection status */
            if (!(sel1) || !(sel3)) {
              if (hdata == NULL) {
                hdata = initTransDataCurveHandles(td, bezt);
              }
            }

            bezt_to_transdata(td++,
                              td2d++,
                              tdg++,
                              adt,
                              bezt,
                              1,
                              sel2,
                              false,
                              intvals,
                              mtx,
                              smtx,
                              unit_scale,
                              offset);
          }
          /* Special hack (must be done after #initTransDataCurveHandles(),
           * as that stores handle settings to restore...):
           *
           * - Check if we've got entire BezTriple selected and we're scaling/rotating that point,
           *   then check if we're using auto-handles.
           * - If so, change them auto-handles to aligned handles so that handles get affected too
           */
          if (ELEM(bezt->h1, HD_AUTO, HD_AUTO_ANIM) && ELEM(bezt->h2, HD_AUTO, HD_AUTO_ANIM) &&
              ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
            if (hdata && (sel1) && (sel3)) {
              bezt->h1 = HD_ALIGN;
              bezt->h2 = HD_ALIGN;
            }
          }
        }
      }
    }

    /* Sets handles based on the selection */
    testhandles_fcurve(fcu, use_handle);
  }

  if (is_prop_edit) {
    /* loop 2: build transdata arrays */
    td = tc->data;

    for (ale = anim_data.first; ale; ale = ale->next) {
      AnimData *adt = ANIM_nla_mapping_get(&ac, ale);
      FCurve *fcu = (FCurve *)ale->key_data;
      TransData *td_start = td;
      float cfra;

      /* F-Curve may not have any keyframes */
      if (fcu->bezt == NULL || (ale->tag == 0)) {
        continue;
      }

      /* convert current-frame to action-time (slightly less accurate, especially under
       * higher scaling ratios, but is faster than converting all points)
       */
      if (adt) {
        cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
      }
      else {
        cfra = (float)CFRA;
      }

      /* only include BezTriples whose 'keyframe' occurs on the
       * same side of the current frame as mouse (if applicable) */
      for (i = 0, bezt = fcu->bezt; i < fcu->totvert; i++, bezt++) {
        if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
          const bool sel2 = (bezt->f2 & SELECT) != 0;
          const bool sel1 = use_handle ? (bezt->f1 & SELECT) != 0 : sel2;
          const bool sel3 = use_handle ? (bezt->f3 & SELECT) != 0 : sel2;

          if (sel1 || sel2) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }
          td++;

          if (sel2) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }
          td++;

          if (sel3 || sel2) {
            td->dist = td->rdist = 0.0f;
          }
          else {
            graph_key_shortest_dist(t, fcu, td_start, td, cfra, use_handle);
          }
          td++;
        }
      }
    }
  }

  /* cleanup temp list */
  ANIM_animdata_freelist(&anim_data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Graph Editor Transform Flush
 *
 * \{ */

/* this function is called on recalcData to apply the transforms applied
 * to the transdata on to the actual keyframe data
 */
void flushTransGraphData(TransInfo *t)
{
  SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
  TransData *td;
  TransData2D *td2d;
  TransDataGraph *tdg;
  Scene *scene = t->scene;
  double secf = FPS;
  int a;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data, td2d = tc->data_2d, tdg = tc->custom.type.data; a < tc->data_len;
       a++, td++, td2d++, tdg++) {
    /* pointers to relevant AnimData blocks are stored in the td->extra pointers */
    AnimData *adt = (AnimData *)td->extra;

    float inv_unit_scale = 1.0f / tdg->unit_scale;

    /* Handle snapping for time values:
     * - We should still be in NLA-mapping time-space.
     * - Only apply to keyframes (but never to handles).
     * - Don't do this when canceling, or else these changes won't go away.
     */
    if ((t->state != TRANS_CANCEL) && (td->flag & TD_NOTIMESNAP) == 0) {
      switch (sipo->autosnap) {
        case SACTSNAP_FRAME: /* snap to nearest frame */
          td2d->loc[0] = floor((double)td2d->loc[0] + 0.5);
          break;

        case SACTSNAP_SECOND: /* snap to nearest second */
          td2d->loc[0] = floor(((double)td2d->loc[0] / secf) + 0.5) * secf;
          break;

        case SACTSNAP_MARKER: /* snap to nearest marker */
          td2d->loc[0] = (float)ED_markers_find_nearest_marker_time(&t->scene->markers,
                                                                    td2d->loc[0]);
          break;
      }
    }

    /* we need to unapply the nla-mapping from the time in some situations */
    if (adt) {
      td2d->loc2d[0] = BKE_nla_tweakedit_remap(adt, td2d->loc[0], NLATIME_CONVERT_UNMAP);
    }
    else {
      td2d->loc2d[0] = td2d->loc[0];
    }

    /** Time-stepping auto-snapping modes don't get applied for Graph Editor transforms,
     * as these use the generic transform modes which don't account for this sort of thing.
     * These ones aren't affected by NLA mapping, so we do this after the conversion...
     *
     * \note We also have to apply to td->loc,
     * as that's what the handle-adjustment step below looks to,
     * otherwise we get "swimming handles".
     *
     * \note We don't do this when canceling transforms, or else these changes don't go away.
     */
    if ((t->state != TRANS_CANCEL) && (td->flag & TD_NOTIMESNAP) == 0 &&
        ELEM(sipo->autosnap, SACTSNAP_STEP, SACTSNAP_TSTEP)) {
      switch (sipo->autosnap) {
        case SACTSNAP_STEP: /* frame step */
          td2d->loc2d[0] = floor((double)td2d->loc[0] + 0.5);
          td->loc[0] = floor((double)td->loc[0] + 0.5);
          break;

        case SACTSNAP_TSTEP: /* second step */
          /* XXX: the handle behavior in this case is still not quite right... */
          td2d->loc[0] = floor(((double)td2d->loc[0] / secf) + 0.5) * secf;
          td->loc[0] = floor(((double)td->loc[0] / secf) + 0.5) * secf;
          break;
      }
    }

    /* if int-values only, truncate to integers */
    if (td->flag & TD_INTVALUES) {
      td2d->loc2d[1] = floorf(td2d->loc[1] * inv_unit_scale - tdg->offset + 0.5f);
    }
    else {
      td2d->loc2d[1] = td2d->loc[1] * inv_unit_scale - tdg->offset;
    }

    if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
      td2d->h1[0] = td2d->ih1[0] + td->loc[0] - td->iloc[0];
      td2d->h1[1] = td2d->ih1[1] + (td->loc[1] - td->iloc[1]) * inv_unit_scale;
    }

    if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
      td2d->h2[0] = td2d->ih2[0] + td->loc[0] - td->iloc[0];
      td2d->h2[1] = td2d->ih2[1] + (td->loc[1] - td->iloc[1]) * inv_unit_scale;
    }
  }
}

/** \} */
