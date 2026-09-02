/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender {

namespace nodes::node_shader_light_evaluation_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNodeTree *ntree = b.tree_or_null();
  const bool is_gpu_internal = ntree && (ntree->flag & NTREE_IS_GPU_SHADER_INTERNAL);

  b.add_input<decl::Int>("LightIndex"_ustr).available(is_gpu_internal);
  b.add_input<decl::Vector>("Position"_ustr)
      .hide_value()
      .description("Evaluation position. Surface position by default");
  b.add_input<decl::Vector>("Normal"_ustr)
      .hide_value()
      .description("Evaluation normal. Surface normal by default");
  b.add_input<decl::Float>("Roughness"_ustr)
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Evaluation roughness. Used for reflection factor")
      .make_available([](bNode &node) { node.custom1 = SHD_LIGHT_EVAL_GLOSSY; });

  b.add_output<decl::Float>("Factor"_ustr).description("Shading reflection factor, scaled by PI");
  b.add_output<decl::Float>("Mask"_ustr)
      .description(
          "All local lights: Mask to smooth the light cutoff.\n"
          "Spot lights: Cutoff mask and spot attenuation.\n"
          "Sun lights: 1.0");
  b.add_output<decl::Vector>("Direction"_ustr)
      .description(
          "Local lights: Normalized direction from evaluation position to light position.\n"
          "Sun lights: Sun direction * -1.0");
  b.add_output<decl::Float>("Distance"_ustr)
      .description(
          "Local lights: Distance from evaluation position to light position.\n"
          "Sun lights: 1.0");
}

static void node_shader_buts_light_evaluation(ui::Layout &layout,
                                              bContext * /*C*/,
                                              PointerRNA *ptr)
{
  layout.prop(ptr, "mode", ui::ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_update_light_evaluation(bNodeTree *ntree, bNode *node)
{
  const int mode = node->custom1;

  bke::node_set_socket_availability(*ntree,
                                    *bke::node_find_socket(*node, SOCK_IN, "Roughness"_ustr),
                                    mode == SHD_LIGHT_EVAL_GLOSSY);
}

static int node_shader_gpu_light_evaluation(GPUMaterial *mat,
                                            bNode *node,
                                            bNodeExecData * /*execdata*/,
                                            GPUNodeStack *in,
                                            GPUNodeStack *out)
{
  const int mode = node->custom1;

  if (!in[0].link) {
    /* Error: not linked to a light accumulation node */
    return false;
  }

  if (!in[1].link) {
    GPU_link(mat, "world_position_get", &in[1].link);
  }
  if (!in[2].link) {
    GPU_link(mat, "world_normals_get", &in[2].link);
  }

  if (out[0].hasoutput) {
    if (mode == SHD_LIGHT_EVAL_GLOSSY) {
      GPU_stack_link(mat, node, "node_light_evaluation_glossy", in, out);
    }
    else {
      GPU_stack_link(mat, node, "node_light_evaluation_diffuse", in, out);
    }
  }
  else {
    /* Simple path that doesn't compute LTC. */
    GPU_link(mat,
             "node_light_evaluation_common",
             in[0].link,
             in[1].link,
             &out[1].link,
             &out[2].link,
             &out[3].link);
  }

  return true;
}

}  // namespace nodes::node_shader_light_evaluation_cc

/* node type definition */
void register_node_type_sh_light_evaluation()
{
  namespace file_ns = nodes::node_shader_light_evaluation_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeLightEvaluation"_ustr, SH_NODE_LIGHT_EVALUATION);
  ntype.ui_name = "Light Evaluation";
  ntype.ui_description = "Light Evaluation";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_light_evaluation;
  ntype.gather_link_search_ops = search_link_ops_for_shader_material_lighting_node;
  ntype.gpu_fn = file_ns::node_shader_gpu_light_evaluation;
  ntype.updatefunc = file_ns::node_shader_update_light_evaluation;
  // ntype.materialx_fn = file_ns::node_shader_materialx;

  bke::node_register_type(ntype);
}

}  // namespace blender
