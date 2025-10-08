/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_magic_cc {

static void sh_node_tex_magic_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f).description(
      "Scale of the texture");
  b.add_input<decl::Float>("Distortion")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(1.0f)
      .description("Amount of distortion");
  b.add_output<decl::Color>("Color").no_muted_links();
  b.add_output<decl::Float>("Factor", "Fac").no_muted_links();
}

static void node_shader_buts_tex_magic(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "turbulence_depth", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

static void node_shader_init_tex_magic(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexMagic *tex = MEM_callocN<NodeTexMagic>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->depth = 2;

  node->storage = tex;
}

static int node_shader_gpu_tex_magic(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  NodeTexMagic *tex = (NodeTexMagic *)node->storage;
  float depth = tex->depth;

  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  return GPU_stack_link(mat, node, "node_tex_magic", in, out, GPU_constant(&depth));
}

class MagicFunction : public mf::MultiFunction {
 private:
  int depth_;

 public:
  MagicFunction(int depth) : depth_(depth)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"MagicFunction", signature};
      builder.single_input<float3>("Vector");
      builder.single_input<float>("Scale");
      builder.single_input<float>("Distortion");
      builder.single_output<ColorGeometry4f>("Color");
      builder.single_output<float>("Fac", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<float> &scale = params.readonly_single_input<float>(1, "Scale");
    const VArray<float> &distortion = params.readonly_single_input<float>(2, "Distortion");

    MutableSpan<ColorGeometry4f> r_color = params.uninitialized_single_output<ColorGeometry4f>(
        3, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output_if_required<float>(4, "Fac");

    const bool compute_factor = !r_fac.is_empty();

    mask.foreach_index([&](const int64_t i) {
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
    });
    if (compute_factor) {
      mask.foreach_index([&](const int64_t i) {
        r_fac[i] = (r_color[i].r + r_color[i].g + r_color[i].b) * (1.0f / 3.0f);
      });
    }
  }
};

static void sh_node_magic_tex_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  NodeTexMagic *tex = (NodeTexMagic *)node.storage;
  builder.construct_and_set_matching_fn<MagicFunction>(tex->depth);
}

}  // namespace blender::nodes::node_shader_tex_magic_cc

void register_node_type_sh_tex_magic()
{
  namespace file_ns = blender::nodes::node_shader_tex_magic_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexMagic", SH_NODE_TEX_MAGIC);
  ntype.ui_name = "Magic Texture";
  ntype.ui_description = "Generate a psychedelic color texture";
  ntype.enum_name_legacy = "TEX_MAGIC";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_magic_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_magic;
  ntype.initfunc = file_ns::node_shader_init_tex_magic;
  blender::bke::node_type_storage(
      ntype, "NodeTexMagic", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_magic;
  ntype.build_multi_function = file_ns::sh_node_magic_tex_build_multi_function;

  blender::bke::node_register_type(ntype);
}
