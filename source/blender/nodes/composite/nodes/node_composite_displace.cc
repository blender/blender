/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "MEM_guardedalloc.h"

#include "BKE_node.hh"

#include "BLI_assert.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"
#include "RNA_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_domain.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "node_composite_util.hh"

/* **************** Displace  ******************** */

namespace blender::nodes::node_composite_displace_cc {

NODE_STORAGE_FUNCS(NodeDisplaceData)

static void cmp_node_displace_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Vector>("Vector")
      .dimensions(2)
      .default_value({1.0f, 1.0f})
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_TRANSLATION)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("X Scale")
      .default_value(0.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("Y Scale")
      .default_value(0.0f)
      .min(-1000.0f)
      .max(1000.0f)
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

static void cmp_node_init_displace(bNodeTree * /*ntree*/, bNode *node)
{
  NodeDisplaceData *data = MEM_callocN<NodeDisplaceData>(__func__);
  data->interpolation = CMP_NODE_INTERPOLATION_ANISOTROPIC;
  node->storage = data;
}

static void cmp_buts_displace(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "interpolation", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::compositor;

class DisplaceOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (this->is_identity()) {
      const Result &input = this->get_input("Image");
      Result &output = this->get_result("Image");
      output.share_data(input);
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
    GPUShader *shader = context().get_shader(this->get_shader_name(interpolation));
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
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Result &input_displacement = get_input("Vector");
    input_displacement.bind_as_texture(shader, "displacement_tx");
    const Result &input_x_scale = get_input("X Scale");
    input_x_scale.bind_as_texture(shader, "x_scale_tx");
    const Result &input_y_scale = get_input("Y Scale");
    input_y_scale.bind_as_texture(shader, "y_scale_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    input_displacement.unbind_as_texture();
    input_x_scale.unbind_as_texture();
    input_y_scale.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_cpu()
  {
    const Result &image = get_input("Image");
    const Result &input_displacement = get_input("Vector");
    const Result &x_scale = get_input("X Scale");
    const Result &y_scale = get_input("Y Scale");

    const Interpolation interpolation = this->get_interpolation();
    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;

    if (interpolation == Interpolation::Anisotropic) {
      this->compute_anisotropic(size, image, output, input_displacement, x_scale, y_scale);
    }
    else {
      this->compute_interpolation(
          interpolation, size, image, output, input_displacement, x_scale, y_scale);
    }
  }

  void compute_interpolation(const Interpolation &interpolation,
                             const int2 &size,
                             const Result &image,
                             Result &output,
                             const Result &input_displacement,
                             const Result &x_scale,
                             const Result &y_scale) const
  {
    parallel_for(size, [&](const int2 base_texel) {
      const float2 coordinates = compute_coordinates(
          base_texel, size, input_displacement, x_scale, y_scale);
      switch (interpolation) {
        /* The anisotropic case requires gradient computation and is handled separately. */
        case Interpolation::Anisotropic:
          BLI_assert_unreachable();
          break;
        case Interpolation::Nearest:
          output.store_pixel(base_texel, image.sample_nearest_zero(coordinates));
          break;
        case Interpolation::Bilinear:
          output.store_pixel(base_texel, image.sample_bilinear_zero(coordinates));
          break;
        case Interpolation::Bicubic:
          output.store_pixel(base_texel, image.sample_cubic_wrap(coordinates, false, false));
          break;
      }
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
                           const Result &input_displacement,
                           const Result &x_scale,
                           const Result &y_scale) const
  {
    auto compute_anisotropic_pixel = [&](const int2 &texel,
                                         const float2 &coordinates,
                                         const float2 &x_gradient,
                                         const float2 &y_gradient) {
      /* Sample the input using the displaced coordinates passing in the computed gradients in
       * order to utilize the anisotropic filtering capabilities of the sampler. */
      output.store_pixel(texel, image.sample_ewa_zero(coordinates, x_gradient, y_gradient));
    };
    parallel_for(math::divide_ceil(size, int2(2)), [&](const int2 base_texel) {
      /* Compute each of the pixels in the 2x2 block, making sure to exempt out of bounds right
       * and upper pixels. */
      const int x = base_texel.x * 2;
      const int y = base_texel.y * 2;

      const int2 lower_left_texel = int2(x, y);
      const int2 lower_right_texel = int2(x + 1, y);
      const int2 upper_left_texel = int2(x, y + 1);
      const int2 upper_right_texel = int2(x + 1, y + 1);

      const float2 lower_left_coordinates = compute_coordinates(
          lower_left_texel, size, input_displacement, x_scale, y_scale);
      const float2 lower_right_coordinates = compute_coordinates(
          lower_right_texel, size, input_displacement, x_scale, y_scale);
      const float2 upper_left_coordinates = compute_coordinates(
          upper_left_texel, size, input_displacement, x_scale, y_scale);
      const float2 upper_right_coordinates = compute_coordinates(
          upper_right_texel, size, input_displacement, x_scale, y_scale);

      /* Compute the partial derivatives using finite difference. Divide by the input size since
       * sample_ewa_zero assumes derivatives with respect to texel coordinates. */
      const float2 lower_x_gradient = (lower_right_coordinates - lower_left_coordinates) / size.x;
      const float2 left_y_gradient = (upper_left_coordinates - lower_left_coordinates) / size.y;
      const float2 right_y_gradient = (upper_right_coordinates - lower_right_coordinates) / size.y;
      const float2 upper_x_gradient = (upper_right_coordinates - upper_left_coordinates) / size.x;

      /* Computes one of the 2x2 pixels given its texel location, coordinates, and gradients. */

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

  float2 compute_coordinates(const int2 &texel,
                             const int2 &size,
                             const Result &input_displacement,
                             const Result &x_scale,
                             const Result &y_scale) const
  {
    /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image
     * size to get the coordinates into the sampler's expected [0, 1] range. */
    float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

    /* Note that the input displacement is in pixel space, so divide by the input size to
     * transform it into the normalized sampler space. */
    float2 scale = float2(x_scale.load_pixel_extended<float, true>(texel),
                          y_scale.load_pixel_extended<float, true>(texel));
    float2 displacement = input_displacement.load_pixel_extended<float2, true>(texel) * scale /
                          float2(size);
    return coordinates - displacement;
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
    BLI_assert_unreachable();
    return "compositor_displace";
  }

  Interpolation get_interpolation() const
  {
    switch (node_storage(bnode()).interpolation) {
      case CMP_NODE_INTERPOLATION_ANISOTROPIC:
        return Interpolation::Anisotropic;
      case CMP_NODE_INTERPOLATION_NEAREST:
        return Interpolation::Nearest;
      case CMP_NODE_INTERPOLATION_BILINEAR:
        return Interpolation::Bilinear;
      case CMP_NODE_INTERPOLATION_BICUBIC:
        return Interpolation::Bicubic;
    }

    BLI_assert_unreachable();
    return Interpolation::Nearest;
  }

  bool is_identity()
  {
    const Result &input_image = get_input("Image");
    if (input_image.is_single_value()) {
      return true;
    }

    const Result &input_displacement = get_input("Vector");
    if (input_displacement.is_single_value() &&
        math::is_zero(input_displacement.get_single_value<float2>()))
    {
      return true;
    }

    const Result &input_x_scale = get_input("X Scale");
    const Result &input_y_scale = get_input("Y Scale");
    if (input_x_scale.is_single_value() && input_x_scale.get_single_value<float>() == 0.0f &&
        input_y_scale.is_single_value() && input_y_scale.get_single_value<float>() == 0.0f)
    {
      return true;
    }

    return false;
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
  ntype.draw_buttons = file_ns::cmp_buts_displace;
  ntype.initfunc = file_ns::cmp_node_init_displace;
  blender::bke::node_type_storage(
      ntype, "NodeDisplaceData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_displace)
