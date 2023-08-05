/* SPDX-FileCopyrightText: 2005 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include <algorithm>

#include "node_shader_util.hh"

#include "BLI_math_base_safe.h"

#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_map_range_cc {

NODE_STORAGE_FUNCS(NodeMapRange)

static void sh_node_map_range_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Value").min(-10000.0f).max(10000.0f).default_value(1.0f);
  b.add_input<decl::Float>("From Min").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("From Max").min(-10000.0f).max(10000.0f).default_value(1.0f);
  b.add_input<decl::Float>("To Min").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("To Max").min(-10000.0f).max(10000.0f).default_value(1.0f);
  b.add_input<decl::Float>("Steps").min(-10000.0f).max(10000.0f).default_value(4.0f);
  b.add_input<decl::Vector>("Vector").min(0.0f).max(1.0f).hide_value();
  b.add_input<decl::Vector>("From Min", "From_Min_FLOAT3");
  b.add_input<decl::Vector>("From Max", "From_Max_FLOAT3").default_value(float3(1.0f));
  b.add_input<decl::Vector>("To Min", "To_Min_FLOAT3");
  b.add_input<decl::Vector>("To Max", "To_Max_FLOAT3").default_value(float3(1.0f));
  b.add_input<decl::Vector>("Steps", "Steps_FLOAT3").default_value(float3(4.0f));
  b.add_output<decl::Float>("Result");
  b.add_output<decl::Vector>("Vector");
}

static void node_shader_buts_map_range(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "interpolation_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (!ELEM(RNA_enum_get(ptr, "interpolation_type"),
            NODE_MAP_RANGE_SMOOTHSTEP,
            NODE_MAP_RANGE_SMOOTHERSTEP))
  {
    uiItemR(layout, ptr, "clamp", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
}

static int node_shader_map_range_ui_class(const bNode *node)
{
  const NodeMapRange &storage = node_storage(*node);
  const eCustomDataType data_type = static_cast<eCustomDataType>(storage.data_type);
  if (data_type == CD_PROP_FLOAT3) {
    return NODE_CLASS_OP_VECTOR;
  }
  return NODE_CLASS_CONVERTER;
}

static void node_shader_update_map_range(bNodeTree *ntree, bNode *node)
{
  const NodeMapRange &storage = node_storage(*node);
  const eCustomDataType data_type = static_cast<eCustomDataType>(storage.data_type);
  const int type = (data_type == CD_PROP_FLOAT) ? SOCK_FLOAT : SOCK_VECTOR;

  Array<bool> new_input_availability(BLI_listbase_count(&node->inputs));
  Array<bool> new_output_availability(BLI_listbase_count(&node->outputs));

  int index;
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &node->inputs, index) {
    new_input_availability[index] = socket->type == type;
  }
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &node->outputs, index) {
    new_output_availability[index] = socket->type == type;
  }

  if (storage.interpolation_type != NODE_MAP_RANGE_STEPPED) {
    if (type == SOCK_FLOAT) {
      new_input_availability[5] = false;
    }
    else {
      new_input_availability[11] = false;
    }
  }

  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &node->inputs, index) {
    bke::nodeSetSocketAvailability(ntree, socket, new_input_availability[index]);
  }
  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &node->outputs, index) {
    bke::nodeSetSocketAvailability(ntree, socket, new_output_availability[index]);
  }
}

static void node_shader_init_map_range(bNodeTree * /*ntree*/, bNode *node)
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
  eCustomDataType data_type;
  int interpolation_type = NODE_MAP_RANGE_LINEAR;

  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("ShaderNodeMapRange");
    node_storage(node).data_type = data_type;
    node_storage(node).interpolation_type = interpolation_type;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static std::optional<eCustomDataType> node_type_from_other_socket(const bNodeSocket &socket)
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
  const std::optional<eCustomDataType> type = node_type_from_other_socket(params.other_socket());
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
                                bNodeExecData * /*execdata*/,
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
      !ELEM(storage.interpolation_type, NODE_MAP_RANGE_SMOOTHSTEP, NODE_MAP_RANGE_SMOOTHERSTEP))
  {
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

template<bool Clamp> static auto build_float_linear()
{
  return mf::build::SI5_SO<float, float, float, float, float, float>(
      Clamp ? "Map Range (clamped)" : "Map Range (unclamped)",
      [](float value, float from_min, float from_max, float to_min, float to_max) -> float {
        const float factor = safe_divide(value - from_min, from_max - from_min);
        float result = to_min + factor * (to_max - to_min);
        if constexpr (Clamp) {
          result = clamp_range(result, to_min, to_max);
        }
        return result;
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
}

template<bool Clamp> static auto build_float_stepped()
{
  return mf::build::SI6_SO<float, float, float, float, float, float, float>(
      Clamp ? "Map Range Stepped (clamped)" : "Map Range Stepped (unclamped)",
      [](float value, float from_min, float from_max, float to_min, float to_max, float steps)
          -> float {
        float factor = safe_divide(value - from_min, from_max - from_min);
        factor = safe_divide(floorf(factor * (steps + 1.0f)), steps);
        float result = to_min + factor * (to_max - to_min);
        if constexpr (Clamp) {
          result = clamp_range(result, to_min, to_max);
        }
        return result;
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
}

template<bool Clamp> static auto build_vector_linear()
{
  return mf::build::SI5_SO<float3, float3, float3, float3, float3, float3>(
      Clamp ? "Vector Map Range (clamped)" : "Vector Map Range (unclamped)",
      [](const float3 &value,
         const float3 &from_min,
         const float3 &from_max,
         const float3 &to_min,
         const float3 &to_max) -> float3 {
        float3 factor = math::safe_divide(value - from_min, from_max - from_min);
        float3 result = factor * (to_max - to_min) + to_min;
        if constexpr (Clamp) {
          result = clamp_range(result, to_min, to_max);
        }
        return result;
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
}

template<bool Clamp> static auto build_vector_stepped()
{
  return mf::build::SI6_SO<float3, float3, float3, float3, float3, float3, float3>(
      Clamp ? "Vector Map Range Stepped (clamped)" : "Vector Map Range Stepped (unclamped)",
      [](const float3 &value,
         const float3 &from_min,
         const float3 &from_max,
         const float3 &to_min,
         const float3 &to_max,
         const float3 &steps) -> float3 {
        float3 factor = math::safe_divide(value - from_min, from_max - from_min);
        factor = math::safe_divide(math::floor(factor * (steps + 1.0f)), steps);
        float3 result = factor * (to_max - to_min) + to_min;
        if constexpr (Clamp) {
          result = clamp_range(result, to_min, to_max);
        }
        return result;
      },
      mf::build::exec_presets::SomeSpanOrSingle<0>());
}

static void sh_node_map_range_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeMapRange &storage = node_storage(builder.node());
  bool clamp = storage.clamp != 0;
  int interpolation_type = storage.interpolation_type;

  switch (storage.data_type) {
    case CD_PROP_FLOAT3:
      switch (interpolation_type) {
        case NODE_MAP_RANGE_LINEAR: {
          if (clamp) {
            static auto fn = build_vector_linear<true>();
            builder.set_matching_fn(fn);
          }
          else {
            static auto fn = build_vector_linear<false>();
            builder.set_matching_fn(fn);
          }
          break;
        }
        case NODE_MAP_RANGE_STEPPED: {
          if (clamp) {
            static auto fn = build_vector_stepped<true>();
            builder.set_matching_fn(fn);
          }
          else {
            static auto fn = build_vector_stepped<false>();
            builder.set_matching_fn(fn);
          }
          break;
        }
        case NODE_MAP_RANGE_SMOOTHSTEP: {
          static auto fn = mf::build::SI5_SO<float3, float3, float3, float3, float3, float3>(
              "Vector Map Range Smoothstep",
              [](const float3 &value,
                 const float3 &from_min,
                 const float3 &from_max,
                 const float3 &to_min,
                 const float3 &to_max) -> float3 {
                float3 factor = math::safe_divide(value - from_min, from_max - from_min);
                clamp_v3(factor, 0.0f, 1.0f);
                factor = (float3(3.0f) - 2.0f * factor) * (factor * factor);
                return factor * (to_max - to_min) + to_min;
              },
              mf::build::exec_presets::SomeSpanOrSingle<0>());
          builder.set_matching_fn(fn);
          break;
        }
        case NODE_MAP_RANGE_SMOOTHERSTEP: {
          static auto fn = mf::build::SI5_SO<float3, float3, float3, float3, float3, float3>(
              "Vector Map Range Smootherstep",
              [](const float3 &value,
                 const float3 &from_min,
                 const float3 &from_max,
                 const float3 &to_min,
                 const float3 &to_max) -> float3 {
                float3 factor = math::safe_divide(value - from_min, from_max - from_min);
                clamp_v3(factor, 0.0f, 1.0f);
                factor = factor * factor * factor * (factor * (factor * 6.0f - 15.0f) + 10.0f);
                return factor * (to_max - to_min) + to_min;
              },
              mf::build::exec_presets::SomeSpanOrSingle<0>());
          builder.set_matching_fn(fn);
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
            static auto fn = build_float_linear<true>();
            builder.set_matching_fn(fn);
          }
          else {
            static auto fn = build_float_linear<false>();
            builder.set_matching_fn(fn);
          }
          break;
        }
        case NODE_MAP_RANGE_STEPPED: {
          if (clamp) {
            static auto fn = build_float_stepped<true>();
            builder.set_matching_fn(fn);
          }
          else {
            static auto fn = build_float_stepped<false>();
            builder.set_matching_fn(fn);
          }
          break;
        }
        case NODE_MAP_RANGE_SMOOTHSTEP: {
          static auto fn = mf::build::SI5_SO<float, float, float, float, float, float>(
              "Map Range Smoothstep",
              [](float value, float from_min, float from_max, float to_min, float to_max)
                  -> float {
                float factor = safe_divide(value - from_min, from_max - from_min);
                factor = std::clamp(factor, 0.0f, 1.0f);
                factor = (3.0f - 2.0f * factor) * (factor * factor);
                return to_min + factor * (to_max - to_min);
              },
              mf::build::exec_presets::SomeSpanOrSingle<0>());
          builder.set_matching_fn(fn);
          break;
        }
        case NODE_MAP_RANGE_SMOOTHERSTEP: {
          static auto fn = mf::build::SI5_SO<float, float, float, float, float, float>(
              "Map Range Smoothstep",
              [](float value, float from_min, float from_max, float to_min, float to_max)
                  -> float {
                float factor = safe_divide(value - from_min, from_max - from_min);
                factor = std::clamp(factor, 0.0f, 1.0f);
                factor = factor * factor * factor * (factor * (factor * 6.0f - 15.0f) + 10.0f);
                return to_min + factor * (to_max - to_min);
              },
              mf::build::exec_presets::SomeSpanOrSingle<0>());
          builder.set_matching_fn(fn);
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

  sh_fn_node_type_base(&ntype, SH_NODE_MAP_RANGE, "Map Range", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_map_range_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_map_range;
  ntype.ui_class = file_ns::node_shader_map_range_ui_class;
  ntype.initfunc = file_ns::node_shader_init_map_range;
  node_type_storage(
      &ntype, "NodeMapRange", node_free_standard_storage, node_copy_standard_storage);
  ntype.updatefunc = file_ns::node_shader_update_map_range;
  ntype.gpu_fn = file_ns::gpu_shader_map_range;
  ntype.build_multi_function = file_ns::sh_node_map_range_build_multi_function;
  ntype.gather_link_search_ops = file_ns::node_map_range_gather_link_searches;
  nodeRegisterType(&ntype);
}
