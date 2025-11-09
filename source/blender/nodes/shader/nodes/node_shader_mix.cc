/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include <algorithm>

#include "BKE_material.hh"

#include "BLI_math_quaternion.hh"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "DNA_material_types.h"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_shader_util.hh"
#include "node_util.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_multi_function.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

namespace blender::nodes::node_sh_mix_cc {

NODE_STORAGE_FUNCS(NodeShaderMix)

static void sh_node_mix_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  /* WARNING:
   * Input socket indices must be kept in sync with ntree_shader_disconnect_inactive_mix_branches
   */
  b.add_input<decl::Float>("Factor", "Factor_Float")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .no_muted_links()
      .description("Amount of mixing between the A and B inputs")
      .compositor_domain_priority(2);
  b.add_input<decl::Vector>("Factor", "Factor_Vector")
      .default_value(float3(0.5f))
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .no_muted_links()
      .description("Amount of mixing between the A and B vector inputs")
      .compositor_domain_priority(2);

  b.add_input<decl::Float>("A", "A_Float")
      .min(-10000.0f)
      .max(10000.0f)
      .is_default_link_socket()
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .description("Value of the first floating number input")
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("B", "B_Float")
      .min(-10000.0f)
      .max(10000.0f)
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .description("Value of the second floating number input")
      .compositor_domain_priority(1);

  b.add_input<decl::Vector>("A", "A_Vector")
      .is_default_link_socket()
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .description("Value of the first vector input")
      .compositor_domain_priority(0);
  b.add_input<decl::Vector>("B", "B_Vector")
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .description("Value of the second vector input")
      .compositor_domain_priority(1);

  b.add_input<decl::Color>("A", "A_Color")
      .default_value({0.5f, 0.5f, 0.5f, 1.0f})
      .is_default_link_socket()
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .description("Value of the first color input")
      .compositor_domain_priority(0);
  b.add_input<decl::Color>("B", "B_Color")
      .default_value({0.5f, 0.5f, 0.5f, 1.0f})
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .description("Value of the second color input")
      .compositor_domain_priority(1);

  b.add_input<decl::Rotation>("A", "A_Rotation")
      .is_default_link_socket()
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .compositor_domain_priority(0);
  b.add_input<decl::Rotation>("B", "B_Rotation")
      .translation_context(BLT_I18NCONTEXT_ID_NODETREE)
      .compositor_domain_priority(1);

  b.add_output<decl::Float>("Result", "Result_Float");
  b.add_output<decl::Vector>("Result", "Result_Vector");
  b.add_output<decl::Color>("Result", "Result_Color");
  b.add_output<decl::Rotation>("Result", "Result_Rotation");
};

static void sh_node_mix_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  const NodeShaderMix &data = node_storage(*static_cast<const bNode *>(ptr->data));
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  switch (data.data_type) {
    case SOCK_FLOAT:
      break;
    case SOCK_VECTOR:
      layout->prop(ptr, "factor_mode", UI_ITEM_NONE, "", ICON_NONE);
      break;
    case SOCK_RGBA:
      layout->prop(ptr, "blend_type", UI_ITEM_NONE, "", ICON_NONE);
      layout->prop(ptr, "clamp_result", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      break;
    case SOCK_ROTATION:
      break;
    default:
      BLI_assert_unreachable();
  }
  layout->prop(ptr, "clamp_factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void sh_node_mix_label(const bNodeTree * /*ntree*/,
                              const bNode *node,
                              char *label,
                              int label_maxncpy)
{
  const NodeShaderMix &storage = node_storage(*node);
  if (storage.data_type == SOCK_RGBA) {
    const char *name;
    bool enum_label = RNA_enum_name(rna_enum_ramp_blend_items, storage.blend_type, &name);
    if (!enum_label) {
      name = N_("Unknown");
    }
    BLI_strncpy_utf8(label, IFACE_(name), label_maxncpy);
    return;
  }
  BLI_strncpy_utf8(label, IFACE_("Mix"), label_maxncpy);
}

static int sh_node_mix_ui_class(const bNode *node)
{
  const NodeShaderMix &storage = node_storage(*node);
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);

  switch (data_type) {
    case SOCK_VECTOR:
      return NODE_CLASS_OP_VECTOR;
    case SOCK_RGBA:
      return NODE_CLASS_OP_COLOR;
    default:
      return NODE_CLASS_CONVERTER;
  }
}

static void sh_node_mix_update(bNodeTree *ntree, bNode *node)
{
  const NodeShaderMix &storage = node_storage(*node);
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);

  bNodeSocket *sock_factor = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *sock_factor_vec = static_cast<bNodeSocket *>(sock_factor->next);

  bool use_vector_factor = data_type == SOCK_VECTOR &&
                           storage.factor_mode != NODE_MIX_MODE_UNIFORM;

  bke::node_set_socket_availability(*ntree, *sock_factor, !use_vector_factor);

  bke::node_set_socket_availability(*ntree, *sock_factor_vec, use_vector_factor);

  for (bNodeSocket *socket = sock_factor_vec->next; socket != nullptr; socket = socket->next) {
    bke::node_set_socket_availability(*ntree, *socket, socket->type == data_type);
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    bke::node_set_socket_availability(*ntree, *socket, socket->type == data_type);
  }
}

class SocketSearchOp {
 public:
  std::string socket_name;
  int type = MA_RAMP_BLEND;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("ShaderNodeMix");
    node_storage(node).data_type = SOCK_RGBA;
    node_storage(node).blend_type = type;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void node_mix_gather_link_searches(GatherLinkSearchOpParams &params)
{
  eNodeSocketDatatype type;
  switch (eNodeSocketDatatype(params.other_socket().type)) {
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_FLOAT:
      type = SOCK_FLOAT;
      break;
    case SOCK_VECTOR:
      type = SOCK_VECTOR;
      break;
    case SOCK_RGBA:
      type = SOCK_RGBA;
      break;
    case SOCK_ROTATION:
      type = SOCK_ROTATION;
      break;
    default:
      return;
  }

  int weight = 0;
  if (params.in_out() == SOCK_OUT) {
    params.add_item(IFACE_("Result"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("ShaderNodeMix");
      node_storage(node).data_type = type;
      params.update_and_connect_available_socket(node, "Result");
    });
  }
  else {
    params.add_item(
        CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "A"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("ShaderNodeMix");
          node_storage(node).data_type = type;
          params.update_and_connect_available_socket(node, "A");
        },
        weight);
    weight--;
    params.add_item(
        CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "B"),
        [type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("ShaderNodeMix");
          node_storage(node).data_type = type;
          params.update_and_connect_available_socket(node, "B");
        },
        weight);
    weight--;
    if (ELEM(type, SOCK_VECTOR, SOCK_RGBA)) {
      params.add_item(
          IFACE_("Factor (Non-Uniform)"),
          [](LinkSearchOpParams &params) {
            bNode &node = params.add_node("ShaderNodeMix");
            node_storage(node).data_type = SOCK_VECTOR;
            node_storage(node).factor_mode = NODE_MIX_MODE_NON_UNIFORM;
            params.update_and_connect_available_socket(node, "Factor");
          },
          weight);
      weight--;
    }
    if (type != SOCK_ROTATION) {
      params.add_item(
          IFACE_("Factor"),
          [type](LinkSearchOpParams &params) {
            bNode &node = params.add_node("ShaderNodeMix");
            node_storage(node).data_type = type;
            params.update_and_connect_available_socket(node, "Factor");
          },
          weight);
      weight--;
    }
  }

  if (type == SOCK_ROTATION) {
    return;
  }

  if (type != SOCK_RGBA) {
    weight--;
  }
  const std::string socket_name = params.in_out() == SOCK_IN ? "A" : "Result";
  for (const EnumPropertyItem *item = rna_enum_ramp_blend_items; item->identifier != nullptr;
       item++)
  {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      params.add_item(IFACE_(item->name), SocketSearchOp{socket_name, item->value}, weight);
    }
  }
}

static void node_mix_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeShaderMix *data = MEM_callocN<NodeShaderMix>(__func__);
  data->data_type = SOCK_FLOAT;
  data->factor_mode = NODE_MIX_MODE_UNIFORM;
  data->clamp_factor = 1;
  data->clamp_result = 0;
  data->blend_type = MA_RAMP_BLEND;
  node->storage = data;
}

static const char *gpu_shader_get_name(eNodeSocketDatatype data_type,
                                       const bool non_uniform,
                                       const int blend_type)
{
  switch (data_type) {
    case SOCK_FLOAT:
      return "node_mix_float";
    case SOCK_VECTOR:
      return (non_uniform) ? "node_mix_vector_non_uniform" : "node_mix_vector";
    case SOCK_RGBA:
      switch (blend_type) {
        case MA_RAMP_BLEND:
          return "node_mix_blend";
        case MA_RAMP_ADD:
          return "node_mix_add";
        case MA_RAMP_MULT:
          return "node_mix_mult";
        case MA_RAMP_SUB:
          return "node_mix_sub";
        case MA_RAMP_SCREEN:
          return "node_mix_screen";
        case MA_RAMP_DIV:
          return "node_mix_div_fallback";
        case MA_RAMP_DIFF:
          return "node_mix_diff";
        case MA_RAMP_EXCLUSION:
          return "node_mix_exclusion";
        case MA_RAMP_DARK:
          return "node_mix_dark";
        case MA_RAMP_LIGHT:
          return "node_mix_light";
        case MA_RAMP_OVERLAY:
          return "node_mix_overlay";
        case MA_RAMP_DODGE:
          return "node_mix_dodge";
        case MA_RAMP_BURN:
          return "node_mix_burn";
        case MA_RAMP_HUE:
          return "node_mix_hue";
        case MA_RAMP_SAT:
          return "node_mix_sat";
        case MA_RAMP_VAL:
          return "node_mix_val";
        case MA_RAMP_COLOR:
          return "node_mix_color";
        case MA_RAMP_SOFT:
          return "node_mix_soft";
        case MA_RAMP_LINEAR:
          return "node_mix_linear";
        default:
          BLI_assert_unreachable();
          return nullptr;
      }
    case SOCK_ROTATION:
      return nullptr;
    default:
      BLI_assert_unreachable();
      return nullptr;
  }
}

static int gpu_shader_mix(GPUMaterial *mat,
                          bNode *node,
                          bNodeExecData * /*execdata*/,
                          GPUNodeStack *in,
                          GPUNodeStack *out)
{
  const NodeShaderMix &storage = node_storage(*node);
  const bool is_non_uniform = storage.factor_mode == NODE_MIX_MODE_NON_UNIFORM;
  const bool is_color_mode = storage.data_type == SOCK_RGBA;
  const bool is_vector_mode = storage.data_type == SOCK_VECTOR;
  const int blend_type = storage.blend_type;
  const char *name = gpu_shader_get_name(
      eNodeSocketDatatype(storage.data_type), is_non_uniform, blend_type);

  if (name == nullptr) {
    return 0;
  }

  if (storage.clamp_factor) {
    if (is_non_uniform && is_vector_mode) {
      const float min[3] = {0.0f, 0.0f, 0.0f};
      const float max[3] = {1.0f, 1.0f, 1.0f};
      const GPUNodeLink *factor_link = in[1].link ? in[1].link : GPU_uniform(in[1].vec);
      GPU_link(mat,
               "node_mix_clamp_vector",
               factor_link,
               GPU_constant(min),
               GPU_constant(max),
               &in[1].link);
    }
    else {
      const float min = 0.0f;
      const float max = 1.0f;
      const GPUNodeLink *factor_link = in[0].link ? in[0].link : GPU_uniform(in[0].vec);
      GPU_link(mat,
               "node_mix_clamp_value",
               factor_link,
               GPU_constant(&min),
               GPU_constant(&max),
               &in[0].link);
    }
  }

  int ret = GPU_stack_link(mat, node, name, in, out);

  if (ret && is_color_mode && storage.clamp_result) {
    const float min[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float max[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    GPU_link(mat,
             "node_mix_clamp_color",
             out[2].link,
             GPU_constant(min),
             GPU_constant(max),
             &out[2].link);
  }
  return ret;
}

class MixColorFunction : public mf::MultiFunction {
 private:
  const bool clamp_factor_;
  const bool clamp_result_;
  const int blend_type_;

 public:
  MixColorFunction(const bool clamp_factor, const bool clamp_result, const int blend_type)
      : clamp_factor_(clamp_factor), clamp_result_(clamp_result), blend_type_(blend_type)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"MixColor", signature};
      builder.single_input<float>("Factor");
      builder.single_input<ColorGeometry4f>("A");
      builder.single_input<ColorGeometry4f>("B");
      builder.single_output<ColorGeometry4f>("Result");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> &fac = params.readonly_single_input<float>(0, "Factor");
    const VArray<ColorGeometry4f> &col1 = params.readonly_single_input<ColorGeometry4f>(1, "A");
    const VArray<ColorGeometry4f> &col2 = params.readonly_single_input<ColorGeometry4f>(2, "B");
    MutableSpan<ColorGeometry4f> results = params.uninitialized_single_output<ColorGeometry4f>(
        3, "Result");

    if (clamp_factor_) {
      mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
        results[i] = col1[i];
        ramp_blend(blend_type_, results[i], std::clamp(fac[i], 0.0f, 1.0f), col2[i]);
      });
    }
    else {
      mask.foreach_index_optimized<int64_t>([&](const int64_t i) {
        results[i] = col1[i];
        ramp_blend(blend_type_, results[i], fac[i], col2[i]);
      });
    }

    if (clamp_result_) {
      mask.foreach_index_optimized<int64_t>(
          [&](const int64_t i) { clamp_v4(results[i], 0.0f, 1.0f); });
    }
  }
};

static const mf::MultiFunction *get_multi_function(const bNode &node)
{
  const NodeShaderMix *data = (NodeShaderMix *)node.storage;
  bool uniform_factor = data->factor_mode == NODE_MIX_MODE_UNIFORM;
  const bool clamp_factor = data->clamp_factor;
  switch (data->data_type) {
    case SOCK_FLOAT: {
      if (clamp_factor) {
        static auto fn = mf::build::SI3_SO<float, float, float, float>(
            "Clamp Mix Float", [](float t, const float a, const float b) {
              return math::interpolate(a, b, std::clamp(t, 0.0f, 1.0f));
            });
        return &fn;
      }
      static auto fn = mf::build::SI3_SO<float, float, float, float>(
          "Mix Float",
          [](const float t, const float a, const float b) { return math::interpolate(a, b, t); });
      return &fn;
    }
    case SOCK_VECTOR: {
      if (clamp_factor) {
        if (uniform_factor) {
          static auto fn = mf::build::SI3_SO<float, float3, float3, float3>(
              "Clamp Mix Vector", [](const float t, const float3 a, const float3 b) {
                return math::interpolate(a, b, std::clamp(t, 0.0f, 1.0f));
              });
          return &fn;
        }
        static auto fn = mf::build::SI3_SO<float3, float3, float3, float3>(
            "Clamp Mix Vector Non Uniform", [](float3 t, const float3 a, const float3 b) {
              t = math::clamp(t, 0.0f, 1.0f);
              return a * (float3(1.0f) - t) + b * t;
            });
        return &fn;
      }
      if (uniform_factor) {
        static auto fn = mf::build::SI3_SO<float, float3, float3, float3>(
            "Mix Vector", [](const float t, const float3 a, const float3 b) {
              return math::interpolate(a, b, t);
            });
        return &fn;
      }
      static auto fn = mf::build::SI3_SO<float3, float3, float3, float3>(
          "Mix Vector Non Uniform", [](const float3 t, const float3 a, const float3 b) {
            return a * (float3(1.0f) - t) + b * t;
          });
      return &fn;
    }
    case SOCK_ROTATION: {
      if (clamp_factor) {
        static auto fn =
            mf::build::SI3_SO<float, math::Quaternion, math::Quaternion, math::Quaternion>(
                "Clamp Mix Rotation",
                [](const float t, const math::Quaternion &a, const math::Quaternion &b) {
                  return math::interpolate(a, b, math::clamp(t, 0.0f, 1.0f));
                });
        return &fn;
      }
      static auto fn =
          mf::build::SI3_SO<float, math::Quaternion, math::Quaternion, math::Quaternion>(
              "Mix Rotation",
              [](const float t, const math::Quaternion &a, const math::Quaternion &b) {
                return math::interpolate(a, b, t);
              });
      return &fn;
    }
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void sh_node_mix_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeShaderMix &storage = node_storage(builder.node());

  if (storage.data_type == SOCK_RGBA) {
    builder.construct_and_set_matching_fn<MixColorFunction>(
        storage.clamp_factor, storage.clamp_result, storage.blend_type);
  }
  else {
    const mf::MultiFunction *fn = get_multi_function(builder.node());
    builder.set_matching_fn(fn);
  }
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  const NodeShaderMix *data = (NodeShaderMix *)node_->storage;

  NodeItem factor = empty();
  NodeItem value1 = empty();
  NodeItem value2 = empty();
  switch (data->data_type) {
    case SOCK_FLOAT:
      factor = get_input_value(0, NodeItem::Type::Float);
      value1 = get_input_value(2, NodeItem::Type::Float);
      value2 = get_input_value(3, NodeItem::Type::Float);
      break;

    case SOCK_VECTOR:
      if (data->factor_mode == NODE_MIX_MODE_UNIFORM) {
        factor = get_input_value(0, NodeItem::Type::Float);
      }
      else {
        factor = get_input_value(1, NodeItem::Type::Vector3);
      }
      value1 = get_input_value(4, NodeItem::Type::Vector3);
      value2 = get_input_value(5, NodeItem::Type::Vector3);
      break;

    case SOCK_RGBA:
      factor = get_input_value(0, NodeItem::Type::Float);
      value1 = get_input_value(6, NodeItem::Type::Color3);
      value2 = get_input_value(7, NodeItem::Type::Color3);
      break;

    default:
      BLI_assert_unreachable();
  }

  if (data->clamp_factor) {
    factor = factor.clamp();
  }
  NodeItem res = factor.mix(value1, value2);
  if (data->data_type == SOCK_RGBA) {
    /* TODO: Apply data->blend_type */

    if (data->clamp_result) {
      res = res.clamp();
    }
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_sh_mix_cc

void register_node_type_sh_mix()
{
  namespace file_ns = blender::nodes::node_sh_mix_cc;

  static blender::bke::bNodeType ntype;
  common_node_type_base(&ntype, "ShaderNodeMix", SH_NODE_MIX);
  ntype.ui_name = "Mix";
  ntype.ui_description = "Mix values by a factor";
  ntype.enum_name_legacy = "MIX";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_mix_declare;
  ntype.ui_class = file_ns::sh_node_mix_ui_class;
  ntype.gpu_fn = file_ns::gpu_shader_mix;
  ntype.updatefunc = file_ns::sh_node_mix_update;
  ntype.initfunc = file_ns::node_mix_init;
  blender::bke::node_type_storage(
      ntype, "NodeShaderMix", node_free_standard_storage, node_copy_standard_storage);
  ntype.build_multi_function = file_ns::sh_node_mix_build_multi_function;
  ntype.draw_buttons = file_ns::sh_node_mix_layout;
  ntype.labelfunc = file_ns::sh_node_mix_label;
  ntype.gather_link_search_ops = file_ns::node_mix_gather_link_searches;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
