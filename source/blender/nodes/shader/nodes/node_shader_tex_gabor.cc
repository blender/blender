/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_numbers.hh"
#include "BLI_noise.hh"

#include "BKE_texture.h"

#include "node_shader_util.hh"
#include "node_util.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_tex_gabor_cc {

NODE_STORAGE_FUNCS(NodeTexGabor)

static void sh_node_tex_gabor_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector")
      .implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD)
      .description(
          "The coordinates at which Gabor noise will be evaluated. The Z component is ignored in "
          "the 2D case");
  b.add_input<decl::Float>("Scale").default_value(5.0f).description(
      "The scale of the Gabor noise");
  b.add_input<decl::Float>("Frequency")
      .default_value(2.0f)
      .min(0.0f)
      .description(
          "The rate at which the Gabor noise changes across space. This is different from the "
          "Scale input in that it only scales perpendicular to the Gabor noise direction");
  b.add_input<decl::Float>("Anisotropy")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "The directionality of Gabor noise. 1 means the noise is completely directional, while "
          "0 means the noise is omnidirectional");
  b.add_input<decl::Float>("Orientation", "Orientation 2D")
      .default_value(math::numbers::pi / 4)
      .subtype(PROP_ANGLE)
      .description("The direction of the anisotropic Gabor noise");
  b.add_input<decl::Vector>("Orientation", "Orientation 3D")
      .default_value({math::numbers::sqrt2, math::numbers::sqrt2, 0.0f})
      .subtype(PROP_DIRECTION)
      .description("The direction of the anisotropic Gabor noise");
  b.add_output<decl::Float>("Value").description(
      "The Gabor noise value with both random intensity and phase. This is equal to sine the "
      "phase multiplied by the intensity");
  b.add_output<decl::Float>("Phase").description(
      "The phase of the Gabor noise, which has no random intensity");
  b.add_output<decl::Float>("Intensity")
      .description("The intensity of the Gabor noise, which has no random phase");
}

static void node_shader_buts_tex_gabor(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "gabor_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_gabor(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexGabor *storage = MEM_callocN<NodeTexGabor>(__func__);
  BKE_texture_mapping_default(&storage->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&storage->base.color_mapping);

  storage->type = SHD_GABOR_TYPE_2D;

  node->storage = storage;
}

static void node_shader_update_tex_gabor(bNodeTree *ntree, bNode *node)
{
  const NodeTexGabor &storage = node_storage(*node);

  bNodeSocket *orientation_2d_socket = bke::node_find_socket(*node, SOCK_IN, "Orientation 2D");
  bke::node_set_socket_availability(
      *ntree, *orientation_2d_socket, storage.type == SHD_GABOR_TYPE_2D);

  bNodeSocket *orientation_3d_socket = bke::node_find_socket(*node, SOCK_IN, "Orientation 3D");
  bke::node_set_socket_availability(
      *ntree, *orientation_3d_socket, storage.type == SHD_GABOR_TYPE_3D);
}

static int node_shader_gpu_tex_gabor(GPUMaterial *material,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  node_shader_gpu_default_tex_coord(material, node, &in[0].link);
  node_shader_gpu_tex_mapping(material, node, in, out);

  const float type = float(node_storage(*node).type);
  return GPU_stack_link(material, node, "node_tex_gabor", in, out, GPU_constant(&type));
}

class GaborNoiseFunction : public mf::MultiFunction {
 private:
  NodeGaborType type_;

 public:
  GaborNoiseFunction(const NodeGaborType type) : type_(type)
  {
    static std::array<mf::Signature, 2> signatures{
        create_signature(SHD_GABOR_TYPE_2D),
        create_signature(SHD_GABOR_TYPE_3D),
    };
    this->set_signature(&signatures[type]);
  }

  static mf::Signature create_signature(const NodeGaborType type)
  {
    mf::Signature signature;
    mf::SignatureBuilder builder{"GaborNoise", signature};

    builder.single_input<float3>("Vector");
    builder.single_input<float>("Scale");
    builder.single_input<float>("Frequency");
    builder.single_input<float>("Anisotropy");

    if (type == SHD_GABOR_TYPE_2D) {
      builder.single_input<float>("Orientation");
    }
    else {
      builder.single_input<float3>("Orientation");
    }

    builder.single_output<float>("Value", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float>("Phase", mf::ParamFlag::SupportsUnusedOutput);
    builder.single_output<float>("Intensity", mf::ParamFlag::SupportsUnusedOutput);

    return signature;
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    const VArray<float> &scale = params.readonly_single_input<float>(1, "Scale");
    const VArray<float> &frequency = params.readonly_single_input<float>(2, "Frequency");
    const VArray<float> &anisotropy = params.readonly_single_input<float>(3, "Anisotropy");
    /* A parameter index of 4 is reserved for Orientation input below. */
    MutableSpan<float> r_value = params.uninitialized_single_output_if_required<float>(5, "Value");
    MutableSpan<float> r_phase = params.uninitialized_single_output_if_required<float>(6, "Phase");
    MutableSpan<float> r_intensity = params.uninitialized_single_output_if_required<float>(
        7, "Intensity");

    switch (type_) {
      case SHD_GABOR_TYPE_2D: {
        const VArray<float> &orientation = params.readonly_single_input<float>(4, "Orientation");
        mask.foreach_index([&](const int64_t i) {
          noise::gabor(vector[i].xy(),
                       scale[i],
                       frequency[i],
                       anisotropy[i],
                       orientation[i],
                       r_value.is_empty() ? nullptr : &r_value[i],
                       r_phase.is_empty() ? nullptr : &r_phase[i],
                       r_intensity.is_empty() ? nullptr : &r_intensity[i]);
        });
        break;
      }
      case SHD_GABOR_TYPE_3D: {
        const VArray<float3> &orientation = params.readonly_single_input<float3>(4, "Orientation");
        mask.foreach_index([&](const int64_t i) {
          noise::gabor(vector[i],
                       scale[i],
                       frequency[i],
                       anisotropy[i],
                       orientation[i],
                       r_value.is_empty() ? nullptr : &r_value[i],
                       r_phase.is_empty() ? nullptr : &r_phase[i],
                       r_intensity.is_empty() ? nullptr : &r_intensity[i]);
        });
        break;
      }
    }
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.allocates_array = false;
    hints.min_grain_size = 100;
    return hints;
  }
};

static void build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeTexGabor &storage = node_storage(builder.node());
  builder.construct_and_set_matching_fn<GaborNoiseFunction>(NodeGaborType(storage.type));
}

}  // namespace blender::nodes::node_shader_tex_gabor_cc

void register_node_type_sh_tex_gabor()
{
  namespace file_ns = blender::nodes::node_shader_tex_gabor_cc;

  static blender::bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexGabor", SH_NODE_TEX_GABOR);
  ntype.ui_name = "Gabor Texture";
  ntype.ui_description = "Generate Gabor noise";
  ntype.enum_name_legacy = "TEX_GABOR";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_gabor_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_gabor;
  ntype.initfunc = file_ns::node_shader_init_tex_gabor;
  node_type_storage(ntype, "NodeTexGabor", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_tex_gabor;
  ntype.updatefunc = file_ns::node_shader_update_tex_gabor;
  ntype.build_multi_function = file_ns::build_multi_function;

  node_register_type(ntype);
}
