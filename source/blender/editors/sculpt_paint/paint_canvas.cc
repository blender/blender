/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "BLI_compiler_compat.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_workspace_types.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "DEG_depsgraph.h"

#include "NOD_shader.h"

#include "WM_toolsystem.h"

namespace blender::ed::sculpt_paint::canvas {
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

}  // namespace blender::ed::sculpt_paint::canvas

extern "C" {

using namespace blender;
using namespace blender::ed::sculpt_paint::canvas;

/* Does the paint tool with the given idname uses a canvas. */
static bool paint_tool_uses_canvas(StringRef idname)
{
  return ELEM(idname, "builtin_brush.Paint", "builtin_brush.Smear", "builtin.color_filter");
}

static bool paint_tool_shading_color_follows_last_used(StringRef idname)
{
  /* TODO(jbakker): complete this list. */
  return ELEM(idname, "builtin_brush.Mask");
}

void ED_paint_tool_update_sticky_shading_color(struct bContext *C, struct Object *ob)
{
  if (ob == nullptr || ob->sculpt == nullptr) {
    return;
  }

  bToolRef *tref = WM_toolsystem_ref_from_context(C);
  if (tref == nullptr) {
    return;
  }
  /* Do not modify when tool follows lat used tool. */
  if (paint_tool_shading_color_follows_last_used(tref->idname)) {
    return;
  }

  ob->sculpt->sticky_shading_color = paint_tool_uses_canvas(tref->idname);
}

static bool paint_tool_shading_color_follows_last_used_tool(struct bContext *C, struct Object *ob)
{
  if (ob == nullptr || ob->sculpt == nullptr) {
    return false;
  }

  bToolRef *tref = WM_toolsystem_ref_from_context(C);
  if (tref == nullptr) {
    return false;
  }

  return paint_tool_shading_color_follows_last_used(tref->idname);
}

bool ED_paint_tool_use_canvas(struct bContext *C, bToolRef *tref)
{
  if (tref == nullptr) {
    tref = WM_toolsystem_ref_from_context(C);
  }
  if (tref == nullptr) {
    return false;
  }

  return paint_tool_uses_canvas(tref->idname);
}

eV3DShadingColorType ED_paint_shading_color_override(bContext *C,
                                                     const PaintModeSettings *settings,
                                                     Object *ob,
                                                     eV3DShadingColorType orig_color_type)
{
  if (!U.experimental.use_sculpt_texture_paint) {
    return orig_color_type;
  }
  /* NOTE: This early exit is temporarily, until a paint mode has been added.
   * For better integration with the vertex paint in sculpt mode we sticky
   * with the last stoke when using tools like masking.
   */
  if (!ED_paint_tool_use_canvas(C, nullptr) &&
      !(paint_tool_shading_color_follows_last_used_tool(C, ob) &&
        ob->sculpt->sticky_shading_color)) {
    return orig_color_type;
  }

  eV3DShadingColorType color_type = orig_color_type;
  switch (settings->canvas_source) {
    case PAINT_CANVAS_SOURCE_COLOR_ATTRIBUTE:
      color_type = V3D_SHADING_VERTEX_COLOR;
      break;
    case PAINT_CANVAS_SOURCE_IMAGE:
      color_type = V3D_SHADING_TEXTURE_COLOR;
      break;
    case PAINT_CANVAS_SOURCE_MATERIAL: {
      TexPaintSlot *slot = get_active_slot(ob);
      if (slot == nullptr) {
        break;
      }

      if (slot->ima) {
        color_type = V3D_SHADING_TEXTURE_COLOR;
      }
      if (slot->attribute_name) {
        color_type = V3D_SHADING_VERTEX_COLOR;
      }

      break;
    }
  }

  return color_type;
}

Image *ED_paint_canvas_image_get(const struct PaintModeSettings *settings, struct Object *ob)
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

int ED_paint_canvas_uvmap_layer_index_get(const struct PaintModeSettings *settings,
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
