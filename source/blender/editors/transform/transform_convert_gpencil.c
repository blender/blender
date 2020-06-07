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

#include "DNA_gpencil_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_gpencil.h"

#include "ED_gpencil.h"

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name Gpencil Transform Creation
 *
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

void createTransGPencil(bContext *C, TransInfo *t)
{
  if (t->data_container_len == 0) {
    return;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  bGPdata *gpd = ED_gpencil_data_get_active(C);
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd);
  bool use_multiframe_falloff = (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_FRAME_FALLOFF) != 0;

  Object *obact = CTX_data_active_object(C);
  bGPDlayer *gpl;
  TransData *td = NULL;
  float mtx[3][3], smtx[3][3];

  const Scene *scene = CTX_data_scene(C);
  const int cfra_scene = CFRA;

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_edit_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_scale_thickness = ((t->mode == TFM_GPENCIL_SHRINKFATTEN) ||
                                   (ts->gp_sculpt.flag & GP_SCULPT_SETT_FLAG_SCALE_THICKNESS));

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* == Grease Pencil Strokes to Transform Data ==
   * Grease Pencil stroke points can be a mixture of 2D (screen-space),
   * or 3D coordinates. However, they're always saved as 3D points.
   * For now, we just do these without creating TransData2D for the 2D
   * strokes. This may cause issues in future though.
   */
  tc->data_len = 0;

  if (gpd == NULL) {
    return;
  }

  /* initialize falloff curve */
  if (is_multiedit) {
    BKE_curvemapping_initialize(ts->gp_sculpt.cur_falloff);
  }

  /* First Pass: Count the number of data-points required for the strokes,
   * (and additional info about the configuration - e.g. 2D/3D?).
   */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      bGPDframe *gpf;
      bGPDstroke *gps;
      bGPDframe *init_gpf = gpl->actframe;
      if (is_multiedit) {
        init_gpf = gpl->frames.first;
      }

      for (gpf = init_gpf; gpf; gpf = gpf->next) {
        if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
          for (gps = gpf->strokes.first; gps; gps = gps->next) {
            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
              continue;
            }

            if (is_prop_edit) {
              /* Proportional Editing... */
              if (is_prop_edit_connected) {
                /* connected only - so only if selected */
                if (gps->flag & GP_STROKE_SELECT) {
                  tc->data_len += gps->totpoints;
                }
              }
              else {
                /* everything goes - connection status doesn't matter */
                tc->data_len += gps->totpoints;
              }
            }
            else {
              /* only selected stroke points are considered */
              if (gps->flag & GP_STROKE_SELECT) {
                bGPDspoint *pt;
                int i;

                // TODO: 2D vs 3D?
                for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                  if (pt->flag & GP_SPOINT_SELECT) {
                    tc->data_len++;
                  }
                }
              }
            }
          }
        }
        /* if not multiedit out of loop */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }

  /* Stop trying if nothing selected */
  if (tc->data_len == 0) {
    return;
  }

  /* Allocate memory for data */
  tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransData(GPencil)");
  td = tc->data;

  unit_m3(smtx);
  unit_m3(mtx);

  /* Second Pass: Build transdata array */
  for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
    /* only editable and visible layers are considered */
    if (BKE_gpencil_layer_is_editable(gpl) && (gpl->actframe != NULL)) {
      const int cfra = (gpl->flag & GP_LAYER_FRAMELOCK) ? gpl->actframe->framenum : cfra_scene;
      bGPDframe *gpf = gpl->actframe;
      bGPDstroke *gps;
      float diff_mat[4][4];
      float inverse_diff_mat[4][4];

      bGPDframe *init_gpf = gpl->actframe;
      if (is_multiedit) {
        init_gpf = gpl->frames.first;
      }
      /* init multiframe falloff options */
      int f_init = 0;
      int f_end = 0;

      if (use_multiframe_falloff) {
        BKE_gpencil_frame_range_selected(gpl, &f_init, &f_end);
      }

      /* calculate difference matrix */
      BKE_gpencil_parent_matrix_get(depsgraph, obact, gpl, diff_mat);
      /* undo matrix */
      invert_m4_m4(inverse_diff_mat, diff_mat);

      /* Make a new frame to work on if the layer's frame
       * and the current scene frame don't match up.
       *
       * - This is useful when animating as it saves that "uh-oh" moment when you realize you've
       *   spent too much time editing the wrong frame...
       */
      // XXX: should this be allowed when framelock is enabled?
      if ((gpf->framenum != cfra) && (!is_multiedit)) {
        gpf = BKE_gpencil_frame_addcopy(gpl, cfra);
        /* in some weird situations (framelock enabled) return NULL */
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

          for (gps = gpf->strokes.first; gps; gps = gps->next) {
            TransData *head = td;
            TransData *tail = td;
            bool stroke_ok;

            /* skip strokes that are invalid for current view */
            if (ED_gpencil_stroke_can_use(C, gps) == false) {
              continue;
            }
            /* check if the color is editable */
            if (ED_gpencil_stroke_color_use(obact, gpl, gps) == false) {
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
                  /* Always all points in strokes that get included */
                  point_ok = true;
                }
                else {
                  /* Only selected points in selected strokes */
                  point_ok = (pt->flag & GP_SPOINT_SELECT) != 0;
                }

                /* do point... */
                if (point_ok) {
                  copy_v3_v3(td->iloc, &pt->x);
                  /* only copy center in local origins.
                   * This allows get interesting effects also when move
                   * using proportional editing */
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

                  /* for other transform modes (e.g. shrink-fatten), need to additional data
                   * but never for mirror
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

                  /* screenspace needs special matrices... */
                  if ((gps->flag & (GP_STROKE_3DSPACE | GP_STROKE_2DSPACE | GP_STROKE_2DIMAGE)) ==
                      0) {
                    /* screenspace */
                    td->protectflag = OB_LOCK_LOCZ | OB_LOCK_ROTZ | OB_LOCK_SCALEZ;
                  }
                  else {
                    /* configure 2D dataspace points so that they don't play up... */
                    if (gps->flag & (GP_STROKE_2DSPACE | GP_STROKE_2DIMAGE)) {
                      td->protectflag = OB_LOCK_LOCZ | OB_LOCK_ROTZ | OB_LOCK_SCALEZ;
                    }
                  }
                  /* apply parent transformations */
                  copy_m3_m4(td->smtx, inverse_diff_mat); /* final position */
                  copy_m3_m4(td->mtx, diff_mat);          /* display position */
                  copy_m3_m4(td->axismtx, diff_mat);      /* axis orientation */

                  /* Triangulation must be calculated again,
                   * so save the stroke for recalc function */
                  td->extra = gps;

                  /* save pointer to object */
                  td->ob = obact;

                  td++;
                  tail++;
                }
              }

              /* March over these points, and calculate the proportional editing distances */
              if (is_prop_edit && (head != tail)) {
                /* XXX: for now, we are similar enough that this works... */
                calc_distanceCurveVerts(head, tail - 1);
              }
            }
          }
        }
        /* if not multiedit out of loop */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
}

/** \} */
