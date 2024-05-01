/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 */

#include "DNA_brush_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_paint.hh"
#include "BKE_scene.hh"

#include "IMB_imbuf_types.hh"

#include "ED_image.hh" /* own include */
#include "ED_mesh.hh"
#include "ED_screen.hh"
#include "ED_uvedit.hh"

#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

Image *ED_space_image(const SpaceImage *sima)
{
  /* NOTE: image_panel_properties() uses pointer to `sima->image` directly. */
  return sima->image;
}

void ED_space_image_set(Main *bmain, SpaceImage *sima, Image *ima, bool automatic)
{
  /* Automatically pin image when manually assigned, otherwise it follows object. */
  if (!automatic && sima->image != ima && sima->mode == SI_MODE_UV) {
    sima->pin = true;
  }

  sima->image = ima;

  if (ima == nullptr || ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE) {
    if (sima->mode == SI_MODE_PAINT) {
      sima->mode = SI_MODE_VIEW;
    }
  }

  if (sima->image) {
    BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
  }

  id_us_ensure_real((ID *)sima->image);

  WM_main_add_notifier(NC_SPACE | ND_SPACE_IMAGE, nullptr);
}

void ED_space_image_sync(Main *bmain, Image *image, bool ignore_render_viewer)
{
  wmWindowManager *wm = (wmWindowManager *)bmain->wm.first;
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_IMAGE) {
          continue;
        }
        SpaceImage *sima = (SpaceImage *)sl;
        if (sima->pin) {
          continue;
        }
        if (ignore_render_viewer && sima->image &&
            ELEM(sima->image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE))
        {
          continue;
        }
        ED_space_image_set(bmain, sima, image, true);
      }
    }
  }
}

void ED_space_image_auto_set(const bContext *C, SpaceImage *sima)
{
  if (sima->mode != SI_MODE_UV || sima->pin) {
    return;
  }

  /* Track image assigned to active face in edit mode. */
  Object *ob = CTX_data_active_object(C);
  if (!(ob && (ob->mode & OB_MODE_EDIT) && ED_space_image_show_uvedit(sima, ob))) {
    return;
  }

  BMEditMesh *em = BKE_editmesh_from_object(ob);
  BMesh *bm = em->bm;
  BMFace *efa = BM_mesh_active_face_get(bm, true, false);
  if (efa == nullptr) {
    return;
  }

  Image *ima = nullptr;
  ED_object_get_active_image(ob, efa->mat_nr + 1, &ima, nullptr, nullptr, nullptr);

  if (ima != sima->image) {
    sima->image = ima;

    if (sima->image) {
      Main *bmain = CTX_data_main(C);
      BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
    }
  }
}

Mask *ED_space_image_get_mask(const SpaceImage *sima)
{
  return sima->mask_info.mask;
}

void ED_space_image_set_mask(bContext *C, SpaceImage *sima, Mask *mask)
{
  sima->mask_info.mask = mask;

  /* weak, but same as image/space */
  id_us_ensure_real((ID *)sima->mask_info.mask);

  if (C) {
    WM_event_add_notifier(C, NC_MASK | NA_SELECTED, mask);
  }
}

ImBuf *ED_space_image_acquire_buffer(SpaceImage *sima, void **r_lock, int tile)
{
  ImBuf *ibuf;

  if (sima && sima->image) {
    const Image *image = sima->image;

#if 0
    if (image->type == IMA_TYPE_R_RESULT && BIF_show_render_spare()) {
      return BIF_render_spare_imbuf();
    }
    else
#endif
    {
      sima->iuser.tile = tile;
      ibuf = BKE_image_acquire_ibuf(sima->image, &sima->iuser, r_lock);
      sima->iuser.tile = 0;
    }

    if (ibuf) {
      if (image->type == IMA_TYPE_R_RESULT && ibuf->x != 0 && ibuf->y != 0) {
        /* Render result might be lazily allocated. Return ibuf without buffers to indicate that
         * there is image buffer but it has no data yet. */
        return ibuf;
      }

      if (ibuf->byte_buffer.data || ibuf->float_buffer.data) {
        return ibuf;
      }
      BKE_image_release_ibuf(sima->image, ibuf, *r_lock);
      *r_lock = nullptr;
    }
  }
  else {
    *r_lock = nullptr;
  }

  return nullptr;
}

void ED_space_image_release_buffer(SpaceImage *sima, ImBuf *ibuf, void *lock)
{
  if (sima && sima->image) {
    BKE_image_release_ibuf(sima->image, ibuf, lock);
  }
}

int ED_space_image_get_display_channel_mask(ImBuf *ibuf)
{
  int result = (SI_USE_ALPHA | SI_SHOW_ALPHA | SI_SHOW_ZBUF | SI_SHOW_R | SI_SHOW_G | SI_SHOW_B);
  if (!ibuf) {
    return result;
  }

  const bool color = ibuf->channels >= 3;
  const bool alpha = ibuf->channels == 4;
  const bool zbuf = ibuf->channels == 1;

  if (!alpha) {
    result &= ~(SI_USE_ALPHA | SI_SHOW_ALPHA);
  }
  if (!zbuf) {
    result &= ~SI_SHOW_ZBUF;
  }
  if (!color) {
    result &= ~(SI_SHOW_R | SI_SHOW_G | SI_SHOW_B);
  }
  return result;
}

bool ED_space_image_has_buffer(SpaceImage *sima)
{
  ImBuf *ibuf;
  void *lock;
  bool has_buffer;

  ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);
  has_buffer = (ibuf != nullptr);
  ED_space_image_release_buffer(sima, ibuf, lock);

  return has_buffer;
}

void ED_space_image_get_size(SpaceImage *sima, int *r_width, int *r_height)
{
  Scene *scene = sima->iuser.scene;
  ImBuf *ibuf;
  void *lock;

  /* TODO(lukas): Support tiled images with different sizes */
  ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);

  if (ibuf && ibuf->x > 0 && ibuf->y > 0) {
    *r_width = ibuf->x;
    *r_height = ibuf->y;
  }
  else if (sima->image && sima->image->type == IMA_TYPE_R_RESULT && scene) {
    /* not very important, just nice */
    BKE_render_resolution(&scene->r, true, r_width, r_height);
  }
  /* I know a bit weak... but preview uses not actual image size */
  // XXX else if (image_preview_active(sima, r_width, r_height));
  else {
    *r_width = IMG_SIZE_FALLBACK;
    *r_height = IMG_SIZE_FALLBACK;
  }

  ED_space_image_release_buffer(sima, ibuf, lock);
}

void ED_space_image_get_size_fl(SpaceImage *sima, float r_size[2])
{
  int size_i[2];
  ED_space_image_get_size(sima, &size_i[0], &size_i[1]);
  r_size[0] = size_i[0];
  r_size[1] = size_i[1];
}

void ED_space_image_get_aspect(SpaceImage *sima, float *r_aspx, float *r_aspy)
{
  Image *ima = sima->image;
  if ((ima == nullptr) || (ima->aspx == 0.0f || ima->aspy == 0.0f)) {
    *r_aspx = *r_aspy = 1.0;
  }
  else {
    BKE_image_get_aspect(ima, r_aspx, r_aspy);
  }
}

void ED_space_image_get_zoom(SpaceImage *sima,
                             const ARegion *region,
                             float *r_zoomx,
                             float *r_zoomy)
{
  int width, height;

  ED_space_image_get_size(sima, &width, &height);

  *r_zoomx = float(BLI_rcti_size_x(&region->winrct) + 1) /
             float(BLI_rctf_size_x(&region->v2d.cur) * width);
  *r_zoomy = float(BLI_rcti_size_y(&region->winrct) + 1) /
             float(BLI_rctf_size_y(&region->v2d.cur) * height);
}

void ED_space_image_get_uv_aspect(SpaceImage *sima, float *r_aspx, float *r_aspy)
{
  int w, h;

  ED_space_image_get_aspect(sima, r_aspx, r_aspy);
  ED_space_image_get_size(sima, &w, &h);

  *r_aspx *= float(w);
  *r_aspy *= float(h);

  if (*r_aspx < *r_aspy) {
    *r_aspy = *r_aspy / *r_aspx;
    *r_aspx = 1.0f;
  }
  else {
    *r_aspx = *r_aspx / *r_aspy;
    *r_aspy = 1.0f;
  }
}

void ED_image_get_uv_aspect(Image *ima, ImageUser *iuser, float *r_aspx, float *r_aspy)
{
  if (ima) {
    int w, h;

    BKE_image_get_aspect(ima, r_aspx, r_aspy);
    BKE_image_get_size(ima, iuser, &w, &h);

    *r_aspx *= float(w);
    *r_aspy *= float(h);
  }
  else {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;
  }
}

void ED_image_mouse_pos(SpaceImage *sima, const ARegion *region, const int mval[2], float co[2])
{
  int sx, sy, width, height;
  float zoomx, zoomy;

  ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);
  ED_space_image_get_size(sima, &width, &height);

  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &sx, &sy);

  co[0] = ((mval[0] - sx) / zoomx) / width;
  co[1] = ((mval[1] - sy) / zoomy) / height;
}

void ED_image_view_center_to_point(SpaceImage *sima, float x, float y)
{
  int width, height;
  float aspx, aspy;

  ED_space_image_get_size(sima, &width, &height);
  ED_space_image_get_aspect(sima, &aspx, &aspy);

  sima->xof = (x - 0.5f) * width * aspx;
  sima->yof = (y - 0.5f) * height * aspy;
}

void ED_image_point_pos(
    SpaceImage *sima, const ARegion *region, float x, float y, float *r_x, float *r_y)
{
  int sx, sy, width, height;
  float zoomx, zoomy;

  ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);
  ED_space_image_get_size(sima, &width, &height);

  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &sx, &sy);

  *r_x = ((x - sx) / zoomx) / width;
  *r_y = ((y - sy) / zoomy) / height;
}

void ED_image_point_pos__reverse(SpaceImage *sima,
                                 const ARegion *region,
                                 const float co[2],
                                 float r_co[2])
{
  float zoomx, zoomy;
  int width, height;
  int sx, sy;

  UI_view2d_view_to_region(&region->v2d, 0.0f, 0.0f, &sx, &sy);
  ED_space_image_get_size(sima, &width, &height);
  ED_space_image_get_zoom(sima, region, &zoomx, &zoomy);

  r_co[0] = (co[0] * width * zoomx) + float(sx);
  r_co[1] = (co[1] * height * zoomy) + float(sy);
}

bool ED_image_slot_cycle(Image *image, int direction)
{
  const int cur = image->render_slot;
  int i, slot;

  BLI_assert(ELEM(direction, -1, 1));

  int num_slots = BLI_listbase_count(&image->renderslots);
  for (i = 1; i < num_slots; i++) {
    slot = (cur + ((direction == -1) ? -i : i)) % num_slots;
    if (slot < 0) {
      slot += num_slots;
    }

    RenderSlot *render_slot = BKE_image_get_renderslot(image, slot);
    if ((render_slot && render_slot->render) || slot == image->last_render_slot) {
      image->render_slot = slot;
      break;
    }
  }

  if (num_slots == 1) {
    image->render_slot = 0;
  }
  else if (i == num_slots) {
    image->render_slot = ((cur == 1) ? 0 : 1);
  }

  if (cur != image->render_slot) {
    BKE_image_partial_update_mark_full_update(image);
  }
  return (cur != image->render_slot);
}

void ED_space_image_scopes_update(const bContext *C,
                                  SpaceImage *sima,
                                  ImBuf *ibuf,
                                  bool use_view_settings)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  /* scope update can be expensive, don't update during paint modes */
  if (sima->mode == SI_MODE_PAINT) {
    return;
  }
  if (ob && ((ob->mode & (OB_MODE_TEXTURE_PAINT | OB_MODE_EDIT)) != 0)) {
    return;
  }

  /* We also don't update scopes of render result during render. */
  if (G.is_rendering) {
    const Image *image = sima->image;
    if (image != nullptr && ELEM(image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE)) {
      return;
    }
  }

  BKE_scopes_update(&sima->scopes,
                    ibuf,
                    use_view_settings ? &scene->view_settings : nullptr,
                    &scene->display_settings);
}

bool ED_space_image_show_render(const SpaceImage *sima)
{
  return (sima->image && ELEM(sima->image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE));
}

bool ED_space_image_show_paint(const SpaceImage *sima)
{
  if (ED_space_image_show_render(sima)) {
    return false;
  }

  return (sima->mode == SI_MODE_PAINT);
}

bool ED_space_image_show_uvedit(const SpaceImage *sima, Object *obedit)
{
  if (sima) {
    if (ED_space_image_show_render(sima)) {
      return false;
    }
    if (sima->mode != SI_MODE_UV) {
      return false;
    }
  }

  if (obedit && obedit->type == OB_MESH) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool ret;

    ret = EDBM_uv_check(em);

    return ret;
  }

  return false;
}

bool ED_space_image_check_show_maskedit(SpaceImage *sima, Object *obedit)
{
  /* check editmode - this is reserved for UV editing */
  if (obedit && ED_space_image_show_uvedit(sima, obedit)) {
    return false;
  }

  return (sima->mode == SI_MODE_MASK);
}

bool ED_space_image_maskedit_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima) {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *obedit = BKE_view_layer_edit_object_get(view_layer);
    return ED_space_image_check_show_maskedit(sima, obedit);
  }

  return false;
}

bool ED_space_image_maskedit_visible_splines_poll(bContext *C)
{
  if (!ED_space_image_maskedit_poll(C)) {
    return false;
  }

  const SpaceImage *space_image = CTX_wm_space_image(C);
  return space_image->mask_info.draw_flag & MASK_DRAWFLAG_SPLINE;
}

bool ED_space_image_paint_curve(const bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima && sima->mode == SI_MODE_PAINT) {
    Brush *br = BKE_paint_brush(&CTX_data_tool_settings(C)->imapaint.paint);

    if (br && (br->flag & BRUSH_CURVE)) {
      return true;
    }
  }

  return false;
}

bool ED_space_image_maskedit_mask_poll(bContext *C)
{
  if (ED_space_image_maskedit_poll(C)) {
    SpaceImage *sima = CTX_wm_space_image(C);
    return sima->mask_info.mask != nullptr;
  }

  return false;
}

bool ED_space_image_maskedit_mask_visible_splines_poll(bContext *C)
{
  if (!ED_space_image_maskedit_mask_poll(C)) {
    return false;
  }

  const SpaceImage *space_image = CTX_wm_space_image(C);
  return space_image->mask_info.draw_flag & MASK_DRAWFLAG_SPLINE;
}

bool ED_space_image_cursor_poll(bContext *C)
{
  return ED_operator_uvedit_space_image(C) || ED_space_image_maskedit_poll(C) ||
         ED_space_image_paint_curve(C);
}
