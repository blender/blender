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

#include "BLI_math_base_safe.h"

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

static void map_range_signature(blender::fn::MFSignatureBuilder *signature, bool use_steps)
{
  signature->single_input<float>("Value");
  signature->single_input<float>("From Min");
  signature->single_input<float>("From Max");
  signature->single_input<float>("To Min");
  signature->single_input<float>("To Max");
  if (use_steps) {
    signature->single_input<float>("Steps");
  }
  signature->single_output<float>("Result");
}

class MapRangeFunction : public blender::fn::MultiFunction {
 private:
  bool clamp_;

 public:
  MapRangeFunction(bool clamp) : clamp_(clamp)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Map Range"};
    map_range_signature(&signature, false);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    const blender::VArray<float> &from_min = params.readonly_single_input<float>(1, "From Min");
    const blender::VArray<float> &from_max = params.readonly_single_input<float>(2, "From Max");
    const blender::VArray<float> &to_min = params.readonly_single_input<float>(3, "To Min");
    const blender::VArray<float> &to_max = params.readonly_single_input<float>(4, "To Max");
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

class MapRangeSteppedFunction : public blender::fn::MultiFunction {
 private:
  bool clamp_;

 public:
  MapRangeSteppedFunction(bool clamp) : clamp_(clamp)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Map Range Stepped"};
    map_range_signature(&signature, true);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    const blender::VArray<float> &from_min = params.readonly_single_input<float>(1, "From Min");
    const blender::VArray<float> &from_max = params.readonly_single_input<float>(2, "From Max");
    const blender::VArray<float> &to_min = params.readonly_single_input<float>(3, "To Min");
    const blender::VArray<float> &to_max = params.readonly_single_input<float>(4, "To Max");
    const blender::VArray<float> &steps = params.readonly_single_input<float>(5, "Steps");
    blender::MutableSpan<float> results = params.uninitialized_single_output<float>(6, "Result");

    for (int64_t i : mask) {
      float factor = safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      factor = safe_divide(floorf(factor * (steps[i] + 1.0f)), steps[i]);
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

class MapRangeSmoothstepFunction : public blender::fn::MultiFunction {
 public:
  MapRangeSmoothstepFunction()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Map Range Smoothstep"};
    map_range_signature(&signature, false);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    const blender::VArray<float> &from_min = params.readonly_single_input<float>(1, "From Min");
    const blender::VArray<float> &from_max = params.readonly_single_input<float>(2, "From Max");
    const blender::VArray<float> &to_min = params.readonly_single_input<float>(3, "To Min");
    const blender::VArray<float> &to_max = params.readonly_single_input<float>(4, "To Max");
    blender::MutableSpan<float> results = params.uninitialized_single_output<float>(5, "Result");

    for (int64_t i : mask) {
      float factor = safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      factor = std::clamp(factor, 0.0f, 1.0f);
      factor = (3.0f - 2.0f * factor) * (factor * factor);
      results[i] = to_min[i] + factor * (to_max[i] - to_min[i]);
    }
  }
};

class MapRangeSmootherstepFunction : public blender::fn::MultiFunction {
 public:
  MapRangeSmootherstepFunction()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Map Range Smoothstep"};
    map_range_signature(&signature, false);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    const blender::VArray<float> &from_min = params.readonly_single_input<float>(1, "From Min");
    const blender::VArray<float> &from_max = params.readonly_single_input<float>(2, "From Max");
    const blender::VArray<float> &to_min = params.readonly_single_input<float>(3, "To Min");
    const blender::VArray<float> &to_max = params.readonly_single_input<float>(4, "To Max");
    blender::MutableSpan<float> results = params.uninitialized_single_output<float>(5, "Result");

    for (int64_t i : mask) {
      float factor = safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      factor = std::clamp(factor, 0.0f, 1.0f);
      factor = factor * factor * factor * (factor * (factor * 6.0f - 15.0f) + 10.0f);
      results[i] = to_min[i] + factor * (to_max[i] - to_min[i]);
    }
  }
};

static void sh_node_map_range_expand_in_mf_network(blender::nodes::NodeMFNetworkBuilder &builder)
{
  bNode &bnode = builder.bnode();
  bool clamp = bnode.custom1 != 0;
  int interpolation_type = bnode.custom2;

  switch (interpolation_type) {
    case NODE_MAP_RANGE_LINEAR: {
      if (clamp) {
        static MapRangeFunction fn_with_clamp{true};
        builder.set_matching_fn(fn_with_clamp);
      }
      else {
        static MapRangeFunction fn_without_clamp{false};
        builder.set_matching_fn(fn_without_clamp);
      }
      break;
    }
    case NODE_MAP_RANGE_STEPPED: {
      if (clamp) {
        static MapRangeSteppedFunction fn_stepped_with_clamp{true};
        builder.set_matching_fn(fn_stepped_with_clamp);
      }
      else {
        static MapRangeSteppedFunction fn_stepped_without_clamp{false};
        builder.set_matching_fn(fn_stepped_without_clamp);
      }
      break;
    }
    case NODE_MAP_RANGE_SMOOTHSTEP: {
      static MapRangeSmoothstepFunction smoothstep;
      builder.set_matching_fn(smoothstep);
      break;
    }
    case NODE_MAP_RANGE_SMOOTHERSTEP: {
      static MapRangeSmootherstepFunction smootherstep;
      builder.set_matching_fn(smootherstep);
      break;
    }
    default:
      builder.set_not_implemented();
      break;
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
