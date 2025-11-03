/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "RNA_enum_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "BKE_node.hh"

#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_displace_cc {

static void cmp_node_displace_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Vector>("Displacement")
      .dimensions(2)
      .default_value({0.0f, 0.0f})
      .structure_type(StructureType::Dynamic);

  PanelDeclarationBuilder &sampling_panel = b.add_panel("Sampling").default_closed(true);
  sampling_panel.add_input<decl::Menu>("Interpolation")
      .default_value(CMP_NODE_INTERPOLATION_BILINEAR)
      .static_items(rna_enum_node_compositor_interpolation_items)
      .description("Interpolation method")
      .optional_label();
  sampling_panel.add_input<decl::Menu>("Extension X")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .description("The extension mode applied to the X axis")
      .optional_label();
  sampling_panel.add_input<decl::Menu>("Extension Y")
      .default_value(CMP_NODE_EXTENSION_MODE_CLIP)
      .static_items(rna_enum_node_compositor_extension_items)
      .description("The extension mode applied to the Y axis")
      .optional_label();
}

static void cmp_node_init_displace(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, kept for forward compatibility. */
  NodeDisplaceData *data = MEM_callocN<NodeDisplaceData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class DisplaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    if (input.is_single_value()) {
      output.share_data(input);
      return;
    }

    const Result &displacement = this->get_input("Displacement");
    if (displacement.is_single_value()) {
      output.share_data(input);
      output.transform(math::from_location<float3x3>(displacement.get_single_value<float2>()));
      output.get_realization_options().interpolation = this->get_interpolation();
      output.get_realization_options().extension_x = this->get_extension_mode_x();
      output.get_realization_options().extension_y = this->get_extension_mode_y();
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
    gpu::Shader *shader = this->context().get_shader(this->get_shader_name(interpolation));
    GPU_shader_bind(shader);

    const Result &input_image = this->get_input("Image");
    if (interpolation == Interpolation::Anisotropic) {
      GPU_texture_anisotropic_filter(input_image, true);
      GPU_texture_mipmap_mode(input_image, true, true);
    }
    else {
      const bool use_bilinear = ELEM(
          interpolation, Interpolation::Bilinear, Interpolation::Bicubic);
      GPU_texture_filter_mode(input_image, use_bilinear);
    }

    const ExtensionMode extension_x = this->get_extension_mode_x();
    const ExtensionMode extension_y = this->get_extension_mode_y();
    GPU_texture_extend_mode_x(input_image, map_extension_mode_to_extend_mode(extension_x));
    GPU_texture_extend_mode_y(input_image, map_extension_mode_to_extend_mode(extension_y));
    input_image.bind_as_texture(shader, "input_tx");

    const Result &displacement = this->get_input("Displacement");
    displacement.bind_as_texture(shader, "displacement_tx");

    const Domain domain = this->compute_domain();
    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    displacement.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu()
  {
    const Result &image = this->get_input("Image");
    const Result &displacement = this->get_input("Displacement");

    const Interpolation interpolation = this->get_interpolation();
    const ExtensionMode extension_x = this->get_extension_mode_x();
    const ExtensionMode extension_y = this->get_extension_mode_y();
    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;

    if (interpolation == Interpolation::Anisotropic) {
      this->compute_anisotropic(size, image, output, displacement);
    }
    else {
      this->compute_interpolation(
          interpolation, size, image, output, displacement, extension_x, extension_y);
    }
  }

  void compute_interpolation(const Interpolation &interpolation,
                             const int2 &size,
                             const Result &image,
                             Result &output,
                             const Result &displacement,
                             const ExtensionMode &extension_mode_x,
                             const ExtensionMode &extension_mode_y) const
  {
    parallel_for(size, [&](const int2 base_texel) {
      const float2 coordinates = this->compute_coordinates(base_texel, size, displacement);
      output.store_pixel(
          base_texel,
          image.sample<Color>(coordinates, interpolation, extension_mode_x, extension_mode_y));
    });
  }

  /* In order to perform EWA sampling, we need to compute the partial derivative of the
   * displaced coordinates along the x and y directions using a finite difference
   * approximation. But in order to avoid loading multiple neighboring displacement values for
   * each pixel, we operate on the image in 2x2 blocks of pixels, where the derivatives are
   * computed horizontally and vertically across the 2x2 block such that odd texels use a
   * forward finite difference equation while even invocations use a backward finite difference
   * equation. */
  void compute_anisotropic(const int2 &size,
                           const Result &image,
                           Result &output,
                           const Result &displacement) const
  {
    parallel_for(math::divide_ceil(size, int2(2)), [&](const int2 base_texel) {
      /* Compute each of the pixels in the 2x2 block, making sure to exempt out of bounds right
       * and upper pixels. */
      const int x = base_texel.x * 2;
      const int y = base_texel.y * 2;

      const int2 lower_left_texel = int2(x, y);
      const int2 lower_right_texel = int2(x + 1, y);
      const int2 upper_left_texel = int2(x, y + 1);
      const int2 upper_right_texel = int2(x + 1, y + 1);

      const float2 lower_left_coordinates = this->compute_coordinates(
          lower_left_texel, size, displacement);
      const float2 lower_right_coordinates = this->compute_coordinates(
          lower_right_texel, size, displacement);
      const float2 upper_left_coordinates = this->compute_coordinates(
          upper_left_texel, size, displacement);
      const float2 upper_right_coordinates = this->compute_coordinates(
          upper_right_texel, size, displacement);

      /* Compute the partial derivatives using finite difference. Divide by the input size since
       * sample_ewa_zero assumes derivatives with respect to texel coordinates. */
      const float2 lower_x_gradient = (lower_right_coordinates - lower_left_coordinates) / size.x;
      const float2 left_y_gradient = (upper_left_coordinates - lower_left_coordinates) / size.y;
      const float2 right_y_gradient = (upper_right_coordinates - lower_right_coordinates) / size.y;
      const float2 upper_x_gradient = (upper_right_coordinates - upper_left_coordinates) / size.x;

      /* Computes one of the 2x2 pixels given its texel location, coordinates, and gradients. */
      auto compute_anisotropic_pixel = [&](const int2 &texel,
                                           const float2 &coordinates,
                                           const float2 &x_gradient,
                                           const float2 &y_gradient) {
        /* Sample the input using the displaced coordinates passing in the computed gradients in
         * order to utilize the anisotropic filtering capabilities of the sampler. */
        output.store_pixel(texel,
                           Color(image.sample_ewa_zero(coordinates, x_gradient, y_gradient)));
      };

      compute_anisotropic_pixel(
          lower_left_texel, lower_left_coordinates, lower_x_gradient, left_y_gradient);
      if (lower_right_texel.x != size.x) {
        compute_anisotropic_pixel(
            lower_right_texel, lower_right_coordinates, lower_x_gradient, right_y_gradient);
      }
      if (upper_left_texel.y != size.y) {
        compute_anisotropic_pixel(
            upper_left_texel, upper_left_coordinates, upper_x_gradient, left_y_gradient);
      }
      if (upper_right_texel.x != size.x && upper_right_texel.y != size.y) {
        compute_anisotropic_pixel(
            upper_right_texel, upper_right_coordinates, upper_x_gradient, right_y_gradient);
      }
    });
  }

  float2 compute_coordinates(const int2 &texel, const int2 &size, const Result &displacement) const
  {
    /* Note that the input displacement is in pixel space, so divide by the input size to
     * transform it into the normalized sampler space. */
    const float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);
    return coordinates - displacement.load_pixel_extended<float2>(texel) / float2(size);
  }

  const char *get_shader_name(const Interpolation &interpolation) const
  {
    switch (interpolation) {
      case Interpolation::Anisotropic:
        return "compositor_displace_anisotropic";
      case Interpolation::Bicubic:
        return "compositor_displace_bicubic";
      case Interpolation::Bilinear:
      case Interpolation::Nearest:
        return "compositor_displace";
    }

    return "compositor_displace";
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
  return new DisplaceOperation(context, node);
}

}  // namespace blender::nodes::node_composite_displace_cc

static void register_node_type_cmp_displace()
{
  namespace file_ns = blender::nodes::node_composite_displace_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDisplace", CMP_NODE_DISPLACE);
  ntype.ui_name = "Displace";
  ntype.ui_description = "Displace pixel position using an offset vector";
  ntype.enum_name_legacy = "DISPLACE";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_displace_declare;
  ntype.initfunc = file_ns::cmp_node_init_displace;
  blender::bke::node_type_storage(
      ntype, "NodeDisplaceData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_displace)
