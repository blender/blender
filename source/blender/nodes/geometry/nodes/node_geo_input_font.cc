/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_vfont.hh"

#include "DNA_vfont_types.h"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_input_font_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Font>("Font").custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    ui::template_id(&params.layout,
                    &params.C,
                    &params.node_ptr,
                    "font",
                    nullptr,
                    "FONT_OT_open",
                    "FONT_OT_unlink");
  });
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->id = id_cast<ID *>(BKE_vfont_builtin_ensure());
}

static void node_geo_exec(GeoNodeExecParams params)
{
  VFont *font = id_cast<VFont *>(params.node().id);
  params.set_output("Font"_ustr, font);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeInputFont");
  ntype.ui_name = "Font";
  ntype.ui_description = "Output a font";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.geometry_node_execute = node_geo_exec;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_input_font_cc
