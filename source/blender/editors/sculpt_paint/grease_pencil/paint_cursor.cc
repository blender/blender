/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "editors/sculpt_paint/paint_cursor.hh"

#include "DNA_grease_pencil_types.h"

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"

#include "BKE_brush.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint_types.hh"

#include "IMB_colormanagement.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_state.hh"

#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint {
static int project_brush_radius_grease_pencil(ViewContext *vc,
                                              const float radius,
                                              const float3 world_location,
                                              const float4x4 &to_world)
{
  const float2 xy_delta = float2(1.0f, 0.0f);

  bool z_flip;
  const float zfac = ED_view3d_calc_zfac_ex(vc->rv3d, world_location, &z_flip);
  if (z_flip) {
    /* Location is behind camera. Return 0 to make the cursor disappear. */
    return 0;
  }
  float3 delta;
  ED_view3d_win_to_delta(vc->region, xy_delta, zfac, delta);

  const float scale = math::length(
      math::transform_direction(to_world, float3(std::numbers::inv_sqrt3)));
  return math::safe_divide(scale * radius, math::length(delta));
}

static void grease_pencil_eraser_draw(PaintCursorContext &pcontext)
{
  float radius = float(pcontext.pixel_radius);

  /* Red-ish color with alpha. */
  immUniformColor4ub(255, 100, 100, 20);
  imm_draw_circle_fill_2d(pcontext.pos, pcontext.mval.x, pcontext.mval.y, radius, 40);

  immUnbindProgram();

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2], viewport_size[3]);

  immUniformColor4f(1.0f, 0.39f, 0.39f, 0.78f);
  immUniform1i("colors_len", 0); /* "simple" mode */
  immUniform1f("dash_width", 12.0f);
  immUniform1f("udash_factor", 0.5f);

  /* XXX Dashed shader gives bad results with sets of small segments
   * currently, temp hack around the issue. :( */
  const int nsegments = max_ii(8, radius / 2);
  imm_draw_circle_wire_2d(pcontext.pos, pcontext.mval.x, pcontext.mval.y, radius, nsegments);
}

void grease_pencil_cursor_draw(PaintCursorContext &pcontext)
{
  if (pcontext.region &&
      !BLI_rcti_isect_pt(&pcontext.region->winrct, pcontext.mval.x, pcontext.mval.y))
  {
    return;
  }

  Object *object = pcontext.object;
  if (object->type != OB_GREASE_PENCIL) {
    return;
  }

  GreasePencil *grease_pencil = id_cast<GreasePencil *>(object->data);
  Paint *paint = pcontext.paint;
  Brush *brush = pcontext.brush;
  if ((brush == nullptr) || (brush->gpencil_settings == nullptr)) {
    return;
  }

  float3 color(1.0f);
  const int2 mval = pcontext.mval;

  if (pcontext.mode == PaintMode::GPencil) {
    /* Hide the cursor while drawing. */
    if (grease_pencil->runtime->is_drawing_stroke) {
      return;
    }

    /* Eraser has a special shape and uses a different shader program. */
    if (brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_ERASE ||
        grease_pencil->runtime->temp_use_eraser)
    {
      /* If we use the eraser from the draw tool with a "scene" radius unit, we need to draw the
       * cursor with the appropriate size. */
      if (grease_pencil->runtime->temp_use_eraser && (brush->flag & BRUSH_LOCK_SIZE) != 0) {
        pcontext.pixel_radius = std::max(int(grease_pencil->runtime->temp_eraser_size / 2.0f), 1);
      }
      else {
        pcontext.pixel_radius = std::max(1, int(brush->size / 2.0f));
      }
      grease_pencil_eraser_draw(pcontext);
      return;
    }

    if (brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_FILL) {
      /* The fill tool doesn't use a brush size currently, but not showing any brush means that it
       * can be hard to see where the cursor is. Use a fixed size that's not too big (10px). By
       * disabling the "Display Cursor" option, this can still be turned off. */
      pcontext.pixel_radius = 10;
    }

    if (brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_TINT) {
      pcontext.pixel_radius = std::max(int(brush->size / 2.0f), 1);
    }

    if (brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_DRAW) {
      if ((brush->flag & BRUSH_LOCK_SIZE) != 0) {
        const bke::greasepencil::Layer *layer = grease_pencil->get_active_layer();
        const ed::greasepencil::DrawingPlacement placement(
            *pcontext.scene, *pcontext.region, *pcontext.vc.v3d, *object, layer);
        const float2 coordinate = float2(pcontext.mval.x - pcontext.region->winrct.xmin,
                                         pcontext.mval.y - pcontext.region->winrct.ymin);
        bool clipped = false;
        const float3 pos = placement.project(coordinate, clipped);
        if (!clipped) {
          const float3 world_location = math::transform_point(placement.to_world_space(), pos);
          pcontext.pixel_radius = project_brush_radius_grease_pencil(&pcontext.vc,
                                                                     brush->unprojected_size /
                                                                         2.0f,
                                                                     world_location,
                                                                     placement.to_world_space());
        }
        else {
          pcontext.pixel_radius = 0;
        }
        brush->size = std::max(pcontext.pixel_radius * 2, 1);
      }
      else {
        pcontext.pixel_radius = brush->size / 2.0f;
      }
    }

    /* Get current drawing material. */
    if (Material *ma = BKE_grease_pencil_object_material_from_brush_get(object, brush)) {
      MaterialGPencilStyle *gp_style = ma->gp_style;

      /* Follow user settings for the size of the draw cursor:
       * - Fixed size, or
       * - Brush size (i.e. stroke thickness)
       */
      if ((gp_style) && ((brush->flag & BRUSH_SMOOTH_STROKE) == 0) &&
          (brush->gpencil_brush_type == GPAINT_BRUSH_TYPE_DRAW))
      {

        const bool use_vertex_color = ed::sculpt_paint::greasepencil::brush_using_vertex_color(
            pcontext.scene->toolsettings->gp_paint, brush);
        const bool use_vertex_color_stroke = use_vertex_color;
        if (use_vertex_color_stroke) {
          IMB_colormanagement_scene_linear_to_srgb_v3(color, brush->color);
        }
        else {
          color = float4(gp_style->stroke_rgba).xyz();
        }
      }
    }

    if ((brush->flag & BRUSH_SMOOTH_STROKE) != 0) {
      color = float3(1.0f, 0.4f, 0.4f);
    }
  }
  else if (pcontext.mode == PaintMode::VertexGPencil) {
    pcontext.pixel_radius = BKE_brush_radius_get(pcontext.paint, brush);
    color = BKE_brush_color_get(paint, brush);
    IMB_colormanagement_scene_linear_to_srgb_v3(color, color);
  }

  GPU_line_width(1.0f);
  /* Inner Ring: Color from UI panel */
  immUniformColor4f(color.x, color.y, color.z, 0.8f);
  imm_draw_circle_wire_2d(pcontext.pos, mval.x, mval.y, pcontext.pixel_radius, 32);

  /* Outer Ring: Dark color for contrast on light backgrounds (e.g. gray on white) */
  const float3 darkcolor = color * 0.40f;
  immUniformColor4f(darkcolor[0], darkcolor[1], darkcolor[2], 0.8f);
  imm_draw_circle_wire_2d(pcontext.pos, mval.x, mval.y, pcontext.pixel_radius + 1, 32);
}
}  // namespace blender::ed::sculpt_paint
