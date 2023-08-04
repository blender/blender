/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_mask_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_mask.h"

#include "ED_clip.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_mask.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "transform.hh"
#include "transform_convert.hh"

struct TransDataMasking {
  bool is_handle;

  float handle[2], orig_handle[2];
  float vec[3][3];
  MaskSplinePoint *point;
  float parent_matrix[3][3];
  float parent_inverse_matrix[3][3];
  char orig_handle_type;

  eMaskWhichHandle which_handle;
};

/* -------------------------------------------------------------------- */
/** \name Masking Transform Creation
 * \{ */

static void MaskHandleToTransData(MaskSplinePoint *point,
                                  eMaskWhichHandle which_handle,
                                  TransData *td,
                                  TransData2D *td2d,
                                  TransDataMasking *tdm,
                                  const float asp[2],
                                  /*const*/ const float parent_matrix[3][3],
                                  /*const*/ const float parent_inverse_matrix[3][3])
{
  BezTriple *bezt = &point->bezt;
  const bool is_sel_any = MASKPOINT_ISSEL_ANY(point);

  tdm->point = point;
  copy_m3_m3(tdm->vec, bezt->vec);

  tdm->is_handle = true;
  copy_m3_m3(tdm->parent_matrix, parent_matrix);
  copy_m3_m3(tdm->parent_inverse_matrix, parent_inverse_matrix);

  BKE_mask_point_handle(point, which_handle, tdm->handle);
  tdm->which_handle = which_handle;

  copy_v2_v2(tdm->orig_handle, tdm->handle);

  mul_v2_m3v2(td2d->loc, parent_matrix, tdm->handle);
  td2d->loc[0] *= asp[0];
  td2d->loc[1] *= asp[1];
  td2d->loc[2] = 0.0f;

  td2d->loc2d = tdm->handle;

  td->flag = 0;
  td->loc = td2d->loc;
  mul_v2_m3v2(td->center, parent_matrix, bezt->vec[1]);
  td->center[0] *= asp[0];
  td->center[1] *= asp[1];
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = nullptr;
  td->val = nullptr;

  if (is_sel_any) {
    td->flag |= TD_SELECTED;
  }

  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  if (which_handle == MASK_WHICH_HANDLE_LEFT) {
    tdm->orig_handle_type = bezt->h1;
  }
  else if (which_handle == MASK_WHICH_HANDLE_RIGHT) {
    tdm->orig_handle_type = bezt->h2;
  }
}

static void MaskPointToTransData(Scene *scene,
                                 MaskSplinePoint *point,
                                 TransData *td,
                                 TransData2D *td2d,
                                 TransDataMasking *tdm,
                                 const bool is_prop_edit,
                                 const float asp[2])
{
  BezTriple *bezt = &point->bezt;
  const bool is_sel_point = MASKPOINT_ISSEL_KNOT(point);
  const bool is_sel_any = MASKPOINT_ISSEL_ANY(point);
  float parent_matrix[3][3], parent_inverse_matrix[3][3];

  BKE_mask_point_parent_matrix_get(point, scene->r.cfra, parent_matrix);
  invert_m3_m3(parent_inverse_matrix, parent_matrix);

  if (is_prop_edit || is_sel_point) {

    tdm->point = point;
    copy_m3_m3(tdm->vec, bezt->vec);

    for (int i = 0; i < 3; i++) {
      copy_m3_m3(tdm->parent_matrix, parent_matrix);
      copy_m3_m3(tdm->parent_inverse_matrix, parent_inverse_matrix);

      /* CV coords are scaled by aspects. this is needed for rotations and
       * proportional editing to be consistent with the stretched CV coords
       * that are displayed. this also means that for display and number-input,
       * and when the CV coords are flushed, these are converted each time */
      mul_v2_m3v2(td2d->loc, parent_matrix, bezt->vec[i]);
      td2d->loc[0] *= asp[0];
      td2d->loc[1] *= asp[1];
      td2d->loc[2] = 0.0f;

      td2d->loc2d = bezt->vec[i];

      td->flag = 0;
      td->loc = td2d->loc;
      mul_v2_m3v2(td->center, parent_matrix, bezt->vec[1]);
      td->center[0] *= asp[0];
      td->center[1] *= asp[1];
      copy_v3_v3(td->iloc, td->loc);

      memset(td->axismtx, 0, sizeof(td->axismtx));
      td->axismtx[2][2] = 1.0f;

      td->ext = nullptr;

      if (i == 1) {
        /* scaling weights */
        td->val = &bezt->weight;
        td->ival = *td->val;
      }
      else {
        td->val = nullptr;
      }

      if (is_sel_any) {
        td->flag |= TD_SELECTED;
      }
      td->dist = 0.0;

      unit_m3(td->mtx);
      unit_m3(td->smtx);

      if (i == 0) {
        tdm->orig_handle_type = bezt->h1;
      }
      else if (i == 2) {
        tdm->orig_handle_type = bezt->h2;
      }

      td++;
      td2d++;
      tdm++;
    }
  }
  else {
    if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
      MaskHandleToTransData(point,
                            MASK_WHICH_HANDLE_STICK,
                            td,
                            td2d,
                            tdm,
                            asp,
                            parent_matrix,
                            parent_inverse_matrix);

      td++;
      td2d++;
      tdm++;
    }
    else {
      if (bezt->f1 & SELECT) {
        MaskHandleToTransData(point,
                              MASK_WHICH_HANDLE_LEFT,
                              td,
                              td2d,
                              tdm,
                              asp,
                              parent_matrix,
                              parent_inverse_matrix);

        if (bezt->h1 == HD_VECT) {
          bezt->h1 = HD_FREE;
        }
        else if (bezt->h1 == HD_AUTO) {
          bezt->h1 = HD_ALIGN_DOUBLESIDE;
          bezt->h2 = HD_ALIGN_DOUBLESIDE;
        }

        td++;
        td2d++;
        tdm++;
      }
      if (bezt->f3 & SELECT) {
        MaskHandleToTransData(point,
                              MASK_WHICH_HANDLE_RIGHT,
                              td,
                              td2d,
                              tdm,
                              asp,
                              parent_matrix,
                              parent_inverse_matrix);

        if (bezt->h2 == HD_VECT) {
          bezt->h2 = HD_FREE;
        }
        else if (bezt->h2 == HD_AUTO) {
          bezt->h1 = HD_ALIGN_DOUBLESIDE;
          bezt->h2 = HD_ALIGN_DOUBLESIDE;
        }

        td++;
        td2d++;
        tdm++;
      }
    }
  }
}

static void createTransMaskingData(bContext *C, TransInfo *t)
{
  Scene *scene = CTX_data_scene(C);
  Mask *mask = CTX_data_edit_mask(C);
  TransData *td = nullptr;
  TransData2D *td2d = nullptr;
  TransDataMasking *tdm = nullptr;
  int count = 0, countsel = 0;
  const bool is_prop_edit = (t->flag & T_PROP_EDIT);
  float asp[2];

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!ED_maskedit_mask_visible_splines_poll(C)) {
    return;
  }

  /* count */
  LISTBASE_FOREACH (MaskLayer *, masklay, &mask->masklayers) {
    if (masklay->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    LISTBASE_FOREACH (MaskSpline *, spline, &masklay->splines) {
      int i;

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (MASKPOINT_ISSEL_ANY(point)) {
          if (MASKPOINT_ISSEL_KNOT(point)) {
            countsel += 3;
          }
          else {
            if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
              countsel += 1;
            }
            else {
              BezTriple *bezt = &point->bezt;
              if (bezt->f1 & SELECT) {
                countsel++;
              }
              if (bezt->f3 & SELECT) {
                countsel++;
              }
            }
          }
        }

        if (is_prop_edit) {
          count += 3;
        }
      }
    }
  }

  /* NOTE: in prop mode we need at least 1 selected. */
  if (countsel == 0) {
    return;
  }

  ED_mask_get_aspect(t->area, t->region, &asp[0], &asp[1]);

  tc->data_len = (is_prop_edit) ? count : countsel;
  td = tc->data = static_cast<TransData *>(
      MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(Mask Editing)"));
  /* for each 2d uv coord a 3d vector is allocated, so that they can be
   * treated just as if they were 3d verts */
  td2d = tc->data_2d = static_cast<TransData2D *>(
      MEM_callocN(tc->data_len * sizeof(TransData2D), "TransObData2D(Mask Editing)"));
  tc->custom.type.data = tdm = static_cast<TransDataMasking *>(
      MEM_callocN(tc->data_len * sizeof(TransDataMasking), "TransDataMasking(Mask Editing)"));
  tc->custom.type.use_free = true;

  /* create data */
  LISTBASE_FOREACH (MaskLayer *, masklay, &mask->masklayers) {
    if (masklay->visibility_flag & (MASK_HIDE_VIEW | MASK_HIDE_SELECT)) {
      continue;
    }

    LISTBASE_FOREACH (MaskSpline *, spline, &masklay->splines) {
      int i;

      for (i = 0; i < spline->tot_point; i++) {
        MaskSplinePoint *point = &spline->points[i];

        if (is_prop_edit || MASKPOINT_ISSEL_ANY(point)) {
          MaskPointToTransData(scene, point, td, td2d, tdm, is_prop_edit, asp);

          if (is_prop_edit || MASKPOINT_ISSEL_KNOT(point)) {
            td += 3;
            td2d += 3;
            tdm += 3;
          }
          else {
            if (BKE_mask_point_handles_mode_get(point) == MASK_HANDLE_MODE_STICK) {
              td++;
              td2d++;
              tdm++;
            }
            else {
              BezTriple *bezt = &point->bezt;
              if (bezt->f1 & SELECT) {
                td++;
                td2d++;
                tdm++;
              }
              if (bezt->f3 & SELECT) {
                td++;
                td2d++;
                tdm++;
              }
            }
          }
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc TransData Masking
 * \{ */

static void flushTransMasking(TransInfo *t)
{
  TransData2D *td;
  TransDataMasking *tdm;
  int a;
  float asp[2], inv[2];

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  ED_mask_get_aspect(t->area, t->region, &asp[0], &asp[1]);
  inv[0] = 1.0f / asp[0];
  inv[1] = 1.0f / asp[1];

  /* flush to 2d vector from internally used 3d vector */
  for (a = 0, td = tc->data_2d, tdm = static_cast<TransDataMasking *>(tc->custom.type.data);
       a < tc->data_len;
       a++, td++, tdm++)
  {
    td->loc2d[0] = td->loc[0] * inv[0];
    td->loc2d[1] = td->loc[1] * inv[1];
    mul_m3_v2(tdm->parent_inverse_matrix, td->loc2d);

    if (tdm->is_handle) {
      BKE_mask_point_set_handle(tdm->point,
                                tdm->which_handle,
                                td->loc2d,
                                (t->flag & T_ALT_TRANSFORM) != 0,
                                tdm->orig_handle,
                                tdm->vec);
    }

    if (t->state == TRANS_CANCEL) {
      if (tdm->which_handle == MASK_WHICH_HANDLE_LEFT) {
        tdm->point->bezt.h1 = tdm->orig_handle_type;
      }
      else if (tdm->which_handle == MASK_WHICH_HANDLE_RIGHT) {
        tdm->point->bezt.h2 = tdm->orig_handle_type;
      }
    }
  }
}

static void recalcData_mask_common(TransInfo *t)
{
  Mask *mask = CTX_data_edit_mask(t->context);

  flushTransMasking(t);

  DEG_id_tag_update(&mask->id, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Mask
 * \{ */

static void special_aftertrans_update__mask(bContext *C, TransInfo *t)
{
  Mask *mask = nullptr;

  if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = static_cast<SpaceClip *>(t->area->spacedata.first);
    mask = ED_space_clip_get_mask(sc);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
    mask = ED_space_image_get_mask(sima);
  }
  else {
    BLI_assert(0);
  }

  if (t->scene->nodetree) {
    WM_event_add_notifier(C, NC_MASK | ND_DATA, &mask->id);
  }

  /* TODO: don't key all masks. */
  if (IS_AUTOKEY_ON(t->scene)) {
    Scene *scene = t->scene;

    if (ED_mask_layer_shape_auto_key_select(mask, scene->r.cfra)) {
      WM_event_add_notifier(C, NC_MASK | ND_DATA, &mask->id);
      DEG_id_tag_update(&mask->id, 0);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_Mask = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransMaskingData,
    /*recalc_data*/ recalcData_mask_common,
    /*special_aftertrans_update*/ special_aftertrans_update__mask,
};
