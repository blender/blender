/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BKE_node.hh"

#include "GPU_material.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_wireframe_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Size").default_value(0.01f).min(0.0f).max(100.0f);
  b.add_output<decl::Float>("Factor", "Fac");
}

static void node_shader_buts_wireframe(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "use_pixel_size", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static int node_shader_gpu_wireframe(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  GPU_material_flag_set(mat, GPU_MATFLAG_BARYCENTRIC);
  /* node->custom1 is use_pixel_size */
  if (node->custom1) {
    return GPU_stack_link(mat, node, "node_wireframe_screenspace", in, out);
  }
  return GPU_stack_link(mat, node, "node_wireframe", in, out);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* NOTE: This node isn't supported by MaterialX. */
  return get_output_default(socket_out_->identifier, NodeItem::Type::Float);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_wireframe_cc

/* node type definition */
void register_node_type_sh_wireframe()
{
  namespace file_ns = blender::nodes::node_shader_wireframe_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeWireframe", SH_NODE_WIREFRAME);
  ntype.ui_name = "Wireframe";
  ntype.ui_description =
      "Retrieve the edges of an object as it appears to Cycles.\nNote: as meshes are triangulated "
      "before being processed by Cycles, topology will always appear triangulated";
  ntype.enum_name_legacy = "WIREFRAME";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_wireframe;
  ntype.gpu_fn = file_ns::node_shader_gpu_wireframe;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
