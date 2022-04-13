/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BLI_compiler_compat.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h"
#include "BKE_material.h"
#include "BKE_paint.h"

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

extern "C" {

using namespace blender::bke::paint::canvas;

Image *BKE_paint_canvas_image_get(const struct PaintModeSettings *settings, struct Object *ob)
{
  switch (settings->canvas_source) {
    case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE:
      return nullptr;
    case PAINT_CANVAS_SOURCE_IMAGE:
      return settings->canvas_image;
    case PAINT_CANVAS_SOURCE_MATERIAL: {
      TexPaintSlot *slot = get_active_slot(ob);
      if (slot == nullptr) {
        break;
      }
      return slot->ima;
    }
  }
  return nullptr;
}

int BKE_paint_canvas_uvmap_layer_index_get(const struct PaintModeSettings *settings,
                                           struct Object *ob)
{
  switch (settings->canvas_source) {
    case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE:
      return -1;
    case PAINT_CANVAS_SOURCE_IMAGE: {
      /* Use active uv map of the object. */
      if (ob->type != OB_MESH) {
        return -1;
      }

      const Mesh *mesh = static_cast<Mesh *>(ob->data);
      return CustomData_get_active_layer_index(&mesh->ldata, CD_MLOOPUV);
    }
    case PAINT_CANVAS_SOURCE_MATERIAL: {
      /* Use uv map of the canvas. */
      TexPaintSlot *slot = get_active_slot(ob);
      if (slot == nullptr) {
        break;
      }

      if (ob->type != OB_MESH) {
        return -1;
      }

      if (slot->uvname == nullptr) {
        return -1;
      }

      const Mesh *mesh = static_cast<Mesh *>(ob->data);
      return CustomData_get_named_layer_index(&mesh->ldata, CD_MLOOPUV, slot->uvname);
    }
  }
  return -1;
}
}
