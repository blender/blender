/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_layer.hh"
#include "BKE_mask.h"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "GPU_state.hh"

#include "ED_clip.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_image.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_uvedit.hh"

#include "ANIM_keyframing.hh"

#include "SEQ_transform.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "RNA_access.hh"

#include "BLF_api.hh"
#include "BLT_translation.hh"

#include "transform.hh"
#include "transform_constraints.hh"
#include "transform_convert.hh"
#include "transform_draw_cursors.hh"
#include "transform_gizmo.hh"
#include "transform_mode.hh"
#include "transform_orientations.hh"
#include "transform_snap.hh"

/* Disabling, since when you type you know what you are doing,
 * and being able to set it to zero is handy. */
// #define USE_NUM_NO_ZERO.

namespace blender::ed::transform {

/* -------------------------------------------------------------------- */
/** \name General Utils
 * \{ */

void transform_view_vector_calc(const TransInfo *t, const float focus[3], float r_vec[3])
{
  if (t->persp != RV3D_ORTHO) {
    sub_v3_v3v3(r_vec, t->viewinv[3], focus);
  }
  else {
    copy_v3_v3(r_vec, t->viewinv[2]);
  }
  normalize_v3(r_vec);
}

bool transdata_check_local_islands(TransInfo *t, short around)
{
  if (t->options & (CTX_CURSOR | CTX_TEXTURE_SPACE)) {
    return false;
  }
  return ((around == V3D_AROUND_LOCAL_ORIGINS) &&
          ELEM(t->obedit_type, OB_MESH, OB_GREASE_PENCIL, OB_CURVES));
}

/** \} */

/* ************************** SPACE DEPENDENT CODE **************************** */

void setTransformViewMatrices(TransInfo *t)
{
  if (!(t->options & CTX_PAINT_CURVE) && (t->spacetype == SPACE_VIEW3D) && t->region &&
      (t->region->regiontype == RGN_TYPE_WINDOW))
  {
    RegionView3D *rv3d = static_cast<RegionView3D *>(t->region->regiondata);

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
}

void setTransformViewAspect(TransInfo *t, float r_aspect[3])
{
  copy_v3_fl(r_aspect, 1.0f);

  if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);

    if (t->options & CTX_MASK) {
      ED_space_image_get_aspect(sima, &r_aspect[0], &r_aspect[1]);
    }
    else if (t->options & CTX_PAINT_CURVE) {
      /* Pass. */
    }
    else {
      ED_space_image_get_uv_aspect(sima, &r_aspect[0], &r_aspect[1]);
    }
  }
  else if (t->spacetype == SPACE_SEQ) {
    if (t->options & CTX_CURSOR) {
      Scene *scene = CTX_data_sequencer_scene(t->context);
      const float2 aspect = seq::image_preview_unit_to_px(scene, r_aspect);
      copy_v2_v2(r_aspect, aspect);
    }
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sclip = static_cast<SpaceClip *>(t->area->spacedata.first);

    if (t->options & CTX_MOVIECLIP) {
      ED_space_clip_get_aspect_dimension_aware(sclip, &r_aspect[0], &r_aspect[1]);
    }
    else {
      ED_space_clip_get_aspect(sclip, &r_aspect[0], &r_aspect[1]);
    }
  }
  else if (t->spacetype == SPACE_GRAPH) {
    /* Depends on context of usage. */
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

  /* Difference with #convertViewVec2D. */
  /* Clamp w/h, mask only. */
  if (mulx / divx < muly / divy) {
    divy = divx;
    muly = mulx;
  }
  else {
    divx = divy;
    mulx = muly;
  }
  /* End difference. */

  r_vec[0] = mulx * dx / divx;
  r_vec[1] = muly * dy / divy;
  r_vec[2] = 0.0f;
}

void convertViewVec(TransInfo *t, float r_vec[3], double dx, double dy)
{
  if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    if (t->options & CTX_PAINT_CURVE) {
      r_vec[0] = dx;
      r_vec[1] = dy;
    }
    else {
      const float xy_delta[2] = {float(dx), float(dy)};
      ED_view3d_win_to_delta(t->region, xy_delta, t->zfac, r_vec);
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      convertViewVec2D_mask(static_cast<View2D *>(t->view), r_vec, dx, dy);
    }
    else if (t->options & CTX_PAINT_CURVE) {
      r_vec[0] = dx;
      r_vec[1] = dy;
    }
    else {
      convertViewVec2D(static_cast<View2D *>(t->view), r_vec, dx, dy);
    }

    r_vec[0] *= t->aspect[0];
    r_vec[1] *= t->aspect[1];
  }
  else if (ELEM(t->spacetype, SPACE_GRAPH, SPACE_NLA)) {
    convertViewVec2D(static_cast<View2D *>(t->view), r_vec, dx, dy);
  }
  else if (ELEM(t->spacetype, SPACE_NODE, SPACE_SEQ)) {
    convertViewVec2D(&t->region->v2d, r_vec, dx, dy);
  }
  else if (t->spacetype == SPACE_CLIP) {
    if (t->options & CTX_MASK) {
      convertViewVec2D_mask(static_cast<View2D *>(t->view), r_vec, dx, dy);
    }
    else {
      convertViewVec2D(static_cast<View2D *>(t->view), r_vec, dx, dy);
    }

    r_vec[0] *= t->aspect[0];
    r_vec[1] *= t->aspect[1];
  }
  else {
    printf("%s: called in an invalid context\n", __func__);
    zero_v3(r_vec);
  }
}

void projectFloatViewCenterFallback(TransInfo *t, float adr[2])
{
  const ARegion *region = t->region;

  if (UNLIKELY(region == nullptr)) {
    /* While this function probably wont be calved without a region.
     * Doing so shouldn't cause errors. */
    adr[0] = 0.0f;
    adr[1] = 0.0f;
    return;
  }

  bool changed = false;
  switch (t->spacetype) {
    case SPACE_VIEW3D: {
      if (region->regiontype == RGN_TYPE_WINDOW) {
        /* NOTE(@ideasman42): When picking a fallback there isn't a "correct" location.
         * By default the region center is the fallback, use this unless there is a reason not to.
         *
         * One exception is when transforming the camera from the camera viewpoint.
         * In this case it's logical to use the camera frames center, see: #141663. */
        if (t->options & CTX_CAMERA) {
          const View3D *v3d = static_cast<View3D *>(t->view);
          const RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          if (rv3d->persp == RV3D_CAMOB && v3d->camera) {
            /* Exclude any camera "shift" because the un-shifted point is the pivot.
             *
             * This may not be the case when transforming a cameras parent however
             * in these situations it's not practical to find a screen space location
             * for a 3D point that couldn't be projected. */
            const bool no_shift = true;
            rctf viewborder = {0};
            ED_view3d_calc_camera_border(
                t->scene, t->depsgraph, region, v3d, rv3d, no_shift, &viewborder);
            adr[0] = BLI_rctf_cent_x(&viewborder);
            adr[1] = BLI_rctf_cent_y(&viewborder);
            changed = true;
          }
        }
      }
      break;
    }
  }

  if (changed == false) {
    adr[0] = region->winx / 2.0f;
    adr[1] = region->winy / 2.0f;
  }
}

void projectIntViewEx(TransInfo *t, const float vec[3], int adr[2], const eV3DProjTest flag)
{
  if (t->spacetype == SPACE_VIEW3D) {
    if (t->region->regiontype == RGN_TYPE_WINDOW) {
      if (ED_view3d_project_int_global(t->region, vec, adr, flag) != V3D_PROJ_RET_OK) {
        /* This is what was done in 2.64, perhaps we can be smarter? */
        adr[0] = int(2140000000.0f);
        adr[1] = int(2140000000.0f);
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);

    if (t->options & CTX_MASK) {
      float v[2];

      v[0] = vec[0] / t->aspect[0];
      v[1] = vec[1] / t->aspect[1];

      BKE_mask_coord_to_image(sima->image, &sima->iuser, v, v);

      ED_image_point_pos__reverse(sima, t->region, v, v);

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

      UI_view2d_view_to_region(static_cast<const View2D *>(t->view), v[0], v[1], &adr[0], &adr[1]);
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    int out[2] = {0, 0};
#if 0
    SpaceAction *sact = t->area->spacedata.first;

    if (sact->flag & SACTION_DRAWTIME) {
      // vec[0] = vec[0] / ((t->scene->r.frs_sec / t->scene->r.frs_sec_base));
      /* Same as below. */
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
  else if (t->spacetype == SPACE_SEQ) { /* XXX not tested yet, but should work. */
    int out[2] = {0, 0};

    UI_view2d_view_to_region((View2D *)t->view, vec[0], vec[1], &out[0], &out[1]);
    adr[0] = out[0];
    adr[1] = out[1];
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = static_cast<SpaceClip *>(t->area->spacedata.first);

    if (t->options & CTX_MASK) {
      MovieClip *clip = ED_space_clip_get_clip(sc);

      if (clip) {
        float v[2];

        v[0] = vec[0] / t->aspect[0];
        v[1] = vec[1] / t->aspect[1];

        BKE_mask_coord_to_movieclip(sc->clip, &sc->user, v, v);

        ED_clip_point_stable_pos__reverse(sc, t->region, v, v);

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

      UI_view2d_view_to_region(static_cast<const View2D *>(t->view), v[0], v[1], &adr[0], &adr[1]);
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
      else if (t->region->regiontype == RGN_TYPE_WINDOW) {
        /* Allow points behind the view #33643. */
        if (ED_view3d_project_float_global(t->region, vec, adr, flag) != V3D_PROJ_RET_OK) {
          /* XXX, 2.64 and prior did this, weak! */
          projectFloatViewCenterFallback(t, adr);
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
      !(t->options & CTX_PAINT_CURVE))
  {
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);

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
    SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);

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
    if (t->obedit_type == OB_GREASE_PENCIL) {
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
    }
  }
  else if (t->spacetype == SPACE_VIEW3D) {
    if (t->options & CTX_PAINT_CURVE) {
      wmWindow *window = CTX_wm_window(C);
      WM_paint_cursor_tag_redraw(window, t->region);
    }
    else {
      /* Do we need more refined tags? */
      if (t->options & CTX_POSE_BONE) {
        WM_event_add_notifier(C, NC_OBJECT | ND_POSE, nullptr);
      }
      else {
        WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
      }

      /* For real-time animation record - send notifiers recognized by animation editors. */
      /* XXX: is this notifier a lame duck? */
      if ((t->animtimer) && animrig::is_autokey_on(t->scene)) {
        WM_event_add_notifier(C, NC_OBJECT | ND_KEYS, nullptr);
      }
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    // SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    // SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
  }
  else if (t->spacetype == SPACE_NLA) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, nullptr);
  }
  else if (t->spacetype == SPACE_NODE) {
    // ED_area_tag_redraw(t->area);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);
  }
  else if (t->spacetype == SPACE_SEQ) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, nullptr);
    /* Key-frames on strips has been moved, so make sure related editors are informed. */
    WM_event_add_notifier(C, NC_ANIMATION, nullptr);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    if (t->options & CTX_MASK) {
      Mask *mask = CTX_data_edit_mask(C);

      WM_event_add_notifier(C, NC_MASK | NA_EDITED, mask);
    }
    else if (t->options & CTX_PAINT_CURVE) {
      wmWindow *window = CTX_wm_window(C);
      WM_paint_cursor_tag_redraw(window, t->region);
    }
    else if (t->options & CTX_CURSOR) {
      ED_area_tag_redraw(t->area);
    }
    else {
      /* XXX how to deal with lock? */
      SpaceImage *sima = (SpaceImage *)t->area->spacedata.first;
      if (sima->lock) {
        BKE_view_layer_synced_ensure(t->scene, t->view_layer);
        WM_event_add_notifier(
            C, NC_GEOM | ND_DATA, BKE_view_layer_edit_object_get(t->view_layer)->data);
      }
      else {
        ED_area_tag_redraw(t->area);
      }
    }
  }
  else if (t->spacetype == SPACE_CLIP) {
    SpaceClip *sc = (SpaceClip *)t->area->spacedata.first;

    if (ED_space_clip_check_show_trackedit(sc)) {
      MovieClip *clip = ED_space_clip_get_clip(sc);

      /* Objects could be parented to tracking data, so send this for viewport refresh. */
      WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

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
  ED_area_status_text(t->area, nullptr);
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace) {
    BKE_workspace_status_clear(workspace);
  }

  if (t->spacetype == SPACE_VIEW3D) {
    /* If auto-keying is enabled, send notifiers that keyframes were added. */
    if (animrig::is_autokey_on(t->scene)) {
      WM_main_add_notifier(NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);
    }

    /* Redraw UV editor. */
    const char uvcalc_correct_flag = ELEM(t->mode, TFM_VERT_SLIDE, TFM_EDGE_SLIDE) ?
                                         UVCALC_TRANSFORM_CORRECT_SLIDE :
                                         UVCALC_TRANSFORM_CORRECT;

    if ((t->data_type == &TransConvertType_Mesh) &&
        (t->settings->uvcalc_flag & uvcalc_correct_flag))
    {
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
    }
  }
}

/* ************************************************* */

static bool transform_modal_item_poll(const wmOperator *op, int value)
{
  const TransInfo *t = static_cast<const TransInfo *>(op->customdata);
  if (t->modifiers & MOD_EDIT_SNAP_SOURCE) {
    if (value == TFM_MODAL_EDIT_SNAP_SOURCE_OFF) {
      return true;
    }
    if (!ELEM(
            value, TFM_MODAL_CANCEL, TFM_MODAL_CONFIRM, TFM_MODAL_ADD_SNAP, TFM_MODAL_REMOVE_SNAP))
    {
      return false;
    }
  }

  switch (value) {
    case TFM_MODAL_CANCEL: {
      /* TODO: Canceling with LMB is not possible when the operator is activated
       * through tweak and the LMB is pressed.
       * Therefore, this item should not appear in the status bar. */
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
      if (value == TFM_MODAL_ADD_SNAP) {
        if (!(t->tsnap.status & SNAP_TARGET_FOUND)) {
          return false;
        }
      }
      else {
        if (!t->tsnap.selectedPoint) {
          return false;
        }
      }
      break;
    }
    case TFM_MODAL_AXIS_Z:
    case TFM_MODAL_PLANE_X:
    case TFM_MODAL_PLANE_Y:
    case TFM_MODAL_PLANE_Z:
    case TFM_MODAL_AUTOCONSTRAINTPLANE:
      if (t->flag & T_2D_EDIT) {
        return false;
      }
      [[fallthrough]];
    case TFM_MODAL_AXIS_X:
    case TFM_MODAL_AXIS_Y:
    case TFM_MODAL_AUTOCONSTRAINT:
      if (t->flag & T_NO_CONSTRAINT) {
        return false;
      }
      break;
    case TFM_MODAL_CONS_OFF: {
      if ((t->con.mode & CON_APPLY) == 0) {
        return false;
      }
      break;
    }
    case TFM_MODAL_INSERTOFS_TOGGLE_DIR:
    case TFM_MODAL_NODE_ATTACH_ON:
    case TFM_MODAL_NODE_ATTACH_OFF:
    case TFM_MODAL_NODE_FRAME: {
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
    case TFM_MODAL_TRANSLATE:
    case TFM_MODAL_ROTATE:
    case TFM_MODAL_RESIZE:
    case TFM_MODAL_VERT_EDGE_SLIDE:
    case TFM_MODAL_TRACKBALL:
    case TFM_MODAL_ROTATE_NORMALS: {
      if (!transform_mode_is_changeable(t->mode)) {
        return false;
      }
      if (value == TFM_MODAL_TRANSLATE && t->mode == TFM_TRANSLATION) {
        /* The tracking transform in MovieClip has an alternate translate that modifies the offset
         * of the tracks. */
        return t->data_type == &TransConvertType_Tracking;
      }
      if (value == TFM_MODAL_ROTATE && t->mode == TFM_ROTATION) {
        return false;
      }
      if (value == TFM_MODAL_RESIZE && t->mode == TFM_RESIZE) {
        /* The tracking transform in MovieClip has an alternate resize that only affects the
         * tracker size and not the search area. */
        return t->data_type == &TransConvertType_Tracking;
      }
      if (value == TFM_MODAL_VERT_EDGE_SLIDE &&
          (!ELEM(t->data_type, &TransConvertType_Mesh, &TransConvertType_MeshUV) ||
           /* WORKAROUND: Avoid repeated keys in status bar.
            *
            * Previously, `Vert/Edge Slide` and `Move` were triggered by the same modal key.
            * But now, to fix #100129 (Status bar incorrectly shows "[G] Move"), `Vert/Edge Slide`
            * has its own modal key. However by default it uses the same key as `Move` (G). So, to
            * avoid displaying the same key twice (G and G), only display this modal key during the
            * `Move` operation.
            *
            * Ideally we should check if it really uses the same key. */
           t->mode != TFM_TRANSLATION))
      {
        return false;
      }
      if (value == TFM_MODAL_TRACKBALL &&
          /* WORKAROUND: Avoid repeated keys in status bar.
           *
           * Previously, `Trackball` and `Rotate` were triggered by the same modal key.
           * But to fix the status bar incorrectly showing "[R] Rotate", `Trackball` has now its
           * own modal key. However by default it uses the same key as `Rotate` (R). So, to avoid
           * displaying the same key twice (R and R), only display this modal key during the
           * `Rotate` operation.
           *
           * Ideally we should check if it really uses the same key. */
          t->mode != TFM_ROTATION)
      {
        return false;
      }
      if (value == TFM_MODAL_ROTATE_NORMALS) {
        return t->mode == TFM_ROTATION && t->data_type == &TransConvertType_Mesh;
      }
      break;
    }
    case TFM_MODAL_EDIT_SNAP_SOURCE_OFF:
      return false;
    case TFM_MODAL_EDIT_SNAP_SOURCE_ON: {
      if (!ELEM(t->spacetype, SPACE_VIEW3D, SPACE_IMAGE)) {
        return false;
      }
      if (!ELEM(
              t->mode, TFM_TRANSLATION, TFM_ROTATION, TFM_RESIZE, TFM_EDGE_SLIDE, TFM_VERT_SLIDE))
      {
        /* More modes can be added over time if this feature proves useful for them. */
        return false;
      }
      if (t->options & CTX_CAMERA) {
        /* Not supported. */
        return false;
      }
      break;
    }
    case TFM_MODAL_PASSTHROUGH_NAVIGATE:
      if (ELEM(t->mode, TFM_EDGE_SLIDE, TFM_VERT_SLIDE, TFM_SHRINKFATTEN)) {
        /* Returning `false` will not prevent the navigation from working, it will just not display
         * the shortcut in the header.
         * Return `false` here to prevent this modal item from affecting the state with
         * #T_ALT_TRANSFORM is used by the operator. */
        return false;
      }
      return t->vod != nullptr;
    case TFM_MODAL_STRIP_CLAMP:
      if (t->spacetype != SPACE_SEQ) {
        return false;
      }
      break;
  }
  return true;
}

wmKeyMap *transform_modal_keymap(wmKeyConfig *keyconf)
{
  static const EnumPropertyItem modal_items[] = {
      {TFM_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
      {TFM_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
      {TFM_MODAL_AXIS_X, "AXIS_X", 0, "X Axis", ""},
      {TFM_MODAL_AXIS_Y, "AXIS_Y", 0, "Y Axis", ""},
      {TFM_MODAL_AXIS_Z, "AXIS_Z", 0, "Z Axis", ""},
      {TFM_MODAL_PLANE_X, "PLANE_X", 0, "X Plane", ""},
      {TFM_MODAL_PLANE_Y, "PLANE_Y", 0, "Y Plane", ""},
      {TFM_MODAL_PLANE_Z, "PLANE_Z", 0, "Z Plane", ""},
      {TFM_MODAL_CONS_OFF, "CONS_OFF", 0, "Clear Constraints", ""},
      {TFM_MODAL_EDIT_SNAP_SOURCE_ON, "EDIT_SNAP_SOURCE_ON", 0, "Set Snap Base", ""},
      {TFM_MODAL_EDIT_SNAP_SOURCE_OFF, "EDIT_SNAP_SOURCE_OFF", 0, "Set Snap Base (Off)", ""},
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
      {TFM_MODAL_PROPSIZE, "PROPORTIONAL_SIZE", 0, "Adjust Proportional Influence", ""},
      {TFM_MODAL_AUTOIK_LEN_INC, "AUTOIK_CHAIN_LEN_UP", 0, "Increase Max AutoIK Chain Length", ""},
      {TFM_MODAL_AUTOIK_LEN_DEC,
       "AUTOIK_CHAIN_LEN_DOWN",
       0,
       "Decrease Max AutoIK Chain Length",
       ""},
      {TFM_MODAL_INSERTOFS_TOGGLE_DIR,
       "INSERTOFS_TOGGLE_DIR",
       0,
       "Toggle Direction for Node Auto-Offset",
       ""},
      {TFM_MODAL_NODE_ATTACH_ON, "NODE_ATTACH_ON", 0, "Node Attachment", ""},
      {TFM_MODAL_NODE_ATTACH_OFF, "NODE_ATTACH_OFF", 0, "Node Attachment (Off)", ""},
      {TFM_MODAL_TRANSLATE, "TRANSLATE", 0, "Move", ""},
      {TFM_MODAL_VERT_EDGE_SLIDE, "VERT_EDGE_SLIDE", 0, "Vert/Edge Slide", ""},
      {TFM_MODAL_ROTATE, "ROTATE", 0, "Rotate", ""},
      {TFM_MODAL_TRACKBALL, "TRACKBALL", 0, "Trackball", ""},
      {TFM_MODAL_RESIZE, "RESIZE", 0, "Resize", ""},
      {TFM_MODAL_ROTATE_NORMALS, "ROTATE_NORMALS", 0, "Rotate Normals", ""},
      {TFM_MODAL_AUTOCONSTRAINT, "AUTOCONSTRAIN", 0, "Automatic Constraint", ""},
      {TFM_MODAL_AUTOCONSTRAINTPLANE, "AUTOCONSTRAINPLANE", 0, "Automatic Constraint Plane", ""},
      {TFM_MODAL_PRECISION, "PRECISION", 0, "Precision Mode", ""},
      {TFM_MODAL_PASSTHROUGH_NAVIGATE, "PASSTHROUGH_NAVIGATE", 0, "Navigate", ""},
      {TFM_MODAL_NODE_FRAME, "NODE_FRAME", 0, "Attach/Detach Frame", ""},
      {TFM_MODAL_STRIP_CLAMP, "STRIP_CLAMP_TOGGLE", 0, "Clamp Strips", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  wmKeyMap *keymap = WM_modalkeymap_ensure(keyconf, "Transform Modal Map", modal_items);
  keymap->poll_modal_item = transform_modal_item_poll;

  /* Default modal map values:
   *
   * \code{.c}
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_RETKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_CONFIRM);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_ESCKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_CANCEL);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_PAGEUPKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_AUTOIK_LEN_INC);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_PAGEDOWNKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_AUTOIK_LEN_DEC);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_GKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_TRANSLATE);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_RKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_ROTATE);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = EVT_SKEY,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_RESIZE);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = MIDDLEMOUSE,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_ANY,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_AUTOCONSTRAINT);
   * WM_modalkeymap_add_item(keymap,
   *                         &(const KeyMapItem_Params){
   *                             .type = MIDDLEMOUSE,
   *                             .value = KM_PRESS,
   *                             .modifier = KM_SHIFT,
   *                             .direction = KM_ANY,
   *                         },
   *                         TFM_MODAL_AUTOCONSTRAINTPLANE);
   * \endcode
   */

  return keymap;
}

static bool transform_event_modal_constraint(TransInfo *t, short modal_type)
{
  if (t->flag & T_NO_CONSTRAINT) {
    return false;
  }

  if (t->flag & T_2D_EDIT && ELEM(modal_type, TFM_MODAL_AXIS_Z, TFM_MODAL_PLANE_Z)) {
    return false;
  }

  int constraint_curr = -1;

  if (t->modifiers & (MOD_CONSTRAINT_SELECT_AXIS | MOD_CONSTRAINT_SELECT_PLANE)) {
    t->modifiers &= ~(MOD_CONSTRAINT_SELECT_AXIS | MOD_CONSTRAINT_SELECT_PLANE);

    /* Avoid changing orientation in this case. */
    constraint_curr = -2;
  }
  else if (t->con.mode & CON_APPLY) {
    constraint_curr = t->con.mode & (CON_AXIS0 | CON_AXIS1 | CON_AXIS2);
  }

  int constraint_new;
  const char *msg_2d = "", *msg_3d = "";

  /* Initialize. */
  switch (modal_type) {
    case TFM_MODAL_AXIS_X:
      msg_2d = IFACE_("along X");
      msg_3d = IFACE_("along %s X");
      constraint_new = CON_AXIS0;
      break;
    case TFM_MODAL_AXIS_Y:
      msg_2d = IFACE_("along Y");
      msg_3d = IFACE_("along %s Y");
      constraint_new = CON_AXIS1;
      break;
    case TFM_MODAL_AXIS_Z:
      msg_2d = IFACE_("along Z");
      msg_3d = IFACE_("along %s Z");
      constraint_new = CON_AXIS2;
      break;
    case TFM_MODAL_PLANE_X:
      msg_3d = IFACE_("locking %s X");
      constraint_new = CON_AXIS1 | CON_AXIS2;
      break;
    case TFM_MODAL_PLANE_Y:
      msg_3d = IFACE_("locking %s Y");
      constraint_new = CON_AXIS0 | CON_AXIS2;
      break;
    case TFM_MODAL_PLANE_Z:
      msg_3d = IFACE_("locking %s Z");
      constraint_new = CON_AXIS0 | CON_AXIS1;
      break;
    default:
      /* Invalid key. */
      return false;
  }

  if (t->flag & T_2D_EDIT) {
    BLI_assert(modal_type < TFM_MODAL_PLANE_X);
    if (constraint_new == CON_AXIS2) {
      return false;
    }

    if (t->data_type == &TransConvertType_SequencerImage) {
      /* Setup the 2d msg string so it writes out the transform space. */
      msg_2d = msg_3d;

      short orient_index = 1;
      if (t->orient_curr == O_DEFAULT || ELEM(constraint_curr, -1, constraint_new)) {
        /* Successive presses on existing axis, cycle orientation modes. */
        orient_index = short((t->orient_curr + 1) % int(ARRAY_SIZE(t->orient)));
      }

      transform_orientations_current_set(t, orient_index);
      if (orient_index != 0) {
        /* Make sure that we don't stop the constraint unless we are looped back around to
         * "no constraint". */
        constraint_curr = -1;
      }
    }

    if (constraint_curr == constraint_new) {
      stopConstraint(t);
    }
    else {
      setUserConstraint(t, constraint_new, msg_2d);
    }
  }
  else {
    short orient_index = 1;
    if (t->orient_curr == O_DEFAULT || ELEM(constraint_curr, -1, constraint_new)) {
      /* Successive presses on existing axis, cycle orientation modes. */
      orient_index = short((t->orient_curr + 1) % int(ARRAY_SIZE(t->orient)));
    }

    transform_orientations_current_set(t, orient_index);
    if (orient_index == 0) {
      stopConstraint(t);
    }
    else {
      setUserConstraint(t, constraint_new, msg_3d);
    }

    /* Take the opportunity to update the gizmo. */
    transform_gizmo_3d_model_from_constraint_and_mode_set(t);
  }
  return true;
}

static void tool_settings_update_snap_toggle(TransInfo *t)
{
  bool is_snap_enabled = (t->modifiers & MOD_SNAP) != 0;

  /* Type is #eSnapFlag, but type must match various snap attributes in #ToolSettings. */
  short *snap_flag_ptr;

  wmMsgParams_RNA msg_key_params = {{}};
  msg_key_params.ptr = RNA_pointer_create_discrete(&t->scene->id, &RNA_ToolSettings, t->settings);
  if ((snap_flag_ptr = transform_snap_flag_from_spacetype_ptr(t, &msg_key_params.prop)) &&
      (is_snap_enabled != bool(*snap_flag_ptr & SCE_SNAP)))
  {
    SET_FLAG_FROM_TEST(*snap_flag_ptr, is_snap_enabled, SCE_SNAP);
    WM_msg_publish_rna_params(t->mbus, &msg_key_params);
  }
}

wmOperatorStatus transformEvent(TransInfo *t, wmOperator *op, const wmEvent *event)
{
  bool is_navigating = t->vod ? ((RegionView3D *)t->region->regiondata)->rflag & RV3D_NAVIGATING :
                                false;

  /* Handle modal numinput events first, if already activated. */
  if (!is_navigating && ((event->val == KM_PRESS) || (event->type == EVT_MODAL_MAP)) &&
      hasNumInput(&t->num) && handleNumInput(t->context, &(t->num), event))
  {
    t->redraw |= TREDRAW_HARD;
  }
  else if (event->type == TIMER) {
    if (ED_uvedit_live_unwrap_timer_check(static_cast<const wmTimer *>(event->customdata))) {
      t->redraw |= TREDRAW_HARD;
    }
  }
  else if (!is_navigating && event->type == MOUSEMOVE) {
    t->mval = float2(event->mval);

    /* Use this for soft redraw. Might cause flicker in object mode. */
    // t->redraw |= TREDRAW_SOFT;
    t->redraw |= TREDRAW_HARD;

    if (t->state == TRANS_STARTING) {
      t->state = TRANS_RUNNING;
    }

    applyMouseInput(t, &t->mouse, t->mval, t->values);

    /* Snapping mouse move events. */
    t->redraw |= handleSnapping(t, event);
  }
  /* Handle modal keymap first. */
  /* Enforce redraw of transform when modifiers are used. */
  else if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case TFM_MODAL_CANCEL:
        if (!(t->modifiers & MOD_EDIT_SNAP_SOURCE)) {
          t->state = TRANS_CANCEL;
        }
        break;
      case TFM_MODAL_CONFIRM:
        if (!(t->modifiers & MOD_EDIT_SNAP_SOURCE)) {
          t->state = TRANS_CONFIRM;
        }
        break;
      case TFM_MODAL_TRANSLATE:
      case TFM_MODAL_ROTATE:
      case TFM_MODAL_RESIZE:
      case TFM_MODAL_TRACKBALL:
      case TFM_MODAL_ROTATE_NORMALS:
      case TFM_MODAL_VERT_EDGE_SLIDE:
        /* Only switch when supported. */
        if (!transform_mode_is_changeable(t->mode)) {
          break;
        }

        if ((event->val == TFM_MODAL_TRANSLATE && t->mode == TFM_TRANSLATION) ||
            (event->val == TFM_MODAL_RESIZE && t->mode == TFM_RESIZE))
        {
          if (t->data_type == &TransConvertType_Tracking) {
            restoreTransObjects(t);

            t->flag ^= T_ALT_TRANSFORM;
            t->redraw |= TREDRAW_HARD;
          }
          break;
        }

        if ((event->val == TFM_MODAL_ROTATE && t->mode == TFM_ROTATION) ||
            (event->val == TFM_MODAL_TRACKBALL && t->mode == TFM_TRACKBALL) ||
            (event->val == TFM_MODAL_ROTATE_NORMALS && t->mode == TFM_NORMAL_ROTATION) ||
            (event->val == TFM_MODAL_VERT_EDGE_SLIDE &&
             ELEM(t->mode, TFM_VERT_SLIDE, TFM_EDGE_SLIDE)))
        {
          break;
        }

        if (event->val == TFM_MODAL_ROTATE_NORMALS && t->data_type != &TransConvertType_Mesh) {
          break;
        }

        restoreTransObjects(t);
        resetTransModal(t);
        resetTransRestrictions(t);

        if (event->val == TFM_MODAL_TRANSLATE) {
          transform_mode_init(t, nullptr, TFM_TRANSLATION);
        }
        else if (event->val == TFM_MODAL_ROTATE) {
          transform_mode_init(t, nullptr, TFM_ROTATION);
        }
        else if (event->val == TFM_MODAL_TRACKBALL) {
          transform_mode_init(t, nullptr, TFM_TRACKBALL);
        }
        else if (event->val == TFM_MODAL_ROTATE_NORMALS) {
          transform_mode_init(t, nullptr, TFM_NORMAL_ROTATION);
        }
        else if (event->val == TFM_MODAL_RESIZE) {
          /* Scale isn't normally very useful after extrude along normals, see #39756 */
          if ((t->con.mode & CON_APPLY) && (t->orient[t->orient_curr].type == V3D_ORIENT_NORMAL)) {
            stopConstraint(t);
          }
          transform_mode_init(t, nullptr, TFM_RESIZE);
        }
        else {
          /* First try Edge Slide. */
          transform_mode_init(t, op, TFM_EDGE_SLIDE);
          /* If that fails, try Vertex Slide. */
          if (t->state == TRANS_CANCEL) {
            resetTransModal(t);
            t->state = TRANS_STARTING;
            transform_mode_init(t, op, TFM_VERT_SLIDE);
          }
          /* Vert Slide can fail on unconnected vertices (rare but possible). */
          if (t->state == TRANS_CANCEL) {
            resetTransModal(t);
            t->state = TRANS_STARTING;
            resetTransRestrictions(t);
            transform_mode_init(t, nullptr, TFM_TRANSLATION);
          }
        }

        /* Need to reinitialize after mode change. */
        transform_snap_reset_from_mode(t, op);
        applyMouseInput(t, &t->mouse, t->mval, t->values);
        t->redraw |= TREDRAW_HARD;
        break;

      case TFM_MODAL_SNAP_INV_ON:
        if (!(t->modifiers & MOD_SNAP_INVERT)) {
          t->modifiers |= MOD_SNAP_INVERT;
          transform_snap_flag_from_modifiers_set(t);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_SNAP_INV_OFF:
        if (t->modifiers & MOD_SNAP_INVERT) {
          t->modifiers &= ~MOD_SNAP_INVERT;
          transform_snap_flag_from_modifiers_set(t);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_SNAP_TOGGLE:
        t->modifiers ^= MOD_SNAP;
        transform_snap_flag_from_modifiers_set(t);
        tool_settings_update_snap_toggle(t);
        t->redraw |= TREDRAW_HARD;
        break;
      case TFM_MODAL_AXIS_X:
      case TFM_MODAL_AXIS_Y:
      case TFM_MODAL_AXIS_Z:
      case TFM_MODAL_PLANE_X:
      case TFM_MODAL_PLANE_Y:
      case TFM_MODAL_PLANE_Z:
        if (transform_event_modal_constraint(t, event->val)) {
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_CONS_OFF:
        if ((t->flag & T_NO_CONSTRAINT) == 0) {
          stopConstraint(t);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_ADD_SNAP:
        addSnapPoint(t);
        t->redraw |= TREDRAW_HARD;
        break;
      case TFM_MODAL_REMOVE_SNAP:
        removeSnapPoint(t);
        t->redraw |= TREDRAW_HARD;
        break;
      case TFM_MODAL_PROPSIZE:
        /* MOUSEPAN usage... */
        if (t->flag & T_PROP_EDIT) {
          float fac = 1.0f + 0.005f * (event->xy[1] - event->prev_xy[1]);
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
        }
        break;
      case TFM_MODAL_PROPSIZE_DOWN:
        if (t->flag & T_PROP_EDIT) {
          t->prop_size /= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          t->prop_size = max_ff(t->prop_size, T_PROP_SIZE_MIN);
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_AUTOIK_LEN_INC:
        if (t->flag & T_AUTOIK) {
          transform_autoik_update(t, 1);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_AUTOIK_LEN_DEC:
        if (t->flag & T_AUTOIK) {
          transform_autoik_update(t, -1);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_INSERTOFS_TOGGLE_DIR:
        if (t->spacetype == SPACE_NODE) {
          SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;

          BLI_assert(t->area->spacetype == t->spacetype);

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
      case TFM_MODAL_NODE_ATTACH_ON:
        t->modifiers |= MOD_NODE_ATTACH;
        t->redraw |= TREDRAW_HARD;
        break;
      case TFM_MODAL_NODE_ATTACH_OFF:
        t->modifiers &= ~MOD_NODE_ATTACH;
        t->redraw |= TREDRAW_HARD;
        break;
      case TFM_MODAL_NODE_FRAME:
        t->modifiers |= MOD_NODE_FRAME;
        t->redraw |= TREDRAW_HARD;
        break;

      case TFM_MODAL_AUTOCONSTRAINT:
      case TFM_MODAL_AUTOCONSTRAINTPLANE:
        if ((t->flag & T_RELEASE_CONFIRM) && (event->prev_val == KM_RELEASE) &&
            event->prev_type == t->launch_event)
        {
          /* Confirm transform if launch key is released after mouse move. */
          t->state = TRANS_CONFIRM;
        }
        else if ((t->flag & T_NO_CONSTRAINT) == 0) {
          if (t->modifiers & (MOD_CONSTRAINT_SELECT_AXIS | MOD_CONSTRAINT_SELECT_PLANE)) {
            /* Confirm. */
            postSelectConstraint(t);
            t->modifiers &= ~(MOD_CONSTRAINT_SELECT_AXIS | MOD_CONSTRAINT_SELECT_PLANE);
            t->redraw = TREDRAW_HARD;
          }
          else {
            if (t->options & CTX_CAMERA) {
              /* Exception for switching to dolly, or trackball, in camera view. */
              if (t->mode == TFM_TRANSLATION) {
                setLocalConstraint(t, (CON_AXIS2), IFACE_("along local Z"));
              }
              else if (t->mode == TFM_ROTATION) {
                restoreTransObjects(t);
                transform_mode_init(t, nullptr, TFM_TRACKBALL);
              }
              t->redraw = TREDRAW_HARD;
            }
            else {
              t->modifiers |= (event->val == TFM_MODAL_AUTOCONSTRAINT) ?
                                  MOD_CONSTRAINT_SELECT_AXIS :
                                  MOD_CONSTRAINT_SELECT_PLANE;
              if (t->con.mode & CON_APPLY) {
                stopConstraint(t);
                initSelectConstraint(t);

                /* In this case we might just want to remove the constraint,
                 * so set #TREDRAW_SOFT to only select the constraint on the next mouse move event.
                 * This way we can kind of "cancel" due to confirmation without constraint. */
                t->redraw = TREDRAW_SOFT;
              }
              else {
                initSelectConstraint(t);

                /* When first called, set #TREDRAW_HARD to select constraint immediately in
                 * #selectConstraint. */
                BLI_assert(t->redraw == TREDRAW_HARD);
              }
            }
          }
        }
        break;
      case TFM_MODAL_PRECISION:
        if (is_navigating) {
          /* WORKAROUND: During navigation, due to key conflicts, precision may be unintentionally
           * enabled. */
        }
        else if (event->prev_val == KM_PRESS) {
          t->modifiers |= MOD_PRECISION;
          /* Mouse position during Snap to Grid is not affected by precision. */
          if (!(validSnap(t) && t->tsnap.target_type == SCE_SNAP_TO_GRID)) {
            t->mouse.precision = true;
          }

          t->redraw |= TREDRAW_HARD;
        }
        else if (event->prev_val == KM_RELEASE) {
          t->modifiers &= ~MOD_PRECISION;
          t->mouse.precision = false;
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_EDIT_SNAP_SOURCE_ON:
        if (!(t->modifiers & MOD_EDIT_SNAP_SOURCE)) {
          transform_mode_snap_source_init(t, nullptr);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case TFM_MODAL_STRIP_CLAMP:
        t->modifiers ^= MOD_STRIP_CLAMP_HOLDS;
        t->redraw |= TREDRAW_HARD;
        break;
      default:
        break;
    }
  }
  /* Else do non-mapped events. */
  else if (event->val == KM_PRESS) {
    switch (event->type) {
      case EVT_CKEY:
        if (event->flag & WM_EVENT_IS_REPEAT) {
          break;
        }
        if (event->modifier & KM_ALT) {
          if (!(t->options & CTX_NO_PET)) {
            t->flag ^= T_PROP_CONNECTED;
            sort_trans_data_dist(t);
            calculatePropRatio(t);
            t->redraw = TREDRAW_HARD;
          }
        }
        break;
      case EVT_OKEY:
        if (event->flag & WM_EVENT_IS_REPEAT) {
          break;
        }
        if ((t->flag & T_PROP_EDIT) && (event->modifier & KM_SHIFT)) {
          t->prop_mode = (t->prop_mode + 1) % PROP_MODE_MAX;
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
        }
        break;
      case EVT_PADPLUSKEY:
        if ((event->modifier & KM_ALT) && (t->flag & T_PROP_EDIT)) {
          t->prop_size *= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          if (t->spacetype == SPACE_VIEW3D && t->persp != RV3D_ORTHO) {
            t->prop_size = min_ff(t->prop_size, ((View3D *)t->view)->clip_end);
          }
          calculatePropRatio(t);
          t->redraw = TREDRAW_HARD;
        }
        break;
      case EVT_PADMINUS:
        if ((event->modifier & KM_ALT) && (t->flag & T_PROP_EDIT)) {
          t->prop_size /= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          calculatePropRatio(t);
          t->redraw = TREDRAW_HARD;
        }
        break;
      case EVT_LEFTALTKEY:
      case EVT_RIGHTALTKEY:
        if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D, SPACE_IMAGE)) {
          t->flag |= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
        }
        break;
      default:
        break;
    }
  }
  else if (event->val == KM_RELEASE) {
    switch (event->type) {
      case EVT_LEFTALTKEY:
      case EVT_RIGHTALTKEY:
        /* TODO: Modal Map */
        if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D, SPACE_IMAGE)) {
          t->flag &= ~T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
        }
        break;
      default: {
        break;
      }
    }

    /* Confirm transform if launch key is released after mouse move. */
    if ((t->flag & T_RELEASE_CONFIRM) && event->type == t->launch_event) {
      t->state = TRANS_CONFIRM;
    }
  }

  /* Per transform event, if present. */
  if (t->mode_info && t->mode_info->handle_event_fn) {
    t->redraw |= t->mode_info->handle_event_fn(t, event);
  }

  /* Try to init modal numinput now, if possible. */
  if (!t->redraw && ((event->val == KM_PRESS) || (event->type == EVT_MODAL_MAP)) &&
      handleNumInput(t->context, &(t->num), event))
  {
    t->redraw |= TREDRAW_HARD;
  }

  if (t->redraw && !ISMOUSE_MOTION(event->type)) {
    /* The status area is currently also tagged to update by the notifiers in
     * `viewRedrawForce`. However, this may change in the future, and tagging
     * the region twice doesn't add any overhead. */
    WM_window_status_area_tag_redraw(CTX_wm_window(t->context));

    if (!ELEM(t->helpline, HLP_ERROR, HLP_ERROR_DASH)) {
      ED_workspace_status_text(t->context, nullptr);
    }
  }

  if (!is_navigating && t->redraw) {
    return wmOperatorStatus(0);
  }
  return OPERATOR_PASS_THROUGH;
}

bool calculateTransformCenter(bContext *C, int centerMode, float cent3d[3], float cent2d[2])
{
  TransInfo *t = MEM_callocN<TransInfo>("TransInfo data");
  bool success;

  t->context = C;

  t->state = TRANS_RUNNING;

  /* Avoid calculating proportional editing. */
  t->options = CTX_NO_PET;

  t->mode = TFM_DUMMY;

  initTransInfo(C, t, nullptr, nullptr);

  /* Avoid doing connectivity lookups (when V3D_AROUND_LOCAL_ORIGINS is set). */
  t->around = V3D_AROUND_CENTER_BOUNDS;

  create_trans_data(C, t); /* Make TransData structs from selection. */

  t->around = centerMode; /* Override user-defined mode. */

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
      /* Copy center from constraint center. Transform center can be local. */
      copy_v3_v3(cent3d, t->center_global);
    }
  }

  /* Does insert keyframes, and clears base flags; doesn't read `transdata`. */
  special_aftertrans_update(C, t);

  postTrans(C, t);

  MEM_freeN(t);

  return success;
}

static bool transinfo_show_overlay(TransInfo *t, ARegion *region)
{
  /* Don't show overlays when not the active view and when overlay is disabled: #57139 */
  if (region != t->region) {
    return false;
  }

  switch (t->spacetype) {
    case SPACE_VIEW3D: {
      const View3D *v3d = static_cast<const View3D *>(t->view);
      return (v3d->flag2 & V3D_HIDE_OVERLAYS) == 0;
    }
    case SPACE_IMAGE: {
      const SpaceImage *sima = static_cast<const SpaceImage *>(t->area->spacedata.first);
      return (sima->overlay.flag & SI_OVERLAY_SHOW_OVERLAYS) != 0;
    }
  }
  return false;
}

static void drawTransformView(const bContext * /*C*/, ARegion *region, void *arg)
{
  TransInfo *t = static_cast<TransInfo *>(arg);

  if (!transinfo_show_overlay(t, region)) {
    return;
  }

  GPU_line_width(1.0f);

  drawConstraint(t);

  switch (t->spacetype) {
    case SPACE_GRAPH:
    case SPACE_ACTION:
      /* Different visualization because the proportional editing in these editors only looks at
       * the x-axis. */
      drawPropRange(t);
      break;

    default:
      drawPropCircle(t);
      break;
  }

  drawSnapping(t);

  if (region == t->region && t->mode_info && t->mode_info->draw_fn) {
    t->mode_info->draw_fn(t);
  }
}

/**
 * Just draw a little warning message in the top-right corner of the viewport
 * to warn that auto-keying is enabled.
 */
static void drawAutoKeyWarning(TransInfo *t, ARegion *region)
{
  const char *printable = IFACE_("Auto Keying On");
  float printable_size[2];
  int xco, yco;
  int offset = 0;

  const rcti *rect = ED_region_visible_rect(region);

  View3D *v3d = nullptr;
  Scene *scene = nullptr;
  if (t->spacetype == SPACE_VIEW3D) {
    v3d = static_cast<View3D *>(t->view);
    scene = t->scene;
  }

  const int font_id = BLF_set_default();
  BLF_width_and_height(
      font_id, printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);

  /* Check to see if the Navigation Gizmo is enabled. */
  if ((t->spacetype != SPACE_VIEW3D) || (v3d == nullptr) ||
      (v3d->gizmo_flag & (V3D_GIZMO_HIDE | V3D_GIZMO_HIDE_NAVIGATE)))
  {
    offset = 10;
  }
  else {
    /* Depending on user MINI_AXIS preference, pad accordingly. */
    switch ((eUserpref_MiniAxisType)U.mini_axis_type) {
      case USER_MINI_AXIS_TYPE_GIZMO:
        offset = U.gizmo_size_navigate_v3d;
        break;
      case USER_MINI_AXIS_TYPE_MINIMAL:
        offset = U.rvisize * std::min((U.pixelsize / U.scale_factor), 1.0f) * 2.5f;
        break;
      case USER_MINI_AXIS_TYPE_NONE:
        offset = U.rvisize;
        break;
    }
  }

  offset *= U.scale_factor;

  xco = (rect->xmax - U.widget_unit) - int(printable_size[0]) - offset;
  yco = (rect->ymax - U.widget_unit);

  /* Warning text (to clarify meaning of overlays)
   * - Original color was red to match the icon, but that clashes badly with a less nasty border.
   */

  float text_color[4], shadow_color[4];
  if (v3d && scene) {
    ED_view3d_text_colors_get(scene, v3d, text_color, shadow_color);
  }
  else {
    UI_GetThemeColor4fv(TH_TEXT_HI, text_color);
    UI_GetThemeColor4fv(TH_BACK, text_color);
  }
  BLF_color4fv(BLF_default(), text_color);
  BLF_shadow(BLF_default(), FontShadowType::Outline, shadow_color);
  BLF_draw_default(xco, yco, 0.0f, printable, BLF_DRAW_STR_DUMMY_MAX);

  /* Auto-key recording icon. */
  GPU_blend(GPU_BLEND_ALPHA);

  xco -= U.widget_unit;
  yco -= int(printable_size[1]) / 2;

  UI_icon_draw(xco, yco, ICON_REC);

  GPU_blend(GPU_BLEND_NONE);
}

static void drawTransformPixel(const bContext * /*C*/, ARegion *region, void *arg)
{
  TransInfo *t = static_cast<TransInfo *>(arg);

  if (!transinfo_show_overlay(t, region)) {
    return;
  }

  if (region != t->region) {
    return;
  }

  /* Draw auto-key-framing hint in the corner
   * - only draw if enabled (advanced users may be distracted/annoyed),
   *   for objects that will be auto-keyframed (no point otherwise),
   *   AND only for the active region (as showing all is too overwhelming)
   */
  if ((U.keying_flag & AUTOKEY_FLAG_NOWARNING) == 0) {
    if (t->options & (CTX_OBJECT | CTX_POSE_BONE)) {
      Scene *scene = t->scene;
      ViewLayer *view_layer = t->view_layer;
      BKE_view_layer_synced_ensure(scene, view_layer);
      Object *ob = BKE_view_layer_active_object_get(view_layer);

      if (ob && animrig::autokeyframe_cfra_can_key(scene, &ob->id)) {
        drawAutoKeyWarning(t, region);
      }
    }
  }
}

void saveTransform(bContext *C, TransInfo *t, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  PropertyRNA *prop;

  bool use_prop_edit = false;
  int prop_edit_flag = 0;

  /* Save proportional edit settings.
   * Skip saving proportional edit if it was not actually used.
   * Note that this value is being saved even if the operation is canceled. This is to maintain a
   * behavior already used by users. */
  if (!(t->options & CTX_NO_PET)) {
    if (t->flag & T_PROP_EDIT_ALL) {
      if (t->flag & T_PROP_EDIT) {
        use_prop_edit = true;
      }
      if (t->flag & T_PROP_CONNECTED) {
        prop_edit_flag |= PROP_EDIT_CONNECTED;
      }
      if (t->flag & T_PROP_PROJECTED) {
        prop_edit_flag |= PROP_EDIT_PROJECTED;
      }
    }

    /* If modal, save settings back in scene if not set as operator argument. */
    if ((t->flag & T_MODAL) || (op->flag & OP_IS_REPEAT)) {
      /* Save settings if not set in operator. */
      if ((prop = RNA_struct_find_property(op->ptr, "use_proportional_edit")) &&
          !RNA_property_is_set(op->ptr, prop))
      {
        BKE_view_layer_synced_ensure(t->scene, t->view_layer);
        const Object *obact = BKE_view_layer_active_object_get(t->view_layer);
        const eObjectMode object_mode = eObjectMode(obact ? obact->mode : OB_MODE_OBJECT);

        if (t->spacetype == SPACE_GRAPH) {
          ts->proportional_fcurve = use_prop_edit;
        }
        else if (t->spacetype == SPACE_ACTION) {
          ts->proportional_action = use_prop_edit;
        }
        else if (t->options & CTX_MASK) {
          ts->proportional_mask = use_prop_edit;
        }
        else if (object_mode == OB_MODE_OBJECT) {
          /* No active object means #TransConvertType_Object [see #convert_type_get()], so use
           * toolsetting for *object*. */
          ts->proportional_objects = use_prop_edit;
        }
        else {
          if (use_prop_edit) {
            ts->proportional_edit |= PROP_EDIT_USE;
          }
          else {
            ts->proportional_edit &= ~PROP_EDIT_USE;
          }
        }
      }

      if ((prop = RNA_struct_find_property(op->ptr, "proportional_size"))) {
        ts->proportional_size = RNA_property_is_set(op->ptr, prop) ?
                                    RNA_property_float_get(op->ptr, prop) :
                                    t->prop_size;
      }

      if ((prop = RNA_struct_find_property(op->ptr, "proportional_edit_falloff")) &&
          !RNA_property_is_set(op->ptr, prop))
      {
        ts->prop_mode = t->prop_mode;
      }
    }
  }

  if (t->state == TRANS_CANCEL) {
    /* No need to edit operator properties or tool settings if we are canceling the operation.
     * These properties must match the original ones. */
    return;
  }

  if (!(t->options & CTX_NO_PET)) {
    if ((prop = RNA_struct_find_property(op->ptr, "use_proportional_edit"))) {
      RNA_property_boolean_set(op->ptr, prop, use_prop_edit);
      RNA_boolean_set(op->ptr, "use_proportional_connected", prop_edit_flag & PROP_EDIT_CONNECTED);
      RNA_boolean_set(op->ptr, "use_proportional_projected", prop_edit_flag & PROP_EDIT_PROJECTED);
      RNA_enum_set(op->ptr, "proportional_edit_falloff", t->prop_mode);
      RNA_float_set(op->ptr, "proportional_size", t->prop_size);
    }
  }

  /* Save back mode in case we're in the generic operator. */
  if ((prop = RNA_struct_find_property(op->ptr, "mode"))) {
    RNA_property_enum_set(op->ptr, prop, t->mode);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "value"))) {
    if (RNA_property_array_check(prop)) {
      RNA_property_float_set_array(op->ptr, prop, t->values_final);
    }
    else {
      RNA_property_float_set(op->ptr, prop, t->values_final[0]);
    }
  }

  /* Save snapping settings. */
  if ((prop = RNA_struct_find_property(op->ptr, "snap"))) {

    /* Update the snap toggle in `ToolSettings`. */
    if (
        /* Update only if snapping has changed during a modal operation. */
        (t->flag & T_MODAL) &&
        /* Skip updating if the snapping mode does not match the snap types. */
        transformModeUseSnap(t) &&
        /* Skip updating the snap toggle if it was not explicitly set by the user. */
        !(t->modifiers & MOD_SNAP_FORCED) &&
        /* Skip updating the snap toggle if snapping was enabled via operator properties. */
        !RNA_property_is_set(op->ptr, prop))
    {
      tool_settings_update_snap_toggle(t);
    }

    bool is_snap_enabled = (t->modifiers & MOD_SNAP) != 0;
    RNA_property_boolean_set(op->ptr, prop, is_snap_enabled);

    if ((prop = RNA_struct_find_property(op->ptr, "snap_elements"))) {
      RNA_property_enum_set(op->ptr, prop, t->tsnap.mode);
      RNA_boolean_set(
          op->ptr, "use_snap_project", (t->tsnap.mode & SCE_SNAP_INDIVIDUAL_PROJECT) != 0);
      RNA_enum_set(op->ptr, "snap_target", t->tsnap.source_operation);

      eSnapTargetOP target = t->tsnap.target_operation;
      RNA_boolean_set(op->ptr, "use_snap_self", (target & SCE_SNAP_TARGET_NOT_ACTIVE) == 0);
      RNA_boolean_set(op->ptr, "use_snap_edit", (target & SCE_SNAP_TARGET_NOT_EDITED) == 0);
      RNA_boolean_set(op->ptr, "use_snap_nonedit", (target & SCE_SNAP_TARGET_NOT_NONEDITED) == 0);
      RNA_boolean_set(
          op->ptr, "use_snap_selectable", (target & SCE_SNAP_TARGET_ONLY_SELECTABLE) != 0);
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "mirror"))) {
    RNA_property_boolean_set(op->ptr, prop, (t->flag & T_NO_MIRROR) == 0);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis"))) {
    if (t->flag & T_MODAL) {
      if (t->con.mode & CON_APPLY) {
        int orient_axis = constraintModeToIndex(t);
        if (orient_axis != -1) {
          RNA_property_enum_set(op->ptr, prop, orient_axis);
          t->con.mode &= ~CON_APPLY;
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

  if ((prop = RNA_struct_find_property(op->ptr, "orient_type"))) {
    short orient_type_set, orient_type_curr;
    orient_type_set = RNA_property_is_set(op->ptr, prop) ? RNA_property_enum_get(op->ptr, prop) :
                                                           -1;
    orient_type_curr = t->orient[t->orient_curr].type;

    if (!ELEM(orient_type_curr, orient_type_set, V3D_ORIENT_CUSTOM_MATRIX)) {
      RNA_property_enum_set(op->ptr, prop, orient_type_curr);
      orient_type_set = orient_type_curr;
    }

    if ((prop = RNA_struct_find_property(op->ptr, "orient_matrix_type")) &&
        !RNA_property_is_set(op->ptr, prop))
    {
      /* Set the first time to register on redo. */
      RNA_property_enum_set(op->ptr, prop, orient_type_set);
      RNA_float_set_array(op->ptr, "orient_matrix", &t->spacemtx[0][0]);
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "constraint_axis"))) {
    bool constraint_axis[3] = {false, false, false};
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
      RNA_property_boolean_set_array(op->ptr, prop, constraint_axis);
    }
    else {
      RNA_property_unset(op->ptr, prop);
    }
  }

  {
    const char *prop_id = nullptr;
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
        op->ptr, prop, (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT_SLIDE) != 0);
  }
}

bool initTransform(bContext *C, TransInfo *t, wmOperator *op, const wmEvent *event, int mode)
{
  int options = 0;
  PropertyRNA *prop;

  mode = transform_mode_really_used(C, eTfmMode(mode));

  t->context = C;

  /* Added initialize, for external calls to set stuff in TransInfo, like undo string. */

  t->state = TRANS_STARTING;

  if ((prop = RNA_struct_find_property(op->ptr, "cursor_transform")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_CURSOR;
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "texture_space")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_TEXTURE_SPACE;
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "gpencil_strokes")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_GPENCIL_STROKES;
    }
  }

  if ((prop = RNA_struct_find_property(op->ptr, "view2d_edge_pan")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    if (RNA_property_boolean_get(op->ptr, prop)) {
      options |= CTX_VIEW2D_EDGE_PAN;
    }
  }

  t->options = eTContext(options);

  t->mode = eTfmMode(mode);

  /* Needed to translate tweak events to mouse buttons. */
  t->launch_event = event ? WM_userdef_event_type_from_keymap_type(event->type) : -1;
  t->is_launch_event_drag = event ? (event->val == KM_PRESS_DRAG) : false;

  unit_m3(t->spacemtx);

  initTransInfo(C, t, op, event);

  if (!G.background) {
    if (t->spacetype == SPACE_VIEW3D) {
      t->draw_handle_view = ED_region_draw_cb_activate(
          t->region->runtime->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
      t->draw_handle_pixel = ED_region_draw_cb_activate(
          t->region->runtime->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
      t->draw_handle_cursor = WM_paint_cursor_activate(
          SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
    }
    else if (ELEM(t->spacetype,
                  SPACE_IMAGE,
                  SPACE_CLIP,
                  SPACE_NODE,
                  SPACE_GRAPH,
                  SPACE_ACTION,
                  SPACE_SEQ))
    {
      t->draw_handle_view = ED_region_draw_cb_activate(
          t->region->runtime->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
      t->draw_handle_cursor = WM_paint_cursor_activate(
          SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
    }
  }

  create_trans_data(C, t); /* Make #TransData structs from selection. */

  if (t->data_len_all == 0) {
    postTrans(C, t);
    return false;
  }

  /* When proportional editing is enabled, data_len_all can be non zero when
   * nothing is selected, if this is the case we can end the transform early.
   *
   * By definition transform-data has selected items in beginning,
   * so only the first item in each container needs to be checked
   * when looking for the presence of selected data. */
  if (t->flag & T_PROP_EDIT) {
    bool has_selected_any = false;
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      if (tc->data_len == 0) {
        continue;
      }

      if (!tc->sorted_index_map) {
        BLI_assert_msg(tc->data[0].flag & TD_SELECTED,
                       "Without sorted_index_map, all items are expected to be selected");
        has_selected_any = true;
        break;
      }

      const int first_selected_index = tc->sorted_index_map[0];
      TransData *td = &tc->data[first_selected_index];
      if (td->flag & TD_SELECTED) {
        has_selected_any = true;
        break;
      }
    }

    if (!has_selected_any) {
      postTrans(C, t);
      return false;
    }
  }

  if (event) {
    /* Keymap for shortcut header prints. */
    t->keymap = WM_keymap_active(CTX_wm_manager(C), op->type->modalkeymap);

    /* Stupid code to have Ctrl-Click on gizmo work ok.
     *
     * Do this only for translation/rotation/resize because only these
     * modes are available from gizmo and doing such check could
     * lead to keymap conflicts for other modes (see #31584)
     */
    if (ELEM(mode, TFM_TRANSLATION, TFM_ROTATION, TFM_RESIZE)) {
      LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &t->keymap->items) {
        if (kmi->flag & KMI_INACTIVE) {
          continue;
        }

        if (kmi->propvalue == TFM_MODAL_SNAP_INV_ON && kmi->val == KM_PRESS) {
          if ((ELEM(kmi->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY) &&
               (event->modifier & KM_CTRL)) ||
              (ELEM(kmi->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY) &&
               (event->modifier & KM_SHIFT)) ||
              (ELEM(kmi->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY) && (event->modifier & KM_ALT)) ||
              ((kmi->type == EVT_OSKEY) && (event->modifier & KM_OSKEY)))
          {
            t->modifiers |= MOD_SNAP_INVERT;
          }
          break;
        }
      }
    }
    if (t->data_type == &TransConvertType_Node) {
      /* Set the initial auto-attach flag based on whether the chosen keymap key is pressed at the
       * start of the operator. */
      t->modifiers |= MOD_NODE_ATTACH;
      LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &t->keymap->items) {
        if (kmi->flag & KMI_INACTIVE) {
          continue;
        }

        if (kmi->propvalue == TFM_MODAL_NODE_ATTACH_OFF && kmi->val == KM_PRESS) {
          if ((ELEM(kmi->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY) &&
               (event->modifier & KM_CTRL)) ||
              (ELEM(kmi->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY) &&
               (event->modifier & KM_SHIFT)) ||
              (ELEM(kmi->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY) && (event->modifier & KM_ALT)) ||
              ((kmi->type == EVT_OSKEY) && (event->modifier & KM_OSKEY)) ||
              ((kmi->type == EVT_HYPER) && (event->modifier & KM_HYPER)))
          {
            t->modifiers &= ~MOD_NODE_ATTACH;
          }
          break;
        }
      }
    }
  }

  initSnapping(t, op); /* Initialize snapping data AFTER mode flags. */

  /* EVIL! pose-mode code can switch translation to rotate when 1 bone is selected.
   * will be removed (ton). */

  /* EVIL2: we gave as argument also texture space context bit... was cleared. */

  /* EVIL3: extend mode for animation editors also switches modes...
   * but is best way to avoid duplicate code. */
  mode = t->mode;

  calculatePropRatio(t);
  calculateCenter(t);

  if (event) {
    /* Initialize accurate transform to settings requested by keymap. */
    bool use_accurate = false;
    if ((prop = RNA_struct_find_property(op->ptr, "use_accurate")) &&
        RNA_property_is_set(op->ptr, prop))
    {
      if (RNA_property_boolean_get(op->ptr, prop)) {
        use_accurate = true;
      }
    }

    initMouseInput(t, &t->mouse, t->center2d, t->mval, use_accurate);
  }

  transform_mode_init(t, op, mode);

  if (t->state == TRANS_CANCEL) {
    postTrans(C, t);
    return false;
  }

  /* Transformation axis from operator. */
  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    t->orient_axis = RNA_property_enum_get(op->ptr, prop);
  }
  if ((prop = RNA_struct_find_property(op->ptr, "orient_axis_ortho")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    t->orient_axis_ortho = RNA_property_enum_get(op->ptr, prop);
  }

  /* Constraint init from operator. */
  if (t->con.mode & CON_APPLY) {
    setUserConstraint(t, t->con.mode, "%s");
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
        BMEditMesh *em = nullptr; /* BKE_editmesh_from_object(t->obedit); */
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

  t->context = nullptr;

  return true;
}

void transformApply(bContext *C, TransInfo *t)
{
  t->context = C;

  if (t->redraw == TREDRAW_HARD) {
    selectConstraint(t);
    if (t->mode_info) {
      t->mode_info->transform_fn(t); /* Calls #recalc_data(). */
    }
  }

  if (t->redraw & TREDRAW_SOFT) {
    viewRedrawForce(C, t);
  }

  t->redraw = TREDRAW_NOTHING;

  /* If auto confirm is on, break after one pass. */
  if (t->options & CTX_AUTOCONFIRM) {
    t->state = TRANS_CONFIRM;
  }

  t->context = nullptr;
}

wmOperatorStatus transformEnd(bContext *C, TransInfo *t)
{
  wmOperatorStatus exit_code = OPERATOR_RUNNING_MODAL;

  t->context = C;

  if (!ELEM(t->state, TRANS_STARTING, TRANS_RUNNING)) {
    /* Handle restoring objects. */
    if (t->state == TRANS_CANCEL) {
      exit_code = OPERATOR_CANCELLED;
      restoreTransObjects(t); /* Calls #recalc_data(). */
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

    /* Does insert keyframes, and clears base flags; doesn't read `transdata`. */
    special_aftertrans_update(C, t);

    /* Free data, also handles overlap [in freeTransCustomData()]. */
    postTrans(C, t);

    /* Send events out for redraws. */
    viewRedrawPost(C, t);

    viewRedrawForce(C, t);

    transform_gizmo_3d_model_from_constraint_and_mode_restore(t);
  }

  t->context = nullptr;

  return exit_code;
}

bool checkUseAxisMatrix(TransInfo *t)
{
  /* Currently only checks for editmode. */
  if (t->flag & T_EDIT) {
    if ((t->around == V3D_AROUND_LOCAL_ORIGINS) &&
        ELEM(t->obedit_type, OB_MESH, OB_CURVES_LEGACY, OB_MBALL, OB_ARMATURE))
    {
      /* Not all editmode supports axis-matrix. */
      return true;
    }
  }

  return false;
}

bool transform_apply_matrix(TransInfo *t, float mat[4][4])
{
  if (t->mode_info && t->mode_info->transform_matrix_fn) {
    t->mode_info->transform_matrix_fn(t, mat);
    return true;
  }
  return false;
}

void transform_final_value_get(const TransInfo *t, float *value, const int value_num)
{
  memcpy(value, t->values_final, sizeof(float) * value_num);
}

void view_vector_calc(const TransInfo *t, const float focus[3], float r_vec[3])
{
  if (t->persp != RV3D_ORTHO) {
    sub_v3_v3v3(r_vec, t->viewinv[3], focus);
  }
  else {
    copy_v3_v3(r_vec, t->viewinv[2]);
  }
  normalize_v3(r_vec);
}

}  // namespace blender::ed::transform
