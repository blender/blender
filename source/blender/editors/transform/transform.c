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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_scene_types.h" /* PET modes */
#include "DNA_workspace_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_alloca.h"
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_ghash.h"
#include "BLI_utildefines_stack.h"
#include "BLI_memarena.h"

#include "BKE_nla.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_particle.h"
#include "BKE_unit.h"
#include "BKE_scene.h"
#include "BKE_mask.h"
#include "BKE_mesh.h"
#include "BKE_report.h"
#include "BKE_workspace.h"

#include "DEG_depsgraph.h"

#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_markers.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "ED_clip.h"
#include "ED_node.h"
#include "ED_gpencil.h"

#include "WM_types.h"
#include "WM_api.h"

#include "UI_view2d.h"
#include "UI_interface.h"
#include "UI_interface_icons.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "transform.h"

/* Disabling, since when you type you know what you are doing,
 * and being able to set it to zero is handy. */
// #define USE_NUM_NO_ZERO

static void drawTransformApply(const struct bContext *C, ARegion *ar, void *arg);
static void doEdgeSlide(TransInfo *t, float perc);
static void doVertSlide(TransInfo *t, float perc);

static void drawEdgeSlide(TransInfo *t);
static void drawVertSlide(TransInfo *t);
static void postInputRotation(TransInfo *t, float values[3]);

static void ElementRotation(
    TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3], const short around);
static void initSnapSpatial(TransInfo *t, float r_snap[3]);

static void storeCustomLNorValue(TransDataContainer *t, BMesh *bm);

/* Transform Callbacks */
static void initBend(TransInfo *t);
static eRedrawFlag handleEventBend(TransInfo *t, const struct wmEvent *event);
static void Bend(TransInfo *t, const int mval[2]);

static void initShear(TransInfo *t);
static eRedrawFlag handleEventShear(TransInfo *t, const struct wmEvent *event);
static void applyShear(TransInfo *t, const int mval[2]);

static void initResize(TransInfo *t);
static void applyResize(TransInfo *t, const int mval[2]);

static void initSkinResize(TransInfo *t);
static void applySkinResize(TransInfo *t, const int mval[2]);

static void initTranslation(TransInfo *t);
static void applyTranslation(TransInfo *t, const int mval[2]);

static void initToSphere(TransInfo *t);
static void applyToSphere(TransInfo *t, const int mval[2]);

static void initRotation(TransInfo *t);
static void applyRotation(TransInfo *t, const int mval[2]);

static void initNormalRotation(TransInfo *t);
static void applyNormalRotation(TransInfo *t, const int mval[2]);

static void initShrinkFatten(TransInfo *t);
static void applyShrinkFatten(TransInfo *t, const int mval[2]);

static void initTilt(TransInfo *t);
static void applyTilt(TransInfo *t, const int mval[2]);

static void initCurveShrinkFatten(TransInfo *t);
static void applyCurveShrinkFatten(TransInfo *t, const int mval[2]);

static void initMaskShrinkFatten(TransInfo *t);
static void applyMaskShrinkFatten(TransInfo *t, const int mval[2]);

static void initGPShrinkFatten(TransInfo *t);
static void applyGPShrinkFatten(TransInfo *t, const int mval[2]);

static void initTrackball(TransInfo *t);
static void applyTrackball(TransInfo *t, const int mval[2]);

static void initPushPull(TransInfo *t);
static void applyPushPull(TransInfo *t, const int mval[2]);

static void initBevelWeight(TransInfo *t);
static void applyBevelWeight(TransInfo *t, const int mval[2]);

static void initCrease(TransInfo *t);
static void applyCrease(TransInfo *t, const int mval[2]);

static void initBoneSize(TransInfo *t);
static void applyBoneSize(TransInfo *t, const int mval[2]);

static void initBoneEnvelope(TransInfo *t);
static void applyBoneEnvelope(TransInfo *t, const int mval[2]);

static void initBoneRoll(TransInfo *t);
static void applyBoneRoll(TransInfo *t, const int mval[2]);

static void initEdgeSlide_ex(
    TransInfo *t, bool use_double_side, bool use_even, bool flipped, bool use_clamp);
static void initEdgeSlide(TransInfo *t);
static eRedrawFlag handleEventEdgeSlide(TransInfo *t, const struct wmEvent *event);
static void applyEdgeSlide(TransInfo *t, const int mval[2]);

static void initVertSlide_ex(TransInfo *t, bool use_even, bool flipped, bool use_clamp);
static void initVertSlide(TransInfo *t);
static eRedrawFlag handleEventVertSlide(TransInfo *t, const struct wmEvent *event);
static void applyVertSlide(TransInfo *t, const int mval[2]);

static void initTimeTranslate(TransInfo *t);
static void applyTimeTranslate(TransInfo *t, const int mval[2]);

static void initTimeSlide(TransInfo *t);
static void applyTimeSlide(TransInfo *t, const int mval[2]);

static void initTimeScale(TransInfo *t);
static void applyTimeScale(TransInfo *t, const int mval[2]);

static void initBakeTime(TransInfo *t);
static void applyBakeTime(TransInfo *t, const int mval[2]);

static void initMirror(TransInfo *t);
static void applyMirror(TransInfo *t, const int mval[2]);

static void initAlign(TransInfo *t);
static void applyAlign(TransInfo *t, const int mval[2]);

static void initSeqSlide(TransInfo *t);
static void applySeqSlide(TransInfo *t, const int mval[2]);

static void initGPOpacity(TransInfo *t);
static void applyGPOpacity(TransInfo *t, const int mval[2]);
/* end transform callbacks */

static bool transdata_check_local_center(TransInfo *t, short around)
{
  return ((around == V3D_AROUND_LOCAL_ORIGINS) &&
          ((t->flag & (T_OBJECT | T_POSE)) ||
           /* implicit: (t->flag & T_EDIT) */
           (ELEM(t->obedit_type, OB_MESH, OB_CURVE, OB_MBALL, OB_ARMATURE, OB_GPENCIL)) ||
           (t->spacetype == SPACE_GRAPH) ||
           (t->options & (CTX_MOVIECLIP | CTX_MASK | CTX_PAINT_CURVE))));
}

bool transdata_check_local_islands(TransInfo *t, short around)
{
  return ((around == V3D_AROUND_LOCAL_ORIGINS) && ((ELEM(t->obedit_type, OB_MESH))));
}

/* ************************** SPACE DEPENDENT CODE **************************** */

void setTransformViewMatrices(TransInfo *t)
{
  if (t->spacetype == SPACE_VIEW3D && t->ar && t->ar->regiontype == RGN_TYPE_WINDOW) {
    RegionView3D *rv3d = t->ar->regiondata;

    copy_m4_m4(t->viewmat, rv3d->viewmat);
    copy_m4_m4(t->viewinv, rv3d->viewinv);
    copy_m4_m4(t->persmat, rv3d->persmat);
    copy_m4_m4(t->persinv, rv3d->persinv);
    t->persp = rv3d->persp;
  }
  else {
    unit_m4(t->viewmat);
    unit_m4(t->viewinv);
    unit_m4(t->persmat);
    unit_m4(t->persinv);
    t->persp = RV3D_ORTHO;
  }

  calculateCenter2D(t);
  calculateCenterLocal(t, t->center_global);
}

void setTransformViewAspect(TransInfo *t, float r_aspect[3])
{
  copy_v3_fl(r_aspect, 1.0f);

  if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = t->sa->spacedata.first;

    if (t->options & CTX_MASK) {
      ED_space_image_get_aspect(sima, &r_aspect[0], &r_aspect[1]);
    }
    else if (t->options & CTX_PAINT_CURVE) {
      /* pass */
    }
    else {
      ED_space_image_get_uv_aspect(sima, &r_aspect[0], &r_aspect[1]);
    }
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sclip = t->sa->spacedata.first;

    if (t->options & CTX_MOVIECLIP) {
      ED_space_clip_get_aspect_dimension_aware(sclip, &r_aspect[0], &r_aspect[1]);
    }
    else {
      ED_space_clip_get_aspect(sclip, &r_aspect[0], &r_aspect[1]);
    }
  }
  else if (t->spacetype == SPACE_GRAPH) {
    /* depemds on context of usage */
  }
}

static void convertViewVec2D(View2D *v2d, float r_vec[3], int dx, int dy)
{
  float divx = BLI_rcti_size_x(&v2d->mask);
  float divy = BLI_rcti_size_y(&v2d->mask);

  r_vec[0] = BLI_rctf_size_x(&v2d->cur) * dx / divx;
  r_vec[1] = BLI_rctf_size_y(&v2d->cur) * dy / divy;
  r_vec[2] = 0.0f;
}

static void convertViewVec2D_mask(View2D *v2d, float r_vec[3], int dx, int dy)
{
  float divx = BLI_rcti_size_x(&v2d->mask);
  float divy = BLI_rcti_size_y(&v2d->mask);

  float mulx = BLI_rctf_size_x(&v2d->cur);
  float muly = BLI_rctf_size_y(&v2d->cur);

  /* difference with convertViewVec2D */
  /* clamp w/h, mask only */
  if (mulx / divx < muly / divy) {
    divy = divx;
    muly = mulx;
  }
  else {
    divx = divy;
    mulx = muly;
  }
  /* end difference */

  r_vec[0] = mulx * dx / divx;
  r_vec[1] = muly * dy / divy;
  r_vec[2] = 0.0f;
}

void convertViewVec(TransInfo *t, float r_vec[3], double dx, double dy)
{
  if ((t->spacetype == SPACE_VIEW3D) && (t->ar->regiontype == RGN_TYPE_WINDOW)) {
    if (t->options & CTX_PAINT_CURVE) {
      r_vec[0] = dx;
      r_vec[1] = dy;
    }
    else {
      const float mval_f[2] = {(float)dx, (float)dy};
      ED_view3d_win_to_delta(t->ar, mval_f, r_vec, t->zfac);
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      convertViewVec2D_mask(t->view, r_vec, dx, dy);
    }
    else if (t->options & CTX_PAINT_CURVE) {
      r_vec[0] = dx;
      r_vec[1] = dy;
    }
    else {
      convertViewVec2D(t->view, r_vec, dx, dy);
    }

    r_vec[0] *= t->aspect[0];
    r_vec[1] *= t->aspect[1];
  }
  else if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
    convertViewVec2D(t->view, r_vec, dx, dy);
  }
  else if (ELEM(t->spacetype, SPACE_NODE, SPACE_SEQ)) {
    convertViewVec2D(&t->ar->v2d, r_vec, dx, dy);
  }
  else if (t->spacetype == SPACE_CLIP) {
    if (t->options & CTX_MASK) {
      convertViewVec2D_mask(t->view, r_vec, dx, dy);
    }
    else {
      convertViewVec2D(t->view, r_vec, dx, dy);
    }

    r_vec[0] *= t->aspect[0];
    r_vec[1] *= t->aspect[1];
  }
  else {
    printf("%s: called in an invalid context\n", __func__);
    zero_v3(r_vec);
  }
}

void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], const eV3DProjTest flag)
{
  if (t->spacetype == SPACE_VIEW3D) {
    if (t->ar->regiontype == RGN_TYPE_WINDOW) {
      if (ED_view3d_project_int_global(t->ar, vec, adr, flag) != V3D_PROJ_RET_OK) {
        /* this is what was done in 2.64, perhaps we can be smarter? */
        adr[0] = (int)2140000000.0f;
        adr[1] = (int)2140000000.0f;
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = t->sa->spacedata.first;

    if (t->options & CTX_MASK) {
      float v[2];

      v[0] = vec[0] / t->aspect[0];
      v[1] = vec[1] / t->aspect[1];

      BKE_mask_coord_to_image(sima->image, &sima->iuser, v, v);

      ED_image_point_pos__reverse(sima, t->ar, v, v);

      adr[0] = v[0];
      adr[1] = v[1];
    }
    else if (t->options & CTX_PAINT_CURVE) {
      adr[0] = vec[0];
      adr[1] = vec[1];
    }
    else {
      float v[2];

      v[0] = vec[0] / t->aspect[0];
      v[1] = vec[1] / t->aspect[1];

      UI_view2d_view_to_region(t->view, v[0], v[1], &adr[0], &adr[1]);
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    int out[2] = {0, 0};
#if 0
    SpaceAction *sact = t->sa->spacedata.first;

    if (sact->flag & SACTION_DRAWTIME) {
      //vec[0] = vec[0]/((t->scene->r.frs_sec / t->scene->r.frs_sec_base));
      /* same as below */
      UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
    }
    else
#endif
    {
      UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
    }

    adr[0] = out[0];
    adr[1] = out[1];
  }
  else if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
    int out[2] = {0, 0};

    UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
    adr[0] = out[0];
    adr[1] = out[1];
  }
  else if (t->spacetype == SPACE_SEQ) { /* XXX not tested yet, but should work */
    int out[2] = {0, 0};

    UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
    adr[0] = out[0];
    adr[1] = out[1];
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = t->sa->spacedata.first;

    if (t->options & CTX_MASK) {
      MovieClip *clip = ED_space_clip_get_clip(sc);

      if (clip) {
        float v[2];

        v[0] = vec[0] / t->aspect[0];
        v[1] = vec[1] / t->aspect[1];

        BKE_mask_coord_to_movieclip(sc->clip, &sc->user, v, v);

        ED_clip_point_stable_pos__reverse(sc, t->ar, v, v);

        adr[0] = v[0];
        adr[1] = v[1];
      }
      else {
        adr[0] = 0;
        adr[1] = 0;
      }
    }
    else if (t->options & CTX_MOVIECLIP) {
      float v[2];

      v[0] = vec[0] / t->aspect[0];
      v[1] = vec[1] / t->aspect[1];

      UI_view2d_view_to_region(t->view, v[0], v[1], &adr[0], &adr[1]);
    }
    else {
      BLI_assert(0);
    }
  }
  else if (t->spacetype == SPACE_NODE) {
    UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &adr[0], &adr[1]);
  }
}
void projectIntView(TransInfo *t, const float vec[3], int adr[2])
{
  projectIntViewEx(t, vec, adr, V3D_PROJ_TEST_NOP);
}

void projectFloatViewEx(TransInfo *t, const float vec[3], float adr[2], const eV3DProjTest flag)
{
  switch (t->spacetype) {
    case SPACE_VIEW3D: {
      if (t->options & CTX_PAINT_CURVE) {
        adr[0] = vec[0];
        adr[1] = vec[1];
      }
      else if (t->ar->regiontype == RGN_TYPE_WINDOW) {
        /* allow points behind the view [#33643] */
        if (ED_view3d_project_float_global(t->ar, vec, adr, flag) != V3D_PROJ_RET_OK) {
          /* XXX, 2.64 and prior did this, weak! */
          adr[0] = t->ar->winx / 2.0f;
          adr[1] = t->ar->winy / 2.0f;
        }
        return;
      }
      break;
    }
    default: {
      int a[2] = {0, 0};
      projectIntView(t, vec, a);
      adr[0] = a[0];
      adr[1] = a[1];
      break;
    }
  }
}
void projectFloatView(TransInfo *t, const float vec[3], float adr[2])
{
  projectFloatViewEx(t, vec, adr, V3D_PROJ_TEST_NOP);
}

void applyAspectRatio(TransInfo *t, float vec[2])
{
  if ((t->spacetype == SPACE_IMAGE) && (t->mode == TFM_TRANSLATION) &&
      !(t->options & CTX_PAINT_CURVE)) {
    SpaceImage *sima = t->sa->spacedata.first;

    if ((sima->flag & SI_COORDFLOATS) == 0) {
      int width, height;
      ED_space_image_get_size(sima, &width, &height);

      vec[0] *= width;
      vec[1] *= height;
    }

    vec[0] /= t->aspect[0];
    vec[1] /= t->aspect[1];
  }
  else if ((t->spacetype == SPACE_CLIP) && (t->mode == TFM_TRANSLATION)) {
    if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
      vec[0] /= t->aspect[0];
      vec[1] /= t->aspect[1];
    }
  }
}

void removeAspectRatio(TransInfo *t, float vec[2])
{
  if ((t->spacetype == SPACE_IMAGE) && (t->mode == TFM_TRANSLATION)) {
    SpaceImage *sima = t->sa->spacedata.first;

    if ((sima->flag & SI_COORDFLOATS) == 0) {
      int width, height;
      ED_space_image_get_size(sima, &width, &height);

      vec[0] /= width;
      vec[1] /= height;
    }

    vec[0] *= t->aspect[0];
    vec[1] *= t->aspect[1];
  }
  else if ((t->spacetype == SPACE_CLIP) && (t->mode == TFM_TRANSLATION)) {
    if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
      vec[0] *= t->aspect[0];
      vec[1] *= t->aspect[1];
    }
  }
}

static void viewRedrawForce(const bContext *C, TransInfo *t)
{
  if (t->options & CTX_GPENCIL_STROKES) {
    bGPdata *gpd = ED_gpencil_data_get_active(C);
    if (gpd) {
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
    }
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_VIEW3D) {
    if (t->options & CTX_PAINT_CURVE) {
      wmWindow *window = CTX_wm_window(C);
      WM_paint_cursor_tag_redraw(window, t->ar);
    }
    else {
      /* Do we need more refined tags? */
      if (t->flag & T_POSE) {
        WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
      }
      else {
        WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
      }

      /* For real-time animation record - send notifiers recognized by animation editors */
      // XXX: is this notifier a lame duck?
      if ((t->animtimer) && IS_AUTOKEY_ON(t->scene)) {
        WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, NULL);
      }
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    // SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    // SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_NLA) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_NODE) {
    // ED_area_tag_redraw(t->sa);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);
  }
  else if (t->spacetype == SPACE_SEQ) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, NULL);
    /* Keyframes on strips has been moved, so make sure related editos are informed. */
    WM_event_add_notifier(C, NC_ANIMATION, NULL);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      Mask *mask = CTX_data_edit_mask(C);

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
    }
    else if (t->options & CTX_PAINT_CURVE) {
      wmWindow *window = CTX_wm_window(C);
      WM_paint_cursor_tag_redraw(window, t->ar);
    }
    else if (t->flag & T_CURSOR) {
      ED_area_tag_redraw(t->sa);
    }
    else {
      // XXX how to deal with lock?
      SpaceImage *sima = (SpaceImage *)t->sa->spacedata.first;
      if (sima->lock) {
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, OBEDIT_FROM_VIEW_LAYER(t->view_layer)->data);
      }
      else {
        ED_area_tag_redraw(t->sa);
      }
    }
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = (SpaceClip *)t->sa->spacedata.first;

    if (ED_space_clip_check_show_trackedit(sc)) {
      MovieClip *clip = ED_space_clip_get_clip(sc);

      /* objects could be parented to tracking data, so send this for viewport refresh */
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

      WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, clip);
    }
    else if (ED_space_clip_check_show_maskedit(sc)) {
      Mask *mask = CTX_data_edit_mask(C);

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
    }
  }
}

static void viewRedrawPost(bContext *C, TransInfo *t)
{
  ED_area_status_text(t->sa, NULL);

  if (t->spacetype == SPACE_VIEW3D) {
    /* if autokeying is enabled, send notifiers that keyframes were added */
    if (IS_AUTOKEY_ON(t->scene)) {
      WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
    }

    /* redraw UV editor */
    if (ELEM(t->mode, TFM_VERT_SLIDE, TFM_EDGE_SLIDE) &&
        (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT)) {
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, NULL);
    }

    /* XXX temp, first hack to get auto-render in compositor work (ton) */
    WM_event_add_notifier(C, NC_SCENE | ND_TRANSFORM_DONE, CTX_data_scene(C));
  }

#if 0  // TRANSFORM_FIX_ME
  if (t->spacetype == SPACE_VIEW3D) {
    allqueue(REDRAWBUTSOBJECT, 0);
    allqueue(REDRAWVIEW3D, 0);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    allqueue(REDRAWIMAGE, 0);
    allqueue(REDRAWVIEW3D, 0);
  }
  else if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA, SPACE_GRAPH)) {
    allqueue(REDRAWVIEW3D, 0);
    allqueue(REDRAWACTION, 0);
    allqueue(REDRAWNLA, 0);
    allqueue(REDRAWIPO, 0);
    allqueue(REDRAWTIME, 0);
    allqueue(REDRAWBUTSOBJECT, 0);
  }

  scrarea_queue_headredraw(curarea);
#endif
}

/* ************************** TRANSFORMATIONS **************************** */

static void view_editmove(unsigned short UNUSED(event))
{
#if 0  // TRANSFORM_FIX_ME
  int refresh = 0;
  /* Regular:   Zoom in */
  /* Shift:     Scroll up */
  /* Ctrl:      Scroll right */
  /* Alt-Shift: Rotate up */
  /* Alt-Ctrl:  Rotate right */

  /* only work in 3D window for now
   * In the end, will have to send to event to a 2D window handler instead
   */
  if (Trans.flag & T_2D_EDIT) {
    return;
  }

  switch (event) {
    case WHEELUPMOUSE:
      if (G.qual & LR_SHIFTKEY) {
        if (G.qual & LR_ALTKEY) {
          G.qual &= ~LR_SHIFTKEY;
          persptoetsen(PAD2);
          G.qual |= LR_SHIFTKEY;
        }
        else {
          persptoetsen(PAD2);
        }
      }
      else if (G.qual & LR_CTRLKEY) {
        if (G.qual & LR_ALTKEY) {
          G.qual &= ~LR_CTRLKEY;
          persptoetsen(PAD4);
          G.qual |= LR_CTRLKEY;
        }
        else {
          persptoetsen(PAD4);
        }
      }
      else if (U.uiflag & USER_WHEELZOOMDIR) {
        persptoetsen(PADMINUS);
      }
      else {
        persptoetsen(PADPLUSKEY);
      }

      refresh = 1;
      break;
    case WHEELDOWNMOUSE:
      if (G.qual & LR_SHIFTKEY) {
        if (G.qual & LR_ALTKEY) {
          G.qual &= ~LR_SHIFTKEY;
          persptoetsen(PAD8);
          G.qual |= LR_SHIFTKEY;
        }
        else {
          persptoetsen(PAD8);
        }
      }
      else if (G.qual & LR_CTRLKEY) {
        if (G.qual & LR_ALTKEY) {
          G.qual &= ~LR_CTRLKEY;
          persptoetsen(PAD6);
          G.qual |= LR_CTRLKEY;
        }
        else {
          persptoetsen(PAD6);
        }
      }
      else if (U.uiflag & USER_WHEELZOOMDIR) {
        persptoetsen(PADPLUSKEY);
      }
      else {
        persptoetsen(PADMINUS);
      }

      refresh = 1;
      break;
  }

  if (refresh) {
    setTransformViewMatrices(&Trans);
  }
#endif
}

/* ************************************************* */

/* NOTE: these defines are saved in keymap files, do not change values but just add new ones */
enum {
  TFM_MODAL_CANCEL = 1,
  TFM_MODAL_CONFIRM = 2,
  TFM_MODAL_TRANSLATE = 3,
  TFM_MODAL_ROTATE = 4,
  TFM_MODAL_RESIZE = 5,
  TFM_MODAL_SNAP_INV_ON = 6,
  TFM_MODAL_SNAP_INV_OFF = 7,
  TFM_MODAL_SNAP_TOGGLE = 8,
  TFM_MODAL_AXIS_X = 9,
  TFM_MODAL_AXIS_Y = 10,
  TFM_MODAL_AXIS_Z = 11,
  TFM_MODAL_PLANE_X = 12,
  TFM_MODAL_PLANE_Y = 13,
  TFM_MODAL_PLANE_Z = 14,
  TFM_MODAL_CONS_OFF = 15,
  TFM_MODAL_ADD_SNAP = 16,
  TFM_MODAL_REMOVE_SNAP = 17,

  /* 18 and 19 used by numinput, defined in transform.h */

  TFM_MODAL_PROPSIZE_UP = 20,
  TFM_MODAL_PROPSIZE_DOWN = 21,
  TFM_MODAL_AUTOIK_LEN_INC = 22,
  TFM_MODAL_AUTOIK_LEN_DEC = 23,

  TFM_MODAL_EDGESLIDE_UP = 24,
  TFM_MODAL_EDGESLIDE_DOWN = 25,

  /* for analog input, like trackpad */
  TFM_MODAL_PROPSIZE = 26,
  /* node editor insert offset (aka auto-offset) direction toggle */
  TFM_MODAL_INSERTOFS_TOGGLE_DIR = 27,
};

static bool transform_modal_item_poll(const wmOperator *op, int value)
{
  const TransInfo *t = op->customdata;
  switch (value) {
    case TFM_MODAL_CANCEL: {
      if ((t->flag & T_RELEASE_CONFIRM) && ISMOUSE(t->launch_event)) {
        return false;
      }
      break;
    }
    case TFM_MODAL_PROPSIZE:
    case TFM_MODAL_PROPSIZE_UP:
    case TFM_MODAL_PROPSIZE_DOWN: {
      if ((t->flag & T_PROP_EDIT) == 0) {
        return false;
      }
      break;
    }
    case TFM_MODAL_ADD_SNAP:
    case TFM_MODAL_REMOVE_SNAP: {
      if (t->spacetype != SPACE_VIEW3D) {
        return false;
      }
      else if (t->tsnap.mode & (SCE_SNAP_MODE_INCREMENT | SCE_SNAP_MODE_GRID)) {
        return false;
      }
      else if (!validSnap(t)) {
        return false;
      }
      break;
    }
    case TFM_MODAL_AXIS_X:
    case TFM_MODAL_AXIS_Y:
    case TFM_MODAL_AXIS_Z:
    case TFM_MODAL_PLANE_X:
    case TFM_MODAL_PLANE_Y:
    case TFM_MODAL_PLANE_Z: {
      if (t->flag & T_NO_CONSTRAINT) {
        return false;
      }
      if (!ELEM(value, TFM_MODAL_AXIS_X, TFM_MODAL_AXIS_Y)) {
        if (t->flag & T_2D_EDIT) {
          return false;
        }
      }
      break;
    }
    case TFM_MODAL_CONS_OFF: {
      if ((t->con.mode & CON_APPLY) == 0) {
        return false;
      }
      break;
    }
    case TFM_MODAL_EDGESLIDE_UP:
    case TFM_MODAL_EDGESLIDE_DOWN: {
      if (t->mode != TFM_EDGE_SLIDE) {
        return false;
      }
      break;
    }
    case TFM_MODAL_INSERTOFS_TOGGLE_DIR: {
      if (t->spacetype != SPACE_NODE) {
        return false;
      }
      break;
    }
    case TFM_MODAL_AUTOIK_LEN_INC:
    case TFM_MODAL_AUTOIK_LEN_DEC: {
      if ((t->flag & T_AUTOIK) == 0) {
        return false;
      }
      break;
    }
  }
  return true;
}

/* called in transform_ops.c, on each regeneration of keymaps */
wmKeyMap *transform_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {TFM_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {TFM_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {TFM_MODAL_AXIS_X, "AXIS_X", 0, "X axis", ""},
      {TFM_MODAL_AXIS_Y, "AXIS_Y", 0, "Y axis", ""},
      {TFM_MODAL_AXIS_Z, "AXIS_Z", 0, "Z axis", ""},
      {TFM_MODAL_PLANE_X, "PLANE_X", 0, "X plane", ""},
      {TFM_MODAL_PLANE_Y, "PLANE_Y", 0, "Y plane", ""},
      {TFM_MODAL_PLANE_Z, "PLANE_Z", 0, "Z plane", ""},
      {TFM_MODAL_CONS_OFF, "CONS_OFF", 0, "Clear Constraints", ""},
      {TFM_MODAL_SNAP_INV_ON, "SNAP_INV_ON", 0, "Snap Invert", ""},
      {TFM_MODAL_SNAP_INV_OFF, "SNAP_INV_OFF", 0, "Snap Invert (Off)", ""},
      {TFM_MODAL_SNAP_TOGGLE, "SNAP_TOGGLE", 0, "Snap Toggle", ""},
      {TFM_MODAL_ADD_SNAP, "ADD_SNAP", 0, "Add Snap Point", ""},
      {TFM_MODAL_REMOVE_SNAP, "REMOVE_SNAP", 0, "Remove Last Snap Point", ""},
      {NUM_MODAL_INCREMENT_UP, "INCREMENT_UP", 0, "Numinput Increment Up", ""},
      {NUM_MODAL_INCREMENT_DOWN, "INCREMENT_DOWN", 0, "Numinput Increment Down", ""},
      {TFM_MODAL_PROPSIZE_UP, "PROPORTIONAL_SIZE_UP", 0, "Increase Proportional Influence", ""},
      {TFM_MODAL_PROPSIZE_DOWN,
       "PROPORTIONAL_SIZE_DOWN",
       0,
       "Decrease Proportional Influence",
       ""},
      {TFM_MODAL_AUTOIK_LEN_INC, "AUTOIK_CHAIN_LEN_UP", 0, "Increase Max AutoIK Chain Length", ""},
      {TFM_MODAL_AUTOIK_LEN_DEC,
       "AUTOIK_CHAIN_LEN_DOWN",
       0,
       "Decrease Max AutoIK Chain Length",
       ""},
      {TFM_MODAL_EDGESLIDE_UP, "EDGESLIDE_EDGE_NEXT", 0, "Select next Edge Slide Edge", ""},
      {TFM_MODAL_EDGESLIDE_DOWN, "EDGESLIDE_PREV_NEXT", 0, "Select previous Edge Slide Edge", ""},
      {TFM_MODAL_PROPSIZE, "PROPORTIONAL_SIZE", 0, "Adjust Proportional Influence", ""},
      {TFM_MODAL_INSERTOFS_TOGGLE_DIR,
       "INSERTOFS_TOGGLE_DIR",
       0,
       "Toggle Direction for Node Auto-offset",
       ""},
      {TFM_MODAL_TRANSLATE, "TRANSLATE", 0, "Move", ""},
      {TFM_MODAL_ROTATE, "ROTATE", 0, "Rotate", ""},
      {TFM_MODAL_RESIZE, "RESIZE", 0, "Resize", ""},
      {0, NULL, 0, NULL, NULL},
  };

  wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Transform Modal Map");

  keymap = WM_modalkeymap_add(keyconf, "Transform Modal Map", modal_items);
  keymap->poll_modal_item = transform_modal_item_poll;

  return keymap;
}

static void transform_event_xyz_constraint(TransInfo *t, short key_type, char cmode, bool is_plane)
{
  if (!(t->flag & T_NO_CONSTRAINT)) {
    int constraint_axis, constraint_plane;
    const bool edit_2d = (t->flag & T_2D_EDIT) != 0;
    const char *msg1 = "", *msg2 = "", *msg3 = "";
    char axis;

    /* Initialize */
    switch (key_type) {
      case XKEY:
        msg1 = TIP_("along X");
        msg2 = TIP_("along %s X");
        msg3 = TIP_("locking %s X");
        axis = 'X';
        constraint_axis = CON_AXIS0;
        break;
      case YKEY:
        msg1 = TIP_("along Y");
        msg2 = TIP_("along %s Y");
        msg3 = TIP_("locking %s Y");
        axis = 'Y';
        constraint_axis = CON_AXIS1;
        break;
      case ZKEY:
        msg1 = TIP_("along Z");
        msg2 = TIP_("along %s Z");
        msg3 = TIP_("locking %s Z");
        axis = 'Z';
        constraint_axis = CON_AXIS2;
        break;
      default:
        /* Invalid key */
        return;
    }
    constraint_plane = ((CON_AXIS0 | CON_AXIS1 | CON_AXIS2) & (~constraint_axis));

    if (edit_2d && (key_type != ZKEY)) {
      if (cmode == axis) {
        stopConstraint(t);
      }
      else {
        setUserConstraint(t, V3D_ORIENT_GLOBAL, constraint_axis, msg1);
      }
    }
    else if (!edit_2d) {
      if (cmode != axis) {
        /* First press, constraint to an axis. */
        t->orientation.index = 0;
        const short *orientation_ptr = t->orientation.types[t->orientation.index];
        const short orientation = orientation_ptr ? *orientation_ptr : V3D_ORIENT_GLOBAL;
        if (is_plane == false) {
          setUserConstraint(t, orientation, constraint_axis, msg2);
        }
        else {
          setUserConstraint(t, orientation, constraint_plane, msg3);
        }
      }
      else {
        /* Successive presses on existing axis, cycle orientation modes. */
        t->orientation.index = (t->orientation.index + 1) % ARRAY_SIZE(t->orientation.types);

        if (t->orientation.index == 0) {
          stopConstraint(t);
        }
        else {
          const short *orientation_ptr = t->orientation.types[t->orientation.index];
          const short orientation = orientation_ptr ? *orientation_ptr : V3D_ORIENT_GLOBAL;
          if (is_plane == false) {
            setUserConstraint(t, orientation, constraint_axis, msg2);
          }
          else {
            setUserConstraint(t, orientation, constraint_plane, msg3);
          }
        }
      }
    }
    t->redraw |= TREDRAW_HARD;
  }
}

int transformEvent(TransInfo *t, const wmEvent *event)
{
  char cmode = constraintModeToChar(t);
  bool handled = false;
  const int modifiers_prev = t->modifiers;
  const int mode_prev = t->mode;

  t->redraw |= handleMouseInput(t, &t->mouse, event);

  /* Handle modal numinput events first, if already activated. */
  if (((event->val == KM_PRESS) || (event->type == EVT_MODAL_MAP)) && hasNumInput(&t->num) &&
      handleNumInput(t->context, &(t->num), event)) {
    t->redraw |= TREDRAW_HARD;
    handled = true;
  }
  else if (event->type == MOUSEMOVE) {
    if (t->modifiers & MOD_CONSTRAINT_SELECT) {
      t->con.mode |= CON_SELECT;
    }

    copy_v2_v2_int(t->mval, event->mval);

    /* Use this for soft redraw. Might cause flicker in object mode */
    // t->redraw |= TREDRAW_SOFT;
    t->redraw |= TREDRAW_HARD;

    if (t->state == TRANS_STARTING) {
      t->state = TRANS_RUNNING;
    }

    applyMouseInput(t, &t->mouse, t->mval, t->values);

    // Snapping mouse move events
    t->redraw |= handleSnapping(t, event);
    handled = true;
  }
  /* handle modal keymap first */
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case TFM_MODAL_CANCEL:
        t->state = TRANS_CANCEL;
        handled = true;
        break;
      case TFM_MODAL_CONFIRM:
        t->state = TRANS_CONFIRM;
        handled = true;
        break;
      case TFM_MODAL_TRANSLATE:
        /* only switch when... */
        if (ELEM(t->mode,
                 TFM_ROTATION,
                 TFM_RESIZE,
                 TFM_TRACKBALL,
                 TFM_EDGE_SLIDE,
                 TFM_VERT_SLIDE)) {
          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          initTranslation(t);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        else if (t->mode == TFM_SEQ_SLIDE) {
          t->flag ^= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        else {
          if (t->obedit_type == OB_MESH) {
            if ((t->mode == TFM_TRANSLATION) && (t->spacetype == SPACE_VIEW3D)) {
              restoreTransObjects(t);
              resetTransModal(t);
              resetTransRestrictions(t);

              /* first try edge slide */
              initEdgeSlide(t);
              /* if that fails, do vertex slide */
              if (t->state == TRANS_CANCEL) {
                resetTransModal(t);
                t->state = TRANS_STARTING;
                initVertSlide(t);
              }
              /* vert slide can fail on unconnected vertices (rare but possible) */
              if (t->state == TRANS_CANCEL) {
                resetTransModal(t);
                t->mode = TFM_TRANSLATION;
                t->state = TRANS_STARTING;
                restoreTransObjects(t);
                resetTransRestrictions(t);
                initTranslation(t);
              }
              initSnapping(t, NULL);  // need to reinit after mode change
              t->redraw |= TREDRAW_HARD;
              handled = true;
            }
          }
          else if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
            if (t->mode == TFM_TRANSLATION) {
              restoreTransObjects(t);

              t->flag ^= T_ALT_TRANSFORM;
              t->redraw |= TREDRAW_HARD;
              handled = true;
            }
          }
        }
        break;
      case TFM_MODAL_ROTATE:
        /* only switch when... */
        if (!(t->options & CTX_TEXTURE) && !(t->options & (CTX_MOVIECLIP | CTX_MASK))) {
          if (ELEM(t->mode,
                   TFM_ROTATION,
                   TFM_RESIZE,
                   TFM_TRACKBALL,
                   TFM_TRANSLATION,
                   TFM_EDGE_SLIDE,
                   TFM_VERT_SLIDE)) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);

            if (t->mode == TFM_ROTATION) {
              initTrackball(t);
            }
            else {
              initRotation(t);
            }
            initSnapping(t, NULL);  // need to reinit after mode change
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        break;
      case TFM_MODAL_RESIZE:
        /* only switch when... */
        if (ELEM(t->mode,
                 TFM_ROTATION,
                 TFM_TRANSLATION,
                 TFM_TRACKBALL,
                 TFM_EDGE_SLIDE,
                 TFM_VERT_SLIDE)) {

          /* Scale isn't normally very useful after extrude along normals, see T39756 */
          if ((t->con.mode & CON_APPLY) && (t->con.orientation == V3D_ORIENT_NORMAL)) {
            stopConstraint(t);
          }

          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          initResize(t);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        else if (t->mode == TFM_SHRINKFATTEN) {
          t->flag ^= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        else if (t->mode == TFM_RESIZE) {
          if (t->options & CTX_MOVIECLIP) {
            restoreTransObjects(t);

            t->flag ^= T_ALT_TRANSFORM;
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        break;

      case TFM_MODAL_SNAP_INV_ON:
        t->modifiers |= MOD_SNAP_INVERT;
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;
      case TFM_MODAL_SNAP_INV_OFF:
        t->modifiers &= ~MOD_SNAP_INVERT;
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;
      case TFM_MODAL_SNAP_TOGGLE:
        t->modifiers ^= MOD_SNAP;
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;
      case TFM_MODAL_AXIS_X:
        if (!(t->flag & T_NO_CONSTRAINT)) {
          transform_event_xyz_constraint(t, XKEY, cmode, false);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_AXIS_Y:
        if ((t->flag & T_NO_CONSTRAINT) == 0) {
          transform_event_xyz_constraint(t, YKEY, cmode, false);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_AXIS_Z:
        if ((t->flag & (T_NO_CONSTRAINT)) == 0) {
          transform_event_xyz_constraint(t, ZKEY, cmode, false);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PLANE_X:
        if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
          transform_event_xyz_constraint(t, XKEY, cmode, true);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PLANE_Y:
        if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
          transform_event_xyz_constraint(t, YKEY, cmode, true);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PLANE_Z:
        if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
          transform_event_xyz_constraint(t, ZKEY, cmode, true);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_CONS_OFF:
        if ((t->flag & T_NO_CONSTRAINT) == 0) {
          stopConstraint(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_ADD_SNAP:
        addSnapPoint(t);
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;
      case TFM_MODAL_REMOVE_SNAP:
        removeSnapPoint(t);
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;
      case TFM_MODAL_PROPSIZE:
        /* MOUSEPAN usage... */
        if (t->flag & T_PROP_EDIT) {
          float fac = 1.0f + 0.005f * (event->y - event->prevy);
          t->prop_size *= fac;
          if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO) {
            t->prop_size = max_ff(min_ff(t->prop_size, ((View3D *)t->view)->clip_end),
                                  T_PROP_SIZE_MIN);
          }
          else {
            t->prop_size = max_ff(min_ff(t->prop_size, T_PROP_SIZE_MAX), T_PROP_SIZE_MIN);
          }
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PROPSIZE_UP:
        if (t->flag & T_PROP_EDIT) {
          t->prop_size *= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO) {
            t->prop_size = min_ff(t->prop_size, ((View3D *)t->view)->clip_end);
          }
          else {
            t->prop_size = min_ff(t->prop_size, T_PROP_SIZE_MAX);
          }
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PROPSIZE_DOWN:
        if (t->flag & T_PROP_EDIT) {
          t->prop_size /= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          t->prop_size = max_ff(t->prop_size, T_PROP_SIZE_MIN);
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_AUTOIK_LEN_INC:
        if (t->flag & T_AUTOIK) {
          transform_autoik_update(t, 1);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_AUTOIK_LEN_DEC:
        if (t->flag & T_AUTOIK) {
          transform_autoik_update(t, -1);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_INSERTOFS_TOGGLE_DIR:
        if (t->spacetype == SPACE_NODE) {
          SpaceNode *snode = (SpaceNode *)t->sa->spacedata.first;

          BLI_assert(t->sa->spacetype == t->spacetype);

          if (snode->insert_ofs_dir == SNODE_INSERTOFS_DIR_RIGHT) {
            snode->insert_ofs_dir = SNODE_INSERTOFS_DIR_LEFT;
          }
          else if (snode->insert_ofs_dir == SNODE_INSERTOFS_DIR_LEFT) {
            snode->insert_ofs_dir = SNODE_INSERTOFS_DIR_RIGHT;
          }
          else {
            BLI_assert(0);
          }

          t->redraw |= TREDRAW_SOFT;
        }
        break;
      /* Those two are only handled in transform's own handler, see T44634! */
      case TFM_MODAL_EDGESLIDE_UP:
      case TFM_MODAL_EDGESLIDE_DOWN:
      default:
        break;
    }
  }
  /* else do non-mapped events */
  else if (event->val == KM_PRESS) {
    switch (event->type) {
      case RIGHTMOUSE:
        t->state = TRANS_CANCEL;
        handled = true;
        break;
      /* enforce redraw of transform when modifiers are used */
      case LEFTSHIFTKEY:
      case RIGHTSHIFTKEY:
        t->modifiers |= MOD_CONSTRAINT_PLANE;
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;

      case SPACEKEY:
        t->state = TRANS_CONFIRM;
        handled = true;
        break;

      case MIDDLEMOUSE:
        if ((t->flag & T_NO_CONSTRAINT) == 0) {
          /* exception for switching to dolly, or trackball, in camera view */
          if (t->flag & T_CAMERA) {
            if (t->mode == TFM_TRANSLATION) {
              setLocalConstraint(t, (CON_AXIS2), TIP_("along local Z"));
            }
            else if (t->mode == TFM_ROTATION) {
              restoreTransObjects(t);
              initTrackball(t);
            }
          }
          else {
            t->modifiers |= MOD_CONSTRAINT_SELECT;
            if (t->con.mode & CON_APPLY) {
              stopConstraint(t);
            }
            else {
              if (event->shift) {
                /* bit hackish... but it prevents mmb select to print the
                 * orientation from menu */
                float mati[3][3];
                strcpy(t->spacename, "global");
                unit_m3(mati);
                initSelectConstraint(t, mati);
              }
              else {
                initSelectConstraint(t, t->spacemtx);
              }
              postSelectConstraint(t);
            }
          }
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case ESCKEY:
        t->state = TRANS_CANCEL;
        handled = true;
        break;
      case PADENTER:
      case RETKEY:
        t->state = TRANS_CONFIRM;
        handled = true;
        break;
      case GKEY:
        /* only switch when... */
        if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL)) {
          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          initTranslation(t);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case SKEY:
        /* only switch when... */
        if (ELEM(t->mode, TFM_ROTATION, TFM_TRANSLATION, TFM_TRACKBALL)) {
          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          initResize(t);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case RKEY:
        /* only switch when... */
        if (!(t->options & CTX_TEXTURE)) {
          if (ELEM(t->mode, TFM_ROTATION, TFM_RESIZE, TFM_TRACKBALL, TFM_TRANSLATION)) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);

            if (t->mode == TFM_ROTATION) {
              initTrackball(t);
            }
            else {
              initRotation(t);
            }
            initSnapping(t, NULL);  // need to reinit after mode change
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        break;
      case CKEY:
        if (event->alt) {
          if (!(t->options & CTX_NO_PET)) {
            t->flag ^= T_PROP_CONNECTED;
            sort_trans_data_dist(t);
            calculatePropRatio(t);
            t->redraw = TREDRAW_HARD;
            handled = true;
          }
        }
        break;
      case OKEY:
        if (t->flag & T_PROP_EDIT && event->shift) {
          t->prop_mode = (t->prop_mode + 1) % PROP_MODE_MAX;
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case PADPLUSKEY:
        if (event->alt && t->flag & T_PROP_EDIT) {
          t->prop_size *= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO) {
            t->prop_size = min_ff(t->prop_size, ((View3D *)t->view)->clip_end);
          }
          calculatePropRatio(t);
          t->redraw = TREDRAW_HARD;
          handled = true;
        }
        break;
      case PAGEUPKEY:
      case WHEELDOWNMOUSE:
        if (t->flag & T_AUTOIK) {
          transform_autoik_update(t, 1);
        }
        else {
          view_editmove(event->type);
        }
        t->redraw = TREDRAW_HARD;
        handled = true;
        break;
      case PADMINUS:
        if (event->alt && t->flag & T_PROP_EDIT) {
          t->prop_size /= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          calculatePropRatio(t);
          t->redraw = TREDRAW_HARD;
          handled = true;
        }
        break;
      case PAGEDOWNKEY:
      case WHEELUPMOUSE:
        if (t->flag & T_AUTOIK) {
          transform_autoik_update(t, -1);
        }
        else {
          view_editmove(event->type);
        }
        t->redraw = TREDRAW_HARD;
        handled = true;
        break;
      case LEFTALTKEY:
      case RIGHTALTKEY:
        if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D)) {
          t->flag |= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case NKEY:
        if (ELEM(t->mode, TFM_ROTATION)) {
          if ((t->flag & T_EDIT) && t->obedit_type == OB_MESH) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);
            initNormalRotation(t);
            t->redraw = TREDRAW_HARD;
            handled = true;
          }
        }
        break;
      default:
        break;
    }

    /* Snapping key events */
    t->redraw |= handleSnapping(t, event);
  }
  else if (event->val == KM_RELEASE) {
    switch (event->type) {
      case LEFTSHIFTKEY:
      case RIGHTSHIFTKEY:
        t->modifiers &= ~MOD_CONSTRAINT_PLANE;
        t->redraw |= TREDRAW_HARD;
        handled = true;
        break;

      case MIDDLEMOUSE:
        if ((t->flag & T_NO_CONSTRAINT) == 0) {
          t->modifiers &= ~MOD_CONSTRAINT_SELECT;
          postSelectConstraint(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case LEFTALTKEY:
      case RIGHTALTKEY:
        if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D)) {
          t->flag &= ~T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      default:
        break;
    }

    /* confirm transform if launch key is released after mouse move */
    if (t->flag & T_RELEASE_CONFIRM) {
      /* XXX Keyrepeat bug in Xorg messes this up, will test when fixed */
      if ((event->type == t->launch_event) && ISMOUSE(t->launch_event)) {
        t->state = TRANS_CONFIRM;
      }
    }
  }

  /* if we change snap options, get the unsnapped values back */
  if ((mode_prev != t->mode) || ((t->modifiers & (MOD_SNAP | MOD_SNAP_INVERT)) !=
                                 (modifiers_prev & (MOD_SNAP | MOD_SNAP_INVERT)))) {
    applyMouseInput(t, &t->mouse, t->mval, t->values);
  }

  /* Per transform event, if present */
  if (t->handleEvent && (!handled ||
                         /* Needed for vertex slide, see [#38756] */
                         (event->type == MOUSEMOVE))) {
    t->redraw |= t->handleEvent(t, event);
  }

  /* Try to init modal numinput now, if possible. */
  if (!(handled || t->redraw) && ((event->val == KM_PRESS) || (event->type == EVT_MODAL_MAP)) &&
      handleNumInput(t->context, &(t->num), event)) {
    t->redraw |= TREDRAW_HARD;
    handled = true;
  }

  if (t->redraw && !ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
    WM_window_status_area_tag_redraw(CTX_wm_window(t->context));
  }

  if (handled || t->redraw) {
    return 0;
  }
  else {
    return OPERATOR_PASS_THROUGH;
  }
}

bool calculateTransformCenter(bContext *C, int centerMode, float cent3d[3], float cent2d[2])
{
  TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
  bool success;

  t->context = C;

  t->state = TRANS_RUNNING;

  /* avoid calculating PET */
  t->options = CTX_NO_PET;

  t->mode = TFM_DUMMY;

  initTransInfo(C, t, NULL, NULL);

  /* avoid doing connectivity lookups (when V3D_AROUND_LOCAL_ORIGINS is set) */
  t->around = V3D_AROUND_CENTER_BOUNDS;

  createTransData(C, t);  // make TransData structs from selection

  t->around = centerMode;  // override userdefined mode

  if (t->data_len_all == 0) {
    success = false;
  }
  else {
    success = true;

    calculateCenter(t);

    if (cent2d) {
      copy_v2_v2(cent2d, t->center2d);
    }

    if (cent3d) {
      // Copy center from constraint center. Transform center can be local
      copy_v3_v3(cent3d, t->center_global);
    }
  }

  /* aftertrans does insert keyframes, and clears base flags; doesn't read transdata */
  special_aftertrans_update(C, t);

  postTrans(C, t);

  MEM_freeN(t);

  return success;
}

typedef enum {
  UP,
  DOWN,
  LEFT,
  RIGHT,
} ArrowDirection;

#define POS_INDEX 0
/* NOTE: this --^ is a bit hackish, but simplifies GPUVertFormat usage among functions
 * private to this file  - merwin
 */

static void drawArrow(ArrowDirection d, short offset, short length, short size)
{
  immBegin(GPU_PRIM_LINES, 6);

  switch (d) {
    case LEFT:
      offset = -offset;
      length = -length;
      size = -size;
      ATTR_FALLTHROUGH;
    case RIGHT:
      immVertex2f(POS_INDEX, offset, 0);
      immVertex2f(POS_INDEX, offset + length, 0);
      immVertex2f(POS_INDEX, offset + length, 0);
      immVertex2f(POS_INDEX, offset + length - size, -size);
      immVertex2f(POS_INDEX, offset + length, 0);
      immVertex2f(POS_INDEX, offset + length - size, size);
      break;

    case DOWN:
      offset = -offset;
      length = -length;
      size = -size;
      ATTR_FALLTHROUGH;
    case UP:
      immVertex2f(POS_INDEX, 0, offset);
      immVertex2f(POS_INDEX, 0, offset + length);
      immVertex2f(POS_INDEX, 0, offset + length);
      immVertex2f(POS_INDEX, -size, offset + length - size);
      immVertex2f(POS_INDEX, 0, offset + length);
      immVertex2f(POS_INDEX, size, offset + length - size);
      break;
  }

  immEnd();
}

static void drawArrowHead(ArrowDirection d, short size)
{
  immBegin(GPU_PRIM_LINES, 4);

  switch (d) {
    case LEFT:
      size = -size;
      ATTR_FALLTHROUGH;
    case RIGHT:
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, -size, -size);
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, -size, size);
      break;

    case DOWN:
      size = -size;
      ATTR_FALLTHROUGH;
    case UP:
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, -size, -size);
      immVertex2f(POS_INDEX, 0, 0);
      immVertex2f(POS_INDEX, size, -size);
      break;
  }

  immEnd();
}

static void drawArc(float size, float angle_start, float angle_end, int segments)
{
  float delta = (angle_end - angle_start) / segments;
  float angle;
  int a;

  immBegin(GPU_PRIM_LINE_STRIP, segments + 1);

  for (angle = angle_start, a = 0; a < segments; angle += delta, a++) {
    immVertex2f(POS_INDEX, cosf(angle) * size, sinf(angle) * size);
  }
  immVertex2f(POS_INDEX, cosf(angle_end) * size, sinf(angle_end) * size);

  immEnd();
}

static bool helpline_poll(bContext *C)
{
  ARegion *ar = CTX_wm_region(C);

  if (ar && ar->regiontype == RGN_TYPE_WINDOW) {
    return 1;
  }
  return 0;
}

static void drawHelpline(bContext *UNUSED(C), int x, int y, void *customdata)
{
  TransInfo *t = (TransInfo *)customdata;

  if (t->helpline != HLP_NONE) {
    float cent[2];
    float mval[3] = {
        x,
        y,
        0.0f,
    };
    float tmval[2] = {
        (float)t->mval[0],
        (float)t->mval[1],
    };

    projectFloatViewEx(t, t->center_global, cent, V3D_PROJ_TEST_CLIP_ZERO);
    /* Offset the values for the area region. */
    const float offset[2] = {
        t->ar->winrct.xmin,
        t->ar->winrct.ymin,
    };

    for (int i = 0; i < 2; i++) {
      cent[i] += offset[i];
      tmval[i] += offset[i];
    }

    GPU_matrix_push();

    /* Dashed lines first. */
    if (ELEM(t->helpline, HLP_SPRING, HLP_ANGLE)) {
      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      UNUSED_VARS_NDEBUG(shdr_pos); /* silence warning */
      BLI_assert(shdr_pos == POS_INDEX);

      GPU_line_width(1.0f);

      immBindBuiltinProgram(GPU_SHADER_2D_LINE_DASHED_UNIFORM_COLOR);

      float viewport_size[4];
      GPU_viewport_size_get_f(viewport_size);
      immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

      immUniform1i("colors_len", 0); /* "simple" mode */
      immUniformThemeColor(TH_VIEW_OVERLAY);
      immUniform1f("dash_width", 6.0f);
      immUniform1f("dash_factor", 0.5f);

      immBegin(GPU_PRIM_LINES, 2);
      immVertex2fv(POS_INDEX, cent);
      immVertex2f(POS_INDEX, tmval[0], tmval[1]);
      immEnd();

      immUnbindProgram();
    }

    /* And now, solid lines. */
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    UNUSED_VARS_NDEBUG(pos); /* silence warning */
    BLI_assert(pos == POS_INDEX);
    immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

    switch (t->helpline) {
      case HLP_SPRING:
        immUniformThemeColor(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3fv(mval);
        GPU_matrix_rotate_axis(-RAD2DEGF(atan2f(cent[0] - tmval[0], cent[1] - tmval[1])), 'Z');

        GPU_line_width(3.0f);
        drawArrow(UP, 5, 10, 5);
        drawArrow(DOWN, 5, 10, 5);
        break;
      case HLP_HARROW:
        immUniformThemeColor(TH_VIEW_OVERLAY);
        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);
        drawArrow(RIGHT, 5, 10, 5);
        drawArrow(LEFT, 5, 10, 5);
        break;
      case HLP_VARROW:
        immUniformThemeColor(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);
        drawArrow(UP, 5, 10, 5);
        drawArrow(DOWN, 5, 10, 5);
        break;
      case HLP_CARROW: {
        /* Draw arrow based on direction defined by custom-points. */
        immUniformThemeColor(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);

        const int *data = t->mouse.data;
        const float dx = data[2] - data[0], dy = data[3] - data[1];
        const float angle = -atan2f(dx, dy);

        GPU_matrix_push();

        GPU_matrix_rotate_axis(RAD2DEGF(angle), 'Z');

        drawArrow(UP, 5, 10, 5);
        drawArrow(DOWN, 5, 10, 5);

        GPU_matrix_pop();
        break;
      }
      case HLP_ANGLE: {
        float dx = tmval[0] - cent[0], dy = tmval[1] - cent[1];
        float angle = atan2f(dy, dx);
        float dist = hypotf(dx, dy);
        float delta_angle = min_ff(15.0f / dist, (float)M_PI / 4.0f);
        float spacing_angle = min_ff(5.0f / dist, (float)M_PI / 12.0f);

        immUniformThemeColor(TH_VIEW_OVERLAY);

        GPU_matrix_translate_3f(cent[0] - tmval[0] + mval[0], cent[1] - tmval[1] + mval[1], 0);

        GPU_line_width(3.0f);
        drawArc(dist, angle - delta_angle, angle - spacing_angle, 10);
        drawArc(dist, angle + spacing_angle, angle + delta_angle, 10);

        GPU_matrix_push();

        GPU_matrix_translate_3f(
            cosf(angle - delta_angle) * dist, sinf(angle - delta_angle) * dist, 0);
        GPU_matrix_rotate_axis(RAD2DEGF(angle - delta_angle), 'Z');

        drawArrowHead(DOWN, 5);

        GPU_matrix_pop();

        GPU_matrix_translate_3f(
            cosf(angle + delta_angle) * dist, sinf(angle + delta_angle) * dist, 0);
        GPU_matrix_rotate_axis(RAD2DEGF(angle + delta_angle), 'Z');

        drawArrowHead(UP, 5);
        break;
      }
      case HLP_TRACKBALL: {
        unsigned char col[3], col2[3];
        UI_GetThemeColor3ubv(TH_GRID, col);

        GPU_matrix_translate_3fv(mval);

        GPU_line_width(3.0f);

        UI_make_axis_color(col, col2, 'X');
        immUniformColor3ubv(col2);

        drawArrow(RIGHT, 5, 10, 5);
        drawArrow(LEFT, 5, 10, 5);

        UI_make_axis_color(col, col2, 'Y');
        immUniformColor3ubv(col2);

        drawArrow(UP, 5, 10, 5);
        drawArrow(DOWN, 5, 10, 5);
        break;
      }
    }

    immUnbindProgram();
    GPU_matrix_pop();
  }
}

static bool transinfo_show_overlay(const struct bContext *C, TransInfo *t, ARegion *ar)
{
  /* Don't show overlays when not the active view and when overlay is disabled: T57139 */
  bool ok = false;
  if (ar == t->ar) {
    ok = true;
  }
  else {
    ScrArea *sa = CTX_wm_area(C);
    if (sa->spacetype == SPACE_VIEW3D) {
      View3D *v3d = sa->spacedata.first;
      if ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) {
        ok = true;
      }
    }
  }
  return ok;
}

static void drawTransformView(const struct bContext *C, ARegion *ar, void *arg)
{
  TransInfo *t = arg;

  if (!transinfo_show_overlay(C, t, ar)) {
    return;
  }

  GPU_line_width(1.0f);

  drawConstraint(t);
  drawPropCircle(C, t);
  drawSnapping(C, t);

  if (ar == t->ar) {
    /* edge slide, vert slide */
    drawEdgeSlide(t);
    drawVertSlide(t);

    /* Rotation */
    drawDial3d(t);
  }
}

/* just draw a little warning message in the top-right corner of the viewport
 * to warn that autokeying is enabled */
static void drawAutoKeyWarning(TransInfo *UNUSED(t), ARegion *ar)
{
  rcti rect;
  const char *printable = IFACE_("Auto Keying On");
  float printable_size[2];
  int xco, yco;

  ED_region_visible_rect(ar, &rect);

  const int font_id = BLF_default();
  BLF_width_and_height(
      font_id, printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);

  xco = (rect.xmax - U.widget_unit) - (int)printable_size[0];
  yco = (rect.ymax - U.widget_unit);

  /* warning text (to clarify meaning of overlays)
   * - original color was red to match the icon, but that clashes badly with a less nasty border
   */
  unsigned char color[3];
  UI_GetThemeColorShade3ubv(TH_TEXT_HI, -50, color);
  BLF_color3ubv(font_id, color);
#ifdef WITH_INTERNATIONAL
  BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#else
  BLF_draw_default_ascii(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);
#endif

  /* autokey recording icon... */
  GPU_blend_set_func_separate(
      GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
  GPU_blend(true);

  xco -= U.widget_unit;
  yco -= (int)printable_size[1] / 2;

  UI_icon_draw(xco, yco, ICON_REC);

  GPU_blend(false);
}

static void drawTransformPixel(const struct bContext *C, ARegion *ar, void *arg)
{
  TransInfo *t = arg;

  if (!transinfo_show_overlay(C, t, ar)) {
    return;
  }

  if (ar == t->ar) {
    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);

    /* draw auto-key-framing hint in the corner
     * - only draw if enabled (advanced users may be distracted/annoyed),
     *   for objects that will be autokeyframed (no point otherwise),
     *   AND only for the active region (as showing all is too overwhelming)
     */
    if ((U.autokey_flag & AUTOKEY_FLAG_NOWARNING) == 0) {
      if (ar == t->ar) {
        if (t->flag & (T_OBJECT | T_POSE)) {
          if (ob && autokeyframe_cfra_can_key(scene, &ob->id)) {
            drawAutoKeyWarning(t, ar);
          }
        }
      }
    }
  }
}

/**
 * \see #initTransform which reads values from the operator.
 */
void saveTransform(bContext *C, TransInfo *t, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  int proportional = 0;
  PropertyRNA *prop;

  // Save back mode in case we're in the generic operator
  if ((prop = RNA_struct_find_property(op->ptr, "mode"))) {
    RNA_property_enum_set(op->ptr, prop, t->mode);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "value"))) {
    float values[4];

    copy_v4_v4(values, (t->flag & T_AUTOVALUES) ? t->auto_values : t->values);

    if (RNA_property_array_check(prop)) {
      RNA_property_float_set_array(op->ptr, prop, values);
    }
    else {
      RNA_property_float_set(op->ptr, prop, values[0]);
    }
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    if (t->flag & T_PROP_EDIT) {
      proportional |= PROP_EDIT_USE;
    }
    if (t->flag & T_PROP_CONNECTED) {
      proportional |= PROP_EDIT_CONNECTED;
    }
    if (t->flag & T_PROP_PROJECTED) {
      proportional |= PROP_EDIT_PROJECTED;
    }
  }

  // If modal, save settings back in scene if not set as operator argument
  if ((t->flag & T_MODAL) || (op->flag & OP_IS_REPEAT)) {
    /* save settings if not set in operator */

    /* skip saving proportional edit if it was not actually used */
    if (!(t->options & CTX_NO_PET)) {
      if ((prop = RNA_struct_find_property(op->ptr, "use_proportional_edit")) &&
          !RNA_property_is_set(op->ptr, prop)) {
        if (t->spacetype == SPACE_GRAPH) {
          ts->proportional_fcurve = proportional;
        }
        else if (t->spacetype == SPACE_ACTION) {
          ts->proportional_action = proportional;
        }
        else if (t->obedit_type != -1) {
          ts->proportional_edit = proportional;
        }
        else if (t->options & CTX_MASK) {
          ts->proportional_mask = proportional != 0;
        }
        else {
          ts->proportional_objects = proportional != 0;
        }
      }

      if ((prop = RNA_struct_find_property(op->ptr, "proportional_size"))) {
        ts->proportional_size = RNA_property_is_set(op->ptr, prop) ?
                                    RNA_property_float_get(op->ptr, prop) :
                                    t->prop_size;
      }

      if ((prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) &&
          !RNA_property_is_set(op->ptr, prop)) {
        ts->prop_mode = t->prop_mode;
      }
    }

    /* do we check for parameter? */
    if (transformModeUseSnap(t)) {
      if (t->modifiers & MOD_SNAP) {
        ts->snap_flag |= SCE_SNAP;
      }
      else {
        ts->snap_flag &= ~SCE_SNAP;
      }
    }

    if (t->spacetype == SPACE_VIEW3D) {
      if ((prop = RNA_struct_find_property(op->ptr, "orient_type")) &&
          !RNA_property_is_set(op->ptr, prop) &&
          (t->orientation.user != V3D_ORIENT_CUSTOM_MATRIX)) {
        TransformOrientationSlot *orient_slot = &t->scene->orientation_slots[SCE_ORIENT_DEFAULT];
        orient_slot->type = t->orientation.user;
        BLI_assert(((orient_slot->index_custom == -1) && (t->orientation.custom == NULL)) ||
                   (BKE_scene_transform_orientation_get_index(t->scene, t->orientation.custom) ==
                    orient_slot->index_custom));
      }
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_proportional_edit"))) {
    RNA_property_boolean_set(op->ptr, prop, proportional & PROP_EDIT_USE);
    RNA_boolean_set(op->ptr, "use_proportional_connected", proportional & PROP_EDIT_CONNECTED);
    RNA_boolean_set(op->ptr, "use_proportional_projected", proportional & PROP_EDIT_PROJECTED);
    RNA_enum_set(op->ptr, "proportional_edit_falloff", t->prop_mode);
    RNA_float_set(op->ptr, "proportional_size", t->prop_size);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "mirror"))) {
    RNA_property_boolean_set(op->ptr, prop, (t->flag & T_NO_MIRROR) == 0);
  }

  /* Orientation used for redo. */
  const bool use_orient_axis = (t->orient_matrix_is_set &&
                                (RNA_struct_find_property(op->ptr, "orient_axis") != NULL));
  short orientation;
  if (t->con.mode & CON_APPLY) {
    orientation = t->con.orientation;
    if (orientation == V3D_ORIENT_CUSTOM) {
      const int orientation_index_custom = BKE_scene_transform_orientation_get_index(
          t->scene, t->orientation.custom);
      /* Maybe we need a t->con.custom_orientation?
       * Seems like it would always match t->orientation.custom. */
      orientation = V3D_ORIENT_CUSTOM + orientation_index_custom;
      BLI_assert(orientation >= V3D_ORIENT_CUSTOM);
    }
  }
  else if ((t->orientation.user == V3D_ORIENT_CUSTOM_MATRIX) &&
           (prop = RNA_struct_find_property(op->ptr, "orient_matrix_type"))) {
    orientation = RNA_property_enum_get(op->ptr, prop);
  }
  else if (use_orient_axis) {
    /* We're not using an orientation, use the fallback. */
    orientation = t->orientation.unset;
  }
  else {
    orientation = V3D_ORIENT_GLOBAL;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis"))) {
    if (t->flag & T_MODAL) {
      if (t->con.mode & CON_APPLY) {
        int orient_axis = constraintModeToIndex(t);
        if (orient_axis != -1) {
          RNA_property_enum_set(op->ptr, prop, orient_axis);
        }
      }
      else {
        RNA_property_enum_set(op->ptr, prop, t->orient_axis);
      }
    }
  }
  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis_ortho"))) {
    if (t->flag & T_MODAL) {
      RNA_property_enum_set(op->ptr, prop, t->orient_axis_ortho);
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "orient_matrix"))) {
    if (t->flag & T_MODAL) {
      if (orientation != V3D_ORIENT_CUSTOM_MATRIX) {
        if (t->flag & T_MODAL) {
          RNA_enum_set(op->ptr, "orient_matrix_type", orientation);
        }
      }
      if (t->con.mode & CON_APPLY) {
        RNA_float_set_array(op->ptr, "orient_matrix", &t->con.mtx[0][0]);
      }
      else if (use_orient_axis) {
        RNA_float_set_array(op->ptr, "orient_matrix", &t->orient_matrix[0][0]);
      }
      else {
        RNA_float_set_array(op->ptr, "orient_matrix", &t->spacemtx[0][0]);
      }
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "orient_type"))) {
    /* constraint orientation can be global, even if user selects something else
     * so use the orientation in the constraint if set */

    /* Use 'orient_matrix' instead. */
    if (t->flag & T_MODAL) {
      if (orientation != V3D_ORIENT_CUSTOM_MATRIX) {
        RNA_property_enum_set(op->ptr, prop, orientation);
      }
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis"))) {
    bool constraint_axis[3] = {false, false, false};
    if (t->flag & T_MODAL) {
      /* Only set if needed, so we can hide in the UI when nothing is set.
       * See 'transform_poll_property'. */
      if (t->con.mode & CON_APPLY) {
        if (t->con.mode & CON_AXIS0) {
          constraint_axis[0] = true;
        }
        if (t->con.mode & CON_AXIS1) {
          constraint_axis[1] = true;
        }
        if (t->con.mode & CON_AXIS2) {
          constraint_axis[2] = true;
        }
      }
      if (ELEM(true, UNPACK3(constraint_axis))) {
        RNA_property_boolean_set_array(op->ptr, prop, constraint_axis);
      }
    }
  }

  {
    const char *prop_id = NULL;
    bool prop_state = true;
    if (t->mode == TFM_SHRINKFATTEN) {
      prop_id = "use_even_offset";
      prop_state = false;
    }

    if (prop_id && (prop = RNA_struct_find_property(op->ptr, prop_id))) {
      RNA_property_boolean_set(op->ptr, prop, ((t->flag & T_ALT_TRANSFORM) == 0) == prop_state);
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "correct_uv"))) {
    RNA_property_boolean_set(
        op->ptr, prop, (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) != 0);
  }

  if (t->mode == TFM_SHEAR) {
    prop = RNA_struct_find_property(op->ptr, "shear_axis");
    t->custom.mode.data = POINTER_FROM_INT(RNA_property_enum_get(op->ptr, prop));
    RNA_property_enum_set(op->ptr, prop, POINTER_AS_INT(t->custom.mode.data));
  }
}

/**
 * \note  caller needs to free 't' on a 0 return
 * \warning  \a event might be NULL (when tweaking from redo panel)
 * \see #saveTransform which writes these values back.
 */
bool initTransform(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event, int mode)
{
  int options = 0;
  PropertyRNA *prop;

  t->context = C;

  /* added initialize, for external calls to set stuff in TransInfo, like undo string */

  t->state = TRANS_STARTING;

  if ((prop = RNA_struct_find_property(op->ptr, "cursor_transform")) &&
      RNA_property_is_set(op->ptr, prop)) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_CURSOR;
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "texture_space")) &&
      RNA_property_is_set(op->ptr, prop)) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_TEXTURE;
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "gpencil_strokes")) &&
      RNA_property_is_set(op->ptr, prop)) {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_GPENCIL_STROKES;
    }
  }

  t->options = options;

  t->mode = mode;

  /* Needed to translate tweak events to mouse buttons. */
  t->launch_event = event ? WM_userdef_event_type_from_keymap_type(event->type) : -1;

  /* XXX Remove this when wm_operator_call_internal doesn't use window->eventstate
   * (which can have type = 0) */
  /* For gizmo only, so assume LEFTMOUSE. */
  if (t->launch_event == 0) {
    t->launch_event = LEFTMOUSE;
  }

  unit_m3(t->spacemtx);

  initTransInfo(C, t, op, event);
  initTransformOrientation(C, t);

  if (t->spacetype == SPACE_VIEW3D) {
    t->draw_handle_apply = ED_region_draw_cb_activate(
        t->ar->type, drawTransformApply, t, REGION_DRAW_PRE_VIEW);
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_pixel = ED_region_draw_cb_activate(
        t->ar->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, helpline_poll, drawHelpline, t);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, helpline_poll, drawHelpline, t);
  }
  else if (t->spacetype == SPACE_CLIP) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, helpline_poll, drawHelpline, t);
  }
  else if (t->spacetype == SPACE_NODE) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, helpline_poll, drawHelpline, t);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, helpline_poll, drawHelpline, t);
  }
  else if (t->spacetype == SPACE_ACTION) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->ar->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        CTX_wm_manager(C), SPACE_TYPE_ANY, RGN_TYPE_ANY, helpline_poll, drawHelpline, t);
  }

  createTransData(C, t);  // make TransData structs from selection

  if (t->data_len_all == 0) {
    postTrans(C, t);
    return 0;
  }

  if (event) {
    /* keymap for shortcut header prints */
    t->keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);

    /* Stupid code to have Ctrl-Click on gizmo work ok.
     *
     * Do this only for translation/rotation/resize because only these
     * modes are available from gizmo and doing such check could
     * lead to keymap conflicts for other modes (see #31584)
     */
    if (ELEM(mode, TFM_TRANSLATION, TFM_ROTATION, TFM_RESIZE)) {
      wmKeyMapItem *kmi;

      for (kmi = t->keymap->items.first; kmi; kmi = kmi->next) {
        if (kmi->flag & KMI_INACTIVE) {
          continue;
        }

        if (kmi->propvalue == TFM_MODAL_SNAP_INV_ON && kmi->val == KM_PRESS) {
          if ((ELEM(kmi->type, LEFTCTRLKEY, RIGHTCTRLKEY) && event->ctrl) ||
              (ELEM(kmi->type, LEFTSHIFTKEY, RIGHTSHIFTKEY) && event->shift) ||
              (ELEM(kmi->type, LEFTALTKEY, RIGHTALTKEY) && event->alt) ||
              ((kmi->type == OSKEY) && event->oskey)) {
            t->modifiers |= MOD_SNAP_INVERT;
          }
          break;
        }
      }
    }
  }

  initSnapping(t, op);  // Initialize snapping data AFTER mode flags

  initSnapSpatial(t, t->snap_spatial);

  /* EVIL! posemode code can switch translation to rotate when 1 bone is selected.
   * will be removed (ton) */

  /* EVIL2: we gave as argument also texture space context bit... was cleared */

  /* EVIL3: extend mode for animation editors also switches modes...
   * but is best way to avoid duplicate code */
  mode = t->mode;

  calculatePropRatio(t);
  calculateCenter(t);

  /* Overwrite initial values if operator supplied a non-null vector.
   *
   * Run before init functions so 'values_modal_offset' can be applied on mouse input.
   */
  BLI_assert(is_zero_v4(t->values_modal_offset));
  if ((prop = RNA_struct_find_property(op->ptr, "value")) && RNA_property_is_set(op->ptr, prop)) {
    float values[4] = {0}; /* in case value isn't length 4, avoid uninitialized memory  */

    if (RNA_property_array_check(prop)) {
      RNA_float_get_array(op->ptr, "value", values);
    }
    else {
      values[0] = RNA_float_get(op->ptr, "value");
    }

    copy_v4_v4(t->values, values);

    if (t->flag & T_MODAL) {
      copy_v4_v4(t->values_modal_offset, values);
      t->redraw = TREDRAW_HARD;
    }
    else {
      copy_v4_v4(t->auto_values, values);
      t->flag |= T_AUTOVALUES;
    }
  }

  if (event) {
    /* Initialize accurate transform to settings requested by keymap. */
    bool use_accurate = false;
    if ((prop = RNA_struct_find_property(op->ptr, "use_accurate")) &&
        RNA_property_is_set(op->ptr, prop)) {
      if (RNA_property_boolean_get(op->ptr, prop)) {
        use_accurate = true;
      }
    }
    initMouseInput(t, &t->mouse, t->center2d, event->mval, use_accurate);
  }

  switch (mode) {
    case TFM_TRANSLATION:
      initTranslation(t);
      break;
    case TFM_ROTATION:
      initRotation(t);
      break;
    case TFM_RESIZE:
      initResize(t);
      break;
    case TFM_SKIN_RESIZE:
      initSkinResize(t);
      break;
    case TFM_TOSPHERE:
      initToSphere(t);
      break;
    case TFM_SHEAR:
      prop = RNA_struct_find_property(op->ptr, "shear_axis");
      t->custom.mode.data = POINTER_FROM_INT(RNA_property_enum_get(op->ptr, prop));
      initShear(t);
      break;
    case TFM_BEND:
      initBend(t);
      break;
    case TFM_SHRINKFATTEN:
      initShrinkFatten(t);
      break;
    case TFM_TILT:
      initTilt(t);
      break;
    case TFM_CURVE_SHRINKFATTEN:
      initCurveShrinkFatten(t);
      break;
    case TFM_MASK_SHRINKFATTEN:
      initMaskShrinkFatten(t);
      break;
    case TFM_GPENCIL_SHRINKFATTEN:
      initGPShrinkFatten(t);
      break;
    case TFM_TRACKBALL:
      initTrackball(t);
      break;
    case TFM_PUSHPULL:
      initPushPull(t);
      break;
    case TFM_CREASE:
      initCrease(t);
      break;
    case TFM_BONESIZE: { /* used for both B-Bone width (bonesize) as for deform-dist (envelope) */
      /* Note: we have to pick one, use the active object. */
      TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_OK(t);
      bArmature *arm = tc->poseobj->data;
      if (arm->drawtype == ARM_ENVELOPE) {
        initBoneEnvelope(t);
        t->mode = TFM_BONE_ENVELOPE_DIST;
      }
      else {
        initBoneSize(t);
      }
      break;
    }
    case TFM_BONE_ENVELOPE:
      initBoneEnvelope(t);
      break;
    case TFM_BONE_ENVELOPE_DIST:
      initBoneEnvelope(t);
      t->mode = TFM_BONE_ENVELOPE_DIST;
      break;
    case TFM_EDGE_SLIDE:
    case TFM_VERT_SLIDE: {
      const bool use_even = (op ? RNA_boolean_get(op->ptr, "use_even") : false);
      const bool flipped = (op ? RNA_boolean_get(op->ptr, "flipped") : false);
      const bool use_clamp = (op ? RNA_boolean_get(op->ptr, "use_clamp") : true);
      if (mode == TFM_EDGE_SLIDE) {
        const bool use_double_side = (op ? !RNA_boolean_get(op->ptr, "single_side") : true);
        initEdgeSlide_ex(t, use_double_side, use_even, flipped, use_clamp);
      }
      else {
        initVertSlide_ex(t, use_even, flipped, use_clamp);
      }
      break;
    }
    case TFM_BONE_ROLL:
      initBoneRoll(t);
      break;
    case TFM_TIME_TRANSLATE:
      initTimeTranslate(t);
      break;
    case TFM_TIME_SLIDE:
      initTimeSlide(t);
      break;
    case TFM_TIME_SCALE:
      initTimeScale(t);
      break;
    case TFM_TIME_DUPLICATE:
      /* same as TFM_TIME_EXTEND, but we need the mode info for later
       * so that duplicate-culling will work properly
       */
      if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
        initTranslation(t);
      }
      else {
        initTimeTranslate(t);
      }
      t->mode = mode;
      break;
    case TFM_TIME_EXTEND:
      /* now that transdata has been made, do like for TFM_TIME_TRANSLATE (for most Animation
       * Editors because they have only 1D transforms for time values) or TFM_TRANSLATION
       * (for Graph/NLA Editors only since they uses 'standard' transforms to get 2D movement)
       * depending on which editor this was called from
       */
      if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
        initTranslation(t);
      }
      else {
        initTimeTranslate(t);
      }
      break;
    case TFM_BAKE_TIME:
      initBakeTime(t);
      break;
    case TFM_MIRROR:
      initMirror(t);
      break;
    case TFM_BWEIGHT:
      initBevelWeight(t);
      break;
    case TFM_ALIGN:
      initAlign(t);
      break;
    case TFM_SEQ_SLIDE:
      initSeqSlide(t);
      break;
    case TFM_NORMAL_ROTATION:
      initNormalRotation(t);
      break;
    case TFM_GPENCIL_OPACITY:
      initGPOpacity(t);
      break;
  }

  if (t->state == TRANS_CANCEL) {
    postTrans(C, t);
    return 0;
  }

  /* Transformation axis from operator */
  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis")) &&
      RNA_property_is_set(op->ptr, prop)) {
    t->orient_axis = RNA_property_enum_get(op->ptr, prop);
  }
  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis_ortho")) &&
      RNA_property_is_set(op->ptr, prop)) {
    t->orient_axis_ortho = RNA_property_enum_get(op->ptr, prop);
  }

  /* Constraint init from operator */
  if ((t->flag & T_MODAL) ||
      /* For mirror operator the constraint axes are effectively the values. */
      (RNA_struct_find_property(op->ptr, "value") == NULL)) {
    if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis")) &&
        RNA_property_is_set(op->ptr, prop)) {
      bool constraint_axis[3];

      RNA_property_boolean_get_array(op->ptr, prop, constraint_axis);

      if (constraint_axis[0] || constraint_axis[1] || constraint_axis[2]) {
        t->con.mode |= CON_APPLY;

        if (constraint_axis[0]) {
          t->con.mode |= CON_AXIS0;
        }
        if (constraint_axis[1]) {
          t->con.mode |= CON_AXIS1;
        }
        if (constraint_axis[2]) {
          t->con.mode |= CON_AXIS2;
        }

        setUserConstraint(t, t->orientation.user, t->con.mode, "%s");
      }
    }
  }
  else {
    /* So we can adjust in non global orientation. */
    if (t->orientation.user != V3D_ORIENT_GLOBAL) {
      t->con.mode |= CON_APPLY | CON_AXIS0 | CON_AXIS1 | CON_AXIS2;
      setUserConstraint(t, t->orientation.user, t->con.mode, "%s");
    }
  }

  /* Don't write into the values when non-modal because they are already set from operator redo
   * values. */
  if (t->flag & T_MODAL) {
    /* Setup the mouse input with initial values. */
    applyMouseInput(t, &t->mouse, t->mouse.imval, t->values);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "preserve_clnor"))) {
    if ((t->flag & T_EDIT) && t->obedit_type == OB_MESH) {

      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        if ((((Mesh *)(tc->obedit->data))->flag & ME_AUTOSMOOTH)) {
          BMEditMesh *em = NULL;  // BKE_editmesh_from_object(t->obedit);
          bool do_skip = false;

          /* Currently only used for two of three most frequent transform ops,
           * can include more ops.
           * Note that scaling cannot be included here,
           * non-uniform scaling will affect normals. */
          if (ELEM(t->mode, TFM_TRANSLATION, TFM_ROTATION)) {
            if (em->bm->totvertsel == em->bm->totvert) {
              /* No need to invalidate if whole mesh is selected. */
              do_skip = true;
            }
          }

          if (t->flag & T_MODAL) {
            RNA_property_boolean_set(op->ptr, prop, false);
          }
          else if (!do_skip) {
            const bool preserve_clnor = RNA_property_boolean_get(op->ptr, prop);
            if (preserve_clnor) {
              BKE_editmesh_lnorspace_update(em);
              t->flag |= T_CLNOR_REBUILD;
            }
            BM_lnorspace_invalidate(em->bm, true);
          }
        }
      }
    }
  }

  t->context = NULL;

  return 1;
}

void transformApply(bContext *C, TransInfo *t)
{
  t->context = C;

  if ((t->redraw & TREDRAW_HARD) || (t->draw_handle_apply == NULL && (t->redraw & TREDRAW_SOFT))) {
    selectConstraint(t);
    if (t->transform) {
      t->transform(t, t->mval);  // calls recalcData()
      viewRedrawForce(C, t);
    }
    t->redraw = TREDRAW_NOTHING;
  }
  else if (t->redraw & TREDRAW_SOFT) {
    viewRedrawForce(C, t);
  }

  /* If auto confirm is on, break after one pass */
  if (t->options & CTX_AUTOCONFIRM) {
    t->state = TRANS_CONFIRM;
  }

  t->context = NULL;
}

static void drawTransformApply(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
  TransInfo *t = arg;

  if (t->redraw & TREDRAW_SOFT) {
    t->redraw |= TREDRAW_HARD;
    transformApply((bContext *)C, t);
  }
}

int transformEnd(bContext *C, TransInfo *t)
{
  int exit_code = OPERATOR_RUNNING_MODAL;

  t->context = C;

  if (t->state != TRANS_STARTING && t->state != TRANS_RUNNING) {
    /* handle restoring objects */
    if (t->state == TRANS_CANCEL) {
      /* exception, edge slide transformed UVs too */
      if (t->mode == TFM_EDGE_SLIDE) {
        doEdgeSlide(t, 0.0f);
      }
      else if (t->mode == TFM_VERT_SLIDE) {
        doVertSlide(t, 0.0f);
      }

      exit_code = OPERATOR_CANCELLED;
      restoreTransObjects(t);  // calls recalcData()
    }
    else {
      if (t->flag & T_CLNOR_REBUILD) {
        FOREACH_TRANS_DATA_CONTAINER (t, tc) {
          BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
          BM_lnorspace_rebuild(em->bm, true);
        }
      }
      exit_code = OPERATOR_FINISHED;
    }

    /* aftertrans does insert keyframes, and clears base flags; doesn't read transdata */
    special_aftertrans_update(C, t);

    /* free data */
    postTrans(C, t);

    /* send events out for redraws */
    viewRedrawPost(C, t);

    viewRedrawForce(C, t);
  }

  t->context = NULL;

  return exit_code;
}

/* ************************** TRANSFORM LOCKS **************************** */

static void protectedTransBits(short protectflag, float vec[3])
{
  if (protectflag & OB_LOCK_LOCX) {
    vec[0] = 0.0f;
  }
  if (protectflag & OB_LOCK_LOCY) {
    vec[1] = 0.0f;
  }
  if (protectflag & OB_LOCK_LOCZ) {
    vec[2] = 0.0f;
  }
}

static void protectedSizeBits(short protectflag, float size[3])
{
  if (protectflag & OB_LOCK_SCALEX) {
    size[0] = 1.0f;
  }
  if (protectflag & OB_LOCK_SCALEY) {
    size[1] = 1.0f;
  }
  if (protectflag & OB_LOCK_SCALEZ) {
    size[2] = 1.0f;
  }
}

static void protectedRotateBits(short protectflag, float eul[3], const float oldeul[3])
{
  if (protectflag & OB_LOCK_ROTX) {
    eul[0] = oldeul[0];
  }
  if (protectflag & OB_LOCK_ROTY) {
    eul[1] = oldeul[1];
  }
  if (protectflag & OB_LOCK_ROTZ) {
    eul[2] = oldeul[2];
  }
}

/* this function only does the delta rotation */
/* axis-angle is usually internally stored as quats... */
static void protectedAxisAngleBits(
    short protectflag, float axis[3], float *angle, float oldAxis[3], float oldAngle)
{
  /* check that protection flags are set */
  if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0) {
    return;
  }

  if (protectflag & OB_LOCK_ROT4D) {
    /* axis-angle getting limited as 4D entities that they are... */
    if (protectflag & OB_LOCK_ROTW) {
      *angle = oldAngle;
    }
    if (protectflag & OB_LOCK_ROTX) {
      axis[0] = oldAxis[0];
    }
    if (protectflag & OB_LOCK_ROTY) {
      axis[1] = oldAxis[1];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      axis[2] = oldAxis[2];
    }
  }
  else {
    /* axis-angle get limited with euler... */
    float eul[3], oldeul[3];

    axis_angle_to_eulO(eul, EULER_ORDER_DEFAULT, axis, *angle);
    axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, oldAxis, oldAngle);

    if (protectflag & OB_LOCK_ROTX) {
      eul[0] = oldeul[0];
    }
    if (protectflag & OB_LOCK_ROTY) {
      eul[1] = oldeul[1];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      eul[2] = oldeul[2];
    }

    eulO_to_axis_angle(axis, angle, eul, EULER_ORDER_DEFAULT);

    /* When converting to axis-angle,
     * we need a special exception for the case when there is no axis. */
    if (IS_EQF(axis[0], axis[1]) && IS_EQF(axis[1], axis[2])) {
      /* for now, rotate around y-axis then (so that it simply becomes the roll) */
      axis[1] = 1.0f;
    }
  }
}

/* this function only does the delta rotation */
static void protectedQuaternionBits(short protectflag, float quat[4], const float oldquat[4])
{
  /* check that protection flags are set */
  if ((protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) == 0) {
    return;
  }

  if (protectflag & OB_LOCK_ROT4D) {
    /* quaternions getting limited as 4D entities that they are... */
    if (protectflag & OB_LOCK_ROTW) {
      quat[0] = oldquat[0];
    }
    if (protectflag & OB_LOCK_ROTX) {
      quat[1] = oldquat[1];
    }
    if (protectflag & OB_LOCK_ROTY) {
      quat[2] = oldquat[2];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      quat[3] = oldquat[3];
    }
  }
  else {
    /* quaternions get limited with euler... (compatibility mode) */
    float eul[3], oldeul[3], nquat[4], noldquat[4];
    float qlen;

    qlen = normalize_qt_qt(nquat, quat);
    normalize_qt_qt(noldquat, oldquat);

    quat_to_eul(eul, nquat);
    quat_to_eul(oldeul, noldquat);

    if (protectflag & OB_LOCK_ROTX) {
      eul[0] = oldeul[0];
    }
    if (protectflag & OB_LOCK_ROTY) {
      eul[1] = oldeul[1];
    }
    if (protectflag & OB_LOCK_ROTZ) {
      eul[2] = oldeul[2];
    }

    eul_to_quat(quat, eul);

    /* restore original quat size */
    mul_qt_fl(quat, qlen);

    /* quaternions flip w sign to accumulate rotations correctly */
    if ((nquat[0] < 0.0f && quat[0] > 0.0f) || (nquat[0] > 0.0f && quat[0] < 0.0f)) {
      mul_qt_fl(quat, -1.0f);
    }
  }
}

/* ******************* TRANSFORM LIMITS ********************** */

static void constraintTransLim(TransInfo *t, TransData *td)
{
  if (td->con) {
    const bConstraintTypeInfo *ctiLoc = BKE_constraint_typeinfo_from_type(
        CONSTRAINT_TYPE_LOCLIMIT);
    const bConstraintTypeInfo *ctiDist = BKE_constraint_typeinfo_from_type(
        CONSTRAINT_TYPE_DISTLIMIT);

    bConstraintOb cob = {NULL};
    bConstraint *con;
    float ctime = (float)(t->scene->r.cfra);

    /* Make a temporary bConstraintOb for using these limit constraints
     * - they only care that cob->matrix is correctly set ;-)
     * - current space should be local
     */
    unit_m4(cob.matrix);
    copy_v3_v3(cob.matrix[3], td->loc);

    /* Evaluate valid constraints */
    for (con = td->con; con; con = con->next) {
      const bConstraintTypeInfo *cti = NULL;
      ListBase targets = {NULL, NULL};

      /* only consider constraint if enabled */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* only use it if it's tagged for this purpose (and the right type) */
      if (con->type == CONSTRAINT_TYPE_LOCLIMIT) {
        bLocLimitConstraint *data = con->data;

        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }
        cti = ctiLoc;
      }
      else if (con->type == CONSTRAINT_TYPE_DISTLIMIT) {
        bDistLimitConstraint *data = con->data;

        if ((data->flag & LIMITDIST_TRANSFORM) == 0) {
          continue;
        }
        cti = ctiDist;
      }

      if (cti) {
        /* do space conversions */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->mtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }
        else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
          /* skip... incompatible spacetype */
          continue;
        }

        /* get constraint targets if needed */
        BKE_constraint_targets_for_solving_get(t->depsgraph, con, &cob, &targets, ctime);

        /* do constraint */
        cti->evaluate_constraint(con, &cob, &targets);

        /* convert spaces again */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->smtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }

        /* free targets list */
        BLI_freelistN(&targets);
      }
    }

    /* copy results from cob->matrix */
    copy_v3_v3(td->loc, cob.matrix[3]);
  }
}

static void constraintob_from_transdata(bConstraintOb *cob, TransData *td)
{
  /* Make a temporary bConstraintOb for use by limit constraints
   * - they only care that cob->matrix is correctly set ;-)
   * - current space should be local
   */
  memset(cob, 0, sizeof(bConstraintOb));
  if (td->ext) {
    if (td->ext->rotOrder == ROT_MODE_QUAT) {
      /* quats */
      /* objects and bones do normalization first too, otherwise
       * we don't necessarily end up with a rotation matrix, and
       * then conversion back to quat gives a different result */
      float quat[4];
      normalize_qt_qt(quat, td->ext->quat);
      quat_to_mat4(cob->matrix, quat);
    }
    else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
      /* axis angle */
      axis_angle_to_mat4(cob->matrix, td->ext->rotAxis, *td->ext->rotAngle);
    }
    else {
      /* eulers */
      eulO_to_mat4(cob->matrix, td->ext->rot, td->ext->rotOrder);
    }
  }
}

static void constraintRotLim(TransInfo *UNUSED(t), TransData *td)
{
  if (td->con) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_ROTLIMIT);
    bConstraintOb cob;
    bConstraint *con;
    bool do_limit = false;

    /* Evaluate valid constraints */
    for (con = td->con; con; con = con->next) {
      /* only consider constraint if enabled */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* we're only interested in Limit-Rotation constraints */
      if (con->type == CONSTRAINT_TYPE_ROTLIMIT) {
        bRotLimitConstraint *data = con->data;

        /* only use it if it's tagged for this purpose */
        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }

        /* skip incompatible spacetypes */
        if (!ELEM(con->ownspace, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL)) {
          continue;
        }

        /* only do conversion if necessary, to preserve quats and eulers */
        if (do_limit == false) {
          constraintob_from_transdata(&cob, td);
          do_limit = true;
        }

        /* do space conversions */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->mtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }

        /* do constraint */
        cti->evaluate_constraint(con, &cob, NULL);

        /* convert spaces again */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->smtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }
      }
    }

    if (do_limit) {
      /* copy results from cob->matrix */
      if (td->ext->rotOrder == ROT_MODE_QUAT) {
        /* quats */
        mat4_to_quat(td->ext->quat, cob.matrix);
      }
      else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* axis angle */
        mat4_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, cob.matrix);
      }
      else {
        /* eulers */
        mat4_to_eulO(td->ext->rot, td->ext->rotOrder, cob.matrix);
      }
    }
  }
}

static void constraintSizeLim(TransInfo *t, TransData *td)
{
  if (td->con && td->ext) {
    const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(CONSTRAINT_TYPE_SIZELIMIT);
    bConstraintOb cob = {NULL};
    bConstraint *con;
    float size_sign[3], size_abs[3];
    int i;

    /* Make a temporary bConstraintOb for using these limit constraints
     * - they only care that cob->matrix is correctly set ;-)
     * - current space should be local
     */
    if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
      /* scale val and reset size */
      return;  // TODO: fix this case
    }
    else {
      /* Reset val if SINGLESIZE but using a constraint */
      if (td->flag & TD_SINGLESIZE) {
        return;
      }

      /* separate out sign to apply back later */
      for (i = 0; i < 3; i++) {
        size_sign[i] = signf(td->ext->size[i]);
        size_abs[i] = fabsf(td->ext->size[i]);
      }

      size_to_mat4(cob.matrix, size_abs);
    }

    /* Evaluate valid constraints */
    for (con = td->con; con; con = con->next) {
      /* only consider constraint if enabled */
      if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) {
        continue;
      }
      if (con->enforce == 0.0f) {
        continue;
      }

      /* we're only interested in Limit-Scale constraints */
      if (con->type == CONSTRAINT_TYPE_SIZELIMIT) {
        bSizeLimitConstraint *data = con->data;

        /* only use it if it's tagged for this purpose */
        if ((data->flag2 & LIMIT_TRANSFORM) == 0) {
          continue;
        }

        /* do space conversions */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->mtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->mtx, cob.matrix);
        }
        else if (con->ownspace != CONSTRAINT_SPACE_LOCAL) {
          /* skip... incompatible spacetype */
          continue;
        }

        /* do constraint */
        cti->evaluate_constraint(con, &cob, NULL);

        /* convert spaces again */
        if (con->ownspace == CONSTRAINT_SPACE_WORLD) {
          /* just multiply by td->smtx (this should be ok) */
          mul_m4_m3m4(cob.matrix, td->smtx, cob.matrix);
        }
      }
    }

    /* copy results from cob->matrix */
    if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
      /* scale val and reset size */
      return;  // TODO: fix this case
    }
    else {
      /* Reset val if SINGLESIZE but using a constraint */
      if (td->flag & TD_SINGLESIZE) {
        return;
      }

      /* extrace scale from matrix and apply back sign */
      mat4_to_size(td->ext->size, cob.matrix);
      mul_v3_v3(td->ext->size, size_sign);
    }
  }
}

/* -------------------------------------------------------------------- */
/* Transform (Bend) */

/** \name Transform Bend
 * \{ */

struct BendCustomData {
  /* All values are in global space. */
  float warp_sta[3];
  float warp_end[3];

  float warp_nor[3];
  float warp_tan[3];

  /* for applying the mouse distance */
  float warp_init_dist;
};

static void initBend(TransInfo *t)
{
  const float mval_fl[2] = {UNPACK2(t->mval)};
  const float *curs;
  float tvec[3];
  struct BendCustomData *data;

  t->mode = TFM_BEND;
  t->transform = Bend;
  t->handleEvent = handleEventBend;

  setInputPostFct(&t->mouse, postInputRotation);
  initMouseInputMode(t, &t->mouse, INPUT_ANGLE_SPRING);

  t->idx_max = 1;
  t->num.idx_max = 1;
  t->snap[0] = 0.0f;
  t->snap[1] = SNAP_INCREMENTAL_ANGLE;
  t->snap[2] = t->snap[1] * 0.2;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
  t->num.unit_type[1] = B_UNIT_LENGTH;

  t->flag |= T_NO_CONSTRAINT;

  // copy_v3_v3(t->center, ED_view3d_cursor3d_get(t->scene, t->view));
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    calculateCenterCursor(t, t->center_global);
  }
  calculateCenterLocal(t, t->center_global);

  t->val = 0.0f;

  data = MEM_callocN(sizeof(*data), __func__);

  curs = t->scene->cursor.location;
  copy_v3_v3(data->warp_sta, curs);
  ED_view3d_win_to_3d(t->sa->spacedata.first, t->ar, curs, mval_fl, data->warp_end);

  copy_v3_v3(data->warp_nor, t->viewinv[2]);
  normalize_v3(data->warp_nor);

  /* tangent */
  sub_v3_v3v3(tvec, data->warp_end, data->warp_sta);
  cross_v3_v3v3(data->warp_tan, tvec, data->warp_nor);
  normalize_v3(data->warp_tan);

  data->warp_init_dist = len_v3v3(data->warp_end, data->warp_sta);

  t->custom.mode.data = data;
  t->custom.mode.use_free = true;
}

static eRedrawFlag handleEventBend(TransInfo *UNUSED(t), const wmEvent *event)
{
  eRedrawFlag status = TREDRAW_NOTHING;

  if (event->type == MIDDLEMOUSE && event->val == KM_PRESS) {
    status = TREDRAW_HARD;
  }

  return status;
}

static void Bend(TransInfo *t, const int UNUSED(mval[2]))
{
  float vec[3];
  float pivot_global[3];
  float warp_end_radius_global[3];
  int i;
  char str[UI_MAX_DRAW_STR];
  const struct BendCustomData *data = t->custom.mode.data;
  const bool is_clamp = (t->flag & T_ALT_TRANSFORM) == 0;

  union {
    struct {
      float angle, scale;
    };
    float vector[2];
  } values;

  /* amount of radians for bend */
  copy_v2_v2(values.vector, t->values);

#if 0
  snapGrid(t, angle_rad);
#else
  /* hrmf, snapping radius is using 'angle' steps, need to convert to something else
   * this isnt essential but nicer to give reasonable snapping values for radius */
  if (t->tsnap.mode & SCE_SNAP_MODE_INCREMENT) {
    const float radius_snap = 0.1f;
    const float snap_hack = (t->snap[1] * data->warp_init_dist) / radius_snap;
    values.scale *= snap_hack;
    snapGridIncrement(t, values.vector);
    values.scale /= snap_hack;
  }
#endif

  if (applyNumInput(&t->num, values.vector)) {
    values.scale = values.scale / data->warp_init_dist;
  }

  copy_v2_v2(t->values, values.vector);

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN * 2];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str,
                 sizeof(str),
                 TIP_("Bend Angle: %s Radius: %s Alt, Clamp %s"),
                 &c[0],
                 &c[NUM_STR_REP_LEN],
                 WM_bool_as_string(is_clamp));
  }
  else {
    /* default header print */
    BLI_snprintf(str,
                 sizeof(str),
                 TIP_("Bend Angle: %.3f Radius: %.4f, Alt, Clamp %s"),
                 RAD2DEGF(values.angle),
                 values.scale * data->warp_init_dist,
                 WM_bool_as_string(is_clamp));
  }

  values.angle *= -1.0f;
  values.scale *= data->warp_init_dist;

  /* calc 'data->warp_end' from 'data->warp_end_init' */
  copy_v3_v3(warp_end_radius_global, data->warp_end);
  dist_ensure_v3_v3fl(warp_end_radius_global, data->warp_sta, values.scale);
  /* done */

  /* calculate pivot */
  copy_v3_v3(pivot_global, data->warp_sta);
  if (values.angle > 0.0f) {
    madd_v3_v3fl(pivot_global,
                 data->warp_tan,
                 -values.scale * shell_angle_to_dist((float)M_PI_2 - values.angle));
  }
  else {
    madd_v3_v3fl(pivot_global,
                 data->warp_tan,
                 +values.scale * shell_angle_to_dist((float)M_PI_2 + values.angle));
  }

  /* TODO(campbell): xform, compensate object center. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;

    float warp_sta_local[3];
    float warp_end_local[3];
    float warp_end_radius_local[3];
    float pivot_local[3];

    if (tc->use_local_mat) {
      sub_v3_v3v3(warp_sta_local, data->warp_sta, tc->mat[3]);
      sub_v3_v3v3(warp_end_local, data->warp_end, tc->mat[3]);
      sub_v3_v3v3(warp_end_radius_local, warp_end_radius_global, tc->mat[3]);
      sub_v3_v3v3(pivot_local, pivot_global, tc->mat[3]);
    }
    else {
      copy_v3_v3(warp_sta_local, data->warp_sta);
      copy_v3_v3(warp_end_local, data->warp_end);
      copy_v3_v3(warp_end_radius_local, warp_end_radius_global);
      copy_v3_v3(pivot_local, pivot_global);
    }

    for (i = 0; i < tc->data_len; i++, td++) {
      float mat[3][3];
      float delta[3];
      float fac, fac_scaled;

      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (UNLIKELY(values.angle == 0.0f)) {
        copy_v3_v3(td->loc, td->iloc);
        continue;
      }

      copy_v3_v3(vec, td->iloc);
      mul_m3_v3(td->mtx, vec);

      fac = line_point_factor_v3(vec, warp_sta_local, warp_end_radius_local);
      if (is_clamp) {
        CLAMP(fac, 0.0f, 1.0f);
      }

      if (t->options & CTX_GPENCIL_STROKES) {
        /* grease pencil multiframe falloff */
        bGPDstroke *gps = (bGPDstroke *)td->extra;
        if (gps != NULL) {
          fac_scaled = fac * td->factor * gps->runtime.multi_frame_falloff;
        }
        else {
          fac_scaled = fac * td->factor;
        }
      }
      else {
        fac_scaled = fac * td->factor;
      }

      axis_angle_normalized_to_mat3(mat, data->warp_nor, values.angle * fac_scaled);
      interp_v3_v3v3(delta, warp_sta_local, warp_end_radius_local, fac_scaled);
      sub_v3_v3(delta, warp_sta_local);

      /* delta is subtracted, rotation adds back this offset */
      sub_v3_v3(vec, delta);

      sub_v3_v3(vec, pivot_local);
      mul_m3_v3(mat, vec);
      add_v3_v3(vec, pivot_local);

      mul_m3_v3(td->smtx, vec);

      /* rotation */
      if ((t->flag & T_POINTS) == 0) {
        ElementRotation(t, tc, td, mat, V3D_AROUND_LOCAL_ORIGINS);
      }

      /* location */
      copy_v3_v3(td->loc, vec);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Shear) */

/** \name Transform Shear
 * \{ */

static void initShear_mouseInputMode(TransInfo *t)
{
  float dir[3];

  if (t->custom.mode.data == NULL) {
    copy_v3_v3(dir, t->orient_matrix[t->orient_axis_ortho]);
  }
  else {
    cross_v3_v3v3(dir, t->orient_matrix[t->orient_axis_ortho], t->orient_matrix[t->orient_axis]);
  }

  /* Without this, half the gizmo handles move in the opposite direction. */
  if ((t->orient_axis_ortho + 1) % 3 != t->orient_axis) {
    negate_v3(dir);
  }

  mul_mat3_m4_v3(t->viewmat, dir);
  if (normalize_v2(dir) == 0.0f) {
    dir[0] = 1.0f;
  }
  setCustomPointsFromDirection(t, &t->mouse, dir);

  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);
}

static void initShear(TransInfo *t)
{
  t->mode = TFM_SHEAR;
  t->transform = applyShear;
  t->handleEvent = handleEventShear;

  if (t->orient_axis == t->orient_axis_ortho) {
    t->orient_axis = 2;
    t->orient_axis_ortho = 1;
  }

  initShear_mouseInputMode(t);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE; /* Don't think we have any unit here? */

  t->flag |= T_NO_CONSTRAINT;
}

static eRedrawFlag handleEventShear(TransInfo *t, const wmEvent *event)
{
  eRedrawFlag status = TREDRAW_NOTHING;

  if (event->type == MIDDLEMOUSE && event->val == KM_PRESS) {
    /* Use custom.mode.data pointer to signal Shear direction */
    if (t->custom.mode.data == NULL) {
      t->custom.mode.data = (void *)1;
    }
    else {
      t->custom.mode.data = NULL;
    }
    initShear_mouseInputMode(t);

    status = TREDRAW_HARD;
  }
  else if (event->type == XKEY && event->val == KM_PRESS) {
    t->custom.mode.data = NULL;
    initShear_mouseInputMode(t);

    status = TREDRAW_HARD;
  }
  else if (event->type == YKEY && event->val == KM_PRESS) {
    t->custom.mode.data = (void *)1;
    initShear_mouseInputMode(t);

    status = TREDRAW_HARD;
  }

  return status;
}

static void applyShear(TransInfo *t, const int UNUSED(mval[2]))
{
  float vec[3];
  float smat[3][3], tmat[3][3], totmat[3][3], axismat[3][3], axismat_inv[3][3];
  float value;
  int i;
  char str[UI_MAX_DRAW_STR];
  const bool is_local_center = transdata_check_local_center(t, t->around);

  value = t->values[0];

  snapGridIncrement(t, &value);

  applyNumInput(&t->num, &value);

  t->values[0] = value;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("Shear: %s %s"), c, t->proptext);
  }
  else {
    /* default header print */
    BLI_snprintf(str,
                 sizeof(str),
                 TIP_("Shear: %.3f %s (Press X or Y to set shear axis)"),
                 value,
                 t->proptext);
  }

  unit_m3(smat);

  // Custom data signals shear direction
  if (t->custom.mode.data == NULL) {
    smat[1][0] = value;
  }
  else {
    smat[0][1] = value;
  }

  copy_v3_v3(axismat_inv[0], t->orient_matrix[t->orient_axis_ortho]);
  copy_v3_v3(axismat_inv[2], t->orient_matrix[t->orient_axis]);
  cross_v3_v3v3(axismat_inv[1], axismat_inv[0], axismat_inv[2]);
  invert_m3_m3(axismat, axismat_inv);

  mul_m3_series(totmat, axismat_inv, smat, axismat);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      const float *center, *co;

      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (t->flag & T_EDIT) {
        mul_m3_series(tmat, td->smtx, totmat, td->mtx);
      }
      else {
        copy_m3_m3(tmat, totmat);
      }

      if (is_local_center) {
        center = td->center;
        co = td->loc;
      }
      else {
        center = tc->center_local;
        co = td->center;
      }

      sub_v3_v3v3(vec, co, center);

      mul_m3_v3(tmat, vec);

      add_v3_v3(vec, center);
      sub_v3_v3(vec, co);

      if (t->options & CTX_GPENCIL_STROKES) {
        /* grease pencil multiframe falloff */
        bGPDstroke *gps = (bGPDstroke *)td->extra;
        if (gps != NULL) {
          mul_v3_fl(vec, td->factor * gps->runtime.multi_frame_falloff);
        }
        else {
          mul_v3_fl(vec, td->factor);
        }
      }
      else {
        mul_v3_fl(vec, td->factor);
      }

      add_v3_v3v3(td->loc, td->iloc, vec);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Resize) */

/** \name Transform Resize
 * \{ */

static void initResize(TransInfo *t)
{
  t->mode = TFM_RESIZE;
  t->transform = applyResize;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->flag |= T_NULL_ONE;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  if ((t->flag & T_EDIT) == 0) {
    t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
    t->num.val_flag[0] |= NUM_NO_ZERO;
    t->num.val_flag[1] |= NUM_NO_ZERO;
    t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
  }

  t->idx_max = 2;
  t->num.idx_max = 2;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
  t->num.unit_type[2] = B_UNIT_NONE;
}

static void headerResize(TransInfo *t, const float vec[3], char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  size_t ofs = 0;
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
  }

  if (t->con.mode & CON_APPLY) {
    switch (t->num.idx_max) {
      case 0:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            TIP_("Scale: %s%s %s"),
                            &tvec[0],
                            t->con.text,
                            t->proptext);
        break;
      case 1:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            TIP_("Scale: %s : %s%s %s"),
                            &tvec[0],
                            &tvec[NUM_STR_REP_LEN],
                            t->con.text,
                            t->proptext);
        break;
      case 2:
        ofs += BLI_snprintf(str + ofs,
                            UI_MAX_DRAW_STR - ofs,
                            TIP_("Scale: %s : %s : %s%s %s"),
                            &tvec[0],
                            &tvec[NUM_STR_REP_LEN],
                            &tvec[NUM_STR_REP_LEN * 2],
                            t->con.text,
                            t->proptext);
        break;
    }
  }
  else {
    if (t->flag & T_2D_EDIT) {
      ofs += BLI_snprintf(str + ofs,
                          UI_MAX_DRAW_STR - ofs,
                          TIP_("Scale X: %s   Y: %s%s %s"),
                          &tvec[0],
                          &tvec[NUM_STR_REP_LEN],
                          t->con.text,
                          t->proptext);
    }
    else {
      ofs += BLI_snprintf(str + ofs,
                          UI_MAX_DRAW_STR - ofs,
                          TIP_("Scale X: %s   Y: %s  Z: %s%s %s"),
                          &tvec[0],
                          &tvec[NUM_STR_REP_LEN],
                          &tvec[NUM_STR_REP_LEN * 2],
                          t->con.text,
                          t->proptext);
    }
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

/**
 * \a smat is reference matrix only.
 *
 * \note this is a tricky area, before making changes see: T29633, T42444
 */
static void TransMat3ToSize(float mat[3][3], float smat[3][3], float size[3])
{
  float rmat[3][3];

  mat3_to_rot_size(rmat, size, mat);

  /* first tried with dotproduct... but the sign flip is crucial */
  if (dot_v3v3(rmat[0], smat[0]) < 0.0f) {
    size[0] = -size[0];
  }
  if (dot_v3v3(rmat[1], smat[1]) < 0.0f) {
    size[1] = -size[1];
  }
  if (dot_v3v3(rmat[2], smat[2]) < 0.0f) {
    size[2] = -size[2];
  }
}

static void ElementResize(TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3])
{
  float tmat[3][3], smat[3][3], center[3];
  float vec[3];

  if (t->flag & T_EDIT) {
    mul_m3_m3m3(smat, mat, td->mtx);
    mul_m3_m3m3(tmat, td->smtx, smat);
  }
  else {
    copy_m3_m3(tmat, mat);
  }

  if (t->con.applySize) {
    t->con.applySize(t, tc, td, tmat);
  }

  /* local constraint shouldn't alter center */
  if (transdata_check_local_center(t, t->around)) {
    copy_v3_v3(center, td->center);
  }
  else if (t->options & CTX_MOVIECLIP) {
    if (td->flag & TD_INDIVIDUAL_SCALE) {
      copy_v3_v3(center, td->center);
    }
    else {
      copy_v3_v3(center, tc->center_local);
    }
  }
  else {
    copy_v3_v3(center, tc->center_local);
  }

  /* Size checked needed since the 3D cursor only uses rotation fields. */
  if (td->ext && td->ext->size) {
    float fsize[3];

    if (t->flag & (T_OBJECT | T_TEXTURE | T_POSE)) {
      float obsizemat[3][3];
      /* Reorient the size mat to fit the oriented object. */
      mul_m3_m3m3(obsizemat, tmat, td->axismtx);
      /* print_m3("obsizemat", obsizemat); */
      TransMat3ToSize(obsizemat, td->axismtx, fsize);
      /* print_v3("fsize", fsize); */
    }
    else {
      mat3_to_size(fsize, tmat);
    }

    protectedSizeBits(td->protectflag, fsize);

    if ((t->flag & T_V3D_ALIGN) == 0) { /* align mode doesn't resize objects itself */
      if ((td->flag & TD_SINGLESIZE) && !(t->con.mode & CON_APPLY)) {
        /* scale val and reset size */
        *td->val = td->ival * (1 + (fsize[0] - 1) * td->factor);

        td->ext->size[0] = td->ext->isize[0];
        td->ext->size[1] = td->ext->isize[1];
        td->ext->size[2] = td->ext->isize[2];
      }
      else {
        /* Reset val if SINGLESIZE but using a constraint */
        if (td->flag & TD_SINGLESIZE) {
          *td->val = td->ival;
        }

        td->ext->size[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
        td->ext->size[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
        td->ext->size[2] = td->ext->isize[2] * (1 + (fsize[2] - 1) * td->factor);
      }
    }

    constraintSizeLim(t, td);
  }

  /* For individual element center, Editmode need to use iloc */
  if (t->flag & T_POINTS) {
    sub_v3_v3v3(vec, td->iloc, center);
  }
  else {
    sub_v3_v3v3(vec, td->center, center);
  }

  mul_m3_v3(tmat, vec);

  add_v3_v3(vec, center);
  if (t->flag & T_POINTS) {
    sub_v3_v3(vec, td->iloc);
  }
  else {
    sub_v3_v3(vec, td->center);
  }

  /* grease pencil falloff */
  if (t->options & CTX_GPENCIL_STROKES) {
    bGPDstroke *gps = (bGPDstroke *)td->extra;
    mul_v3_fl(vec, td->factor * gps->runtime.multi_frame_falloff);

    /* scale stroke thickness */
    if (td->val) {
      snapGridIncrement(t, t->values);
      applyNumInput(&t->num, t->values);

      float ratio = t->values[0];
      *td->val = td->ival * ratio * gps->runtime.multi_frame_falloff;
      CLAMP_MIN(*td->val, 0.001f);
    }
  }
  else {
    mul_v3_fl(vec, td->factor);
  }

  if (t->flag & (T_OBJECT | T_POSE)) {
    mul_m3_v3(td->smtx, vec);
  }

  protectedTransBits(td->protectflag, vec);
  if (td->loc) {
    add_v3_v3v3(td->loc, td->iloc, vec);
  }

  constraintTransLim(t, td);
}

static void applyResize(TransInfo *t, const int UNUSED(mval[2]))
{
  float mat[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];

  if (t->flag & T_AUTOVALUES) {
    copy_v3_v3(t->values, t->auto_values);
  }
  else {
    float ratio = t->values[0];

    copy_v3_fl(t->values, ratio);

    snapGridIncrement(t, t->values);

    if (applyNumInput(&t->num, t->values)) {
      constraintNumInput(t, t->values);
    }

    applySnapping(t, t->values);
  }

  size_to_mat3(mat, t->values);
  if (t->con.mode & CON_APPLY) {
    t->con.applySize(t, NULL, NULL, mat);

    /* Only so we have re-usable value with redo. */
    float pvec[3] = {0.0f, 0.0f, 0.0f};
    int j = 0;
    for (i = 0; i < 3; i++) {
      if (!(t->con.mode & (CON_AXIS0 << i))) {
        t->values[i] = 1.0f;
      }
      else {
        pvec[j++] = t->values[i];
      }
    }
    headerResize(t, pvec, str);
  }
  else {
    headerResize(t, t->values, str);
  }

  copy_m3_m3(t->mat, mat);  // used in gizmo

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      ElementResize(t, tc, td, mat);
    }
  }

  /* evil hack - redo resize if cliping needed */
  if (t->flag & T_CLIP_UV && clipUVTransform(t, t->values, 1)) {
    size_to_mat3(mat, t->values);

    if (t->con.mode & CON_APPLY) {
      t->con.applySize(t, NULL, NULL, mat);
    }

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        ElementResize(t, tc, td, mat);
      }

      /* In proportional edit it can happen that */
      /* vertices in the radius of the brush end */
      /* outside the clipping area               */
      /* XXX HACK - dg */
      if (t->flag & T_PROP_EDIT_ALL) {
        clipUVData(t);
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Skin) */

/** \name Transform Skin
 * \{ */

static void initSkinResize(TransInfo *t)
{
  t->mode = TFM_SKIN_RESIZE;
  t->transform = applySkinResize;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->flag |= T_NULL_ONE;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  if ((t->flag & T_EDIT) == 0) {
    t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
    t->num.val_flag[0] |= NUM_NO_ZERO;
    t->num.val_flag[1] |= NUM_NO_ZERO;
    t->num.val_flag[2] |= NUM_NO_ZERO;
#endif
  }

  t->idx_max = 2;
  t->num.idx_max = 2;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
  t->num.unit_type[2] = B_UNIT_NONE;
}

static void applySkinResize(TransInfo *t, const int UNUSED(mval[2]))
{
  float size[3], mat[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];

  copy_v3_fl(size, t->values[0]);

  snapGridIncrement(t, size);

  if (applyNumInput(&t->num, size)) {
    constraintNumInput(t, size);
  }

  applySnapping(t, size);

  if (t->flag & T_AUTOVALUES) {
    copy_v3_v3(size, t->auto_values);
  }

  copy_v3_v3(t->values, size);

  size_to_mat3(mat, size);

  headerResize(t, size, str);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float tmat[3][3], smat[3][3];
      float fsize[3];

      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (t->flag & T_EDIT) {
        mul_m3_m3m3(smat, mat, td->mtx);
        mul_m3_m3m3(tmat, td->smtx, smat);
      }
      else {
        copy_m3_m3(tmat, mat);
      }

      if (t->con.applySize) {
        t->con.applySize(t, NULL, NULL, tmat);
      }

      mat3_to_size(fsize, tmat);
      td->val[0] = td->ext->isize[0] * (1 + (fsize[0] - 1) * td->factor);
      td->val[1] = td->ext->isize[1] * (1 + (fsize[1] - 1) * td->factor);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (ToSphere) */

/** \name Transform ToSphere
 * \{ */

static void initToSphere(TransInfo *t)
{
  int i;

  t->mode = TFM_TOSPHERE;
  t->transform = applyToSphere;

  initMouseInputMode(t, &t->mouse, INPUT_HORIZONTAL_RATIO);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->num.val_flag[0] |= NUM_NULL_ONE | NUM_NO_NEGATIVE;
  t->flag |= T_NO_CONSTRAINT;

  // Calculate average radius
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      t->val += len_v3v3(tc->center_local, td->iloc);
    }
  }

  t->val /= (float)t->data_len_all;
}

static void applyToSphere(TransInfo *t, const int UNUSED(mval[2]))
{
  float vec[3];
  float ratio, radius;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0];

  snapGridIncrement(t, &ratio);

  applyNumInput(&t->num, &ratio);

  CLAMP(ratio, 0.0f, 1.0f);

  t->values[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("To Sphere: %s %s"), c, t->proptext);
  }
  else {
    /* default header print */
    BLI_snprintf(str, sizeof(str), TIP_("To Sphere: %.4f %s"), ratio, t->proptext);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float tratio;
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      sub_v3_v3v3(vec, td->iloc, tc->center_local);

      radius = normalize_v3(vec);

      tratio = ratio * td->factor;

      mul_v3_fl(vec, radius * (1.0f - tratio) + t->val * tratio);

      add_v3_v3v3(td->loc, tc->center_local, vec);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Rotation) */

/** \name Transform Rotation
 * \{ */

static void postInputRotation(TransInfo *t, float values[3])
{
  float axis_final[3];
  copy_v3_v3(axis_final, t->orient_matrix[t->orient_axis]);
  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, NULL, NULL, axis_final, values);
  }
}

static void initRotation(TransInfo *t)
{
  t->mode = TFM_ROTATION;
  t->transform = applyRotation;

  setInputPostFct(&t->mouse, postInputRotation);
  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[2]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  if (t->flag & T_2D_EDIT) {
    t->flag |= T_NO_CONSTRAINT;
  }
}

/* Used by Transform Rotation and Transform Normal Rotation */
static void headerRotation(TransInfo *t, char str[UI_MAX_DRAW_STR], float final)
{
  size_t ofs = 0;

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_("Rot: %s %s %s"), &c[0], t->con.text, t->proptext);
  }
  else {
    ofs += BLI_snprintf(str + ofs,
                        UI_MAX_DRAW_STR - ofs,
                        TIP_("Rot: %.2f%s %s"),
                        RAD2DEGF(final),
                        t->con.text,
                        t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

/**
 * Applies values of rotation to `td->loc` and `td->ext->quat`
 * based on a rotation matrix (mat) and a pivot (center).
 *
 * Protected axis and other transform settings are taken into account.
 */
static void ElementRotation_ex(
    TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3], const float *center)
{
  float vec[3], totmat[3][3], smat[3][3];
  float eul[3], fmat[3][3], quat[4];

  if (t->flag & T_POINTS) {
    mul_m3_m3m3(totmat, mat, td->mtx);
    mul_m3_m3m3(smat, td->smtx, totmat);

    /* apply gpencil falloff */
    if (t->options & CTX_GPENCIL_STROKES) {
      bGPDstroke *gps = (bGPDstroke *)td->extra;
      float sx = smat[0][0];
      float sy = smat[1][1];
      float sz = smat[2][2];

      mul_m3_fl(smat, gps->runtime.multi_frame_falloff);
      /* fix scale */
      smat[0][0] = sx;
      smat[1][1] = sy;
      smat[2][2] = sz;
    }

    sub_v3_v3v3(vec, td->iloc, center);
    mul_m3_v3(smat, vec);

    add_v3_v3v3(td->loc, vec, center);

    sub_v3_v3v3(vec, td->loc, td->iloc);
    protectedTransBits(td->protectflag, vec);
    add_v3_v3v3(td->loc, td->iloc, vec);

    if (td->flag & TD_USEQUAT) {
      mul_m3_series(fmat, td->smtx, mat, td->mtx);
      mat3_to_quat(quat, fmat);  // Actual transform

      if (td->ext->quat) {
        mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);

        /* is there a reason not to have this here? -jahka */
        protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
      }
    }
  }
  /**
   * HACK WARNING
   *
   * This is some VERY ugly special case to deal with pose mode.
   *
   * The problem is that mtx and smtx include each bone orientation.
   *
   * That is needed to rotate each bone properly, HOWEVER, to calculate
   * the translation component, we only need the actual armature object's
   * matrix (and inverse). That is not all though. Once the proper translation
   * has been computed, it has to be converted back into the bone's space.
   */
  else if (t->flag & T_POSE) {
    // Extract and invert armature object matrix

    if ((td->flag & TD_NO_LOC) == 0) {
      sub_v3_v3v3(vec, td->center, center);

      mul_m3_v3(tc->mat3, vec);   // To Global space
      mul_m3_v3(mat, vec);        // Applying rotation
      mul_m3_v3(tc->imat3, vec);  // To Local space

      add_v3_v3(vec, center);
      /* vec now is the location where the object has to be */

      sub_v3_v3v3(vec, vec, td->center);  // Translation needed from the initial location

      /* special exception, see TD_PBONE_LOCAL_MTX definition comments */
      if (td->flag & TD_PBONE_LOCAL_MTX_P) {
        /* do nothing */
      }
      else if (td->flag & TD_PBONE_LOCAL_MTX_C) {
        mul_m3_v3(tc->mat3, vec);         // To Global space
        mul_m3_v3(td->ext->l_smtx, vec);  // To Pose space (Local Location)
      }
      else {
        mul_m3_v3(tc->mat3, vec);  // To Global space
        mul_m3_v3(td->smtx, vec);  // To Pose space
      }

      protectedTransBits(td->protectflag, vec);

      add_v3_v3v3(td->loc, td->iloc, vec);

      constraintTransLim(t, td);
    }

    /* rotation */
    /* MORE HACK: as in some cases the matrix to apply location and rot/scale is not the same,
     * and ElementRotation() might be called in Translation context (with align snapping),
     * we need to be sure to actually use the *rotation* matrix here...
     * So no other way than storing it in some dedicated members of td->ext! */
    if ((t->flag & T_V3D_ALIGN) == 0) { /* align mode doesn't rotate objects itself */
      /* euler or quaternion/axis-angle? */
      if (td->ext->rotOrder == ROT_MODE_QUAT) {
        mul_m3_series(fmat, td->ext->r_smtx, mat, td->ext->r_mtx);

        mat3_to_quat(quat, fmat); /* Actual transform */

        mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
        /* this function works on end result */
        protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
      }
      else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* calculate effect based on quats */
        float iquat[4], tquat[4];

        axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);

        mul_m3_series(fmat, td->ext->r_smtx, mat, td->ext->r_mtx);
        mat3_to_quat(quat, fmat); /* Actual transform */
        mul_qt_qtqt(tquat, quat, iquat);

        quat_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, tquat);

        /* this function works on end result */
        protectedAxisAngleBits(td->protectflag,
                               td->ext->rotAxis,
                               td->ext->rotAngle,
                               td->ext->irotAxis,
                               td->ext->irotAngle);
      }
      else {
        float eulmat[3][3];

        mul_m3_m3m3(totmat, mat, td->ext->r_mtx);
        mul_m3_m3m3(smat, td->ext->r_smtx, totmat);

        /* calculate the total rotatation in eulers */
        copy_v3_v3(eul, td->ext->irot);
        eulO_to_mat3(eulmat, eul, td->ext->rotOrder);

        /* mat = transform, obmat = bone rotation */
        mul_m3_m3m3(fmat, smat, eulmat);

        mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

        /* and apply (to end result only) */
        protectedRotateBits(td->protectflag, eul, td->ext->irot);
        copy_v3_v3(td->ext->rot, eul);
      }

      constraintRotLim(t, td);
    }
  }
  else {
    if ((td->flag & TD_NO_LOC) == 0) {
      /* translation */
      sub_v3_v3v3(vec, td->center, center);
      mul_m3_v3(mat, vec);
      add_v3_v3(vec, center);
      /* vec now is the location where the object has to be */
      sub_v3_v3(vec, td->center);
      mul_m3_v3(td->smtx, vec);

      protectedTransBits(td->protectflag, vec);

      add_v3_v3v3(td->loc, td->iloc, vec);
    }

    constraintTransLim(t, td);

    /* rotation */
    if ((t->flag & T_V3D_ALIGN) == 0) {  // align mode doesn't rotate objects itself
      /* euler or quaternion? */
      if ((td->ext->rotOrder == ROT_MODE_QUAT) || (td->flag & TD_USEQUAT)) {
        /* can be called for texture space translate for example, then opt out */
        if (td->ext->quat) {
          mul_m3_series(fmat, td->smtx, mat, td->mtx);
          mat3_to_quat(quat, fmat);  // Actual transform

          mul_qt_qtqt(td->ext->quat, quat, td->ext->iquat);
          /* this function works on end result */
          protectedQuaternionBits(td->protectflag, td->ext->quat, td->ext->iquat);
        }
      }
      else if (td->ext->rotOrder == ROT_MODE_AXISANGLE) {
        /* calculate effect based on quats */
        float iquat[4], tquat[4];

        axis_angle_to_quat(iquat, td->ext->irotAxis, td->ext->irotAngle);

        mul_m3_series(fmat, td->smtx, mat, td->mtx);
        mat3_to_quat(quat, fmat);  // Actual transform
        mul_qt_qtqt(tquat, quat, iquat);

        quat_to_axis_angle(td->ext->rotAxis, td->ext->rotAngle, tquat);

        /* this function works on end result */
        protectedAxisAngleBits(td->protectflag,
                               td->ext->rotAxis,
                               td->ext->rotAngle,
                               td->ext->irotAxis,
                               td->ext->irotAngle);
      }
      else {
        float obmat[3][3];

        mul_m3_m3m3(totmat, mat, td->mtx);
        mul_m3_m3m3(smat, td->smtx, totmat);

        /* calculate the total rotatation in eulers */
        add_v3_v3v3(eul, td->ext->irot, td->ext->drot); /* correct for delta rot */
        eulO_to_mat3(obmat, eul, td->ext->rotOrder);
        /* mat = transform, obmat = object rotation */
        mul_m3_m3m3(fmat, smat, obmat);

        mat3_to_compatible_eulO(eul, td->ext->rot, td->ext->rotOrder, fmat);

        /* correct back for delta rot */
        sub_v3_v3v3(eul, eul, td->ext->drot);

        /* and apply */
        protectedRotateBits(td->protectflag, eul, td->ext->irot);
        copy_v3_v3(td->ext->rot, eul);
      }

      constraintRotLim(t, td);
    }
  }
}

static void ElementRotation(
    TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3], const short around)
{
  const float *center;

  /* local constraint shouldn't alter center */
  if (transdata_check_local_center(t, around)) {
    center = td->center;
  }
  else {
    center = tc->center_local;
  }

  ElementRotation_ex(t, tc, td, mat, center);
}

static void applyRotationValue(TransInfo *t, float angle, float axis[3])
{
  float mat[3][3];
  int i;

  axis_angle_normalized_to_mat3(mat, axis, angle);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {

      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (t->con.applyRot) {
        t->con.applyRot(t, tc, td, axis, NULL);
        axis_angle_normalized_to_mat3(mat, axis, angle * td->factor);
      }
      else if (t->flag & T_PROP_EDIT) {
        axis_angle_normalized_to_mat3(mat, axis, angle * td->factor);
      }

      ElementRotation(t, tc, td, mat, t->around);
    }
  }
}

static void applyRotation(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];

  float final;

  final = t->values[0];

  snapGridIncrement(t, &final);

  float axis_final[3];
  copy_v3_v3(axis_final, t->orient_matrix[t->orient_axis]);

  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, NULL, NULL, axis_final, NULL);
  }

  applySnapping(t, &final);

  /* Used to clamp final result in [-PI, PI[ range, no idea why,
   * inheritance from 2.4x area, see T48998. */
  applyNumInput(&t->num, &final);

  t->values[0] = final;

  headerRotation(t, str, final);

  applyRotationValue(t, final, axis_final);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Rotation - Trackball) */

/** \name Transform Rotation - Trackball
 * \{ */

static void initTrackball(TransInfo *t)
{
  t->mode = TFM_TRACKBALL;
  t->transform = applyTrackball;

  initMouseInputMode(t, &t->mouse, INPUT_TRACKBALL);

  t->idx_max = 1;
  t->num.idx_max = 1;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[2]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;
  t->num.unit_type[1] = B_UNIT_ROTATION;

  t->flag |= T_NO_CONSTRAINT;
}

static void applyTrackballValue(TransInfo *t,
                                const float axis1[3],
                                const float axis2[3],
                                float angles[2])
{
  float mat[3][3];
  float axis[3];
  float angle;
  int i;

  mul_v3_v3fl(axis, axis1, angles[0]);
  madd_v3_v3fl(axis, axis2, angles[1]);
  angle = normalize_v3(axis);
  axis_angle_normalized_to_mat3(mat, axis, angle);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (t->flag & T_PROP_EDIT) {
        axis_angle_normalized_to_mat3(mat, axis, td->factor * angle);
      }

      ElementRotation(t, tc, td, mat, t->around);
    }
  }
}

static void applyTrackball(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float axis1[3], axis2[3];
#if 0 /* UNUSED */
  float mat[3][3], totmat[3][3], smat[3][3];
#endif
  float phi[2];

  copy_v3_v3(axis1, t->persinv[0]);
  copy_v3_v3(axis2, t->persinv[1]);
  normalize_v3(axis1);
  normalize_v3(axis2);

  copy_v2_v2(phi, t->values);

  snapGridIncrement(t, phi);

  applyNumInput(&t->num, phi);

  copy_v2_v2(t->values, phi);

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN * 2];

    outputNumInput(&(t->num), c, &t->scene->unit);

    ofs += BLI_snprintf(str + ofs,
                        sizeof(str) - ofs,
                        TIP_("Trackball: %s %s %s"),
                        &c[0],
                        &c[NUM_STR_REP_LEN],
                        t->proptext);
  }
  else {
    ofs += BLI_snprintf(str + ofs,
                        sizeof(str) - ofs,
                        TIP_("Trackball: %.2f %.2f %s"),
                        RAD2DEGF(phi[0]),
                        RAD2DEGF(phi[1]),
                        t->proptext);
  }

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, sizeof(str) - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }

#if 0 /* UNUSED */
  axis_angle_normalized_to_mat3(smat, axis1, phi[0]);
  axis_angle_normalized_to_mat3(totmat, axis2, phi[1]);

  mul_m3_m3m3(mat, smat, totmat);

  // TRANSFORM_FIX_ME
  //copy_m3_m3(t->mat, mat);  // used in gizmo
#endif

  applyTrackballValue(t, axis1, axis2, phi);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Normal Rotation) */

/** \name Transform Normal Rotation
 * \{ */

static void storeCustomLNorValue(TransDataContainer *tc, BMesh *bm)
{
  BMLoopNorEditDataArray *lnors_ed_arr = BM_loop_normal_editdata_array_init(bm, false);
  // BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata;

  tc->custom.mode.data = lnors_ed_arr;
  tc->custom.mode.free_cb = freeCustomNormalArray;
}

void freeCustomNormalArray(TransInfo *t, TransDataContainer *tc, TransCustomData *custom_data)
{
  BMLoopNorEditDataArray *lnors_ed_arr = custom_data->data;

  if (t->state == TRANS_CANCEL) {
    BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    /* Restore custom loop normal on cancel */
    for (int i = 0; i < lnors_ed_arr->totloop; i++, lnor_ed++) {
      BKE_lnor_space_custom_normal_to_data(
          bm->lnor_spacearr->lspacearr[lnor_ed->loop_index], lnor_ed->niloc, lnor_ed->clnors_data);
    }
  }

  BM_loop_normal_editdata_array_free(lnors_ed_arr);

  tc->custom.mode.data = NULL;
  tc->custom.mode.free_cb = NULL;
}

static void initNormalRotation(TransInfo *t)
{
  t->mode = TFM_NORMAL_ROTATION;
  t->transform = applyNormalRotation;

  setInputPostFct(&t->mouse, postInputRotation);
  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[2]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    BKE_editmesh_ensure_autosmooth(em);
    BKE_editmesh_lnorspace_update(em);

    storeCustomLNorValue(tc, bm);
  }
}

/* Works by getting custom normal from clnor_data, transform, then store */
static void applyNormalRotation(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];

  float axis_final[3];
  copy_v3_v3(axis_final, t->orient_matrix[t->orient_axis]);

  if ((t->con.mode & CON_APPLY) && t->con.applyRot) {
    t->con.applyRot(t, NULL, NULL, axis_final, NULL);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    BMLoopNorEditDataArray *lnors_ed_arr = tc->custom.mode.data;
    BMLoopNorEditData *lnor_ed = lnors_ed_arr->lnor_editdata;

    float axis[3];
    float mat[3][3];
    float angle = t->values[0];
    copy_v3_v3(axis, axis_final);

    snapGridIncrement(t, &angle);

    applySnapping(t, &angle);

    applyNumInput(&t->num, &angle);

    headerRotation(t, str, angle);

    axis_angle_normalized_to_mat3(mat, axis, angle);

    for (int i = 0; i < lnors_ed_arr->totloop; i++, lnor_ed++) {
      mul_v3_m3v3(lnor_ed->nloc, mat, lnor_ed->niloc);

      BKE_lnor_space_custom_normal_to_data(
          bm->lnor_spacearr->lspacearr[lnor_ed->loop_index], lnor_ed->nloc, lnor_ed->clnors_data);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}

/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Translation) */

static void initSnapSpatial(TransInfo *t, float r_snap[3])
{
  if (t->spacetype == SPACE_VIEW3D) {
    RegionView3D *rv3d = t->ar->regiondata;

    if (rv3d) {
      View3D *v3d = t->sa->spacedata.first;
      r_snap[0] = 0.0f;
      r_snap[1] = ED_view3d_grid_view_scale(t->scene, v3d, rv3d, NULL) * 1.0f;
      r_snap[2] = r_snap[1] * 0.1f;
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    r_snap[0] = 0.0f;
    r_snap[1] = 0.0625f;
    r_snap[2] = 0.03125f;
  }
  else if (t->spacetype == SPACE_CLIP) {
    r_snap[0] = 0.0f;
    r_snap[1] = 0.125f;
    r_snap[2] = 0.0625f;
  }
  else if (t->spacetype == SPACE_NODE) {
    r_snap[0] = 0.0f;
    r_snap[1] = r_snap[2] = ED_node_grid_size();
  }
  else if (t->spacetype == SPACE_GRAPH) {
    r_snap[0] = 0.0f;
    r_snap[1] = 1.0;
    r_snap[2] = 0.1f;
  }
  else {
    r_snap[0] = 0.0f;
    r_snap[1] = r_snap[2] = 1.0f;
  }
}

/** \name Transform Translation
 * \{ */

static void initTranslation(TransInfo *t)
{
  if (t->spacetype == SPACE_ACTION) {
    /* this space uses time translate */
    BKE_report(t->reports,
               RPT_ERROR,
               "Use 'Time_Translate' transform mode instead of 'Translation' mode "
               "for translating keyframes in Dope Sheet Editor");
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_TRANSLATION;
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
    SpaceNode *snode = (SpaceNode *)t->sa->spacedata.first;

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
      if (td->flag & TD_NOACTION) {
        break;
      }

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
        float pvec[3];
        t->con.applyVec(t, tc, td, vec, tvec, pvec);
      }
      else {
        copy_v3_v3(tvec, vec);
      }

      if (use_rotate_offset) {
        add_v3_v3(tvec, rotate_offset);
      }

      mul_m3_v3(td->smtx, tvec);

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
  float value_final[3];

  if (t->flag & T_AUTOVALUES) {
    copy_v3_v3(t->values, t->auto_values);
  }
  else {
    if ((t->con.mode & CON_APPLY) == 0) {
      snapGridIncrement(t, t->values);
    }

    if (applyNumInput(&t->num, t->values)) {
      removeAspectRatio(t, t->values);
    }

    applySnapping(t, t->values);
  }

  if (t->con.mode & CON_APPLY) {
    float pvec[3] = {0.0f, 0.0f, 0.0f};
    t->con.applyVec(t, NULL, NULL, t->values, value_final, pvec);
    headerTranslation(t, pvec, str);

    /* only so we have re-usable value with redo, see T46741. */
    mul_v3_m3v3(t->values, t->con.imtx, value_final);
  }
  else {
    headerTranslation(t, t->values, str);
    copy_v3_v3(value_final, t->values);
  }

  /* don't use 't->values' now on */

  applyTranslationValue(t, value_final);

  /* evil hack - redo translation if clipping needed */
  if (t->flag & T_CLIP_UV && clipUVTransform(t, value_final, 0)) {
    applyTranslationValue(t, value_final);

    /* In proportional edit it can happen that */
    /* vertices in the radius of the brush end */
    /* outside the clipping area               */
    /* XXX HACK - dg */
    if (t->flag & T_PROP_EDIT_ALL) {
      clipUVData(t);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Shrink-Fatten) */

/** \name Transform Shrink-Fatten
 * \{ */

static void initShrinkFatten(TransInfo *t)
{
  // If not in mesh edit mode, fallback to Resize
  if ((t->flag & T_EDIT) == 0 || (t->obedit_type != OB_MESH)) {
    initResize(t);
  }
  else {
    t->mode = TFM_SHRINKFATTEN;
    t->transform = applyShrinkFatten;

    initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

    t->idx_max = 0;
    t->num.idx_max = 0;
    t->snap[0] = 0.0f;
    t->snap[1] = 1.0f;
    t->snap[2] = t->snap[1] * 0.1f;

    copy_v3_fl(t->num.val_inc, t->snap[1]);
    t->num.unit_sys = t->scene->unit.system;
    t->num.unit_type[0] = B_UNIT_LENGTH;

    t->flag |= T_NO_CONSTRAINT;
  }
}

static void applyShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
  float distance;
  int i;
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;

  distance = -t->values[0];

  snapGridIncrement(t, &distance);

  applyNumInput(&t->num, &distance);

  t->values[0] = -distance;

  /* header print for NumInput */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Shrink/Fatten:"), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, &t->scene->unit);
    ofs += BLI_snprintf(str + ofs, sizeof(str) - ofs, " %s", c);
  }
  else {
    /* default header print */
    ofs += BLI_snprintf(str + ofs, sizeof(str) - ofs, " %.4f", distance);
  }

  if (t->proptext[0]) {
    ofs += BLI_snprintf(str + ofs, sizeof(str) - ofs, " %s", t->proptext);
  }
  ofs += BLI_strncpy_rlen(str + ofs, ", (", sizeof(str) - ofs);

  if (t->keymap) {
    wmKeyMapItem *kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_RESIZE);
    if (kmi) {
      ofs += WM_keymap_item_to_string(kmi, false, str + ofs, sizeof(str) - ofs);
    }
  }
  BLI_snprintf(str + ofs,
               sizeof(str) - ofs,
               TIP_(" or Alt) Even Thickness %s"),
               WM_bool_as_string((t->flag & T_ALT_TRANSFORM) != 0));
  /* done with header string */

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float tdistance; /* temp dist */
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      /* get the final offset */
      tdistance = distance * td->factor;
      if (td->ext && (t->flag & T_ALT_TRANSFORM) != 0) {
        tdistance *= td->ext->isize[0]; /* shell factor */
      }

      madd_v3_v3v3fl(td->loc, td->iloc, td->axismtx[2], tdistance);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Tilt) */

/** \name Transform Tilt
 * \{ */

static void initTilt(TransInfo *t)
{
  t->mode = TFM_TILT;
  t->transform = applyTilt;

  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[2]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyTilt(TransInfo *t, const int UNUSED(mval[2]))
{
  int i;
  char str[UI_MAX_DRAW_STR];

  float final;

  final = t->values[0];

  snapGridIncrement(t, &final);

  applyNumInput(&t->num, &final);

  t->values[0] = final;

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("Tilt: %s° %s"), &c[0], t->proptext);

    /* XXX For some reason, this seems needed for this op, else RNA prop is not updated... :/ */
    t->values[0] = final;
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Tilt: %.2f° %s"), RAD2DEGF(final), t->proptext);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival + final * td->factor;
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Curve Shrink/Fatten) */

/** \name Transform Curve Shrink/Fatten
 * \{ */

static void initCurveShrinkFatten(TransInfo *t)
{
  t->mode = TFM_CURVE_SHRINKFATTEN;
  t->transform = applyCurveShrinkFatten;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
  t->num.val_flag[0] |= NUM_NO_ZERO;
#endif

  t->flag |= T_NO_CONSTRAINT;
}

static void applyCurveShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0];

  snapGridIncrement(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);
    BLI_snprintf(str, sizeof(str), TIP_("Shrink/Fatten: %s"), c);
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Shrink/Fatten: %3f"), ratio);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival * ratio;
        /* apply PET */
        *td->val = (*td->val * td->factor) + ((1.0f - td->factor) * td->ival);
        if (*td->val <= 0.0f) {
          *td->val = 0.001f;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Mask Shrink/Fatten) */

/** \name Transform Mask Shrink/Fatten
 * \{ */

static void initMaskShrinkFatten(TransInfo *t)
{
  t->mode = TFM_MASK_SHRINKFATTEN;
  t->transform = applyMaskShrinkFatten;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
  t->num.val_flag[0] |= NUM_NO_ZERO;
#endif

  t->flag |= T_NO_CONSTRAINT;
}

static void applyMaskShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
  float ratio;
  int i;
  bool initial_feather = false;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0];

  snapGridIncrement(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);
    BLI_snprintf(str, sizeof(str), TIP_("Feather Shrink/Fatten: %s"), c);
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Feather Shrink/Fatten: %3f"), ratio);
  }

  /* detect if no points have feather yet */
  if (ratio > 1.0f) {
    initial_feather = true;

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_NOACTION) {
          break;
        }

        if (td->flag & TD_SKIP) {
          continue;
        }

        if (td->ival >= 0.001f) {
          initial_feather = false;
        }
      }
    }
  }

  /* apply shrink/fatten */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (td = tc->data, i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        if (initial_feather) {
          *td->val = td->ival + (ratio - 1.0f) * 0.01f;
        }
        else {
          *td->val = td->ival * ratio;
        }

        /* apply PET */
        *td->val = (*td->val * td->factor) + ((1.0f - td->factor) * td->ival);
        if (*td->val <= 0.0f) {
          *td->val = 0.001f;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (GPencil Shrink/Fatten) */

/** \name Transform GPencil Strokes Shrink/Fatten
 * \{ */

static void initGPShrinkFatten(TransInfo *t)
{
  t->mode = TFM_GPENCIL_SHRINKFATTEN;
  t->transform = applyGPShrinkFatten;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
  t->num.val_flag[0] |= NUM_NO_ZERO;
#endif

  t->flag |= T_NO_CONSTRAINT;
}

static void applyGPShrinkFatten(TransInfo *t, const int UNUSED(mval[2]))
{
  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0];

  snapGridIncrement(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);
    BLI_snprintf(str, sizeof(str), TIP_("Shrink/Fatten: %s"), c);
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Shrink/Fatten: %3f"), ratio);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival * ratio;
        /* apply PET */
        *td->val = (*td->val * td->factor) + ((1.0f - td->factor) * td->ival);
        if (*td->val <= 0.0f) {
          *td->val = 0.001f;
        }
      }
    }
  }

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (GPencil Opacity) */

/** \name Transform GPencil Strokes Opacity
 * \{ */

static void initGPOpacity(TransInfo *t)
{
  t->mode = TFM_GPENCIL_OPACITY;
  t->transform = applyGPOpacity;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_ZERO;
#ifdef USE_NUM_NO_ZERO
  t->num.val_flag[0] |= NUM_NO_ZERO;
#endif

  t->flag |= T_NO_CONSTRAINT;
}

static void applyGPOpacity(TransInfo *t, const int UNUSED(mval[2]))
{
  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0];

  snapGridIncrement(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);
    BLI_snprintf(str, sizeof(str), TIP_("Opacity: %s"), c);
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Opacity: %3f"), ratio);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival * ratio;
        /* apply PET */
        *td->val = (*td->val * td->factor) + ((1.0f - td->factor) * td->ival);
        CLAMP(*td->val, 0.0f, 1.0f);
      }
    }
  }

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Push/Pull) */

/** \name Transform Push/Pull
 * \{ */

static void initPushPull(TransInfo *t)
{
  t->mode = TFM_PUSHPULL;
  t->transform = applyPushPull;

  initMouseInputMode(t, &t->mouse, INPUT_VERTICAL_ABSOLUTE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 1.0f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_LENGTH;
}

static void applyPushPull(TransInfo *t, const int UNUSED(mval[2]))
{
  float vec[3], axis_global[3];
  float distance;
  int i;
  char str[UI_MAX_DRAW_STR];

  distance = t->values[0];

  snapGridIncrement(t, &distance);

  applyNumInput(&t->num, &distance);

  t->values[0] = distance;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("Push/Pull: %s%s %s"), c, t->con.text, t->proptext);
  }
  else {
    /* default header print */
    BLI_snprintf(
        str, sizeof(str), TIP_("Push/Pull: %.4f%s %s"), distance, t->con.text, t->proptext);
  }

  if (t->con.applyRot && t->con.mode & CON_APPLY) {
    t->con.applyRot(t, NULL, NULL, axis_global, NULL);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      sub_v3_v3v3(vec, tc->center_local, td->center);
      if (t->con.applyRot && t->con.mode & CON_APPLY) {
        float axis[3];
        copy_v3_v3(axis, axis_global);
        t->con.applyRot(t, tc, td, axis, NULL);

        mul_m3_v3(td->smtx, axis);
        if (isLockConstraint(t)) {
          float dvec[3];
          project_v3_v3v3(dvec, vec, axis);
          sub_v3_v3(vec, dvec);
        }
        else {
          project_v3_v3v3(vec, vec, axis);
        }
      }
      normalize_v3_length(vec, distance * td->factor);

      add_v3_v3v3(td->loc, td->iloc, vec);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Bevel Weight) */

/** \name Transform Bevel Weight
 * \{ */

static void initBevelWeight(TransInfo *t)
{
  t->mode = TFM_BWEIGHT;
  t->transform = applyBevelWeight;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_DELTA);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyBevelWeight(TransInfo *t, const int UNUSED(mval[2]))
{
  float weight;
  int i;
  char str[UI_MAX_DRAW_STR];

  weight = t->values[0];

  CLAMP_MAX(weight, 1.0f);

  snapGridIncrement(t, &weight);

  applyNumInput(&t->num, &weight);

  t->values[0] = weight;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (weight >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: +%s %s"), c, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: %s %s"), c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (weight >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: +%.3f %s"), weight, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Bevel Weight: %.3f %s"), weight, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->val) {
        *td->val = td->ival + weight * td->factor;
        if (*td->val < 0.0f) {
          *td->val = 0.0f;
        }
        if (*td->val > 1.0f) {
          *td->val = 1.0f;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Crease) */

/** \name Transform Crease
 * \{ */

static void initCrease(TransInfo *t)
{
  t->mode = TFM_CREASE;
  t->transform = applyCrease;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_DELTA);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyCrease(TransInfo *t, const int UNUSED(mval[2]))
{
  float crease;
  int i;
  char str[UI_MAX_DRAW_STR];

  crease = t->values[0];

  CLAMP_MAX(crease, 1.0f);

  snapGridIncrement(t, &crease);

  applyNumInput(&t->num, &crease);

  t->values[0] = crease;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (crease >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: +%s %s"), c, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: %s %s"), c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (crease >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: +%.3f %s"), crease, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Crease: %.3f %s"), crease, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival + crease * td->factor;
        if (*td->val < 0.0f) {
          *td->val = 0.0f;
        }
        if (*td->val > 1.0f) {
          *td->val = 1.0f;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (EditBone (B-bone) width scaling) */

/** \name Transform B-bone width scaling
 * \{ */

static void initBoneSize(TransInfo *t)
{
  t->mode = TFM_BONESIZE;
  t->transform = applyBoneSize;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->idx_max = 2;
  t->num.idx_max = 2;
  t->num.val_flag[0] |= NUM_NULL_ONE;
  t->num.val_flag[1] |= NUM_NULL_ONE;
  t->num.val_flag[2] |= NUM_NULL_ONE;
  t->num.flag |= NUM_AFFECT_ALL;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
  t->num.unit_type[2] = B_UNIT_NONE;
}

static void headerBoneSize(TransInfo *t, const float vec[3], char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", vec[0]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN], NUM_STR_REP_LEN, "%.4f", vec[1]);
    BLI_snprintf(&tvec[NUM_STR_REP_LEN * 2], NUM_STR_REP_LEN, "%.4f", vec[2]);
  }

  /* hmm... perhaps the y-axis values don't need to be shown? */
  if (t->con.mode & CON_APPLY) {
    if (t->num.idx_max == 0) {
      BLI_snprintf(
          str, UI_MAX_DRAW_STR, TIP_("ScaleB: %s%s %s"), &tvec[0], t->con.text, t->proptext);
    }
    else {
      BLI_snprintf(str,
                   UI_MAX_DRAW_STR,
                   TIP_("ScaleB: %s : %s : %s%s %s"),
                   &tvec[0],
                   &tvec[NUM_STR_REP_LEN],
                   &tvec[NUM_STR_REP_LEN * 2],
                   t->con.text,
                   t->proptext);
    }
  }
  else {
    BLI_snprintf(str,
                 UI_MAX_DRAW_STR,
                 TIP_("ScaleB X: %s  Y: %s  Z: %s%s %s"),
                 &tvec[0],
                 &tvec[NUM_STR_REP_LEN],
                 &tvec[NUM_STR_REP_LEN * 2],
                 t->con.text,
                 t->proptext);
  }
}

static void ElementBoneSize(TransInfo *t, TransDataContainer *tc, TransData *td, float mat[3][3])
{
  float tmat[3][3], smat[3][3], oldy;
  float sizemat[3][3];

  mul_m3_m3m3(smat, mat, td->mtx);
  mul_m3_m3m3(tmat, td->smtx, smat);

  if (t->con.applySize) {
    t->con.applySize(t, tc, td, tmat);
  }

  /* we've tucked the scale in loc */
  oldy = td->iloc[1];
  size_to_mat3(sizemat, td->iloc);
  mul_m3_m3m3(tmat, tmat, sizemat);
  mat3_to_size(td->loc, tmat);
  td->loc[1] = oldy;
}

static void applyBoneSize(TransInfo *t, const int UNUSED(mval[2]))
{
  float size[3], mat[3][3];
  float ratio = t->values[0];
  int i;
  char str[UI_MAX_DRAW_STR];

  copy_v3_fl(size, ratio);

  snapGridIncrement(t, size);

  if (applyNumInput(&t->num, size)) {
    constraintNumInput(t, size);
  }

  copy_v3_v3(t->values, size);

  size_to_mat3(mat, size);

  if (t->con.applySize) {
    t->con.applySize(t, NULL, NULL, mat);
  }

  copy_m3_m3(t->mat, mat);  // used in gizmo

  headerBoneSize(t, size, str);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      ElementBoneSize(t, tc, td, mat);
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Bone Envelope) */

/** \name Transform Bone Envelope
 * \{ */

static void initBoneEnvelope(TransInfo *t)
{
  t->mode = TFM_BONE_ENVELOPE;
  t->transform = applyBoneEnvelope;

  initMouseInputMode(t, &t->mouse, INPUT_SPRING);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyBoneEnvelope(TransInfo *t, const int UNUSED(mval[2]))
{
  float ratio;
  int i;
  char str[UI_MAX_DRAW_STR];

  ratio = t->values[0];

  snapGridIncrement(t, &ratio);

  applyNumInput(&t->num, &ratio);

  t->values[0] = ratio;

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);
    BLI_snprintf(str, sizeof(str), TIP_("Envelope: %s"), c);
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Envelope: %3f"), ratio);
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        /* if the old/original value was 0.0f, then just use ratio */
        if (td->ival) {
          *td->val = td->ival * ratio;
        }
        else {
          *td->val = ratio;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Original Data Store */

/** \name Orig-Data Store Utility Functions
 * \{ */

static void slide_origdata_init_flag(TransInfo *t, TransDataContainer *tc, SlideOrigData *sod)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  const bool has_layer_math = CustomData_has_math(&bm->ldata);
  const int cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);

  if ((t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) &&
      /* don't do this at all for non-basis shape keys, too easy to
       * accidentally break uv maps or vertex colors then */
      (bm->shapenr <= 1) && (has_layer_math || (cd_loop_mdisp_offset != -1))) {
    sod->use_origfaces = true;
    sod->cd_loop_mdisp_offset = cd_loop_mdisp_offset;
  }
  else {
    sod->use_origfaces = false;
    sod->cd_loop_mdisp_offset = -1;
  }
}

static void slide_origdata_init_data(TransDataContainer *tc, SlideOrigData *sod)
{
  if (sod->use_origfaces) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;

    sod->origfaces = BLI_ghash_ptr_new(__func__);
    sod->bm_origfaces = BM_mesh_create(&bm_mesh_allocsize_default,
                                       &((struct BMeshCreateParams){
                                           .use_toolflags = false,
                                       }));
    /* we need to have matching customdata */
    BM_mesh_copy_init_customdata(sod->bm_origfaces, bm, NULL);
  }
}

static void slide_origdata_create_data_vert(BMesh *bm,
                                            SlideOrigData *sod,
                                            TransDataGenericSlideVert *sv)
{
  BMIter liter;
  int j, l_num;
  float *loop_weights;

  /* copy face data */
  // BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT) {
  BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, sv->v);
  l_num = liter.count;
  loop_weights = BLI_array_alloca(loop_weights, l_num);
  for (j = 0; j < l_num; j++) {
    BMLoop *l = BM_iter_step(&liter);
    BMLoop *l_prev, *l_next;
    void **val_p;
    if (!BLI_ghash_ensure_p(sod->origfaces, l->f, &val_p)) {
      BMFace *f_copy = BM_face_copy(sod->bm_origfaces, bm, l->f, true, true);
      *val_p = f_copy;
    }

    if ((l_prev = BM_loop_find_prev_nodouble(l, l->next, FLT_EPSILON)) &&
        (l_next = BM_loop_find_next_nodouble(l, l_prev, FLT_EPSILON))) {
      loop_weights[j] = angle_v3v3v3(l_prev->v->co, l->v->co, l_next->v->co);
    }
    else {
      loop_weights[j] = 0.0f;
    }
  }

  /* store cd_loop_groups */
  if (sod->layer_math_map_num && (l_num != 0)) {
    sv->cd_loop_groups = BLI_memarena_alloc(sod->arena, sod->layer_math_map_num * sizeof(void *));
    for (j = 0; j < sod->layer_math_map_num; j++) {
      const int layer_nr = sod->layer_math_map[j];
      sv->cd_loop_groups[j] = BM_vert_loop_groups_data_layer_create(
          bm, sv->v, layer_nr, loop_weights, sod->arena);
    }
  }
  else {
    sv->cd_loop_groups = NULL;
  }

  BLI_ghash_insert(sod->origverts, sv->v, sv);
}

static void slide_origdata_create_data(TransDataContainer *tc,
                                       SlideOrigData *sod,
                                       TransDataGenericSlideVert *sv_array,
                                       unsigned int v_stride,
                                       unsigned int v_num)
{
  if (sod->use_origfaces) {
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMesh *bm = em->bm;
    unsigned int i;
    TransDataGenericSlideVert *sv;

    int layer_index_dst;
    int j;

    layer_index_dst = 0;

    /* TODO: We don't need `sod->layer_math_map` when there are no loops linked
     * to one of the sliding vertices. */
    if (CustomData_has_math(&bm->ldata)) {
      /* over alloc, only 'math' layers are indexed */
      sod->layer_math_map = MEM_mallocN(bm->ldata.totlayer * sizeof(int), __func__);
      for (j = 0; j < bm->ldata.totlayer; j++) {
        if (CustomData_layer_has_math(&bm->ldata, j)) {
          sod->layer_math_map[layer_index_dst++] = j;
        }
      }
      BLI_assert(layer_index_dst != 0);
    }

    sod->layer_math_map_num = layer_index_dst;

    sod->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

    sod->origverts = BLI_ghash_ptr_new_ex(__func__, v_num);

    for (i = 0, sv = sv_array; i < v_num; i++, sv = POINTER_OFFSET(sv, v_stride)) {
      slide_origdata_create_data_vert(bm, sod, sv);
    }

    if (tc->mirror.axis_flag) {
      TransData *td = tc->data;
      TransDataGenericSlideVert *sv_mirror;

      sod->sv_mirror = MEM_callocN(sizeof(*sv_mirror) * tc->data_len, __func__);
      sod->totsv_mirror = tc->data_len;

      sv_mirror = sod->sv_mirror;

      for (i = 0; i < tc->data_len; i++, td++) {
        BMVert *eve = td->extra;
        /* Check the vertex has been used since both sides
         * of the mirror may be selected & sliding. */
        if (eve && !BLI_ghash_haskey(sod->origverts, eve)) {
          sv_mirror->v = eve;
          copy_v3_v3(sv_mirror->co_orig_3d, eve->co);

          slide_origdata_create_data_vert(bm, sod, sv_mirror);
          sv_mirror++;
        }
        else {
          sod->totsv_mirror--;
        }
      }

      if (sod->totsv_mirror == 0) {
        MEM_freeN(sod->sv_mirror);
        sod->sv_mirror = NULL;
      }
    }
  }
}

/**
 * If we're sliding the vert, return its original location, if not, the current location is good.
 */
static const float *slide_origdata_orig_vert_co(SlideOrigData *sod, BMVert *v)
{
  TransDataGenericSlideVert *sv = BLI_ghash_lookup(sod->origverts, v);
  return sv ? sv->co_orig_3d : v->co;
}

static void slide_origdata_interp_data_vert(SlideOrigData *sod,
                                            BMesh *bm,
                                            bool is_final,
                                            TransDataGenericSlideVert *sv)
{
  BMIter liter;
  int j, l_num;
  float *loop_weights;
  const bool is_moved = (len_squared_v3v3(sv->v->co, sv->co_orig_3d) > FLT_EPSILON);
  const bool do_loop_weight = sod->layer_math_map_num && is_moved;
  const bool do_loop_mdisps = is_final && is_moved && (sod->cd_loop_mdisp_offset != -1);
  const float *v_proj_axis = sv->v->no;
  /* original (l->prev, l, l->next) projections for each loop ('l' remains unchanged) */
  float v_proj[3][3];

  if (do_loop_weight || do_loop_mdisps) {
    project_plane_normalized_v3_v3v3(v_proj[1], sv->co_orig_3d, v_proj_axis);
  }

  // BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT)
  BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, sv->v);
  l_num = liter.count;
  loop_weights = do_loop_weight ? BLI_array_alloca(loop_weights, l_num) : NULL;
  for (j = 0; j < l_num; j++) {
    BMFace *f_copy; /* the copy of 'f' */
    BMLoop *l = BM_iter_step(&liter);

    f_copy = BLI_ghash_lookup(sod->origfaces, l->f);

    /* only loop data, no vertex data since that contains shape keys,
     * and we do not want to mess up other shape keys */
    BM_loop_interp_from_face(bm, l, f_copy, false, false);

    /* make sure face-attributes are correct (e.g. MTexPoly) */
    BM_elem_attrs_copy_ex(sod->bm_origfaces, bm, f_copy, l->f, 0x0, CD_MASK_NORMAL);

    /* weight the loop */
    if (do_loop_weight) {
      const float eps = 1.0e-8f;
      const BMLoop *l_prev = l->prev;
      const BMLoop *l_next = l->next;
      const float *co_prev = slide_origdata_orig_vert_co(sod, l_prev->v);
      const float *co_next = slide_origdata_orig_vert_co(sod, l_next->v);
      bool co_prev_ok;
      bool co_next_ok;

      /* In the unlikely case that we're next to a zero length edge -
       * walk around the to the next.
       *
       * Since we only need to check if the vertex is in this corner,
       * its not important _which_ loop - as long as its not overlapping
       * 'sv->co_orig_3d', see: T45096. */
      project_plane_normalized_v3_v3v3(v_proj[0], co_prev, v_proj_axis);
      while (UNLIKELY(((co_prev_ok = (len_squared_v3v3(v_proj[1], v_proj[0]) > eps)) == false) &&
                      ((l_prev = l_prev->prev) != l->next))) {
        co_prev = slide_origdata_orig_vert_co(sod, l_prev->v);
        project_plane_normalized_v3_v3v3(v_proj[0], co_prev, v_proj_axis);
      }
      project_plane_normalized_v3_v3v3(v_proj[2], co_next, v_proj_axis);
      while (UNLIKELY(((co_next_ok = (len_squared_v3v3(v_proj[1], v_proj[2]) > eps)) == false) &&
                      ((l_next = l_next->next) != l->prev))) {
        co_next = slide_origdata_orig_vert_co(sod, l_next->v);
        project_plane_normalized_v3_v3v3(v_proj[2], co_next, v_proj_axis);
      }

      if (co_prev_ok && co_next_ok) {
        const float dist = dist_signed_squared_to_corner_v3v3v3(
            sv->v->co, UNPACK3(v_proj), v_proj_axis);

        loop_weights[j] = (dist >= 0.0f) ? 1.0f : ((dist <= -eps) ? 0.0f : (1.0f + (dist / eps)));
        if (UNLIKELY(!isfinite(loop_weights[j]))) {
          loop_weights[j] = 0.0f;
        }
      }
      else {
        loop_weights[j] = 0.0f;
      }
    }
  }

  if (sod->layer_math_map_num && sv->cd_loop_groups) {
    if (do_loop_weight) {
      for (j = 0; j < sod->layer_math_map_num; j++) {
        BM_vert_loop_groups_data_layer_merge_weights(
            bm, sv->cd_loop_groups[j], sod->layer_math_map[j], loop_weights);
      }
    }
    else {
      for (j = 0; j < sod->layer_math_map_num; j++) {
        BM_vert_loop_groups_data_layer_merge(bm, sv->cd_loop_groups[j], sod->layer_math_map[j]);
      }
    }
  }

  /* Special handling for multires
   *
   * Interpolate from every other loop (not ideal)
   * However values will only be taken from loops which overlap other mdisps.
   * */
  if (do_loop_mdisps) {
    float(*faces_center)[3] = BLI_array_alloca(faces_center, l_num);
    BMLoop *l;

    BM_ITER_ELEM_INDEX (l, &liter, sv->v, BM_LOOPS_OF_VERT, j) {
      BM_face_calc_center_median(l->f, faces_center[j]);
    }

    BM_ITER_ELEM_INDEX (l, &liter, sv->v, BM_LOOPS_OF_VERT, j) {
      BMFace *f_copy = BLI_ghash_lookup(sod->origfaces, l->f);
      float f_copy_center[3];
      BMIter liter_other;
      BMLoop *l_other;
      int j_other;

      BM_face_calc_center_median(f_copy, f_copy_center);

      BM_ITER_ELEM_INDEX (l_other, &liter_other, sv->v, BM_LOOPS_OF_VERT, j_other) {
        BM_face_interp_multires_ex(bm,
                                   l_other->f,
                                   f_copy,
                                   faces_center[j_other],
                                   f_copy_center,
                                   sod->cd_loop_mdisp_offset);
      }
    }
  }
}

static void slide_origdata_interp_data(Object *obedit,
                                       SlideOrigData *sod,
                                       TransDataGenericSlideVert *sv,
                                       unsigned int v_stride,
                                       unsigned int v_num,
                                       bool is_final)
{
  if (sod->use_origfaces) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    BMesh *bm = em->bm;
    unsigned int i;
    const bool has_mdisps = (sod->cd_loop_mdisp_offset != -1);

    for (i = 0; i < v_num; i++, sv = POINTER_OFFSET(sv, v_stride)) {

      if (sv->cd_loop_groups || has_mdisps) {
        slide_origdata_interp_data_vert(sod, bm, is_final, sv);
      }
    }

    if (sod->sv_mirror) {
      sv = sod->sv_mirror;
      for (i = 0; i < v_num; i++, sv++) {
        if (sv->cd_loop_groups || has_mdisps) {
          slide_origdata_interp_data_vert(sod, bm, is_final, sv);
        }
      }
    }
  }
}

static void slide_origdata_free_date(SlideOrigData *sod)
{
  if (sod->use_origfaces) {
    if (sod->bm_origfaces) {
      BM_mesh_free(sod->bm_origfaces);
      sod->bm_origfaces = NULL;
    }

    if (sod->origfaces) {
      BLI_ghash_free(sod->origfaces, NULL, NULL);
      sod->origfaces = NULL;
    }

    if (sod->origverts) {
      BLI_ghash_free(sod->origverts, NULL, NULL);
      sod->origverts = NULL;
    }

    if (sod->arena) {
      BLI_memarena_free(sod->arena);
      sod->arena = NULL;
    }

    MEM_SAFE_FREE(sod->layer_math_map);

    MEM_SAFE_FREE(sod->sv_mirror);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Edge Slide) */

/** \name Transform Edge Slide
 * \{ */

static void calcEdgeSlideCustomPoints(struct TransInfo *t)
{
  EdgeSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;

  setCustomPoints(t, &t->mouse, sld->mval_end, sld->mval_start);

  /* setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

static BMEdge *get_other_edge(BMVert *v, BMEdge *e)
{
  BMIter iter;
  BMEdge *e_iter;

  BM_ITER_ELEM (e_iter, &iter, v, BM_EDGES_OF_VERT) {
    if (BM_elem_flag_test(e_iter, BM_ELEM_SELECT) && e_iter != e) {
      return e_iter;
    }
  }

  return NULL;
}

/* interpoaltes along a line made up of 2 segments (used for edge slide) */
static void interp_line_v3_v3v3v3(
    float p[3], const float v1[3], const float v2[3], const float v3[3], float t)
{
  float t_mid, t_delta;

  /* could be pre-calculated */
  t_mid = line_point_factor_v3(v2, v1, v3);

  t_delta = t - t_mid;
  if (t_delta < 0.0f) {
    if (UNLIKELY(fabsf(t_mid) < FLT_EPSILON)) {
      copy_v3_v3(p, v2);
    }
    else {
      interp_v3_v3v3(p, v1, v2, t / t_mid);
    }
  }
  else {
    t = t - t_mid;
    t_mid = 1.0f - t_mid;

    if (UNLIKELY(fabsf(t_mid) < FLT_EPSILON)) {
      copy_v3_v3(p, v3);
    }
    else {
      interp_v3_v3v3(p, v2, v3, t / t_mid);
    }
  }
}

/**
 * Find the closest point on the ngon on the opposite side.
 * used to set the edge slide distance for ngons.
 */
static bool bm_loop_calc_opposite_co(BMLoop *l_tmp, const float plane_no[3], float r_co[3])
{
  /* skip adjacent edges */
  BMLoop *l_first = l_tmp->next;
  BMLoop *l_last = l_tmp->prev;
  BMLoop *l_iter;
  float dist = FLT_MAX;
  bool found = false;

  l_iter = l_first;
  do {
    float tvec[3];
    if (isect_line_plane_v3(tvec, l_iter->v->co, l_iter->next->v->co, l_tmp->v->co, plane_no)) {
      const float fac = line_point_factor_v3(tvec, l_iter->v->co, l_iter->next->v->co);
      /* allow some overlap to avoid missing the intersection because of float precision */
      if ((fac > -FLT_EPSILON) && (fac < 1.0f + FLT_EPSILON)) {
        /* likelihood of multiple intersections per ngon is quite low,
         * it would have to loop back on its self, but better support it
         * so check for the closest opposite edge */
        const float tdist = len_v3v3(l_tmp->v->co, tvec);
        if (tdist < dist) {
          copy_v3_v3(r_co, tvec);
          dist = tdist;
          found = true;
        }
      }
    }
  } while ((l_iter = l_iter->next) != l_last);

  return found;
}

/**
 * Given 2 edges and a loop, step over the loops
 * and calculate a direction to slide along.
 *
 * \param r_slide_vec: the direction to slide,
 * the length of the vector defines the slide distance.
 */
static BMLoop *get_next_loop(
    BMVert *v, BMLoop *l, BMEdge *e_prev, BMEdge *e_next, float r_slide_vec[3])
{
  BMLoop *l_first;
  float vec_accum[3] = {0.0f, 0.0f, 0.0f};
  float vec_accum_len = 0.0f;
  int i = 0;

  BLI_assert(BM_edge_share_vert(e_prev, e_next) == v);
  BLI_assert(BM_vert_in_edge(l->e, v));

  l_first = l;
  do {
    l = BM_loop_other_edge_loop(l, v);

    if (l->e == e_next) {
      if (i) {
        normalize_v3_length(vec_accum, vec_accum_len / (float)i);
      }
      else {
        /* When there is no edge to slide along,
         * we must slide along the vector defined by the face we're attach to */
        BMLoop *l_tmp = BM_face_vert_share_loop(l_first->f, v);

        BLI_assert(ELEM(l_tmp->e, e_prev, e_next) && ELEM(l_tmp->prev->e, e_prev, e_next));

        if (l_tmp->f->len == 4) {
          /* we could use code below, but in this case
           * sliding diagonally across the quad works well */
          sub_v3_v3v3(vec_accum, l_tmp->next->next->v->co, v->co);
        }
        else {
          float tdir[3];
          BM_loop_calc_face_direction(l_tmp, tdir);
          cross_v3_v3v3(vec_accum, l_tmp->f->no, tdir);
#if 0
          /* rough guess, we can  do better! */
          normalize_v3_length(vec_accum,
                              (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f);
#else
          /* be clever, check the opposite ngon edge to slide into.
           * this gives best results */
          {
            float tvec[3];
            float dist;

            if (bm_loop_calc_opposite_co(l_tmp, tdir, tvec)) {
              dist = len_v3v3(l_tmp->v->co, tvec);
            }
            else {
              dist = (BM_edge_calc_length(e_prev) + BM_edge_calc_length(e_next)) / 2.0f;
            }

            normalize_v3_length(vec_accum, dist);
          }
#endif
        }
      }

      copy_v3_v3(r_slide_vec, vec_accum);
      return l;
    }
    else {
      /* accumulate the normalized edge vector,
       * normalize so some edges don't skew the result */
      float tvec[3];
      sub_v3_v3v3(tvec, BM_edge_other_vert(l->e, v)->co, v->co);
      vec_accum_len += normalize_v3(tvec);
      add_v3_v3(vec_accum, tvec);
      i += 1;
    }

    if (BM_loop_other_edge_loop(l, v)->e == e_next) {
      if (i) {
        normalize_v3_length(vec_accum, vec_accum_len / (float)i);
      }

      copy_v3_v3(r_slide_vec, vec_accum);
      return BM_loop_other_edge_loop(l, v);
    }

  } while ((l != l->radial_next) && ((l = l->radial_next) != l_first));

  if (i) {
    normalize_v3_length(vec_accum, vec_accum_len / (float)i);
  }

  copy_v3_v3(r_slide_vec, vec_accum);

  return NULL;
}

/**
 * Calculate screenspace `mval_start` / `mval_end`, optionally slide direction.
 */
static void calcEdgeSlide_mval_range(TransInfo *t,
                                     TransDataContainer *tc,
                                     EdgeSlideData *sld,
                                     const int *sv_table,
                                     const int loop_nr,
                                     const float mval[2],
                                     const bool use_occlude_geometry,
                                     const bool use_calc_direction)
{
  TransDataEdgeSlideVert *sv_array = sld->sv;
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  ARegion *ar = t->ar;
  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;
  float projectMat[4][4];
  BMBVHTree *bmbvh;

  /* only for use_calc_direction */
  float(*loop_dir)[3] = NULL, *loop_maxdist = NULL;

  float mval_start[2], mval_end[2];
  float mval_dir[3], dist_best_sq;
  BMIter iter;
  BMEdge *e;

  if (t->spacetype == SPACE_VIEW3D) {
    /* background mode support */
    v3d = t->sa ? t->sa->spacedata.first : NULL;
    rv3d = t->ar ? t->ar->regiondata : NULL;
  }

  if (!rv3d) {
    /* ok, let's try to survive this */
    unit_m4(projectMat);
  }
  else {
    ED_view3d_ob_project_mat_get(rv3d, tc->obedit, projectMat);
  }

  if (use_occlude_geometry) {
    bmbvh = BKE_bmbvh_new_from_editmesh(em, BMBVH_RESPECT_HIDDEN, NULL, false);
  }
  else {
    bmbvh = NULL;
  }

  /* find mouse vectors, the global one, and one per loop in case we have
   * multiple loops selected, in case they are oriented different */
  zero_v3(mval_dir);
  dist_best_sq = -1.0f;

  if (use_calc_direction) {
    loop_dir = MEM_callocN(sizeof(float[3]) * loop_nr, "sv loop_dir");
    loop_maxdist = MEM_mallocN(sizeof(float) * loop_nr, "sv loop_maxdist");
    copy_vn_fl(loop_maxdist, loop_nr, -1.0f);
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      int i;

      /* search cross edges for visible edge to the mouse cursor,
       * then use the shared vertex to calculate screen vector*/
      for (i = 0; i < 2; i++) {
        BMIter iter_other;
        BMEdge *e_other;

        BMVert *v = i ? e->v1 : e->v2;
        BM_ITER_ELEM (e_other, &iter_other, v, BM_EDGES_OF_VERT) {
          /* screen-space coords */
          float sco_a[3], sco_b[3];
          float dist_sq;
          int j, l_nr;

          if (BM_elem_flag_test(e_other, BM_ELEM_SELECT)) {
            continue;
          }

          /* This test is only relevant if object is not wire-drawn! See [#32068]. */
          if (use_occlude_geometry &&
              !BMBVH_EdgeVisible(bmbvh, e_other, t->depsgraph, ar, v3d, tc->obedit)) {
            continue;
          }

          BLI_assert(sv_table[BM_elem_index_get(v)] != -1);
          j = sv_table[BM_elem_index_get(v)];

          if (sv_array[j].v_side[1]) {
            ED_view3d_project_float_v3_m4(ar, sv_array[j].v_side[1]->co, sco_b, projectMat);
          }
          else {
            add_v3_v3v3(sco_b, v->co, sv_array[j].dir_side[1]);
            ED_view3d_project_float_v3_m4(ar, sco_b, sco_b, projectMat);
          }

          if (sv_array[j].v_side[0]) {
            ED_view3d_project_float_v3_m4(ar, sv_array[j].v_side[0]->co, sco_a, projectMat);
          }
          else {
            add_v3_v3v3(sco_a, v->co, sv_array[j].dir_side[0]);
            ED_view3d_project_float_v3_m4(ar, sco_a, sco_a, projectMat);
          }

          /* global direction */
          dist_sq = dist_squared_to_line_segment_v2(mval, sco_b, sco_a);
          if ((dist_best_sq == -1.0f) ||
              /* intentionally use 2d size on 3d vector */
              (dist_sq < dist_best_sq && (len_squared_v2v2(sco_b, sco_a) > 0.1f))) {
            dist_best_sq = dist_sq;
            sub_v3_v3v3(mval_dir, sco_b, sco_a);
          }

          if (use_calc_direction) {
            /* per loop direction */
            l_nr = sv_array[j].loop_nr;
            if (loop_maxdist[l_nr] == -1.0f || dist_sq < loop_maxdist[l_nr]) {
              loop_maxdist[l_nr] = dist_sq;
              sub_v3_v3v3(loop_dir[l_nr], sco_b, sco_a);
            }
          }
        }
      }
    }
  }

  if (use_calc_direction) {
    int i;
    sv_array = sld->sv;
    for (i = 0; i < sld->totsv; i++, sv_array++) {
      /* switch a/b if loop direction is different from global direction */
      int l_nr = sv_array->loop_nr;
      if (dot_v3v3(loop_dir[l_nr], mval_dir) < 0.0f) {
        swap_v3_v3(sv_array->dir_side[0], sv_array->dir_side[1]);
        SWAP(BMVert *, sv_array->v_side[0], sv_array->v_side[1]);
      }
    }

    MEM_freeN(loop_dir);
    MEM_freeN(loop_maxdist);
  }

  /* possible all of the edge loops are pointing directly at the view */
  if (UNLIKELY(len_squared_v2(mval_dir) < 0.1f)) {
    mval_dir[0] = 0.0f;
    mval_dir[1] = 100.0f;
  }

  /* zero out start */
  zero_v2(mval_start);

  /* dir holds a vector along edge loop */
  copy_v2_v2(mval_end, mval_dir);
  mul_v2_fl(mval_end, 0.5f);

  sld->mval_start[0] = t->mval[0] + mval_start[0];
  sld->mval_start[1] = t->mval[1] + mval_start[1];

  sld->mval_end[0] = t->mval[0] + mval_end[0];
  sld->mval_end[1] = t->mval[1] + mval_end[1];

  if (bmbvh) {
    BKE_bmbvh_free(bmbvh);
  }
}

static void calcEdgeSlide_even(TransInfo *t,
                               TransDataContainer *tc,
                               EdgeSlideData *sld,
                               const float mval[2])
{
  TransDataEdgeSlideVert *sv = sld->sv;

  if (sld->totsv > 0) {
    ARegion *ar = t->ar;
    RegionView3D *rv3d = NULL;
    float projectMat[4][4];

    int i = 0;

    float v_proj[2];
    float dist_sq = 0;
    float dist_min_sq = FLT_MAX;

    if (t->spacetype == SPACE_VIEW3D) {
      /* background mode support */
      rv3d = t->ar ? t->ar->regiondata : NULL;
    }

    if (!rv3d) {
      /* ok, let's try to survive this */
      unit_m4(projectMat);
    }
    else {
      ED_view3d_ob_project_mat_get(rv3d, tc->obedit, projectMat);
    }

    for (i = 0; i < sld->totsv; i++, sv++) {
      /* Set length */
      sv->edge_len = len_v3v3(sv->dir_side[0], sv->dir_side[1]);

      ED_view3d_project_float_v2_m4(ar, sv->v->co, v_proj, projectMat);
      dist_sq = len_squared_v2v2(mval, v_proj);
      if (dist_sq < dist_min_sq) {
        dist_min_sq = dist_sq;
        sld->curr_sv_index = i;
      }
    }
  }
  else {
    sld->curr_sv_index = 0;
  }
}

static bool createEdgeSlideVerts_double_side(TransInfo *t, TransDataContainer *tc)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  BMIter iter;
  BMEdge *e;
  BMVert *v;
  TransDataEdgeSlideVert *sv_array;
  int sv_tot;
  int *sv_table; /* BMVert -> sv_array index */
  EdgeSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
  float mval[2] = {(float)t->mval[0], (float)t->mval[1]};
  int numsel, i, loop_nr;
  bool use_occlude_geometry = false;
  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;

  slide_origdata_init_flag(t, tc, &sld->orig_data);

  sld->curr_sv_index = 0;

  /*ensure valid selection*/
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
      BMIter iter2;
      numsel = 0;
      BM_ITER_ELEM (e, &iter2, v, BM_EDGES_OF_VERT) {
        if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
          /* BMESH_TODO: this is probably very evil,
           * set v->e to a selected edge*/
          v->e = e;

          numsel++;
        }
      }

      if (numsel == 0 || numsel > 2) {
        MEM_freeN(sld);
        return false; /* invalid edge selection */
      }
    }
  }

  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (BM_elem_flag_test(e, BM_ELEM_SELECT)) {
      /* note, any edge with loops can work, but we won't get predictable results, so bail out */
      if (!BM_edge_is_manifold(e) && !BM_edge_is_boundary(e)) {
        /* can edges with at least once face user */
        MEM_freeN(sld);
        return false;
      }
    }
  }

  sv_table = MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

#define INDEX_UNSET -1
#define INDEX_INVALID -2

  {
    int j = 0;
    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        BM_elem_flag_enable(v, BM_ELEM_TAG);
        sv_table[i] = INDEX_UNSET;
        j += 1;
      }
      else {
        BM_elem_flag_disable(v, BM_ELEM_TAG);
        sv_table[i] = INDEX_INVALID;
      }
      BM_elem_index_set(v, i); /* set_inline */
    }
    bm->elem_index_dirty &= ~BM_VERT;

    if (!j) {
      MEM_freeN(sld);
      MEM_freeN(sv_table);
      return false;
    }
    sv_tot = j;
  }

  sv_array = MEM_callocN(sizeof(TransDataEdgeSlideVert) * sv_tot, "sv_array");
  loop_nr = 0;

  STACK_DECLARE(sv_array);
  STACK_INIT(sv_array, sv_tot);

  while (1) {
    float vec_a[3], vec_b[3];
    BMLoop *l_a, *l_b;
    BMLoop *l_a_prev, *l_b_prev;
    BMVert *v_first;
    /* If this succeeds call get_next_loop()
     * which calculates the direction to slide based on clever checks.
     *
     * otherwise we simply use 'e_dir' as an edge-rail.
     * (which is better when the attached edge is a boundary, see: T40422)
     */
#define EDGESLIDE_VERT_IS_INNER(v, e_dir) \
  ((BM_edge_is_boundary(e_dir) == false) && (BM_vert_edge_count_nonwire(v) == 2))

    v = NULL;
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        break;
      }
    }

    if (!v) {
      break;
    }

    if (!v->e) {
      continue;
    }

    v_first = v;

    /*walk along the edge loop*/
    e = v->e;

    /*first, rewind*/
    do {
      e = get_other_edge(v, e);
      if (!e) {
        e = v->e;
        break;
      }

      if (!BM_elem_flag_test(BM_edge_other_vert(e, v), BM_ELEM_TAG)) {
        break;
      }

      v = BM_edge_other_vert(e, v);
    } while (e != v_first->e);

    BM_elem_flag_disable(v, BM_ELEM_TAG);

    l_a = e->l;
    l_b = e->l->radial_next;

    /* regarding e_next, use get_next_loop()'s improved interpolation where possible */
    {
      BMEdge *e_next = get_other_edge(v, e);
      if (e_next) {
        get_next_loop(v, l_a, e, e_next, vec_a);
      }
      else {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
        if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
          get_next_loop(v, l_a, e, l_tmp->e, vec_a);
        }
        else {
          sub_v3_v3v3(vec_a, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
        }
      }
    }

    /* !BM_edge_is_boundary(e); */
    if (l_b != l_a) {
      BMEdge *e_next = get_other_edge(v, e);
      if (e_next) {
        get_next_loop(v, l_b, e, e_next, vec_b);
      }
      else {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
        if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
          get_next_loop(v, l_b, e, l_tmp->e, vec_b);
        }
        else {
          sub_v3_v3v3(vec_b, BM_edge_other_vert(l_tmp->e, v)->co, v->co);
        }
      }
    }
    else {
      l_b = NULL;
    }

    l_a_prev = NULL;
    l_b_prev = NULL;

#define SV_FROM_VERT(v) \
  ((sv_table[BM_elem_index_get(v)] == INDEX_UNSET) ? \
       ((void)(sv_table[BM_elem_index_get(v)] = STACK_SIZE(sv_array)), \
        STACK_PUSH_RET_PTR(sv_array)) : \
       (&sv_array[sv_table[BM_elem_index_get(v)]]))

    /*iterate over the loop*/
    v_first = v;
    do {
      bool l_a_ok_prev;
      bool l_b_ok_prev;
      TransDataEdgeSlideVert *sv;
      BMVert *v_prev;
      BMEdge *e_prev;

      /* XXX, 'sv' will initialize multiple times, this is suspicious. see [#34024] */
      BLI_assert(v != NULL);
      BLI_assert(sv_table[BM_elem_index_get(v)] != INDEX_INVALID);
      sv = SV_FROM_VERT(v);
      sv->v = v;
      copy_v3_v3(sv->v_co_orig, v->co);
      sv->loop_nr = loop_nr;

      if (l_a || l_a_prev) {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_a ? l_a : l_a_prev, v);
        sv->v_side[0] = BM_edge_other_vert(l_tmp->e, v);
        copy_v3_v3(sv->dir_side[0], vec_a);
      }

      if (l_b || l_b_prev) {
        BMLoop *l_tmp = BM_loop_other_edge_loop(l_b ? l_b : l_b_prev, v);
        sv->v_side[1] = BM_edge_other_vert(l_tmp->e, v);
        copy_v3_v3(sv->dir_side[1], vec_b);
      }

      v_prev = v;
      v = BM_edge_other_vert(e, v);

      e_prev = e;
      e = get_other_edge(v, e);

      if (!e) {
        BLI_assert(v != NULL);

        BLI_assert(sv_table[BM_elem_index_get(v)] != INDEX_INVALID);
        sv = SV_FROM_VERT(v);

        sv->v = v;
        copy_v3_v3(sv->v_co_orig, v->co);
        sv->loop_nr = loop_nr;

        if (l_a) {
          BMLoop *l_tmp = BM_loop_other_edge_loop(l_a, v);
          sv->v_side[0] = BM_edge_other_vert(l_tmp->e, v);
          if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
            get_next_loop(v, l_a, e_prev, l_tmp->e, sv->dir_side[0]);
          }
          else {
            sub_v3_v3v3(sv->dir_side[0], sv->v_side[0]->co, v->co);
          }
        }

        if (l_b) {
          BMLoop *l_tmp = BM_loop_other_edge_loop(l_b, v);
          sv->v_side[1] = BM_edge_other_vert(l_tmp->e, v);
          if (EDGESLIDE_VERT_IS_INNER(v, l_tmp->e)) {
            get_next_loop(v, l_b, e_prev, l_tmp->e, sv->dir_side[1]);
          }
          else {
            sub_v3_v3v3(sv->dir_side[1], sv->v_side[1]->co, v->co);
          }
        }

        BM_elem_flag_disable(v, BM_ELEM_TAG);
        BM_elem_flag_disable(v_prev, BM_ELEM_TAG);

        break;
      }
      l_a_ok_prev = (l_a != NULL);
      l_b_ok_prev = (l_b != NULL);

      l_a_prev = l_a;
      l_b_prev = l_b;

      if (l_a) {
        l_a = get_next_loop(v, l_a, e_prev, e, vec_a);
      }
      else {
        zero_v3(vec_a);
      }

      if (l_b) {
        l_b = get_next_loop(v, l_b, e_prev, e, vec_b);
      }
      else {
        zero_v3(vec_b);
      }

      if (l_a && l_b) {
        /* pass */
      }
      else {
        if (l_a || l_b) {
          /* find the opposite loop if it was missing previously */
          if (l_a == NULL && l_b && (l_b->radial_next != l_b)) {
            l_a = l_b->radial_next;
          }
          else if (l_b == NULL && l_a && (l_a->radial_next != l_a)) {
            l_b = l_a->radial_next;
          }
        }
        else if (e->l != NULL) {
          /* if there are non-contiguous faces, we can still recover
           * the loops of the new edges faces */

          /* note!, the behavior in this case means edges may move in opposite directions,
           * this could be made to work more usefully. */

          if (l_a_ok_prev) {
            l_a = e->l;
            l_b = (l_a->radial_next != l_a) ? l_a->radial_next : NULL;
          }
          else if (l_b_ok_prev) {
            l_b = e->l;
            l_a = (l_b->radial_next != l_b) ? l_b->radial_next : NULL;
          }
        }

        if (!l_a_ok_prev && l_a) {
          get_next_loop(v, l_a, e, e_prev, vec_a);
        }
        if (!l_b_ok_prev && l_b) {
          get_next_loop(v, l_b, e, e_prev, vec_b);
        }
      }

      BM_elem_flag_disable(v, BM_ELEM_TAG);
      BM_elem_flag_disable(v_prev, BM_ELEM_TAG);
    } while ((e != v_first->e) && (l_a || l_b));

#undef SV_FROM_VERT
#undef INDEX_UNSET
#undef INDEX_INVALID

    loop_nr++;

#undef EDGESLIDE_VERT_IS_INNER
  }

  /* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

  BLI_assert(STACK_SIZE(sv_array) == sv_tot);

  sld->sv = sv_array;
  sld->totsv = sv_tot;

  /* use for visibility checks */
  if (t->spacetype == SPACE_VIEW3D) {
    v3d = t->sa ? t->sa->spacedata.first : NULL;
    rv3d = t->ar ? t->ar->regiondata : NULL;
    use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE &&
                            !XRAY_ENABLED(v3d));
  }

  calcEdgeSlide_mval_range(t, tc, sld, sv_table, loop_nr, mval, use_occlude_geometry, true);

  /* create copies of faces for customdata projection */
  bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
  slide_origdata_init_data(tc, &sld->orig_data);
  slide_origdata_create_data(
      tc, &sld->orig_data, (TransDataGenericSlideVert *)sld->sv, sizeof(*sld->sv), sld->totsv);

  if (rv3d) {
    calcEdgeSlide_even(t, tc, sld, mval);
  }

  sld->em = em;

  tc->custom.mode.data = sld;

  MEM_freeN(sv_table);

  return true;
}

/**
 * A simple version of #createEdgeSlideVerts_double_side
 * Which assumes the longest unselected.
 */
static bool createEdgeSlideVerts_single_side(TransInfo *t, TransDataContainer *tc)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  BMIter iter;
  BMEdge *e;
  TransDataEdgeSlideVert *sv_array;
  int sv_tot;
  int *sv_table; /* BMVert -> sv_array index */
  EdgeSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
  float mval[2] = {(float)t->mval[0], (float)t->mval[1]};
  int loop_nr;
  bool use_occlude_geometry = false;
  View3D *v3d = NULL;
  RegionView3D *rv3d = NULL;

  if (t->spacetype == SPACE_VIEW3D) {
    /* background mode support */
    v3d = t->sa ? t->sa->spacedata.first : NULL;
    rv3d = t->ar ? t->ar->regiondata : NULL;
  }

  slide_origdata_init_flag(t, tc, &sld->orig_data);

  sld->curr_sv_index = 0;
  /* ensure valid selection */
  {
    int i = 0, j = 0;
    BMVert *v;

    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT)) {
        float len_sq_max = -1.0f;
        BMIter iter2;
        BM_ITER_ELEM (e, &iter2, v, BM_EDGES_OF_VERT) {
          if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
            float len_sq = BM_edge_calc_length_squared(e);
            if (len_sq > len_sq_max) {
              len_sq_max = len_sq;
              v->e = e;
            }
          }
        }

        if (len_sq_max != -1.0f) {
          j++;
        }
      }
      BM_elem_index_set(v, i); /* set_inline */
    }
    bm->elem_index_dirty &= ~BM_VERT;

    if (!j) {
      MEM_freeN(sld);
      return false;
    }

    sv_tot = j;
  }

  BLI_assert(sv_tot != 0);
  /* over alloc */
  sv_array = MEM_callocN(sizeof(TransDataEdgeSlideVert) * bm->totvertsel, "sv_array");

  /* same loop for all loops, weak but we dont connect loops in this case */
  loop_nr = 1;

  sv_table = MEM_mallocN(sizeof(*sv_table) * bm->totvert, __func__);

  {
    int i = 0, j = 0;
    BMVert *v;

    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      sv_table[i] = -1;
      if ((v->e != NULL) && (BM_elem_flag_test(v, BM_ELEM_SELECT))) {
        if (BM_elem_flag_test(v->e, BM_ELEM_SELECT) == 0) {
          TransDataEdgeSlideVert *sv;
          sv = &sv_array[j];
          sv->v = v;
          copy_v3_v3(sv->v_co_orig, v->co);
          sv->v_side[0] = BM_edge_other_vert(v->e, v);
          sub_v3_v3v3(sv->dir_side[0], sv->v_side[0]->co, v->co);
          sv->loop_nr = 0;
          sv_table[i] = j;
          j += 1;
        }
      }
    }
  }

  /* check for wire vertices,
   * interpolate the directions of wire verts between non-wire verts */
  if (sv_tot != bm->totvert) {
    const int sv_tot_nowire = sv_tot;
    TransDataEdgeSlideVert *sv_iter = sv_array;

    for (int i = 0; i < sv_tot_nowire; i++, sv_iter++) {
      BMIter eiter;
      BM_ITER_ELEM (e, &eiter, sv_iter->v, BM_EDGES_OF_VERT) {
        /* walk over wire */
        TransDataEdgeSlideVert *sv_end = NULL;
        BMEdge *e_step = e;
        BMVert *v = sv_iter->v;
        int j;

        j = sv_tot;

        while (1) {
          BMVert *v_other = BM_edge_other_vert(e_step, v);
          int endpoint = ((sv_table[BM_elem_index_get(v_other)] != -1) +
                          (BM_vert_is_edge_pair(v_other) == false));

          if ((BM_elem_flag_test(e_step, BM_ELEM_SELECT) &&
               BM_elem_flag_test(v_other, BM_ELEM_SELECT)) &&
              (endpoint == 0)) {
            /* scan down the list */
            TransDataEdgeSlideVert *sv;
            BLI_assert(sv_table[BM_elem_index_get(v_other)] == -1);
            sv_table[BM_elem_index_get(v_other)] = j;
            sv = &sv_array[j];
            sv->v = v_other;
            copy_v3_v3(sv->v_co_orig, v_other->co);
            copy_v3_v3(sv->dir_side[0], sv_iter->dir_side[0]);
            j++;

            /* advance! */
            v = v_other;
            e_step = BM_DISK_EDGE_NEXT(e_step, v_other);
          }
          else {
            if ((endpoint == 2) && (sv_tot != j)) {
              BLI_assert(BM_elem_index_get(v_other) != -1);
              sv_end = &sv_array[sv_table[BM_elem_index_get(v_other)]];
            }
            break;
          }
        }

        if (sv_end) {
          int sv_tot_prev = sv_tot;
          const float *co_src = sv_iter->v->co;
          const float *co_dst = sv_end->v->co;
          const float *dir_src = sv_iter->dir_side[0];
          const float *dir_dst = sv_end->dir_side[0];
          sv_tot = j;

          while (j-- != sv_tot_prev) {
            float factor;
            factor = line_point_factor_v3(sv_array[j].v->co, co_src, co_dst);
            interp_v3_v3v3(sv_array[j].dir_side[0], dir_src, dir_dst, factor);
          }
        }
      }
    }
  }

  /* EDBM_flag_disable_all(em, BM_ELEM_SELECT); */

  sld->sv = sv_array;
  sld->totsv = sv_tot;

  /* use for visibility checks */
  if (t->spacetype == SPACE_VIEW3D) {
    v3d = t->sa ? t->sa->spacedata.first : NULL;
    rv3d = t->ar ? t->ar->regiondata : NULL;
    use_occlude_geometry = (v3d && TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->dt > OB_WIRE &&
                            !XRAY_ENABLED(v3d));
  }

  calcEdgeSlide_mval_range(t, tc, sld, sv_table, loop_nr, mval, use_occlude_geometry, false);

  /* create copies of faces for customdata projection */
  bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
  slide_origdata_init_data(tc, &sld->orig_data);
  slide_origdata_create_data(
      tc, &sld->orig_data, (TransDataGenericSlideVert *)sld->sv, sizeof(*sld->sv), sld->totsv);

  if (rv3d) {
    calcEdgeSlide_even(t, tc, sld, mval);
  }

  sld->em = em;

  tc->custom.mode.data = sld;

  MEM_freeN(sv_table);

  return true;
}

void projectEdgeSlideData(TransInfo *t, bool is_final)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    EdgeSlideData *sld = tc->custom.mode.data;
    SlideOrigData *sod = &sld->orig_data;

    if (sod->use_origfaces == false) {
      return;
    }

    slide_origdata_interp_data(tc->obedit,
                               sod,
                               (TransDataGenericSlideVert *)sld->sv,
                               sizeof(*sld->sv),
                               sld->totsv,
                               is_final);
  }
}

void freeEdgeSlideTempFaces(EdgeSlideData *sld)
{
  slide_origdata_free_date(&sld->orig_data);
}

void freeEdgeSlideVerts(TransInfo *UNUSED(t),
                        TransDataContainer *UNUSED(tc),
                        TransCustomData *custom_data)
{
  EdgeSlideData *sld = custom_data->data;

  if (!sld) {
    return;
  }

  freeEdgeSlideTempFaces(sld);

  bmesh_edit_end(sld->em->bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);

  MEM_freeN(sld->sv);
  MEM_freeN(sld);

  custom_data->data = NULL;
}

static void initEdgeSlide_ex(
    TransInfo *t, bool use_double_side, bool use_even, bool flipped, bool use_clamp)
{
  EdgeSlideData *sld;
  bool ok = false;

  t->mode = TFM_EDGE_SLIDE;
  t->transform = applyEdgeSlide;
  t->handleEvent = handleEventEdgeSlide;

  {
    EdgeSlideParams *slp = MEM_callocN(sizeof(*slp), __func__);
    slp->use_even = use_even;
    slp->flipped = flipped;
    /* happens to be best for single-sided */
    if (use_double_side == false) {
      slp->flipped = !flipped;
    }
    slp->perc = 0.0f;

    if (!use_clamp) {
      t->flag |= T_ALT_TRANSFORM;
    }

    t->custom.mode.data = slp;
    t->custom.mode.use_free = true;
  }

  if (use_double_side) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      ok |= createEdgeSlideVerts_double_side(t, tc);
    }
  }
  else {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      ok |= createEdgeSlideVerts_single_side(t, tc);
    }
  }

  if (!ok) {
    t->state = TRANS_CANCEL;
    return;
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    sld = tc->custom.mode.data;
    if (!sld) {
      continue;
    }
    tc->custom.mode.free_cb = freeEdgeSlideVerts;
  }

  /* set custom point first if you want value to be initialized by init */
  calcEdgeSlideCustomPoints(t);
  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO_FLIP);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void initEdgeSlide(TransInfo *t)
{
  initEdgeSlide_ex(t, true, false, false, true);
}

static eRedrawFlag handleEventEdgeSlide(struct TransInfo *t, const struct wmEvent *event)
{
  if (t->mode == TFM_EDGE_SLIDE) {
    EdgeSlideParams *slp = t->custom.mode.data;

    if (slp) {
      switch (event->type) {
        case EKEY:
          if (event->val == KM_PRESS) {
            slp->use_even = !slp->use_even;
            calcEdgeSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case FKEY:
          if (event->val == KM_PRESS) {
            slp->flipped = !slp->flipped;
            calcEdgeSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case CKEY:
          /* use like a modifier key */
          if (event->val == KM_PRESS) {
            t->flag ^= T_ALT_TRANSFORM;
            calcEdgeSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case EVT_MODAL_MAP:
#if 0
          switch (event->val) {
            case TFM_MODAL_EDGESLIDE_DOWN:
              sld->curr_sv_index = ((sld->curr_sv_index - 1) + sld->totsv) % sld->totsv;
              return TREDRAW_HARD;
            case TFM_MODAL_EDGESLIDE_UP:
              sld->curr_sv_index = (sld->curr_sv_index + 1) % sld->totsv;
              return TREDRAW_HARD;
          }
#endif
          break;
        case MOUSEMOVE:
          calcEdgeSlideCustomPoints(t);
          break;
        default:
          break;
      }
    }
  }
  return TREDRAW_NOTHING;
}

static void drawEdgeSlide(TransInfo *t)
{
  if ((t->mode == TFM_EDGE_SLIDE) && TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data) {
    const EdgeSlideParams *slp = t->custom.mode.data;
    EdgeSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

    /* Even mode */
    if ((slp->use_even == true) || (is_clamp == false)) {
      const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;

      GPU_depth_test(false);

      GPU_blend(true);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

      GPU_matrix_push();
      GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);

      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

      if (slp->use_even == true) {
        float co_a[3], co_b[3], co_mark[3];
        TransDataEdgeSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
        const float fac = (slp->perc + 1.0f) / 2.0f;
        const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
        const float guide_size = ctrl_size - 0.5f;
        const int alpha_shade = -30;

        add_v3_v3v3(co_a, curr_sv->v_co_orig, curr_sv->dir_side[0]);
        add_v3_v3v3(co_b, curr_sv->v_co_orig, curr_sv->dir_side[1]);

        GPU_line_width(line_size);
        immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
        immBeginAtMost(GPU_PRIM_LINES, 4);
        if (curr_sv->v_side[0]) {
          immVertex3fv(pos, curr_sv->v_side[0]->co);
          immVertex3fv(pos, curr_sv->v_co_orig);
        }
        if (curr_sv->v_side[1]) {
          immVertex3fv(pos, curr_sv->v_side[1]->co);
          immVertex3fv(pos, curr_sv->v_co_orig);
        }
        immEnd();

        immUniformThemeColorShadeAlpha(TH_SELECT, -30, alpha_shade);
        GPU_point_size(ctrl_size);
        immBegin(GPU_PRIM_POINTS, 1);
        if (slp->flipped) {
          if (curr_sv->v_side[1]) {
            immVertex3fv(pos, curr_sv->v_side[1]->co);
          }
        }
        else {
          if (curr_sv->v_side[0]) {
            immVertex3fv(pos, curr_sv->v_side[0]->co);
          }
        }
        immEnd();

        immUniformThemeColorShadeAlpha(TH_SELECT, 255, alpha_shade);
        GPU_point_size(guide_size);
        immBegin(GPU_PRIM_POINTS, 1);
        interp_line_v3_v3v3v3(co_mark, co_b, curr_sv->v_co_orig, co_a, fac);
        immVertex3fv(pos, co_mark);
        immEnd();
      }
      else {
        if (is_clamp == false) {
          const int side_index = sld->curr_side_unclamp;
          TransDataEdgeSlideVert *sv;
          int i;
          const int alpha_shade = -160;

          GPU_line_width(line_size);
          immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);
          immBegin(GPU_PRIM_LINES, sld->totsv * 2);

          /* TODO(campbell): Loop over all verts  */
          sv = sld->sv;
          for (i = 0; i < sld->totsv; i++, sv++) {
            float a[3], b[3];

            if (!is_zero_v3(sv->dir_side[side_index])) {
              copy_v3_v3(a, sv->dir_side[side_index]);
            }
            else {
              copy_v3_v3(a, sv->dir_side[!side_index]);
            }

            mul_v3_fl(a, 100.0f);
            negate_v3_v3(b, a);
            add_v3_v3(a, sv->v_co_orig);
            add_v3_v3(b, sv->v_co_orig);

            immVertex3fv(pos, a);
            immVertex3fv(pos, b);
          }
          immEnd();
        }
        else {
          BLI_assert(0);
        }
      }

      immUnbindProgram();

      GPU_matrix_pop();

      GPU_blend(false);

      GPU_depth_test(true);
    }
  }
}

static void doEdgeSlide(TransInfo *t, float perc)
{
  EdgeSlideParams *slp = t->custom.mode.data;
  EdgeSlideData *sld_active = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;

  slp->perc = perc;

  if (slp->use_even == false) {
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
    if (is_clamp) {
      const int side_index = (perc < 0.0f);
      const float perc_final = fabsf(perc);
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        EdgeSlideData *sld = tc->custom.mode.data;
        TransDataEdgeSlideVert *sv = sld->sv;
        for (int i = 0; i < sld->totsv; i++, sv++) {
          madd_v3_v3v3fl(sv->v->co, sv->v_co_orig, sv->dir_side[side_index], perc_final);
        }
        sld->curr_side_unclamp = side_index;
      }
    }
    else {
      const float perc_init = fabsf(perc) *
                              ((sld_active->curr_side_unclamp == (perc < 0.0f)) ? 1 : -1);
      const int side_index = sld_active->curr_side_unclamp;
      FOREACH_TRANS_DATA_CONTAINER (t, tc) {
        EdgeSlideData *sld = tc->custom.mode.data;
        TransDataEdgeSlideVert *sv = sld->sv;
        for (int i = 0; i < sld->totsv; i++, sv++) {
          float dir_flip[3];
          float perc_final = perc_init;
          if (!is_zero_v3(sv->dir_side[side_index])) {
            copy_v3_v3(dir_flip, sv->dir_side[side_index]);
          }
          else {
            copy_v3_v3(dir_flip, sv->dir_side[!side_index]);
            perc_final *= -1;
          }
          madd_v3_v3v3fl(sv->v->co, sv->v_co_orig, dir_flip, perc_final);
        }
      }
    }
  }
  else {
    /**
     * Implementation note, even mode ignores the starting positions and uses
     * only the a/b verts, this could be changed/improved so the distance is
     * still met but the verts are moved along their original path (which may not be straight),
     * however how it works now is OK and matches 2.4x - Campbell
     *
     * \note `len_v3v3(curr_sv->dir_side[0], curr_sv->dir_side[1])`
     * is the same as the distance between the original vert locations,
     * same goes for the lines below.
     */
    TransDataEdgeSlideVert *curr_sv = &sld_active->sv[sld_active->curr_sv_index];
    const float curr_length_perc = curr_sv->edge_len *
                                   (((slp->flipped ? perc : -perc) + 1.0f) / 2.0f);

    float co_a[3];
    float co_b[3];

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      EdgeSlideData *sld = tc->custom.mode.data;
      TransDataEdgeSlideVert *sv = sld->sv;
      for (int i = 0; i < sld->totsv; i++, sv++) {
        if (sv->edge_len > FLT_EPSILON) {
          const float fac = min_ff(sv->edge_len, curr_length_perc) / sv->edge_len;

          add_v3_v3v3(co_a, sv->v_co_orig, sv->dir_side[0]);
          add_v3_v3v3(co_b, sv->v_co_orig, sv->dir_side[1]);

          if (slp->flipped) {
            interp_line_v3_v3v3v3(sv->v->co, co_b, sv->v_co_orig, co_a, fac);
          }
          else {
            interp_line_v3_v3v3v3(sv->v->co, co_a, sv->v_co_orig, co_b, fac);
          }
        }
      }
    }
  }
}

static void applyEdgeSlide(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  EdgeSlideParams *slp = t->custom.mode.data;
  bool flipped = slp->flipped;
  bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

  final = t->values[0];

  snapGridIncrement(t, &final);

  /* only do this so out of range values are not displayed */
  if (is_constrained) {
    CLAMP(final, -1.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values[0] = final;

  /* header string */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Edge Slide: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, &t->scene->unit);
    ofs += BLI_strncpy_rlen(str + ofs, &c[0], sizeof(str) - ofs);
  }
  else {
    ofs += BLI_snprintf(str + ofs, sizeof(str) - ofs, "%.4f ", final);
  }
  ofs += BLI_snprintf(
      str + ofs, sizeof(str) - ofs, TIP_("(E)ven: %s, "), WM_bool_as_string(use_even));
  if (use_even) {
    ofs += BLI_snprintf(
        str + ofs, sizeof(str) - ofs, TIP_("(F)lipped: %s, "), WM_bool_as_string(flipped));
  }
  ofs += BLI_snprintf(
      str + ofs, sizeof(str) - ofs, TIP_("Alt or (C)lamp: %s"), WM_bool_as_string(is_clamp));
  /* done with header string */

  /* do stuff here */
  doEdgeSlide(t, final);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Vert Slide) */

/** \name Transform Vert Slide
 * \{ */

static void calcVertSlideCustomPoints(struct TransInfo *t)
{
  VertSlideParams *slp = t->custom.mode.data;
  VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
  TransDataVertSlideVert *sv = &sld->sv[sld->curr_sv_index];

  const float *co_orig_3d = sv->co_orig_3d;
  const float *co_curr_3d = sv->co_link_orig_3d[sv->co_link_curr];

  float co_curr_2d[2], co_orig_2d[2];

  int mval_ofs[2], mval_start[2], mval_end[2];

  ED_view3d_project_float_v2_m4(t->ar, co_orig_3d, co_orig_2d, sld->proj_mat);
  ED_view3d_project_float_v2_m4(t->ar, co_curr_3d, co_curr_2d, sld->proj_mat);

  ARRAY_SET_ITEMS(mval_ofs, t->mouse.imval[0] - co_orig_2d[0], t->mouse.imval[1] - co_orig_2d[1]);
  ARRAY_SET_ITEMS(mval_start, co_orig_2d[0] + mval_ofs[0], co_orig_2d[1] + mval_ofs[1]);
  ARRAY_SET_ITEMS(mval_end, co_curr_2d[0] + mval_ofs[0], co_curr_2d[1] + mval_ofs[1]);

  if (slp->flipped && slp->use_even) {
    setCustomPoints(t, &t->mouse, mval_start, mval_end);
  }
  else {
    setCustomPoints(t, &t->mouse, mval_end, mval_start);
  }

  /* setCustomPoints isn't normally changing as the mouse moves,
   * in this case apply mouse input immediately so we don't refresh
   * with the value from the previous points */
  applyMouseInput(t, &t->mouse, t->mval, t->values);
}

/**
 * Run once when initializing vert slide to find the reference edge
 */
static void calcVertSlideMouseActiveVert(struct TransInfo *t, const int mval[2])
{
  /* Active object may have no selected vertices. */
  VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
  float mval_fl[2] = {UNPACK2(mval)};
  TransDataVertSlideVert *sv;

  /* set the vertex to use as a reference for the mouse direction 'curr_sv_index' */
  float dist_sq = 0.0f;
  float dist_min_sq = FLT_MAX;
  int i;

  for (i = 0, sv = sld->sv; i < sld->totsv; i++, sv++) {
    float co_2d[2];

    ED_view3d_project_float_v2_m4(t->ar, sv->co_orig_3d, co_2d, sld->proj_mat);

    dist_sq = len_squared_v2v2(mval_fl, co_2d);
    if (dist_sq < dist_min_sq) {
      dist_min_sq = dist_sq;
      sld->curr_sv_index = i;
    }
  }
}

/**
 * Run while moving the mouse to slide along the edge matching the mouse direction
 */
static void calcVertSlideMouseActiveEdges(struct TransInfo *t, const int mval[2])
{
  VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
  float imval_fl[2] = {UNPACK2(t->mouse.imval)};
  float mval_fl[2] = {UNPACK2(mval)};

  float dir[3];
  TransDataVertSlideVert *sv;
  int i;

  /* note: we could save a matrix-multiply for each vertex
   * by finding the closest edge in local-space.
   * However this skews the outcome with non-uniform-scale. */

  /* first get the direction of the original mouse position */
  sub_v2_v2v2(dir, imval_fl, mval_fl);
  ED_view3d_win_to_delta(t->ar, dir, dir, t->zfac);
  normalize_v3(dir);

  for (i = 0, sv = sld->sv; i < sld->totsv; i++, sv++) {
    if (sv->co_link_tot > 1) {
      float dir_dot_best = -FLT_MAX;
      int co_link_curr_best = -1;
      int j;

      for (j = 0; j < sv->co_link_tot; j++) {
        float tdir[3];
        float dir_dot;

        sub_v3_v3v3(tdir, sv->co_orig_3d, sv->co_link_orig_3d[j]);
        mul_mat3_m4_v3(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat, tdir);
        project_plane_v3_v3v3(tdir, tdir, t->viewinv[2]);

        normalize_v3(tdir);
        dir_dot = dot_v3v3(dir, tdir);
        if (dir_dot > dir_dot_best) {
          dir_dot_best = dir_dot;
          co_link_curr_best = j;
        }
      }

      if (co_link_curr_best != -1) {
        sv->co_link_curr = co_link_curr_best;
      }
    }
  }
}

static bool createVertSlideVerts(TransInfo *t, TransDataContainer *tc)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;
  BMIter iter;
  BMIter eiter;
  BMEdge *e;
  BMVert *v;
  TransDataVertSlideVert *sv_array;
  VertSlideData *sld = MEM_callocN(sizeof(*sld), "sld");
  int j;

  slide_origdata_init_flag(t, tc, &sld->orig_data);

  sld->curr_sv_index = 0;

  j = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    bool ok = false;
    if (BM_elem_flag_test(v, BM_ELEM_SELECT) && v->e) {
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          ok = true;
          break;
        }
      }
    }

    if (ok) {
      BM_elem_flag_enable(v, BM_ELEM_TAG);
      j += 1;
    }
    else {
      BM_elem_flag_disable(v, BM_ELEM_TAG);
    }
  }

  if (!j) {
    MEM_freeN(sld);
    return false;
  }

  sv_array = MEM_callocN(sizeof(TransDataVertSlideVert) * j, "sv_array");

  j = 0;
  BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
    if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
      int k;
      sv_array[j].v = v;
      copy_v3_v3(sv_array[j].co_orig_3d, v->co);

      k = 0;
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          k++;
        }
      }

      sv_array[j].co_link_orig_3d = MEM_mallocN(sizeof(*sv_array[j].co_link_orig_3d) * k,
                                                __func__);
      sv_array[j].co_link_tot = k;

      k = 0;
      BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
        if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
          BMVert *v_other = BM_edge_other_vert(e, v);
          copy_v3_v3(sv_array[j].co_link_orig_3d[k], v_other->co);
          k++;
        }
      }
      j++;
    }
  }

  sld->sv = sv_array;
  sld->totsv = j;

  bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
  slide_origdata_init_data(tc, &sld->orig_data);
  slide_origdata_create_data(
      tc, &sld->orig_data, (TransDataGenericSlideVert *)sld->sv, sizeof(*sld->sv), sld->totsv);

  sld->em = em;

  tc->custom.mode.data = sld;

  /* most likely will be set below */
  unit_m4(sld->proj_mat);

  if (t->spacetype == SPACE_VIEW3D) {
    /* view vars */
    RegionView3D *rv3d = NULL;
    ARegion *ar = t->ar;

    rv3d = ar ? ar->regiondata : NULL;
    if (rv3d) {
      ED_view3d_ob_project_mat_get(rv3d, tc->obedit, sld->proj_mat);
    }
  }

  /* XXX, calc vert slide across all objects */
  if (tc == t->data_container) {
    calcVertSlideMouseActiveVert(t, t->mval);
    calcVertSlideMouseActiveEdges(t, t->mval);
  }

  return true;
}

void projectVertSlideData(TransInfo *t, bool is_final)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    VertSlideData *sld = tc->custom.mode.data;
    SlideOrigData *sod = &sld->orig_data;
    if (sod->use_origfaces == true) {
      slide_origdata_interp_data(tc->obedit,
                                 sod,
                                 (TransDataGenericSlideVert *)sld->sv,
                                 sizeof(*sld->sv),
                                 sld->totsv,
                                 is_final);
    }
  }
}

void freeVertSlideTempFaces(VertSlideData *sld)
{
  slide_origdata_free_date(&sld->orig_data);
}

void freeVertSlideVerts(TransInfo *UNUSED(t),
                        TransDataContainer *UNUSED(tc),
                        TransCustomData *custom_data)
{
  VertSlideData *sld = custom_data->data;

  if (!sld) {
    return;
  }

  freeVertSlideTempFaces(sld);

  bmesh_edit_end(sld->em->bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);

  if (sld->totsv > 0) {
    TransDataVertSlideVert *sv = sld->sv;
    int i = 0;
    for (i = 0; i < sld->totsv; i++, sv++) {
      MEM_freeN(sv->co_link_orig_3d);
    }
  }

  MEM_freeN(sld->sv);
  MEM_freeN(sld);

  custom_data->data = NULL;
}

static void initVertSlide_ex(TransInfo *t, bool use_even, bool flipped, bool use_clamp)
{

  t->mode = TFM_VERT_SLIDE;
  t->transform = applyVertSlide;
  t->handleEvent = handleEventVertSlide;

  {
    VertSlideParams *slp = MEM_callocN(sizeof(*slp), __func__);
    slp->use_even = use_even;
    slp->flipped = flipped;
    slp->perc = 0.0f;

    if (!use_clamp) {
      t->flag |= T_ALT_TRANSFORM;
    }

    t->custom.mode.data = slp;
    t->custom.mode.use_free = true;
  }

  bool ok = false;
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    ok |= createVertSlideVerts(t, tc);
    VertSlideData *sld = tc->custom.mode.data;
    if (sld) {
      tc->custom.mode.free_cb = freeVertSlideVerts;
    }
  }

  if (ok == false) {
    t->state = TRANS_CANCEL;
    return;
  }

  /* set custom point first if you want value to be initialized by init */
  calcVertSlideCustomPoints(t);
  initMouseInputMode(t, &t->mouse, INPUT_CUSTOM_RATIO);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 0.1f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void initVertSlide(TransInfo *t)
{
  initVertSlide_ex(t, false, false, true);
}

static eRedrawFlag handleEventVertSlide(struct TransInfo *t, const struct wmEvent *event)
{
  if (t->mode == TFM_VERT_SLIDE) {
    VertSlideParams *slp = t->custom.mode.data;

    if (slp) {
      switch (event->type) {
        case EKEY:
          if (event->val == KM_PRESS) {
            slp->use_even = !slp->use_even;
            if (slp->flipped) {
              calcVertSlideCustomPoints(t);
            }
            return TREDRAW_HARD;
          }
          break;
        case FKEY:
          if (event->val == KM_PRESS) {
            slp->flipped = !slp->flipped;
            calcVertSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
        case CKEY:
          /* use like a modifier key */
          if (event->val == KM_PRESS) {
            t->flag ^= T_ALT_TRANSFORM;
            calcVertSlideCustomPoints(t);
            return TREDRAW_HARD;
          }
          break;
#if 0
        case EVT_MODAL_MAP:
          switch (event->val) {
            case TFM_MODAL_EDGESLIDE_DOWN:
              sld->curr_sv_index = ((sld->curr_sv_index - 1) + sld->totsv) % sld->totsv;
              break;
            case TFM_MODAL_EDGESLIDE_UP:
              sld->curr_sv_index = (sld->curr_sv_index + 1) % sld->totsv;
              break;
          }
          break;
#endif
        case MOUSEMOVE: {
          /* don't recalculate the best edge */
          const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
          if (is_clamp) {
            calcVertSlideMouseActiveEdges(t, event->mval);
          }
          calcVertSlideCustomPoints(t);
          break;
        }
        default:
          break;
      }
    }
  }
  return TREDRAW_NOTHING;
}

static void drawVertSlide(TransInfo *t)
{
  if ((t->mode == TFM_VERT_SLIDE) && TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data) {
    const VertSlideParams *slp = t->custom.mode.data;
    VertSlideData *sld = TRANS_DATA_CONTAINER_FIRST_OK(t)->custom.mode.data;
    const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);

    /* Non-Prop mode */
    {
      TransDataVertSlideVert *curr_sv = &sld->sv[sld->curr_sv_index];
      TransDataVertSlideVert *sv;
      const float ctrl_size = UI_GetThemeValuef(TH_FACEDOT_SIZE) + 1.5f;
      const float line_size = UI_GetThemeValuef(TH_OUTLINE_WIDTH) + 0.5f;
      const int alpha_shade = -160;
      int i;

      GPU_depth_test(false);

      GPU_blend(true);
      GPU_blend_set_func_separate(
          GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);

      GPU_matrix_push();
      GPU_matrix_mul(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);

      GPU_line_width(line_size);

      const uint shdr_pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColorShadeAlpha(TH_EDGE_SELECT, 80, alpha_shade);

      immBegin(GPU_PRIM_LINES, sld->totsv * 2);
      if (is_clamp) {
        sv = sld->sv;
        for (i = 0; i < sld->totsv; i++, sv++) {
          immVertex3fv(shdr_pos, sv->co_orig_3d);
          immVertex3fv(shdr_pos, sv->co_link_orig_3d[sv->co_link_curr]);
        }
      }
      else {
        sv = sld->sv;
        for (i = 0; i < sld->totsv; i++, sv++) {
          float a[3], b[3];
          sub_v3_v3v3(a, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
          mul_v3_fl(a, 100.0f);
          negate_v3_v3(b, a);
          add_v3_v3(a, sv->co_orig_3d);
          add_v3_v3(b, sv->co_orig_3d);

          immVertex3fv(shdr_pos, a);
          immVertex3fv(shdr_pos, b);
        }
      }
      immEnd();

      GPU_point_size(ctrl_size);

      immBegin(GPU_PRIM_POINTS, 1);
      immVertex3fv(shdr_pos,
                   (slp->flipped && slp->use_even) ?
                       curr_sv->co_link_orig_3d[curr_sv->co_link_curr] :
                       curr_sv->co_orig_3d);
      immEnd();

      immUnbindProgram();

      /* direction from active vertex! */
      if ((t->mval[0] != t->mouse.imval[0]) || (t->mval[1] != t->mouse.imval[1])) {
        float zfac;
        float mval_ofs[2];
        float co_orig_3d[3];
        float co_dest_3d[3];

        mval_ofs[0] = t->mval[0] - t->mouse.imval[0];
        mval_ofs[1] = t->mval[1] - t->mouse.imval[1];

        mul_v3_m4v3(
            co_orig_3d, TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat, curr_sv->co_orig_3d);
        zfac = ED_view3d_calc_zfac(t->ar->regiondata, co_orig_3d, NULL);

        ED_view3d_win_to_delta(t->ar, mval_ofs, co_dest_3d, zfac);

        invert_m4_m4(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->imat,
                     TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->obmat);
        mul_mat3_m4_v3(TRANS_DATA_CONTAINER_FIRST_OK(t)->obedit->imat, co_dest_3d);

        add_v3_v3(co_dest_3d, curr_sv->co_orig_3d);

        GPU_line_width(1.0f);

        immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

        float viewport_size[4];
        GPU_viewport_size_get_f(viewport_size);
        immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

        immUniform1i("colors_len", 0); /* "simple" mode */
        immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        immUniform1f("dash_width", 6.0f);
        immUniform1f("dash_factor", 0.5f);

        immBegin(GPU_PRIM_LINES, 2);
        immVertex3fv(shdr_pos, curr_sv->co_orig_3d);
        immVertex3fv(shdr_pos, co_dest_3d);
        immEnd();

        immUnbindProgram();
      }

      GPU_matrix_pop();

      GPU_depth_test(true);
    }
  }
}

static void doVertSlide(TransInfo *t, float perc)
{
  VertSlideParams *slp = t->custom.mode.data;

  slp->perc = perc;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    VertSlideData *sld = tc->custom.mode.data;
    TransDataVertSlideVert *svlist = sld->sv, *sv;
    int i;

    sv = svlist;

    if (slp->use_even == false) {
      for (i = 0; i < sld->totsv; i++, sv++) {
        interp_v3_v3v3(sv->v->co, sv->co_orig_3d, sv->co_link_orig_3d[sv->co_link_curr], perc);
      }
    }
    else {
      TransDataVertSlideVert *sv_curr = &sld->sv[sld->curr_sv_index];
      const float edge_len_curr = len_v3v3(sv_curr->co_orig_3d,
                                           sv_curr->co_link_orig_3d[sv_curr->co_link_curr]);
      const float tperc = perc * edge_len_curr;

      for (i = 0; i < sld->totsv; i++, sv++) {
        float edge_len;
        float dir[3];

        sub_v3_v3v3(dir, sv->co_link_orig_3d[sv->co_link_curr], sv->co_orig_3d);
        edge_len = normalize_v3(dir);

        if (edge_len > FLT_EPSILON) {
          if (slp->flipped) {
            madd_v3_v3v3fl(sv->v->co, sv->co_link_orig_3d[sv->co_link_curr], dir, -tperc);
          }
          else {
            madd_v3_v3v3fl(sv->v->co, sv->co_orig_3d, dir, tperc);
          }
        }
        else {
          copy_v3_v3(sv->v->co, sv->co_orig_3d);
        }
      }
    }
  }
}

static void applyVertSlide(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];
  size_t ofs = 0;
  float final;
  VertSlideParams *slp = t->custom.mode.data;
  const bool flipped = slp->flipped;
  const bool use_even = slp->use_even;
  const bool is_clamp = !(t->flag & T_ALT_TRANSFORM);
  const bool is_constrained = !(is_clamp == false || hasNumInput(&t->num));

  final = t->values[0];

  snapGridIncrement(t, &final);

  /* only do this so out of range values are not displayed */
  if (is_constrained) {
    CLAMP(final, 0.0f, 1.0f);
  }

  applyNumInput(&t->num, &final);

  t->values[0] = final;

  /* header string */
  ofs += BLI_strncpy_rlen(str + ofs, TIP_("Vert Slide: "), sizeof(str) - ofs);
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];
    outputNumInput(&(t->num), c, &t->scene->unit);
    ofs += BLI_strncpy_rlen(str + ofs, &c[0], sizeof(str) - ofs);
  }
  else {
    ofs += BLI_snprintf(str + ofs, sizeof(str) - ofs, "%.4f ", final);
  }
  ofs += BLI_snprintf(
      str + ofs, sizeof(str) - ofs, TIP_("(E)ven: %s, "), WM_bool_as_string(use_even));
  if (use_even) {
    ofs += BLI_snprintf(
        str + ofs, sizeof(str) - ofs, TIP_("(F)lipped: %s, "), WM_bool_as_string(flipped));
  }
  ofs += BLI_snprintf(
      str + ofs, sizeof(str) - ofs, TIP_("Alt or (C)lamp: %s"), WM_bool_as_string(is_clamp));
  /* done with header string */

  /* do stuff here */
  doVertSlide(t, final);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (EditBone Roll) */

/** \name Transform EditBone Roll
 * \{ */

static void initBoneRoll(TransInfo *t)
{
  t->mode = TFM_BONE_ROLL;
  t->transform = applyBoneRoll;

  initMouseInputMode(t, &t->mouse, INPUT_ANGLE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = DEG2RAD(5.0);
  t->snap[2] = DEG2RAD(1.0);

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_use_radians = (t->scene->unit.system_rotation == USER_UNIT_ROT_RADIANS);
  t->num.unit_type[0] = B_UNIT_ROTATION;

  t->flag |= T_NO_CONSTRAINT | T_NO_PROJECT;
}

static void applyBoneRoll(TransInfo *t, const int UNUSED(mval[2]))
{
  int i;
  char str[UI_MAX_DRAW_STR];

  float final;

  final = t->values[0];

  snapGridIncrement(t, &final);

  applyNumInput(&t->num, &final);

  t->values[0] = final;

  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    BLI_snprintf(str, sizeof(str), TIP_("Roll: %s"), &c[0]);
  }
  else {
    BLI_snprintf(str, sizeof(str), TIP_("Roll: %.2f"), RAD2DEGF(final));
  }

  /* set roll values */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      *(td->val) = td->ival - final;
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Bake-Time) */

/** \name Transform Bake-Time
 * \{ */

static void initBakeTime(TransInfo *t)
{
  t->transform = applyBakeTime;
  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  t->idx_max = 0;
  t->num.idx_max = 0;
  t->snap[0] = 0.0f;
  t->snap[1] = 1.0f;
  t->snap[2] = t->snap[1] * 0.1f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE; /* Don't think this uses units? */
}

static void applyBakeTime(TransInfo *t, const int mval[2])
{
  float time;
  int i;
  char str[UI_MAX_DRAW_STR];

  float fac = 0.1f;

  /* XXX, disable precision for now,
   * this isn't even accessible by the user */
#if 0
  if (t->mouse.precision) {
    /* calculate ratio for shiftkey pos, and for total, and blend these for precision */
    time = (float)(t->center2d[0] - t->mouse.precision_mval[0]) * fac;
    time += 0.1f * ((float)(t->center2d[0] * fac - mval[0]) - time);
  }
  else
#endif
  {
    time = (float)(t->center2d[0] - mval[0]) * fac;
  }

  snapGridIncrement(t, &time);

  applyNumInput(&t->num, &time);

  /* header print for NumInput */
  if (hasNumInput(&t->num)) {
    char c[NUM_STR_REP_LEN];

    outputNumInput(&(t->num), c, &t->scene->unit);

    if (time >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Time: +%s %s"), c, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Time: %s %s"), c, t->proptext);
    }
  }
  else {
    /* default header print */
    if (time >= 0.0f) {
      BLI_snprintf(str, sizeof(str), TIP_("Time: +%.3f %s"), time, t->proptext);
    }
    else {
      BLI_snprintf(str, sizeof(str), TIP_("Time: %.3f %s"), time, t->proptext);
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      if (td->val) {
        *td->val = td->ival + time * td->factor;
        if (td->ext->size && *td->val < *td->ext->size) {
          *td->val = *td->ext->size;
        }
        if (td->ext->quat && *td->val > *td->ext->quat) {
          *td->val = *td->ext->quat;
        }
      }
    }
  }

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Mirror) */

/** \name Transform Mirror
 * \{ */

static void initMirror(TransInfo *t)
{
  t->transform = applyMirror;
  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  t->flag |= T_NULL_ONE;
  if ((t->flag & T_EDIT) == 0) {
    t->flag |= T_NO_ZERO;
  }
}

static void applyMirror(TransInfo *t, const int UNUSED(mval[2]))
{
  float size[3], mat[3][3];
  int i;
  char str[UI_MAX_DRAW_STR];

  /*
   * OPTIMIZATION:
   * This still recalcs transformation on mouse move
   * while it should only recalc on constraint change
   * */

  /* if an axis has been selected */
  if (t->con.mode & CON_APPLY) {
    size[0] = size[1] = size[2] = -1;

    size_to_mat3(mat, size);

    if (t->con.applySize) {
      t->con.applySize(t, NULL, NULL, mat);
    }

    BLI_snprintf(str, sizeof(str), TIP_("Mirror%s"), t->con.text);

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_NOACTION) {
          break;
        }

        if (td->flag & TD_SKIP) {
          continue;
        }

        ElementResize(t, tc, td, mat);
      }
    }

    recalcData(t);

    ED_area_status_text(t->sa, str);
  }
  else {
    size[0] = size[1] = size[2] = 1;

    size_to_mat3(mat, size);

    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        if (td->flag & TD_NOACTION) {
          break;
        }

        if (td->flag & TD_SKIP) {
          continue;
        }

        ElementResize(t, tc, td, mat);
      }
    }

    recalcData(t);

    if (t->flag & T_2D_EDIT) {
      ED_area_status_text(t->sa, TIP_("Select a mirror axis (X, Y)"));
    }
    else {
      ED_area_status_text(t->sa, TIP_("Select a mirror axis (X, Y, Z)"));
    }
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Align) */

/** \name Transform Align
 * \{ */

static void initAlign(TransInfo *t)
{
  t->flag |= T_NO_CONSTRAINT;

  t->transform = applyAlign;

  initMouseInputMode(t, &t->mouse, INPUT_NONE);
}

static void applyAlign(TransInfo *t, const int UNUSED(mval[2]))
{
  float center[3];
  int i;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    /* saving original center */
    copy_v3_v3(center, tc->center_local);
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      float mat[3][3], invmat[3][3];

      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      /* around local centers */
      if (t->flag & (T_OBJECT | T_POSE)) {
        copy_v3_v3(tc->center_local, td->center);
      }
      else {
        if (t->settings->selectmode & SCE_SELECT_FACE) {
          copy_v3_v3(tc->center_local, td->center);
        }
      }

      invert_m3_m3(invmat, td->axismtx);

      mul_m3_m3m3(mat, t->spacemtx, invmat);

      ElementRotation(t, tc, td, mat, t->around);
    }
    /* restoring original center */
    copy_v3_v3(tc->center_local, center);
  }

  recalcData(t);

  ED_area_status_text(t->sa, TIP_("Align"));
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Sequencer Slide) */

/** \name Transform Sequencer Slide
 * \{ */

static void initSeqSlide(TransInfo *t)
{
  t->transform = applySeqSlide;

  initMouseInputMode(t, &t->mouse, INPUT_VECTOR);

  t->idx_max = 1;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  t->snap[0] = 0.0f;
  t->snap[1] = floorf(t->scene->r.frs_sec / t->scene->r.frs_sec_base);
  t->snap[2] = 10.0f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  /* Would be nice to have a time handling in units as well
   * (supporting frames in addition to "natural" time...). */
  t->num.unit_type[0] = B_UNIT_NONE;
  t->num.unit_type[1] = B_UNIT_NONE;
}

static void headerSeqSlide(TransInfo *t, const float val[2], char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  size_t ofs = 0;

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.0f, %.0f", val[0], val[1]);
  }

  ofs += BLI_snprintf(
      str + ofs, UI_MAX_DRAW_STR - ofs, TIP_("Sequence Slide: %s%s, ("), &tvec[0], t->con.text);

  if (t->keymap) {
    wmKeyMapItem *kmi = WM_modalkeymap_find_propvalue(t->keymap, TFM_MODAL_TRANSLATE);
    if (kmi) {
      ofs += WM_keymap_item_to_string(kmi, false, str + ofs, UI_MAX_DRAW_STR - ofs);
    }
  }
  ofs += BLI_snprintf(str + ofs,
                      UI_MAX_DRAW_STR - ofs,
                      TIP_(" or Alt) Expand to fit %s"),
                      WM_bool_as_string((t->flag & T_ALT_TRANSFORM) != 0));
}

static void applySeqSlideValue(TransInfo *t, const float val[2])
{
  int i;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      if (td->flag & TD_NOACTION) {
        break;
      }

      if (td->flag & TD_SKIP) {
        continue;
      }

      madd_v2_v2v2fl(td->loc, td->iloc, val, td->factor);
    }
  }
}

static void applySeqSlide(TransInfo *t, const int mval[2])
{
  char str[UI_MAX_DRAW_STR];

  snapSequenceBounds(t, mval);

  if (t->con.mode & CON_APPLY) {
    float pvec[3] = {0.0f, 0.0f, 0.0f};
    float tvec[3];
    t->con.applyVec(t, NULL, NULL, t->values, tvec, pvec);
    copy_v3_v3(t->values, tvec);
  }
  else {
    // snapGridIncrement(t, t->values);
    applyNumInput(&t->num, t->values);
  }

  t->values[0] = floorf(t->values[0] + 0.5f);
  t->values[1] = floorf(t->values[1] + 0.5f);

  headerSeqSlide(t, t->values, str);
  applySeqSlideValue(t, t->values);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Animation Editors - Transform Utils
 *
 * Special Helpers for Various Settings
 */

/** \name Animation Editor Utils
 * \{ */

/* This function returns the snapping 'mode' for Animation Editors only
 * We cannot use the standard snapping due to NLA-strip scaling complexities.
 */
// XXX these modifier checks should be keymappable
static short getAnimEdit_SnapMode(TransInfo *t)
{
  short autosnap = SACTSNAP_OFF;

  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;

    if (saction) {
      autosnap = saction->autosnap;
    }
  }
  else if (t->spacetype == SPACE_GRAPH) {
    SpaceGraph *sipo = (SpaceGraph *)t->sa->spacedata.first;

    if (sipo) {
      autosnap = sipo->autosnap;
    }
  }
  else if (t->spacetype == SPACE_NLA) {
    SpaceNla *snla = (SpaceNla *)t->sa->spacedata.first;

    if (snla) {
      autosnap = snla->autosnap;
    }
  }
  else {
    autosnap = SACTSNAP_OFF;
  }

  /* toggle autosnap on/off
   * - when toggling on, prefer nearest frame over 1.0 frame increments
   */
  if (t->modifiers & MOD_SNAP_INVERT) {
    if (autosnap) {
      autosnap = SACTSNAP_OFF;
    }
    else {
      autosnap = SACTSNAP_FRAME;
    }
  }

  return autosnap;
}

/* This function is used by Animation Editor specific transform functions to do
 * the Snap Keyframe to Nearest Frame/Marker
 */
static void doAnimEdit_SnapFrame(
    TransInfo *t, TransData *td, TransData2D *td2d, AnimData *adt, short autosnap)
{
  /* snap key to nearest frame or second? */
  if (ELEM(autosnap, SACTSNAP_FRAME, SACTSNAP_SECOND)) {
    const Scene *scene = t->scene;
    const double secf = FPS;
    double val;

    /* convert frame to nla-action time (if needed) */
    if (adt) {
      val = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
    }
    else {
      val = *(td->val);
    }

    /* do the snapping to nearest frame/second */
    if (autosnap == SACTSNAP_FRAME) {
      val = floorf(val + 0.5);
    }
    else if (autosnap == SACTSNAP_SECOND) {
      val = (float)(floor((val / secf) + 0.5) * secf);
    }

    /* convert frame out of nla-action time */
    if (adt) {
      *(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
    }
    else {
      *(td->val) = val;
    }
  }
  /* snap key to nearest marker? */
  else if (autosnap == SACTSNAP_MARKER) {
    float val;

    /* convert frame to nla-action time (if needed) */
    if (adt) {
      val = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_MAP);
    }
    else {
      val = *(td->val);
    }

    /* snap to nearest marker */
    // TODO: need some more careful checks for where data comes from
    val = (float)ED_markers_find_nearest_marker_time(&t->scene->markers, val);

    /* convert frame out of nla-action time */
    if (adt) {
      *(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
    }
    else {
      *(td->val) = val;
    }
  }

  /* If the handles are to be moved too
   * (as side-effect of keyframes moving, to keep the general effect)
   * offset them by the same amount so that the general angles are maintained
   * (i.e. won't change while handles are free-to-roam and keyframes are snap-locked).
   */
  if ((td->flag & TD_MOVEHANDLE1) && td2d->h1) {
    td2d->h1[0] = td2d->ih1[0] + *td->val - td->ival;
  }
  if ((td->flag & TD_MOVEHANDLE2) && td2d->h2) {
    td2d->h2[0] = td2d->ih2[0] + *td->val - td->ival;
  }
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Animation Translation) */

/** \name Transform Animation Translation
 * \{ */

static void initTimeTranslate(TransInfo *t)
{
  /* this tool is only really available in the Action Editor... */
  if (!ELEM(t->spacetype, SPACE_ACTION, SPACE_SEQ)) {
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_TIME_TRANSLATE;
  t->transform = applyTimeTranslate;

  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  /* num-input has max of (n-1) */
  t->idx_max = 0;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  /* initialize snap like for everything else */
  t->snap[0] = 0.0f;
  t->snap[1] = t->snap[2] = 1.0f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  /* No time unit supporting frames currently... */
  t->num.unit_type[0] = B_UNIT_NONE;
}

static void headerTimeTranslate(TransInfo *t, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];
  int ofs = 0;

  /* if numeric input is active, use results from that, otherwise apply snapping to result */
  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    const Scene *scene = t->scene;
    const short autosnap = getAnimEdit_SnapMode(t);
    const double secf = FPS;
    float val = t->values[0];

    /* apply snapping + frame->seconds conversions */
    if (autosnap == SACTSNAP_STEP) {
      /* frame step */
      val = floorf(val + 0.5f);
    }
    else if (autosnap == SACTSNAP_TSTEP) {
      /* second step */
      val = floorf((double)val / secf + 0.5);
    }
    else if (autosnap == SACTSNAP_SECOND) {
      /* nearest second */
      val = (float)((double)val / secf);
    }

    if (autosnap == SACTSNAP_FRAME) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%d.00 (%.4f)", (int)val, val);
    }
    else if (autosnap == SACTSNAP_SECOND) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%d.00 sec (%.4f)", (int)val, val);
    }
    else if (autosnap == SACTSNAP_TSTEP) {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f sec", val);
    }
    else {
      BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", val);
    }
  }

  ofs += BLI_snprintf(str, UI_MAX_DRAW_STR, TIP_("DeltaX: %s"), &tvec[0]);

  if (t->flag & T_PROP_EDIT_ALL) {
    ofs += BLI_snprintf(
        str + ofs, UI_MAX_DRAW_STR - ofs, TIP_(" Proportional size: %.2f"), t->prop_size);
  }
}

static void applyTimeTranslateValue(TransInfo *t)
{
  Scene *scene = t->scene;
  int i;

  const short autosnap = getAnimEdit_SnapMode(t);
  const double secf = FPS;

  float deltax, val /* , valprev */;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    TransData2D *td2d = tc->data_2d;
    /* It doesn't matter whether we apply to t->data or
     * t->data2d, but t->data2d is more convenient. */
    for (i = 0; i < tc->data_len; i++, td++, td2d++) {
      /* it is assumed that td->extra is a pointer to the AnimData,
       * whose active action is where this keyframe comes from
       * (this is only valid when not in NLA)
       */
      AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;

      /* valprev = *td->val; */ /* UNUSED */

      /* check if any need to apply nla-mapping */
      if (adt && (t->spacetype != SPACE_SEQ)) {
        deltax = t->values[0];

        if (autosnap == SACTSNAP_TSTEP) {
          deltax = (float)(floor(((double)deltax / secf) + 0.5) * secf);
        }
        else if (autosnap == SACTSNAP_STEP) {
          deltax = floorf(deltax + 0.5f);
        }

        val = BKE_nla_tweakedit_remap(adt, td->ival, NLATIME_CONVERT_MAP);
        val += deltax * td->factor;
        *(td->val) = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_UNMAP);
      }
      else {
        deltax = val = t->values[0];

        if (autosnap == SACTSNAP_TSTEP) {
          val = (float)(floor(((double)deltax / secf) + 0.5) * secf);
        }
        else if (autosnap == SACTSNAP_STEP) {
          val = floorf(val + 0.5f);
        }

        *(td->val) = td->ival + val;
      }

      /* apply nearest snapping */
      doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
    }
  }
}

static void applyTimeTranslate(TransInfo *t, const int mval[2])
{
  View2D *v2d = (View2D *)t->view;
  char str[UI_MAX_DRAW_STR];

  /* calculate translation amount from mouse movement - in 'time-grid space' */
  if (t->flag & T_MODAL) {
    float cval[2], sval[2];
    UI_view2d_region_to_view(v2d, mval[0], mval[0], &cval[0], &cval[1]);
    UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[0], &sval[0], &sval[1]);

    /* we only need to calculate effect for time (applyTimeTranslate only needs that) */
    t->values[0] = cval[0] - sval[0];
  }

  /* handle numeric-input stuff */
  t->vec[0] = t->values[0];
  applyNumInput(&t->num, &t->vec[0]);
  t->values[0] = t->vec[0];
  headerTimeTranslate(t, str);

  applyTimeTranslateValue(t);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Animation Time Slide) */

/** \name Transform Animation Time Slide
 * \{ */

static void initTimeSlide(TransInfo *t)
{
  /* this tool is only really available in the Action Editor... */
  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;

    /* set flag for drawing stuff */
    saction->flag |= SACTION_MOVING;
  }
  else {
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_TIME_SLIDE;
  t->transform = applyTimeSlide;

  initMouseInputMode(t, &t->mouse, INPUT_NONE);

  {
    Scene *scene = t->scene;
    float *range;
    t->custom.mode.data = range = MEM_mallocN(sizeof(float[2]), "TimeSlide Min/Max");
    t->custom.mode.use_free = true;

    float min = 999999999.0f, max = -999999999.0f;
    int i;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      TransData *td = tc->data;
      for (i = 0; i < tc->data_len; i++, td++) {
        AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;
        float val = *(td->val);

        /* strip/action time to global (mapped) time */
        if (adt) {
          val = BKE_nla_tweakedit_remap(adt, val, NLATIME_CONVERT_MAP);
        }

        if (min > val) {
          min = val;
        }
        if (max < val) {
          max = val;
        }
      }
    }

    if (min == max) {
      /* just use the current frame ranges */
      min = (float)PSFRA;
      max = (float)PEFRA;
    }

    range[0] = min;
    range[1] = max;
  }

  /* num-input has max of (n-1) */
  t->idx_max = 0;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  /* initialize snap like for everything else */
  t->snap[0] = 0.0f;
  t->snap[1] = t->snap[2] = 1.0f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  /* No time unit supporting frames currently... */
  t->num.unit_type[0] = B_UNIT_NONE;
}

static void headerTimeSlide(TransInfo *t, const float sval, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    const float *range = t->custom.mode.data;
    float minx = range[0];
    float maxx = range[1];
    float cval = t->values[0];
    float val;

    val = 2.0f * (cval - sval) / (maxx - minx);
    CLAMP(val, -1.0f, 1.0f);

    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", val);
  }

  BLI_snprintf(str, UI_MAX_DRAW_STR, TIP_("TimeSlide: %s"), &tvec[0]);
}

static void applyTimeSlideValue(TransInfo *t, float sval)
{
  int i;
  const float *range = t->custom.mode.data;
  float minx = range[0];
  float maxx = range[1];

  /* set value for drawing black line */
  if (t->spacetype == SPACE_ACTION) {
    SpaceAction *saction = (SpaceAction *)t->sa->spacedata.first;
    float cvalf = t->values[0];

    saction->timeslide = cvalf;
  }

  /* It doesn't matter whether we apply to t->data or
   * t->data2d, but t->data2d is more convenient. */
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    for (i = 0; i < tc->data_len; i++, td++) {
      /* it is assumed that td->extra is a pointer to the AnimData,
       * whose active action is where this keyframe comes from
       * (this is only valid when not in NLA)
       */
      AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;
      float cval = t->values[0];

      /* only apply to data if in range */
      if ((sval > minx) && (sval < maxx)) {
        float cvalc = CLAMPIS(cval, minx, maxx);
        float ival = td->ival;
        float timefac;

        /* NLA mapping magic here works as follows:
         * - "ival" goes from strip time to global time
         * - calculation is performed into td->val in global time
         *   (since sval and min/max are all in global time)
         * - "td->val" then gets put back into strip time
         */
        if (adt) {
          /* strip to global */
          ival = BKE_nla_tweakedit_remap(adt, ival, NLATIME_CONVERT_MAP);
        }

        /* left half? */
        if (ival < sval) {
          timefac = (sval - ival) / (sval - minx);
          *(td->val) = cvalc - timefac * (cvalc - minx);
        }
        else {
          timefac = (ival - sval) / (maxx - sval);
          *(td->val) = cvalc + timefac * (maxx - cvalc);
        }

        if (adt) {
          /* global to strip */
          *(td->val) = BKE_nla_tweakedit_remap(adt, *(td->val), NLATIME_CONVERT_UNMAP);
        }
      }
    }
  }
}

static void applyTimeSlide(TransInfo *t, const int mval[2])
{
  View2D *v2d = (View2D *)t->view;
  float cval[2], sval[2];
  const float *range = t->custom.mode.data;
  float minx = range[0];
  float maxx = range[1];
  char str[UI_MAX_DRAW_STR];

  /* calculate mouse co-ordinates */
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &cval[0], &cval[1]);
  UI_view2d_region_to_view(v2d, t->mouse.imval[0], t->mouse.imval[1], &sval[0], &sval[1]);

  /* t->values[0] stores cval[0], which is the current mouse-pointer location (in frames) */
  // XXX Need to be able to repeat this
  /* t->values[0] = cval[0]; */ /* UNUSED (reset again later). */

  /* handle numeric-input stuff */
  t->vec[0] = 2.0f * (cval[0] - sval[0]) / (maxx - minx);
  applyNumInput(&t->num, &t->vec[0]);
  t->values[0] = (maxx - minx) * t->vec[0] / 2.0f + sval[0];

  headerTimeSlide(t, sval[0], str);
  applyTimeSlideValue(t, sval[0]);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* -------------------------------------------------------------------- */
/* Transform (Animation Time Scale) */

/** \name Transform Animation Time Scale
 * \{ */

static void initTimeScale(TransInfo *t)
{
  float center[2];

  /* this tool is only really available in the Action Editor
   * AND NLA Editor (for strip scaling)
   */
  if (ELEM(t->spacetype, SPACE_ACTION, SPACE_NLA) == 0) {
    t->state = TRANS_CANCEL;
  }

  t->mode = TFM_TIME_SCALE;
  t->transform = applyTimeScale;

  /* recalculate center2d to use CFRA and mouse Y, since that's
   * what is used in time scale */
  if ((t->flag & T_OVERRIDE_CENTER) == 0) {
    t->center_global[0] = t->scene->r.cfra;
    projectFloatView(t, t->center_global, center);
    center[1] = t->mouse.imval[1];
  }

  /* force a reinit with the center2d used here */
  initMouseInput(t, &t->mouse, center, t->mouse.imval, false);

  initMouseInputMode(t, &t->mouse, INPUT_SPRING_FLIP);

  t->flag |= T_NULL_ONE;
  t->num.val_flag[0] |= NUM_NULL_ONE;

  /* num-input has max of (n-1) */
  t->idx_max = 0;
  t->num.flag = 0;
  t->num.idx_max = t->idx_max;

  /* initialize snap like for everything else */
  t->snap[0] = 0.0f;
  t->snap[1] = t->snap[2] = 1.0f;

  copy_v3_fl(t->num.val_inc, t->snap[1]);
  t->num.unit_sys = t->scene->unit.system;
  t->num.unit_type[0] = B_UNIT_NONE;
}

static void headerTimeScale(TransInfo *t, char str[UI_MAX_DRAW_STR])
{
  char tvec[NUM_STR_REP_LEN * 3];

  if (hasNumInput(&t->num)) {
    outputNumInput(&(t->num), tvec, &t->scene->unit);
  }
  else {
    BLI_snprintf(&tvec[0], NUM_STR_REP_LEN, "%.4f", t->values[0]);
  }

  BLI_snprintf(str, UI_MAX_DRAW_STR, TIP_("ScaleX: %s"), &tvec[0]);
}

static void applyTimeScaleValue(TransInfo *t)
{
  Scene *scene = t->scene;
  int i;

  const short autosnap = getAnimEdit_SnapMode(t);
  const double secf = FPS;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td = tc->data;
    TransData2D *td2d = tc->data_2d;
    for (i = 0; i < tc->data_len; i++, td++, td2d++) {
      /* it is assumed that td->extra is a pointer to the AnimData,
       * whose active action is where this keyframe comes from
       * (this is only valid when not in NLA)
       */
      AnimData *adt = (t->spacetype != SPACE_NLA) ? td->extra : NULL;
      float startx = CFRA;
      float fac = t->values[0];

      if (autosnap == SACTSNAP_TSTEP) {
        fac = (float)(floor((double)fac / secf + 0.5) * secf);
      }
      else if (autosnap == SACTSNAP_STEP) {
        fac = floorf(fac + 0.5f);
      }

      /* take proportional editing into account */
      fac = ((fac - 1.0f) * td->factor) + 1;

      /* check if any need to apply nla-mapping */
      if (adt) {
        startx = BKE_nla_tweakedit_remap(adt, startx, NLATIME_CONVERT_UNMAP);
      }

      /* now, calculate the new value */
      *(td->val) = ((td->ival - startx) * fac) + startx;

      /* apply nearest snapping */
      doAnimEdit_SnapFrame(t, td, td2d, adt, autosnap);
    }
  }
}

static void applyTimeScale(TransInfo *t, const int UNUSED(mval[2]))
{
  char str[UI_MAX_DRAW_STR];

  /* handle numeric-input stuff */
  t->vec[0] = t->values[0];
  applyNumInput(&t->num, &t->vec[0]);
  t->values[0] = t->vec[0];
  headerTimeScale(t, str);

  applyTimeScaleValue(t);

  recalcData(t);

  ED_area_status_text(t->sa, str);
}
/** \} */

/* TODO, move to: transform_query.c */
bool checkUseAxisMatrix(TransInfo *t)
{
  /* currently only checks for editmode */
  if (t->flag & T_EDIT) {
    if ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
        (ELEM(t->obedit_type, OB_MESH, OB_CURVE, OB_MBALL, OB_ARMATURE))) {
      /* not all editmode supports axis-matrix */
      return true;
    }
  }

  return false;
}
