/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_compiler_compat.h"

#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_workspace_types.h"

#include "BKE_material.h"
#include "BKE_paint.hh"

#include "WM_toolsystem.h"

#include "ED_paint.h"

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

using namespace blender::ed::sculpt_paint::canvas;

/* Does the paint tool with the given idname uses a canvas. */
static bool paint_tool_uses_canvas(blender::StringRef idname)
{
  return ELEM(idname, "builtin_brush.Paint", "builtin_brush.Smear", "builtin.color_filter");
}

static bool paint_tool_shading_color_follows_last_used(blender::StringRef idname)
{
  /* TODO(jbakker): complete this list. */
  return ELEM(idname, "builtin_brush.Mask");
}

void ED_paint_tool_update_sticky_shading_color(bContext *C, Object *ob)
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

static bool paint_tool_shading_color_follows_last_used_tool(bContext *C, Object *ob)
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

bool ED_paint_tool_use_canvas(bContext *C, bToolRef *tref)
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
        ob->sculpt->sticky_shading_color))
  {
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
}
