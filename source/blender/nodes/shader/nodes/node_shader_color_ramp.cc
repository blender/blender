/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2005 Blender Foundation */

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

static void node_shader_init_valtorgb(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_colorband_add(true);
}

static int gpu_shader_valtorgb(GPUMaterial *mat,
                               bNode *node,
                               bNodeExecData * /*execdata*/,
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

class ColorBandFunction : public mf::MultiFunction {
 private:
  const ColorBand &color_band_;

 public:
  ColorBandFunction(const ColorBand &color_band) : color_band_(color_band)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Color Band", signature};
      builder.single_input<float>("Value");
      builder.single_output<ColorGeometry4f>("Color");
      builder.single_output<float>("Alpha");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float> &values = params.readonly_single_input<float>(0, "Value");
    MutableSpan<ColorGeometry4f> colors = params.uninitialized_single_output<ColorGeometry4f>(
        1, "Color");
    MutableSpan<float> alphas = params.uninitialized_single_output<float>(2, "Alpha");

    for (int64_t i : mask) {
      ColorGeometry4f color;
      BKE_colorband_evaluate(&color_band_, values[i], color);
      colors[i] = color;
      alphas[i] = color.a;
    }
  }
};

static void sh_node_valtorgb_build_multi_function(nodes::NodeMultiFunctionBuilder &builder)
{
  const bNode &bnode = builder.node();
  const ColorBand *color_band = (const ColorBand *)bnode.storage;
  builder.construct_and_set_matching_fn<ColorBandFunction>(*color_band);
}

}  // namespace blender::nodes::node_shader_color_ramp_cc

void register_node_type_sh_valtorgb()
{
  namespace file_ns = blender::nodes::node_shader_color_ramp_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_VALTORGB, "Color Ramp", NODE_CLASS_CONVERTER);
  ntype.declare = file_ns::sh_node_valtorgb_declare;
  ntype.initfunc = file_ns::node_shader_init_valtorgb;
  node_type_size_preset(&ntype, NODE_SIZE_LARGE);
  node_type_storage(&ntype, "ColorBand", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_valtorgb;
  ntype.build_multi_function = file_ns::sh_node_valtorgb_build_multi_function;

  nodeRegisterType(&ntype);
}
