/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2023 Blender Foundation. */

/** \file
 * \ingroup edgreasepencil
 */

#include <array>

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "BLI_math.h"
#include "BLI_math_matrix.hh"

#include "BLT_translation.h"

#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "ED_grease_pencil.h"

namespace blender::ed::greasepencil {

struct ColorTemplate {
  const char *name;
  float line[4];
  float fill[4];
  bool show_fill;
};

static const ColorTemplate gp_stroke_material_black = {
    N_("Black"),
    {0.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    false,
};

static const ColorTemplate gp_stroke_material_white = {
    N_("White"),
    {1.0f, 1.0f, 1.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    false,
};

static const ColorTemplate gp_stroke_material_red = {
    N_("Red"),
    {1.0f, 0.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    false,
};

static const ColorTemplate gp_stroke_material_green = {
    N_("Green"),
    {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    false,
};

static const ColorTemplate gp_stroke_material_blue = {
    N_("Blue"),
    {0.0f, 0.0f, 1.0f, 1.0f},
    {0.0f, 0.0f, 0.0f, 0.0f},
    false,
};

static const ColorTemplate gp_fill_material_grey = {
    N_("Grey"),
    {0.358f, 0.358f, 0.358f, 1.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
    true,
};

static std::array<float3, 175> stroke_positions({
    float3(-1.281f, 0.0f, -0.315f), float3(-1.269f, 0.0f, -0.302f), float3(-1.261f, 0.0f, -0.293f),
    float3(-1.251f, 0.0f, -0.282f), float3(-1.241f, 0.0f, -0.271f), float3(-1.23f, 0.0f, -0.259f),
    float3(-1.219f, 0.0f, -0.247f), float3(-1.208f, 0.0f, -0.234f), float3(-1.196f, 0.0f, -0.221f),
    float3(-1.184f, 0.0f, -0.208f), float3(-1.172f, 0.0f, -0.194f), float3(-1.159f, 0.0f, -0.18f),
    float3(-1.147f, 0.0f, -0.165f), float3(-1.134f, 0.0f, -0.151f), float3(-1.121f, 0.0f, -0.136f),
    float3(-1.108f, 0.0f, -0.121f), float3(-1.094f, 0.0f, -0.106f), float3(-1.08f, 0.0f, -0.091f),
    float3(-1.066f, 0.0f, -0.076f), float3(-1.052f, 0.0f, -0.061f), float3(-1.037f, 0.0f, -0.047f),
    float3(-1.022f, 0.0f, -0.032f), float3(-1.007f, 0.0f, -0.017f), float3(-0.991f, 0.0f, -0.003f),
    float3(-0.975f, 0.0f, 0.012f),  float3(-0.959f, 0.0f, 0.027f),  float3(-0.942f, 0.0f, 0.041f),
    float3(-0.926f, 0.0f, 0.056f),  float3(-0.909f, 0.0f, 0.071f),  float3(-0.893f, 0.0f, 0.086f),
    float3(-0.876f, 0.0f, 0.1f),    float3(-0.859f, 0.0f, 0.115f),  float3(-0.842f, 0.0f, 0.129f),
    float3(-0.824f, 0.0f, 0.144f),  float3(-0.807f, 0.0f, 0.158f),  float3(-0.79f, 0.0f, 0.172f),
    float3(-0.773f, 0.0f, 0.186f),  float3(-0.755f, 0.0f, 0.199f),  float3(-0.738f, 0.0f, 0.212f),
    float3(-0.721f, 0.0f, 0.224f),  float3(-0.703f, 0.0f, 0.236f),  float3(-0.686f, 0.0f, 0.248f),
    float3(-0.67f, 0.0f, 0.26f),    float3(-0.653f, 0.0f, 0.27f),   float3(-0.637f, 0.0f, 0.28f),
    float3(-0.621f, 0.0f, 0.29f),   float3(-0.605f, 0.0f, 0.298f),  float3(-0.589f, 0.0f, 0.306f),
    float3(-0.574f, 0.0f, 0.313f),  float3(-0.559f, 0.0f, 0.319f),  float3(-0.544f, 0.0f, 0.325f),
    float3(-0.53f, 0.0f, 0.331f),   float3(-0.516f, 0.0f, 0.336f),  float3(-0.503f, 0.0f, 0.34f),
    float3(-0.489f, 0.0f, 0.344f),  float3(-0.477f, 0.0f, 0.347f),  float3(-0.464f, 0.0f, 0.35f),
    float3(-0.452f, 0.0f, 0.352f),  float3(-0.44f, 0.0f, 0.354f),   float3(-0.429f, 0.0f, 0.355f),
    float3(-0.418f, 0.0f, 0.355f),  float3(-0.407f, 0.0f, 0.355f),  float3(-0.397f, 0.0f, 0.354f),
    float3(-0.387f, 0.0f, 0.353f),  float3(-0.378f, 0.0f, 0.351f),  float3(-0.368f, 0.0f, 0.348f),
    float3(-0.36f, 0.0f, 0.344f),   float3(-0.351f, 0.0f, 0.34f),   float3(-0.344f, 0.0f, 0.336f),
    float3(-0.336f, 0.0f, 0.33f),   float3(-0.329f, 0.0f, 0.324f),  float3(-0.322f, 0.0f, 0.318f),
    float3(-0.316f, 0.0f, 0.31f),   float3(-0.311f, 0.0f, 0.303f),  float3(-0.306f, 0.0f, 0.294f),
    float3(-0.301f, 0.0f, 0.285f),  float3(-0.297f, 0.0f, 0.275f),  float3(-0.293f, 0.0f, 0.264f),
    float3(-0.29f, 0.0f, 0.253f),   float3(-0.288f, 0.0f, 0.241f),  float3(-0.286f, 0.0f, 0.229f),
    float3(-0.285f, 0.0f, 0.216f),  float3(-0.284f, 0.0f, 0.202f),  float3(-0.283f, 0.0f, 0.188f),
    float3(-0.283f, 0.0f, 0.173f),  float3(-0.284f, 0.0f, 0.158f),  float3(-0.285f, 0.0f, 0.142f),
    float3(-0.286f, 0.0f, 0.125f),  float3(-0.288f, 0.0f, 0.108f),  float3(-0.29f, 0.0f, 0.091f),
    float3(-0.293f, 0.0f, 0.073f),  float3(-0.295f, 0.0f, 0.054f),  float3(-0.298f, 0.0f, 0.035f),
    float3(-0.302f, 0.0f, 0.016f),  float3(-0.305f, 0.0f, -0.004f), float3(-0.309f, 0.0f, -0.024f),
    float3(-0.313f, 0.0f, -0.044f), float3(-0.317f, 0.0f, -0.065f), float3(-0.321f, 0.0f, -0.085f),
    float3(-0.326f, 0.0f, -0.106f), float3(-0.33f, 0.0f, -0.127f),  float3(-0.335f, 0.0f, -0.148f),
    float3(-0.339f, 0.0f, -0.168f), float3(-0.344f, 0.0f, -0.189f), float3(-0.348f, 0.0f, -0.21f),
    float3(-0.353f, 0.0f, -0.23f),  float3(-0.357f, 0.0f, -0.25f),  float3(-0.361f, 0.0f, -0.27f),
    float3(-0.365f, 0.0f, -0.29f),  float3(-0.369f, 0.0f, -0.309f), float3(-0.372f, 0.0f, -0.328f),
    float3(-0.375f, 0.0f, -0.347f), float3(-0.377f, 0.0f, -0.365f), float3(-0.379f, 0.0f, -0.383f),
    float3(-0.38f, 0.0f, -0.4f),    float3(-0.38f, 0.0f, -0.417f),  float3(-0.38f, 0.0f, -0.434f),
    float3(-0.379f, 0.0f, -0.449f), float3(-0.377f, 0.0f, -0.464f), float3(-0.374f, 0.0f, -0.478f),
    float3(-0.371f, 0.0f, -0.491f), float3(-0.366f, 0.0f, -0.503f), float3(-0.361f, 0.0f, -0.513f),
    float3(-0.354f, 0.0f, -0.523f), float3(-0.347f, 0.0f, -0.531f), float3(-0.339f, 0.0f, -0.538f),
    float3(-0.33f, 0.0f, -0.543f),  float3(-0.32f, 0.0f, -0.547f),  float3(-0.31f, 0.0f, -0.549f),
    float3(-0.298f, 0.0f, -0.55f),  float3(-0.286f, 0.0f, -0.55f),  float3(-0.274f, 0.0f, -0.548f),
    float3(-0.261f, 0.0f, -0.544f), float3(-0.247f, 0.0f, -0.539f), float3(-0.232f, 0.0f, -0.533f),
    float3(-0.218f, 0.0f, -0.525f), float3(-0.202f, 0.0f, -0.515f), float3(-0.186f, 0.0f, -0.503f),
    float3(-0.169f, 0.0f, -0.49f),  float3(-0.151f, 0.0f, -0.475f), float3(-0.132f, 0.0f, -0.458f),
    float3(-0.112f, 0.0f, -0.44f),  float3(-0.091f, 0.0f, -0.42f),  float3(-0.069f, 0.0f, -0.398f),
    float3(-0.045f, 0.0f, -0.375f), float3(-0.021f, 0.0f, -0.35f),  float3(0.005f, 0.0f, -0.324f),
    float3(0.031f, 0.0f, -0.297f),  float3(0.06f, 0.0f, -0.268f),   float3(0.089f, 0.0f, -0.238f),
    float3(0.12f, 0.0f, -0.207f),   float3(0.153f, 0.0f, -0.175f),  float3(0.187f, 0.0f, -0.14f),
    float3(0.224f, 0.0f, -0.104f),  float3(0.262f, 0.0f, -0.067f),  float3(0.302f, 0.0f, -0.027f),
    float3(0.344f, 0.0f, 0.014f),   float3(0.388f, 0.0f, 0.056f),   float3(0.434f, 0.0f, 0.1f),
    float3(0.483f, 0.0f, 0.145f),   float3(0.533f, 0.0f, 0.191f),   float3(0.585f, 0.0f, 0.238f),
    float3(0.637f, 0.0f, 0.284f),   float3(0.69f, 0.0f, 0.33f),     float3(0.746f, 0.0f, 0.376f),
    float3(0.802f, 0.0f, 0.421f),   float3(0.859f, 0.0f, 0.464f),   float3(0.915f, 0.0f, 0.506f),
    float3(0.97f, 0.0f, 0.545f),    float3(1.023f, 0.0f, 0.581f),   float3(1.075f, 0.0f, 0.614f),
    float3(1.122f, 0.0f, 0.643f),   float3(1.169f, 0.0f, 0.671f),   float3(1.207f, 0.0f, 0.693f),
    float3(1.264f, 0.0f, 0.725f),
});

static constexpr std::array<float, 175> stroke_radii({
    0.038f, 0.069f, 0.089f, 0.112f, 0.134f, 0.155f, 0.175f, 0.194f, 0.211f, 0.227f, 0.242f, 0.256f,
    0.268f, 0.28f,  0.29f,  0.299f, 0.307f, 0.315f, 0.322f, 0.329f, 0.335f, 0.341f, 0.346f, 0.351f,
    0.355f, 0.36f,  0.364f, 0.368f, 0.371f, 0.373f, 0.376f, 0.377f, 0.378f, 0.379f, 0.379f, 0.379f,
    0.38f,  0.38f,  0.381f, 0.382f, 0.384f, 0.386f, 0.388f, 0.39f,  0.393f, 0.396f, 0.399f, 0.403f,
    0.407f, 0.411f, 0.415f, 0.42f,  0.425f, 0.431f, 0.437f, 0.443f, 0.45f,  0.457f, 0.464f, 0.471f,
    0.479f, 0.487f, 0.495f, 0.503f, 0.512f, 0.52f,  0.528f, 0.537f, 0.545f, 0.553f, 0.562f, 0.57f,
    0.579f, 0.588f, 0.597f, 0.606f, 0.615f, 0.625f, 0.635f, 0.644f, 0.654f, 0.664f, 0.675f, 0.685f,
    0.696f, 0.707f, 0.718f, 0.729f, 0.74f,  0.751f, 0.761f, 0.772f, 0.782f, 0.793f, 0.804f, 0.815f,
    0.828f, 0.843f, 0.86f,  0.879f, 0.897f, 0.915f, 0.932f, 0.947f, 0.962f, 0.974f, 0.985f, 0.995f,
    1.004f, 1.011f, 1.018f, 1.024f, 1.029f, 1.033f, 1.036f, 1.037f, 1.037f, 1.035f, 1.032f, 1.029f,
    1.026f, 1.023f, 1.021f, 1.019f, 1.017f, 1.016f, 1.016f, 1.016f, 1.016f, 1.017f, 1.017f, 1.018f,
    1.017f, 1.017f, 1.016f, 1.015f, 1.013f, 1.009f, 1.005f, 0.998f, 0.99f,  0.98f,  0.968f, 0.955f,
    0.939f, 0.923f, 0.908f, 0.895f, 0.882f, 0.87f,  0.858f, 0.844f, 0.828f, 0.81f,  0.79f,  0.769f,
    0.747f, 0.724f, 0.7f,   0.676f, 0.651f, 0.625f, 0.599f, 0.573f, 0.546f, 0.516f, 0.483f, 0.446f,
    0.407f, 0.365f, 0.322f, 0.28f,  0.236f, 0.202f, 0.155f,
});

static constexpr std::array<float, 175> stroke_opacities({
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
});

static int add_material_from_template(Main &bmain, Object &ob, const ColorTemplate &pct)
{
  int index;
  Material *ma = BKE_grease_pencil_object_material_ensure_by_name(
      &bmain, &ob, DATA_(pct.name), &index);

  copy_v4_v4(ma->gp_style->stroke_rgba, pct.line);
  srgb_to_linearrgb_v4(ma->gp_style->stroke_rgba, ma->gp_style->stroke_rgba);

  copy_v4_v4(ma->gp_style->fill_rgba, pct.fill);
  srgb_to_linearrgb_v4(ma->gp_style->fill_rgba, ma->gp_style->fill_rgba);

  if (pct.show_fill) {
    ma->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }

  return index;
}

static bke::CurvesGeometry create_drawing_data(const Span<float3> positions,
                                               const Span<float> radii,
                                               const Span<float> opacities,
                                               const Span<int> offsets,
                                               const Span<int> materials,
                                               const Span<int> radii_factor,
                                               const float4x4 &matrix)
{
  using namespace blender::bke;
  CurvesGeometry curves(offsets.last(), offsets.size() - 1);
  curves.offsets_for_write().copy_from(offsets);

  curves.fill_curve_types(CURVE_TYPE_POLY);

  MutableAttributeAccessor attributes = curves.attributes_for_write();
  MutableSpan<float3> point_positions = curves.positions_for_write();
  point_positions.copy_from(positions);

  SpanAttributeWriter<float> point_radii = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", ATTR_DOMAIN_POINT);
  point_radii.span.copy_from(radii);

  SpanAttributeWriter<float> point_opacities = attributes.lookup_or_add_for_write_span<float>(
      "opacity", ATTR_DOMAIN_POINT);
  point_opacities.span.copy_from(opacities);

  SpanAttributeWriter<bool> stroke_cyclic = attributes.lookup_or_add_for_write_span<bool>(
      "cyclic", ATTR_DOMAIN_CURVE);
  stroke_cyclic.span.fill(false);

  SpanAttributeWriter<int> stroke_materials = attributes.lookup_or_add_for_write_span<int>(
      "material_index", ATTR_DOMAIN_CURVE);
  stroke_materials.span.copy_from(materials);

  const OffsetIndices points_by_curve = curves.points_by_curve();
  for (const int curve_i : curves.curves_range()) {
    const IndexRange points = points_by_curve[curve_i];
    for (const int point_i : points) {
      point_positions[point_i] = math::transform_point(matrix, point_positions[point_i]);
      point_radii.span[point_i] *= radii_factor[curve_i];
    }
  }

  point_radii.finish();
  point_opacities.finish();

  stroke_cyclic.finish();
  stroke_materials.finish();

  return curves;
}

void create_blank(Main &bmain, Object &object, const int frame_numer)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  int material_index = add_material_from_template(bmain, object, gp_stroke_material_black);
  object.actcol = material_index + 1;

  Layer &new_layer = grease_pencil.add_layer(grease_pencil.root_group.wrap(), "GP_Layer");
  grease_pencil.active_layer = &new_layer;

  grease_pencil.add_empty_drawings(1);

  GreasePencilFrame frame{0, 0, BEZT_KEYTYPE_KEYFRAME};
  new_layer.insert_frame(frame_numer, frame);
}

void create_stroke(Main &bmain, Object &object, float4x4 matrix, const int frame_numer)
{
  using namespace blender::bke::greasepencil;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);

  int material_index = add_material_from_template(bmain, object, gp_stroke_material_black);
  add_material_from_template(bmain, object, gp_stroke_material_black);
  add_material_from_template(bmain, object, gp_stroke_material_white);
  add_material_from_template(bmain, object, gp_stroke_material_red);
  add_material_from_template(bmain, object, gp_stroke_material_green);
  add_material_from_template(bmain, object, gp_stroke_material_blue);
  add_material_from_template(bmain, object, gp_fill_material_grey);
  object.actcol = material_index + 1;

  Layer &layer_lines = grease_pencil.add_layer(grease_pencil.root_group.wrap(), "Lines");
  Layer &layer_color = grease_pencil.add_layer(grease_pencil.root_group.wrap(), "Color");
  grease_pencil.active_layer = &layer_lines;

  grease_pencil.add_empty_drawings(2);

  GreasePencilDrawing &drawing = *reinterpret_cast<GreasePencilDrawing *>(
      grease_pencil.drawings_for_write()[1]);
  drawing.geometry.wrap() = create_drawing_data(
      stroke_positions, stroke_radii, stroke_opacities, {0, 175}, {material_index}, {75}, matrix);

  GreasePencilFrame frame_lines{0, 0, BEZT_KEYTYPE_KEYFRAME};
  GreasePencilFrame frame_color{1, 0, BEZT_KEYTYPE_KEYFRAME};
  layer_lines.insert_frame(frame_numer, frame_lines);
  layer_color.insert_frame(frame_numer, frame_color);
}

}  // namespace blender::ed::greasepencil