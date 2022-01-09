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

#include "node_shader_util.hh"

#include "BLI_noise.hh"

#include "UI_interface.h"
#include "UI_resources.h"

namespace blender::nodes::node_shader_tex_musgrave_cc {

NODE_STORAGE_FUNCS(NodeTexMusgrave)

static void sh_node_tex_musgrave_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>(N_("Vector")).hide_value().implicit_field();
  b.add_input<decl::Float>(N_("W")).min(-1000.0f).max(1000.0f).make_available([](bNode &node) {
    /* Default to 1 instead of 4, because it is much faster. */
    node_storage(node).dimensions = 1;
  });
  b.add_input<decl::Float>(N_("Scale")).min(-1000.0f).max(1000.0f).default_value(5.0f);
  b.add_input<decl::Float>(N_("Detail")).min(0.0f).max(15.0f).default_value(2.0f);
  b.add_input<decl::Float>(N_("Dimension")).min(0.0f).max(1000.0f).default_value(2.0f);
  b.add_input<decl::Float>(N_("Lacunarity")).min(0.0f).max(1000.0f).default_value(2.0f);
  b.add_input<decl::Float>(N_("Offset")).min(-1000.0f).max(1000.0f);
  b.add_input<decl::Float>(N_("Gain")).min(0.0f).max(1000.0f).default_value(1.0f);
  b.add_output<decl::Float>(N_("Fac")).no_muted_links();
}

static void node_shader_buts_tex_musgrave(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "musgrave_dimensions", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemR(layout, ptr, "musgrave_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_musgrave(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeTexMusgrave *tex = MEM_cnew<NodeTexMusgrave>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->musgrave_type = SHD_MUSGRAVE_FBM;
  tex->dimensions = 3;

  node->storage = tex;
}

static const char *gpu_shader_name_get(const int type, const int dimensions)
{
  BLI_assert(type >= 0 && type < 5);
  BLI_assert(dimensions > 0 && dimensions < 5);

  switch (type) {
    case SHD_MUSGRAVE_MULTIFRACTAL:
      return std::array{"node_tex_musgrave_multi_fractal_1d",
                        "node_tex_musgrave_multi_fractal_2d",
                        "node_tex_musgrave_multi_fractal_3d",
                        "node_tex_musgrave_multi_fractal_4d"}[dimensions - 1];
    case SHD_MUSGRAVE_FBM:
      return std::array{"node_tex_musgrave_fBm_1d",
                        "node_tex_musgrave_fBm_2d",
                        "node_tex_musgrave_fBm_3d",
                        "node_tex_musgrave_fBm_4d"}[dimensions - 1];
    case SHD_MUSGRAVE_HYBRID_MULTIFRACTAL:
      return std::array{"node_tex_musgrave_hybrid_multi_fractal_1d",
                        "node_tex_musgrave_hybrid_multi_fractal_2d",
                        "node_tex_musgrave_hybrid_multi_fractal_3d",
                        "node_tex_musgrave_hybrid_multi_fractal_4d"}[dimensions - 1];
    case SHD_MUSGRAVE_RIDGED_MULTIFRACTAL:
      return std::array{"node_tex_musgrave_ridged_multi_fractal_1d",
                        "node_tex_musgrave_ridged_multi_fractal_2d",
                        "node_tex_musgrave_ridged_multi_fractal_3d",
                        "node_tex_musgrave_ridged_multi_fractal_4d"}[dimensions - 1];
    case SHD_MUSGRAVE_HETERO_TERRAIN:
      return std::array{"node_tex_musgrave_hetero_terrain_1d",
                        "node_tex_musgrave_hetero_terrain_2d",
                        "node_tex_musgrave_hetero_terrain_3d",
                        "node_tex_musgrave_hetero_terrain_4d"}[dimensions - 1];
  }
  return nullptr;
}

static int node_shader_gpu_tex_musgrave(GPUMaterial *mat,
                                        bNode *node,
                                        bNodeExecData *UNUSED(execdata),
                                        GPUNodeStack *in,
                                        GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexMusgrave *tex = (NodeTexMusgrave *)node->storage;
  int dimensions = tex->dimensions;
  int type = tex->musgrave_type;

  const char *name = gpu_shader_name_get(type, dimensions);

  return GPU_stack_link(mat, node, name, in, out);
}

static void node_shader_update_tex_musgrave(bNodeTree *ntree, bNode *node)
{
  const NodeTexMusgrave &storage = node_storage(*node);

  bNodeSocket *inVectorSock = nodeFindSocket(node, SOCK_IN, "Vector");
  bNodeSocket *inWSock = nodeFindSocket(node, SOCK_IN, "W");
  bNodeSocket *inOffsetSock = nodeFindSocket(node, SOCK_IN, "Offset");
  bNodeSocket *inGainSock = nodeFindSocket(node, SOCK_IN, "Gain");

  nodeSetSocketAvailability(ntree, inVectorSock, storage.dimensions != 1);
  nodeSetSocketAvailability(ntree, inWSock, storage.dimensions == 1 || storage.dimensions == 4);
  nodeSetSocketAvailability(ntree,
                            inOffsetSock,
                            storage.musgrave_type != SHD_MUSGRAVE_MULTIFRACTAL &&
                                storage.musgrave_type != SHD_MUSGRAVE_FBM);
  nodeSetSocketAvailability(ntree,
                            inGainSock,
                            storage.musgrave_type == SHD_MUSGRAVE_HYBRID_MULTIFRACTAL ||
                                storage.musgrave_type == SHD_MUSGRAVE_RIDGED_MULTIFRACTAL);

  bNodeSocket *outFacSock = nodeFindSocket(node, SOCK_OUT, "Fac");
  node_sock_label(outFacSock, "Height");
}

class MusgraveFunction : public fn::MultiFunction {
 private:
  const int dimensions_;
  const int musgrave_type_;

 public:
  MusgraveFunction(const int dimensions, const int musgrave_type)
      : dimensions_(dimensions), musgrave_type_(musgrave_type)
  {
    BLI_assert(dimensions >= 1 && dimensions <= 4);
    BLI_assert(musgrave_type >= 0 && musgrave_type <= 4);
    static std::array<fn::MFSignature, 20> signatures{
        create_signature(1, SHD_MUSGRAVE_MULTIFRACTAL),
        create_signature(2, SHD_MUSGRAVE_MULTIFRACTAL),
        create_signature(3, SHD_MUSGRAVE_MULTIFRACTAL),
        create_signature(4, SHD_MUSGRAVE_MULTIFRACTAL),

        create_signature(1, SHD_MUSGRAVE_FBM),
        create_signature(2, SHD_MUSGRAVE_FBM),
        create_signature(3, SHD_MUSGRAVE_FBM),
        create_signature(4, SHD_MUSGRAVE_FBM),

        create_signature(1, SHD_MUSGRAVE_HYBRID_MULTIFRACTAL),
        create_signature(2, SHD_MUSGRAVE_HYBRID_MULTIFRACTAL),
        create_signature(3, SHD_MUSGRAVE_HYBRID_MULTIFRACTAL),
        create_signature(4, SHD_MUSGRAVE_HYBRID_MULTIFRACTAL),

        create_signature(1, SHD_MUSGRAVE_RIDGED_MULTIFRACTAL),
        create_signature(2, SHD_MUSGRAVE_RIDGED_MULTIFRACTAL),
        create_signature(3, SHD_MUSGRAVE_RIDGED_MULTIFRACTAL),
        create_signature(4, SHD_MUSGRAVE_RIDGED_MULTIFRACTAL),

        create_signature(1, SHD_MUSGRAVE_HETERO_TERRAIN),
        create_signature(2, SHD_MUSGRAVE_HETERO_TERRAIN),
        create_signature(3, SHD_MUSGRAVE_HETERO_TERRAIN),
        create_signature(4, SHD_MUSGRAVE_HETERO_TERRAIN),
    };
    this->set_signature(&signatures[dimensions + musgrave_type * 4 - 1]);
  }

  static fn::MFSignature create_signature(const int dimensions, const int musgrave_type)
  {
    fn::MFSignatureBuilder signature{"Musgrave"};

    if (ELEM(dimensions, 2, 3, 4)) {
      signature.single_input<float3>("Vector");
    }
    if (ELEM(dimensions, 1, 4)) {
      signature.single_input<float>("W");
    }
    signature.single_input<float>("Scale");
    signature.single_input<float>("Detail");
    signature.single_input<float>("Dimension");
    signature.single_input<float>("Lacunarity");
    if (ELEM(musgrave_type,
             SHD_MUSGRAVE_RIDGED_MULTIFRACTAL,
             SHD_MUSGRAVE_HYBRID_MULTIFRACTAL,
             SHD_MUSGRAVE_HETERO_TERRAIN)) {
      signature.single_input<float>("Offset");
    }
    if (ELEM(musgrave_type, SHD_MUSGRAVE_RIDGED_MULTIFRACTAL, SHD_MUSGRAVE_HYBRID_MULTIFRACTAL)) {
      signature.single_input<float>("Gain");
    }

    signature.single_output<float>("Fac");

    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    auto get_vector = [&](int param_index) -> VArray<float3> {
      return params.readonly_single_input<float3>(param_index, "Vector");
    };
    auto get_w = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "W");
    };
    auto get_scale = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Scale");
    };
    auto get_detail = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Detail");
    };
    auto get_dimension = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Dimension");
    };
    auto get_lacunarity = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Lacunarity");
    };
    auto get_offset = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Offset");
    };
    auto get_gain = [&](int param_index) -> VArray<float> {
      return params.readonly_single_input<float>(param_index, "Gain");
    };

    auto get_r_factor = [&](int param_index) -> MutableSpan<float> {
      return params.uninitialized_single_output_if_required<float>(param_index, "Fac");
    };

    int param = ELEM(dimensions_, 2, 3, 4) + ELEM(dimensions_, 1, 4);
    const VArray<float> &scale = get_scale(param++);
    const VArray<float> &detail = get_detail(param++);
    const VArray<float> &dimension = get_dimension(param++);
    const VArray<float> &lacunarity = get_lacunarity(param++);

    switch (musgrave_type_) {
      case SHD_MUSGRAVE_MULTIFRACTAL: {
        MutableSpan<float> r_factor = get_r_factor(param++);
        const bool compute_factor = !r_factor.is_empty();
        switch (dimensions_) {
          case 1: {
            const VArray<float> &w = get_w(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float position = w[i] * scale[i];
                r_factor[i] = noise::musgrave_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
          case 2: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float2 position = float2(pxyz[0], pxyz[1]);
                r_factor[i] = noise::musgrave_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
          case 3: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 position = vector[i] * scale[i];
                r_factor[i] = noise::musgrave_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
          case 4: {
            const VArray<float3> &vector = get_vector(0);
            const VArray<float> &w = get_w(1);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float pw = w[i] * scale[i];
                const float4 position{pxyz[0], pxyz[1], pxyz[2], pw};
                r_factor[i] = noise::musgrave_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
        }
        break;
      }
      case SHD_MUSGRAVE_RIDGED_MULTIFRACTAL: {
        const VArray<float> &offset = get_offset(param++);
        const VArray<float> &gain = get_gain(param++);
        MutableSpan<float> r_factor = get_r_factor(param++);
        const bool compute_factor = !r_factor.is_empty();
        switch (dimensions_) {
          case 1: {
            const VArray<float> &w = get_w(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float position = w[i] * scale[i];
                r_factor[i] = noise::musgrave_ridged_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
          case 2: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float2 position = float2(pxyz[0], pxyz[1]);
                r_factor[i] = noise::musgrave_ridged_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
          case 3: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 position = vector[i] * scale[i];
                r_factor[i] = noise::musgrave_ridged_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
          case 4: {
            const VArray<float3> &vector = get_vector(0);
            const VArray<float> &w = get_w(1);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float pw = w[i] * scale[i];
                const float4 position{pxyz[0], pxyz[1], pxyz[2], pw};
                r_factor[i] = noise::musgrave_ridged_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
        }
        break;
      }
      case SHD_MUSGRAVE_HYBRID_MULTIFRACTAL: {
        const VArray<float> &offset = get_offset(param++);
        const VArray<float> &gain = get_gain(param++);
        MutableSpan<float> r_factor = get_r_factor(param++);
        const bool compute_factor = !r_factor.is_empty();
        switch (dimensions_) {
          case 1: {
            const VArray<float> &w = get_w(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float position = w[i] * scale[i];
                r_factor[i] = noise::musgrave_hybrid_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
          case 2: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float2 position = float2(pxyz[0], pxyz[1]);
                r_factor[i] = noise::musgrave_hybrid_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
          case 3: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 position = vector[i] * scale[i];
                r_factor[i] = noise::musgrave_hybrid_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
          case 4: {
            const VArray<float3> &vector = get_vector(0);
            const VArray<float> &w = get_w(1);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float pw = w[i] * scale[i];
                const float4 position{pxyz[0], pxyz[1], pxyz[2], pw};
                r_factor[i] = noise::musgrave_hybrid_multi_fractal(
                    position, dimension[i], lacunarity[i], detail[i], offset[i], gain[i]);
              }
            }
            break;
          }
        }
        break;
      }
      case SHD_MUSGRAVE_FBM: {
        MutableSpan<float> r_factor = get_r_factor(param++);
        const bool compute_factor = !r_factor.is_empty();
        switch (dimensions_) {
          case 1: {
            const VArray<float> &w = get_w(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float position = w[i] * scale[i];
                r_factor[i] = noise::musgrave_fBm(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
          case 2: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float2 position = float2(pxyz[0], pxyz[1]);
                r_factor[i] = noise::musgrave_fBm(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
          case 3: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 position = vector[i] * scale[i];
                r_factor[i] = noise::musgrave_fBm(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
          case 4: {
            const VArray<float3> &vector = get_vector(0);
            const VArray<float> &w = get_w(1);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float pw = w[i] * scale[i];
                const float4 position{pxyz[0], pxyz[1], pxyz[2], pw};
                r_factor[i] = noise::musgrave_fBm(
                    position, dimension[i], lacunarity[i], detail[i]);
              }
            }
            break;
          }
        }
        break;
      }
      case SHD_MUSGRAVE_HETERO_TERRAIN: {
        const VArray<float> &offset = get_offset(param++);
        MutableSpan<float> r_factor = get_r_factor(param++);
        const bool compute_factor = !r_factor.is_empty();
        switch (dimensions_) {
          case 1: {
            const VArray<float> &w = get_w(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float position = w[i] * scale[i];
                r_factor[i] = noise::musgrave_hetero_terrain(
                    position, dimension[i], lacunarity[i], detail[i], offset[i]);
              }
            }
            break;
          }
          case 2: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float2 position = float2(pxyz[0], pxyz[1]);
                r_factor[i] = noise::musgrave_hetero_terrain(
                    position, dimension[i], lacunarity[i], detail[i], offset[i]);
              }
            }
            break;
          }
          case 3: {
            const VArray<float3> &vector = get_vector(0);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 position = vector[i] * scale[i];
                r_factor[i] = noise::musgrave_hetero_terrain(
                    position, dimension[i], lacunarity[i], detail[i], offset[i]);
              }
            }
            break;
          }
          case 4: {
            const VArray<float3> &vector = get_vector(0);
            const VArray<float> &w = get_w(1);
            if (compute_factor) {
              for (int64_t i : mask) {
                const float3 pxyz = vector[i] * scale[i];
                const float pw = w[i] * scale[i];
                const float4 position{pxyz[0], pxyz[1], pxyz[2], pw};
                r_factor[i] = noise::musgrave_hetero_terrain(
                    position, dimension[i], lacunarity[i], detail[i], offset[i]);
              }
            }
            break;
          }
        }
        break;
      }
    }
  }
};

static void sh_node_musgrave_build_multi_function(
    blender::nodes::NodeMultiFunctionBuilder &builder)
{
  bNode &node = builder.node();
  NodeTexMusgrave *tex = (NodeTexMusgrave *)node.storage;
  builder.construct_and_set_matching_fn<MusgraveFunction>(tex->dimensions, tex->musgrave_type);
}

}  // namespace blender::nodes::node_shader_tex_musgrave_cc

void register_node_type_sh_tex_musgrave()
{
  namespace file_ns = blender::nodes::node_shader_tex_musgrave_cc;

  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_TEX_MUSGRAVE, "Musgrave Texture", NODE_CLASS_TEXTURE);
  ntype.declare = file_ns::sh_node_tex_musgrave_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_musgrave;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, file_ns::node_shader_init_tex_musgrave);
  node_type_storage(
      &ntype, "NodeTexMusgrave", node_free_standard_storage, node_copy_standard_storage);
  node_type_gpu(&ntype, file_ns::node_shader_gpu_tex_musgrave);
  node_type_update(&ntype, file_ns::node_shader_update_tex_musgrave);
  ntype.build_multi_function = file_ns::sh_node_musgrave_build_multi_function;

  nodeRegisterType(&ntype);
}
