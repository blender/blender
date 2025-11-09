/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_image.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_paint.hh"

#include "IMB_imbuf_types.hh"

#include <sstream>

namespace blender::bke::paint::canvas {
static TexPaintSlot *get_active_slot(Object *ob)
{
  Material *mat = BKE_object_material_get(ob, ob->actcol);
  if (mat == nullptr) {
    return nullptr;
  }
  if (mat->texpaintslot == nullptr) {
    return nullptr;
  }
  if (mat->paint_active_slot >= mat->tot_slots) {
    return nullptr;
  }

  TexPaintSlot *slot = &mat->texpaintslot[mat->paint_active_slot];
  return slot;
}

}  // namespace blender::bke::paint::canvas

using namespace blender::bke::paint::canvas;

bool BKE_paint_canvas_image_get(PaintModeSettings *settings,
                                Object *ob,
                                Image **r_image,
                                ImageUser **r_image_user)
{
  *r_image = nullptr;
  *r_image_user = nullptr;

  switch (settings->canvas_source) {
    case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE:
      break;

    case PAINT_CANVAS_SOURCE_IMAGE:
      *r_image = settings->canvas_image;
      *r_image_user = &settings->image_user;
      break;

    case PAINT_CANVAS_SOURCE_MATERIAL: {
      TexPaintSlot *slot = get_active_slot(ob);
      if (slot == nullptr) {
        break;
      }

      *r_image = slot->ima;
      *r_image_user = slot->image_user;
      break;
    }
  }
  return *r_image != nullptr;
}

static bool has_uv_map_attribute(const Mesh &mesh, const blender::StringRef name)
{
  using namespace blender;
  return bke::mesh::is_uv_map(mesh.attributes().lookup_meta_data(name));
}

std::optional<blender::StringRef> BKE_paint_canvas_uvmap_name_get(
    const PaintModeSettings *settings, Object *ob)
{
  switch (settings->canvas_source) {
    case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE:
      return std::nullopt;
    case PAINT_CANVAS_SOURCE_IMAGE: {
      /* Use active uv map of the object. */
      if (ob->type != OB_MESH) {
        return std::nullopt;
      }

      const Mesh *mesh = static_cast<Mesh *>(ob->data);
      if (!has_uv_map_attribute(*mesh, mesh->active_uv_map_name())) {
        return std::nullopt;
      }
      return mesh->active_uv_map_name();
    }
    case PAINT_CANVAS_SOURCE_MATERIAL: {
      /* Use uv map of the canvas. */
      TexPaintSlot *slot = get_active_slot(ob);
      if (slot == nullptr) {
        break;
      }

      if (ob->type != OB_MESH) {
        return std::nullopt;
      }

      if (slot->uvname == nullptr) {
        return std::nullopt;
      }

      const Mesh *mesh = static_cast<Mesh *>(ob->data);
      if (!has_uv_map_attribute(*mesh, slot->uvname)) {
        return std::nullopt;
      }
      return slot->uvname;
    }
  }
  return std::nullopt;
}

char *BKE_paint_canvas_key_get(PaintModeSettings *settings, Object *ob)
{
  std::stringstream ss;
  ss << "UV_MAP:" << BKE_paint_canvas_uvmap_name_get(settings, ob).value_or("");

  Image *image;
  ImageUser *image_user;
  if (BKE_paint_canvas_image_get(settings, ob, &image, &image_user)) {
    ss << ",SEAM_MARGIN:" << image->seam_margin;
    ImageUser tile_user = *image_user;
    LISTBASE_FOREACH (ImageTile *, image_tile, &image->tiles) {
      tile_user.tile = image_tile->tile_number;
      ImBuf *image_buffer = BKE_image_acquire_ibuf(image, &tile_user, nullptr);
      if (!image_buffer) {
        continue;
      }
      ss << ",TILE_" << image_tile->tile_number;
      ss << "(" << image_buffer->x << "," << image_buffer->y << ")";
      BKE_image_release_ibuf(image, image_buffer, nullptr);
    }
  }

  return BLI_strdup(ss.str().c_str());
}
