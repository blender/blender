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

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_task.h"

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_unit.h"

#include "ED_node.h"
#include "ED_screen.h"

#include "WM_api.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/** \name Transform (Translate) Custom Data
 * \{ */

/** Rotation may be enabled when snapping. */
enum eTranslateRotateMode {
  /** Don't rotate (default). */
  TRANSLATE_ROTATE_OFF = 0,
  /** Perform rotation (currently only snap to normal is used). */
  TRANSLATE_ROTATE_ON,
  /** Rotate, resetting back to the disabled state. */
  TRANSLATE_ROTATE_RESET,
};

/**
 * Custom data, stored in #TransInfo.custom.mode.data
 */
struct TranslateCustomData {
  /** Settings used in the last call to #applyTranslation. */
  struct {
    enum eTranslateRotateMode rotate_mode;
  } prev;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Translation) Element
 * \{ */

/**
 * \note Small arrays / data-structures should be stored copied for faster memory access.
 */
struct TransDataArgs_Translate {
  const TransInfo *t;
  const TransDataContainer *tc;
  const float pivot_local[3];
  const float vec[3];
  enum eTranslateRotateMode rotate_mode;
};

static void transdata_elem_translate(const TransInfo *t,
                                     const TransDataContainer *tc,
                                     TransData *td,
                                     const float pivot_local[3],
                                     const float vec[3],
                                     enum eTranslateRotateMode rotate_mode)
{
  float rotate_offset[3] = {0};
  bool use_rotate_offset = false;

  /* Handle snapping rotation before doing the translation. */
  if (rotate_mode != TRANSLATE_ROTATE_OFF) {
    float mat[3][3];

    if (rotate_mode == TRANSLATE_ROTATE_RESET) {
      unit_m3(mat);
    }
    else {
      BLI_assert(rotate_mode == TRANSLATE_ROTATE_ON);

      const float *original_normal;

      /* In pose mode, we want to align normals with Y axis of bones. */
      if (t->options & CTX_POSE_BONE) {
        original_normal = td->axismtx[1];
      }
      else {
        original_normal = td->axismtx[2];
      }

      rotation_between_vecs_to_mat3(mat, original_normal, t->tsnap.snapNormal);
    }

    ElementRotation_ex(t, tc, td, mat, pivot_local);

    if (td->loc) {
      use_rotate_offset = true;
      sub_v3_v3v3(rotate_offset, td->loc, td->iloc);
    }
  }

  float tvec[3];

  if (t->con.applyVec) {
    t->con.applyVec(t, tc, td, vec, tvec);
  }
  else {
    copy_v3_v3(tvec, vec);
  }

  mul_m3_v3(td->smtx, tvec);

  if (use_rotate_offset) {
    add_v3_v3(tvec, rotate_offset);
  }

  if (t->options & CTX_GPENCIL_STROKES) {
    /* Grease pencil multi-frame falloff. */
    bGPDstroke *gps = (bGPDstroke *)td->extra;
    if (gps != NULL) {
      mul_v3_fl(tvec, td->factor * gps->runtime.multi_frame_falloff);
    }
    else {
      mul_v3_fl(tvec, td->factor);
    }
  }
  else {
    /* Proportional editing falloff. */
    mul_v3_fl(tvec, td->factor);
  }

  protectedTransBits(td->protectflag, tvec);

  if (td->loc) {
    add_v3_v3v3(td->loc, td->iloc, tvec);
  }

  constraintTransLim(t, td);
}

static void transdata_elem_translate_fn(void *__restrict iter_data_v,
                                        const int iter,
                                        const TaskParallelTLS *__restrict UNUSED(tls))
{
  struct TransDataArgs_Translate *data = iter_data_v;
  TransData *td = &data->tc->data[iter];
  if (td->flag & TD_SKIP) {
    return;
  }
  transdata_elem_translate(data->t, data->tc, td, data->pivot_local, data->vec, data->rotate_mode);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform (Translation)
 * \{ */

static void translate_dist_to_str(char *r_str,
                                  const int len_max,
                                  const float val,
                                  const UnitSettings *unit)
{
  if (unit) {
    BKE_unit_value_as_string(
        r_str, len_max, val * unit->scale_length, 4, B_UNIT_LENGTH, unit, false);
  }
  else {
    /* Check range to prevent string buffer overflow. */
    BLI_snprintf(r_str, len_max, IN_RANGE_INCL(val, -1e10f, 1e10f) ? "%.4f" : "%.4e", val);
  }
}

static void headerTranslation(TransInfo *t, const float vec[3], char str[UI_MAX_DRAW_STR])
{
  size_t ofs = 0;
  char dvec_str[3][NUM_STR_REP_LEN];
  char dist_str[NUM_STR_REP_LEN];
  float dist;

  UnitSettings *unit = NULL;
  if (!(t->flag & T_2D_EDIT)) {
    unit = &t->scene->unit;
  }

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), dvec_str[0], &t->scene->unit);
    dist = len_v3(t->num.val);
  }
  else {
    float dvec[3];
    copy_v3_v3(dvec, vec);
    if (t->spacetype == SPACE_GRAPH) {
      /* WORKAROUND:
       * Special case where snapping is done in #recalData.
       * Update the header based on the #center_local. */
      const short autosnap = getAnimEdit_SnapMode(t);
      float ival = TRANS_DATA_CONTAINER_FIRST_OK(t)->center_local[0];
      float val = ival + dvec[0];
      snapFrameTransform(t, autosnap, ival, val, &val);
      dvec[0] = val - ival;
    }

    if (t->con.mode & CON_APPLY) {
      int i = 0;
      if (t->con.mode & CON_AXIS0) {
        dvec[i++] = dvec[0];
      }
      if (t->con.mode & CON_AXIS1) {
        dvec[i++] = dvec[1];
      }
      if (t->con.mode & CON_AXIS2) {
        dvec[i++] = dvec[2];
      }
      while (i != 3) {
        dvec[i++] = 0.0f;
      }
    }

    if (t->flag & T_2D_EDIT) {
      applyAspectRatio(t, dvec);
    }

    dist = len_v3(dvec);

    for (int i = 0; i < 3; i++) {
      translate_dist_to_str(dvec_str[i], sizeof(dvec_str[i]), dvec[i], unit);
    }
  }

  translate_dist_to_str(dist_str, sizeof(dist_str), dist, unit);

  if (t->flag & T_PROP_EDIT_ALL) {
    char prop_str[NUM_STR_REP_LEN];
    translate_dist_to_str(prop_str, sizeof(prop_str), t->prop_size, unit);

    ofs += BLI_snprintf_rlen(str + ofs,
                             UI_MAX_DRAW_STR - ofs,
                             "%s %s: %s   ",
                             TIP_("Proportional Size"),
                             t->proptext,
                             prop_str);
  }

  if (t->flag & T_AUTOIK) {
    short chainlen = t->settings->autoik_chainlen;
    if (chainlen) {
      ofs += BLI_snprintf_rlen(
          str + ofs, UI_MAX_DRAW_STR - ofs, TIP_("Auto IK Length: %d"), chainlen);
      ofs += BLI_strncpy_rlen(str + ofs, "   ", UI_MAX_DRAW_STR - ofs);
    }
  }

  if (t->con.mode & CON_APPLY) {
    switch (t->num.idx_max) {
      case 0:
        ofs += BLI_snprintf_rlen(
            str + ofs, UI_MAX_DRAW_STR - ofs, "D: %s (%s)%s", dvec_str[0], dist_str, t->con.text);
        break;
      case 1:
        ofs += BLI_snprintf_rlen(str + ofs,
                                 UI_MAX_DRAW_STR - ofs,
                                 "D: %s   D: %s (%s)%s",
                                 dvec_str[0],
                                 dvec_str[1],
                                 dist_str,
                                 t->con.text);
        break;
      case 2:
        ofs += BLI_snprintf_rlen(str + ofs,
                                 UI_MAX_DRAW_STR - ofs,
                                 "D: %s   D: %s   D: %s (%s)%s",
                                 dvec_str[0],
                                 dvec_str[1],
                                 dvec_str[2],
                                 dist_str,
                                 t->con.text);
        break;
    }
  }
  else {
    if (t->flag & T_2D_EDIT) {
      ofs += BLI_snprintf_rlen(str + ofs,
                               UI_MAX_DRAW_STR - ofs,
                               "Dx: %s   Dy: %s (%s)%s",
                               dvec_str[0],
                               dvec_str[1],
                               dist_str,
                               t->con.text);
    }
    else {
      ofs += BLI_snprintf_rlen(str + ofs,
                               UI_MAX_DRAW_STR - ofs,
                               "Dx: %s   Dy: %s   Dz: %s (%s)%s",
                               dvec_str[0],
                               dvec_str[1],
                               dvec_str[2],
                               dist_str,
                               t->con.text);
    }
  }

  if (t->spacetype == SPACE_NODE) {
    SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;

    if ((snode->flag & SNODE_SKIP_INSOFFSET) == 0) {
      const char *str_old = BLI_strdup(str);
      const char *str_dir = (snode->insert_ofs_dir == SNODE_INSERTOFS_DIR_RIGHT) ? TIP_("right") :
                                                                                   TIP_("left");
      char str_km[64];

      WM_modalkeymap_items_to_string(
          t->keymap, TFM_MODAL_INSERTOFS_TOGGLE_DIR, true, str_km, sizeof(str_km));

      ofs += BLI_snprintf_rlen(str,
                               UI_MAX_DRAW_STR,
                               TIP_("Auto-offset set to %s - press %s to toggle direction  |  %s"),
                               str_dir,
                               str_km,
                               str_old);

      MEM_freeN((void *)str_old);
    }
  }
}

static void ApplySnapTranslation(TransInfo *t, float vec[3])
{
  float point[3];
  getSnapPoint(t, point);

  if (t->spacetype == SPACE_NODE) {
    char border = t->tsnap.snapNodeBorder;
    if (border & (NODE_LEFT | NODE_RIGHT)) {
      vec[0] = point[0] - t->tsnap.snapTarget[0];
    }
    if (border & (NODE_BOTTOM | NODE_TOP)) {
      vec[1] = point[1] - t->tsnap.snapTarget[1];
    }
  }
  else if (t->spacetype == SPACE_SEQ) {
    transform_snap_sequencer_apply_translate(t, vec);
  }
  else {
    if (t->spacetype == SPACE_VIEW3D) {
      if (t->options & CTX_PAINT_CURVE) {
        if (ED_view3d_project_float_global(t->region, point, point, V3D_PROJ_TEST_NOP) !=
            V3D_PROJ_RET_OK) {
          zero_v3(point); /* no good answer here... */
        }
      }
    }

    sub_v3_v3v3(vec, point, t->tsnap.snapTarget);
  }
}

static void applyTranslationValue(TransInfo *t, const float vec[3])
{
  struct TranslateCustomData *custom_data = t->custom.mode.data;

  enum eTranslateRotateMode rotate_mode = TRANSLATE_ROTATE_OFF;

  if (activeSnap(t) && usingSnappingNormal(t) && validSnappingNormal(t)) {
    rotate_mode = TRANSLATE_ROTATE_ON;
  }

  /* Check to see if this needs to be re-enabled. */
  if (rotate_mode == TRANSLATE_ROTATE_OFF) {
    if (t->flag & T_POINTS) {
      /* When transforming points, only use rotation when snapping is enabled
       * since re-applying translation without rotation removes rotation. */
    }
    else {
      /* When transforming data that itself stores rotation (objects, bones etc),
       * apply rotation if it was applied (with the snap normal) previously.
       * This is needed because failing to rotate will leave the rotation at the last
       * value used before snapping was disabled. */
      if (custom_data->prev.rotate_mode == TRANSLATE_ROTATE_ON) {
        rotate_mode = TRANSLATE_ROTATE_RESET;
      }
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    float pivot_local[3];
    if (rotate_mode != TRANSLATE_ROTATE_OFF) {
      copy_v3_v3(pivot_local, t->tsnap.snapTarget);
      /* The pivot has to be in local-space (see T49494) */
      if (tc->use_local_mat) {
        mul_m4_v3(tc->imat, pivot_local);
      }
    }

    if (tc->data_len < TRANSDATA_THREAD_LIMIT) {
      TransData *td = tc->data;
      for (int i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_SKIP) {
          continue;
        }
        transdata_elem_translate(t, tc, td, pivot_local, vec, rotate_mode);
      }
    }
    else {
      struct TransDataArgs_Translate data = {
          .t = t,
          .tc = tc,
          .pivot_local = {UNPACK3(pivot_local)},
          .vec = {UNPACK3(vec)},
          .rotate_mode = rotate_mode,
      };
      TaskParallelSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      BLI_task_parallel_range(0, tc->data_len, &data, transdata_elem_translate_fn, &settings);
    }
  }

  custom_data->prev.rotate_mode = rotate_mode;
}

static void applyTranslation(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  float global_dir[3] = {0.0f};

  if (t->flag & T_INPUT_IS_VALUES_FINAL) {
    mul_v3_m3v3(global_dir, t->spacemtx, t->values);
  }
  else if (applyNumInput(&t->num, global_dir)) {
    if (t->con.mode & CON_APPLY) {
      if (t->con.mode & CON_AXIS0) {
        mul_v3_v3fl(global_dir, t->spacemtx[0], global_dir[0]);
      }
      else if (t->con.mode & CON_AXIS1) {
        mul_v3_v3fl(global_dir, t->spacemtx[1], global_dir[0]);
      }
      else if (t->con.mode & CON_AXIS2) {
        mul_v3_v3fl(global_dir, t->spacemtx[2], global_dir[0]);
      }
    }
    else {
      mul_v3_m3v3(global_dir, t->spacemtx, global_dir);
    }
    if (t->flag & T_2D_EDIT) {
      removeAspectRatio(t, global_dir);
    }
  }
  else {
    copy_v3_v3(global_dir, t->values);
    if (!is_zero_v3(t->values_modal_offset)) {
      float values_ofs[3];
      mul_v3_m3v3(values_ofs, t->spacemtx, t->values_modal_offset);
      add_v3_v3(global_dir, values_ofs);
    }

    t->tsnap.snapElem = 0;
    applySnapping(t, global_dir);
    transform_snap_grid(t, global_dir);

    if (t->con.mode & CON_APPLY) {
      float in[3];
      copy_v3_v3(in, global_dir);
      t->con.applyVec(t, NULL, NULL, in, global_dir);
    }

    float incr_dir[3];
    copy_v3_v3(incr_dir, global_dir);
    if (!(activeSnap(t) && validSnap(t)) &&
        transform_snap_increment_ex(t, (t->con.mode & CON_APPLY) != 0, incr_dir)) {

      /* Test for mixed snap with grid. */
      float snap_dist_sq = FLT_MAX;
      if (t->tsnap.snapElem != 0) {
        snap_dist_sq = len_squared_v3v3(t->values, global_dir);
      }
      if ((snap_dist_sq == FLT_MAX) || (len_squared_v3v3(global_dir, incr_dir) < snap_dist_sq)) {
        copy_v3_v3(global_dir, incr_dir);
      }
    }
  }

  applyTranslationValue(t, global_dir);

  /* evil hack - redo translation if clipping needed */
  if (t->flag & T_CLIP_UV && clipUVTransform(t, global_dir, 0)) {
    applyTranslationValue(t, global_dir);

    /* In proportional edit it can happen that */
    /* vertices in the radius of the brush end */
    /* outside the clipping area               */
    /* XXX HACK - dg */
    if (t->flag & T_PROP_EDIT) {
      clipUVData(t);
    }
  }

  /* Set the redo value. */
  mul_v3_m3v3(t->values_final, t->spacemtx_inv, global_dir);
  headerTranslation(t, (t->con.mode & CON_APPLY) ? t->values_final : global_dir, str);

  recalcData(t);
  ED_area_status_text(t->area, str);
}

void initTranslation(TransInfo *t)
{
  if (t->spacetype == SPACE_ACTION) {
    /* this space uses time translate */
    BKE_report(t->reports,
               RPT_ERROR,
               "Use 'Time_Translate' transform mode instead of 'Translation' mode "
               "for translating keyframes in Dope Sheet Editor");
    t->state = TRANS_CANCEL;
  }

  t->transform = applyTranslation;
  t->tsnap.applySnap = ApplySnapTranslation;
  t->tsnap.distance = transform_snap_distance_len_squared_fn;

  initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

  t->idx_max = (t->flag & T_2D_EDIT) ? 1 : 2;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  copy_v2_v2(t->snap, t->snap_spatial);

  copy_v3_fl(t->num.val_inc, t->snap[0]);
  t->num.unit_sys = t->scene->unit.system;
  if (t->spacetype == SPACE_VIEW3D) {
    /* Handling units makes only sense in 3Dview... See T38877. */
    t->num.unit_type[0] = B_UNIT_LENGTH;
    t->num.unit_type[1] = B_UNIT_LENGTH;
    t->num.unit_type[2] = B_UNIT_LENGTH;
  }
  else {
    /* SPACE_GRAPH, SPACE_ACTION, etc. could use some time units, when we have them... */
    t->num.unit_type[0] = B_UNIT_NONE;
    t->num.unit_type[1] = B_UNIT_NONE;
    t->num.unit_type[2] = B_UNIT_NONE;
  }

  transform_mode_default_modal_orientation_set(
      t, (t->options & CTX_CAMERA) ? V3D_ORIENT_VIEW : V3D_ORIENT_GLOBAL);

  struct TranslateCustomData *custom_data = MEM_callocN(sizeof(*custom_data), __func__);
  custom_data->prev.rotate_mode = TRANSLATE_ROTATE_OFF;
  t->custom.mode.data = custom_data;
  t->custom.mode.use_free = true;
}

/** \} */
