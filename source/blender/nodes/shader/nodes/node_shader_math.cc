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
#include "NOD_socket_search_link.hh"
#include "NOD_value_elem_eval.hh"

#include "RNA_enum_types.hh"

/* **************** SCALAR MATH ******************** */

namespace blender::nodes::node_shader_math_cc {

static void sh_node_math_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Value").default_value(0.5f).min(-10000.0f).max(10000.0f).label_fn(
      [](bNode node) {
        switch (node.custom1) {
          case NODE_MATH_POWER:
            return IFACE_("Base");
          case NODE_MATH_DEGREES:
            return IFACE_("Radians");
          case NODE_MATH_RADIANS:
            return IFACE_("Degrees");
          default:
            return IFACE_("Value");
        }
      });
  b.add_input<decl::Float>("Value", "Value_001")
      .default_value(0.5f)
      .min(-10000.0f)
      .max(10000.0f)
      .label_fn([](bNode node) {
        switch (node.custom1) {
          case NODE_MATH_WRAP:
            return IFACE_("Max");
          case NODE_MATH_MULTIPLY_ADD:
            return IFACE_("Multiplier");
          case NODE_MATH_LESS_THAN:
          case NODE_MATH_GREATER_THAN:
            return IFACE_("Threshold");
          case NODE_MATH_PINGPONG:
            return IFACE_("Scale");
          case NODE_MATH_SNAP:
            return IFACE_("Increment");
          case NODE_MATH_POWER:
            return IFACE_("Exponent");
          case NODE_MATH_LOGARITHM:
            return IFACE_("Base");
          default:
            return IFACE_("Value");
        }
      });
  b.add_input<decl::Float>("Value", "Value_002")
      .default_value(0.5f)
      .min(-10000.0f)
      .max(10000.0f)
      .label_fn([](bNode node) {
        switch (node.custom1) {
          case NODE_MATH_WRAP:
            return IFACE_("Min");
          case NODE_MATH_MULTIPLY_ADD:
            return IFACE_("Addend");
          case NODE_MATH_COMPARE:
            return IFACE_("Epsilon");
          case NODE_MATH_SMOOTH_MAX:
          case NODE_MATH_SMOOTH_MIN:
            return IFACE_("Distance");
          default:
            return IFACE_("Value");
        }
      });
  b.add_output<decl::Float>("Value");
}

class SocketSearchOp {
 public:
  std::string socket_name;
  NodeMathOperation mode = NODE_MATH_ADD;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("ShaderNodeMath");
    node.custom1 = mode;
    params.update_and_connect_available_socket(node, socket_name);
  }
};

static void sh_node_math_gather_link_searches(GatherLinkSearchOpParams &params)
{
  if (!params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                  SOCK_FLOAT))
  {
    return;
  }

  const bool is_geometry_node_tree = params.node_tree().type == NTREE_GEOMETRY;
  const int weight = ELEM(params.other_socket().type, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN) ? 0 : -1;

  for (const EnumPropertyItem *item = rna_enum_node_math_items; item->identifier != nullptr;
       item++)
  {
    if (item->name != nullptr && item->identifier[0] != '\0') {
      const int gn_weight =
          (is_geometry_node_tree &&
           ELEM(item->value, NODE_MATH_COMPARE, NODE_MATH_GREATER_THAN, NODE_MATH_LESS_THAN)) ?
              -1 :
              weight;
      params.add_item(CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, item->name),
                      SocketSearchOp{"Value", (NodeMathOperation)item->value},
                      gn_weight);
    }
  }
}

static const char *gpu_shader_get_name(int mode)
{
  const FloatMathOperationInfo *info = get_float_math_operation_info(mode);
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
                           bNodeExecData * /*execdata*/,
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

static void node_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  const NodeMathOperation op = NodeMathOperation(params.node.custom1);
  switch (op) {
    case NODE_MATH_ADD:
    case NODE_MATH_SUBTRACT:
    case NODE_MATH_MULTIPLY:
    case NODE_MATH_DIVIDE: {
      FloatElem output_elem = params.get_input_elem<FloatElem>("Value");
      output_elem.merge(params.get_input_elem<FloatElem>("Value_001"));
      params.set_output_elem("Value", output_elem);
      break;
    }
    default:
      break;
  }
}

static void node_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  const NodeMathOperation op = NodeMathOperation(params.node.custom1);
  switch (op) {
    case NODE_MATH_ADD:
    case NODE_MATH_SUBTRACT:
    case NODE_MATH_MULTIPLY:
    case NODE_MATH_DIVIDE: {
      params.set_input_elem("Value", params.get_output_elem<value_elem::FloatElem>("Value"));
      break;
    }
    default:
      break;
  }
}

static void node_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const NodeMathOperation op = NodeMathOperation(params.node.custom1);
  const StringRef first_input_id = "Value";
  const StringRef second_input_id = "Value_001";
  const StringRef output_id = "Value";
  switch (op) {
    case NODE_MATH_ADD: {
      params.set_input(first_input_id,
                       params.get_output<float>(output_id) -
                           params.get_input<float>(second_input_id));
      break;
    }
    case NODE_MATH_SUBTRACT: {
      params.set_input(first_input_id,
                       params.get_output<float>(output_id) +
                           params.get_input<float>(second_input_id));
      break;
    }
    case NODE_MATH_MULTIPLY: {
      params.set_input(first_input_id,
                       math::safe_divide(params.get_output<float>(output_id),
                                         params.get_input<float>(second_input_id)));
      break;
    }
    case NODE_MATH_DIVIDE: {
      params.set_input(first_input_id,
                       params.get_output<float>(output_id) *
                           params.get_input<float>(second_input_id));
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
  NodeMathOperation op = NodeMathOperation(node_->custom1);
  NodeItem res = empty();

  /* Single operand operations */
  NodeItem x = get_input_value(0, NodeItem::Type::Float);

  switch (op) {
    case NODE_MATH_SINE:
      res = x.sin();
      break;
    case NODE_MATH_COSINE:
      res = x.cos();
      break;
    case NODE_MATH_TANGENT:
      res = x.tan();
      break;
    case NODE_MATH_ARCSINE:
      res = x.asin();
      break;
    case NODE_MATH_ARCCOSINE:
      res = x.acos();
      break;
    case NODE_MATH_ARCTANGENT:
      res = x.atan();
      break;
    case NODE_MATH_ROUND:
      res = (x + val(0.5f)).floor();
      break;
    case NODE_MATH_ABSOLUTE:
      res = x.abs();
      break;
    case NODE_MATH_FLOOR:
      res = x.floor();
      break;
    case NODE_MATH_CEIL:
      res = x.ceil();
      break;
    case NODE_MATH_FRACTION:
      res = x % val(1.0f);
      break;
    case NODE_MATH_SQRT:
      res = x.sqrt();
      break;
    case NODE_MATH_INV_SQRT:
      res = val(1.0f) / x.sqrt();
      break;
    case NODE_MATH_SIGN:
      res = x.sign();
      break;
    case NODE_MATH_EXPONENT:
      res = x.exp();
      break;
    case NODE_MATH_RADIANS:
      res = x * val(float(M_PI) / 180.0f);
      break;
    case NODE_MATH_DEGREES:
      res = x * val(180.0f * float(M_1_PI));
      break;
    case NODE_MATH_SINH:
      res = x.sinh();
      break;
    case NODE_MATH_COSH:
      res = x.cosh();
      break;
    case NODE_MATH_TANH:
      res = x.tanh();
      break;
    case NODE_MATH_TRUNC:
      res = x.sign() * x.abs().floor();
      break;

    default: {
      /* 2-operand operations */
      NodeItem y = get_input_value(1, NodeItem::Type::Float);

      switch (op) {
        case NODE_MATH_ADD:
          res = x + y;
          break;
        case NODE_MATH_SUBTRACT:
          res = x - y;
          break;
        case NODE_MATH_MULTIPLY:
          res = x * y;
          break;
        case NODE_MATH_DIVIDE:
          res = x / y;
          break;
        case NODE_MATH_POWER:
          res = x ^ y;
          break;
        case NODE_MATH_LOGARITHM:
          res = x.ln() / y.ln();
          break;
        case NODE_MATH_MINIMUM:
          res = x.min(y);
          break;
        case NODE_MATH_MAXIMUM:
          res = x.max(y);
          break;
        case NODE_MATH_LESS_THAN:
          res = x.if_else(NodeItem::CompareOp::Less, y, val(1.0f), val(0.0f));
          break;
        case NODE_MATH_GREATER_THAN:
          res = x.if_else(NodeItem::CompareOp::Greater, y, val(1.0f), val(0.0f));
          break;
        case NODE_MATH_MODULO:
          res = x % y;
          break;
        case NODE_MATH_ARCTAN2:
          res = x.atan2(y);
          break;
        case NODE_MATH_SNAP:
          res = (x / y).floor() * y;
          break;
        case NODE_MATH_PINGPONG: {
          NodeItem fract_part = (x - y) / (y * val(2.0f));
          NodeItem if_branch = ((fract_part - fract_part.floor()) * y * val(2.0f) - y).abs();
          res = y.if_else(NodeItem::CompareOp::NotEq, val(0.0f), if_branch, val(0.0f));
          break;
        }
        case NODE_MATH_FLOORED_MODULO:
          res = y.if_else(
              NodeItem::CompareOp::NotEq, val(0.0f), (x - (x / y).floor() * y), val(0.0f));
          break;

        default: {
          /* 3-operand operations */
          NodeItem z = get_input_value(2, NodeItem::Type::Float);

          switch (op) {
            case NODE_MATH_WRAP: {
              NodeItem range = (y - z);
              NodeItem if_branch = x - (range * ((x - z) / range).floor());
              res = range.if_else(NodeItem::CompareOp::NotEq, val(0.0f), if_branch, z);
              break;
            }
            case NODE_MATH_COMPARE:
              res = z.if_else(NodeItem::CompareOp::Less, (x - y).abs(), val(1.0f), val(0.0f));
              break;
            case NODE_MATH_MULTIPLY_ADD:
              res = x * y + z;
              break;
            case NODE_MATH_SMOOTH_MIN:
            case NODE_MATH_SMOOTH_MAX: {
              auto make_smoothmin = [&](NodeItem a, NodeItem b, NodeItem k) {
                NodeItem h = (k - (a - b).abs()).max(val(0.0f)) / k;
                NodeItem if_branch = a.min(b) - h * h * h * k * val(1.0f / 6.0f);
                return k.if_else(NodeItem::CompareOp::NotEq, val(0.0f), if_branch, a.min(b));
              };
              if (op == NODE_MATH_SMOOTH_MIN) {
                res = make_smoothmin(x, y, z);
              }
              else {
                res = -make_smoothmin(-x, -y, -z);
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

  bool clamp_output = node_->custom2 != 0;
  if (clamp_output && res) {
    res = res.clamp();
  }

  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_math_cc

void register_node_type_sh_math()
{
  namespace file_ns = blender::nodes::node_shader_math_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeMath", SH_NODE_MATH);
  ntype.ui_name = "Math";
  ntype.ui_description = "Perform math operations";
  ntype.enum_name_legacy = "MATH";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_math_declare;
  ntype.labelfunc = node_math_label;
  ntype.gpu_fn = file_ns::gpu_shader_math;
  ntype.updatefunc = node_math_update;
  ntype.build_multi_function = blender::nodes::node_math_build_multi_function;
  ntype.gather_link_search_ops = file_ns::sh_node_math_gather_link_searches;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  ntype.eval_elem = file_ns::node_eval_elem;
  ntype.eval_inverse_elem = file_ns::node_eval_inverse_elem;
  ntype.eval_inverse = file_ns::node_eval_inverse;

  blender::bke::node_register_type(ntype);
}
