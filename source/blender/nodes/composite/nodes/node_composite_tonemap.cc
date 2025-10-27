/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_types.hh"

#include "IMB_colormanagement.hh"

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_tonemap_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_TONE_MAP_PHOTORECEPTOR,
     "RD_PHOTORECEPTOR",
     0,
     N_("R/D Photoreceptor"),
     N_("More advanced algorithm based on eye physiology, by Reinhard and Devlin")},
    {CMP_NODE_TONE_MAP_SIMPLE,
     "RH_SIMPLE",
     0,
     N_("Rh Simple"),
     N_("Simpler photographic algorithm by Reinhard")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_tonemap_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_TONE_MAP_PHOTORECEPTOR)
      .static_items(type_items)
      .optional_label();

  b.add_input<decl::Float>("Key")
      .default_value(0.18f)
      .min(0.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_SIMPLE)
      .description(
          "The luminance that will be mapped to the log average luminance, typically set to the "
          "middle gray value");
  b.add_input<decl::Float>("Balance")
      .default_value(1.0f)
      .min(0.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_SIMPLE)
      .description(
          "Balances low and high luminance areas. Lower values emphasize details in shadows, "
          "while higher values compress highlights more smoothly");
  b.add_input<decl::Float>("Gamma")
      .default_value(1.0f)
      .min(0.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_SIMPLE)
      .description("Gamma correction factor applied after tone mapping");

  b.add_input<decl::Float>("Intensity")
      .default_value(0.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_PHOTORECEPTOR)
      .description(
          "Controls the intensity of the image, lower values makes it darker while higher values "
          "makes it lighter");
  b.add_input<decl::Float>("Contrast")
      .default_value(0.0f)
      .min(0.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_PHOTORECEPTOR)
      .description(
          "Controls the contrast of the image. Zero automatically sets the contrast based on its "
          "global range for better luminance distribution");
  b.add_input<decl::Float>("Light Adaptation")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_PHOTORECEPTOR)
      .description(
          "Specifies if tone mapping operates on the entire image or per pixel, 0 means the "
          "entire image, 1 means it is per pixel, and values in between blends between both");
  b.add_input<decl::Float>("Chromatic Adaptation")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .usage_by_single_menu(CMP_NODE_TONE_MAP_PHOTORECEPTOR)
      .description(
          "Specifies if tone mapping operates on the luminance or on each channel independently, "
          "0 means it uses luminance, 1 means it is per channel, and values in between blends "
          "between both");
}

static void node_composit_init_tonemap(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, but still allocated for forward compatibility. */
  NodeTonemap *ntm = MEM_callocN<NodeTonemap>(__func__);
  node->storage = ntm;
}

using namespace blender::compositor;

class ToneMapOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input_image = this->get_input("Image");
    Result &output_image = this->get_result("Image");
    if (input_image.is_single_value()) {
      output_image.share_data(input_image);
      return;
    }

    switch (get_type()) {
      case CMP_NODE_TONE_MAP_SIMPLE:
        execute_simple();
        return;
      case CMP_NODE_TONE_MAP_PHOTORECEPTOR:
        execute_photoreceptor();
        return;
    }

    output_image.share_data(input_image);
  }

  /* Tone mapping based on equation (3) from Reinhard, Erik, et al. "Photographic tone reproduction
   * for digital images." Proceedings of the 29th annual conference on Computer graphics and
   * interactive techniques. 2002. */
  void execute_simple()
  {
    if (this->context().use_gpu()) {
      execute_simple_gpu();
    }
    else {
      execute_simple_cpu();
    }
  }

  void execute_simple_gpu()
  {
    const float luminance_scale = compute_luminance_scale();
    const float luminance_scale_blend_factor = compute_luminance_scale_blend_factor();
    const float gamma = this->get_gamma();
    const float inverse_gamma = gamma != 0.0f ? 1.0f / gamma : 0.0f;

    gpu::Shader *shader = context().get_shader("compositor_tone_map_simple");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "luminance_scale", luminance_scale);
    GPU_shader_uniform_1f(shader, "luminance_scale_blend_factor", luminance_scale_blend_factor);
    GPU_shader_uniform_1f(shader, "inverse_gamma", inverse_gamma);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }

  void execute_simple_cpu()
  {
    const float luminance_scale = compute_luminance_scale();
    const float luminance_scale_blend_factor = compute_luminance_scale_blend_factor();
    const float gamma = this->get_gamma();
    const float inverse_gamma = gamma != 0.0f ? 1.0f / gamma : 0.0f;

    const Result &image = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 input_color = float4(image.load_pixel<Color>(texel));

      /* Equation (2) from Reinhard's 2002 paper. */
      float4 scaled_color = input_color * luminance_scale;

      /* Equation (3) from Reinhard's 2002 paper, but with the 1 replaced with the blend factor for
       * more flexibility. See ToneMapOperation::compute_luminance_scale_blend_factor. */
      float4 denominator = luminance_scale_blend_factor + scaled_color;
      float4 tone_mapped_color = math::safe_divide(scaled_color, denominator);

      if (inverse_gamma != 0.0f) {
        tone_mapped_color = math::pow(math::max(tone_mapped_color, float4(0.0f)), inverse_gamma);
      }

      output.store_pixel(texel, Color(float4(tone_mapped_color.xyz(), input_color.w)));
    });
  }

  /* Computes the scaling factor in equation (2) from Reinhard's 2002 paper. */
  float compute_luminance_scale()
  {
    const float geometric_mean = compute_geometric_mean_of_luminance();
    return geometric_mean != 0.0 ? this->get_key() / geometric_mean : 0.0f;
  }

  /* Computes equation (1) from Reinhard's 2002 paper. However, note that the equation in the paper
   * is most likely wrong, and the intention is actually to compute the geometric mean through a
   * logscale arithmetic mean, that is, the division should happen inside the exponential function,
   * not outside of it. That's because the sum of the log luminance will be a very large negative
   * number, whose exponential will almost always be zero, which is unexpected and useless. */
  float compute_geometric_mean_of_luminance()
  {
    return std::exp(compute_average_log_luminance());
  }

  float get_key()
  {
    return math::max(0.0f, this->get_input("Key").get_single_value_default(0.18f));
  }

  /* Equation (3) from Reinhard's 2002 paper blends between high luminance scaling for high
   * luminance values and low luminance scaling for low luminance values. This is done by adding 1
   * to the denominator, since for low luminance values, the denominator will be close to 1 and for
   * high luminance values, the 1 in the denominator will be relatively insignificant. But the
   * response of such function is not always ideal, so in this implementation, the 1 was exposed as
   * a parameter to the user for more flexibility. */
  float compute_luminance_scale_blend_factor()
  {
    return math::max(0.0f, this->get_input("Balance").get_single_value_default(1.0f));
  }

  float get_gamma()
  {
    return math::max(0.0f, this->get_input("Gamma").get_single_value_default(1.0f));
  }

  /* Tone mapping based on equation (1) and the trilinear interpolation between equations (6) and
   * (7) from Reinhard, Erik, and Kate Devlin. "Dynamic range reduction inspired by photoreceptor
   * physiology." IEEE transactions on visualization and computer graphics 11.1 (2005): 13-24. */
  void execute_photoreceptor()
  {
    if (this->context().use_gpu()) {
      execute_photoreceptor_gpu();
    }
    else {
      execute_photoreceptor_cpu();
    }
  }

  void execute_photoreceptor_gpu()
  {
    const float4 global_adaptation_level = compute_global_adaptation_level();
    const float contrast = compute_contrast();
    const float intensity = compute_intensity();
    const float chromatic_adaptation = get_chromatic_adaptation();
    const float light_adaptation = get_light_adaptation();

    gpu::Shader *shader = context().get_shader("compositor_tone_map_photoreceptor");
    GPU_shader_bind(shader);

    GPU_shader_uniform_4fv(shader, "global_adaptation_level", global_adaptation_level);
    GPU_shader_uniform_1f(shader, "contrast", contrast);
    GPU_shader_uniform_1f(shader, "intensity", intensity);
    GPU_shader_uniform_1f(shader, "chromatic_adaptation", chromatic_adaptation);
    GPU_shader_uniform_1f(shader, "light_adaptation", light_adaptation);

    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    GPU_shader_uniform_3fv(shader, "luminance_coefficients", luminance_coefficients);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
  }

  void execute_photoreceptor_cpu()
  {
    const float4 global_adaptation_level = compute_global_adaptation_level();
    const float contrast = compute_contrast();
    const float intensity = compute_intensity();
    const float chromatic_adaptation = get_chromatic_adaptation();
    const float light_adaptation = get_light_adaptation();

    float3 luminance_coefficients;
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);

    const Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      float4 input_color = float4(input.load_pixel<Color>(texel));
      float input_luminance = math::dot(input_color.xyz(), luminance_coefficients);

      /* Trilinear interpolation between equations (6) and (7) from Reinhard's 2005 paper. */
      float4 local_adaptation_level = math::interpolate(
          float4(input_luminance), input_color, chromatic_adaptation);
      float4 adaptation_level = math::interpolate(
          global_adaptation_level, local_adaptation_level, light_adaptation);

      /* Equation (1) from Reinhard's 2005 paper, assuming `Vmax` is 1. */
      float4 semi_saturation = math::pow(intensity * adaptation_level, contrast);
      float4 tone_mapped_color = math::safe_divide(input_color, input_color + semi_saturation);

      output.store_pixel(texel, Color(float4(tone_mapped_color.xyz(), input_color.w)));
    });
  }

  /* Computes the global adaptation level from the trilinear interpolation equations constructed
   * from equations (6) and (7) in Reinhard's 2005 paper. */
  float4 compute_global_adaptation_level()
  {
    const float4 average_color = compute_average_color();
    const float average_luminance = compute_average_luminance();
    const float chromatic_adaptation = get_chromatic_adaptation();
    return math::interpolate(float4(average_luminance), average_color, chromatic_adaptation);
  }

  float4 compute_average_color()
  {
    /* The average color will reduce to zero if chromatic adaptation is zero, so just return zero
     * in this case to avoid needlessly computing the average. See the trilinear interpolation
     * equations constructed from equations (6) and (7) in Reinhard's 2005 paper. */
    if (get_chromatic_adaptation() == 0.0f) {
      return float4(0.0f);
    }

    const Result &input = get_input("Image");
    return sum_color(context(), input) / (input.domain().size.x * input.domain().size.y);
  }

  float compute_average_luminance()
  {
    /* The average luminance will reduce to zero if chromatic adaptation is one, so just return
     * zero in this case to avoid needlessly computing the average. See the trilinear interpolation
     * equations constructed from equations (6) and (7) in Reinhard's 2005 paper. */
    if (get_chromatic_adaptation() == 1.0f) {
      return 0.0f;
    }

    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    const Result &input = get_input("Image");
    float sum = sum_luminance(context(), input, luminance_coefficients);
    return sum / (input.domain().size.x * input.domain().size.y);
  }

  /* Computes equation (5) from Reinhard's 2005 paper. */
  float compute_intensity()
  {
    return std::exp(-this->get_intensity());
  }

  /* If the contrast is not zero, return it, otherwise, a zero contrast denote automatic derivation
   * of the contrast value based on equations (2) and (4) from Reinhard's 2005 paper. */
  float compute_contrast()
  {
    if (this->get_contrast() != 0.0f) {
      return this->get_contrast();
    }

    const float log_maximum_luminance = compute_log_maximum_luminance();
    const float log_minimum_luminance = compute_log_minimum_luminance();

    /* This is merely to guard against zero division later. */
    if (log_maximum_luminance == log_minimum_luminance) {
      return 1.0f;
    }

    const float average_log_luminance = compute_average_log_luminance();
    const float dynamic_range = log_maximum_luminance - log_minimum_luminance;
    const float luminance_key = (log_maximum_luminance - average_log_luminance) / (dynamic_range);

    return 0.3f + 0.7f * std::pow(luminance_key, 1.4f);
  }

  float compute_average_log_luminance()
  {
    const Result &input_image = get_input("Image");

    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    const float sum_of_log_luminance = sum_log_luminance(
        context(), input_image, luminance_coefficients);

    return sum_of_log_luminance / (input_image.domain().size.x * input_image.domain().size.y);
  }

  float compute_log_maximum_luminance()
  {
    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    const float maximum = maximum_luminance(context(), get_input("Image"), luminance_coefficients);
    return std::log(math::max(maximum, 1e-5f));
  }

  float compute_log_minimum_luminance()
  {
    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    const float minimum = minimum_luminance(context(), get_input("Image"), luminance_coefficients);
    return std::log(math::max(minimum, 1e-5f));
  }

  float get_intensity()
  {
    return this->get_input("Intensity").get_single_value_default(0.0f);
  }

  float get_contrast()
  {
    return math::max(0.0f, this->get_input("Contrast").get_single_value_default(0.0f));
  }

  float get_chromatic_adaptation()
  {
    return math::clamp(
        this->get_input("Chromatic Adaptation").get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  float get_light_adaptation()
  {
    return math::clamp(
        this->get_input("Light Adaptation").get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  CMPNodeToneMapType get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_TONE_MAP_PHOTORECEPTOR);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeToneMapType>(menu_value.value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ToneMapOperation(context, node);
}

}  // namespace blender::nodes::node_composite_tonemap_cc

static void register_node_type_cmp_tonemap()
{
  namespace file_ns = blender::nodes::node_composite_tonemap_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTonemap", CMP_NODE_TONEMAP);
  ntype.ui_name = "Tonemap";
  ntype.ui_description =
      "Map one set of colors to another in order to approximate the appearance of high dynamic "
      "range";
  ntype.enum_name_legacy = "TONEMAP";
  ntype.nclass = NODE_CLASS_OP_COLOR;
  ntype.declare = file_ns::cmp_node_tonemap_declare;
  ntype.initfunc = file_ns::node_composit_init_tonemap;
  blender::bke::node_type_storage(
      ntype, "NodeTonemap", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_tonemap)
