/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_math_base.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_ambient_occlusion_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Color").default_value({1.0f, 1.0f, 1.0f, 1.0f});
  b.add_input<decl::Float>("Distance").default_value(1.0f).min(0.0f).max(1000.0f);
  b.add_input<decl::Vector>("Normal").min(-1.0f).max(1.0f).hide_value();
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("AO");
}

static void node_shader_buts_ambient_occlusion(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "samples", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  layout->prop(ptr, "inside", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  layout->prop(ptr, "only_local", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static int node_shader_gpu_ambient_occlusion(GPUMaterial *mat,
                                             bNode *node,
                                             bNodeExecData * /*execdata*/,
                                             GPUNodeStack *in,
                                             GPUNodeStack *out)
{
  if (!in[2].link) {
    GPU_link(mat, "world_normals_get", &in[2].link);
  }

  GPU_material_flag_set(mat, GPU_MATFLAG_AO);

  float inverted = (node->custom2 & SHD_AO_INSIDE) ? 1.0f : 0.0f;
  float f_samples = divide_ceil_u(node->custom1, 4);

  return GPU_stack_link(mat,
                        node,
                        "node_ambient_occlusion",
                        in,
                        out,
                        GPU_constant(&inverted),
                        GPU_constant(&f_samples));
}

static void node_shader_init_ambient_occlusion(bNodeTree * /*ntree*/, bNode *node)
{
  node->custom1 = 16; /* samples */
  node->custom2 = 0;
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  /* TODO: observed crash while rendering MaterialX_v1_38_6::ExceptionShaderGenError */
  /**
   * \code{.cc}
   * NodeItem maxdistance = get_input_value("Distance", NodeItem::Type::Float);
   * NodeItem res = create_node("ambientocclusion", NodeItem::Type::Float);
   * res.set_input("coneangle", val(90.0f));
   * res.set_input("maxdistance", maxdistance);
   * \endcode
   */
  return get_output_default(socket_out_->identifier, NodeItem::Type::Any);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_ambient_occlusion_cc

/* node type definition */
void register_node_type_sh_ambient_occlusion()
{
  namespace file_ns = blender::nodes::node_shader_ambient_occlusion_cc;

  static blender::bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeAmbientOcclusion", SH_NODE_AMBIENT_OCCLUSION);
  ntype.ui_name = "Ambient Occlusion";
  ntype.ui_description =
      "Compute how much the hemisphere above the shading point is occluded, for example to add "
      "weathering effects to corners.\nNote: For Cycles, this may slow down renders significantly";
  ntype.enum_name_legacy = "AMBIENT_OCCLUSION";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_ambient_occlusion;
  ntype.initfunc = file_ns::node_shader_init_ambient_occlusion;
  ntype.gpu_fn = file_ns::node_shader_gpu_ambient_occlusion;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
