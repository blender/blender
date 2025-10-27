/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_types.hh"

#include "UI_interface_layout.hh"

#include "GPU_shader.hh"

#include "COM_algorithm_pad.hh"
#include "COM_algorithm_parallel_reduction.hh"
#include "COM_algorithm_recursive_gaussian_blur.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_symmetric_blur_weights.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_blur_cc {

static const EnumPropertyItem type_items[] = {
    {R_FILTER_BOX, "FLAT", 0, N_("Flat"), ""},
    {R_FILTER_TENT, "TENT", 0, N_("Tent"), ""},
    {R_FILTER_QUAD, "QUAD", 0, N_("Quadratic"), ""},
    {R_FILTER_CUBIC, "CUBIC", 0, N_("Cubic"), ""},
    {R_FILTER_GAUSS, "GAUSS", 0, N_("Gaussian"), ""},
    {R_FILTER_FAST_GAUSS, "FAST_GAUSS", 0, N_("Fast Gaussian"), ""},
    {R_FILTER_CATROM, "CATROM", 0, N_("Catrom"), ""},
    {R_FILTER_MITCH, "MITCH", 0, N_("Mitch"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_blur_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Vector>("Size")
      .dimensions(2)
      .default_value({0.0f, 0.0f})
      .min(0.0f)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Menu>("Type")
      .default_value(R_FILTER_GAUSS)
      .static_items(type_items)
      .optional_label();
  b.add_input<decl::Bool>("Extend Bounds").default_value(false);
  b.add_input<decl::Bool>("Separable")
      .default_value(true)
      .description(
          "Use faster approximation by blurring along the horizontal and vertical directions "
          "independently");
}

static void node_composit_init_blur(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, but allocated for forward compatibility. */
  NodeBlurData *data = MEM_callocN<NodeBlurData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class BlurOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    Result &output = this->get_result("Image");
    if (this->is_identity()) {
      output.share_data(input);
      return;
    }

    const Result &size = this->get_input("Size");
    if (this->get_extend_bounds()) {
      Result padded_input = this->context().create_result(ResultType::Color);
      Result padded_size = this->context().create_result(ResultType::Float2);

      const int2 padding_size = this->compute_extended_boundary_size(size);

      pad(this->context(), input, padded_input, padding_size, PaddingMethod::Zero);
      pad(this->context(), size, padded_size, padding_size, PaddingMethod::Extend);

      this->execute_blur(padded_input, padded_size);
      padded_input.release();
      padded_size.release();
    }
    else {
      this->execute_blur(input, size);
    }
  }

  /* Computes the number of pixels that the image should be extended by if Extend Bounds is
   * enabled. */
  int2 compute_extended_boundary_size(const Result &size)
  {
    BLI_assert(this->get_extend_bounds());

    /* For constant sized blur, the extension should just be the blur radius. */
    if (size.is_single_value()) {
      return int2(math::ceil(this->get_blur_size()));
    }

    /* For variable sized blur, the extension should be the maximum size. */
    return int2(math::ceil(this->compute_maximum_blur_size()));
  }

  void execute_blur(const Result &input, const Result &size)
  {
    Result &output = this->get_result("Image");
    if (!size.is_single_value()) {
      this->execute_variable_size(input, size, output);
    }
    else if (this->get_type() == R_FILTER_FAST_GAUSS) {
      recursive_gaussian_blur(this->context(), input, output, this->get_blur_size());
    }
    else if (use_separable_filter()) {
      symmetric_separable_blur(
          this->context(), input, output, this->get_blur_size(), this->get_type());
    }
    else {
      this->execute_constant_size(input, output);
    }
  }

  void execute_constant_size(const Result &input, Result &output)
  {
    if (this->context().use_gpu()) {
      this->execute_constant_size_gpu(input, output);
    }
    else {
      this->execute_constant_size_cpu(input, output);
    }
  }

  void execute_constant_size_gpu(const Result &input, Result &output)
  {
    gpu::Shader *shader = context().get_shader("compositor_symmetric_blur");
    GPU_shader_bind(shader);

    input.bind_as_texture(shader, "input_tx");

    const float2 blur_radius = this->get_blur_size();

    const Result &weights = context().cache_manager().symmetric_blur_weights.get(
        context(), this->get_type(), blur_radius);
    weights.bind_as_texture(shader, "weights_tx");

    const Domain domain = input.domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output.unbind_as_image();
    input.unbind_as_texture();
    weights.unbind_as_texture();
  }

  void execute_constant_size_cpu(const Result &input, Result &output)
  {
    const float2 blur_radius = this->get_blur_size();
    const Result &weights = this->context().cache_manager().symmetric_blur_weights.get(
        this->context(), this->get_type(), blur_radius);

    const Domain domain = input.domain();
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 accumulated_color = float4(0.0f);

      /* First, compute the contribution of the center pixel. */
      float4 center_color = float4(input.load_pixel_extended<Color>(texel));
      accumulated_color += center_color * weights.load_pixel<float>(int2(0));

      int2 weights_size = weights.domain().size;

      /* Then, compute the contributions of the pixels along the x axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int x = 1; x < weights_size.x; x++) {
        float weight = weights.load_pixel<float>(int2(x, 0));
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, 0))) * weight;
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-x, 0))) *
                             weight;
      }

      /* Then, compute the contributions of the pixels along the y axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int y = 1; y < weights_size.y; y++) {
        float weight = weights.load_pixel<float>(int2(0, y));
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(0, y))) * weight;
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(0, -y))) *
                             weight;
      }

      /* Finally, compute the contributions of the pixels in the four quadrants of the filter,
       * noting that the weights texture only stores the weights for the upper right quadrant, but
       * since the filter is symmetric, the same weight is used for the rest of the quadrants and
       * we add all four of their contributions. */
      for (int y = 1; y < weights_size.y; y++) {
        for (int x = 1; x < weights_size.x; x++) {
          float weight = weights.load_pixel<float>(int2(x, y));
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, y))) *
                               weight;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-x, y))) *
                               weight;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, -y))) *
                               weight;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-x, -y))) *
                               weight;
        }
      }

      output.store_pixel(texel, Color(accumulated_color));
    });
  }

  void execute_variable_size(const Result &input, const Result &size, Result &output)
  {
    if (this->context().use_gpu()) {
      this->execute_variable_size_gpu(input, size, output);
    }
    else {
      this->execute_variable_size_cpu(input, size, output);
    }
  }

  void execute_variable_size_gpu(const Result &input, const Result &size_input, Result &output)
  {
    const float2 blur_radius = this->compute_maximum_blur_size();
    const Result &weights = context().cache_manager().symmetric_blur_weights.get(
        context(), this->get_type(), blur_radius);

    gpu::Shader *shader = context().get_shader("compositor_symmetric_blur_variable_size");
    GPU_shader_bind(shader);

    input.bind_as_texture(shader, "input_tx");
    weights.bind_as_texture(shader, "weights_tx");
    size_input.bind_as_texture(shader, "size_tx");

    const Domain domain = input.domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output.unbind_as_image();
    input.unbind_as_texture();
    weights.unbind_as_texture();
    size_input.unbind_as_texture();
  }

  void execute_variable_size_cpu(const Result &input, const Result &size_input, Result &output)
  {
    const float2 blur_radius = this->compute_maximum_blur_size();
    const Result &weights = this->context().cache_manager().symmetric_blur_weights.get(
        this->context(), this->get_type(), blur_radius);

    const Domain domain = input.domain();
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 accumulated_color = float4(0.0f);
      float4 accumulated_weight = float4(0.0f);

      const float2 size = math::max(float2(0.0f), size_input.load_pixel_extended<float2>(texel));
      int2 radius = int2(math::ceil(size));
      float2 coordinates_scale = float2(1.0f) / (size + float2(1.0f));

      /* First, compute the contribution of the center pixel. */
      float4 center_color = float4(input.load_pixel_extended<Color>(texel));
      float center_weight = weights.load_pixel<float>(int2(0));
      accumulated_color += center_color * center_weight;
      accumulated_weight += center_weight;

      /* Then, compute the contributions of the pixels along the x axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int x = 1; x <= radius.x; x++) {
        float weight_coordinates = (x + 0.5f) * coordinates_scale.x;
        float weight = weights.sample_bilinear_extended(float2(weight_coordinates, 0.0f)).x;
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, 0))) * weight;
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-x, 0))) *
                             weight;
        accumulated_weight += weight * 2.0f;
      }

      /* Then, compute the contributions of the pixels along the y axis of the filter, noting that
       * the weights texture only stores the weights for the positive half, but since the filter is
       * symmetric, the same weight is used for the negative half and we add both of their
       * contributions. */
      for (int y = 1; y <= radius.y; y++) {
        float weight_coordinates = (y + 0.5f) * coordinates_scale.y;
        float weight = weights.sample_bilinear_extended(float2(0.0f, weight_coordinates)).x;
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(0, y))) * weight;
        accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(0, -y))) *
                             weight;
        accumulated_weight += weight * 2.0f;
      }

      /* Finally, compute the contributions of the pixels in the four quadrants of the filter,
       * noting that the weights texture only stores the weights for the upper right quadrant, but
       * since the filter is symmetric, the same weight is used for the rest of the quadrants and
       * we add all four of their contributions. */
      for (int y = 1; y <= radius.y; y++) {
        for (int x = 1; x <= radius.x; x++) {
          float2 weight_coordinates = (float2(x, y) + float2(0.5f)) * coordinates_scale;
          float weight = weights.sample_bilinear_extended(weight_coordinates).x;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, y))) *
                               weight;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-x, y))) *
                               weight;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(x, -y))) *
                               weight;
          accumulated_color += float4(input.load_pixel_extended<Color>(texel + int2(-x, -y))) *
                               weight;
          accumulated_weight += weight * 4.0f;
        }
      }

      accumulated_color = math::safe_divide(accumulated_color, accumulated_weight);

      output.store_pixel(texel, Color(accumulated_color));
    });
  }

  float2 compute_maximum_blur_size()
  {
    return math::max(float2(0.0f), maximum_float2(this->context(), this->get_input("Size")));
  }

  bool is_identity()
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      return true;
    }

    const Result &size = this->get_input("Size");
    if (!size.is_single_value()) {
      return false;
    }

    if (this->get_blur_size() == float2(0.0)) {
      return true;
    }

    return false;
  }

  /* The blur node can operate with different filter types, evaluated on the normalized distance to
   * the center of the filter. Some of those filters are separable and can be computed as such. If
   * the Separable input is true, then the filter is always computed as separable even if it is not
   * in fact separable, in which case, the used filter is a cheaper approximation to the actual
   * filter. Otherwise, the filter is computed as separable if it is in fact separable and as a
   * normal 2D filter otherwise. */
  bool use_separable_filter()
  {
    if (this->get_separable()) {
      return true;
    }

    /* Only Gaussian filters are separable. The rest is not. */
    switch (this->get_type()) {
      case R_FILTER_GAUSS:
      case R_FILTER_FAST_GAUSS:
        return true;
      default:
        return false;
    }
  }

  float2 get_blur_size()
  {
    BLI_assert(this->get_input("Size").is_single_value());
    return math::max(float2(0.0f), this->get_input("Size").get_single_value<float2>());
  }

  bool get_separable()
  {
    return this->get_input("Separable").get_single_value_default(true);
  }

  bool get_extend_bounds()
  {
    return this->get_input("Extend Bounds").get_single_value_default(false);
  }

  int get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(R_FILTER_GAUSS);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return menu_value.value;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BlurOperation(context, node);
}

}  // namespace blender::nodes::node_composite_blur_cc

static void register_node_type_cmp_blur()
{
  namespace file_ns = blender::nodes::node_composite_blur_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeBlur", CMP_NODE_BLUR);
  ntype.ui_name = "Blur";
  ntype.ui_description = "Blur an image, using several blur modes";
  ntype.enum_name_legacy = "BLUR";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_blur_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc = file_ns::node_composit_init_blur;
  blender::bke::node_type_storage(
      ntype, "NodeBlurData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_blur)
