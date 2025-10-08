/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

#include "FN_multi_function_builder.hh"

#include "NOD_inverse_eval_params.hh"
#include "NOD_multi_function.hh"
#include "NOD_value_elem_eval.hh"

namespace blender::nodes::node_shader_sepcomb_xyz_cc::sep {

static void sh_node_sepxyz_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Float>("X");
  b.add_output<decl::Float>("Y");
  b.add_output<decl::Float>("Z");
}

static int gpu_shader_sepxyz(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData * /*execdata*/,
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_xyz", in, out);
}

class MF_SeparateXYZ : public mf::MultiFunction {
 public:
  MF_SeparateXYZ()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Separate XYZ", signature};
      builder.single_input<float3>("XYZ");
      builder.single_output<float>("X", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Y", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Z", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vectors = params.readonly_single_input<float3>(0, "XYZ");
    MutableSpan<float> xs = params.uninitialized_single_output_if_required<float>(1, "X");
    MutableSpan<float> ys = params.uninitialized_single_output_if_required<float>(2, "Y");
    MutableSpan<float> zs = params.uninitialized_single_output_if_required<float>(3, "Z");

    std::array<MutableSpan<float>, 3> outputs = {xs, ys, zs};
    Vector<int> used_outputs;
    if (!xs.is_empty()) {
      used_outputs.append(0);
    }
    if (!ys.is_empty()) {
      used_outputs.append(1);
    }
    if (!zs.is_empty()) {
      used_outputs.append(2);
    }

    devirtualize_varray(vectors, [&](auto vectors) {
      mask.foreach_segment_optimized([&](const auto segment) {
        const int used_outputs_num = used_outputs.size();
        const int *used_outputs_data = used_outputs.data();

        for (const int64_t i : segment) {
          const float3 &vector = vectors[i];
          for (const int out_i : IndexRange(used_outputs_num)) {
            const int coordinate = used_outputs_data[out_i];
            outputs[coordinate][i] = vector[coordinate];
          }
        }
      });
    });
  }
};

static void sh_node_sepxyz_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static MF_SeparateXYZ separate_fn;
  builder.set_matching_fn(separate_fn);
}

static void sh_node_sepxyz_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  const VectorElem vector_elem = params.get_input_elem<VectorElem>("Vector");
  params.set_output_elem("X", vector_elem.x);
  params.set_output_elem("Y", vector_elem.y);
  params.set_output_elem("Z", vector_elem.z);
}

static void sh_node_sepxyz_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  value_elem::VectorElem result;
  result.x = params.get_output_elem<FloatElem>("X");
  result.y = params.get_output_elem<FloatElem>("Y");
  result.z = params.get_output_elem<FloatElem>("Z");
  params.set_input_elem("Vector", result);
}

static void sh_node_sepxyz_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  params.set_input("Vector",
                   float3(params.get_output<float>("X"),
                          params.get_output<float>("Y"),
                          params.get_output<float>("Z")));
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem vector = get_input_value("Vector", NodeItem::Type::Vector3);
  int index = STREQ(socket_out_->identifier, "X") ? 0 :
              STREQ(socket_out_->identifier, "Y") ? 1 :
                                                    2;
  return vector[index];
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_sepcomb_xyz_cc::sep

void register_node_type_sh_sepxyz()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_xyz_cc::sep;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeSeparateXYZ", SH_NODE_SEPXYZ);
  ntype.ui_name = "Separate XYZ";
  ntype.ui_description = "Split a vector into its X, Y, and Z components";
  ntype.enum_name_legacy = "SEPXYZ";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_sepxyz_declare;
  ntype.gpu_fn = file_ns::gpu_shader_sepxyz;
  ntype.build_multi_function = file_ns::sh_node_sepxyz_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  ntype.eval_elem = file_ns::sh_node_sepxyz_eval_elem;
  ntype.eval_inverse_elem = file_ns::sh_node_sepxyz_eval_inverse_elem;
  ntype.eval_inverse = file_ns::sh_node_sepxyz_eval_inverse;

  blender::bke::node_register_type(ntype);
}

namespace blender::nodes::node_shader_sepcomb_xyz_cc::comb {

static void sh_node_combxyz_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>("X").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Y").min(-10000.0f).max(10000.0f);
  b.add_input<decl::Float>("Z").min(-10000.0f).max(10000.0f);
  b.add_output<decl::Vector>("Vector");
}

static int gpu_shader_combxyz(GPUMaterial *mat,
                              bNode *node,
                              bNodeExecData * /*execdata*/,
                              GPUNodeStack *in,
                              GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "combine_xyz", in, out);
}

static void sh_node_combxyz_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static auto fn = mf::build::SI3_SO<float, float, float, float3>(
      "Combine Vector",
      [](float x, float y, float z) { return float3(x, y, z); },
      mf::build::exec_presets::AllSpanOrSingle());
  builder.set_matching_fn(fn);
}

static void sh_node_combxyz_eval_elem(value_elem::ElemEvalParams &params)
{
  using namespace value_elem;
  VectorElem vector_elem;
  vector_elem.x = params.get_input_elem<FloatElem>("X");
  vector_elem.y = params.get_input_elem<FloatElem>("Y");
  vector_elem.z = params.get_input_elem<FloatElem>("Z");
  params.set_output_elem("Vector", vector_elem);
}

static void sh_node_combxyz_eval_inverse_elem(value_elem::InverseElemEvalParams &params)
{
  using namespace value_elem;
  const VectorElem output_elem = params.get_output_elem<VectorElem>("Vector");
  params.set_input_elem("X", output_elem.x);
  params.set_input_elem("Y", output_elem.y);
  params.set_input_elem("Z", output_elem.z);
}

static void sh_node_combxyz_eval_inverse(inverse_eval::InverseEvalParams &params)
{
  const float3 output = params.get_output<float3>("Vector");
  params.set_input("X", output.x);
  params.set_input("Y", output.y);
  params.set_input("Z", output.z);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem x = get_input_value("X", NodeItem::Type::Float);
  NodeItem y = get_input_value("Y", NodeItem::Type::Float);
  NodeItem z = get_input_value("Z", NodeItem::Type::Float);

  return create_node("combine3", NodeItem::Type::Vector3, {{"in1", x}, {"in2", y}, {"in3", z}});
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_sepcomb_xyz_cc::comb

void register_node_type_sh_combxyz()
{
  namespace file_ns = blender::nodes::node_shader_sepcomb_xyz_cc::comb;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeCombineXYZ", SH_NODE_COMBXYZ);
  ntype.ui_name = "Combine XYZ";
  ntype.ui_description = "Create a vector from X, Y, and Z components";
  ntype.enum_name_legacy = "COMBXYZ";
  ntype.nclass = NODE_CLASS_CONVERTER;
  ntype.declare = file_ns::sh_node_combxyz_declare;
  ntype.gpu_fn = file_ns::gpu_shader_combxyz;
  ntype.build_multi_function = file_ns::sh_node_combxyz_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  ntype.eval_elem = file_ns::sh_node_combxyz_eval_elem;
  ntype.eval_inverse_elem = file_ns::sh_node_combxyz_eval_inverse_elem;
  ntype.eval_inverse = file_ns::sh_node_combxyz_eval_inverse;

  blender::bke::node_register_type(ntype);
}
