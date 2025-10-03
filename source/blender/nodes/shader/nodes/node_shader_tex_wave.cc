/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_texture.h"

#include "BLI_math_vector.h"
#include "BLI_noise.hh"

#include "NOD_multi_function.hh"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_wave_cc {

static void sh_node_tex_wave_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector").implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Float>("Scale").min(-1000.0f).max(1000.0f).default_value(5.0f).description(
      "Overall texture scale");
  b.add_input<decl::Float>("Distortion")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(0.0f)
      .description("Amount of distortion of the wave");
  b.add_input<decl::Float>("Detail").min(0.0f).max(15.0f).default_value(2.0f).description(
      "Amount of distortion noise detail");
  b.add_input<decl::Float>("Detail Scale")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(1.0f)
      .description("Scale of distortion noise");
  b.add_input<decl::Float>("Detail Roughness")
      .min(0.0f)
      .max(1.0f)
      .default_value(0.5f)
      .subtype(PROP_FACTOR)
      .description("Blend between a smoother noise pattern, and rougher with sharper peaks");
  b.add_input<decl::Float>("Phase Offset")
      .min(-1000.0f)
      .max(1000.0f)
      .default_value(0.0f)
      .description(
          "Position of the wave along the Bands Direction.\n"
          "This can be used as an input for more control over the distortion");
  b.add_output<decl::Color>("Color").no_muted_links();
  b.add_output<decl::Float>("Factor", "Fac").no_muted_links();
}

static void node_shader_buts_tex_wave(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "wave_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  int type = RNA_enum_get(ptr, "wave_type");
  if (type == SHD_WAVE_BANDS) {
    layout->prop(ptr, "bands_direction", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }
  else { /* SHD_WAVE_RINGS */
    layout->prop(ptr, "rings_direction", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  }

  layout->prop(ptr, "wave_profile", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_wave(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexWave *tex = MEM_callocN<NodeTexWave>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  tex->wave_type = SHD_WAVE_BANDS;
  tex->bands_direction = SHD_WAVE_BANDS_DIRECTION_X;
  tex->rings_direction = SHD_WAVE_RINGS_DIRECTION_X;
  tex->wave_profile = SHD_WAVE_PROFILE_SIN;
  node->storage = tex;
}

static int node_shader_gpu_tex_wave(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(mat, node, &in[0].link);
  node_shader_gpu_tex_mapping(mat, node, in, out);

  NodeTexWave *tex = (NodeTexWave *)node->storage;
  float wave_type = tex->wave_type;
  float bands_direction = tex->bands_direction;
  float rings_direction = tex->rings_direction;
  float wave_profile = tex->wave_profile;

  return GPU_stack_link(mat,
                        node,
                        "node_tex_wave",
                        in,
                        out,
                        GPU_constant(&wave_type),
                        GPU_constant(&bands_direction),
                        GPU_constant(&rings_direction),
                        GPU_constant(&wave_profile));
}

class WaveFunction : public mf::MultiFunction {
 private:
  int wave_type_;
  int bands_direction_;
  int rings_direction_;
  int wave_profile_;

 public:
  WaveFunction(int wave_type, int bands_direction, int rings_direction, int wave_profile)
      : wave_type_(wave_type),
        bands_direction_(bands_direction),
        rings_direction_(rings_direction),
        wave_profile_(wave_profile)
  {
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"MagicFunction", signature};
      builder.single_input<float3>("Vector");
      builder.single_input<float>("Scale");
      builder.single_input<float>("Distortion");
      builder.single_input<float>("Detail");
      builder.single_input<float>("Detail Scale");
      builder.single_input<float>("Detail Roughness");
      builder.single_input<float>("Phase Offset");
      builder.single_output<ColorGeometry4f>("Color", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Fac");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<float> &scale = params.readonly_single_input<float>(1, "Scale");
    const VArray<float> &distortion = params.readonly_single_input<float>(2, "Distortion");
    const VArray<float> &detail = params.readonly_single_input<float>(3, "Detail");
    const VArray<float> &dscale = params.readonly_single_input<float>(4, "Detail Scale");
    const VArray<float> &droughness = params.readonly_single_input<float>(5, "Detail Roughness");
    const VArray<float> &phase = params.readonly_single_input<float>(6, "Phase Offset");

    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(7, "Color");
    MutableSpan<float> r_fac = params.uninitialized_single_output<float>(8, "Fac");

    mask.foreach_index([&](const int64_t i) {
      float3 p = vector[i] * scale[i];
      /* Prevent precision issues on unit coordinates. */
      p = (p + 0.000001f) * 0.999999f;

      float n = 0.0f;
      float val = 0.0f;

      switch (wave_type_) {
        case SHD_WAVE_BANDS:
          switch (bands_direction_) {
            case SHD_WAVE_BANDS_DIRECTION_X:
              n = p.x * 20.0f;
              break;
            case SHD_WAVE_BANDS_DIRECTION_Y:
              n = p.y * 20.0f;
              break;
            case SHD_WAVE_BANDS_DIRECTION_Z:
              n = p.z * 20.0f;
              break;
            case SHD_WAVE_BANDS_DIRECTION_DIAGONAL:
              n = (p.x + p.y + p.z) * 10.0f;
              break;
          }
          break;
        case SHD_WAVE_RINGS:
          float3 rp = p;
          switch (rings_direction_) {
            case SHD_WAVE_RINGS_DIRECTION_X:
              rp *= float3(0.0f, 1.0f, 1.0f);
              break;
            case SHD_WAVE_RINGS_DIRECTION_Y:
              rp *= float3(1.0f, 0.0f, 1.0f);
              break;
            case SHD_WAVE_RINGS_DIRECTION_Z:
              rp *= float3(1.0f, 1.0f, 0.0f);
              break;
            case SHD_WAVE_RINGS_DIRECTION_SPHERICAL:
              /* Ignore. */
              break;
          }
          n = len_v3(rp) * 20.0f;
          break;
      }

      n += phase[i];

      if (distortion[i] != 0.0f) {
        n += distortion[i] *
             (noise::perlin_fbm<float3>(p * dscale[i], detail[i], droughness[i], 2.0f, true) *
                  2.0f -
              1.0f);
      }

      switch (wave_profile_) {
        case SHD_WAVE_PROFILE_SIN:
          val = 0.5f + 0.5f * sinf(n - M_PI_2);
          break;
        case SHD_WAVE_PROFILE_SAW:
          n /= M_PI * 2.0f;
          val = n - floorf(n);
          break;
        case SHD_WAVE_PROFILE_TRI:
          n /= M_PI * 2.0f;
          val = fabsf(n - floorf(n + 0.5f)) * 2.0f;
          break;
      }

      r_fac[i] = val;
    });
    if (!r_color.is_empty()) {
      mask.foreach_index([&](const int64_t i) {
        r_color[i] = ColorGeometry4f(r_fac[i], r_fac[i], r_fac[i], 1.0f);
      });
    }
  }
};

static void sh_node_wave_tex_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const bNode &node = builder.node();
  NodeTexWave *tex = (NodeTexWave *)node.storage;
  builder.construct_and_set_matching_fn<WaveFunction>(
      tex->wave_type, tex->bands_direction, tex->rings_direction, tex->wave_profile);
}

NODE_SHADER_MATERIALX_BEGIN
#ifdef WITH_MATERIALX
{
  NodeTexWave *tex = (NodeTexWave *)node_->storage;

  NodeItem scale = get_input_value("Scale", NodeItem::Type::Float);
  NodeItem distortion = get_input_value("Distortion", NodeItem::Type::Float);
  NodeItem detail = get_input_default("Detail", NodeItem::Type::Float);
  NodeItem detail_scale = get_input_value("Detail Scale", NodeItem::Type::Float);
  NodeItem detail_rough = get_input_value("Detail Roughness", NodeItem::Type::Float);
  NodeItem phase_offset = get_input_value("Phase Offset", NodeItem::Type::Float);
  NodeItem vector = get_input_link("Vector", NodeItem::Type::Vector3);
  if (!vector) {
    vector = texcoord_node(NodeItem::Type::Vector3);
  }

  /* Adjustment to get result as Cycles. */
  distortion = distortion * val(10.0f);
  detail_scale = detail_scale * val(10.0f);

  NodeItem pos = vector * scale;
  NodeItem fractal = create_node("fractal3d",
                                 NodeItem::Type::Float,
                                 {{"position", pos},
                                  {"octaves", val(int(detail.value->asA<float>()))},
                                  {"lacunarity", val(2.0f)}});
  NodeItem value = val(0.0f);
  switch (tex->wave_type) {
    case SHD_WAVE_BANDS:
      switch (tex->bands_direction) {
        case SHD_WAVE_BANDS_DIRECTION_X:
          value = pos[0] * val(20.0f);
          break;
        case SHD_WAVE_BANDS_DIRECTION_Y:
          value = pos[1] * val(20.0f);
          break;
        case SHD_WAVE_BANDS_DIRECTION_Z:
          value = pos[2] * val(20.0f);
          break;
        case SHD_WAVE_BANDS_DIRECTION_DIAGONAL:
          value = (pos[0] + pos[1] + pos[2]) * val(10.0f);
          break;
        default:
          BLI_assert_unreachable();
      }
      break;
    case SHD_WAVE_RINGS:
      NodeItem rpos = pos;
      switch (tex->rings_direction) {
        case SHD_WAVE_RINGS_DIRECTION_X:
          rpos = pos * val(MaterialX::Vector3(0.0f, 1.0f, 1.0f));
          break;
        case SHD_WAVE_RINGS_DIRECTION_Y:
          rpos = pos * val(MaterialX::Vector3(1.0f, 0.0f, 1.0f));
          break;
        case SHD_WAVE_RINGS_DIRECTION_Z:
          rpos = pos * val(MaterialX::Vector3(1.0f, 1.0f, 0.0f));
          break;
        case SHD_WAVE_RINGS_DIRECTION_SPHERICAL:
          /* Ignore. */
          break;
        default:
          BLI_assert_unreachable();
      }
      value = rpos.length() * val(20.0f);
      break;
  }
  value = value + phase_offset + distortion * detail_scale * fractal;

  NodeItem res = empty();
  switch (tex->wave_profile) {
    case SHD_WAVE_PROFILE_SIN:
      res = val(0.5f) + val(0.5f) * (value - val(float(M_PI_2))).sin();
      break;
    case SHD_WAVE_PROFILE_SAW:
      value = value / val(float(M_PI * 2.0f));
      res = value - value.floor();
      break;
    case SHD_WAVE_PROFILE_TRI:
      value = value / val(float(M_PI * 2.0f));
      res = (value - (value + val(0.5f)).floor()).abs() * val(2.0f);
      break;
    default:
      BLI_assert_unreachable();
  }
  return res;
}
#endif
NODE_SHADER_MATERIALX_END

}  // namespace blender::nodes::node_shader_tex_wave_cc

void register_node_type_sh_tex_wave()
{
  namespace file_ns = blender::nodes::node_shader_tex_wave_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexWave", SH_NODE_TEX_WAVE);
  ntype.ui_name = "Wave Texture";
  ntype.ui_description = "Generate procedural bands or rings with noise";
  ntype.enum_name_legacy = "TEX_WAVE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_wave_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_wave;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Middle);
  ntype.initfunc = file_ns::node_shader_init_tex_wave;
  blender::bke::node_type_storage(
      ntype, "NodeTexWave", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_wave;
  ntype.build_multi_function = file_ns::sh_node_wave_tex_build_multi_function;
  ntype.materialx_fn = file_ns::node_shader_materialx;
  blender::bke::node_type_size(ntype, 160, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
