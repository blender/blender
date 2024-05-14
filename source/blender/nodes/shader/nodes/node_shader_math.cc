/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "NOD_math_functions.hh"
#include "NOD_multi_function.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

/* **************** SCALAR MATH ******************** */

namespace blender::nodes::node_shader_math_cc {

static void sh_node_math_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("Value").default_value(0.5f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Value", "Value_001").default_value(0.5f).min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Value", "Value_002").default_value(0.5f).min(-10000.0f).max(10000.0f);
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
  if (!params.node_tree().typeinfo->validate_link(
          static_cast<eNodeSocketDatatype>(params.other_socket().type), SOCK_FLOAT))
  {
    return;
  }

  const bool is_geometry_node_tree = params.node_tree().type == NTREE_GEOMETRY;
  const int weight = ELEM(params.other_socket().type, SOCK_FLOAT, SOCK_BOOLEAN, SOCK_INT) ? 0 : -1;

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

static const mf::MultiFunction *get_base_multi_function(const bNode &node)
{
  const int mode = node.custom1;
  const mf::MultiFunction *base_fn = nullptr;

  try_dispatch_float_math_fl_to_fl(
      mode, [&](auto devi_fn, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI1_SO<float, float>(
            info.title_case_name.c_str(), function, devi_fn);
        base_fn = &fn;
      });
  if (base_fn != nullptr) {
    return base_fn;
  }

  try_dispatch_float_math_fl_fl_to_fl(
      mode, [&](auto devi_fn, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI2_SO<float, float, float>(
            info.title_case_name.c_str(), function, devi_fn);
        base_fn = &fn;
      });
  if (base_fn != nullptr) {
    return base_fn;
  }

  try_dispatch_float_math_fl_fl_fl_to_fl(
      mode, [&](auto devi_fn, auto function, const FloatMathOperationInfo &info) {
        static auto fn = mf::build::SI3_SO<float, float, float, float>(
            info.title_case_name.c_str(), function, devi_fn);
        base_fn = &fn;
      });
  if (base_fn != nullptr) {
    return base_fn;
  }

  return nullptr;
}

class ClampWrapperFunction : public mf::MultiFunction {
 private:
  const mf::MultiFunction &fn_;

 public:
  ClampWrapperFunction(const mf::MultiFunction &fn) : fn_(fn)
  {
    this->set_signature(&fn.signature());
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context context) const override
  {
    fn_.call(mask, params, context);

    /* Assumes the output parameter is the last one. */
    const int output_param_index = this->param_amount() - 1;
    /* This has actually been initialized in the call above. */
    MutableSpan<float> results = params.uninitialized_single_output<float>(output_param_index);

    mask.foreach_index_optimized<int>([&](const int i) {
      float &value = results[i];
      CLAMP(value, 0.0f, 1.0f);
    });
  }
};

static void sh_node_math_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const mf::MultiFunction *base_function = get_base_multi_function(builder.node());

  const bool clamp_output = builder.node().custom2 != 0;
  if (clamp_output) {
    builder.construct_and_set_matching_fn<ClampWrapperFunction>(*base_function);
  }
  else {
    builder.set_matching_fn(base_function);
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

  sh_fn_node_type_base(&ntype, SH_NODE_MATH, "Math", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_math_declare;
  ntype.labelfunc = node_math_label;
  ntype.gpu_fn = file_ns::gpu_shader_math;
  ntype.updatefunc = node_math_update;
  ntype.build_multi_function = file_ns::sh_node_math_build_multi_function;
  ntype.gather_link_search_ops = file_ns::sh_node_math_gather_link_searches;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::nodeRegisterType(&ntype);
}
