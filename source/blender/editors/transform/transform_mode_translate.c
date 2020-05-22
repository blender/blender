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

#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_unit.h"

#include "ED_screen.h"

#include "WM_api.h"

#include "UI_interface.h"

#include "BLT_translation.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/* Transform (Translation) */

/** \name Transform Translation
 * \{ */

static void headerTranslation(TransInfo *t, const float vec[3], char str[UI_MAX_DRAW_STR])
{
  size_t ofs = 0;
  char tvec[NUM_STR_REP_LEN * 3];
  char distvec[NUM_STR_REP_LEN];
  char autoik[NUM_STR_REP_LEN];
  float dist;

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
    dist = len_v3(t->num.val);
  }
  else {
    float dvec[3];

    copy_v3_v3(dvec, vec);
    applyAspectRatio(t, dvec);

    dist = len_v3(vec);
    if (!(t->flag & T_2D_EDIT) && t->scene->unit.system) {
      int i;

      for (i = 0; i < 3; i++) {
        bUnit_AsString2(&tvec[NUM_STR_REP_LEN * i],
                        NUM_STR_REP_LEN,
                        dvec[i] * t->scene->unit.scale_length,
                        4,
                        B_UNIT_LENGTH,
                        &t->scene->unit,
                        true);
      }
    }
    else {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", dvec[0]);
      BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", dvec[1]);
      BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", dvec[2]);
    }
  }

  if (!(t->flag & T_2D_EDIT) && t->scene->unit.system) {
    bUnit_AsString2(distvec,
                    sizeof(distvec),
                    dist * t->scene->unit.scale_length,
                    4,
                    B_UNIT_LENGTH,
                    &t->scene->unit,
                    false);
  }
  else if (dist > 1e10f || dist < -1e10f) {
    /* prevent string buffer overflow */
    BLI_snprintf(distvec, NUM_STR_REP_LEN, "%.4e", dist);
  }
  else {
    BLI_snprintf(distvec, NUM_STR_REP_LEN, "%.4f", dist);
  }

  if (t->flag & T_AUTOIK) {
    short chainlen = t->settings->autoik_chainlen;

    if (chainlen) {
      BLI_snprintf(autoik, NUM_STR_REP_LEN, TIP_("AutoIK-Len: %d"), chainlen);
    }
    else {
      autoik[0] = '\0';
    }
  }
  else {
    autoik[0] = '\0';
  }

  if (t->con.mode & CON_APPLY) {
    switch (t->num.idx_max) {
      case 0:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            "D: %s (%s)%s %s  %s",
                            &tvec[0],
                            distvec,
                            t->con.text,
                            t->proptext,
                            autoik);
        break;
      case 1:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            "D: %s   D: %s (%s)%s %s  %s",
                            &tvec[0],
                            &tvec[NUM_STR_REP_LEN],
                            distvec,
                            t->con.text,
                            t->proptext,
                            autoik);
        break;
      case 2:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            "D: %s   D: %s  D: %s (%s)%s %s  %s",
                            &tvec[0],
                            &tvec[NUM_STR_REP_LEN],
                            &tvec[NUM_STR_REP_LEN * 2],
                            distvec,
                            t->con.text,
                            t->proptext,
                            autoik);
        break;
    }
  }
  else {
    if (t->flag & T_2D_EDIT) {
      ofs += BLI_snprintf(str + ofs,
                          UI_MAX_DRAW_STR - ofs,
                          "Dx: %s   Dy: %s (%s)%s %s",
                          &tvec[0],
                          &tvec[NUM_STR_REP_LEN],
                          distvec,
                          t->con.text,
                          t->proptext);
    }
    else {
      ofs += BLI_snprintf(str + ofs,
                          UI_MAX_DRAW_STR - ofs,
                          "Dx: %s   Dy: %s  Dz: %s (%s)%s %s  %s",
                          &tvec[0],
                          &tvec[NUM_STR_REP_LEN],
                          &tvec[NUM_STR_REP_LEN * 2],
                          distvec,
                          t->con.text,
                          t->proptext,
                          autoik);
    }
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
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

      ofs += BLI_snprintf(str,
                          UI_MAX_DRAW_STR,
                          TIP_("Auto-offset set to %s - press %s to toggle direction  |  %s"),
                          str_dir,
                          str_km,
                          str_old);

      MEM_freeN((void *)str_old);
    }
  }
}

static void applyTranslationValue(TransInfo *t, const float vec[3])
{
  const bool apply_snap_align_rotation = usingSnappingNormal(
      t);  // && (t->tsnap.status & POINT_INIT);
  float tvec[3];

  /* The ideal would be "apply_snap_align_rotation" only when a snap point is found
   * so, maybe inside this function is not the best place to apply this rotation.
   * but you need "handle snapping rotation before doing the translation" (really?) */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    float pivot[3];
    if (apply_snap_align_rotation) {
      copy_v3_v3(pivot, t->tsnap.snapTarget);
      /* The pivot has to be in local-space (see T49494) */
      if (tc->use_local_mat) {
        mul_m4_v3(tc->imat, pivot);
      }
    }

    TransData *td = tc->data;
    for (int i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_SKIP) {
        continue;
      }

      float rotate_offset[3] = {0};
      bool use_rotate_offset = false;

      /* handle snapping rotation before doing the translation */
      if (apply_snap_align_rotation) {
        float mat[3][3];

        if (validSnappingNormal(t)) {
          const float *original_normal;

          /* In pose mode, we want to align normals with Y axis of bones... */
          if (t->flag & T_POSE) {
            original_normal = td->axismtx[1];
          }
          else {
            original_normal = td->axismtx[2];
          }

          rotation_between_vecs_to_mat3(mat, original_normal, t->tsnap.snapNormal);
        }
        else {
          unit_m3(mat);
        }

        ElementRotation_ex(t, tc, td, mat, pivot);

        if (td->loc) {
          use_rotate_offset = true;
          sub_v3_v3v3(rotate_offset, td->loc, td->iloc);
        }
      }

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
        /* grease pencil multiframe falloff */
        bGPDstroke *gps = (bGPDstroke *)td->extra;
        if (gps != NULL) {
          mul_v3_fl(tvec, td->factor * gps->runtime.multi_frame_falloff);
        }
        else {
          mul_v3_fl(tvec, td->factor);
        }
      }
      else {
        /* proportional editing falloff */
        mul_v3_fl(tvec, td->factor);
      }

      protectedTransBits(td->protectflag, tvec);

      if (td->loc) {
        add_v3_v3v3(td->loc, td->iloc, tvec);
      }

      constraintTransLim(t, td);
    }
  }
}

static void applyTranslation(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  float global_dir[3];

  if (t->flag & T_INPUT_IS_VALUES_FINAL) {
    mul_v3_m3v3(global_dir, t->spacemtx, t->values);
  }
  else {
    copy_v3_v3(global_dir, t->values);
    if ((t->con.mode & CON_APPLY) == 0) {
      snapGridIncrement(t, global_dir);
    }

    if (applyNumInput(&t->num, global_dir)) {
      removeAspectRatio(t, global_dir);
    }

    applySnapping(t, global_dir);
  }

  if (t->con.mode & CON_APPLY) {
    float in[3];
    copy_v3_v3(in, global_dir);
    t->con.applyVec(t, NULL, NULL, in, global_dir);
    headerTranslation(t, global_dir, str);
  }
  else {
    headerTranslation(t, global_dir, str);
  }

  applyTranslationValue(t, global_dir);

  /* evil hack - redo translation if clipping needed */
  if (t->flag & T_CLIP_UV && clipUVTransform(t, global_dir, 0)) {
    applyTranslationValue(t, global_dir);

    /* In proportional edit it can happen that */
    /* vertices in the radius of the brush end */
    /* outside the clipping area               */
    /* XXX HACK - dg */
    if (t->flag & T_PROP_EDIT_ALL) {
      clipUVData(t);
    }
  }

  /* Set the redo value. */
  mul_v3_m3v3(t->values_final, t->spacemtx_inv, global_dir);

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

  initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

  t->idx_max = (t->flag & T_2D_EDIT) ? 1 : 2;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  copy_v3_v3(t->snap, t->snap_spatial);

  copy_v3_fl(t->num.val_inc, t->snap[1]);
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
}
/** \} */
