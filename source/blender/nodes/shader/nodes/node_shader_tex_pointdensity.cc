/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BKE_texture.h"

#include "RE_texture.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_pointdensity_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector").hide_value();
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Density");
}

static void node_shader_buts_tex_pointdensity(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  bNode *node = (bNode *)ptr->data;
  NodeShaderTexPointDensity *shader_point_density = (NodeShaderTexPointDensity *)node->storage;
  Object *ob = (Object *)node->id;

  PointerRNA ob_ptr, obdata_ptr;
  RNA_id_pointer_create((ID *)ob, &ob_ptr);
  RNA_id_pointer_create(ob ? (ID *)ob->data : nullptr, &obdata_ptr);

  uiItemR(layout, ptr, "point_source", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "object", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);

  if (node->id && shader_point_density->point_source == SHD_POINTDENSITY_SOURCE_PSYS) {
    PointerRNA dataptr;
    RNA_id_pointer_create((ID *)node->id, &dataptr);
    uiItemPointerR(
        layout, ptr, "particle_system", &dataptr, "particle_systems", nullptr, ICON_NONE);
  }

  uiItemR(layout, ptr, "space", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "radius", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "resolution", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  if (shader_point_density->point_source == SHD_POINTDENSITY_SOURCE_PSYS) {
    uiItemR(layout, ptr, "particle_color_source", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else {
    uiItemR(layout, ptr, "vertex_color_source", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    if (shader_point_density->ob_color_source == SHD_POINTDENSITY_COLOR_VERTWEIGHT) {
      if (ob_ptr.data) {
        uiItemPointerR(
            layout, ptr, "vertex_attribute_name", &ob_ptr, "vertex_groups", "", ICON_NONE);
      }
    }
    if (shader_point_density->ob_color_source == SHD_POINTDENSITY_COLOR_VERTCOL) {
      if (obdata_ptr.data) {
        uiItemPointerR(
            layout, ptr, "vertex_attribute_name", &obdata_ptr, "vertex_colors", "", ICON_NONE);
      }
    }
  }
}

static void node_shader_init_tex_pointdensity(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderTexPointDensity *point_density = MEM_new<NodeShaderTexPointDensity>("new pd node");
  point_density->resolution = 100;
  point_density->radius = 0.3f;
  point_density->space = SHD_POINTDENSITY_SPACE_OBJECT;
  point_density->color_source = SHD_POINTDENSITY_COLOR_PARTAGE;
  node->storage = point_density;
}

static void node_shader_free_tex_pointdensity(bNode *node)
{
  NodeShaderTexPointDensity *point_density = (NodeShaderTexPointDensity *)node->storage;
  PointDensity *pd = &point_density->pd;
  RE_point_density_free(pd);
  BKE_texture_pointdensity_free_data(pd);
  *pd = dna::shallow_zero_initialize();
  MEM_freeN(point_density);
}

static void node_shader_copy_tex_pointdensity(bNodeTree * /*dst_ntree*/,
                                              bNode *dest_node,
                                              const bNode *src_node)
{
  dest_node->storage = MEM_dupallocN(src_node->storage);
  NodeShaderTexPointDensity *point_density = (NodeShaderTexPointDensity *)dest_node->storage;
  PointDensity *pd = &point_density->pd;
  *pd = dna::shallow_zero_initialize();
}

}  // namespace blender::nodes::node_shader_tex_pointdensity_cc

/* node type definition */
void register_node_type_sh_tex_pointdensity()
{
  namespace file_ns = blender::nodes::node_shader_tex_pointdensity_cc;

  static bNodeType ntype;

  sh_node_type_base(&ntype, SH_NODE_TEX_POINTDENSITY, "Point Density", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_pointdensity;
  ntype.initfunc = file_ns::node_shader_init_tex_pointdensity;
  node_type_storage(&ntype,
                    "NodeShaderTexPointDensity",
                    file_ns::node_shader_free_tex_pointdensity,
                    file_ns::node_shader_copy_tex_pointdensity);

  nodeRegisterType(&ntype);
}
