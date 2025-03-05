/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_main.hh"
#include "BLI_assert.h"
#include "BLI_bounds.hh"
#include "BLI_color.hh"
#include "BLI_math_color.h"
#include "BLI_math_euler_types.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.hh"
#include "BLI_offset_indices.hh"
#include "BLI_path_utils.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_report.hh"

#include "BLI_string.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_grease_pencil.hh"

#include "grease_pencil_io_intern.hh"

#include "nanosvg.h"

#include <fmt/core.h>
#include <fmt/format.h>

/** \file
 * \ingroup bgrease_pencil
 */

using blender::bke::greasepencil::Drawing;
using blender::bke::greasepencil::Layer;
using blender::bke::greasepencil::TreeNode;

namespace blender::io::grease_pencil {

class SVGImporter : public GreasePencilImporter {
 public:
  using GreasePencilImporter::GreasePencilImporter;

  bool read(StringRefNull filepath);
};

static std::string get_layer_id(const NSVGshape &shape, const int prefix)
{
  return (shape.id_parent[0] == '\0') ? fmt::format("Layer_{:03d}", prefix) :
                                        fmt::format("{:s}", shape.id_parent);
}

/* Unpack internal NanoSVG color. */
static ColorGeometry4f unpack_nano_color(const uint pack)
{
  const uchar4 rgb_u = {uint8_t(((pack) >> 0) & 0xFF),
                        uint8_t(((pack) >> 8) & 0xFF),
                        uint8_t(((pack) >> 16) & 0xFF),
                        uint8_t(((pack) >> 24) & 0xFF)};
  const float4 rgb_f = {float(rgb_u[0]) / 255.0f,
                        float(rgb_u[1]) / 255.0f,
                        float(rgb_u[2]) / 255.0f,
                        float(rgb_u[3]) / 255.0f};

  ColorGeometry4f color;
  srgb_to_linearrgb_v4(color, rgb_f);
  return color;
}

/* Simple approximation of a gradient by a single color. */
static ColorGeometry4f average_gradient_color(const NSVGgradient &svg_gradient)
{
  const Span<NSVGgradientStop> stops = {svg_gradient.stops, svg_gradient.nstops};

  float4 avg_color = float4(0, 0, 0, 0);
  if (stops.is_empty()) {
    return ColorGeometry4f(avg_color);
  }

  for (const int i : stops.index_range()) {
    avg_color += float4(unpack_nano_color(stops[i].color));
  }
  avg_color /= stops.size();

  return ColorGeometry4f(avg_color);
}

/* TODO Gradients are not yet supported (will output magenta placeholder color).
 * This is because gradients for fill materials in particular can only be defined by materials.
 * Since each path can have a unique gradient it potentially requires a material per curve.
 * Stroke gradients could be baked into vertex colors. */
static ColorGeometry4f convert_svg_color(const NSVGpaint &svg_paint)
{
  switch (NSVGpaintType(svg_paint.type)) {
    case NSVG_PAINT_UNDEF:
      return ColorGeometry4f(1, 0, 1, 1);
    case NSVG_PAINT_NONE:
      return ColorGeometry4f(0, 0, 0, 1);
    case NSVG_PAINT_COLOR:
      return unpack_nano_color(svg_paint.color);
    case NSVG_PAINT_LINEAR_GRADIENT:
      return average_gradient_color(*svg_paint.gradient);
    case NSVG_PAINT_RADIAL_GRADIENT:
      return average_gradient_color(*svg_paint.gradient);

    default:
      BLI_assert_unreachable();
      return ColorGeometry4f(0, 0, 0, 0);
  }
}

/* Make room for curves and points from the SVG shape.
 * Returns the index range of newly added curves. */
static IndexRange extend_curves_geometry(bke::CurvesGeometry &curves, const NSVGshape &shape)
{
  const int old_curves_num = curves.curves_num();
  const int old_points_num = curves.points_num();
  const Span<int> old_offsets = curves.offsets();

  /* Count curves and points. */
  Vector<int> new_curve_offsets;
  for (NSVGpath *path = shape.paths; path; path = path->next) {
    if (path->npts == 0) {
      continue;
    }
    BLI_assert(path->npts >= 1 && path->npts == int(path->npts / 3) * 3 + 1);
    /* nanosvg converts everything to bezier curves, points come in triplets. Round up to the
     * next full integer, since there is one point without handles (3*n+1 points in total). */
    const int point_num = (path->npts + 2) / 3;
    new_curve_offsets.append(point_num);
  }
  if (new_curve_offsets.is_empty()) {
    return {};
  }
  new_curve_offsets.append(0);
  const OffsetIndices new_points_by_curve = offset_indices::accumulate_counts_to_offsets(
      new_curve_offsets, old_points_num);

  const IndexRange new_curves_range = {old_curves_num, new_points_by_curve.size()};
  const int curves_num = new_curves_range.one_after_last();
  const int points_num = new_points_by_curve.total_size() + old_points_num;

  Array<int> new_offsets(curves_num + 1);
  if (old_curves_num > 0) {
    new_offsets.as_mutable_span().slice(0, old_curves_num).copy_from(old_offsets.drop_back(1));
  }
  new_offsets.as_mutable_span()
      .slice(old_curves_num, new_curve_offsets.size())
      .copy_from(new_curve_offsets);

  curves.resize(points_num, curves_num);
  curves.offsets_for_write().copy_from(new_offsets);

  curves.tag_topology_changed();

  return new_curves_range;
}

static void shape_attributes_to_curves(bke::CurvesGeometry &curves,
                                       const NSVGshape &shape,
                                       const IndexRange curves_range,
                                       const float4x4 &transform,
                                       const int material_index)
{
  /* Path width is twice the radius. */
  const float path_width_scale = 0.5f * math::average(math::to_scale(transform));
  const OffsetIndices points_by_curve = curves.points_by_curve();

  /* nanosvg converts everything to Bezier curves. */
  curves.curve_types_for_write().slice(curves_range).fill(CURVE_TYPE_BEZIER);
  curves.update_curve_types();

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  bke::SpanAttributeWriter<int> materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  MutableSpan<bool> cyclic = curves.cyclic_for_write();

  bke::SpanAttributeWriter fill_colors = attributes.lookup_or_add_for_write_span<ColorGeometry4f>(
      "fill_color", bke::AttrDomain::Curve);
  bke::SpanAttributeWriter<float> fill_opacities = attributes.lookup_or_add_for_write_span<float>(
      "fill_opacity", bke::AttrDomain::Curve);

  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float3> handle_positions_left = curves.handle_positions_left_for_write();
  MutableSpan<float3> handle_positions_right = curves.handle_positions_right_for_write();
  MutableSpan<int8_t> handle_types_left = curves.handle_types_left_for_write();
  MutableSpan<int8_t> handle_types_right = curves.handle_types_right_for_write();
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_span<float>(
      "radius", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<ColorGeometry4f> vertex_colors =
      attributes.lookup_or_add_for_write_span<ColorGeometry4f>("vertex_color",
                                                               bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> point_opacities = attributes.lookup_or_add_for_write_span<float>(
      "opacity", bke::AttrDomain::Point);

  materials.span.slice(curves_range).fill(material_index);
  const ColorGeometry4f shape_color = convert_svg_color(shape.fill);
  if (fill_colors) {
    fill_colors.span.slice(curves_range).fill(shape_color);
  }
  if (fill_opacities) {
    fill_opacities.span.slice(curves_range).fill(shape_color.a);
  }

  int curve_index = curves_range.start();
  for (NSVGpath *path = shape.paths; path; path = path->next) {
    if (path->npts == 0) {
      continue;
    }

    cyclic[curve_index] = bool(path->closed);

    /* 2D vectors in triplets: [control point, left handle, right handle]. */
    const Span<float2> svg_path_data = Span<float>(path->pts, 2 * path->npts).cast<float2>();

    const IndexRange points = points_by_curve[curve_index];
    for (const int i : points.index_range()) {
      const int point_index = points[i];
      const float2 pos_center = svg_path_data[i * 3];
      const float2 pos_handle_left = (i > 0) ? svg_path_data[i * 3 - 1] : pos_center;
      const float2 pos_handle_right = (i < points.size() - 1) ? svg_path_data[i * 3 + 1] :
                                                                pos_center;
      positions[point_index] = math::transform_point(transform, float3(pos_center, 0.0f));
      handle_positions_left[point_index] = math::transform_point(transform,
                                                                 float3(pos_handle_left, 0.0f));
      handle_positions_right[point_index] = math::transform_point(transform,
                                                                  float3(pos_handle_right, 0.0f));
      handle_types_left[point_index] = BEZIER_HANDLE_FREE;
      handle_types_right[point_index] = BEZIER_HANDLE_FREE;

      radii.span[point_index] = shape.strokeWidth * path_width_scale;

      const ColorGeometry4f point_color = convert_svg_color(shape.stroke);
      if (vertex_colors) {
        vertex_colors.span[point_index] = point_color;
      }
      if (point_opacities) {
        point_opacities.span[point_index] = point_color.a;
      }
    }

    ++curve_index;
  }

  materials.finish();
  fill_colors.finish();
  fill_opacities.finish();
  radii.finish();
  vertex_colors.finish();
  point_opacities.finish();
  curves.tag_positions_changed();
  curves.tag_radii_changed();
}

static void shift_to_bounds_center(GreasePencil &grease_pencil)
{
  const std::optional<Bounds<float3>> bounds = [&]() {
    std::optional<Bounds<float3>> bounds;
    for (GreasePencilDrawingBase *drawing_base : grease_pencil.drawings()) {
      if (drawing_base->type != GP_DRAWING) {
        continue;
      }
      Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
      bounds = bounds::merge(bounds, drawing.strokes().bounds_min_max());
    }
    return bounds;
  }();
  if (!bounds) {
    return;
  }
  const float3 offset = -bounds->center();

  for (GreasePencilDrawingBase *drawing_base : grease_pencil.drawings()) {
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }
    Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
    drawing.strokes_for_write().translate(offset);
    drawing.tag_positions_changed();
  }
}

bool SVGImporter::read(StringRefNull filepath)
{
  /* Fixed SVG unit for scaling. */
  constexpr const char *svg_units = "mm";
  constexpr float svg_dpi = 96.0f;

  char abs_filepath[FILE_MAX];
  STRNCPY(abs_filepath, filepath.c_str());
  BLI_path_abs(abs_filepath, BKE_main_blendfile_path_from_global());

  NSVGimage *svg_data = nullptr;
  svg_data = nsvgParseFromFile(abs_filepath, svg_units, svg_dpi);
  if (svg_data == nullptr) {
    BKE_report(context_.reports, RPT_ERROR, "Could not open SVG");
    return false;
  }

  /* Create grease pencil object. */
  char filename[FILE_MAX];
  BLI_path_split_file_part(abs_filepath, filename, ARRAY_SIZE(filename));
  object_ = create_object(filename);
  if (object_ == nullptr) {
    BKE_report(context_.reports, RPT_ERROR, "Unable to create new object");
    nsvgDelete(svg_data);
    return false;
  }
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object_->data);

  const float scene_unit_scale = (context_.scene->unit.system != USER_UNIT_NONE &&
                                  params_.use_scene_unit) ?
                                     context_.scene->unit.scale_length :
                                     1.0f;
  /* Overall scale for SVG coordinates in millimeters. */
  const float svg_scale = 0.001f * scene_unit_scale * params_.scale;
  /* Grease pencil is rotated 90 degrees in X axis by default. */
  const float4x4 transform = math::scale(
      math::from_rotation<float4x4>(math::EulerXYZ(DEG2RAD(-90), 0, 0)), float3(svg_scale));

  /* True if any shape has a color gradient, which are not fully supported. */
  bool has_color_gradient = false;

  /* Loop all shapes. */
  std::string prv_id = "*";
  int prefix = 0;
  for (NSVGshape *shape = svg_data->shapes; shape; shape = shape->next) {
    std::string layer_id = get_layer_id(*shape, prefix);
    if (prv_id != layer_id) {
      prefix++;
      layer_id = get_layer_id(*shape, prefix);
      prv_id = layer_id;
    }

    /* Check if the layer exist and create if needed. */
    Layer &layer = [&]() -> Layer & {
      TreeNode *layer_node = grease_pencil.find_node_by_name(layer_id);
      if (layer_node && layer_node->is_layer()) {
        return layer_node->as_layer();
      }

      Layer &layer = grease_pencil.add_layer(layer_id);
      layer.as_node().flag |= GP_LAYER_TREE_NODE_USE_LIGHTS;
      return layer;
    }();

    /* Check frame. */
    Drawing *drawing = grease_pencil.get_drawing_at(layer, params_.frame_number);
    if (drawing == nullptr) {
      drawing = grease_pencil.insert_frame(layer, params_.frame_number);
      if (!drawing) {
        continue;
      }
    }

    /* Create materials. */
    const bool is_fill = bool(shape->fill.type);
    const bool is_stroke = bool(shape->stroke.type) || !is_fill;
    const StringRefNull mat_name = (is_stroke ? (is_fill ? "Both" : "Stroke") : "Fill");
    const int material_index = create_material(mat_name, is_stroke, is_fill);

    if (ELEM(shape->fill.type, NSVG_PAINT_LINEAR_GRADIENT, NSVG_PAINT_RADIAL_GRADIENT)) {
      has_color_gradient = true;
    }

    bke::CurvesGeometry &curves = drawing->strokes_for_write();
    const IndexRange new_curves_range = extend_curves_geometry(curves, *shape);
    if (new_curves_range.is_empty()) {
      continue;
    }

    shape_attributes_to_curves(curves, *shape, new_curves_range, transform, material_index);
    drawing->strokes_for_write() = std::move(curves);
  }

  /* Free SVG memory. */
  nsvgDelete(svg_data);

  /* Calculate bounding box and move all points to new origin center. */
  if (params_.recenter_bounds) {
    shift_to_bounds_center(grease_pencil);
  }

  if (has_color_gradient) {
    BKE_report(context_.reports,
               RPT_WARNING,
               "SVG has gradients, Grease Pencil color will be approximated");
  }

  return true;
}

bool import_svg(const IOContext &context, const ImportParams &params, StringRefNull filepath)
{
  SVGImporter importer(context, params);
  return importer.read(filepath);
}

}  // namespace blender::io::grease_pencil
