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

/* **************** Map Range ******************** */
static bNodeSocketTemplate sh_node_map_range_in[] = {
    {SOCK_FLOAT, N_("Value"), 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, PROP_NONE},
    {SOCK_FLOAT, N_("From Min"), 0.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("From Max"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("To Min"), 0.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("To Max"), 1.0f, 1.0f, 1.0f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Steps"), 4.0f, 1.0f, 1.0f, 1.0f, 0.0f, 10000.0f, PROP_NONE},
    {-1, ""},
};
static bNodeSocketTemplate sh_node_map_range_out[] = {
    {SOCK_FLOAT, N_("Result")},
    {-1, ""},
};

static void node_shader_update_map_range(bNodeTree *UNUSED(ntree), bNode *node)
{
  bNodeSocket *sockSteps = nodeFindSocket(node, SOCK_IN, "Steps");
  nodeSetSocketAvailability(sockSteps, node->custom2 == NODE_MAP_RANGE_STEPPED);
}

static void node_shader_init_map_range(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = true;                  /* use_clamp */
  node->custom2 = NODE_MAP_RANGE_LINEAR; /* interpolation */
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_MAP_RANGE_LINEAR:
      return "map_range_linear";
    case NODE_MAP_RANGE_STEPPED:
      return "map_range_stepped";
    case NODE_MAP_RANGE_SMOOTHSTEP:
      return "map_range_smoothstep";
    case NODE_MAP_RANGE_SMOOTHERSTEP:
      return "map_range_smootherstep";
  }

  return nullptr;
}

static int gpu_shader_map_range(GPUMaterial *mat,
                                bNode *node,
                                bNodeExecData *UNUSED(execdata),
                                GPUNodeStack *in,
                                GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom2);

  int ret = 0;
  if (name != nullptr) {
    ret = GPU_stack_link(mat, node, name, in, out);
  }
  else {
    ret = GPU_stack_link(mat, node, "map_range_linear", in, out);
  }
  if (ret && node->custom1 &&
      !ELEM(node->custom2, NODE_MAP_RANGE_SMOOTHSTEP, NODE_MAP_RANGE_SMOOTHERSTEP)) {
    GPU_link(mat, "clamp_range", out[0].link, in[3].link, in[4].link, &out[0].link);
  }
  return ret;
}

class MapRangeFunction : public blender::fn::MultiFunction {
 private:
  bool clamp_;

 public:
  MapRangeFunction(bool clamp) : clamp_(clamp)
  {
    blender::fn::MFSignatureBuilder signature = this->get_builder("Map Range");
    signature.single_input<float>("Value");
    signature.single_input<float>("From Min");
    signature.single_input<float>("From Max");
    signature.single_input<float>("To Min");
    signature.single_input<float>("To Max");
    signature.single_output<float>("Result");
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    blender::fn::VSpan<float> values = params.readonly_single_input<float>(0, "Value");
    blender::fn::VSpan<float> from_min = params.readonly_single_input<float>(1, "From Min");
    blender::fn::VSpan<float> from_max = params.readonly_single_input<float>(2, "From Max");
    blender::fn::VSpan<float> to_min = params.readonly_single_input<float>(3, "To Min");
    blender::fn::VSpan<float> to_max = params.readonly_single_input<float>(4, "To Max");
    blender::MutableSpan<float> results = params.uninitialized_single_output<float>(5, "Result");

    for (int64_t i : mask) {
      float factor = safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      results[i] = to_min[i] + factor * (to_max[i] - to_min[i]);
    }

    if (clamp_) {
      for (int64_t i : mask) {
        results[i] = (to_min[i] > to_max[i]) ? clamp_f(results[i], to_max[i], to_min[i]) :
                                               clamp_f(results[i], to_min[i], to_max[i]);
      }
    }
  }
};

static void sh_node_map_range_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  bNode &bnode = builder.bnode();
  bool clamp = bnode.custom1 != 0;
  int interpolation_type = bnode.custom2;

  if (interpolation_type == NODE_MAP_RANGE_LINEAR) {
    static MapRangeFunction fn_with_clamp{true};
    static MapRangeFunction fn_without_clamp{false};

    if (clamp) {
      builder.set_matching_fn(fn_with_clamp);
    }
    else {
      builder.set_matching_fn(fn_without_clamp);
    }
  }
  else {
    builder.set_not_implemented();
  }
}

void register_node_type_sh_map_range(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_MAP_RANGE, "Map Range", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_map_range_in, sh_node_map_range_out);
  node_type_init(&ntype, node_shader_init_map_range);
  node_type_update(&ntype, node_shader_update_map_range);
  node_type_gpu(&ntype, gpu_shader_map_range);
  ntype.expand_in_mf_network = sh_node_map_range_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
