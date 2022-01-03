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

namespace blender::nodes::node_shader_tex_magic_cc {

static void sh_node_tex_magic_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector")).implicit_field();
  b.add_input<decl::Float>(N_("Scale")).min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>(N_("Distortion")).min(-1000.0f).max(1000.0f).default_value(1.0f);
  b.add_output<decl::Color>(N_("Color")).no_muted_links();
  b.add_output<decl::Float>(N_("Fac")).no_muted_links();
};

static void node_shader_init_tex_magic(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexMagic *tex = MEM_cnew<NodeTexMagic>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->depth = 2;

  node->storage = tex;
}

static int node_shader_gpu_tex_magic(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData *UNUSED(execdata),
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  NodeTexMagic *tex = (NodeTexMagic *)node->storage;
  float depth = tex->depth;

  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_magic", in, out, GPU_constant(&depth));
}

class MagicFunction : public fn::MultiFunction {
 private:
  int depth_;

 public:
  MagicFunction(int depth) : depth_(depth)
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"MagicFunction"};
    signature.single_input<float3>("Vector");
    signature.single_input<float>("Scale");
    signature.single_input<float>("Distortion");
    signature.single_output<ColorGeometry4f>("Color");
    signature.single_output<float>("Fac");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<float> &scale = params.readonly_single_input<float>(1, "Scale");
    const VArray<float> &distortion = params.readonly_single_input<float>(2, "Distortion");

    MutableSpan<ColorGeometry4f> r_color = params.uninitialized_single_output<ColorGeometry4f>(
        3, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output_if_required<float>(4, "Fac");

    const bool compute_factor = !r_fac.is_empty();

    for (int64_t i : mask) {
      const float3 co = vector[i] * scale[i];
      const float distort = distortion[i];
      float x = sinf((co[0] + co[1] + co[2]) * 5.0f);
      float y = cosf((-co[0] + co[1] - co[2]) * 5.0f);
      float z = -cosf((-co[0] - co[1] + co[2]) * 5.0f);

      if (depth_ > 0) {
        x *= distort;
        y *= distort;
        z *= distort;
        y = -cosf(x - y + z);
        y *= distort;

        if (depth_ > 1) {
          x = cosf(x - y - z);
          x *= distort;

          if (depth_ > 2) {
            z = sinf(-x - y - z);
            z *= distort;

            if (depth_ > 3) {
              x = -cosf(-x + y - z);
              x *= distort;

              if (depth_ > 4) {
                y = -sinf(-x + y + z);
                y *= distort;

                if (depth_ > 5) {
                  y = -cosf(-x + y + z);
                  y *= distort;

                  if (depth_ > 6) {
                    x = cosf(x + y + z);
                    x *= distort;

                    if (depth_ > 7) {
                      z = sinf(x + y - z);
                      z *= distort;

                      if (depth_ > 8) {
                        x = -cosf(-x - y + z);
                        x *= distort;

                        if (depth_ > 9) {
                          y = -sinf(x - y + z);
                          y *= distort;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }

      if (distort != 0.0f) {
        const float d = distort * 2.0f;
        x /= d;
        y /= d;
        z /= d;
      }

      r_color[i] = ColorGeometry4f(0.5f - x, 0.5f - y, 0.5f - z, 1.0f);
    }
    if (compute_factor) {
      for (int64_t i : mask) {
        r_fac[i] = (r_color[i].r + r_color[i].g + r_color[i].b) * (1.0f / 3.0f);
      }
    }
  }
};

static void sh_node_magic_tex_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  NodeTexMagic *tex = (NodeTexMagic *)node.storage;
  builder.construct_and_set_matching_fn<MagicFunction>(tex->depth);
}

}  // namespace blender::nodes::node_shader_tex_magic_cc

void register_node_type_sh_tex_magic()
{
  namespace file_ns = blender::nodes::node_shader_tex_magic_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_MAGIC, "Magic Texture", NODE_CLASS_TEXTURE, 0);
  ntype.declare = file_ns::sh_node_tex_magic_declare;
  node_type_init(&ntype, file_ns::node_shader_init_tex_magic);
  node_type_storage(
      &ntype, "NodeTexMagic", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_tex_magic);
  ntype.build_multi_function = file_ns::sh_node_magic_tex_build_multi_function;

  nodeRegisterType(&ntype);
}
