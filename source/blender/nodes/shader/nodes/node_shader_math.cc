/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

#include "NOD_math_functions.hh"

/* **************** SCALAR MATH ******************** */
static bNodeSocketTemplate sh_node_math_in[] = {
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.0f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""}};

static bNodeSocketTemplate sh_node_math_out[] = {{SOCK_FLOAT, N_("Value")}, {-1, ""}};

static const char *gpu_shader_get_name(int mode)
{
  const blender::nodes::FloatMathOperationInfo *info =
      blender::nodes::get_float_math_operation_info(mode);
  if (!info) {
    return nullptr;
  }
  if (info->shader_name.is_empty()) {
    return nullptr;
  }
  return info->shader_name.c_str();
}

static int gpu_shader_math(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData *UNUSED(execdata),
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  if (name != nullptr) {
    int ret = GPU_stack_link(mat, node, name, in, out);

    if (ret && node->custom2 & SHD_MATH_CLAMP) {
      float min[3] = {0.0f, 0.0f, 0.0f};
      float max[3] = {1.0f, 1.0f, 1.0f};
      GPU_link(
          mat, "clamp_value", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
    }
    return ret;
  }

  return 0;
}

static const blender::fn::MultiFunction &get_base_multi_function(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  const int mode = builder.bnode().custom1;

  const blender::fn::MultiFunction *base_fn = nullptr;

  blender::nodes::try_dispatch_float_math_fl_to_fl(
      mode, [&](auto function, const blender::nodes::FloatMathOperationInfo &info) {
        static blender::fn::CustomMF_SI_SO<float, float> fn{info.title_case_name, function};
        base_fn = &fn;
      });
  if (base_fn != nullptr) {
    return *base_fn;
  }

  blender::nodes::try_dispatch_float_math_fl_fl_to_fl(
      mode, [&](auto function, const blender::nodes::FloatMathOperationInfo &info) {
        static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{info.title_case_name,
                                                                      function};
        base_fn = &fn;
      });
  if (base_fn != nullptr) {
    return *base_fn;
  }

  blender::nodes::try_dispatch_float_math_fl_fl_fl_to_fl(
      mode, [&](auto function, const blender::nodes::FloatMathOperationInfo &info) {
        static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, float> fn{
            info.title_case_name, function};
        base_fn = &fn;
      });
  if (base_fn != nullptr) {
    return *base_fn;
  }

  return builder.get_not_implemented_fn();
}

static void sh_node_math_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  const blender::fn::MultiFunction &base_function = get_base_multi_function(builder);

  const blender::nodes::DNode &dnode = builder.dnode();
  blender::fn::MFNetwork &network = builder.network();
  blender::fn::MFFunctionNode &base_node = network.add_function(base_function);

  builder.network_map().add_try_match(*dnode.context(), dnode->inputs(), base_node.inputs());

  const bool clamp_output = builder.bnode().custom2 != 0;
  if (clamp_output) {
    static blender::fn::CustomMF_SI_SO<float, float> clamp_fn{"Clamp", [](float value) {
                                                                CLAMP(value, 0.0f, 1.0f);
                                                                return value;
                                                              }};
    blender::fn::MFFunctionNode &clamp_node = network.add_function(clamp_fn);
    network.add_link(base_node.output(0), clamp_node.input(0));
    builder.network_map().add(blender::nodes::DOutputSocket(dnode.context(), &dnode->output(0)),
                              clamp_node.output(0));
  }
  else {
    builder.network_map().add(blender::nodes::DOutputSocket(dnode.context(), &dnode->output(0)),
                              base_node.output(0));
  }
}

void register_node_type_sh_math(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_math_in, sh_node_math_out);
  node_type_label(&ntype, node_math_label);
  node_type_gpu(&ntype, gpu_shader_math);
  node_type_update(&ntype, node_math_update);
  ntype.expand_in_mf_network = sh_node_math_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
