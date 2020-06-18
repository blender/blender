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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spimage
 */

#include "DNA_brush_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"

#include "IMB_imbuf_types.h"

#include "DEG_depsgraph.h"

#include "ED_image.h" /* own include */
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_uvedit.h"

#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

/* note; image_panel_properties() uses pointer to sima->image directly */
Image *ED_space_image(SpaceImage *sima)
{
  return sima->image;
}

void ED_space_image_set(Main *bmain, SpaceImage *sima, Object *obedit, Image *ima, bool automatic)
{
  /* Automatically pin image when manually assigned, otherwise it follows object. */
  if (!automatic && sima->image != ima && sima->mode == SI_MODE_UV) {
    sima->pin = true;
  }

  /* change the space ima after because uvedit_face_visible_test uses the space ima
   * to check if the face is displayed in UV-localview */
  sima->image = ima;

  if (ima == NULL || ima->type == IMA_TYPE_R_RESULT || ima->type == IMA_TYPE_COMPOSITE) {
    if (sima->mode == SI_MODE_PAINT) {
      sima->mode = SI_MODE_VIEW;
    }
  }

  if (sima->image) {
    BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
  }

  id_us_ensure_real((ID *)sima->image);

  if (obedit) {
    WM_main_add_notifier(NC_GEOM | ND_DATA, obedit->data);
  }

  WM_main_add_notifier(NC_SPACE | ND_SPACE_IMAGE, NULL);
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
  if (efa == NULL) {
    return;
  }

  Image *ima = NULL;
  ED_object_get_active_image(ob, efa->mat_nr + 1, &ima, NULL, NULL, NULL);

  if (ima != sima->image) {
    sima->image = ima;

    if (sima->image) {
      Main *bmain = CTX_data_main(C);
      BKE_image_signal(bmain, sima->image, &sima->iuser, IMA_SIGNAL_USER_NEW_IMAGE);
    }
  }
}

Mask *ED_space_image_get_mask(SpaceImage *sima)
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
#if 0
    if (sima->image->type == IMA_TYPE_R_RESULT && BIF_show_render_spare()) {
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
      if (ibuf->rect || ibuf->rect_float) {
        return ibuf;
      }
      BKE_image_release_ibuf(sima->image, ibuf, *r_lock);
      *r_lock = NULL;
    }
  }
  else {
    *r_lock = NULL;
  }

  return NULL;
}

void ED_space_image_release_buffer(SpaceImage *sima, ImBuf *ibuf, void *lock)
{
  if (sima && sima->image) {
    BKE_image_release_ibuf(sima->image, ibuf, lock);
  }
}

/* Get the SpaceImage flag that is valid for the given ibuf. */
int ED_space_image_get_display_channel_mask(ImBuf *ibuf)
{
  int result = (SI_USE_ALPHA | SI_SHOW_ALPHA | SI_SHOW_ZBUF | SI_SHOW_R | SI_SHOW_G | SI_SHOW_B);
  if (!ibuf) {
    return result;
  }

  const bool color = ibuf->channels >= 3;
  const bool alpha = ibuf->channels == 4;
  const bool zbuf = ibuf->zbuf || ibuf->zbuf_float || (ibuf->channels == 1);

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
  has_buffer = (ibuf != NULL);
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
    *r_width = (scene->r.xsch * scene->r.size) / 100;
    *r_height = (scene->r.ysch * scene->r.size) / 100;

    if ((scene->r.mode & R_BORDER) && (scene->r.mode & R_CROP)) {
      *r_width *= BLI_rctf_size_x(&scene->r.border);
      *r_height *= BLI_rctf_size_y(&scene->r.border);
    }
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
  if ((ima == NULL) || (ima->aspx == 0.0f || ima->aspy == 0.0f)) {
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

  *r_zoomx = (float)(BLI_rcti_size_x(&region->winrct) + 1) /
             (float)(BLI_rctf_size_x(&region->v2d.cur) * width);
  *r_zoomy = (float)(BLI_rcti_size_y(&region->winrct) + 1) /
             (float)(BLI_rctf_size_y(&region->v2d.cur) * height);
}

void ED_space_image_get_uv_aspect(SpaceImage *sima, float *r_aspx, float *r_aspy)
{
  int w, h;

  ED_space_image_get_aspect(sima, r_aspx, r_aspy);
  ED_space_image_get_size(sima, &w, &h);

  *r_aspx *= (float)w;
  *r_aspy *= (float)h;

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

    *r_aspx *= (float)w;
    *r_aspy *= (float)h;
  }
  else {
    *r_aspx = 1.0f;
    *r_aspy = 1.0f;
  }
}

/* takes event->mval */
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

  r_co[0] = (co[0] * width * zoomx) + (float)sx;
  r_co[1] = (co[1] * height * zoomy) + (float)sy;
}

/**
 * This is more a user-level functionality, for going to next/prev used slot,
 * Stepping onto the last unused slot too.
 */
bool ED_image_slot_cycle(struct Image *image, int direction)
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

  if (i == num_slots) {
    image->render_slot = ((cur == 1) ? 0 : 1);
  }

  return (cur != image->render_slot);
}

void ED_space_image_scopes_update(const struct bContext *C,
                                  struct SpaceImage *sima,
                                  struct ImBuf *ibuf,
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
    if (image != NULL && (image->type == IMA_TYPE_R_RESULT || image->type == IMA_TYPE_COMPOSITE)) {
      return;
    }
  }

  BKE_scopes_update(&sima->scopes,
                    ibuf,
                    use_view_settings ? &scene->view_settings : NULL,
                    &scene->display_settings);
}

bool ED_space_image_show_render(SpaceImage *sima)
{
  return (sima->image && ELEM(sima->image->type, IMA_TYPE_R_RESULT, IMA_TYPE_COMPOSITE));
}

bool ED_space_image_show_paint(SpaceImage *sima)
{
  if (ED_space_image_show_render(sima)) {
    return false;
  }

  return (sima->mode == SI_MODE_PAINT);
}

bool ED_space_image_show_uvedit(SpaceImage *sima, Object *obedit)
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
    struct BMEditMesh *em = BKE_editmesh_from_object(obedit);
    bool ret;

    ret = EDBM_uv_check(em);

    return ret;
  }

  return false;
}

/* matches clip function */
bool ED_space_image_check_show_maskedit(SpaceImage *sima, ViewLayer *view_layer)
{
  /* check editmode - this is reserved for UV editing */
  Object *ob = OBACT(view_layer);
  if (ob && ob->mode & OB_MODE_EDIT && ED_space_image_show_uvedit(sima, ob)) {
    return false;
  }

  return (sima->mode == SI_MODE_MASK);
}

bool ED_space_image_maskedit_poll(bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    return ED_space_image_check_show_maskedit(sima, view_layer);
  }

  return false;
}

bool ED_space_image_paint_curve(const bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);

  if (sima && sima->mode == SI_MODE_PAINT) {
    Brush *br = CTX_data_tool_settings(C)->imapaint.paint.brush;

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
    return sima->mask_info.mask != NULL;
  }

  return false;
}

bool ED_space_image_cursor_poll(bContext *C)
{
  return ED_operator_uvedit_space_image(C) || ED_space_image_maskedit_poll(C) ||
         ED_space_image_paint_curve(C);
}
