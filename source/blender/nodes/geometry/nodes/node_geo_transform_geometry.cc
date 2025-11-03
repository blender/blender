/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"

#include "GEO_transform.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_transform_geometry_cc {

static EnumPropertyItem mode_items[] = {
    {GEO_NODE_TRANSFORM_MODE_COMPONENTS,
     "COMPONENTS",
     0,
     N_("Components"),
     N_("Provide separate location, rotation and scale")},
    {GEO_NODE_TRANSFORM_MODE_MATRIX, "MATRIX", 0, N_("Matrix"), N_("Use a transformation matrix")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Geometry")
      .is_default_link_socket()
      .description("Geometry to transform");
  b.add_output<decl::Geometry>("Geometry").propagate_all().align_with_previous();
  b.add_input<decl::Menu>("Mode")
      .static_items(mode_items)
      .optional_label()
      .description("How the transformation is specified");
  b.add_input<decl::Vector>("Translation")
      .subtype(PROP_TRANSLATION)
      .usage_by_single_menu(GEO_NODE_TRANSFORM_MODE_COMPONENTS);
  b.add_input<decl::Rotation>("Rotation").usage_by_single_menu(GEO_NODE_TRANSFORM_MODE_COMPONENTS);
  b.add_input<decl::Vector>("Scale")
      .default_value({1, 1, 1})
      .subtype(PROP_XYZ)
      .usage_by_single_menu(GEO_NODE_TRANSFORM_MODE_COMPONENTS);
  b.add_input<decl::Matrix>("Transform").usage_by_single_menu(GEO_NODE_TRANSFORM_MODE_MATRIX);
}

static bool use_translate(const math::Quaternion &rotation, const float3 scale)
{
  if (math::length_squared(rotation.imaginary_part()) > 1e-10f) {
    return false;
  }
  if (compare_ff(scale.x, 1.0f, 1e-9f) != 1 || compare_ff(scale.y, 1.0f, 1e-9f) != 1 ||
      compare_ff(scale.z, 1.0f, 1e-9f) != 1)
  {
    return false;
  }
  return true;
}

static void report_errors(GeoNodeExecParams &params,
                          const geometry::TransformGeometryErrors &errors)
{
  if (errors.bad_volume_transform) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("Invalid transformation for volume grids"));
  }
  else if (errors.volume_too_small) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("Volume scale is lower than permitted by OpenVDB"));
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const auto mode = params.get_input<NodeGeometryTransformMode>("Mode");
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (mode == GEO_NODE_TRANSFORM_MODE_MATRIX) {
    const float4x4 transform = params.extract_input<float4x4>("Transform");
    if (auto errors = geometry::transform_geometry(geometry_set, transform)) {
      report_errors(params, *errors);
    }
  }
  else {
    const float3 translation = params.extract_input<float3>("Translation");
    const math::Quaternion rotation = params.extract_input<math::Quaternion>("Rotation");
    const float3 scale = params.extract_input<float3>("Scale");

    /* Use only translation if rotation and scale don't apply. */
    if (use_translate(rotation, scale)) {
      geometry::translate_geometry(geometry_set, translation);
    }
    else {
      if (auto errors = geometry::transform_geometry(
              geometry_set, math::from_loc_rot_scale<float4x4>(translation, rotation, scale)))
      {
        report_errors(params, *errors);
      }
    }
  }

  params.set_output("Geometry", std::move(geometry_set));
}

static void register_node()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeTransform", GEO_NODE_TRANSFORM_GEOMETRY);
  ntype.ui_name = "Transform Geometry";
  ntype.ui_description = "Translate, rotate or scale the geometry";
  ntype.enum_name_legacy = "TRANSFORM_GEOMETRY";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_geo_transform_geometry_cc
