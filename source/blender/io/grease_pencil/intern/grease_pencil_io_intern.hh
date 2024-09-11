/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_bounds.hh"
#include "BLI_color.hh"
#include "BLI_function_ref.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "grease_pencil_io.hh"

#include <cstdint>
#include <optional>

#pragma once

/** \file
 * \ingroup bgrease_pencil
 */

struct Scene;
struct Object;
struct Material;
struct RegionView3D;
namespace blender::bke::greasepencil {
class Layer;
class Drawing;
}  // namespace blender::bke::greasepencil

namespace blender::io::grease_pencil {

class GreasePencilImporter {
 protected:
  const IOContext context_;
  const ImportParams params_;

  Object *object_ = nullptr;

 public:
  GreasePencilImporter(const IOContext &context, const ImportParams &params);

  Object *create_object(StringRefNull name);
  int32_t create_material(StringRefNull name, bool stroke, bool fill);
};

class GreasePencilExporter {
 public:
  struct ObjectInfo {
    Object *object;
    float depth;
  };

 protected:
  const IOContext context_;
  const ExportParams params_;

  /* Camera projection matrix, only available with an active camera. */
  std::optional<float4x4> camera_persmat_;
  blender::Bounds<float2> render_rect_;

 public:
  GreasePencilExporter(const IOContext &context, const ExportParams &params);

  void prepare_render_params(Scene &scene, int frame_number);

  static ColorGeometry4f compute_average_stroke_color(const Material &material,
                                                      const Span<ColorGeometry4f> vertex_colors);
  static float compute_average_stroke_opacity(const Span<float> opacities);

  /* Returns a value if point sizes are all equal. */
  static std::optional<float> try_get_uniform_point_width(const RegionView3D &rv3d,
                                                          const Span<float3> world_positions,
                                                          const Span<float> radii);

  Vector<ObjectInfo> retrieve_objects() const;

  using WriteStrokeFn = FunctionRef<void(const Span<float3> positions,
                                         bool cyclic,
                                         const ColorGeometry4f &color,
                                         float opacity,
                                         std::optional<float> width,
                                         bool round_cap,
                                         bool is_outline)>;

  void foreach_stroke_in_layer(const Object &object,
                               const bke::greasepencil::Layer &layer,
                               const bke::greasepencil::Drawing &drawing,
                               WriteStrokeFn stroke_fn);

  float2 project_to_screen(const float4x4 &transform, const float3 &position) const;
};

}  // namespace blender::io::grease_pencil
