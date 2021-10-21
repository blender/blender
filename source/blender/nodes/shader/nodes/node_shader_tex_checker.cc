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

#include "../node_shader_util.h"

namespace blender::nodes {

static void sh_node_tex_checker_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").min(-10000.0f).max(10000.0f).implicit_field();
  b.add_input<decl::Color>("Color1").default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_input<decl::Color>("Color2").default_value({0.2f, 0.2f, 0.2f, 1.0f});
  b.add_input<decl::Float>("Scale")
      .min(-10000.0f)
      .max(10000.0f)
      .default_value(5.0f)
      .no_muted_links();
  b.add_output<decl::Color>("Color");
  b.add_output<decl::Float>("Fac");
};

}  // namespace blender::nodes

static void node_shader_init_tex_checker(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexChecker *tex = (NodeTexChecker *)MEM_callocN(sizeof(NodeTexChecker), "NodeTexChecker");
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);

  node->storage = tex;
}

static int node_shader_gpu_tex_checker(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData *UNUSED(execdata),
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_checker", in, out);
}

namespace blender::nodes {

class NodeTexChecker : public fn::MultiFunction {
 public:
  NodeTexChecker()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Checker"};
    signature.single_input<float3>("Vector");
    signature.single_input<ColorGeometry4f>("Color1");
    signature.single_input<ColorGeometry4f>("Color2");
    signature.single_input<float>("Scale");
    signature.single_output<ColorGeometry4f>("Color");
    signature.single_output<float>("Fac");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            fn::MFParams params,
            fn::MFContext UNUSED(context)) const override
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

    for (int64_t i : mask) {
      /* Avoid precision issues on unit coordinates. */
      const float3 p = (vector[i] * scale[i] + 0.000001f) * 0.999999f;

      const int xi = abs((int)(floorf(p.x)));
      const int yi = abs((int)(floorf(p.y)));
      const int zi = abs((int)(floorf(p.z)));

      r_fac[i] = ((xi % 2 == yi % 2) == (zi % 2)) ? 1.0f : 0.0f;
    }

    if (!r_color.is_empty()) {
      for (int64_t i : mask) {
        r_color[i] = (r_fac[i] == 1.0f) ? color1[i] : color2[i];
      }
    }
  }
};

static void sh_node_tex_checker_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static NodeTexChecker fn;
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes

void register_node_type_sh_tex_checker(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_CHECKER, "Checker Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = blender::nodes::sh_node_tex_checker_declare;
  node_type_init(&ntype, node_shader_init_tex_checker);
  node_type_storage(
      &ntype, "NodeTexChecker", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, node_shader_gpu_tex_checker);
  ntype.build_multi_function = blender::nodes::sh_node_tex_checker_build_multi_function;

  nodeRegisterType(&ntype);
}
