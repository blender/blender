/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "NOD_multi_function.hh"

namespace blender::nodes::node_shader_tex_checker_cc {

static void sh_node_tex_checker_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field(
      NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Color>("Color1")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .description("Color of the first checker");
  b.add_input<decl::Color>("Color2")
      .default_value({0.2f, 0.2f, 0.2f, 1.0f})
      .description("Color of the second checker");
  b.add_input<decl::Float>("Scale")
      .min(-10000.0f)
      .max(10000.0f)
      .default_value(5.0f)
      .no_muted_links()
      .description(
          "Overall texture scale.\n"
          "The scale is a factor of the bounding box of the face divided by the Scale value");
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Factor", "Fac");
}

static void node_shader_init_tex_checker(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexChecker *tex = MEM_callocN<NodeTexChecker>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);

  node->storage = tex;
}

static int node_shader_gpu_tex_checker(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_checker", in, out);
}

class NodeTexChecker : public mf::MultiFunction {
 public:
  NodeTexChecker()
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Checker", signature};
      builder.single_input<float3>("Vector");
      builder.single_input<ColorGeometry4f>("Color1");
      builder.single_input<ColorGeometry4f>("Color2");
      builder.single_input<float>("Scale");
      builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Fac");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<ColorGeometry4f> &color1 = params.readonly_single_input<ColorGeometry4f>(
        1, "Color1");
    const VArray<ColorGeometry4f> &color2 = params.readonly_single_input<ColorGeometry4f>(
        2, "Color2");
    const VArray<float> &scale = params.readonly_single_input<float>(3, "Scale");
    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(4, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output<float>(5, "Fac");

    mask.foreach_index([&](const int64_t i) {
      /* Avoid precision issues on unit coordinates. */
      const float3 p = (vector[i] * scale[i] + 0.000001f) * 0.999999f;

      const int xi = abs(int(floorf(p.x)));
      const int yi = abs(int(floorf(p.y)));
      const int zi = abs(int(floorf(p.z)));

      r_fac[i] = ((xi % 2 == yi % 2) == (zi % 2)) ? 1.0f : 0.0f;
    });

    if (!r_color.is_empty()) {
      mask.foreach_index(
          [&](const int64_t i) { r_color[i] = (r_fac[i] == 1.0f) ? color1[i] : color2[i]; });
    }
  }
};

static void sh_node_tex_checker_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static NodeTexChecker fn;
  builder.set_matching_fn(fn);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeItem vector = get_input_link("Vector", NodeItem::Type::Vector2);
  if (!vector) {
    vector = texcoord_node();
  }
  NodeItem value1 = val(1.0f);
  NodeItem value2 = val(0.0f);
  if (STREQ(socket_out_->identifier, "Color")) {
    value1 = get_input_value("Color1", NodeItem::Type::Color3);
    value2 = get_input_value("Color2", NodeItem::Type::Color3);
  }
  NodeItem scale = get_input_value("Scale", NodeItem::Type::Float);

  vector = (vector * scale) % val(2.0f);
  return (vector[0].floor() + vector[1].floor())
      .if_else(NodeItem::CompareOp::Eq, val(1.0f), value1, value2);
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_tex_checker_cc

void register_node_type_sh_tex_checker()
{
  namespace file_ns = blender::nodes::node_shader_tex_checker_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexChecker", SH_NODE_TEX_CHECKER);
  ntype.ui_name = "Checker Texture";
  ntype.ui_description = "Generate a checkerboard texture";
  ntype.enum_name_legacy = "TEX_CHECKER";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_checker_declare;
  ntype.initfunc = file_ns::node_shader_init_tex_checker;
  blender::bke::node_type_storage(
      ntype, "NodeTexChecker", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_checker;
  ntype.build_multi_function = file_ns::sh_node_tex_checker_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;

  blender::bke::node_register_type(ntype);
}
