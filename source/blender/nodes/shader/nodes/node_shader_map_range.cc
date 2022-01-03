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

#include <algorithm>

#include "node_shader_util.h"

#include "BLI_math_base_safe.h"

#include "NOD_socket_search_link.hh"

NODE_STORAGE_FUNCS(NodeMapRange)

namespace blender::nodes::node_shader_map_range_cc {

static void sh_node_map_range_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Value")).min(-10000.0f).max(10000.0f).default_value(1.0f);
  b.add_input<decl::Float>(N_("From Min")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("From Max")).min(-10000.0f).max(10000.0f).default_value(1.0f);
  b.add_input<decl::Float>(N_("To Min")).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>(N_("To Max")).min(-10000.0f).max(10000.0f).default_value(1.0f);
  b.add_input<decl::Float>(N_("Steps")).min(-10000.0f).max(10000.0f).default_value(4.0f);
  b.add_input<decl::Vector>(N_("Vector")).min(0.0f).max(1.0f).hide_value();
  b.add_input<decl::Vector>(N_("From Min"), "From_Min_FLOAT3");
  b.add_input<decl::Vector>(N_("From Max"), "From_Max_FLOAT3").default_value(float3(1.0f));
  b.add_input<decl::Vector>(N_("To Min"), "To_Min_FLOAT3");
  b.add_input<decl::Vector>(N_("To Max"), "To_Max_FLOAT3").default_value(float3(1.0f));
  b.add_input<decl::Vector>(N_("Steps"), "Steps_FLOAT3").default_value(float3(4.0f));
  b.add_output<decl::Float>(N_("Result"));
  b.add_output<decl::Vector>(N_("Vector"));
};

static void node_shader_update_map_range(bNodeTree *ntree, bNode *node)
{
  const NodeMapRange &storage = node_storage(*node);
  const CustomDataType data_type = static_cast<CustomDataType>(storage.data_type);
  const int type = (data_type == CD_PROP_FLOAT) ? SOCK_FLOAT : SOCK_VECTOR;

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    nodeSetSocketAvailability(ntree, socket, socket->type == type);
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    nodeSetSocketAvailability(ntree, socket, socket->type == type);
  }

  if (storage.interpolation_type != NODE_MAP_RANGE_STEPPED) {
    if (type == SOCK_FLOAT) {
      bNodeSocket *sockSteps = (bNodeSocket *)BLI_findlink(&node->inputs, 5);
      nodeSetSocketAvailability(ntree, sockSteps, false);
    }
    else {
      bNodeSocket *sockSteps = (bNodeSocket *)BLI_findlink(&node->inputs, 11);
      nodeSetSocketAvailability(ntree, sockSteps, false);
    }
  }
}

static void node_shader_init_map_range(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeMapRange *data = MEM_cnew<NodeMapRange>(__func__);
  data->clamp = 1;
  data->data_type = CD_PROP_FLOAT;
  data->interpolation_type = NODE_MAP_RANGE_LINEAR;
  node->custom1 = true;                  /* use_clamp */
  node->custom2 = NODE_MAP_RANGE_LINEAR; /* interpolation */
  node->storage = data;
}

class SocketSearchOp {
 public:
  std::string socket_name;
  CustomDataType data_type;
  int interpolation_type = NODE_MAP_RANGE_LINEAR;

  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("ShaderNodeMapRange");
    node_storage(node).data_type = data_type;
    node_storage(node).interpolation_type = interpolation_type;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static std::optional<CustomDataType> node_type_from_other_socket(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
    case SOCK_BOOLEAN:
    case SOCK_INT:
      return CD_PROP_FLOAT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
      return CD_PROP_FLOAT3;
    default:
      return {};
  }
}

static void node_map_range_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const std::optional<CustomDataType> type = node_type_from_other_socket(params.other_socket());
  if (!type) {
    return;
  }

  if (params.in_out() == SOCK_IN) {
    if (*type == CD_PROP_FLOAT3) {
      params.add_item(IFACE_("Vector"), SocketSearchOp{"Vector", *type}, 0);
    }
    else {
      params.add_item(IFACE_("Value"), SocketSearchOp{"Value", *type}, 0);
    }
    params.add_item(IFACE_("From Min"), SocketSearchOp{"From Min", *type}, -1);
    params.add_item(IFACE_("From Max"), SocketSearchOp{"From Max", *type}, -1);
    params.add_item(IFACE_("To Min"), SocketSearchOp{"To Min", *type}, -2);
    params.add_item(IFACE_("To Max"), SocketSearchOp{"To Max", *type}, -2);
    params.add_item(IFACE_("Steps"), SocketSearchOp{"Steps", *type, NODE_MAP_RANGE_STEPPED}, -3);
  }
  else {
    if (*type == CD_PROP_FLOAT3) {
      params.add_item(IFACE_("Vector"), SocketSearchOp{"Vector", *type});
    }
    else {
      params.add_item(IFACE_("Result"), SocketSearchOp{"Result", *type});
    }
  }
}

static const char *gpu_shader_get_name(int mode, bool use_vector)
{
  if (use_vector) {
    switch (mode) {
      case NODE_MAP_RANGE_LINEAR:
        return "vector_map_range_linear";
      case NODE_MAP_RANGE_STEPPED:
        return "vector_map_range_stepped";
      case NODE_MAP_RANGE_SMOOTHSTEP:
        return "vector_map_range_smoothstep";
      case NODE_MAP_RANGE_SMOOTHERSTEP:
        return "vector_map_range_smootherstep";
    }
  }
  else {
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
  }

  return nullptr;
}

static int gpu_shader_map_range(GPUMaterial *mat,
                                bNode *node,
                                bNodeExecData *UNUSED(execdata),
                                GPUNodeStack *in,
                                GPUNodeStack *out)
{
  const NodeMapRange &storage = node_storage(*node);
  bool use_vector = (storage.data_type == CD_PROP_FLOAT3);
  const char *name = gpu_shader_get_name(storage.interpolation_type, use_vector);
  float clamp = storage.clamp ? 1.0f : 0.0f;
  int ret = 0;
  if (name != nullptr) {
    ret = GPU_stack_link(mat, node, name, in, out, GPU_constant(&clamp));
  }
  else {
    ret = GPU_stack_link(mat, node, "map_range_linear", in, out, GPU_constant(&clamp));
  }
  if (ret && storage.clamp && !use_vector &&
      !ELEM(storage.interpolation_type, NODE_MAP_RANGE_SMOOTHSTEP, NODE_MAP_RANGE_SMOOTHERSTEP)) {
    GPU_link(mat, "clamp_range", out[0].link, in[3].link, in[4].link, &out[0].link);
  }
  return ret;
}

static inline float clamp_range(const float value, const float min, const float max)
{
  return (min > max) ? std::clamp(value, max, min) : std::clamp(value, min, max);
}

static float3 clamp_range(const float3 value, const float3 min, const float3 max)
{
  return float3(clamp_range(value.x, min.x, max.x),
                clamp_range(value.y, min.y, max.y),
                clamp_range(value.z, min.z, max.z));
}

static void map_range_vector_signature(blender::fn::MFSignatureBuilder *signature, bool use_steps)
{
  signature->single_input<float3>("Vector");
  signature->single_input<float3>("From Min");
  signature->single_input<float3>("From Max");
  signature->single_input<float3>("To Min");
  signature->single_input<float3>("To Max");
  if (use_steps) {
    signature->single_input<float3>("Steps");
  }
  signature->single_output<float3>("Vector");
}

class MapRangeVectorFunction : public blender::fn::MultiFunction {
 private:
  bool clamp_;

 public:
  MapRangeVectorFunction(bool clamp) : clamp_(clamp)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Vector Map Range"};
    map_range_vector_signature(&signature, false);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float3> &values = params.readonly_single_input<float3>(0, "Vector");
    const blender::VArray<float3> &from_min = params.readonly_single_input<float3>(1, "From Min");
    const blender::VArray<float3> &from_max = params.readonly_single_input<float3>(2, "From Max");
    const blender::VArray<float3> &to_min = params.readonly_single_input<float3>(3, "To Min");
    const blender::VArray<float3> &to_max = params.readonly_single_input<float3>(4, "To Max");
    blender::MutableSpan<float3> results = params.uninitialized_single_output<float3>(5, "Vector");

    for (int64_t i : mask) {
      float3 factor = float3::safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      results[i] = factor * (to_max[i] - to_min[i]) + to_min[i];
    }

    if (clamp_) {
      for (int64_t i : mask) {
        results[i] = clamp_range(results[i], to_min[i], to_max[i]);
      }
    }
  }
};

class MapRangeSteppedVectorFunction : public blender::fn::MultiFunction {
 private:
  bool clamp_;

 public:
  MapRangeSteppedVectorFunction(bool clamp) : clamp_(clamp)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Vector Map Range Stepped"};
    map_range_vector_signature(&signature, true);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float3> &values = params.readonly_single_input<float3>(0, "Vector");
    const blender::VArray<float3> &from_min = params.readonly_single_input<float3>(1, "From Min");
    const blender::VArray<float3> &from_max = params.readonly_single_input<float3>(2, "From Max");
    const blender::VArray<float3> &to_min = params.readonly_single_input<float3>(3, "To Min");
    const blender::VArray<float3> &to_max = params.readonly_single_input<float3>(4, "To Max");
    const blender::VArray<float3> &steps = params.readonly_single_input<float3>(5, "Steps");
    blender::MutableSpan<float3> results = params.uninitialized_single_output<float3>(6, "Vector");

    for (int64_t i : mask) {
      float3 factor = float3::safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      factor = float3::safe_divide(float3::floor(factor * (steps[i] + 1.0f)), steps[i]);
      results[i] = factor * (to_max[i] - to_min[i]) + to_min[i];
    }

    if (clamp_) {
      for (int64_t i : mask) {
        results[i] = clamp_range(results[i], to_min[i], to_max[i]);
      }
    }
  }
};

class MapRangeSmoothstepVectorFunction : public blender::fn::MultiFunction {
 public:
  MapRangeSmoothstepVectorFunction()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Vector Map Range Smoothstep"};
    map_range_vector_signature(&signature, false);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float3> &values = params.readonly_single_input<float3>(0, "Vector");
    const blender::VArray<float3> &from_min = params.readonly_single_input<float3>(1, "From Min");
    const blender::VArray<float3> &from_max = params.readonly_single_input<float3>(2, "From Max");
    const blender::VArray<float3> &to_min = params.readonly_single_input<float3>(3, "To Min");
    const blender::VArray<float3> &to_max = params.readonly_single_input<float3>(4, "To Max");
    blender::MutableSpan<float3> results = params.uninitialized_single_output<float3>(5, "Vector");

    for (int64_t i : mask) {
      float3 factor = float3::safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      clamp_v3(factor, 0.0f, 1.0f);
      factor = (float3(3.0f) - 2.0f * factor) * (factor * factor);
      results[i] = factor * (to_max[i] - to_min[i]) + to_min[i];
    }
  }
};

class MapRangeSmootherstepVectorFunction : public blender::fn::MultiFunction {
 public:
  MapRangeSmootherstepVectorFunction()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Vector Map Range Smoothstep"};
    map_range_vector_signature(&signature, false);
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float3> &values = params.readonly_single_input<float3>(0, "Vector");
    const blender::VArray<float3> &from_min = params.readonly_single_input<float3>(1, "From Min");
    const blender::VArray<float3> &from_max = params.readonly_single_input<float3>(2, "From Max");
    const blender::VArray<float3> &to_min = params.readonly_single_input<float3>(3, "To Min");
    const blender::VArray<float3> &to_max = params.readonly_single_input<float3>(4, "To Max");
    blender::MutableSpan<float3> results = params.uninitialized_single_output<float3>(5, "Vector");

    for (int64_t i : mask) {
      float3 factor = float3::safe_divide(values[i] - from_min[i], from_max[i] - from_min[i]);
      clamp_v3(factor, 0.0f, 1.0f);
      factor = factor * factor * factor * (factor * (factor * 6.0f - 15.0f) + 10.0f);
      results[i] = factor * (to_max[i] - to_min[i]) + to_min[i];
    }
  }
};

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
        results[i] = clamp_range(results[i], to_min[i], to_max[i]);
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
        results[i] = clamp_range(results[i], to_min[i], to_max[i]);
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

static void sh_node_map_range_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  const NodeMapRange &storage = node_storage(builder.node());
  bool clamp = storage.clamp != 0;
  int interpolation_type = storage.interpolation_type;

  switch (storage.data_type) {
    case CD_PROP_FLOAT3:
      switch (interpolation_type) {
        case NODE_MAP_RANGE_LINEAR: {
          if (clamp) {
            static MapRangeVectorFunction fn_with_clamp{true};
            builder.set_matching_fn(fn_with_clamp);
          }
          else {
            static MapRangeVectorFunction fn_without_clamp{false};
            builder.set_matching_fn(fn_without_clamp);
          }
          break;
        }
        case NODE_MAP_RANGE_STEPPED: {
          if (clamp) {
            static MapRangeSteppedVectorFunction fn_stepped_with_clamp{true};
            builder.set_matching_fn(fn_stepped_with_clamp);
          }
          else {
            static MapRangeSteppedVectorFunction fn_stepped_without_clamp{false};
            builder.set_matching_fn(fn_stepped_without_clamp);
          }
          break;
        }
        case NODE_MAP_RANGE_SMOOTHSTEP: {
          static MapRangeSmoothstepVectorFunction smoothstep;
          builder.set_matching_fn(smoothstep);
          break;
        }
        case NODE_MAP_RANGE_SMOOTHERSTEP: {
          static MapRangeSmootherstepVectorFunction smootherstep;
          builder.set_matching_fn(smootherstep);
          break;
        }
        default:
          break;
      }
      break;
    case CD_PROP_FLOAT:
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
          break;
      }
      break;
  }
}

}  // namespace blender::nodes::node_shader_map_range_cc

void register_node_type_sh_map_range()
{
  namespace file_ns = blender::nodes::node_shader_map_range_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_MAP_RANGE, "Map Range", NODE_CLASS_CONVERTER, 0);
  ntype.declare = file_ns::sh_node_map_range_declare;
  node_type_init(&ntype, file_ns::node_shader_init_map_range);
  node_type_storage(
      &ntype, "NodeMapRange", node_free_standard_storage, node_copy_standard_storage);
  node_type_update(&ntype, file_ns::node_shader_update_map_range);
  node_type_gpu(&ntype, file_ns::gpu_shader_map_range);
  ntype.build_multi_function = file_ns::sh_node_map_range_build_multi_function;
  ntype.gather_link_search_ops = file_ns::node_map_range_gather_link_searches;
  nodeRegisterType(&ntype);
}
