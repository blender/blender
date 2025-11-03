/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "DNA_node_types.h"

#include "RNA_enum_types.hh"

#include "BKE_node.hh"

#include "COM_algorithm_sample_pixel.hh"
#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_map_uv_cc {

static void cmp_node_map_uv_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms)
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Vector>("UV")
      .default_value({1.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .description(
          "The UV coordinates at which to sample the texture. The Z component is assumed to "
          "contain an alpha channel")
      .structure_type(StructureType::Dynamic);

  PanelDeclarationBuilder &sampling_panel = b.add_panel("Sampling").default_closed(true);
  sampling_panel.add_input<decl::Menu>("Interpolation")
      .default_value(CMP_NODE_INTERPOLATION_BILINEAR)
      .static_items(rna_enum_node_compositor_interpolation_items)
      .optional_label()
      .description("Interpolation method");
  sampling_panel.add_input<decl::Menu>("Extension X")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the X axis");
  sampling_panel.add_input<decl::Menu>("Extension Y")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .optional_label()
      .description("The extension mode applied to the Y axis");
}

static void node_composit_init_map_uv(bNodeTree * /*ntree*/, bNode *node)
{
  NodeMapUVData *data = MEM_callocN<NodeMapUVData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class MapUVOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    const Result &input_uv = this->get_input("UV");
    if (input_uv.is_single_value()) {
      this->execute_single();
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_gpu();
    }
    else {
      this->execute_cpu();
    }
  }

  void execute_gpu()
  {
    const Interpolation interpolation = this->get_interpolation();
    gpu::Shader *shader = context().get_shader(this->get_shader_name(interpolation));
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    if (interpolation == Interpolation::Anisotropic) {
      GPU_texture_anisotropic_filter(input_image, true);
      GPU_texture_mipmap_mode(input_image, true, true);
    }
    else {
      const bool use_bilinear = ELEM(
          interpolation, Interpolation::Bilinear, Interpolation::Bicubic);
      GPU_texture_filter_mode(input_image, use_bilinear);
    }

    GPU_texture_extend_mode_x(input_image,
                              map_extension_mode_to_extend_mode(this->get_extension_mode_x()));
    GPU_texture_extend_mode_y(input_image,
                              map_extension_mode_to_extend_mode(this->get_extension_mode_y()));

    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_uv = get_input("UV");
    input_uv.bind_as_texture(shader, "uv_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    input_uv.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  char const *get_shader_name(const Interpolation &interpolation)
  {
    switch (interpolation) {
      case Interpolation::Anisotropic:
        return "compositor_map_uv_anisotropic";
      case Interpolation::Bicubic:
        return "compositor_map_uv_bicubic";
      case Interpolation::Bilinear:
      case Interpolation::Nearest:
        return "compositor_map_uv";
    }

    return "compositor_map_uv";
  }

  void execute_cpu()
  {
    const Interpolation interpolation = this->get_interpolation();
    if (interpolation == Interpolation::Anisotropic) {
      this->execute_cpu_anisotropic();
    }
    else {
      this->execute_cpu_interpolation(interpolation);
    }
  }

  void execute_single()
  {
    const Interpolation interpolation = this->get_interpolation();
    const ExtensionMode extension_mode_x = this->get_extension_mode_x();
    const ExtensionMode extension_mode_y = this->get_extension_mode_y();
    const Result &input_uv = get_input("UV");
    const Result &input_image = get_input("Image");

    float2 uv_coordinates = input_uv.get_single_value<float3>().xy();
    float4 sampled_color = float4(sample_pixel(this->context(),
                                               input_image,
                                               interpolation,
                                               extension_mode_x,
                                               extension_mode_y,
                                               uv_coordinates));

    /* The UV input is assumed to contain an alpha channel as its third channel, since the
     * UV coordinates might be defined in only a subset area of the UV texture as mentioned.
     * In that case, the alpha is typically opaque at the subset area and transparent
     * everywhere else, and alpha pre-multiplication is then performed. This format of having
     * an alpha channel in the UV coordinates is the format used by UV passes in render
     * engines, hence the mentioned logic. */
    float alpha = input_uv.get_single_value<float3>().z;

    float4 result = sampled_color * alpha;

    Result &output = get_result("Image");
    output.allocate_single_value();
    output.set_single_value(Color(result));
  }

  void execute_cpu_interpolation(const Interpolation &interpolation)
  {
    const ExtensionMode extension_mode_x = this->get_extension_mode_x();
    const ExtensionMode extension_mode_y = this->get_extension_mode_y();
    const Result &input_image = get_input("Image");
    const Result &input_uv = get_input("UV");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float2 uv_coordinates = input_uv.load_pixel<float3>(texel).xy();
      float4 sampled_color = float4(input_image.sample<Color>(
          uv_coordinates, interpolation, extension_mode_x, extension_mode_y));
      /* The UV input is assumed to contain an alpha channel as its third channel, since the
       * UV coordinates might be defined in only a subset area of the UV texture as mentioned.
       * In that case, the alpha is typically opaque at the subset area and transparent
       * everywhere else, and alpha pre-multiplication is then performed. This format of having
       * an alpha channel in the UV coordinates is the format used by UV passes in render
       * engines, hence the mentioned logic. */
      float alpha = input_uv.load_pixel<float3>(texel).z;

      float4 result = sampled_color * alpha;

      output_image.store_pixel(texel, Color(result));
    });
  }

  void execute_cpu_anisotropic()
  {
    const Result &input_image = get_input("Image");
    const Result &input_uv = get_input("UV");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);

    /* In order to perform EWA sampling, we need to compute the partial derivative of the UV
     * coordinates along the x and y directions using a finite difference approximation. But in
     * order to avoid loading multiple neighboring UV coordinates for each pixel, we operate on
     * the image in 2x2 blocks of pixels, where the derivatives are computed horizontally and
     * vertically across the 2x2 block such that odd texels use a forward finite difference
     * equation while even invocations use a backward finite difference equation. */
    const int2 size = domain.size;
    const int2 uv_size = input_uv.domain().size;
    parallel_for(math::divide_ceil(size, int2(2)), [&](const int2 base_texel) {
      const int x = base_texel.x * 2;
      const int y = base_texel.y * 2;

      const int2 lower_left_texel = int2(x, y);
      const int2 lower_right_texel = int2(x + 1, y);
      const int2 upper_left_texel = int2(x, y + 1);
      const int2 upper_right_texel = int2(x + 1, y + 1);

      const float2 lower_left_uv = input_uv.load_pixel<float3>(lower_left_texel).xy();
      const float2 lower_right_uv = input_uv.load_pixel_extended<float3>(lower_right_texel).xy();
      const float2 upper_left_uv = input_uv.load_pixel_extended<float3>(upper_left_texel).xy();
      const float2 upper_right_uv = input_uv.load_pixel_extended<float3>(upper_right_texel).xy();

      /* Compute the partial derivatives using finite difference. Divide by the input size since
       * sample_ewa_zero assumes derivatives with respect to texel coordinates. */
      const float2 lower_x_gradient = (lower_right_uv - lower_left_uv) / uv_size.x;
      const float2 left_y_gradient = (upper_left_uv - lower_left_uv) / uv_size.y;
      const float2 right_y_gradient = (upper_right_uv - lower_right_uv) / uv_size.y;
      const float2 upper_x_gradient = (upper_right_uv - upper_left_uv) / uv_size.x;

      /* Computes one of the 2x2 pixels given its texel location, coordinates, and gradients. */
      auto compute_pixel = [&](const int2 &texel,
                               const float2 &coordinates,
                               const float2 &x_gradient,
                               const float2 &y_gradient) {
        /* Sample the input using the UV coordinates passing in the computed gradients in order
         * to utilize the anisotropic filtering capabilities of the sampler. */
        float4 sampled_color = input_image.sample_ewa_zero(coordinates, x_gradient, y_gradient);

        /* The UV input is assumed to contain an alpha channel as its third channel, since the
         * UV coordinates might be defined in only a subset area of the UV texture as mentioned.
         * In that case, the alpha is typically opaque at the subset area and transparent
         * everywhere else, and alpha pre-multiplication is then performed. This format of having
         * an alpha channel in the UV coordinates is the format used by UV passes in render
         * engines, hence the mentioned logic. */
        float alpha = input_uv.load_pixel<float3>(texel).z;

        float4 result = sampled_color * alpha;

        output_image.store_pixel(texel, Color(result));
      };

      /* Compute each of the pixels in the 2x2 block, making sure to exempt out of bounds right
       * and upper pixels. */
      compute_pixel(lower_left_texel, lower_left_uv, lower_x_gradient, left_y_gradient);
      if (lower_right_texel.x != size.x) {
        compute_pixel(lower_right_texel, lower_right_uv, lower_x_gradient, right_y_gradient);
      }
      if (upper_left_texel.y != size.y) {
        compute_pixel(upper_left_texel, upper_left_uv, upper_x_gradient, left_y_gradient);
      }
      if (upper_right_texel.x != size.x && upper_right_texel.y != size.y) {
        compute_pixel(upper_right_texel, upper_right_uv, upper_x_gradient, right_y_gradient);
      }
    });
  }

  Interpolation get_interpolation()
  {
    const Result &input = this->get_input("Interpolation");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_INTERPOLATION_BILINEAR);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPNodeInterpolation interpolation = static_cast<CMPNodeInterpolation>(menu_value.value);
    switch (interpolation) {
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
      case CMP_NODE_INTERPOLATION_ANISOTROPIC:
        return Interpolation::Anisotropic;
    }

    return Interpolation::Nearest;
  }

  ExtensionMode get_extension_mode_x()
  {
    const Result &input = this->get_input("Extension X");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_x = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_x) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }

  ExtensionMode get_extension_mode_y()
  {
    const Result &input = this->get_input("Extension Y");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_EXTENSION_MODE_CLIP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    const CMPExtensionMode extension_y = static_cast<CMPExtensionMode>(menu_value.value);
    switch (extension_y) {
      case CMP_NODE_EXTENSION_MODE_CLIP:
        return ExtensionMode::Clip;
      case CMP_NODE_EXTENSION_MODE_REPEAT:
        return ExtensionMode::Repeat;
      case CMP_NODE_EXTENSION_MODE_EXTEND:
        return ExtensionMode::Extend;
    }

    return ExtensionMode::Clip;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new MapUVOperation(context, node);
}

}  // namespace blender::nodes::node_composite_map_uv_cc

static void register_node_type_cmp_mapuv()
{
  namespace file_ns = blender::nodes::node_composite_map_uv_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeMapUV", CMP_NODE_MAP_UV);
  ntype.ui_name = "Map UV";
  ntype.ui_description =
      "Map a texture using UV coordinates, to apply a texture to objects in compositing";
  ntype.enum_name_legacy = "MAP_UV";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_map_uv_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.initfunc = file_ns::node_composit_init_map_uv;
  blender::bke::node_type_storage(
      ntype, "NodeMapUVData", node_free_standard_storage, node_copy_standard_storage);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_mapuv)
