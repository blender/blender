/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_bounds.hh"
#include "BLI_bounds_types.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"

#include "BKE_node.hh"

#include "GPU_shader.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Crop  ******************** */

namespace blender::nodes::node_composite_crop_cc {

static void cmp_node_crop_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Int>("X")
      .default_value(0)
      .min(0)

      .description("The X position of the lower left corner of the crop region");
  b.add_input<decl::Int>("Y")
      .default_value(0)
      .min(0)

      .description("The Y position of the lower left corner of the crop region");
  b.add_input<decl::Int>("Width")
      .default_value(1920)
      .min(1)

      .description("The width of the crop region");
  b.add_input<decl::Int>("Height")
      .default_value(1080)
      .min(1)

      .description("The height of the crop region");
  b.add_input<decl::Bool>("Alpha Crop")
      .default_value(false)

      .description(
          "Sets the areas outside of the crop region to be transparent instead of actually "
          "cropping the size of the image");
}

using namespace blender::compositor;

class CropOperation : public NodeOperation {
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

    if (this->is_alpha_crop()) {
      this->execute_alpha_crop();
    }
    else {
      this->execute_image_crop();
    }
  }

  /* Crop by replacing areas outside of the cropping bounds with zero alpha. The output have the
   * same domain as the input image. */
  void execute_alpha_crop()
  {
    if (this->context().use_gpu()) {
      this->execute_alpha_crop_gpu();
    }
    else {
      this->execute_alpha_crop_cpu();
    }
  }

  void execute_alpha_crop_gpu()
  {
    gpu::Shader *shader = this->context().get_shader("compositor_alpha_crop");
    GPU_shader_bind(shader);

    const Bounds<int2> bounds = this->compute_cropping_bounds();
    GPU_shader_uniform_2iv(shader, "lower_bound", bounds.min);
    GPU_shader_uniform_2iv(shader, "upper_bound", bounds.max);

    const Result &input_image = this->get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = this->compute_domain();

    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_alpha_crop_cpu()
  {
    const Bounds<int2> bounds = this->compute_cropping_bounds();

    const Result &input = this->get_input("Image");

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      /* The lower bound is inclusive and upper bound is exclusive. */
      bool is_inside = texel.x >= bounds.min.x && texel.y >= bounds.min.y &&
                       texel.x < bounds.max.x && texel.y < bounds.max.y;
      /* Write the pixel color if it is inside the cropping region, otherwise, write zero. */
      float4 color = is_inside ? float4(input.load_pixel<Color>(texel)) : float4(0.0f);
      output.store_pixel(texel, Color(color));
    });
  }

  /* Crop the image into a new size that matches the cropping bounds. */
  void execute_image_crop()
  {
    if (this->context().use_gpu()) {
      this->execute_image_crop_gpu();
    }
    else {
      this->execute_image_crop_cpu();
    }
  }

  void execute_image_crop_gpu()
  {
    const Bounds<int2> bounds = this->compute_cropping_bounds();

    gpu::Shader *shader = this->context().get_shader("compositor_image_crop");
    GPU_shader_bind(shader);

    GPU_shader_uniform_2iv(shader, "lower_bound", bounds.min);

    const Result &input_image = this->get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const int2 size = bounds.size();

    Result &output_image = this->get_result("Image");
    output_image.allocate_texture(Domain(size, this->compute_domain().transformation));
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_image_crop_cpu()
  {
    const Bounds<int2> bounds = this->compute_cropping_bounds();

    const Result &input = this->get_input("Image");

    const int2 size = bounds.size();
    Result &output = this->get_result("Image");
    output.allocate_texture(Domain(size, this->compute_domain().transformation));

    parallel_for(size, [&](const int2 texel) {
      output.store_pixel(texel, input.load_pixel<Color>(texel + bounds.min));
    });
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    const Result &input = this->get_input("Image");
    /* Single value inputs can't be cropped and are returned as is. */
    if (input.is_single_value()) {
      return true;
    }

    const Bounds<int2> bounds = this->compute_cropping_bounds();
    const int2 input_size = input.domain().size;
    /* The cropping bounds cover the whole image, so no cropping happens. */
    if (bounds.min == int2(0) && bounds.max == input_size) {
      return true;
    }

    return false;
  }

  Bounds<int2> compute_cropping_bounds()
  {
    const int2 input_size = this->get_input("Image").domain().size;

    const int x = math::clamp(
        this->get_input("X").get_single_value_default(0), 0, input_size.x - 1);
    const int y = math::clamp(
        this->get_input("Y").get_single_value_default(0), 0, input_size.y - 1);
    const int width = math::max(1, this->get_input("Width").get_single_value_default(100));
    const int height = math::max(1, this->get_input("Height").get_single_value_default(100));

    const Bounds<int2> input_bounds = Bounds<int2>(int2(0), input_size);

    const Bounds<int2> crop_bounds = Bounds<int2>(int2(x, y), int2(x + width, y + height));
    return *bounds::intersect(crop_bounds, input_bounds);
  }

  /* If true, the region outside of the cropping bounds will be set to a zero alpha value instead
   * of actually cropping the size of the image. */
  bool is_alpha_crop()
  {
    return this->get_input("Alpha Crop").get_single_value_default(false);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CropOperation(context, node);
}

}  // namespace blender::nodes::node_composite_crop_cc

static void register_node_type_cmp_crop()
{
  namespace file_ns = blender::nodes::node_composite_crop_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeCrop", CMP_NODE_CROP);
  ntype.ui_name = "Crop";
  ntype.ui_description =
      "Crops image to a smaller region, either making the cropped area transparent or resizing "
      "the image";
  ntype.enum_name_legacy = "CROP";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_crop_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_crop)
