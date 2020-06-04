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
#include "DNA_mask_types.h"
#include "DNA_mesh_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_mask.h"
#include "BKE_scene.h"

#include "GPU_state.h"

#include "ED_clip.h"
#include "ED_gpencil.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_node.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "UI_interface_icons.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "BLF_api.h"
#include "BLT_translation.h"

#include "transform.h"
#include "transform_constraints.h"
#include "transform_convert.h"
#include "transform_draw_cursors.h"
#include "transform_mode.h"
#include "transform_snap.h"

/* Disabling, since when you type you know what you are doing,
 * and being able to set it to zero is handy. */
// #define USE_NUM_NO_ZERO

static void drawTransformApply(const struct bContext *C, ARegion *region, void *arg);

static void initSnapSpatial(TransInfo *t, float r_snap[3]);

bool transdata_check_local_islands(TransInfo *t, short around)
{
  return ((around == V3D_AROUND_LOCAL_ORIGINS) && ((ELEM(t->obedit_type, OB_MESH, OB_GPENCIL))));
}

/* ************************** SPACE DEPENDENT CODE **************************** */

void setTransformViewMatrices(TransInfo *t)
{
  if (t->spacetype == SPACE_VIEW3D && t->region && t->region->regiontype == RGN_TYPE_WINDOW) {
    RegionView3D *rv3d = t->region->regiondata;

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
    SpaceImage *sima = t->area->spacedata.first;

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
    SpaceClip *sclip = t->area->spacedata.first;

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
  if ((t->spacetype == SPACE_VIEW3D) && (t->region->regiontype == RGN_TYPE_WINDOW)) {
    if (t->options & CTX_PAINT_CURVE) {
      r_vec[0] = dx;
      r_vec[1] = dy;
    }
    else {
      const float mval_f[2] = {(float)dx, (float)dy};
      ED_view3d_win_to_delta(t->region, mval_f, r_vec, t->zfac);
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
    convertViewVec2D(&t->region->v2d, r_vec, dx, dy);
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
    if (t->region->regiontype == RGN_TYPE_WINDOW) {
      if (ED_view3d_project_int_global(t->region, vec, adr, flag) != V3D_PROJ_RET_OK) {
        /* this is what was done in 2.64, perhaps we can be smarter? */
        adr[0] = (int)2140000000.0f;
        adr[1] = (int)2140000000.0f;
      }
    }
  }
  else if (t->spacetype == SPACE_IMAGE) {
    SpaceImage *sima = t->area->spacedata.first;

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

      UI_view2d_view_to_region(t->view, v[0], v[1], &adr[0], &adr[1]);
    }
  }
  else if (t->spacetype == SPACE_ACTION) {
    int out[2] = {0, 0};
#if 0
    SpaceAction *sact = t->area->spacedata.first;

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
    SpaceClip *sc = t->area->spacedata.first;

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
      else if (t->region->regiontype == RGN_TYPE_WINDOW) {
        /* allow points behind the view [#33643] */
        if (ED_view3d_project_float_global(t->region, vec, adr, flag) != V3D_PROJ_RET_OK) {
          /* XXX, 2.64 and prior did this, weak! */
          adr[0] = t->region->winx / 2.0f;
          adr[1] = t->region->winy / 2.0f;
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
    SpaceImage *sima = t->area->spacedata.first;

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
    SpaceImage *sima = t->area->spacedata.first;

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
      WM_paint_cursor_tag_redraw(window, t->region);
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
    // SpaceAction *saction = (SpaceAction *)t->area->spacedata.first;
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    // SpaceGraph *sipo = (SpaceGraph *)t->area->spacedata.first;
    WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_NLA) {
    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  }
  else if (t->spacetype == SPACE_NODE) {
    // ED_area_tag_redraw(t->area);
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
      WM_paint_cursor_tag_redraw(window, t->region);
    }
    else if (t->flag & T_CURSOR) {
      ED_area_tag_redraw(t->area);
    }
    else {
      // XXX how to deal with lock?
      SpaceImage *sima = (SpaceImage *)t->area->spacedata.first;
      if (sima->lock) {
        WM_event_add_notifier(C, NC_GEOM | ND_DATA, OBEDIT_FROM_VIEW_LAYER(t->view_layer)->data);
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
  ED_area_status_text(t->area, NULL);

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

static void view_editmove(ushort UNUSED(event))
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
      else if ((t->tsnap.mode & ~(SCE_SNAP_MODE_INCREMENT | SCE_SNAP_MODE_GRID)) == 0) {
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

  wmKeyMap *keymap = WM_modalkeymap_find(keyconf, "Transform Modal Map");

  keymap = WM_modalkeymap_ensure(keyconf, "Transform Modal Map", modal_items);
  keymap->poll_modal_item = transform_modal_item_poll;

  return keymap;
}

static void transform_event_xyz_constraint(TransInfo *t, short key_type, bool is_plane)
{
  if (!(t->flag & T_NO_CONSTRAINT)) {
    char cmode = constraintModeToChar(t);
    int constraint_axis, constraint_plane;
    const bool edit_2d = (t->flag & T_2D_EDIT) != 0;
    const char *msg1 = "", *msg2 = "", *msg3 = "";
    char axis;

    /* Initialize */
    switch (key_type) {
      case EVT_XKEY:
        msg1 = TIP_("along X");
        msg2 = TIP_("along %s X");
        msg3 = TIP_("locking %s X");
        axis = 'X';
        constraint_axis = CON_AXIS0;
        break;
      case EVT_YKEY:
        msg1 = TIP_("along Y");
        msg2 = TIP_("along %s Y");
        msg3 = TIP_("locking %s Y");
        axis = 'Y';
        constraint_axis = CON_AXIS1;
        break;
      case EVT_ZKEY:
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

    if (edit_2d && (key_type != EVT_ZKEY)) {
      if (cmode == axis) {
        stopConstraint(t);
      }
      else {
        setUserConstraint(t, V3D_ORIENT_GLOBAL, constraint_axis, msg1);
      }
    }
    else if (!edit_2d) {
      if (t->orient_curr == 0 || ELEM(cmode, '\0', axis)) {
        /* Successive presses on existing axis, cycle orientation modes. */
        t->orient_curr = (short)((t->orient_curr + 1) % (int)ARRAY_SIZE(t->orient));
        transform_orientations_current_set(t, t->orient_curr);
      }

      if (t->orient_curr == 0) {
        stopConstraint(t);
      }
      else {
        const short orientation = t->orient[t->orient_curr].type;
        if (is_plane == false) {
          setUserConstraint(t, orientation, constraint_axis, msg2);
        }
        else {
          setUserConstraint(t, orientation, constraint_plane, msg3);
        }
      }
    }
    t->redraw |= TREDRAW_HARD;
  }
}

int transformEvent(TransInfo *t, const wmEvent *event)
{
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
        if (t->mode == TFM_TRANSLATION) {
          if ((t->obedit_type == OB_MESH) && (t->spacetype == SPACE_VIEW3D)) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);

            /* first try edge slide */
            transform_mode_init(t, NULL, TFM_EDGE_SLIDE);
            /* if that fails, do vertex slide */
            if (t->state == TRANS_CANCEL) {
              resetTransModal(t);
              t->state = TRANS_STARTING;
              transform_mode_init(t, NULL, TFM_VERT_SLIDE);
            }
            /* vert slide can fail on unconnected vertices (rare but possible) */
            if (t->state == TRANS_CANCEL) {
              resetTransModal(t);
              t->state = TRANS_STARTING;
              restoreTransObjects(t);
              resetTransRestrictions(t);
              transform_mode_init(t, NULL, TFM_TRANSLATION);
            }
            initSnapping(t, NULL);  // need to reinit after mode change
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
          else if (t->options & (CTX_MOVIECLIP | CTX_MASK)) {
            restoreTransObjects(t);

            t->flag ^= T_ALT_TRANSFORM;
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        else if (t->mode == TFM_SEQ_SLIDE) {
          t->flag ^= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        else if (transform_mode_is_changeable(t->mode)) {
          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          transform_mode_init(t, NULL, TFM_TRANSLATION);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_ROTATE:
        /* only switch when... */
        if (!(t->options & CTX_TEXTURE) && !(t->options & (CTX_MOVIECLIP | CTX_MASK))) {
          if (transform_mode_is_changeable(t->mode)) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);

            if (t->mode == TFM_ROTATION) {
              transform_mode_init(t, NULL, TFM_TRACKBALL);
            }
            else {
              transform_mode_init(t, NULL, TFM_ROTATION);
            }
            initSnapping(t, NULL);  // need to reinit after mode change
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        break;
      case TFM_MODAL_RESIZE:
        /* only switch when... */
        if (t->mode == TFM_RESIZE) {
          if (t->options & CTX_MOVIECLIP) {
            restoreTransObjects(t);

            t->flag ^= T_ALT_TRANSFORM;
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        else if (t->mode == TFM_SHRINKFATTEN) {
          t->flag ^= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        else if (transform_mode_is_changeable(t->mode)) {
          /* Scale isn't normally very useful after extrude along normals, see T39756 */
          if ((t->con.mode & CON_APPLY) && (t->orient[t->orient_curr].type == V3D_ORIENT_NORMAL)) {
            stopConstraint(t);
          }

          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          transform_mode_init(t, NULL, TFM_RESIZE);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
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
          transform_event_xyz_constraint(t, EVT_XKEY, false);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_AXIS_Y:
        if ((t->flag & T_NO_CONSTRAINT) == 0) {
          transform_event_xyz_constraint(t, EVT_YKEY, false);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_AXIS_Z:
        if ((t->flag & (T_NO_CONSTRAINT)) == 0) {
          transform_event_xyz_constraint(t, EVT_ZKEY, false);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PLANE_X:
        if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
          transform_event_xyz_constraint(t, EVT_XKEY, true);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PLANE_Y:
        if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
          transform_event_xyz_constraint(t, EVT_YKEY, true);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case TFM_MODAL_PLANE_Z:
        if ((t->flag & (T_NO_CONSTRAINT | T_2D_EDIT)) == 0) {
          transform_event_xyz_constraint(t, EVT_ZKEY, true);
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
      case EVT_ESCKEY:
      case RIGHTMOUSE:
        t->state = TRANS_CANCEL;
        handled = true;
        break;

      case EVT_SPACEKEY:
      case EVT_PADENTER:
      case EVT_RETKEY:
        if (event->is_repeat) {
          break;
        }
        t->state = TRANS_CONFIRM;
        handled = true;
        break;

      /* enforce redraw of transform when modifiers are used */
      case EVT_LEFTSHIFTKEY:
      case EVT_RIGHTSHIFTKEY:
        t->modifiers |= MOD_CONSTRAINT_PLANE;
        t->redraw |= TREDRAW_HARD;
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
              transform_mode_init(t, NULL, TFM_TRACKBALL);
            }
          }
          else {
            t->modifiers |= MOD_CONSTRAINT_SELECT;
            if (t->con.mode & CON_APPLY) {
              stopConstraint(t);
            }
            else {
              initSelectConstraint(t);
              postSelectConstraint(t);
            }
          }
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case EVT_GKEY:
        if (event->is_repeat) {
          break;
        }
        /* only switch when... */
        if (t->mode != TFM_TRANSLATION && transform_mode_is_changeable(t->mode)) {
          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          transform_mode_init(t, NULL, TFM_TRANSLATION);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case EVT_SKEY:
        if (event->is_repeat) {
          break;
        }
        /* only switch when... */
        if (t->mode != TFM_RESIZE && transform_mode_is_changeable(t->mode)) {
          restoreTransObjects(t);
          resetTransModal(t);
          resetTransRestrictions(t);
          transform_mode_init(t, NULL, TFM_RESIZE);
          initSnapping(t, NULL);  // need to reinit after mode change
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case EVT_RKEY:
        if (event->is_repeat) {
          break;
        }
        /* only switch when... */
        if (!(t->options & CTX_TEXTURE)) {
          if (transform_mode_is_changeable(t->mode)) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);

            if (t->mode == TFM_ROTATION) {
              transform_mode_init(t, NULL, TFM_TRACKBALL);
            }
            else {
              transform_mode_init(t, NULL, TFM_ROTATION);
            }
            initSnapping(t, NULL);  // need to reinit after mode change
            t->redraw |= TREDRAW_HARD;
            handled = true;
          }
        }
        break;
      case EVT_CKEY:
        if (event->is_repeat) {
          break;
        }
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
      case EVT_OKEY:
        if (event->is_repeat) {
          break;
        }
        if (t->flag & T_PROP_EDIT && event->shift) {
          t->prop_mode = (t->prop_mode + 1) % PROP_MODE_MAX;
          calculatePropRatio(t);
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case EVT_PADPLUSKEY:
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
      case EVT_PAGEUPKEY:
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
      case EVT_PADMINUS:
        if (event->alt && t->flag & T_PROP_EDIT) {
          t->prop_size /= (t->modifiers & MOD_PRECISION) ? 1.01f : 1.1f;
          calculatePropRatio(t);
          t->redraw = TREDRAW_HARD;
          handled = true;
        }
        break;
      case EVT_PAGEDOWNKEY:
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
      case EVT_LEFTALTKEY:
      case EVT_RIGHTALTKEY:
        if (ELEM(t->spacetype, SPACE_SEQ, SPACE_VIEW3D)) {
          t->flag |= T_ALT_TRANSFORM;
          t->redraw |= TREDRAW_HARD;
          handled = true;
        }
        break;
      case EVT_NKEY:
        if (event->is_repeat) {
          break;
        }
        if (ELEM(t->mode, TFM_ROTATION)) {
          if ((t->flag & T_EDIT) && t->obedit_type == OB_MESH) {
            restoreTransObjects(t);
            resetTransModal(t);
            resetTransRestrictions(t);
            transform_mode_init(t, NULL, TFM_NORMAL_ROTATION);
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
      case EVT_LEFTSHIFTKEY:
      case EVT_RIGHTSHIFTKEY:
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
      case EVT_LEFTALTKEY:
      case EVT_RIGHTALTKEY:
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
    if ((t->flag & T_RELEASE_CONFIRM) && event->type == t->launch_event) {
      t->state = TRANS_CONFIRM;
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

static bool transinfo_show_overlay(const struct bContext *C, TransInfo *t, ARegion *region)
{
  /* Don't show overlays when not the active view and when overlay is disabled: T57139 */
  bool ok = false;
  if (region == t->region) {
    ok = true;
  }
  else {
    ScrArea *area = CTX_wm_area(C);
    if (area->spacetype == SPACE_VIEW3D) {
      View3D *v3d = area->spacedata.first;
      if ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) {
        ok = true;
      }
    }
  }
  return ok;
}

static void drawTransformView(const struct bContext *C, ARegion *region, void *arg)
{
  TransInfo *t = arg;

  if (!transinfo_show_overlay(C, t, region)) {
    return;
  }

  GPU_line_width(1.0f);

  drawConstraint(t);
  drawPropCircle(C, t);
  drawSnapping(C, t);

  if (region == t->region) {
    /* edge slide, vert slide */
    drawEdgeSlide(t);
    drawVertSlide(t);

    /* Rotation */
    drawDial3d(t);
  }
}

/* just draw a little warning message in the top-right corner of the viewport
 * to warn that autokeying is enabled */
static void drawAutoKeyWarning(TransInfo *UNUSED(t), ARegion *region)
{
  const char *printable = IFACE_("Auto Keying On");
  float printable_size[2];
  int xco, yco;

  const rcti *rect = ED_region_visible_rect(region);

  const int font_id = BLF_default();
  BLF_width_and_height(
      font_id, printable, BLF_DRAW_STR_DUMMY_MAX, &printable_size[0], &printable_size[1]);

  xco = (rect->xmax - U.widget_unit) - (int)printable_size[0];
  yco = (rect->ymax - U.widget_unit);

  /* warning text (to clarify meaning of overlays)
   * - original color was red to match the icon, but that clashes badly with a less nasty border
   */
  uchar color[3];
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

static void drawTransformPixel(const struct bContext *C, ARegion *region, void *arg)
{
  TransInfo *t = arg;

  if (!transinfo_show_overlay(C, t, region)) {
    return;
  }

  if (region == t->region) {
    Scene *scene = t->scene;
    ViewLayer *view_layer = t->view_layer;
    Object *ob = OBACT(view_layer);

    /* draw auto-key-framing hint in the corner
     * - only draw if enabled (advanced users may be distracted/annoyed),
     *   for objects that will be autokeyframed (no point otherwise),
     *   AND only for the active region (as showing all is too overwhelming)
     */
    if ((U.autokey_flag & AUTOKEY_FLAG_NOWARNING) == 0) {
      if (region == t->region) {
        if (t->flag & (T_OBJECT | T_POSE)) {
          if (ob && autokeyframe_cfra_can_key(scene, &ob->id)) {
            drawAutoKeyWarning(t, region);
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

  if (!(t->con.mode & CON_APPLY) && (t->flag & T_MODAL) &&
      ELEM(t->mode, TFM_TRANSLATION, TFM_RESIZE)) {
    /* When redoing these modes the first time, it's more convenient to save
     * the Global orientation. */
    mul_m3_v3(t->spacemtx, t->values_final);
    unit_m3(t->spacemtx);

    BLI_assert(t->orient_curr == 0);
    t->orient[0].type = V3D_ORIENT_GLOBAL;
  }

  // Save back mode in case we're in the generic operator
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
        else if ((t->options & CTX_CURSOR) == 0) {
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
  }

  if (t->flag & T_MODAL) {
    /* do we check for parameter? */
    if (transformModeUseSnap(t)) {
      if (!(t->modifiers & MOD_SNAP) != !(ts->snap_flag & SCE_SNAP)) {
        if (t->modifiers & MOD_SNAP) {
          ts->snap_flag |= SCE_SNAP;
        }
        else {
          ts->snap_flag &= ~SCE_SNAP;
        }
        WM_msg_publish_rna_prop(t->mbus, &t->scene->id, ts, ToolSettings, use_snap);
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

  if ((prop = RNA_struct_find_property(op->ptr, "orient_type"))) {
    short orient_type_set, orient_type_curr;
    orient_type_set = RNA_property_is_set(op->ptr, prop) ? RNA_property_enum_get(op->ptr, prop) :
                                                           -1;
    orient_type_curr = t->orient[t->orient_curr].type;

    if (!ELEM(orient_type_curr, orient_type_set, V3D_ORIENT_CUSTOM_MATRIX)) {
      RNA_property_enum_set(op->ptr, prop, orient_type_curr);
      orient_type_set = orient_type_curr;
    }

    if (((prop = RNA_struct_find_property(op->ptr, "orient_matrix_type")) &&
         !RNA_property_is_set(op->ptr, prop))) {
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

  if ((t->options & CTX_SCULPT) && !(t->options & CTX_PAINT_CURVE)) {
    ED_sculpt_end_transform(C);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "correct_uv"))) {
    RNA_property_boolean_set(
        op->ptr, prop, (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT) != 0);
  }
}

static void initSnapSpatial(TransInfo *t, float r_snap[3])
{
  if (t->spacetype == SPACE_VIEW3D) {
    RegionView3D *rv3d = t->region->regiondata;

    if (rv3d) {
      View3D *v3d = t->area->spacedata.first;
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

  if (CTX_wm_view3d(C) != NULL) {
    Object *ob = CTX_data_active_object(C);
    if (ob && ob->mode == OB_MODE_SCULPT && ob->sculpt) {
      options |= CTX_SCULPT;
    }
  }

  t->options = options;

  t->mode = mode;

  /* Needed to translate tweak events to mouse buttons. */
  t->launch_event = event ? WM_userdef_event_type_from_keymap_type(event->type) : -1;
  t->is_launch_event_tweak = event ? ISTWEAK(event->type) : false;

  /* XXX Remove this when wm_operator_call_internal doesn't use window->eventstate
   * (which can have type = 0) */
  /* For gizmo only, so assume LEFTMOUSE. */
  if (t->launch_event == 0) {
    t->launch_event = LEFTMOUSE;
  }

  unit_m3(t->spacemtx);

  initTransInfo(C, t, op, event);

  /* Use the custom orientation when it is set. */
  short orient_index = t->orient[0].type == V3D_ORIENT_CUSTOM_MATRIX ? 0 : t->orient_curr;
  transform_orientations_current_set(t, orient_index);

  if (t->spacetype == SPACE_VIEW3D) {
    t->draw_handle_apply = ED_region_draw_cb_activate(
        t->region->type, drawTransformApply, t, REGION_DRAW_PRE_VIEW);
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->region->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_pixel = ED_region_draw_cb_activate(
        t->region->type, drawTransformPixel, t, REGION_DRAW_POST_PIXEL);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
  }
  else if (t->spacetype == SPACE_IMAGE) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->region->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
  }
  else if (t->spacetype == SPACE_CLIP) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->region->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
  }
  else if (t->spacetype == SPACE_NODE) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->region->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
  }
  else if (t->spacetype == SPACE_GRAPH) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->region->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
  }
  else if (t->spacetype == SPACE_ACTION) {
    t->draw_handle_view = ED_region_draw_cb_activate(
        t->region->type, drawTransformView, t, REGION_DRAW_POST_VIEW);
    t->draw_handle_cursor = WM_paint_cursor_activate(
        SPACE_TYPE_ANY, RGN_TYPE_ANY, transform_draw_cursor_poll, transform_draw_cursor_draw, t);
  }

  createTransData(C, t);  // make TransData structs from selection

  if ((t->options & CTX_SCULPT) && !(t->options & CTX_PAINT_CURVE)) {
    ED_sculpt_init_transform(C);
  }

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
          if ((ELEM(kmi->type, EVT_LEFTCTRLKEY, EVT_RIGHTCTRLKEY) && event->ctrl) ||
              (ELEM(kmi->type, EVT_LEFTSHIFTKEY, EVT_RIGHTSHIFTKEY) && event->shift) ||
              (ELEM(kmi->type, EVT_LEFTALTKEY, EVT_RIGHTALTKEY) && event->alt) ||
              ((kmi->type == EVT_OSKEY) && event->oskey)) {
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

  transform_mode_init(t, op, mode);

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
  if (t->con.mode & CON_APPLY) {
    setUserConstraint(t, t->orient[t->orient_curr].type, t->con.mode, "%s");
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
              BKE_editmesh_lnorspace_update(em, tc->obedit->data);
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

static void drawTransformApply(const bContext *C, ARegion *UNUSED(region), void *arg)
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

    /* Free data, also handles overlap [in freeTransCustomData()]. */
    postTrans(C, t);

    /* send events out for redraws */
    viewRedrawPost(C, t);

    viewRedrawForce(C, t);
  }

  t->context = NULL;

  return exit_code;
}

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
