/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_image_types.h"

#include "node_geometry_util.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

namespace blender::nodes::node_geo_image_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Image>("Image").custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    uiTemplateID(&params.layout,
                 &params.C,
                 &params.node_ptr,
                 "image",
                 "IMAGE_OT_new",
                 "IMAGE_OT_open",
                 nullptr);
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Image", reinterpret_cast<Image *>(params.node().id));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputImage", GEO_NODE_IMAGE);
  ntype.ui_name = "Image";
  ntype.ui_description = "Input an image data-block";
  ntype.enum_name_legacy = "IMAGE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_image_cc
