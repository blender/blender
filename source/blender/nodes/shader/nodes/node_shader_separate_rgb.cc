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
 * The Original Code is Copyright (C) 2021 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_separate_rgb_cc {

static void sh_node_seprgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Color>(N_("Image")).default_value({0.8f, 0.8f, 0.8f, 1.0f});
  b.add_output<decl::Float>(N_("R"));
  b.add_output<decl::Float>(N_("G"));
  b.add_output<decl::Float>(N_("B"));
};

static void node_shader_exec_seprgb(void *UNUSED(data),
                                    int UNUSED(thread),
                                    bNode *UNUSED(node),
                                    bNodeExecData *UNUSED(execdata),
                                    bNodeStack **in,
                                    bNodeStack **out)
{
  float col[3];
  nodestack_get_vec(col, SOCK_VECTOR, in[0]);

  out[0]->vec[0] = col[0];
  out[1]->vec[0] = col[1];
  out[2]->vec[0] = col[2];
}

static int gpu_shader_seprgb(GPUMaterial *mat,
                             bNode *node,
                             bNodeExecData *UNUSED(execdata),
                             GPUNodeStack *in,
                             GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "separate_rgb", in, out);
}

class SeparateRGBFunction : public blender::fn::MultiFunction {
 public:
  SeparateRGBFunction()
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Separate RGB"};
    signature.single_input<blender::ColorGeometry4f>("Color");
    signature.single_output<float>("R");
    signature.single_output<float>("G");
    signature.single_output<float>("B");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<blender::ColorGeometry4f> &colors =
        params.readonly_single_input<blender::ColorGeometry4f>(0, "Color");
    blender::MutableSpan<float> rs = params.uninitialized_single_output<float>(1, "R");
    blender::MutableSpan<float> gs = params.uninitialized_single_output<float>(2, "G");
    blender::MutableSpan<float> bs = params.uninitialized_single_output<float>(3, "B");

    for (int64_t i : mask) {
      blender::ColorGeometry4f color = colors[i];
      rs[i] = color.r;
      gs[i] = color.g;
      bs[i] = color.b;
    }
  }
};

static void sh_node_seprgb_build_multi_function(blender::nodes::NodeMultiFunctionBuilder &builder)
{
  static SeparateRGBFunction fn;
  builder.set_matching_fn(fn);
}

}  // namespace blender::nodes::node_shader_separate_rgb_cc

void register_node_type_sh_seprgb()
{
  namespace file_ns = blender::nodes::node_shader_separate_rgb_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_SEPRGB, "Separate RGB", NODE_CLASS_CONVERTER, 0);
  ntype.declare = file_ns::sh_node_seprgb_declare;
  node_type_exec(&ntype, nullptr, nullptr, file_ns::node_shader_exec_seprgb);
  node_type_gpu(&ntype, file_ns::gpu_shader_seprgb);
  ntype.build_multi_function = file_ns::sh_node_seprgb_build_multi_function;

  nodeRegisterType(&ntype);
}
