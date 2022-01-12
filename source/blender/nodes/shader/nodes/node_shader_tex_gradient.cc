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
 * The Original Code is Copyright (C) 2005 Gradienter Foundation.
 * All rights reserved.
 */

#include "node_shader_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_tex_gradient_cc {

static void sh_node_tex_gradient_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector")).hide_value().implicit_field();
  b.add_output<decl::Color>(N_("Color")).no_muted_links();
  b.add_output<decl::Float>(N_("Fac")).no_muted_links();
}

static void node_shader_buts_tex_gradient(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "gradient_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_gradient(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexGradient *tex = MEM_cnew<NodeTexGradient>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->gradient_type = SHD_BLEND_LINEAR;

  node->storage = tex;
}

static int node_shader_gpu_tex_gradient(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexGradient *tex = (NodeTexGradient *)node->storage;
  float gradient_type = tex->gradient_type;
  return GPU_stack_link(mat, node, "node_tex_gradient", in, out, GPU_constant(&gradient_type));
}

class GradientFunction : public fn::MultiFunction {
 private:
  int gradient_type_;

 public:
  GradientFunction(int gradient_type) : gradient_type_(gradient_type)
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"GradientFunction"};
    signature.single_input<float3>("Vector");
    signature.single_output<ColorGeometry4f>("Color");
    signature.single_output<float>("Fac");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");

    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(1, "Color");
    MutableSpan<float> fac = params.uninitialized_single_output<float>(2, "Fac");

    const bool compute_color = !r_color.is_empty();

    switch (gradient_type_) {
      case SHD_BLEND_LINEAR: {
        for (int64_t i : mask) {
          fac[i] = vector[i].x;
        }
        break;
      }
      case SHD_BLEND_QUADRATIC: {
        for (int64_t i : mask) {
          const float r = std::max(vector[i].x, 0.0f);
          fac[i] = r * r;
        }
        break;
      }
      case SHD_BLEND_EASING: {
        for (int64_t i : mask) {
          const float r = std::min(std::max(vector[i].x, 0.0f), 1.0f);
          const float t = r * r;
          fac[i] = (3.0f * t - 2.0f * t * r);
        }
        break;
      }
      case SHD_BLEND_DIAGONAL: {
        for (int64_t i : mask) {
          fac[i] = (vector[i].x + vector[i].y) * 0.5f;
        }
        break;
      }
      case SHD_BLEND_RADIAL: {
        for (int64_t i : mask) {
          fac[i] = atan2f(vector[i].y, vector[i].x) / (M_PI * 2.0f) + 0.5f;
        }
        break;
      }
      case SHD_BLEND_QUADRATIC_SPHERE: {
        for (int64_t i : mask) {
          /* Bias a little bit for the case where input is a unit length vector,
           * to get exactly zero instead of a small random value depending
           * on float precision. */
          const float r = std::max(0.999999f - vector[i].length(), 0.0f);
          fac[i] = r * r;
        }
        break;
      }
      case SHD_BLEND_SPHERICAL: {
        for (int64_t i : mask) {
          /* Bias a little bit for the case where input is a unit length vector,
           * to get exactly zero instead of a small random value depending
           * on float precision. */
          fac[i] = std::max(0.999999f - vector[i].length(), 0.0f);
        }
        break;
      }
    }
    if (compute_color) {
      for (int64_t i : mask) {
        r_color[i] = ColorGeometry4f(fac[i], fac[i], fac[i], 1.0f);
      }
    }
  }
};

static void sh_node_gradient_tex_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  NodeTexGradient *tex = (NodeTexGradient *)node.storage;
  builder.construct_and_set_matching_fn<GradientFunction>(tex->gradient_type);
}

}  // namespace blender::nodes::node_shader_tex_gradient_cc

void register_node_type_sh_tex_gradient()
{
  namespace file_ns = blender::nodes::node_shader_tex_gradient_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_GRADIENT, "Gradient Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_gradient_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_gradient;
  node_type_init(&ntype, file_ns::node_shader_init_tex_gradient);
  node_type_storage(
      &ntype, "NodeTexGradient", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_tex_gradient);
  ntype.build_multi_function = file_ns::sh_node_gradient_tex_build_multi_function;

  nodeRegisterType(&ntype);
}
