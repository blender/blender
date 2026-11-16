/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "DNA_material_types.h"

namespace blender::nodes::node_geo_grease_pencil_material_info {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  {
    auto &p = b.add_panel("Stroke"_ustr).default_closed(false);
    p.add_output<decl::Color>("Stroke Base Color"_ustr);
    p.add_output<decl::Bool>("Stroke Holdout"_ustr);
    p.add_output<decl::Bool>("Stroke Self Overlap"_ustr);
  }

  {
    auto &p = b.add_panel("Fill"_ustr).default_closed(false);
    p.add_output<decl::Color>("Fill Base Color"_ustr);
    p.add_output<decl::Bool>("Fill Holdout"_ustr);
  }

  b.add_input<decl::Material>("Material"_ustr).optional_label();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  Material *material = params.extract_input<Material *>("Material"_ustr);

  if (material == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }

  const MaterialGPencilStyle *gp_style = material->gp_style;

  if (gp_style == nullptr) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("Material is not a Grease Pencil material"));
    params.set_default_remaining_outputs();
    return;
  }

  params.set_output("Stroke Base Color"_ustr, ColorGeometry4f(gp_style->stroke_rgba));
  params.set_output("Stroke Holdout"_ustr, bool(gp_style->flag & GP_MATERIAL_IS_STROKE_HOLDOUT));
  params.set_output("Stroke Self Overlap"_ustr,
                    bool(gp_style->flag & GP_MATERIAL_DISABLE_STENCIL));
  params.set_output("Fill Base Color"_ustr, ColorGeometry4f(gp_style->fill_rgba));
  params.set_output("Fill Holdout"_ustr, bool(gp_style->flag & GP_MATERIAL_IS_FILL_HOLDOUT));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGreasePencilMaterialInfo"_ustr);
  ntype.ui_name = "Grease Pencil Material Info";
  ntype.ui_description = "Retrieve information from a Grease Pencil material";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.default_width = bke::NodeWidth::_200;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grease_pencil_material_info
