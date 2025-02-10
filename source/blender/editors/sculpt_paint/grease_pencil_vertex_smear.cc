/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_gpencil_legacy_types.h"

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"

#include "BLI_index_mask.hh"
#include "BLI_math_color.hh"
#include "BLI_math_vector.hh"
#include "BLI_task.hh"
#include "BLI_virtual_array.hh"

#include "grease_pencil_intern.hh"

namespace blender::ed::sculpt_paint::greasepencil {

struct ColorGrid {
  /* Flat array of colors. The length of this is size^2. */
  Array<float4> colors;
  /* Size of the grid. Used as the width and height. Should be divisible by 2. */
  int size;
  /* The size of each cell in pixels (screen space). Used as the cell width and height. */
  int cell_size_px;
  /* The center position of the grid (screen space). */
  float2 center;

  /* Compute the screen space position based on a grid position and a center. */
  float2 pos_to_coords(const int2 pos, const float2 center) const
  {
    const float2 centered = float2(pos - this->size / 2) + float2(0.5f);
    return (centered * this->cell_size_px) + center;
  }

  /* Compute a grid position based on a screen space position and a center. */
  int2 coords_to_pos(const float2 coord, const float2 center) const
  {
    const int2 pos = int2(math::floor((coord - center) / float(this->cell_size_px)));
    return pos + ((this->size + 1) / 2);
  }

  /* Compute a grid index (into the colors array) based on a grid position. Returns -1 if the
   * position is out of bounds. */
  int pos_to_index(const int2 pos) const
  {
    if (pos.x >= 0 && pos.x < this->size && pos.y >= 0 && pos.y < this->size) {
      return pos.y * this->size + pos.x;
    }
    return -1;
  }
};

class VertexSmearOperation : public GreasePencilStrokeOperationCommon {
  using GreasePencilStrokeOperationCommon::GreasePencilStrokeOperationCommon;

 public:
  void on_stroke_begin(const bContext &C, const InputSample &start_sample) override;
  void on_stroke_extended(const bContext &C, const InputSample &extension_sample) override;
  void on_stroke_done(const bContext &C) override;

 private:
  ColorGrid color_grid_;
  void init_color_grid(const bContext &C, float2 start_position);
};

void VertexSmearOperation::init_color_grid(const bContext &C, const float2 start_position)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const bool use_selection_masking = GPENCIL_ANY_VERTEX_MASK(
      eGP_vertex_SelectMaskFlag(scene.toolsettings->gpencil_selectmode_vertex));
  const float radius = brush_radius(scene, brush, 1.0f);

  /* Setup grid values. */
  /* TODO: Make this a setting. */
  color_grid_.cell_size_px = 10.0f;
  color_grid_.center = start_position;
  color_grid_.size = int(math::ceil((radius * 2.0f) / color_grid_.cell_size_px));

  /* Initialize the color array. */
  const int grid_array_length = color_grid_.size * color_grid_.size;
  color_grid_.colors.reinitialize(grid_array_length);
  color_grid_.colors.fill(float4(0.0f));

  /* Initialize grid values. */
  this->foreach_editable_drawing(C, [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask point_selection = point_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (point_selection.is_empty()) {
      return false;
    }
    const Array<float2> view_positions = calculate_view_positions(params, point_selection);
    const Array<float> radii = calculate_view_radii(params, point_selection);
    const VArray<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors();
    /* Compute the colors in the grid by averaging the vertex colors of the points that
     * intersect each cell. */
    Array<int> points_per_cell(grid_array_length, 0);
    point_selection.foreach_index([&](const int point_i) {
      const float2 view_pos = view_positions[point_i];
      const float view_radius = radii[point_i];
      const ColorGeometry4f color = vertex_colors[point_i];

      const int bounds_size = math::floor(view_radius / color_grid_.cell_size_px) * 2 + 1;
      const int2 bounds_center = color_grid_.coords_to_pos(view_pos, color_grid_.center);
      const int2 bounds_min = bounds_center - (bounds_size / 2);
      const int2 bounds_max = bounds_center + (bounds_size / 2);
      if (!(bounds_min.x < color_grid_.size && bounds_max.x >= 0 &&
            bounds_min.y < color_grid_.size && bounds_max.y >= 0))
      {
        /* Point is out of bounds. */
        return;
      }
      for (int y = bounds_min.y; y <= bounds_max.y; y++) {
        for (int x = bounds_min.x; x <= bounds_max.x; x++) {
          const int2 grid_pos = int2(x, y);
          const int cell_i = color_grid_.pos_to_index(grid_pos);
          if (cell_i == -1) {
            continue;
          }
          const float2 cell_pos = color_grid_.pos_to_coords(grid_pos, color_grid_.center);
          if (math::distance_squared(cell_pos, view_pos) <= view_radius * view_radius) {
            color_grid_.colors[cell_i] += float4(color.r, color.g, color.b, 1.0f);
            points_per_cell[cell_i]++;
          }
        }
      }
    });
    /* Divide by the total to get the average color per cell. */
    for (const int cell_i : color_grid_.colors.index_range()) {
      if (points_per_cell[cell_i] > 0) {
        color_grid_.colors[cell_i] *= 1.0f / float(points_per_cell[cell_i]);
      }
    }
    /* Don't trigger updates for the grid initialization. */
    return false;
  });
}

void VertexSmearOperation::on_stroke_begin(const bContext &C, const InputSample &start_sample)
{
  this->init_stroke(C, start_sample);
  this->init_color_grid(C, start_sample.mouse_position);
}

void VertexSmearOperation::on_stroke_extended(const bContext &C,
                                              const InputSample &extension_sample)
{
  const Scene &scene = *CTX_data_scene(&C);
  Paint &paint = *BKE_paint_get_active_from_context(&C);
  const Brush &brush = *BKE_paint_brush(&paint);
  const float radius = brush_radius(scene, brush, extension_sample.pressure);

  const bool use_selection_masking = GPENCIL_ANY_VERTEX_MASK(
      eGP_vertex_SelectMaskFlag(scene.toolsettings->gpencil_selectmode_vertex));

  this->foreach_editable_drawing(C, GrainSize(1), [&](const GreasePencilStrokeParams &params) {
    IndexMaskMemory memory;
    const IndexMask point_selection = point_mask_for_stroke_operation(
        params, use_selection_masking, memory);
    if (point_selection.is_empty()) {
      return false;
    }
    const Array<float2> view_positions = calculate_view_positions(params, point_selection);
    MutableSpan<ColorGeometry4f> vertex_colors = params.drawing.vertex_colors_for_write();
    point_selection.foreach_index(GrainSize(1024), [&](const int64_t point_i) {
      const float2 view_pos = view_positions[point_i];
      const int2 grid_pos = color_grid_.coords_to_pos(view_pos, extension_sample.mouse_position);
      const int cell_i = color_grid_.pos_to_index(grid_pos);
      if (cell_i == -1 || color_grid_.colors[cell_i][3] == 0.0f) {
        return;
      }
      const ColorGeometry4f mix_color = ColorGeometry4f(color_grid_.colors[cell_i]);

      const float distance_falloff = math::clamp(
          1.0f - (math::distance(color_grid_.center, view_pos) / radius * 2), 0.0f, 1.0f);
      const float influence = brush_point_influence(scene,
                                                    brush,
                                                    view_pos,
                                                    extension_sample,
                                                    params.multi_frame_falloff) *
                              distance_falloff;
      if (influence > 0.0f) {
        ColorGeometry4f &color = vertex_colors[point_i];
        const float alpha = color.a;
        color = math::interpolate(color, mix_color, influence);
        color.a = alpha;
      }
    });
    return true;
  });
}

void VertexSmearOperation::on_stroke_done(const bContext & /*C*/) {}

std::unique_ptr<GreasePencilStrokeOperation> new_vertex_smear_operation()
{
  return std::make_unique<VertexSmearOperation>();
}

}  // namespace blender::ed::sculpt_paint::greasepencil
