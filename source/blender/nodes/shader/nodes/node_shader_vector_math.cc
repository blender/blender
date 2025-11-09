/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_math_functions.hh"
#include "NOD_multi_function.hh"
#include "NOD_socket_search_link.hh"
#include "NOD_value_elem_eval.hh"

#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_vector_math_cc {

static void sh_node_vector_math_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).label_fn([](bNode node) {
    switch (node.custom1) {
      case NODE_VECTOR_MATH_POWER:
        return IFACE_("Base");
      default:
        return IFACE_("Vector");
    }
  });
  b.add_input<decl::Vector>("Vector", "Vector_001")
      .min(-10000.0f)
      .max(10000.0f)
      .label_fn([](bNode node) {
        switch (node.custom1) {
          case NODE_VECTOR_MATH_POWER:
            return IFACE_("Exponent");
          case NODE_VECTOR_MATH_MULTIPLY_ADD:
            return IFACE_("Multiplier");
          case NODE_VECTOR_MATH_FACEFORWARD:
            return IFACE_("Incident");
          case NODE_VECTOR_MATH_WRAP:
            return IFACE_("Max");
          case NODE_VECTOR_MATH_SNAP:
            return IFACE_("Increment");
          default:
            return IFACE_("Vector");
        }
      });
  b.add_input<decl::Vector>("Vector", "Vector_002")
      .min(-10000.0f)
      .max(10000.0f)
      .label_fn([](bNode node) {
        switch (node.custom1) {
          case NODE_VECTOR_MATH_MULTIPLY_ADD:
            return IFACE_("Addend");
          case NODE_VECTOR_MATH_FACEFORWARD:
            return IFACE_("Reference");
          case NODE_VECTOR_MATH_WRAP:
            return IFACE_("Min");
          default:
            return IFACE_("Vector");
        }
      });
  b.add_input<decl::Float>("Scale").default_value(1.0f).min(-10000.0f).max(10000.0f).label_fn(
      [](bNode node) {
        switch (node.custom1) {
          case NODE_VECTOR_MATH_SCALE:
          default:
            return IFACE_("Scale");
          case NODE_VECTOR_MATH_REFRACT:
            return IFACE_("IOR");
        }
      });
  b.add_output<decl::Vector>("Vector");
  b.add_output<decl::Float>("Value");
}

static void node_shader_buts_vect_math(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "operation", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

class SocketSearchOp {
 public:
  std::string socket_name;
  NodeVectorMathOperation mode = NODE_VECTOR_MATH_ADD;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("ShaderNodeVectorMath");
    node.custom1 = mode;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void sh_node_vector_math_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                  SOCK_VECTOR))
  {
    return;
  }

  const int weight = ELEM(params.other_socket().type, SOCK_VECTOR, SOCK_RGBA) ? 0 : -1;

  for (const EnumPropertyItem *item = rna_enum_node_vec_math_items; item->identifier != nullptr;
       item++)
  {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      if ((params.in_out() == SOCK_OUT) && ELEM(item->value,
                                                NODE_VECTOR_MATH_LENGTH,
                                                NODE_VECTOR_MATH_DISTANCE,
                                                NODE_VECTOR_MATH_DOT_PRODUCT))
      {
        params.add_item(CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, item->name),
                        SocketSearchOp{"Value", (NodeVectorMathOperation)item->value},
                        weight);
      }
      else {
        params.add_item(CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, item->name),
                        SocketSearchOp{"Vector", (NodeVectorMathOperation)item->value},
                        weight);
      }
    }
  }
}

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_VECTOR_MATH_ADD:
      return "vector_math_add";
    case NODE_VECTOR_MATH_SUBTRACT:
      return "vector_math_subtract";
    case NODE_VECTOR_MATH_MULTIPLY:
      return "vector_math_multiply";
    case NODE_VECTOR_MATH_DIVIDE:
      return "vector_math_divide";

    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      return "vector_math_cross";
    case NODE_VECTOR_MATH_PROJECT:
      return "vector_math_project";
    case NODE_VECTOR_MATH_REFLECT:
      return "vector_math_reflect";
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      return "vector_math_dot";

    case NODE_VECTOR_MATH_DISTANCE:
      return "vector_math_distance";
    case NODE_VECTOR_MATH_LENGTH:
      return "vector_math_length";
    case NODE_VECTOR_MATH_SCALE:
      return "vector_math_scale";
    case NODE_VECTOR_MATH_NORMALIZE:
      return "vector_math_normalize";

    case NODE_VECTOR_MATH_SNAP:
      return "vector_math_snap";
    case NODE_VECTOR_MATH_FLOOR:
      return "vector_math_floor";
    case NODE_VECTOR_MATH_CEIL:
      return "vector_math_ceil";
    case NODE_VECTOR_MATH_MODULO:
      return "vector_math_modulo";
    case NODE_VECTOR_MATH_FRACTION:
      return "vector_math_fraction";
    case NODE_VECTOR_MATH_ABSOLUTE:
      return "vector_math_absolute";
    case NODE_VECTOR_MATH_MINIMUM:
      return "vector_math_minimum";
    case NODE_VECTOR_MATH_MAXIMUM:
      return "vector_math_maximum";
    case NODE_VECTOR_MATH_WRAP:
      return "vector_math_wrap";
    case NODE_VECTOR_MATH_SINE:
      return "vector_math_sine";
    case NODE_VECTOR_MATH_COSINE:
      return "vector_math_cosine";
    case NODE_VECTOR_MATH_TANGENT:
      return "vector_math_tangent";
    case NODE_VECTOR_MATH_REFRACT:
      return "vector_math_refract";
    case NODE_VECTOR_MATH_FACEFORWARD:
      return "vector_math_faceforward";
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      return "vector_math_multiply_add";
    case NODE_VECTOR_MATH_POWER:
      return "vector_math_power";
    case NODE_VECTOR_MATH_SIGN:
      return "vector_math_sign";
  }

  return nullptr;
}

static int gpu_shader_vector_math(GPUMaterial *mat,
                                  bNode *node,
                                  bNodeExecData * /*execdata*/,
                                  GPUNodeStack *in,
                                  GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  if (name != nullptr) {
    return GPU_stack_link(mat, node, name, in, out);
  }

  return 0;
}

static void node_shader_update_vector_math(bNodeTree *ntree, bNode *node)
{
  bNodeSocket *sockB = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *sockC = (bNodeSocket *)BLI_findlink(&node->inputs, 2);
  bNodeSocket *sockScale = bke::node_find_socket(*node, SOCK_IN, "Scale");

  bNodeSocket *sockVector = bke::node_find_socket(*node, SOCK_OUT, "Vector");
  bNodeSocket *sockValue = bke::node_find_socket(*node, SOCK_OUT, "Value");

  bke::node_set_socket_availability(*ntree,
                                    *sockB,
                                    !ELEM(node->custom1,
                                          NODE_VECTOR_MATH_SINE,
                                          NODE_VECTOR_MATH_COSINE,
                                          NODE_VECTOR_MATH_TANGENT,
                                          NODE_VECTOR_MATH_CEIL,
                                          NODE_VECTOR_MATH_SCALE,
                                          NODE_VECTOR_MATH_FLOOR,
                                          NODE_VECTOR_MATH_LENGTH,
                                          NODE_VECTOR_MATH_ABSOLUTE,
                                          NODE_VECTOR_MATH_FRACTION,
                                          NODE_VECTOR_MATH_NORMALIZE,
                                          NODE_VECTOR_MATH_SIGN));
  bke::node_set_socket_availability(*ntree,
                                    *sockC,
                                    ELEM(node->custom1,
                                         NODE_VECTOR_MATH_WRAP,
                                         NODE_VECTOR_MATH_FACEFORWARD,
                                         NODE_VECTOR_MATH_MULTIPLY_ADD));
  bke::node_set_socket_availability(
      *ntree, *sockScale, ELEM(node->custom1, NODE_VECTOR_MATH_SCALE, NODE_VECTOR_MATH_REFRACT));
  bke::node_set_socket_availability(*ntree,
                                    *sockVector,
                                    !ELEM(node->custom1,
                                          NODE_VECTOR_MATH_LENGTH,
                                          NODE_VECTOR_MATH_DISTANCE,
                                          NODE_VECTOR_MATH_DOT_PRODUCT));
  bke::node_set_socket_availability(*ntree,
                                    *sockValue,
                                    ELEM(node->custom1,
                                         NODE_VECTOR_MATH_LENGTH,
                                         NODE_VECTOR_MATH_DISTANCE,
                                         NODE_VECTOR_MATH_DOT_PRODUCT));
}

static const mf::MultiFunction *get_multi_function(const bNode &node)
{
  NodeVectorMathOperation operation = NodeVectorMathOperation(node.custom1);

  const mf::MultiFunction *multi_fn = nullptr;

  try_dispatch_float_math_fl3_fl3_to_fl3(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI2_SO<float3, float3, float3>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  try_dispatch_float_math_fl3_fl3_fl3_to_fl3(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI3_SO<float3, float3, float3, float3>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  try_dispatch_float_math_fl3_fl3_fl_to_fl3(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI3_SO<float3, float3, float, float3>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  try_dispatch_float_math_fl3_fl3_to_fl(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI2_SO<float3, float3, float>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  try_dispatch_float_math_fl3_fl_to_fl3(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI2_SO<float3, float, float3>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  try_dispatch_float_math_fl3_to_fl3(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI1_SO<float3, float3>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  try_dispatch_float_math_fl3_to_fl(
      operation, [&](auto exec_preset, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI1_SO<float3, float>(
            info.title_case_name.c_str(), function, exec_preset);
        multi_fn = &fn;
      });
  if (multi_fn != nullptr) {
    return multi_fn;
  }

  return nullptr;
}

static void sh_node_vector_math_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *fn = get_multi_function(builder.node());
  builder.set_matching_fn(fn);
}

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  const NodeVectorMathOperation op = NodeVectorMathOperation(params.node.custom1);
  switch (op) {
    case NODE_VECTOR_MATH_ADD:
    case NODE_VECTOR_MATH_SUBTRACT:
    case NODE_VECTOR_MATH_MULTIPLY:
    case NODE_VECTOR_MATH_DIVIDE: {
      VectorElem output_elem;
      output_elem.merge(params.get_input_elem<VectorElem>("Vector"));
      output_elem.merge(params.get_input_elem<VectorElem>("Vector_001"));
      params.set_output_elem("Vector", output_elem);
      break;
    }
    case NODE_VECTOR_MATH_SCALE: {
      VectorElem output_elem;
      output_elem.merge(params.get_input_elem<VectorElem>("Vector"));
      if (params.get_input_elem<FloatElem>("Scale")) {
        output_elem = VectorElem::all();
      }
      params.set_output_elem("Vector", output_elem);
    }
    default:
      break;
  }
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  const NodeVectorMathOperation op = NodeVectorMathOperation(params.node.custom1);
  switch (op) {
    case NODE_VECTOR_MATH_ADD:
    case NODE_VECTOR_MATH_SUBTRACT:
    case NODE_VECTOR_MATH_MULTIPLY:
    case NODE_VECTOR_MATH_DIVIDE:
    case NODE_VECTOR_MATH_SCALE: {
      params.set_input_elem("Vector", params.get_output_elem<value_elem::VectorElem>("Vector"));
      break;
    }
    default:
      break;
  }
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const NodeVectorMathOperation op = NodeVectorMathOperation(params.node.custom1);
  const StringRef first_input_id = "Vector";
  const StringRef second_input_id = "Vector_001";
  const StringRef scale_input_id = "Scale";
  const StringRef output_vector_id = "Vector";
  switch (op) {
    case NODE_VECTOR_MATH_ADD: {
      params.set_input(first_input_id,
                       params.get_output<float3>(output_vector_id) -
                           params.get_input<float3>(second_input_id));
      break;
    }
    case NODE_VECTOR_MATH_SUBTRACT: {
      params.set_input(first_input_id,
                       params.get_output<float3>(output_vector_id) +
                           params.get_input<float3>(second_input_id));
      break;
    }
    case NODE_VECTOR_MATH_MULTIPLY: {
      params.set_input(first_input_id,
                       math::safe_divide(params.get_output<float3>(output_vector_id),
                                         params.get_input<float3>(second_input_id)));
      break;
    }
    case NODE_VECTOR_MATH_DIVIDE: {
      params.set_input(first_input_id,
                       params.get_output<float3>(output_vector_id) *
                           params.get_input<float3>(second_input_id));
      break;
    }
    case NODE_VECTOR_MATH_SCALE: {
      params.set_input(first_input_id,
                       math::safe_divide(params.get_output<float3>(output_vector_id),
                                         float3(params.get_input<float>(scale_input_id))));
      break;
    }
    default: {
      break;
    }
  }
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  auto op = node_->custom1;
  NodeItem res = empty();
  const NodeItem null_vec = val(MaterialX::Vector3(0.0f));

  /* Single operand operations */
  NodeItem x = get_input_value(0, NodeItem::Type::Vector3);

  switch (op) {
    case NODE_VECTOR_MATH_SINE:
      res = x.sin();
      break;
    case NODE_VECTOR_MATH_COSINE:
      res = x.cos();
      break;
    case NODE_VECTOR_MATH_TANGENT:
      res = x.tan();
      break;
    case NODE_VECTOR_MATH_ABSOLUTE:
      res = x.abs();
      break;
    case NODE_VECTOR_MATH_FLOOR:
      res = x.floor();
      break;
    case NODE_VECTOR_MATH_CEIL:
      res = x.ceil();
      break;
    case NODE_VECTOR_MATH_FRACTION:
      res = x % val(1.0f);
      break;
    case NODE_VECTOR_MATH_LENGTH:
      res = x.length();
      break;
    case NODE_VECTOR_MATH_NORMALIZE: {
      NodeItem length = x.length();
      res = length.if_else(NodeItem::CompareOp::Eq, val(0.0f), null_vec, x / length);
      break;
    }

    default: {
      /* 2-operand operations */
      NodeItem y = get_input_value(1, NodeItem::Type::Vector3);
      NodeItem w = get_input_value(3, NodeItem::Type::Float);

      switch (op) {
        case NODE_VECTOR_MATH_ADD:
          res = x + y;
          break;
        case NODE_VECTOR_MATH_SUBTRACT:
          res = x - y;
          break;
        case NODE_VECTOR_MATH_MULTIPLY:
          res = x * y;
          break;
        case NODE_VECTOR_MATH_DIVIDE:
          res = x / y;
          break;
        case NODE_VECTOR_MATH_MINIMUM:
          res = x.min(y);
          break;
        case NODE_VECTOR_MATH_MAXIMUM:
          res = x.max(y);
          break;
        case NODE_VECTOR_MATH_MODULO:
          res = x % y;
          break;
        case NODE_VECTOR_MATH_SNAP:
          res = (x / y).floor() * y;
          break;
        case NODE_VECTOR_MATH_CROSS_PRODUCT:
          res = create_node("crossproduct", NodeItem::Type::Vector3, {{"in1", x}, {"in2", y}});
          break;
        case NODE_VECTOR_MATH_DOT_PRODUCT:
          res = x.dotproduct(y);
          break;
        case NODE_VECTOR_MATH_PROJECT: {
          NodeItem len_sq = y.dotproduct(y);
          res = len_sq.if_else(
              NodeItem::CompareOp::NotEq, val(0.0f), (x.dotproduct(y) / len_sq) * y, null_vec);
          break;
        }
        case NODE_VECTOR_MATH_REFLECT:
          /* TODO: use <reflect> node in MaterialX 1.38.9 */
          res = x - val(2.0f) * y.dotproduct(x) * y;
          break;
        case NODE_VECTOR_MATH_DISTANCE:
          res = (y - x).length();
          break;
        case NODE_VECTOR_MATH_SCALE:
          res = x * w;
          break;

        default: {
          /* 3-operand operations */
          NodeItem z = get_input_value(2, NodeItem::Type::Vector3);

          switch (op) {
            case NODE_VECTOR_MATH_MULTIPLY_ADD:
              res = x * y + z;
              break;
            case NODE_VECTOR_MATH_REFRACT: {
              /* TODO: use <refract> node in MaterialX 1.38.9 */
              NodeItem dot_yx = y.dotproduct(x);
              NodeItem k = val(1.0f) - (w * w * (val(1.0f) - (dot_yx * dot_yx)));
              NodeItem r = w * x - ((w * dot_yx + k.sqrt()) * y);
              res = k.if_else(NodeItem::CompareOp::GreaterEq, val(0.0f), r, null_vec);
              break;
            }
            case NODE_VECTOR_MATH_FACEFORWARD: {
              res = z.dotproduct(y).if_else(NodeItem::CompareOp::GreaterEq, val(0.0f), -x, x);
              break;
            }
            case NODE_VECTOR_MATH_WRAP: {
              NodeItem range = (y - z);
              NodeItem if_branch = x - (range * ((x - z) / range).floor());

              res = create_node("combine3", NodeItem::Type::Vector3);
              std::vector<std::string> inputs = {"in1", "in2", "in3"};

              for (size_t i = 0; i < inputs.size(); ++i) {
                res.set_input(
                    inputs[i],
                    range[i].if_else(NodeItem::CompareOp::NotEq, val(0.0f), if_branch[i], z[i]));
              }
              break;
            }
            default:
              BLI_assert_unreachable();
          }
        }
      }
    }
  }

  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_vector_math_cc

void register_node_type_sh_vect_math()
{
  namespace file_ns = blender::nodes::node_shader_vector_math_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeVectorMath", SH_NODE_VECTOR_MATH);
  ntype.ui_name = "Vector Math";
  ntype.ui_description = "Perform vector math operation";
  ntype.enum_name_legacy = "VECT_MATH";
  ntype.nclass = NODE_CLASS_OP_VECTOR;
  ntype.declare = file_ns::sh_node_vector_math_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_vect_math;
  ntype.labelfunc = node_vector_math_label;
  ntype.gpu_fn = file_ns::gpu_shader_vector_math;
  ntype.updatefunc = file_ns::node_shader_update_vector_math;
  ntype.build_multi_function = file_ns::sh_node_vector_math_build_multi_function;
  ntype.gather_link_search_ops = file_ns::sh_node_vector_math_gather_link_searches;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  ntype.eval_elem = file_ns::node_eval_elem;
  ntype.eval_inverse_elem = file_ns::node_eval_inverse_elem;
  ntype.eval_inverse = file_ns::node_eval_inverse;

  blender::bke::node_register_type(ntype);
}
