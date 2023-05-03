/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#include "DNA_gpencil_legacy_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_gpencil_curve_legacy.h"
#include "BKE_gpencil_geom_legacy.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_layer.h"

#include "ED_gpencil_legacy.h"
#include "ED_keyframing.h"

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Gpencil Transform Creation
 * \{ */

static void createTransGPencil_center_get(bGPDstroke *gps, float r_center[3])
{
  bGPDspoint *pt;
  int i;

  zero_v3(r_center);
  int tot_sel = 0;
  for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
    if (pt->flag & GP_SPOINT_SELECT) {
      add_v3_v3(r_center, &pt->x);
      tot_sel++;
    }
  }

  if (tot_sel > 0) {
    mul_v3_fl(r_center, 1.0f / tot_sel);
  }
}

static short get_bezt_sel_triple_flag(BezTriple *bezt, const bool handles_visible)
{
#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)
#define SEL_ALL ((1 << 0) | (1 << 1) | (1 << 2))

  short flag = 0;

  if (handles_visible) {
    flag = ((bezt->f1 & SELECT) ? SEL_F1 : 0) | ((bezt->f2 & SELECT) ? SEL_F2 : 0) |
           ((bezt->f3 & SELECT) ? SEL_F3 : 0);
  }
  else if (bezt->f2 & SELECT) {
    flag = SEL_ALL;
  }

  /* Special case for auto & aligned handles */
  if ((flag != SEL_ALL) && (flag & SEL_F2)) {
    if (ELEM(bezt->h1, HD_AUTO, HD_ALIGN) && ELEM(bezt->h2, HD_AUTO, HD_ALIGN)) {
      flag = SEL_ALL;
    }
  }

#undef SEL_F1
#undef SEL_F2
#undef SEL_F3
  return flag;
}

static void createTransGPencil_curves(bContext *C,
                                      TransInfo *t,
                                      Depsgraph *depsgraph,
                                      ToolSettings *ts,
                                      Object *obact,
                                      bGPdata *gpd,
                                      const int cfra_scene,
                                      const bool is_multiedit,
                                      const bool use_multiframe_falloff,
                                      const bool is_prop_edit,
                                      const bool is_prop_edit_connected,
                                      const bool is_scale_thickness)
{
#define SEL_F1 (1 << 0)
#define SEL_F2 (1 << 1)
#define SEL_F3 (1 << 2)

  View3D *v3d = t->view;
  Scene *scene = CTX_data_scene(C);
  const bool handle_only_selected_visible = (v3d->overlay.handle_display == CURVE_HANDLE_SELECTED);
  const bool handle_all_visible = (v3d->overlay.handle_display == CURVE_HANDLE_ALL);

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  tc->data_len = 0;

  /* Number of selected curve points */
  uint32_t tot_curve_points = 0, tot_sel_curve_points = 0, tot_points = 0, tot_sel_points = 0;
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Only editable and visible layers are considered. */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
      for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* Check if the color is editable. */
            if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
              continue;
            }
            /* Check if stroke has an editcurve */
            if (gps->editcurve == NULL) {
              continue;
            }

            bGPDcurve *gpc = gps->editcurve;
            for (int i = 0; i < gpc->tot_curve_points; i++) {
              bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
              BezTriple *bezt = &gpc_pt->bezt;
              if (bezt->hide) {
                continue;
              }

              const bool handles_visible = (handle_all_visible ||
                                            (handle_only_selected_visible &&
                                             (gpc_pt->flag & GP_CURVE_POINT_SELECT)));

              const short sel_flag = get_bezt_sel_triple_flag(bezt, handles_visible);
              if (sel_flag & (SEL_F1 | SEL_F2 | SEL_F3)) {
                if (sel_flag & SEL_F1) {
                  tot_sel_points++;
                }
                if (sel_flag & SEL_F2) {
                  tot_sel_points++;
                }
                if (sel_flag & SEL_F3) {
                  tot_sel_points++;
                }
                tot_sel_curve_points++;
              }

              if (is_prop_edit) {
                tot_points += 3;
                tot_curve_points++;
              }
            }
          }
        }

        /* If not multi-edit out of loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }

  if (((is_prop_edit && !is_prop_edit_connected) ? tot_curve_points : tot_sel_points) == 0) {
    tc->data_len = 0;
    return;
  }

  int data_len_pt = 0;

  if (is_prop_edit) {
    tc->data_len = tot_points;
    data_len_pt = tot_curve_points;
  }
  else {
    tc->data_len = tot_sel_points;
    data_len_pt = tot_sel_curve_points;
  }

  if (tc->data_len == 0) {
    return;
  }

  transform_around_single_fallback_ex(t, data_len_pt);

  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), __func__);
  TransData *td = tc->data;

  const bool use_around_origins_for_handles_test = ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                                    transform_mode_use_local_origins(t));

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Only editable and visible layers are considered. */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      const int cfra = (gpl->flag & GP_LAYER_FRAMELOCK) ? gpl->actframe->framenum : cfra_scene;
      bGPDframe *gpf = gpl->actframe;
      bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
      float diff_mat[4][4], mtx[3][3];
      float smtx[3][3];

      /* Init multiframe falloff options. */
      int f_init = 0;
      int f_end = 0;

      if (use_multiframe_falloff) {
        BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
      }

      if ((gpf->framenum != cfra) && (!is_multiedit)) {
        if (IS_AUTOKEY_ON(scene)) {
          gpf = BKE_gpencil_frame_addcopy(gpl, cfra);
        }
        /* In some weird situations (frame-lock enabled) return NULL. */
        if (gpf == NULL) {
          continue;
        }
        if (!is_multiedit) {
          init_gpf = gpf;
        }
      }

      /* Calculate difference matrix. */
      BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat);
      copy_m3_m4(mtx, diff_mat);
      pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          /* If multi-frame and falloff, recalculate and save value. */
          float falloff = 1.0f; /* by default no falloff */
          if ((is_multiedit) && (use_multiframe_falloff)) {
            /* Falloff depends on distance to active frame
             * (relative to the overall frame range). */
            falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }

          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* Check if the color is editable. */
            if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
              continue;
            }
            /* Check if stroke has an editcurve */
            if (gps->editcurve == NULL) {
              continue;
            }
            TransData *head, *tail;
            head = tail = td;

            gps->runtime.multi_frame_falloff = falloff;
            bool need_handle_recalc = false;

            bGPDcurve *gpc = gps->editcurve;
            const bool is_cyclic = gps->flag & GP_STROKE_CYCLIC;
            for (int i = 0; i < gpc->tot_curve_points; i++) {
              bGPDcurve_point *gpc_pt = &gpc->curve_points[i];
              BezTriple *bezt = &gpc_pt->bezt;
              if (bezt->hide) {
                continue;
              }

              TransDataCurveHandleFlags *hdata = NULL;
              bool bezt_use = false;
              const bool handles_visible = (handle_all_visible ||
                                            (handle_only_selected_visible &&
                                             (gpc_pt->flag & GP_CURVE_POINT_SELECT)));
              const short sel_flag = get_bezt_sel_triple_flag(bezt, handles_visible);
              /* Iterate over bezier triple */
              for (int j = 0; j < 3; j++) {
                bool is_ctrl_point = (j == 1);
                bool sel = sel_flag & (1 << j);

                if (is_prop_edit || sel) {
                  copy_v3_v3(td->iloc, bezt->vec[j]);
                  td->loc = bezt->vec[j];
                  bool rotate_around_ctrl = !handles_visible ||
                                            (t->around == V3D_AROUND_LOCAL_ORIGINS) ||
                                            (bezt->f2 & SELECT);
                  copy_v3_v3(td->center, bezt->vec[rotate_around_ctrl ? 1 : j]);

                  if (!handles_visible || is_ctrl_point) {
                    if (bezt->f2 & SELECT) {
                      td->flag = TD_SELECTED;
                    }
                    else {
                      td->flag = 0;
                    }
                  }
                  else if (handles_visible) {
                    if (sel) {
                      td->flag = TD_SELECTED;
                    }
                    else {
                      td->flag = 0;
                    }
                  }

                  td->ext = NULL;
                  if (is_ctrl_point) {
                    if (t->mode != TFM_MIRROR) {
                      if (t->mode != TFM_GPENCIL_OPACITY) {
                        if (is_scale_thickness) {
                          td->val = &(gpc_pt->pressure);
                          td->ival = gpc_pt->pressure;
                        }
                      }
                      else {
                        td->val = &(gpc_pt->strength);
                        td->ival = gpc_pt->strength;
                      }
                    }
                  }
                  else {
                    td->val = NULL;
                  }

                  if (hdata == NULL) {
                    if (is_ctrl_point && ((sel_flag & SEL_F1 & SEL_F3) == 0)) {
                      hdata = initTransDataCurveHandles(td, bezt);
                    }
                    else if (!is_ctrl_point) {
                      hdata = initTransDataCurveHandles(td, bezt);
                    }
                  }

                  td->extra = gps;
                  td->ob = obact;

                  copy_m3_m3(td->smtx, smtx);
                  copy_m3_m3(td->mtx, mtx);
                  copy_m3_m3(td->axismtx, mtx);

                  td++;
                  tail++;
                }

                bezt_use |= sel;
              }

              /* Update the handle types so transformation is possible */
              if (bezt_use && !ELEM(t->mode, TFM_GPENCIL_OPACITY, TFM_GPENCIL_SHRINKFATTEN)) {
                BKE_nurb_bezt_handle_test(
                    bezt, SELECT, handles_visible, use_around_origins_for_handles_test);
                need_handle_recalc = true;
              }
            }

            if (is_prop_edit && (head != tail)) {
              calc_distanceCurveVerts(head, tail - 1, is_cyclic);
            }

            if (need_handle_recalc) {
              BKE_gpencil_editcurve_recalculate_handles(gps);
            }
          }
        }

        /* If not multi-edit out of loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
#undef SEL_F1
#undef SEL_F2
#undef SEL_F3
}

static void createTransGPencil_strokes(bContext *C,
                                       TransInfo *t,
                                       Depsgraph *depsgraph,
                                       ToolSettings *ts,
                                       Object *obact,
                                       bGPdata *gpd,
                                       const int cfra_scene,
                                       const bool is_multiedit,
                                       const bool use_multiframe_falloff,
                                       const bool is_prop_edit,
                                       const bool is_prop_edit_connected,
                                       const bool is_scale_thickness)
{
  Scene *scene = CTX_data_scene(C);
  TransData *td = NULL;
  float mtx[3][3], smtx[3][3];

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  /* == Grease Pencil Strokes to Transform Data ==
   * Grease Pencil stroke points can be a mixture of 2D (screen-space),
   * or 3D coordinates. However, they're always saved as 3D points.
   * For now, we just do these without creating TransData2D for the 2D
   * strokes. This may cause issues in future though.
   */
  tc->data_len = 0;

  /* First Pass: Count the number of data-points required for the strokes,
   * (and additional info about the configuration - e.g. 2D/3D?).
   */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* Only editable and visible layers are considered. */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf;
      bGPDstroke *gps;
      bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;

      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          for (gps = gpf->strokes.first; gps; gps = gps->next) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* Check if the color is editable. */
            if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
              continue;
            }

            if (is_prop_edit) {
              /* Proportional Editing... */
              if (is_prop_edit_connected) {
                /* Connected only - so only if selected. */
                if (gps->flag & GP_STROKE_SELECT) {
                  tc->data_len += gps->totpoints;
                }
              }
              else {
                /* Everything goes - connection status doesn't matter. */
                tc->data_len += gps->totpoints;
              }
            }
            else {
              /* Only selected stroke points are considered. */
              if (gps->flag & GP_STROKE_SELECT) {
                bGPDspoint *pt;
                int i;

                for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                  if (pt->flag & GP_SPOINT_SELECT) {
                    tc->data_len++;
                  }
                }
              }
            }
          }
        }
        /* If not multi-edit out of loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }

  /* Stop trying if nothing selected. */
  if (tc->data_len == 0) {
    return;
  }

  /* Allocate memory for data */
  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(GPencil)");
  td = tc->data;

  unit_m3(smtx);
  unit_m3(mtx);

  /* Second Pass: Build transdata array. */
  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      const int cfra = (gpl->flag & GP_LAYER_FRAMELOCK) ? gpl->actframe->framenum : cfra_scene;
      bGPDframe *gpf = gpl->actframe;
      float diff_mat[3][3];
      float inverse_diff_mat[3][3];

      bGPDframe *init_gpf = (is_multiedit) ? gpl->frames.first : gpl->actframe;
      /* Init multiframe falloff options. */
      int f_init = 0;
      int f_end = 0;

      if (use_multiframe_falloff) {
        BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
      }

      /* Calculate difference matrix. */
      {
        float diff_mat_tmp[4][4];
        BKE_gpencil_layer_transform_matrix_get(depsgraph, obact, gpl, diff_mat_tmp);
        copy_m3_m4(diff_mat, diff_mat_tmp);
      }

      /* Use safe invert for cases where the input matrix has zero axes. */
      invert_m3_m3_safe_ortho(inverse_diff_mat, diff_mat);

      /* Make a new frame to work on if the layer's frame
       * and the current scene frame don't match up.
       *
       * - This is useful when animating as it saves that "uh-oh" moment when you realize you've
       *   spent too much time editing the wrong frame...
       */
      if ((gpf->framenum != cfra) && (!is_multiedit)) {
        if (IS_AUTOKEY_ON(scene)) {
          gpf = BKE_gpencil_frame_addcopy(gpl, cfra);
        }
        /* In some weird situations (frame-lock enabled) return NULL. */
        if (gpf == NULL) {
          continue;
        }
        if (!is_multiedit) {
          init_gpf = gpf;
        }
      }

      /* Loop over strokes, adding TransData for points as needed... */
      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {

          /* If multi-frame and falloff, recalculate and save value. */
          float falloff = 1.0f; /* by default no falloff */
          if ((is_multiedit) && (use_multiframe_falloff)) {
            /* Falloff depends on distance to active frame
             * (relative to the overall frame range). */
            falloff = BKE_gpencil_multiframe_falloff_calc(
                gpf, gpl->actframe->framenum, f_init, f_end, ts->gp_sculpt.cur_falloff);
          }

          LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
            TransData *head = td;
            TransData *tail = td;
            bool stroke_ok;

            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_material_editable(obact, gpl, gps) == false) {
              continue;
            }
            /* What we need to include depends on proportional editing settings... */
            if (is_prop_edit) {
              if (is_prop_edit_connected) {
                /* A) "Connected" - Only those in selected strokes */
                stroke_ok = (gps->flag & GP_STROKE_SELECT) != 0;
              }
              else {
                /* B) All points, always */
                stroke_ok = true;
              }
            }
            else {
              /* C) Only selected points in selected strokes */
              stroke_ok = (gps->flag & GP_STROKE_SELECT) != 0;
            }

            /* Do stroke... */
            if (stroke_ok && gps->totpoints) {
              bGPDspoint *pt;
              int i;

              /* save falloff factor */
              gps->runtime.multi_frame_falloff = falloff;

              /* calculate stroke center */
              float center[3];
              createTransGPencil_center_get(gps, center);

              /* add all necessary points... */
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                bool point_ok;

                /* include point? */
                if (is_prop_edit) {
                  /* Always all points in strokes that get included. */
                  point_ok = true;
                }
                else {
                  /* Only selected points in selected strokes. */
                  point_ok = (pt->flag & GP_SPOINT_SELECT) != 0;
                }

                /* do point... */
                if (point_ok) {
                  copy_v3_v3(td->iloc, &pt->x);
                  /* Only copy center in local origins.
                   * This allows get interesting effects also when move
                   * using proportional editing. */
                  if ((gps->flag & GP_STROKE_SELECT) &&
                      (ts->transform_pivot_point == V3D_AROUND_LOCAL_ORIGINS)) {
                    copy_v3_v3(td->center, center);
                  }
                  else {
                    copy_v3_v3(td->center, &pt->x);
                  }

                  td->loc = &pt->x;

                  td->flag = 0;

                  if (pt->flag & GP_SPOINT_SELECT) {
                    td->flag |= TD_SELECTED;
                  }

                  /* For other transform modes (e.g. shrink-fatten), need to additional data
                   * but never for mirror.
                   */
                  if (t->mode != TFM_MIRROR) {
                    if (t->mode != TFM_GPENCIL_OPACITY) {
                      if (is_scale_thickness) {
                        td->val = &pt->pressure;
                        td->ival = pt->pressure;
                      }
                    }
                    else {
                      td->val = &pt->strength;
                      td->ival = pt->strength;
                    }
                  }

                  /* Screen-space needs special matrices. */
                  if ((gps->flag & (GP_STROKE_3DSPACE | GP_STROKE_2DSPACE | GP_STROKE_2DIMAGE)) ==
                      0) {
                    /* Screen-space. */
                    td->protectflag = OB_LOCK_LOCZ | OB_LOCK_ROTZ | OB_LOCK_SCALEZ;
                  }
                  else {
                    /* configure 2D data-space points so that they don't play up. */
                    if (gps->flag & (GP_STROKE_2DSPACE | GP_STROKE_2DIMAGE)) {
                      td->protectflag = OB_LOCK_LOCZ | OB_LOCK_ROTZ | OB_LOCK_SCALEZ;
                    }
                  }
                  /* apply parent transformations */
                  copy_m3_m3(td->smtx, inverse_diff_mat); /* final position */
                  copy_m3_m3(td->mtx, diff_mat);          /* display position */
                  copy_m3_m3(td->axismtx, diff_mat);      /* axis orientation */

                  /* Triangulation must be calculated again,
                   * so save the stroke for recalculate function. */
                  td->extra = gps;

                  /* Save pointer to object. */
                  td->ob = obact;

                  td++;
                  tail++;
                }
              }

              /* March over these points, and calculate the proportional editing distances. */
              if (is_prop_edit && (head != tail)) {
                calc_distanceCurveVerts(head, tail - 1, false);
              }
            }
          }
        }
        /* If not multi-edit out of loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
}

static void createTransGPencil(bContext *C, TransInfo *t)
{
  if (t->data_container_len == 0) {
    return;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  BKE_view_layer_synced_ensure(t->scene, t->view_layer);
  Object *obact = BKE_view_layer_active_object_get(t->view_layer);
  bGPdata *gpd = obact->data;
  BLI_assert(gpd != NULL);

  const int cfra_scene = scene->r.cfra;

  const bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  const bool use_multiframe_falloff = (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) !=
                                      0;

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_edit_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_scale_thickness = ((t->mode == TFM_GPENCIL_SHRINKFATTEN) ||
                                   (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_SCALE_THICKNESS));

  const bool is_curve_edit = (bool)GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd);

  /* initialize falloff curve */
  if (is_multiedit) {
    BKE_curvemapping_init(ts->gp_sculpt.cur_falloff);
  }

  if (gpd == NULL) {
    return;
  }

  if (is_curve_edit) {
    createTransGPencil_curves(C,
                              t,
                              depsgraph,
                              ts,
                              obact,
                              gpd,
                              cfra_scene,
                              is_multiedit,
                              use_multiframe_falloff,
                              is_prop_edit,
                              is_prop_edit_connected,
                              is_scale_thickness);
  }
  else {
    createTransGPencil_strokes(C,
                               t,
                               depsgraph,
                               ts,
                               obact,
                               gpd,
                               cfra_scene,
                               is_multiedit,
                               use_multiframe_falloff,
                               is_prop_edit,
                               is_prop_edit_connected,
                               is_scale_thickness);
  }
}

static void recalcData_gpencil_strokes(TransInfo *t)
{
  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);
  GHash *strokes = BLI_ghash_ptr_new(__func__);

  TransData *td = tc->data;
  bGPdata *gpd = td->ob->data;
  const bool is_curve_edit = (bool)GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd);
  for (int i = 0; i < tc->data_len; i++, td++) {
    bGPDstroke *gps = td->extra;

    if ((gps != NULL) && !BLI_ghash_haskey(strokes, gps)) {
      BLI_ghash_insert(strokes, gps, gps);
      if (is_curve_edit && gps->editcurve != NULL) {
        BKE_gpencil_editcurve_recalculate_handles(gps);
        gps->flag |= GP_STROKE_NEEDS_CURVE_UPDATE;
      }
      /* Calc geometry data. */
      BKE_gpencil_stroke_geometry_update(gpd, gps);
    }
  }
  BLI_ghash_free(strokes, NULL, NULL);
}

/** \} */

TransConvertTypeInfo TransConvertType_GPencil = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*createTransData*/ createTransGPencil,
    /*recalcData*/ recalcData_gpencil_strokes,
    /*special_aftertrans_update*/ NULL,
};
