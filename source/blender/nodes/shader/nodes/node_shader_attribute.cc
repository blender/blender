/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "node_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLI_ghash.hh"

#include "RNA_access.hh"

namespace blender {

namespace nodes::node_shader_attribute_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *ntree = b.tree_or_null();
  const bool is_gpu_internal = ntree && (ntree->flag & NTREE_IS_GPU_SHADER_INTERNAL);

  b.add_input<decl::Int>("LightIndex"_ustr).available(is_gpu_internal);

  b.add_output<decl::Color>("Color"_ustr);
  b.add_output<decl::Vector>("Vector"_ustr);
  b.add_output<decl::Float>("Factor"_ustr, "Fac"_ustr);
  b.add_output<decl::Float>("Alpha"_ustr);
}

static void node_shader_buts_attribute(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "attribute_type", UI_ITEM_NONE, "", ICON_NONE);
  layout.prop(ptr,
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
  NodeShaderAttribute *attr = MEM_new<NodeShaderAttribute>("NodeShaderAttribute");
  node->storage = attr;
}

static int node_shader_gpu_attribute(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  NodeShaderAttribute *attr = static_cast<NodeShaderAttribute *>(node->storage);
  float attr_hash = 0.0f;
  float error_attr[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  GPUNodeLink *cd_attr;

  switch (attr->type) {
    case SHD_ATTRIBUTE_LIGHT: {
      if (!in[0].link) {
        /* Error: Attribute node is not linked to a light accumulation node. */
        cd_attr = GPU_constant(error_attr);
      }
      else if (STREQ(attr->name, "is_sun")) {
        GPU_link(mat, "node_attribute_light_is_sun", in[0].link, &cd_attr);
      }
      else if (STREQ(attr->name, "is_point")) {
        GPU_link(mat, "node_attribute_light_is_point", in[0].link, &cd_attr);
      }
      else if (STREQ(attr->name, "is_spot")) {
        GPU_link(mat, "node_attribute_light_is_spot", in[0].link, &cd_attr);
      }
      else if (STREQ(attr->name, "is_area")) {
        GPU_link(mat, "node_attribute_light_is_area", in[0].link, &cd_attr);
      }
      else if (STREQ(attr->name, "cutoff_distance")) {
        GPU_link(mat, "node_attribute_light_cutoff_distance", in[0].link, &cd_attr);
      }
      else {
        GPU_material_flag_set(mat, GPU_MATFLAG_LIGHT_ATTRIBUTE);
        const bool use_dupli = false;
        /* Mimic gpu_node_graph_add_uniform_attribute */
        uint hash_code = BLI_ghashutil_strhash_p(attr->name) << 1 | (use_dupli ? 0 : 1);

        attr_hash = *reinterpret_cast<float *>(&hash_code);
        GPU_link(mat, "node_attribute_light", in[0].link, GPU_uniform(&attr_hash), &cd_attr);
      }
      break;
    }
    case SHD_ATTRIBUTE_GEOMETRY: {
      cd_attr = GPU_attribute(mat, CD_AUTO_FROM_NAME, attr->name);

      if (STREQ(attr->name, "color")) {
        GPU_link(mat, "node_attribute_color", cd_attr, &cd_attr);
      }
      else if (STREQ(attr->name, "temperature")) {
        GPU_link(mat, "node_attribute_temperature", cd_attr, &cd_attr);
      }
      break;
    }
    case SHD_ATTRIBUTE_VIEW_LAYER: {
      cd_attr = GPU_layer_attribute(mat, attr->name);
      break;
    }
    case SHD_ATTRIBUTE_OBJECT:
    case SHD_ATTRIBUTE_INSTANCER: {
      cd_attr = GPU_uniform_attribute(mat,
                                      attr->name,
                                      attr->type == SHD_ATTRIBUTE_INSTANCER,
                                      reinterpret_cast<uint32_t *>(&attr_hash));

      GPU_link(mat, "node_attribute_uniform", cd_attr, GPU_constant(&attr_hash), &cd_attr);
      break;
    }
  }

  GPU_link(mat, "node_attribute", cd_attr, &out[0].link, &out[1].link, &out[2].link, &out[3].link);

  if (attr->type == SHD_ATTRIBUTE_GEOMETRY) {
    for (const auto [i, sock] : node->outputs.enumerate()) {
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

}  // namespace nodes::node_shader_attribute_cc

/* node type definition */
void register_node_type_sh_attribute()
{
  namespace file_ns = nodes::node_shader_attribute_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeAttribute"_ustr, SH_NODE_ATTRIBUTE);
  ntype.ui_name = "Attribute";
  ntype.ui_description = "Retrieve attributes attached to objects or geometry";
  ntype.enum_name_legacy = "ATTRIBUTE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_attribute;
  ntype.initfunc = file_ns::node_shader_init_attribute;
  bke::node_type_storage(
      ntype, "NodeShaderAttribute", node_free_standard_storage, node_copy_standard_storage);
  ntype.gather_link_search_ops = search_link_ops_for_shader_material_lighting_node;
  ntype.gpu_fn = file_ns::node_shader_gpu_attribute;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  bke::node_register_type(ntype);
}

}  // namespace blender
