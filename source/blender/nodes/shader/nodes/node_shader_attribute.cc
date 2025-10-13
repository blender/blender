/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "node_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

namespace blender::nodes::node_shader_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Vector>("Vector");
  b.add_output<decl::Float>("Factor", "Fac");
  b.add_output<decl::Float>("Alpha");
}

static void node_shader_buts_attribute(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "attribute_type", UI_ITEM_NONE, "", ICON_NONE);
  layout->prop(ptr,
               RNA_struct_find_property(ptr, "attribute_name"),
               -1,
               0,
               UI_ITEM_NONE,
               "",
               ICON_NONE,
               IFACE_("Name"));
}

static void node_shader_init_attribute(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderAttribute *attr = MEM_callocN<NodeShaderAttribute>("NodeShaderAttribute");
  node->storage = attr;
}

static int node_shader_gpu_attribute(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  NodeShaderAttribute *attr = static_cast<NodeShaderAttribute *>(node->storage);
  bool is_varying = attr->type == SHD_ATTRIBUTE_GEOMETRY;
  float attr_hash = 0.0f;

  GPUNodeLink *cd_attr;

  if (is_varying) {
    cd_attr = GPU_attribute(mat, CD_AUTO_FROM_NAME, attr->name);

    if (STREQ(attr->name, "color")) {
      GPU_link(mat, "node_attribute_color", cd_attr, &cd_attr);
    }
    else if (STREQ(attr->name, "temperature")) {
      GPU_link(mat, "node_attribute_temperature", cd_attr, &cd_attr);
    }
  }
  else if (attr->type == SHD_ATTRIBUTE_VIEW_LAYER) {
    cd_attr = GPU_layer_attribute(mat, attr->name);
  }
  else {
    cd_attr = GPU_uniform_attribute(mat,
                                    attr->name,
                                    attr->type == SHD_ATTRIBUTE_INSTANCER,
                                    reinterpret_cast<uint32_t *>(&attr_hash));

    GPU_link(mat, "node_attribute_uniform", cd_attr, GPU_constant(&attr_hash), &cd_attr);
  }

  GPU_stack_link(mat, node, "node_attribute", in, out, cd_attr);

  if (is_varying) {
    int i;
    LISTBASE_FOREACH_INDEX (bNodeSocket *, sock, &node->outputs, i) {
      node_shader_gpu_bump_tex_coord(mat, node, &out[i].link);
    }
  }

  return 1;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* TODO: some outputs expected be implemented within the next iteration
   * (see node-definition `<geompropvalue>`). */
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_attribute_cc

/* node type definition */
void register_node_type_sh_attribute()
{
  namespace file_ns = blender::nodes::node_shader_attribute_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeAttribute", SH_NODE_ATTRIBUTE);
  ntype.ui_name = "Attribute";
  ntype.ui_description = "Retrieve attributes attached to objects or geometry";
  ntype.enum_name_legacy = "ATTRIBUTE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_attribute;
  ntype.initfunc = file_ns::node_shader_init_attribute;
  blender::bke::node_type_storage(
      ntype, "NodeShaderAttribute", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_attribute;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
