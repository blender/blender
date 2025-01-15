/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BLI_hash.h"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_input_aov_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Image>("Color");
  b.add_output<decl::Image>("Value");
}

static void node_shader_buts_input_aov(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "aov_name", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static void node_shader_init_input_aov(bNodeTree * /*ntree*/, bNode *node)
{
  NodeShaderOutputAOV *aov = MEM_cnew<NodeShaderOutputAOV>("NodeShaderInputAOV");
  node->storage = aov;
}

static int node_shader_gpu_input_aov(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  GPUNodeLink *outlink;
  NodeShaderOutputAOV *aov = (NodeShaderOutputAOV *)node->storage;
  uint hash = BLI_hash_string(aov->name);
  /* WORKAROUND: We don't support int/uint constants for now. So make sure the aliasing works.
   * We cast back to uint in GLSL. */
  BLI_STATIC_ASSERT(sizeof(float) == sizeof(uint),
                    "GPUCodegen: AOV hash needs float and uint to be the same size.");
  GPUNodeLink *hash_link = GPU_constant((float *)&hash);

  GPU_material_flag_set(mat, GPU_MATFLAG_AOV);
  GPU_stack_link(mat, node, "node_input_aov", in, out, hash_link, &outlink);
  // GPU_material_add_output_link_aov(mat, outlink, hash);
  return true;
}

}  // namespace blender::nodes::node_shader_input_aov_cc

/* node type definition */
void register_node_type_sh_input_aov()
{
  namespace file_ns = blender::nodes::node_shader_input_aov_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeInputAOV", SH_NODE_INPUT_AOV);
  ntype.enum_name_legacy = "INPUT_AOV";
  ntype.ui_name = "AOV Input";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_input_aov;
  ntype.initfunc = file_ns::node_shader_init_input_aov;
  blender::bke::node_type_storage(
      &ntype, "NodeShaderOutputAOV", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_input_aov;

  ntype.no_muting = true;

  blender::bke::node_register_type(&ntype);
}
