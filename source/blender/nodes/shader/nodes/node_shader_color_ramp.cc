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

#include "DNA_texture_types.h"

#include "BLI_color.hh"

#include "node_shader_util.hh"

namespace blender::nodes::node_shader_color_ramp_cc {

static void sh_node_valtorgb_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Float>(N_("Fac")).default_value(0.5f).min(0.0f).max(1.0f).subtype(PROP_FACTOR);
  b.add_output<decl::Color>(N_("Color"));
  b.add_output<decl::Float>(N_("Alpha"));
}

static void node_shader_init_valtorgb(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

static int gpu_shader_valtorgb(GPUMaterial *mat,
                               bNode *node,
                               bNodeExecData *UNUSED(execdata),
                               GPUNodeStack *in,
                               GPUNodeStack *out)
{
  struct ColorBand *coba = (ColorBand *)node->storage;
  float *array, layer;
  int size;

  /* Common / easy case optimization. */
  if ((coba->tot <= 2) && (coba->color_mode == COLBAND_BLEND_RGB)) {
    float mul_bias[2];
    switch (coba->ipotype) {
      case COLBAND_INTERP_LINEAR:
        mul_bias[0] = 1.0f / (coba->data[1].pos - coba->data[0].pos);
        mul_bias[1] = -mul_bias[0] * coba->data[0].pos;
        return GPU_stack_link(mat,
                              node,
                              "valtorgb_opti_linear",
                              in,
                              out,
                              GPU_uniform(mul_bias),
                              GPU_uniform(&coba->data[0].r),
                              GPU_uniform(&coba->data[1].r));
      case COLBAND_INTERP_CONSTANT:
        mul_bias[1] = max_ff(coba->data[0].pos, coba->data[1].pos);
        return GPU_stack_link(mat,
                              node,
                              "valtorgb_opti_constant",
                              in,
                              out,
                              GPU_uniform(&mul_bias[1]),
                              GPU_uniform(&coba->data[0].r),
                              GPU_uniform(&coba->data[1].r));
      case COLBAND_INTERP_EASE:
        mul_bias[0] = 1.0f / (coba->data[1].pos - coba->data[0].pos);
        mul_bias[1] = -mul_bias[0] * coba->data[0].pos;
        return GPU_stack_link(mat,
                              node,
                              "valtorgb_opti_ease",
                              in,
                              out,
                              GPU_uniform(mul_bias),
                              GPU_uniform(&coba->data[0].r),
                              GPU_uniform(&coba->data[1].r));
      default:
        break;
    }
  }

  BKE_colorband_evaluate_table_rgba(coba, &array, &size);
  GPUNodeLink *tex = GPU_color_band(mat, size, array, &layer);

  if (coba->ipotype == COLBAND_INTERP_CONSTANT) {
    return GPU_stack_link(mat, node, "valtorgb_nearest", in, out, tex, GPU_constant(&layer));
  }

  return GPU_stack_link(mat, node, "valtorgb", in, out, tex, GPU_constant(&layer));
}

class ColorBandFunction : public blender::fn::MultiFunction {
 private:
  const ColorBand &color_band_;

 public:
  ColorBandFunction(const ColorBand &color_band) : color_band_(color_band)
  {
    static blender::fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static blender::fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Color Band"};
    signature.single_input<float>("Value");
    signature.single_output<blender::ColorGeometry4f>("Color");
    signature.single_output<float>("Alpha");
    return signature.build();
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext UNUSED(context)) const override
  {
    const blender::VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    blender::MutableSpan<blender::ColorGeometry4f> colors =
        params.uninitialized_single_output<blender::ColorGeometry4f>(1, "Color");
    blender::MutableSpan<float> alphas = params.uninitialized_single_output<float>(2, "Alpha");

    for (int64_t i : mask) {
      blender::ColorGeometry4f color;
      BKE_colorband_evaluate(&color_band_, values[i], color);
      colors[i] = color;
      alphas[i] = color.a;
    }
  }
};

static void sh_node_valtorgb_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &bnode = builder.node();
  const ColorBand *color_band = (const ColorBand *)bnode.storage;
  builder.construct_and_set_matching_fn<ColorBandFunction>(*color_band);
}

}  // namespace blender::nodes::node_shader_color_ramp_cc

void register_node_type_sh_valtorgb()
{
  namespace file_ns = blender::nodes::node_shader_color_ramp_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_VALTORGB, "ColorRamp", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_valtorgb_declare;
  node_type_init(&ntype, file_ns::node_shader_init_valtorgb);
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::gpu_shader_valtorgb);
  ntype.build_multi_function = file_ns::sh_node_valtorgb_build_multi_function;

  nodeRegisterType(&ntype);
}
