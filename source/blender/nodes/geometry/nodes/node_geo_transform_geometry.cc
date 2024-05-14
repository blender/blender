/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"
#include "BLI_task.hh"

#include "NOD_rna_define.hh"

#include "GEO_transform.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_transform_geometry_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::Vector>("Translation").subtype(PROP_TRANSLATION);
  b.add_input<decl::Rotation>("Rotation");
  b.add_input<decl::Vector>("Scale").default_value({1, 1, 1}).subtype(PROP_XYZ);
  b.add_input<decl::Matrix>("Transform");
  b.add_output<decl::Geometry>("Geometry").propagate_all();
}

static void node_update(bNodeTree *tree, bNode *node)
{
  bNodeSocket *translation_socket = static_cast<bNodeSocket *>(node->inputs.first)->next;
  bNodeSocket *rotation_socket = translation_socket->next;
  bNodeSocket *scale_socket = rotation_socket->next;
  bNodeSocket *transform_socket = scale_socket->next;

  const bool use_matrix = node->custom1 == GEO_NODE_TRANSFORM_MODE_MATRIX;

  bke::nodeSetSocketAvailability(tree, translation_socket, !use_matrix);
  bke::nodeSetSocketAvailability(tree, rotation_socket, !use_matrix);
  bke::nodeSetSocketAvailability(tree, scale_socket, !use_matrix);
  bke::nodeSetSocketAvailability(tree, transform_socket, use_matrix);
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}

static bool use_translate(const math::Quaternion &rotation, const float3 scale)
{
  if (math::angle_of(rotation).radian() > 1e-7f) {
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
  if (errors.volume_too_small) {
    params.error_message_add(NodeWarningType::Warning,
                             TIP_("Volume scale is lower than permitted by OpenVDB"));
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bool use_matrix = params.node().custom1 == GEO_NODE_TRANSFORM_MODE_MATRIX;
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  if (use_matrix) {
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

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem mode_items[] = {
      {GEO_NODE_TRANSFORM_MODE_COMPONENTS,
       "COMPONENTS",
       0,
       "Components",
       "Provide separate location, rotation and scale"},
      {GEO_NODE_TRANSFORM_MODE_MATRIX, "MATRIX", 0, "Matrix", "Use a transformation matrix"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mode",
                    "Mode",
                    "How the transformation is specified",
                    mode_items,
                    NOD_inline_enum_accessors(custom1));
}

static void register_node()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_TRANSFORM_GEOMETRY, "Transform Geometry", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.updatefunc = node_update;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_geo_transform_geometry_cc
