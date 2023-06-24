/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_image_types.h"

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_geo_image_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Image>("Image");
}

static void node_layout(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  uiTemplateID(layout,
               C,
               ptr,
               "image",
               "IMAGE_OT_new",
               "IMAGE_OT_open",
               nullptr,
               UI_TEMPLATE_ID_FILTER_ALL,
               false,
               nullptr);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  params.set_output("Image", reinterpret_cast<Image *>(params.node().id));
}

}  // namespace blender::nodes::node_geo_image_cc

void register_node_type_geo_image()
{
  namespace file_ns = blender::nodes::node_geo_image_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_IMAGE, "Image", NODE_CLASS_INPUT);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::LARGE);
  nodeRegisterType(&ntype);
}
