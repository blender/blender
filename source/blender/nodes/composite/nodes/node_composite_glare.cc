/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <limits>

#include "MEM_guardedalloc.h"

#if defined(WITH_FFTW3)
#  include <fftw3.h>
#endif

#include "BLI_array.hh"
#include "BLI_assert.h"
#include "BLI_fftw.hh"
#include "BLI_index_range.hh"
#include "BLI_math_angle_types.hh"
#include "BLI_math_base.hh"
#include "BLI_math_color.h"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_noise.hh"
#include "BLI_task.hh"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_algorithm_convolve.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"
#include "COM_utilities_diagonals.hh"

#include "node_composite_util.hh"

#define MAX_GLARE_ITERATIONS 5

namespace blender::nodes::node_composite_glare_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_GLARE_BLOOM, "BLOOM", 0, N_("Bloom"), ""},
    {CMP_NODE_GLARE_GHOST, "GHOSTS", 0, N_("Ghosts"), ""},
    {CMP_NODE_GLARE_STREAKS, "STREAKS", 0, N_("Streaks"), ""},
    {CMP_NODE_GLARE_FOG_GLOW, "FOG_GLOW", 0, N_("Fog Glow"), ""},
    {CMP_NODE_GLARE_SIMPLE_STAR, "SIMPLE_STAR", 0, N_("Simple Star"), ""},
    {CMP_NODE_GLARE_SUN_BEAMS, "SUN_BEAMS", 0, N_("Sun Beams"), ""},
    {CMP_NODE_GLARE_KERNEL, "KERNEL", 0, N_("Kernel"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem quality_items[] = {
    {CMP_NODE_GLARE_QUALITY_HIGH, "HIGH", 0, N_("High"), ""},
    {CMP_NODE_GLARE_QUALITY_MEDIUM, "MEDIUM", 0, N_("Medium"), ""},
    {CMP_NODE_GLARE_QUALITY_LOW, "LOW", 0, N_("Low"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class KernelDataType : uint8_t {
  Float = 0,
  Color = 1,
};

static const EnumPropertyItem kernel_data_type_items[] = {
    {int(KernelDataType::Float),
     "FLOAT",
     0,
     N_("Float"),
     N_("The kernel is a float and will be convolved with all input channels")},
    {int(KernelDataType::Color),
     "COLOR",
     0,
     N_("Color"),
     N_("The kernel is a color and each channel of the kernel will be convolved with each "
        "respective channel in the input")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_glare_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image")
      .structure_type(StructureType::Dynamic)
      .description("The image with the generated glare added")
      .align_with_previous();

  b.add_output<decl::Color>("Glare")
      .structure_type(StructureType::Dynamic)
      .description("The generated glare");
  b.add_output<decl::Color>("Highlights")
      .structure_type(StructureType::Dynamic)
      .description("The extracted highlights from which the glare was generated");

  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_GLARE_STREAKS)
      .static_items(type_items)
      .optional_label();
  b.add_input<decl::Menu>("Quality")
      .default_value(CMP_NODE_GLARE_QUALITY_MEDIUM)
      .static_items(quality_items)
      .optional_label();

  PanelDeclarationBuilder &highlights_panel = b.add_panel("Highlights").default_closed(true);
  highlights_panel.add_input<decl::Float>("Threshold", "Highlights Threshold")
      .default_value(1.0f)
      .min(0.0f)
      .description(
          "The brightness level at which pixels are considered part of the highlights that "
          "produce a glare");
  highlights_panel.add_input<decl::Float>("Smoothness", "Highlights Smoothness")
      .default_value(0.1f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("The smoothness of the extracted highlights");

  PanelDeclarationBuilder &supress_highlights_panel =
      highlights_panel.add_panel("Clamp").default_closed(true);
  supress_highlights_panel.add_input<decl::Bool>("Clamp", "Clamp Highlights")
      .default_value(false)
      .panel_toggle()
      .description("Clamp bright highlights");
  supress_highlights_panel.add_input<decl::Float>("Maximum", "Maximum Highlights")
      .default_value(10.0f)
      .min(0.0f)
      .description(
          "Clamp bright highlights such that their brightness are not larger than this value");

  PanelDeclarationBuilder &mix_panel = b.add_panel("Adjust");
  mix_panel.add_input<decl::Float>("Strength")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Adjusts the brightness of the glare");
  mix_panel.add_input<decl::Float>("Saturation")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description("Adjusts the saturation of the glare");
  mix_panel.add_input<decl::Color>("Tint")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .description("Tints the glare. Consider desaturating the glare to more accurate tinting");

  PanelDeclarationBuilder &glare_panel = b.add_panel("Glare");
  glare_panel.add_input<decl::Float>("Size")
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type",
                     {CMP_NODE_GLARE_FOG_GLOW, CMP_NODE_GLARE_BLOOM, CMP_NODE_GLARE_SUN_BEAMS})
      .description(
          "The size of the glare relative to the image. 1 means the glare covers the entire "
          "image, 0.5 means the glare covers half the image, and so on");
  glare_panel.add_input<decl::Int>("Streaks")
      .default_value(4)
      .min(1)
      .max(16)
      .usage_by_menu("Type", CMP_NODE_GLARE_STREAKS)
      .description("The number of streaks");
  glare_panel.add_input<decl::Float>("Streaks Angle")
      .default_value(0.0f)
      .subtype(PROP_ANGLE)
      .usage_by_menu("Type", CMP_NODE_GLARE_STREAKS)
      .description("The angle that the first streak makes with the horizontal axis");
  glare_panel.add_input<decl::Int>("Iterations")
      .default_value(3)
      .min(2)
      .max(5)
      .usage_by_menu("Type",
                     {CMP_NODE_GLARE_SIMPLE_STAR, CMP_NODE_GLARE_GHOST, CMP_NODE_GLARE_STREAKS})
      .description(
          "The number of ghosts for Ghost glare or the quality and spread of Glare for Streaks "
          "and Simple Star");
  glare_panel.add_input<decl::Float>("Fade")
      .default_value(0.9f)
      .min(0.75f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", {CMP_NODE_GLARE_SIMPLE_STAR, CMP_NODE_GLARE_STREAKS})
      .description("Streak fade-out factor");
  glare_panel.add_input<decl::Float>("Color Modulation")
      .default_value(0.25)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", {CMP_NODE_GLARE_GHOST, CMP_NODE_GLARE_STREAKS})
      .description("Modulates colors of streaks and ghosts for a spectral dispersion effect");
  glare_panel.add_input<decl::Bool>("Diagonal", "Diagonal Star")
      .default_value(true)
      .usage_by_menu("Type", CMP_NODE_GLARE_SIMPLE_STAR)
      .description("Align the star diagonally");
  glare_panel.add_input<decl::Vector>("Sun Position")
      .subtype(PROP_FACTOR)
      .dimensions(2)
      .default_value({0.5f, 0.5f})
      .min(0.0f)
      .max(1.0f)
      .usage_by_menu("Type", CMP_NODE_GLARE_SUN_BEAMS)
      .description(
          "The position of the source of the rays in normalized coordinates. 0 means lower left "
          "corner and 1 means upper right corner");
  glare_panel.add_input<decl::Float>("Jitter")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0)
      .subtype(PROP_FACTOR)
      .usage_by_menu("Type", CMP_NODE_GLARE_SUN_BEAMS)
      .description(
          "The amount of jitter to introduce while computing rays, higher jitter can be faster "
          "but can produce grainy or noisy results");
  glare_panel.add_input<decl::Menu>("Kernel Data Type")
      .default_value(KernelDataType::Float)
      .static_items(kernel_data_type_items)
      .usage_by_menu("Type", CMP_NODE_GLARE_KERNEL)
      .optional_label();
  glare_panel.add_input<decl::Float>("Kernel", "Float Kernel")
      .hide_value()
      .structure_type(StructureType::Dynamic)
      .usage_by_menu("Kernel Data Type", int(KernelDataType::Float))
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms);
  glare_panel.add_input<decl::Color>("Kernel", "Color Kernel")
      .hide_value()
      .structure_type(StructureType::Dynamic)
      .usage_by_menu("Kernel Data Type", int(KernelDataType::Color))
      .compositor_realization_mode(CompositorInputRealizationMode::Transforms);
}

static void node_composit_init_glare(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, but kept for forward compatibility. */
  NodeGlare *ndg = MEM_callocN<NodeGlare>(__func__);
  node->storage = ndg;
}

class SocketSearchOp {
 public:
  CMPNodeGlareType type = CMP_NODE_GLARE_SIMPLE_STAR;
  void operator()(LinkSearchOpParams &params)
  {
    bNode &node = params.add_node("CompositorNodeGlare");
    bNodeSocket &type_socket = *blender::bke::node_find_socket(node, SOCK_IN, "Type");
    type_socket.default_value_typed<bNodeSocketValueMenu>()->value = this->type;
    params.update_and_connect_available_socket(node, "Image");
  }
};

static void gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype from_socket_type = eNodeSocketDatatype(params.other_socket().type);
  if (!params.node_tree().typeinfo->validate_link(from_socket_type, SOCK_RGBA)) {
    return;
  }

  params.add_item(IFACE_("Simple Star"), SocketSearchOp{CMP_NODE_GLARE_SIMPLE_STAR});
  params.add_item(IFACE_("Fog Glow"), SocketSearchOp{CMP_NODE_GLARE_FOG_GLOW});
  params.add_item(IFACE_("Streaks"), SocketSearchOp{CMP_NODE_GLARE_STREAKS});
  params.add_item(IFACE_("Ghost"), SocketSearchOp{CMP_NODE_GLARE_GHOST});
  params.add_item(IFACE_("Bloom"), SocketSearchOp{CMP_NODE_GLARE_BLOOM});
  params.add_item(IFACE_("Sun Beams"), SocketSearchOp{CMP_NODE_GLARE_SUN_BEAMS});
  params.add_item(IFACE_("Kernel"), SocketSearchOp{CMP_NODE_GLARE_KERNEL});
}

using namespace blender::compositor;

class GlareOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &image_input = this->get_input("Image");
    Result &glare_output = this->get_result("Glare");
    Result &highlights_output = this->get_result("Highlights");

    if (image_input.is_single_value()) {
      Result &image_output = this->get_result("Image");
      if (image_output.should_compute()) {
        image_output.share_data(image_input);
      }
      if (glare_output.should_compute()) {
        glare_output.allocate_invalid();
      }
      if (highlights_output.should_compute()) {
        highlights_output.allocate_invalid();
      }
      return;
    }

    Result highlights = this->compute_highlights();
    Result glare = this->compute_glare(highlights);

    if (highlights_output.should_compute()) {
      if (highlights.domain().size != image_input.domain().size) {
        /* The highlights were computed on a fraction of the image size, see the get_quality_factor
         * method. So we need to upsample them while writing as opposed to just stealing the
         * existing data. */
        this->write_highlights_output(highlights);
      }
      else {
        highlights_output.steal_data(highlights);
      }
    }
    highlights.release();

    /* Combine the original input and the generated glare. */
    execute_mix(glare);

    if (glare_output.should_compute()) {
      this->write_glare_output(glare);
    }
    glare.release();
  }

  /* -----------------
   * Glare Highlights.
   * ----------------- */

  Result compute_highlights()
  {
    if (this->context().use_gpu()) {
      return this->execute_highlights_gpu();
    }
    return this->execute_highlights_cpu();
  }

  Result execute_highlights_gpu()
  {
    gpu::Shader *shader = context().get_shader("compositor_glare_highlights");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "threshold", this->get_threshold());
    GPU_shader_uniform_1f(shader, "highlights_smoothness", this->get_highlights_smoothness());
    GPU_shader_uniform_1f(shader, "max_brightness", this->get_maximum_brightness());
    GPU_shader_uniform_1i(shader, "quality", this->get_quality());

    const Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image, true);
    input_image.bind_as_texture(shader, "input_tx");

    const int2 highlights_size = this->get_glare_image_size();
    Result highlights_result = context().create_result(ResultType::Color);
    highlights_result.allocate_texture(highlights_size);
    highlights_result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, highlights_size);

    GPU_shader_unbind();
    input_image.unbind_as_texture();
    highlights_result.unbind_as_image();

    return highlights_result;
  }

  Result execute_highlights_cpu()
  {
    const float threshold = this->get_threshold();
    const float highlights_smoothness = this->get_highlights_smoothness();
    const float max_brightness = this->get_maximum_brightness();

    const Result &input = get_input("Image");

    const int2 highlights_size = this->get_glare_image_size();
    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(highlights_size);

    const CMPNodeGlareQuality quality = this->get_quality();
    const int2 input_size = input.domain().size;

    parallel_for(highlights_size, [&](const int2 texel) {
      float4 color = float4(0.0f);

      switch (quality) {
        case CMP_NODE_GLARE_QUALITY_HIGH: {
          color = float4(input.load_pixel<Color>(texel));
          break;
        }

        /* Down-sample the image 2 times to match the output size by averaging the 2x2 block of
         * pixels into a single output pixel. This is done due to the bilinear interpolation at the
         * center of the 2x2 block of pixels */
        case CMP_NODE_GLARE_QUALITY_MEDIUM: {
          float2 normalized_coordinates = (float2(texel) * 2.0f + float2(1.0f)) /
                                          float2(input_size);
          color = input.sample_bilinear_extended(normalized_coordinates);
          break;
        }

          /* Down-sample the image 4 times to match the output size by averaging each 4x4 block of
           * pixels into a single output pixel. This is done by averaging 4 bilinear taps at the
           * center of each of the corner 2x2 pixel blocks, which are themselves the average of the
           * 2x2 block due to the bilinear interpolation at the center. */
        case CMP_NODE_GLARE_QUALITY_LOW: {

          float2 lower_left_coordinates = (float2(texel) * 4.0f + float2(1.0f)) /
                                          float2(input_size);
          float4 lower_left_color = input.sample_bilinear_extended(lower_left_coordinates);

          float2 lower_right_coordinates = (float2(texel) * 4.0f + float2(3.0f, 1.0f)) /
                                           float2(input_size);
          float4 lower_right_color = input.sample_bilinear_extended(lower_right_coordinates);

          float2 upper_left_coordinates = (float2(texel) * 4.0f + float2(1.0f, 3.0f)) /
                                          float2(input_size);
          float4 upper_left_color = input.sample_bilinear_extended(upper_left_coordinates);

          float2 upper_right_coordinates = (float2(texel) * 4.0f + float2(3.0f)) /
                                           float2(input_size);
          float4 upper_right_color = input.sample_bilinear_extended(upper_right_coordinates);

          color = (upper_left_color + upper_right_color + lower_left_color + lower_right_color) /
                  4.0f;
          break;
        }
      }

      float4 hsva;
      rgb_to_hsv_v(color, hsva);

      /* Clamp the brightness of the highlights such that pixels whose brightness are less than the
       * threshold will be equal to the threshold and will become zero once threshold is subtracted
       * later. We also clamp by the specified max brightness to suppress very bright highlights.
       *
       * We use a smooth clamping function such that highlights do not become very sharp but use
       * the adaptive variant such that we guarantee that zero highlights remain zero even after
       * smoothing. Notice that when we mention zero, we mean zero after subtracting the threshold,
       * so we actually mean the minimum bound, the threshold. See the adaptive_smooth_clamp
       * function for more information. */
      const float clamped_brightness = this->adaptive_smooth_clamp(
          hsva.z, threshold, max_brightness, highlights_smoothness);

      /* The final brightness is relative to the threshold. */
      hsva.z = clamped_brightness - threshold;

      float4 rgba;
      hsv_to_rgb_v(hsva, rgba);

      output.store_pixel(texel, Color(float4(rgba.xyz(), 1.0f)));
    });

    return output;
  }

  float get_maximum_brightness()
  {
    /* Clamp disabled, return the maximum possible brightness. */
    if (!this->get_clamp_highlights()) {
      return std::numeric_limits<float>::max();
    }

    /* Brightness of the highlights are relative to the threshold, see execute_highlights_cpu, so
     * we add the threshold such that the maximum brightness corresponds to the actual brightness
     * of the computed highlights. */
    return this->get_threshold() + this->get_max_highlights();
  }

  /* A Quadratic Polynomial smooth minimum function *without* normalization, based on:
   *
   *   https://iquilezles.org/articles/smin/
   *
   * This should not be converted into a common utility function in BLI because the glare code is
   * specifically designed for it as can be seen in the adaptive_smooth_clamp method, and it is
   * intentionally not normalized. */
  float smooth_min(const float a, const float b, const float smoothness)
  {
    if (smoothness == 0.0f) {
      return math::min(a, b);
    }
    const float h = math::max(smoothness - math::abs(a - b), 0.0f) / smoothness;
    return math::min(a, b) - h * h * smoothness * (1.0f / 4.0f);
  }

  float smooth_max(const float a, const float b, const float smoothness)
  {
    return -this->smooth_min(-a, -b, smoothness);
  }

  /* Clamps the input x within min_value and max_value using a quadratic polynomial smooth minimum
   * and maximum functions, with individual control over their smoothness. */
  float smooth_clamp(const float x,
                     const float min_value,
                     const float max_value,
                     const float min_smoothness,
                     const float max_smoothness)
  {
    return this->smooth_min(
        max_value, this->smooth_max(min_value, x, min_smoothness), max_smoothness);
  }

  /* A variant of smooth_clamp that limits the smoothness such that the function evaluates to the
   * given min for 0 <= min <= max and x >= 0. The aforementioned guarantee holds for the standard
   * clamp function by definition, but since the smooth clamp function gradually increases before
   * the specified min/max, if min/max are sufficiently close together or to zero, they will not
   * evaluate to min at zero or at min, since zero or min will be at the region of the gradual
   * increase.
   *
   * It can be shown that the width of the gradual increase region is equivalent to the smoothness
   * parameter, so smoothness can't be larger than the difference between the min/max and zero, or
   * larger than the difference between min and max themselves. Otherwise, zero or min will lie
   * inside the gradual increase region of min/max. So we limit the smoothness of min/max by taking
   * the minimum with the distances to zero and to the distance to the other bound. */
  float adaptive_smooth_clamp(const float x,
                              const float min_value,
                              const float max_value,
                              const float smoothness)
  {
    const float range_distance = math::distance(min_value, max_value);
    const float distance_from_min_to_zero = math::distance(min_value, 0.0f);
    const float distance_from_max_to_zero = math::distance(max_value, 0.0f);

    const float max_safe_smoothness_for_min = math::min(distance_from_min_to_zero, range_distance);
    const float max_safe_smoothness_for_max = math::min(distance_from_max_to_zero, range_distance);

    const float min_smoothness = math::min(smoothness, max_safe_smoothness_for_min);
    const float max_smoothness = math::min(smoothness, max_safe_smoothness_for_max);

    return this->smooth_clamp(x, min_value, max_value, min_smoothness, max_smoothness);
  }

  float get_threshold()
  {
    return math::max(0.0f, this->get_input("Highlights Threshold").get_single_value_default(1.0f));
  }

  float get_highlights_smoothness()
  {
    return math::max(0.0f,
                     this->get_input("Highlights Smoothness").get_single_value_default(0.1f));
  }

  bool get_clamp_highlights()
  {
    return this->get_input("Clamp Highlights").get_single_value_default(false);
  }

  float get_max_highlights()
  {
    return math::max(0.0f, this->get_input("Maximum Highlights").get_single_value_default(0.0f));
  }

  /* Writes the given input highlights by upsampling it using bilinear interpolation to match the
   * size of the original input, allocating the highlights output and writing the result to it. */
  void write_highlights_output(const Result &highlights)
  {
    if (this->context().use_gpu()) {
      this->write_highlights_output_gpu(highlights);
    }
    else {
      this->write_highlights_output_cpu(highlights);
    }
  }

  void write_highlights_output_gpu(const Result &highlights)
  {
    gpu::Shader *shader = this->context().get_shader("compositor_glare_write_highlights_output");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(highlights, true);
    GPU_texture_extend_mode(highlights, GPU_SAMPLER_EXTEND_MODE_EXTEND);
    highlights.bind_as_texture(shader, "input_tx");

    const Result &image_input = this->get_input("Image");
    Result &output = this->get_result("Highlights");
    output.allocate_texture(image_input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, output.domain().size);

    GPU_shader_unbind();
    output.unbind_as_image();
    highlights.unbind_as_texture();
  }

  void write_highlights_output_cpu(const Result &highlights)
  {
    const Result &image_input = this->get_input("Image");
    Result &output = this->get_result("Highlights");
    output.allocate_texture(image_input.domain());

    const int2 size = output.domain().size;
    parallel_for(size, [&](const int2 texel) {
      float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(size);
      output.store_pixel(texel,
                         Color(highlights.sample_bilinear_extended(normalized_coordinates)));
    });
  }

  /* ------
   * Glare.
   * ------ */

  Result compute_glare(Result &highlights_result)
  {
    if (!this->should_compute_glare()) {
      return this->context().create_result(ResultType::Color);
    }

    switch (this->get_type()) {
      case CMP_NODE_GLARE_SIMPLE_STAR:
        return this->execute_simple_star(highlights_result);
      case CMP_NODE_GLARE_FOG_GLOW:
        return this->execute_fog_glow(highlights_result);
      case CMP_NODE_GLARE_STREAKS:
        return this->execute_streaks(highlights_result);
      case CMP_NODE_GLARE_GHOST:
        return this->execute_ghost(highlights_result);
      case CMP_NODE_GLARE_BLOOM:
        return this->execute_bloom(highlights_result);
      case CMP_NODE_GLARE_SUN_BEAMS:
        return this->execute_sun_beams(highlights_result);
      case CMP_NODE_GLARE_KERNEL:
        return this->execute_kernel(highlights_result);
    }

    return this->execute_simple_star(highlights_result);
  }

  /* Glare should be computed either because the glare output is needed directly or the image
   * output is needed. */
  bool should_compute_glare()
  {
    return this->get_result("Glare").should_compute() ||
           this->get_result("Image").should_compute();
  }

  /* ------------------
   * Simple Star Glare.
   * ------------------ */

  Result execute_simple_star(const Result &highlights)
  {
    if (this->get_diagonal_star()) {
      return execute_simple_star_diagonal(highlights);
    }
    return execute_simple_star_axis_aligned(highlights);
  }

  Result execute_simple_star_axis_aligned(const Result &highlights)
  {
    Result horizontal_pass_result = execute_simple_star_horizontal_pass(highlights);
    Result vertical_pass_result = this->execute_simple_star_vertical_pass(highlights,
                                                                          horizontal_pass_result);
    horizontal_pass_result.release();
    return vertical_pass_result;
  }

  Result execute_simple_star_vertical_pass(const Result &highlights,
                                           const Result &horizontal_pass_result)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_vertical_pass_gpu(highlights, horizontal_pass_result);
    }
    return this->execute_simple_star_vertical_pass_cpu(highlights, horizontal_pass_result);
  }

  Result execute_simple_star_vertical_pass_gpu(const Result &highlights,
                                               const Result &horizontal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result vertical_pass_result = context().create_result(ResultType::Color);
    vertical_pass_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(vertical_pass_result, highlights);

    gpu::Shader *shader = context().get_shader("compositor_glare_simple_star_vertical_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", this->get_fade());

    horizontal_pass_result.bind_as_texture(shader, "horizontal_tx");

    vertical_pass_result.bind_as_image(shader, "vertical_img");

    /* Dispatch a thread for each column in the image. */
    const int width = size.x;
    compute_dispatch_threads_at_least(shader, int2(width, 1));

    horizontal_pass_result.unbind_as_texture();
    vertical_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return vertical_pass_result;
  }

  Result execute_simple_star_vertical_pass_cpu(const Result &highlights,
                                               const Result &horizontal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result output = this->context().create_result(ResultType::Color);
    output.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      output.store_pixel(texel, highlights.load_pixel<Color>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = this->get_fade();

    /* Dispatch a thread for each column in the image. */
    const int width = size.x;
    threading::parallel_for(IndexRange(width), 1, [&](const IndexRange sub_range) {
      for (const int64_t x : sub_range) {
        int height = size.y;

        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * column mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from bottom to top by mixing the value of
           * the pixel in the column with the average value of the previous output and next input
           * in the same column. */
          for (int y = 0; y < height; y++) {
            int2 texel = int2(x, y);
            float4 previous_output = float4(output.load_pixel_zero<Color>(texel - int2(0, i)));
            float4 current_input = float4(output.load_pixel<Color>(texel));
            float4 next_input = float4(output.load_pixel_zero<Color>(texel + int2(0, i)));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, Color(causal_output));
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from top to bottom by mixing the value
           * of the pixel in the column with the average value of the previous output and next
           * input in the same column. */
          for (int y = height - 1; y >= 0; y--) {
            int2 texel = int2(x, y);
            float4 previous_output = float4(output.load_pixel_zero<Color>(texel + int2(0, i)));
            float4 current_input = float4(output.load_pixel<Color>(texel));
            float4 next_input = float4(output.load_pixel_zero<Color>(texel - int2(0, i)));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, Color(non_causal_output));
          }
        }

        /* For each pixel in the column mapped to the current invocation thread, add the result of
         * the horizontal pass to the vertical pass. */
        for (int y = 0; y < height; y++) {
          int2 texel = int2(x, y);
          float4 horizontal = float4(horizontal_pass_result.load_pixel<Color>(texel));
          float4 vertical = float4(output.load_pixel<Color>(texel));
          float4 combined = horizontal + vertical;
          output.store_pixel(texel, Color(float4(combined.xyz(), 1.0f)));
        }
      }
    });

    return output;
  }

  Result execute_simple_star_horizontal_pass(const Result &highlights)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_horizontal_pass_gpu(highlights);
    }
    return this->execute_simple_star_horizontal_pass_cpu(highlights);
  }

  Result execute_simple_star_horizontal_pass_gpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result horizontal_pass_result = context().create_result(ResultType::Color);
    horizontal_pass_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(horizontal_pass_result, highlights);

    gpu::Shader *shader = context().get_shader("compositor_glare_simple_star_horizontal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", this->get_fade());

    horizontal_pass_result.bind_as_image(shader, "horizontal_img");

    /* Dispatch a thread for each row in the image. */
    compute_dispatch_threads_at_least(shader, int2(size.y, 1));

    horizontal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return horizontal_pass_result;
  }

  Result execute_simple_star_horizontal_pass_cpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result horizontal_pass_result = context().create_result(ResultType::Color);
    horizontal_pass_result.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      horizontal_pass_result.store_pixel(texel, highlights.load_pixel<Color>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = this->get_fade();

    /* Dispatch a thread for each row in the image. */
    const int width = size.x;
    threading::parallel_for(IndexRange(size.y), 1, [&](const IndexRange sub_range) {
      for (const int64_t y : sub_range) {
        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * row mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from left to right by mixing the value of
           * the pixel in the row with the average value of the previous output and next input in
           * the same row. */
          for (int x = 0; x < width; x++) {
            int2 texel = int2(x, y);
            float4 previous_output = float4(
                horizontal_pass_result.load_pixel_zero<Color>(texel - int2(i, 0)));
            float4 current_input = float4(horizontal_pass_result.load_pixel<Color>(texel));
            float4 next_input = float4(
                horizontal_pass_result.load_pixel_zero<Color>(texel + int2(i, 0)));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            horizontal_pass_result.store_pixel(texel, Color(causal_output));
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from right to left by mixing the
           * value of the pixel in the row with the average value of the previous output and next
           * input in the same row. */
          for (int x = width - 1; x >= 0; x--) {
            int2 texel = int2(x, y);
            float4 previous_output = float4(
                horizontal_pass_result.load_pixel_zero<Color>(texel + int2(i, 0)));
            float4 current_input = float4(horizontal_pass_result.load_pixel<Color>(texel));
            float4 next_input = float4(
                horizontal_pass_result.load_pixel_zero<Color>(texel - int2(i, 0)));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            horizontal_pass_result.store_pixel(texel, Color(non_causal_output));
          }
        }
      }
    });

    return horizontal_pass_result;
  }

  Result execute_simple_star_diagonal(const Result &highlights)
  {
    Result diagonal_pass_result = execute_simple_star_diagonal_pass(highlights);
    Result anti_diagonal_pass_result = this->execute_simple_star_anti_diagonal_pass(
        highlights, diagonal_pass_result);
    diagonal_pass_result.release();
    return anti_diagonal_pass_result;
  }

  Result execute_simple_star_anti_diagonal_pass(const Result &highlights,
                                                const Result &diagonal_pass_result)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_anti_diagonal_pass_gpu(highlights, diagonal_pass_result);
    }
    return this->execute_simple_star_anti_diagonal_pass_cpu(highlights, diagonal_pass_result);
  }

  Result execute_simple_star_anti_diagonal_pass_gpu(const Result &highlights,
                                                    const Result &diagonal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result anti_diagonal_pass_result = context().create_result(ResultType::Color);
    anti_diagonal_pass_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(anti_diagonal_pass_result, highlights);

    gpu::Shader *shader = context().get_shader("compositor_glare_simple_star_anti_diagonal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", this->get_fade());

    diagonal_pass_result.bind_as_texture(shader, "diagonal_tx");

    anti_diagonal_pass_result.bind_as_image(shader, "anti_diagonal_img");

    /* Dispatch a thread for each diagonal in the image. */
    compute_dispatch_threads_at_least(shader, int2(compute_number_of_diagonals(size), 1));

    diagonal_pass_result.unbind_as_texture();
    anti_diagonal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return anti_diagonal_pass_result;
  }

  Result execute_simple_star_anti_diagonal_pass_cpu(const Result &highlights,
                                                    const Result &diagonal_pass_result)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result output = this->context().create_result(ResultType::Color);
    output.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      output.store_pixel(texel, highlights.load_pixel<Color>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = this->get_fade();

    /* Dispatch a thread for each diagonal in the image. */
    const int diagonals_count = compute_number_of_diagonals(size);
    threading::parallel_for(IndexRange(diagonals_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t index : sub_range) {
        int anti_diagonal_length = compute_anti_diagonal_length(size, index);
        int2 start = compute_anti_diagonal_start(size, index);
        int2 direction = get_anti_diagonal_direction();
        int2 end = start + (anti_diagonal_length - 1) * direction;

        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * anti diagonal mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from the start of the anti diagonal to
           * its end by mixing the value of the pixel in the anti diagonal with the average value
           * of the previous output and next input in the same anti diagonal. */
          for (int j = 0; j < anti_diagonal_length; j++) {
            int2 texel = start + j * direction;
            float4 previous_output = float4(output.load_pixel_zero<Color>(texel - i * direction));
            float4 current_input = float4(output.load_pixel<Color>(texel));
            float4 next_input = float4(output.load_pixel_zero<Color>(texel + i * direction));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, Color(causal_output));
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from the end of the diagonal to its
           * start by mixing the value of the pixel in the diagonal with the average value of the
           * previous output and next input in the same diagonal. */
          for (int j = 0; j < anti_diagonal_length; j++) {
            int2 texel = end - j * direction;
            float4 previous_output = float4(output.load_pixel_zero<Color>(texel + i * direction));
            float4 current_input = float4(output.load_pixel<Color>(texel));
            float4 next_input = float4(output.load_pixel_zero<Color>(texel - i * direction));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            output.store_pixel(texel, Color(non_causal_output));
          }
        }

        /* For each pixel in the anti diagonal mapped to the current invocation thread, add the
         * result of the diagonal pass to the vertical pass. */
        for (int j = 0; j < anti_diagonal_length; j++) {
          int2 texel = start + j * direction;
          float4 horizontal = float4(diagonal_pass_result.load_pixel<Color>(texel));
          float4 vertical = float4(output.load_pixel<Color>(texel));
          float4 combined = horizontal + vertical;
          output.store_pixel(texel, Color(float4(combined.xyz(), 1.0f)));
        }
      }
    });

    return output;
  }

  Result execute_simple_star_diagonal_pass(const Result &highlights)
  {
    if (this->context().use_gpu()) {
      return this->execute_simple_star_diagonal_pass_gpu(highlights);
    }
    return this->execute_simple_star_diagonal_pass_cpu(highlights);
  }

  Result execute_simple_star_diagonal_pass_gpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result diagonal_pass_result = context().create_result(ResultType::Color);
    diagonal_pass_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(diagonal_pass_result, highlights);

    gpu::Shader *shader = context().get_shader("compositor_glare_simple_star_diagonal_pass");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "iterations", get_number_of_iterations());
    GPU_shader_uniform_1f(shader, "fade_factor", this->get_fade());

    diagonal_pass_result.bind_as_image(shader, "diagonal_img");

    /* Dispatch a thread for each diagonal in the image. */
    compute_dispatch_threads_at_least(shader, int2(compute_number_of_diagonals(size), 1));

    diagonal_pass_result.unbind_as_image();
    GPU_shader_unbind();

    return diagonal_pass_result;
  }

  Result execute_simple_star_diagonal_pass_cpu(const Result &highlights)
  {
    /* First, copy the highlights result to the output since we will be doing the computation
     * in-place. */
    const int2 size = highlights.domain().size;
    Result diagonal_pass_result = this->context().create_result(ResultType::Color);
    diagonal_pass_result.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      diagonal_pass_result.store_pixel(texel, highlights.load_pixel<Color>(texel));
    });

    const int iterations = this->get_number_of_iterations();
    const float fade_factor = this->get_fade();

    /* Dispatch a thread for each diagonal in the image. */
    const int diagonals_count = compute_number_of_diagonals(size);
    threading::parallel_for(IndexRange(diagonals_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t index : sub_range) {
        int diagonal_length = compute_diagonal_length(size, index);
        int2 start = compute_diagonal_start(size, index);
        int2 direction = get_diagonal_direction();
        int2 end = start + (diagonal_length - 1) * direction;

        /* For each iteration, apply a causal filter followed by a non causal filters along the
         * diagonal mapped to the current thread invocation. */
        for (int i = 0; i < iterations; i++) {
          /* Causal Pass:
           * Sequentially apply a causal filter running from the start of the diagonal to its end
           * by mixing the value of the pixel in the diagonal with the average value of the
           * previous output and next input in the same diagonal. */
          for (int j = 0; j < diagonal_length; j++) {
            int2 texel = start + j * direction;
            float4 previous_output = float4(
                diagonal_pass_result.load_pixel_zero<Color>(texel - i * direction));
            float4 current_input = float4(diagonal_pass_result.load_pixel<Color>(texel));
            float4 next_input = float4(
                diagonal_pass_result.load_pixel_zero<Color>(texel + i * direction));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 causal_output = math::interpolate(current_input, neighbor_average, fade_factor);
            diagonal_pass_result.store_pixel(texel, Color(causal_output));
          }

          /* Non Causal Pass:
           * Sequentially apply a non causal filter running from the end of the diagonal to its
           * start by mixing the value of the pixel in the diagonal with the average value of the
           * previous output and next input in the same diagonal. */
          for (int j = 0; j < diagonal_length; j++) {
            int2 texel = end - j * direction;
            float4 previous_output = float4(
                diagonal_pass_result.load_pixel_zero<Color>(texel + i * direction));
            float4 current_input = float4(diagonal_pass_result.load_pixel<Color>(texel));
            float4 next_input = float4(
                diagonal_pass_result.load_pixel_zero<Color>(texel - i * direction));

            float4 neighbor_average = (previous_output + next_input) / 2.0f;
            float4 non_causal_output = math::interpolate(
                current_input, neighbor_average, fade_factor);
            diagonal_pass_result.store_pixel(texel, Color(non_causal_output));
          }
        }
      }
    });

    return diagonal_pass_result;
  }

  bool get_diagonal_star()
  {
    return this->get_input("Diagonal Star").get_single_value_default(true);
  }

  /* --------------
   * Streaks Glare.
   * -------------- */

  Result execute_streaks(const Result &highlights)
  {
    /* Create an initially zero image where streaks will be accumulated. */
    const int2 size = highlights.domain().size;
    Result accumulated_streaks_result = context().create_result(ResultType::Color);
    accumulated_streaks_result.allocate_texture(size);
    if (this->context().use_gpu()) {
      const float4 zero_color = float4(0.0f);
      GPU_texture_clear(accumulated_streaks_result, GPU_DATA_FLOAT, zero_color);
    }
    else {
      parallel_for(size, [&](const int2 texel) {
        accumulated_streaks_result.store_pixel(texel, Color(float4(0.0f)));
      });
    }

    /* For each streak, compute its direction and apply a streak filter in that direction, then
     * accumulate the result into the accumulated streaks result. */
    for (const int streak_index : IndexRange(get_number_of_streaks())) {
      const float2 streak_direction = compute_streak_direction(streak_index);
      Result streak_result = apply_streak_filter(highlights, streak_direction);
      this->accumulate_streak(streak_result, accumulated_streaks_result);
      streak_result.release();
    }

    return accumulated_streaks_result;
  }

  Result apply_streak_filter(const Result &highlights, const float2 &streak_direction)
  {
    if (this->context().use_gpu()) {
      return this->apply_streak_filter_gpu(highlights, streak_direction);
    }
    return this->apply_streak_filter_cpu(highlights, streak_direction);
  }

  Result apply_streak_filter_gpu(const Result &highlights, const float2 &streak_direction)
  {
    gpu::Shader *shader = context().get_shader("compositor_glare_streaks_filter");
    GPU_shader_bind(shader);

    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    const int2 size = highlights.domain().size;
    Result input_streak_result = context().create_result(ResultType::Color);
    input_streak_result.allocate_texture(size);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    GPU_texture_copy(input_streak_result, highlights);

    Result output_streak_result = context().create_result(ResultType::Color);
    output_streak_result.allocate_texture(size);

    /* For the given number of iterations, apply the streak filter in the given direction. The
     * result of the previous iteration is used as the input of the current iteration. */
    const IndexRange iterations_range = IndexRange(get_number_of_iterations());
    for (const int iteration : iterations_range) {
      const float color_modulator = compute_streak_color_modulator(iteration);
      const float iteration_magnitude = compute_streak_iteration_magnitude(iteration);
      const float3 fade_factors = compute_streak_fade_factors(iteration_magnitude);
      const float2 streak_vector = streak_direction * iteration_magnitude;

      GPU_shader_uniform_1f(shader, "color_modulator", color_modulator);
      GPU_shader_uniform_3fv(shader, "fade_factors", fade_factors);
      GPU_shader_uniform_2fv(shader, "streak_vector", streak_vector);

      GPU_texture_filter_mode(input_streak_result, true);
      GPU_texture_extend_mode(input_streak_result, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
      input_streak_result.bind_as_texture(shader, "input_streak_tx");

      output_streak_result.bind_as_image(shader, "output_streak_img");

      compute_dispatch_threads_at_least(shader, size);

      input_streak_result.unbind_as_texture();
      output_streak_result.unbind_as_image();

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (iteration != iterations_range.last()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        GPU_texture_copy(input_streak_result, output_streak_result);
      }
    }

    input_streak_result.release();
    GPU_shader_unbind();

    return output_streak_result;
  }

  Result apply_streak_filter_cpu(const Result &highlights, const float2 &streak_direction)
  {
    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    const int2 size = highlights.domain().size;
    Result input = this->context().create_result(ResultType::Color);
    input.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      input.store_pixel(texel, highlights.load_pixel<Color>(texel));
    });

    Result output = this->context().create_result(ResultType::Color);
    output.allocate_texture(size);

    /* For the given number of iterations, apply the streak filter in the given direction. The
     * result of the previous iteration is used as the input of the current iteration. */
    const IndexRange iterations_range = IndexRange(this->get_number_of_iterations());
    for (const int iteration : iterations_range) {
      const float color_modulator = this->compute_streak_color_modulator(iteration);
      const float iteration_magnitude = this->compute_streak_iteration_magnitude(iteration);
      const float3 fade_factors = this->compute_streak_fade_factors(iteration_magnitude);
      const float2 streak_vector = streak_direction * iteration_magnitude;

      parallel_for(size, [&](const int2 texel) {
        /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
         * size to get the coordinates into the sampler's expected [0, 1] range. Similarly,
         * transform the vector into the sampler's space by dividing by the input size. */
        float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);
        float2 vector = streak_vector / float2(size);

        /* Load three equally spaced neighbors to the current pixel in the direction of the streak
         * vector. */
        float4 neighbors[3];
        neighbors[0] = input.sample_bilinear_zero(coordinates + vector);
        neighbors[1] = input.sample_bilinear_zero(coordinates + vector * 2.0f);
        neighbors[2] = input.sample_bilinear_zero(coordinates + vector * 3.0f);

        /* Attenuate the value of two of the channels for each of the neighbors by multiplying by
         * the color modulator. The particular channels for each neighbor were chosen to be
         * visually similar to the modulation pattern of chromatic aberration. */
        neighbors[0] *= float4(1.0f, color_modulator, color_modulator, 1.0f);
        neighbors[1] *= float4(color_modulator, color_modulator, 1.0f, 1.0f);
        neighbors[2] *= float4(color_modulator, 1.0f, color_modulator, 1.0f);

        /* Compute the weighted sum of all neighbors using the given fade factors as weights. The
         * weights are expected to be lower for neighbors that are further away. */
        float4 weighted_neighbors_sum = float4(0.0f);
        for (int i = 0; i < 3; i++) {
          weighted_neighbors_sum += fade_factors[i] * neighbors[i];
        }

        /* The output is the average between the center color and the weighted sum of the
         * neighbors. Which intuitively mean that highlights will spread in the direction of the
         * streak, which is the desired result. */
        float4 center_color = input.sample_bilinear_zero(coordinates);
        float4 output_color = (center_color + weighted_neighbors_sum) / 2.0f;
        output.store_pixel(texel, Color(output_color));
      });

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (iteration != iterations_range.last()) {
        parallel_for(size, [&](const int2 texel) {
          input.store_pixel(texel, output.load_pixel<Color>(texel));
        });
      }
    }

    input.release();
    return output;
  }

  void accumulate_streak(const Result &streak_result, Result &accumulated_streaks_result)
  {
    if (this->context().use_gpu()) {
      this->accumulate_streak_gpu(streak_result, accumulated_streaks_result);
    }
    else {
      this->accumulate_streak_cpu(streak_result, accumulated_streaks_result);
    }
  }

  void accumulate_streak_gpu(const Result &streak_result, Result &accumulated_streaks_result)
  {
    gpu::Shader *shader = this->context().get_shader("compositor_glare_streaks_accumulate");
    GPU_shader_bind(shader);

    const float attenuation_factor = this->compute_streak_attenuation_factor();
    GPU_shader_uniform_1f(shader, "attenuation_factor", attenuation_factor);

    streak_result.bind_as_texture(shader, "streak_tx");
    accumulated_streaks_result.bind_as_image(shader, "accumulated_streaks_img", true);

    compute_dispatch_threads_at_least(shader, streak_result.domain().size);

    streak_result.unbind_as_texture();
    accumulated_streaks_result.unbind_as_image();
    GPU_shader_unbind();
  }

  void accumulate_streak_cpu(const Result &streak, Result &accumulated_streaks)
  {
    const float attenuation_factor = this->compute_streak_attenuation_factor();

    const int2 size = streak.domain().size;
    parallel_for(size, [&](const int2 texel) {
      float4 attenuated_streak = float4(streak.load_pixel<Color>(texel)) * attenuation_factor;
      float4 current_accumulated_streaks = float4(accumulated_streaks.load_pixel<Color>(texel));
      float4 combined_streaks = current_accumulated_streaks + attenuated_streak;
      accumulated_streaks.store_pixel(texel, Color(float4(combined_streaks.xyz(), 1.0f)));
    });
  }

  /* As the number of iterations increase, the streaks spread farther and their intensity decrease.
   * To maintain similar intensities regardless of the number of iterations, streaks with lower
   * number of iteration are linearly attenuated. When the number of iterations is maximum, we need
   * not attenuate, so the denominator should be one, and when the number of iterations is one, we
   * need the attenuation to be maximum. This can be modeled as a simple decreasing linear equation
   * by substituting the two aforementioned cases. */
  float compute_streak_attenuation_factor()
  {
    return 1.0f / (MAX_GLARE_ITERATIONS + 1 - get_number_of_iterations());
  }

  /* Given the index of the streak in the [0, Number Of Streaks - 1] range, compute the unit
   * direction vector defining the streak. The streak directions should make angles with the x-axis
   * that are equally spaced and covers the whole two pi range, starting with the user supplied
   * angle. */
  float2 compute_streak_direction(int streak_index)
  {
    const int number_of_streaks = get_number_of_streaks();
    const float start_angle = this->get_streaks_angle();
    const float angle = start_angle + (float(streak_index) / number_of_streaks) * (M_PI * 2.0f);
    return float2(math::cos(angle), math::sin(angle));
  }

  /* Different color channels of the streaks can be modulated by being multiplied by the color
   * modulator computed by this method. The color modulation is expected to be maximum when the
   * modulation factor is 1 and non existent when it is zero. But since the color modulator is
   * multiplied to the channel and the multiplicative identity is 1, we invert the modulation
   * factor. Moreover, color modulation should be less visible on higher iterations because they
   * produce the farther more faded away parts of the streaks. To achieve that, the modulation
   * factor is raised to the power of the iteration, noting that the modulation value is in the
   * [0, 1] range so the higher the iteration the lower the resulting modulation factor. The plus
   * one makes sure the power starts at one. */
  float compute_streak_color_modulator(int iteration)
  {
    return 1.0f - std::pow(this->get_color_modulation(), iteration + 1);
  }

  /* Streaks are computed by iteratively applying a filter that samples 3 neighboring pixels in the
   * direction of the streak. Those neighboring pixels are then combined using a weighted sum. The
   * weights of the neighbors are the fade factors computed by this method. Farther neighbors are
   * expected to have lower weights because they contribute less to the combined result. Since the
   * iteration magnitude represents how far the neighbors are, as noted in the description of the
   * compute_streak_iteration_magnitude method, the fade factor for the closest neighbor is
   * computed as the user supplied fade parameter raised to the power of the magnitude, noting that
   * the fade value is in the [0, 1] range while the magnitude is larger than or equal one, so the
   * higher the power the lower the resulting fade factor. Furthermore, the other two neighbors are
   * just squared and cubed versions of the fade factor for the closest neighbor to get even lower
   * fade factors for those farther neighbors. */
  float3 compute_streak_fade_factors(float iteration_magnitude)
  {
    const float fade_factor = std::pow(this->get_fade(), iteration_magnitude);
    return float3(fade_factor, std::pow(fade_factor, 2.0f), std::pow(fade_factor, 3.0f));
  }

  /* Streaks are computed by iteratively applying a filter that samples the neighboring pixels in
   * the direction of the streak. Each higher iteration samples pixels that are farther away, the
   * magnitude computed by this method describes how farther away the neighbors are sampled. The
   * magnitude exponentially increase with the iteration. A base of 4, was chosen as compromise
   * between better quality and performance, since a lower base corresponds to more tightly spaced
   * neighbors but would require more iterations to produce a streak of the same length. */
  float compute_streak_iteration_magnitude(int iteration)
  {
    return std::pow(4.0f, iteration);
  }

  int get_number_of_streaks()
  {
    return math::clamp(this->get_input("Streaks").get_single_value_default(4), 1, 16);
  }

  float get_streaks_angle()
  {
    return this->get_input("Streaks Angle").get_single_value_default(0.0f);
  }

  /* ------------
   * Ghost Glare.
   * ------------ */

  Result execute_ghost(const Result &highlights)
  {
    Result base_ghost_result = compute_base_ghost(highlights);
    Result accumulated_ghosts_result = context().create_result(ResultType::Color);
    if (this->context().use_gpu()) {
      this->accumulate_ghosts_gpu(base_ghost_result, accumulated_ghosts_result);
    }
    else {
      this->accumulate_ghosts_cpu(base_ghost_result, accumulated_ghosts_result);
    }

    base_ghost_result.release();
    return accumulated_ghosts_result;
  }

  void accumulate_ghosts_gpu(const Result &base_ghost_result, Result &accumulated_ghosts_result)
  {
    gpu::Shader *shader = context().get_shader("compositor_glare_ghost_accumulate");
    GPU_shader_bind(shader);

    /* Color modulators are constant across iterations. */
    std::array<float4, 4> color_modulators = compute_ghost_color_modulators();
    GPU_shader_uniform_4fv_array(shader,
                                 "color_modulators",
                                 color_modulators.size(),
                                 (const float (*)[4])color_modulators.data());

    /* Zero initialize output image where ghosts will be accumulated. */
    const float4 zero_color = float4(0.0f);
    const int2 size = base_ghost_result.domain().size;
    accumulated_ghosts_result.allocate_texture(size);
    GPU_texture_clear(accumulated_ghosts_result, GPU_DATA_FLOAT, zero_color);

    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    Result input_ghost_result = context().create_result(ResultType::Color);
    input_ghost_result.allocate_texture(size);
    GPU_texture_copy(input_ghost_result, base_ghost_result);

    /* For the given number of iterations, accumulate four ghosts with different scales and color
     * modulators. The result of the previous iteration is used as the input of the current
     * iteration. We start from index 1 because we are not interested in the scales produced for
     * the first iteration according to visual judgment, see the compute_ghost_scales method. */
    const IndexRange iterations_range = IndexRange(get_number_of_iterations()).drop_front(1);
    for (const int i : iterations_range) {
      std::array<float, 4> scales = compute_ghost_scales(i);
      GPU_shader_uniform_4fv(shader, "scales", scales.data());

      input_ghost_result.bind_as_texture(shader, "input_ghost_tx");
      accumulated_ghosts_result.bind_as_image(shader, "accumulated_ghost_img", true);

      compute_dispatch_threads_at_least(shader, size);

      input_ghost_result.unbind_as_texture();
      accumulated_ghosts_result.unbind_as_image();

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (i != iterations_range.last()) {
        GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
        GPU_texture_copy(input_ghost_result, accumulated_ghosts_result);
      }
    }

    GPU_shader_unbind();
    input_ghost_result.release();
  }

  void accumulate_ghosts_cpu(const Result &base_ghost, Result &accumulated_ghosts_result)
  {
    /* Color modulators are constant across iterations. */
    std::array<float4, 4> color_modulators = this->compute_ghost_color_modulators();

    /* Zero initialize output image where ghosts will be accumulated. */
    const int2 size = base_ghost.domain().size;
    accumulated_ghosts_result.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      accumulated_ghosts_result.store_pixel(texel, Color(float4(0.0f)));
    });

    /* Copy the highlights result into a new result because the output will be copied to the input
     * after each iteration. */
    Result input = context().create_result(ResultType::Color);
    input.allocate_texture(size);
    parallel_for(size, [&](const int2 texel) {
      input.store_pixel(texel, base_ghost.load_pixel<Color>(texel));
    });

    /* For the given number of iterations, accumulate four ghosts with different scales and color
     * modulators. The result of the previous iteration is used as the input of the current
     * iteration. We start from index 1 because we are not interested in the scales produced for
     * the first iteration according to visual judgment, see the compute_ghost_scales method. */
    const IndexRange iterations_range = IndexRange(this->get_number_of_iterations()).drop_front(1);
    for (const int i : iterations_range) {
      std::array<float, 4> scales = compute_ghost_scales(i);

      parallel_for(size, [&](const int2 texel) {
        /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
         * size to get the coordinates into the sampler's expected [0, 1] range. */
        float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

        /* We accumulate four variants of the input ghost texture, each is scaled by some amount
         * and possibly multiplied by some color as a form of color modulation. */
        float4 accumulated_ghost = float4(0.0f);
        for (int i = 0; i < 4; i++) {
          float scale = scales[i];
          float4 color_modulator = color_modulators[i];

          /* Scale the coordinates for the ghost, pre subtract 0.5 and post add 0.5 to use 0.5 as
           * the origin of the scaling. */
          float2 scaled_coordinates = (coordinates - 0.5f) * scale + 0.5f;

          /* The value of the ghost is attenuated by a scalar multiple of the inverse distance to
           * the center, such that it is maximum at the center and become zero further from the
           * center, making sure to take the scale into account. The scalar multiple of 1 / 4 is
           * chosen using visual judgment. */
          float distance_to_center = math::distance(coordinates, float2(0.5f)) * 2.0f;
          float attenuator = math::max(0.0f, 1.0f - distance_to_center * math::abs(scale)) / 4.0f;

          /* Accumulate the scaled ghost after attenuating and color modulating its value. */
          float4 multiplier = attenuator * color_modulator;
          accumulated_ghost += input.sample_bilinear_zero(scaled_coordinates) * multiplier;
        }

        float4 current_accumulated_ghost = float4(
            accumulated_ghosts_result.load_pixel<Color>(texel));
        float4 combined_ghost = current_accumulated_ghost + accumulated_ghost;
        accumulated_ghosts_result.store_pixel(texel, Color(float4(combined_ghost.xyz(), 1.0f)));
      });

      /* The accumulated result serves as the input for the next iteration, so copy the result to
       * the input result since it can't be used for reading and writing simultaneously. Skip
       * copying for the last iteration since it is not needed. */
      if (i != iterations_range.last()) {
        parallel_for(size, [&](const int2 texel) {
          input.store_pixel(texel, accumulated_ghosts_result.load_pixel<Color>(texel));
        });
      }
    }

    input.release();
  }

  /* Computes two ghosts by blurring the highlights with two different radii, then adds them into a
   * single base ghost image after scaling them by some factor and flipping the bigger ghost along
   * the center of the image. */
  Result compute_base_ghost(const Result &highlights)
  {
    Result small_ghost_result = context().create_result(ResultType::Color);
    symmetric_separable_blur(context(),
                             highlights,
                             small_ghost_result,
                             float2(get_small_ghost_radius()),
                             R_FILTER_GAUSS);

    Result big_ghost_result = context().create_result(ResultType::Color);
    symmetric_separable_blur(
        context(), highlights, big_ghost_result, float2(get_big_ghost_radius()), R_FILTER_GAUSS);

    Result base_ghost_result = context().create_result(ResultType::Color);
    if (this->context().use_gpu()) {
      this->compute_base_ghost_gpu(small_ghost_result, big_ghost_result, base_ghost_result);
    }
    else {
      this->compute_base_ghost_cpu(small_ghost_result, big_ghost_result, base_ghost_result);
    }

    small_ghost_result.release();
    big_ghost_result.release();

    return base_ghost_result;
  }

  void compute_base_ghost_gpu(const Result &small_ghost_result,
                              const Result &big_ghost_result,
                              Result &base_ghost_result)
  {
    gpu::Shader *shader = context().get_shader("compositor_glare_ghost_base");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(small_ghost_result, true);
    GPU_texture_extend_mode(small_ghost_result, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    small_ghost_result.bind_as_texture(shader, "small_ghost_tx");

    GPU_texture_filter_mode(big_ghost_result, true);
    GPU_texture_extend_mode(big_ghost_result, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    big_ghost_result.bind_as_texture(shader, "big_ghost_tx");

    base_ghost_result.allocate_texture(small_ghost_result.domain());
    base_ghost_result.bind_as_image(shader, "combined_ghost_img");

    compute_dispatch_threads_at_least(shader, base_ghost_result.domain().size);

    GPU_shader_unbind();
    small_ghost_result.unbind_as_texture();
    big_ghost_result.unbind_as_texture();
    base_ghost_result.unbind_as_image();
  }

  void compute_base_ghost_cpu(const Result &small_ghost_result,
                              const Result &big_ghost_result,
                              Result &combined_ghost)
  {
    const int2 size = small_ghost_result.domain().size;
    combined_ghost.allocate_texture(size);

    parallel_for(size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the input sampler at the center of the pixel and divide by the image
       * size to get the coordinates into the sampler's expected [0, 1] range. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      /* The small ghost is scaled down with the origin as the center of the image by a factor
       * of 2.13, while the big ghost is flipped and scaled up with the origin as the center of the
       * image by a factor of 0.97. Note that 1) The negative scale implements the flipping. 2)
       * Factors larger than 1 actually scales down the image since the factor multiplies the
       * coordinates and not the images itself. 3) The values are arbitrarily chosen using visual
       * judgment. */
      float small_ghost_scale = 2.13f;
      float big_ghost_scale = -0.97f;

      /* Scale the coordinates for the small and big ghosts, pre subtract 0.5 and post add 0.5 to
       * use 0.5 as the origin of the scaling. Notice that the big ghost is flipped due to the
       * negative scale. */
      float2 small_ghost_coordinates = (coordinates - 0.5f) * small_ghost_scale + 0.5f;
      float2 big_ghost_coordinates = (coordinates - 0.5f) * big_ghost_scale + 0.5f;

      /* The values of the ghosts are attenuated by the inverse distance to the center, such that
       * they are maximum at the center and become zero further from the center, making sure to
       * take the aforementioned scale into account. */
      float distance_to_center = math::distance(coordinates, float2(0.5f)) * 2.0f;
      float small_ghost_attenuator = math::max(0.0f,
                                               1.0f - distance_to_center * small_ghost_scale);
      float big_ghost_attenuator = math::max(
          0.0f, 1.0f - distance_to_center * math::abs(big_ghost_scale));

      float4 small_ghost = small_ghost_result.sample_bilinear_zero(small_ghost_coordinates) *
                           small_ghost_attenuator;
      float4 big_ghost = big_ghost_result.sample_bilinear_zero(big_ghost_coordinates) *
                         big_ghost_attenuator;

      combined_ghost.store_pixel(texel, Color(small_ghost + big_ghost));
    });
  }

  /* In each iteration of ghost accumulation, four ghosts are accumulated, each of which might be
   * modulated by multiplying by some color modulator, this function generates a color modulator
   * for each of the four ghosts. The first ghost is always unmodulated, so is the multiplicative
   * identity of 1. The second ghost gets only its green and blue channels modulated, the third
   * ghost gets only its red and green channels modulated, and the fourth ghost gets only its red
   * and blue channels modulated. */
  std::array<float4, 4> compute_ghost_color_modulators()
  {
    const float color_modulation_factor = get_ghost_color_modulation_factor();

    std::array<float4, 4> color_modulators;
    color_modulators[0] = float4(1.0f);
    color_modulators[1] = float4(1.0f, color_modulation_factor, color_modulation_factor, 1.0f);
    color_modulators[2] = float4(color_modulation_factor, color_modulation_factor, 1.0f, 1.0f);
    color_modulators[3] = float4(color_modulation_factor, 1.0f, color_modulation_factor, 1.0f);

    return color_modulators;
  }

  /* In each iteration of ghost accumulation, four ghosts with different scales are accumulated.
   * Given the index of a certain iteration, this method computes the 4 scales for it. Assuming we
   * have n number of iterations, that means the total number of accumulations is 4 * n. To get a
   * variety of scales, we generate an arithmetic progression that starts from 2.1 and ends at zero
   * exclusive, containing 4 * n elements. The start scale of 2.1 is chosen arbitrarily using
   * visual judgment. To get more scale variations, every other scale is inverted with a slight
   * change in scale such that it alternates between scaling down and up, additionally every other
   * ghost is flipped across the image center by negating its scale. Finally, to get variations
   * across the number of iterations, a shift of 0.5 is introduced when the number of iterations is
   * odd, that way, the user will get variations when changing the number of iterations as opposed
   * to just getting less or more ghosts. */
  std::array<float, 4> compute_ghost_scales(int iteration)
  {
    /* Shift scales by 0.5 for odd number of iterations as discussed in the method description.
     */
    const float offset = (get_number_of_iterations() % 2 == 1) ? 0.5f : 0.0f;

    std::array<float, 4> scales;
    for (const int i : IndexRange(scales.size())) {
      /* Global index in all accumulations. */
      const int global_i = iteration * 4 + i;
      /* Arithmetic progression in the range [0, 1) + offset. */
      const float progression = (global_i + offset) / (get_number_of_iterations() * 4);
      /* Remap range [0, 1) to [1, 0) and multiply to remap to [2.1, 0). */
      scales[i] = 2.1f * (1.0f - progression);

      /* Invert the scale with a slight variation and flip it across the image center through
       * negation for odd scales as discussed in the method description. */
      if (i % 2 == 1) {
        scales[i] = -0.99f / scales[i];
      }
    }

    return scales;
  }

  /* The operation computes two base ghosts by blurring the highlights with two different radii,
   * this method computes the blur radius for the smaller one. The value is chosen using visual
   * judgment. Make sure to take the quality factor into account, see the get_quality_factor method
   * for more information. */
  float get_small_ghost_radius()
  {
    return 16.0f / get_quality_factor();
  }

  /* Computes the blur radius of the bigger ghost, which is double the blur radius if the smaller
   * one, see the get_small_ghost_radius for more information. */
  float get_big_ghost_radius()
  {
    return get_small_ghost_radius() * 2.0f;
  }

  /* The color channels of the glare can be modulated by being multiplied by this factor. In the
   * user interface, 0 means no modulation and 1 means full modulation. But since the factor is
   * multiplied, 1 corresponds to no modulation and 0 corresponds to full modulation, so we
   * subtract from one. */
  float get_ghost_color_modulation_factor()
  {
    return 1.0f - this->get_color_modulation();
  }

  /* ------------
   * Bloom Glare.
   * ------------ */

  /* Bloom is computed by first progressively half-down-sampling the highlights down to a certain
   * size, then progressively double-up-sampling the last down-sampled result up to the original
   * size of the highlights, adding the down-sampled result of the same size in each up-sampling
   * step. This can be illustrated as follows:
   *
   *             Highlights   ---+--->  Bloom
   *                  |                   |
   *             Down-sampled ---+---> Up-sampled
   *                  |                   |
   *             Down-sampled ---+---> Up-sampled
   *                  |                   |
   *             Down-sampled ---+---> Up-sampled
   *                  |                   ^
   *                 ...                  |
   *            Down-sampled  ------------'
   *
   * The smooth down-sampling followed by smooth up-sampling can be thought of as a cheap way to
   * approximate a large radius blur, and adding the corresponding down-sampled result while
   * up-sampling is done to counter the attenuation that happens during down-sampling.
   *
   * Smaller down-sampled results contribute to larger glare size, so controlling the size can be
   * done by stopping down-sampling down to a certain size, where the maximum possible size is
   * achieved when down-sampling happens down to the smallest size of 2. */
  Result execute_bloom(Result &highlights)
  {
    const int chain_length = this->compute_bloom_chain_length();

    /* If the chain length is less than 2, that means no down-sampling will happen, so we just
     * return a copy of the highlights. This is a sanitization of a corner case, so no need to
     * worry about optimizing the copy away. */
    if (chain_length < 2) {
      Result bloom_result = context().create_result(ResultType::Color);
      bloom_result.allocate_texture(highlights.domain());
      if (this->context().use_gpu()) {
        GPU_texture_copy(bloom_result, highlights);
      }
      else {
        parallel_for(bloom_result.domain().size, [&](const int2 texel) {
          bloom_result.store_pixel(texel, highlights.load_pixel<Color>(texel));
        });
      }
      return bloom_result;
    }

    Array<Result> downsample_chain = compute_bloom_downsample_chain(highlights, chain_length);

    /* Notice that for a chain length of n, we need (n - 1) up-sampling passes. */
    const IndexRange upsample_passes_range(chain_length - 1);

    for (const int i : upsample_passes_range) {
      Result &input = downsample_chain[upsample_passes_range.last() - i + 1];
      Result &output = downsample_chain[upsample_passes_range.last() - i];
      if (this->context().use_gpu()) {
        this->compute_bloom_upsample_gpu(input, output);
      }
      else {
        this->compute_bloom_upsample_cpu(input, output);
      }
      input.release();
    }

    return downsample_chain[0];
  }

  void compute_bloom_upsample_gpu(const Result &input, Result &output)
  {
    gpu::Shader *shader = context().get_shader("compositor_glare_bloom_upsample");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(input, true);
    input.bind_as_texture(shader, "input_tx");

    output.bind_as_image(shader, "output_img", true);

    compute_dispatch_threads_at_least(shader, output.domain().size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_bloom_upsample_cpu(const Result &input, Result &output)
  {
    /* Each invocation corresponds to one output pixel, where the output has twice the size of the
     * input. */
    const int2 size = output.domain().size;
    parallel_for(size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to
       * get the coordinates into the sampler's expected [0, 1] range. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      /* All the offsets in the following code section are in the normalized pixel space of the
       * output image, so compute its normalized pixel size. */
      float2 pixel_size = 1.0f / float2(size);

      /* Upsample by applying a 3x3 tent filter on the bi-linearly interpolated values evaluated at
       * the center of neighboring output pixels. As more tent filter upsampling passes are
       * applied, the result approximates a large sized Gaussian filter. This upsampling strategy
       * is described in the talk:
       *
       *   Next Generation Post Processing in Call of Duty: Advanced Warfare
       *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
       *
       * In particular, the upsampling strategy is described and illustrated in slide 162 titled
       * "Upsampling - Our Solution". */
      float4 upsampled = float4(0.0f);
      upsampled += (4.0f / 16.0f) * input.sample_bilinear_extended(coordinates);
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(-1.0f, 0.0f));
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(0.0f, 1.0f));
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(1.0f, 0.0f));
      upsampled += (2.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(0.0f, -1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(-1.0f, -1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(-1.0f, 1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(1.0f, -1.0f));
      upsampled += (1.0f / 16.0f) *
                   input.sample_bilinear_extended(coordinates + pixel_size * float2(1.0f, 1.0f));

      float4 combined = float4(output.load_pixel<Color>(texel)) + upsampled;
      output.store_pixel(texel, Color(float4(combined.xyz(), 1.0f)));
    });
  }

  /* Progressively down-sample the given result into a result with half the size for the given
   * chain length, returning an array containing the chain of down-sampled results. The first
   * result of the chain is the given result itself for easier handling. The chain length is
   * expected not to exceed the binary logarithm of the smaller dimension of the given result,
   * because that would result in down-sampling passes that produce useless textures with just
   * one pixel. */
  Array<Result> compute_bloom_downsample_chain(const Result &highlights, int chain_length)
  {
    const Result downsampled_result = context().create_result(ResultType::Color);
    Array<Result> downsample_chain(chain_length, downsampled_result);

    /* We copy the original highlights result to the first result of the chain to make the code
     * easier. */
    Result &base_layer = downsample_chain[0];
    base_layer.allocate_texture(highlights.domain());
    if (this->context().use_gpu()) {
      GPU_texture_copy(base_layer, highlights);
    }
    else {
      parallel_for(base_layer.domain().size, [&](const int2 texel) {
        base_layer.store_pixel(texel, highlights.load_pixel<Color>(texel));
      });
    }

    /* In turn, the number of passes is one less than the chain length, because the first result
     * needn't be computed. */
    const IndexRange downsample_passes_range(chain_length - 1);

    for (const int i : downsample_passes_range) {
      /* For the first down-sample pass, we use a special "Karis" down-sample pass that applies a
       * form of local tone mapping to reduce the contributions of fireflies, see the shader for
       * more information. Later passes use a simple average down-sampling filter because
       * fireflies doesn't service the first pass. */
      const bool use_karis_average = i == downsample_passes_range.first();
      if (this->context().use_gpu()) {
        this->compute_bloom_downsample_gpu(
            downsample_chain[i], downsample_chain[i + 1], use_karis_average);
      }
      else {
        if (use_karis_average) {
          this->compute_bloom_downsample_cpu<true>(downsample_chain[i], downsample_chain[i + 1]);
        }
        else {
          this->compute_bloom_downsample_cpu<false>(downsample_chain[i], downsample_chain[i + 1]);
        }
      }
    }

    return downsample_chain;
  }

  void compute_bloom_downsample_gpu(const Result &input,
                                    Result &output,
                                    const bool use_karis_average)
  {
    gpu::Shader *shader = context().get_shader(
        use_karis_average ? "compositor_glare_bloom_downsample_karis_average" :
                            "compositor_glare_bloom_downsample_simple_average");
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(input, true);
    input.bind_as_texture(shader, "input_tx");

    output.allocate_texture(input.domain().size / 2);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, output.domain().size);

    input.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  template<bool UseKarisAverage>
  void compute_bloom_downsample_cpu(const Result &input, Result &output)
  {
    const int2 size = input.domain().size / 2;
    output.allocate_texture(size);

    /* Each invocation corresponds to one output pixel, where the output has half the size of the
     * input. */
    parallel_for(size, [&](const int2 texel) {
      /* Add 0.5 to evaluate the sampler at the center of the pixel and divide by the image size to
       * get the coordinates into the sampler's expected [0, 1] range. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      /* All the offsets in the following code section are in the normalized pixel space of the
       * input texture, so compute its normalized pixel size. */
      float2 pixel_size = 1.0f / float2(input.domain().size);

      /* Each invocation downsamples a 6x6 area of pixels around the center of the corresponding
       * output pixel, but instead of sampling each of the 36 pixels in the area, we only sample 13
       * positions using bilinear fetches at the center of a number of overlapping square 4-pixel
       * groups. This downsampling strategy is described in the talk:
       *
       *   Next Generation Post Processing in Call of Duty: Advanced Warfare
       *   https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
       *
       * In particular, the downsampling strategy is described and illustrated in slide 153 titled
       * "Downsampling - Our Solution". This is employed as it significantly improves the stability
       * of the glare as can be seen in the videos in the talk. */
      float4 center = input.sample_bilinear_extended(coordinates);
      float4 upper_left_near = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(-1.0f, 1.0f));
      float4 upper_right_near = input.sample_bilinear_extended(coordinates +
                                                               pixel_size * float2(1.0f, 1.0f));
      float4 lower_left_near = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(-1.0f, -1.0f));
      float4 lower_right_near = input.sample_bilinear_extended(coordinates +
                                                               pixel_size * float2(1.0f, -1.0f));
      float4 left_far = input.sample_bilinear_extended(coordinates +
                                                       pixel_size * float2(-2.0f, 0.0f));
      float4 right_far = input.sample_bilinear_extended(coordinates +
                                                        pixel_size * float2(2.0f, 0.0f));
      float4 upper_far = input.sample_bilinear_extended(coordinates +
                                                        pixel_size * float2(0.0f, 2.0f));
      float4 lower_far = input.sample_bilinear_extended(coordinates +
                                                        pixel_size * float2(0.0f, -2.0f));
      float4 upper_left_far = input.sample_bilinear_extended(coordinates +
                                                             pixel_size * float2(-2.0f, 2.0f));
      float4 upper_right_far = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(2.0f, 2.0f));
      float4 lower_left_far = input.sample_bilinear_extended(coordinates +
                                                             pixel_size * float2(-2.0f, -2.0f));
      float4 lower_right_far = input.sample_bilinear_extended(coordinates +
                                                              pixel_size * float2(2.0f, -2.0f));

      float4 result;
      if constexpr (!UseKarisAverage) {
        /* The original weights equation mentioned in slide 153 is:
         *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
         * The 0.5 corresponds to the center group of pixels and the 0.125 corresponds to the other
         * groups of pixels. The center is sampled 4 times, the far non corner pixels are sampled 2
         * times, the near corner pixels are sampled only once; but their weight is quadruple the
         * weights of other groups; so they count as sampled 4 times, finally the far corner pixels
         * are sampled only once, essentially totaling 32 samples. So the weights are as used in
         * the following code section. */
        result = (4.0f / 32.0f) * center +
                 (4.0f / 32.0f) *
                     (upper_left_near + upper_right_near + lower_left_near + lower_right_near) +
                 (2.0f / 32.0f) * (left_far + right_far + upper_far + lower_far) +
                 (1.0f / 32.0f) *
                     (upper_left_far + upper_right_far + lower_left_far + lower_right_far);
      }
      else {
        /* Reduce the contributions of fireflies on the result by reducing each group of pixels
         * using a Karis brightness weighted sum. This is described in slide 168 titled "Fireflies
         * - Partial Karis Average".
         *
         * This needn't be done on all downsampling passes, but only the first one, since fireflies
         * will not survive the first pass, later passes can use the weighted average. */
        float4 center_weighted_sum = this->karis_brightness_weighted_sum(
            upper_left_near, upper_right_near, lower_right_near, lower_left_near);
        float4 upper_left_weighted_sum = this->karis_brightness_weighted_sum(
            upper_left_far, upper_far, center, left_far);
        float4 upper_right_weighted_sum = this->karis_brightness_weighted_sum(
            upper_far, upper_right_far, right_far, center);
        float4 lower_right_weighted_sum = this->karis_brightness_weighted_sum(
            center, right_far, lower_right_far, lower_far);
        float4 lower_left_weighted_sum = this->karis_brightness_weighted_sum(
            left_far, center, lower_far, lower_left_far);

        /* The original weights equation mentioned in slide 153 is:
         *   0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
         * Multiply both sides by 8 and you get:
         *   4 + 1 + 1 + 1 + 1 = 8
         * So the weights are as used in the following code section. */
        result = (4.0f / 8.0f) * center_weighted_sum +
                 (1.0f / 8.0f) * (upper_left_weighted_sum + upper_right_weighted_sum +
                                  lower_left_weighted_sum + lower_right_weighted_sum);
      }

      output.store_pixel(texel, Color(result));
    });
  }

  /* Computes the weighted average of the given four colors, which are assumed to the colors of
   * spatially neighboring pixels. The weights are computed so as to reduce the contributions of
   * fireflies on the result by applying a form of local tone mapping as described by Brian Karis
   * in the article "Graphic Rants: Tone Mapping".
   *
   * https://graphicrants.blogspot.com/2013/12/tone-mapping.html */
  float4 karis_brightness_weighted_sum(const float4 &color1,
                                       const float4 &color2,
                                       const float4 &color3,
                                       const float4 &color4)
  {
    float4 brightness = float4(math::reduce_max(color1.xyz()),
                               math::reduce_max(color2.xyz()),
                               math::reduce_max(color3.xyz()),
                               math::reduce_max(color4.xyz()));
    float4 weights = 1.0f / (brightness + 1.0f);
    return (color1 * weights.x + color2 * weights.y + color3 * weights.z + color4 * weights.w) *
           math::safe_rcp(math::reduce_add(weights));
  }

  /* The maximum possible glare size is achieved when we down-sampled down to the smallest size of
   * 2, which would result in a down-sampling chain length of the binary logarithm of the smaller
   * dimension of the size of the highlights.
   *
   * However, as users might want a smaller glare size, we reduce the chain length by the size
   * supplied by the user. Also make sure that log2 does not get zero. */
  int compute_bloom_chain_length()
  {
    const int2 image_size = this->get_glare_image_size();
    const int smaller_dimension = math::reduce_min(image_size);
    const float scaled_dimension = smaller_dimension * this->get_size();
    return int(std::log2(math::max(1.0f, scaled_dimension)));
  }

  /* ---------------
   * Fog Glow Glare.
   * --------------- */

  Result execute_fog_glow(const Result &highlights)
  {
#if defined(WITH_FFTW3)

    const int kernel_size = int(math::reduce_max(highlights.domain().size));

    /* Since we will be doing a circular convolution, we need to zero pad our input image by
     * the kernel size to avoid the kernel affecting the pixels at the other side of image.
     * Therefore, zero boundary is assumed. */
    const int needed_padding_amount = kernel_size;
    const int2 image_size = highlights.domain().size;
    const int2 needed_spatial_size = image_size + needed_padding_amount - 1;
    const int2 spatial_size = fftw::optimal_size_for_real_transform(needed_spatial_size);

    /* The FFTW real to complex transforms utilizes the hermitian symmetry of real transforms and
     * stores only half the output since the other half is redundant, so we only allocate half of
     * the first dimension. See Section 4.3.4 Real-data DFT Array Format in the FFTW manual for
     * more information. */
    const int2 frequency_size = int2(spatial_size.x / 2 + 1, spatial_size.y);

    /* We only process the color channels, the alpha channel is written to the output as is. */
    const int channels_count = 3;
    const int image_channels_count = 4;
    const int64_t spatial_pixels_per_channel = int64_t(spatial_size.x) * spatial_size.y;
    const int64_t frequency_pixels_per_channel = int64_t(frequency_size.x) * frequency_size.y;
    const int64_t spatial_pixels_count = spatial_pixels_per_channel * channels_count;
    const int64_t frequency_pixels_count = frequency_pixels_per_channel * channels_count;

    float *image_spatial_domain = fftwf_alloc_real(spatial_pixels_count);
    std::complex<float> *image_frequency_domain = reinterpret_cast<std::complex<float> *>(
        fftwf_alloc_complex(frequency_pixels_count));

    /* Create a real to complex plan to transform the image to the frequency domain. */
    fftwf_plan forward_plan = fftwf_plan_dft_r2c_2d(
        spatial_size.y,
        spatial_size.x,
        image_spatial_domain,
        reinterpret_cast<fftwf_complex *>(image_frequency_domain),
        FFTW_ESTIMATE);

    const float *highlights_buffer = nullptr;
    if (this->context().use_gpu()) {
      GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
      highlights_buffer = static_cast<const float *>(
          GPU_texture_read(highlights, GPU_DATA_FLOAT, 0));
    }
    else {
      highlights_buffer = static_cast<const float *>(highlights.cpu_data().data());
    }

    /* Zero pad the image to the required spatial domain size, storing each channel in planar
     * format for better cache locality, that is, RRRR...GGGG...BBBB. */
    threading::parallel_for(IndexRange(spatial_size.y), 1, [&](const IndexRange sub_y_range) {
      for (const int64_t y : sub_y_range) {
        for (const int64_t x : IndexRange(spatial_size.x)) {
          const bool is_inside_image = x < image_size.x && y < image_size.y;
          for (const int64_t channel : IndexRange(channels_count)) {
            const int64_t base_index = y * spatial_size.x + x;
            const int64_t output_index = base_index + spatial_pixels_per_channel * channel;
            if (is_inside_image) {
              const int64_t image_index = (y * image_size.x + x) * image_channels_count + channel;
              image_spatial_domain[output_index] = highlights_buffer[image_index];
            }
            else {
              image_spatial_domain[output_index] = 0.0f;
            }
          }
        }
      }
    });

    threading::parallel_for(IndexRange(channels_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t channel : sub_range) {
        fftwf_execute_dft_r2c(forward_plan,
                              image_spatial_domain + spatial_pixels_per_channel * channel,
                              reinterpret_cast<fftwf_complex *>(image_frequency_domain) +
                                  frequency_pixels_per_channel * channel);
      }
    });

    const FogGlowKernel &fog_glow_kernel = context().cache_manager().fog_glow_kernels.get(
        kernel_size, spatial_size, this->compute_fog_glow_field_of_view());

    /* Multiply the kernel and the image in the frequency domain to perform the convolution. The
     * FFT is not normalized, meaning the result of the FFT followed by an inverse FFT will result
     * in an image that is scaled by a factor of the product of the width and height, so we take
     * that into account by dividing by that scale. See Section 4.8.6 Multi-dimensional Transforms
     * of the FFTW manual for more information. */
    const float normalization_scale = float(spatial_size.x) * spatial_size.y *
                                      fog_glow_kernel.normalization_factor();
    threading::parallel_for(IndexRange(frequency_size.y), 1, [&](const IndexRange sub_y_range) {
      for (const int64_t channel : IndexRange(channels_count)) {
        for (const int64_t y : sub_y_range) {
          for (const int64_t x : IndexRange(frequency_size.x)) {
            const int64_t base_index = x + y * frequency_size.x;
            const int64_t output_index = base_index + frequency_pixels_per_channel * channel;
            const std::complex<float> kernel_value = fog_glow_kernel.frequencies()[base_index];
            image_frequency_domain[output_index] *= kernel_value / normalization_scale;
          }
        }
      }
    });

    /* Create a complex to real plan to transform the image to the real domain. */
    fftwf_plan backward_plan = fftwf_plan_dft_c2r_2d(
        spatial_size.y,
        spatial_size.x,
        reinterpret_cast<fftwf_complex *>(image_frequency_domain),
        image_spatial_domain,
        FFTW_ESTIMATE);

    threading::parallel_for(IndexRange(channels_count), 1, [&](const IndexRange sub_range) {
      for (const int64_t channel : sub_range) {
        fftwf_execute_dft_c2r(backward_plan,
                              reinterpret_cast<fftwf_complex *>(image_frequency_domain) +
                                  frequency_pixels_per_channel * channel,
                              image_spatial_domain + spatial_pixels_per_channel * channel);
      }
    });

    Result fog_glow_result = context().create_result(ResultType::Color);
    fog_glow_result.allocate_texture(highlights.domain());

    /* For GPU, write the output to the exist highlights_buffer then upload to the result after,
     * while for CPU, write to the result directly. */
    float *output = this->context().use_gpu() ?
                        const_cast<float *>(highlights_buffer) :
                        static_cast<float *>(fog_glow_result.cpu_data().data());

    /* Copy the result to the output. */
    threading::parallel_for(IndexRange(image_size.y), 1, [&](const IndexRange sub_y_range) {
      for (const int64_t y : sub_y_range) {
        for (const int64_t x : IndexRange(image_size.x)) {
          for (const int64_t channel : IndexRange(channels_count)) {
            const int64_t output_index = (x + y * image_size.x) * image_channels_count;
            const int64_t base_index = x + y * spatial_size.x;
            const int64_t input_index = base_index + spatial_pixels_per_channel * channel;
            output[output_index + channel] = image_spatial_domain[input_index];
            output[output_index + 3] = highlights_buffer[output_index + 3];
          }
        }
      }
    });

    if (this->context().use_gpu()) {
      GPU_texture_update(fog_glow_result, GPU_DATA_FLOAT, output);
      /* CPU writes to the output directly, so no need to free it. */
      MEM_freeN(output);
    }

    fftwf_destroy_plan(forward_plan);
    fftwf_destroy_plan(backward_plan);
    fftwf_free(image_spatial_domain);
    fftwf_free(image_frequency_domain);
#else
    Result fog_glow_result = context().create_result(ResultType::Color);
    fog_glow_result.allocate_texture(highlights.domain());
    if (this->context().use_gpu()) {
      GPU_texture_copy(fog_glow_result, highlights);
    }
    else {
      parallel_for(fog_glow_result.domain().size, [&](const int2 texel) {
        fog_glow_result.store_pixel(texel, highlights.load_pixel<float4>(texel));
      });
    }
#endif

    return fog_glow_result;
  }

  /* Computes the field of view of the glare based on the give size as per:
   *
   *   Spencer, Greg, et al. "Physically-Based Glare Effects for Digital Images."
   *   Proceedings of the 22nd Annual Conference on Computer Graphics and Interactive Techniques,
   *   1995.
   *
   * We choose a minimum field of view of 10 degrees using visual judgement on typical setups,
   * otherwise, a too small field of view would make the evaluation domain of the glare lie almost
   * entirely in the central Gaussian of the function, losing the exponential characteristic of the
   * function. Additionally, we take the power of the size with 1/3 to adjust the rate of change of
   * the size to make the apparent size of the glare more linear with respect to the size input. */
  math::AngleRadian compute_fog_glow_field_of_view()
  {
    return math::AngleRadian::from_degree(
        math::interpolate(180.0f, 10.0f, math::pow(this->get_size(), 1.0f / 3.0f)));
  }

  /* ----------
   * Sun Beams.
   * ---------- */

  Result execute_sun_beams(Result &highlights)
  {
    const int2 input_size = highlights.domain().size;
    const int max_steps = int(this->get_size() * math::length(input_size));
    if (max_steps == 0) {
      Result sun_beams_result = context().create_result(ResultType::Color);
      sun_beams_result.allocate_texture(highlights.domain());
      if (this->context().use_gpu()) {
        GPU_texture_copy(sun_beams_result, highlights);
      }
      else {
        parallel_for(sun_beams_result.domain().size, [&](const int2 texel) {
          sun_beams_result.store_pixel(texel, highlights.load_pixel<Color>(texel));
        });
      }
      return sun_beams_result;
    }

    if (this->context().use_gpu()) {
      return this->execute_sun_beams_gpu(highlights, max_steps);
    }
    else {
      return this->execute_sun_beams_cpu(highlights, max_steps);
    }
  }

  Result execute_sun_beams_gpu(Result &highlights, const int max_steps)
  {
    gpu::Shader *shader = context().get_shader(this->get_compositor_sun_beams_shader());
    GPU_shader_bind(shader);

    GPU_shader_uniform_2fv(shader, "source", this->get_sun_position());
    GPU_shader_uniform_1f(shader, "jitter_factor", this->get_jitter_factor());
    GPU_shader_uniform_1i(shader, "max_steps", max_steps);

    GPU_texture_filter_mode(highlights, true);
    GPU_texture_extend_mode(highlights, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    highlights.bind_as_texture(shader, "input_tx");

    Result output_image = context().create_result(ResultType::Color);
    const Domain domain = highlights.domain();
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    highlights.unbind_as_texture();
    return output_image;
  }

  const char *get_compositor_sun_beams_shader()
  {
    if (this->get_use_jitter()) {
      return "compositor_glare_sun_beams_jitter";
    }
    return "compositor_glare_sun_beams";
  }

  Result execute_sun_beams_cpu(Result &highlights, const int max_steps)
  {
    const float2 source = this->get_sun_position();

    Result output = context().create_result(ResultType::Color);
    output.allocate_texture(highlights.domain());

    const int2 input_size = highlights.domain().size;
    float jitter_factor = this->get_jitter_factor();
    bool use_jitter = this->get_use_jitter();
    parallel_for(input_size, [&](const int2 texel) {
      /* The number of steps is the distance in pixels from the source to the current texel. With
       * at least a single step and at most the user specified maximum ray length, which is
       * proportional to the diagonal pixel count. */
      float unbounded_steps = math::max(
          1.0f, math::distance(float2(texel), source * float2(input_size)));
      int steps = math::min(max_steps, int(unbounded_steps));

      /* We integrate from the current pixel to the source pixel, so compute the start coordinates
       * and step vector in the direction to source. Notice that the step vector is still computed
       * from the unbounded steps, such that the total integration length becomes limited by the
       * bounded steps, and thus by the maximum ray length. */
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(input_size);
      float2 vector_to_source = source - coordinates;
      float2 step_vector = vector_to_source / unbounded_steps;

      float accumulated_weight = 0.0f;
      float4 accumulated_color = float4(0.0f);

      int number_of_steps = (1.0f - jitter_factor) * steps;
      float random_offset = noise::hash_to_float(texel.x, texel.y);

      for (int i = 0; i <= number_of_steps; i++) {
        float position_index = this->get_sample_position(
            i, use_jitter, jitter_factor, random_offset);
        float2 position = coordinates + position_index * step_vector;

        /* We are already past the image boundaries, and any future steps are also past the image
         * boundaries, so break. */
        if (position.x < 0.0f || position.y < 0.0f || position.x > 1.0f || position.y > 1.0f) {
          break;
        }

        float4 sample_color = highlights.sample_bilinear_zero(position);

        /* Attenuate the contributions of pixels that are further away from the source using a
         * quadratic falloff. */
        float weight = math::square(1.0f - position_index / float(steps));

        accumulated_weight += weight;
        accumulated_color += sample_color * weight;
      }

      if (accumulated_weight != 0.0f) {
        accumulated_color /= accumulated_weight;
      }
      else {
        accumulated_color = highlights.sample_bilinear_zero(coordinates);
      }
      output.store_pixel(texel, Color(accumulated_color));
    });
    return output;
  }

  /* Returns an index for a position along the path between the texel and the source.
   *
   * When jitter is enabled, the position index is computed using the Global Shift
   * sampling technique: a hash-based global shift is applied to the indices which is then
   * factored to cover the range [0, steps].
   * Without jitter, the integer index `i` is returned
   * directly.
   */

  float get_sample_position(const int i,
                            const bool use_jitter,
                            float jitter_factor,
                            const float random_offset)
  {
    if (use_jitter) {
      return math::safe_divide(i + random_offset, 1.0f - jitter_factor);
    }
    return i;
  }

  bool get_use_jitter()
  {
    return this->get_jitter_factor() != 0.0f;
  }

  float get_jitter_factor()
  {
    return math::clamp(this->get_input("Jitter").get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  /* ----------
   * Kernel.
   * ---------- */

  Result execute_kernel(const Result &highlights)
  {
    const Result &kernel = this->get_kernel_input();
    Result kernel_result = this->context().create_result(ResultType::Color);

    if (kernel.is_single_value()) {
      kernel_result.allocate_texture(highlights.domain());
      if (this->context().use_gpu()) {
        GPU_texture_copy(kernel_result, highlights);
      }
      else {
        parallel_for(kernel_result.domain().size, [&](const int2 texel) {
          kernel_result.store_pixel(texel, highlights.load_pixel<Color>(texel));
        });
      }
      return kernel_result;
    }

    if (this->get_quality() == CMP_NODE_GLARE_QUALITY_HIGH) {
      convolve(this->context(), highlights, kernel, kernel_result, true);
    }
    else {
      Result downsampled_kernel = this->downsample_kernel(kernel);
      convolve(this->context(), highlights, downsampled_kernel, kernel_result, true);
      downsampled_kernel.release();
    }

    return kernel_result;
  }

  Result downsample_kernel(const Result &kernel)
  {
    if (this->context().use_gpu()) {
      return this->downsample_kernel_gpu(kernel);
    }

    return this->downsample_kernel_cpu(kernel);
  }

  Result downsample_kernel_cpu(const Result &kernel)
  {
    Result downsampled_kernel = this->context().create_result(kernel.type());
    const int2 size = kernel.domain().size / this->get_quality_factor();
    downsampled_kernel.allocate_texture(size);

    if (kernel.type() == ResultType::Float) {
      parallel_for(size, [&](const int2 texel) {
        const float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(size);
        downsampled_kernel.store_pixel(texel,
                                       kernel.sample_bilinear_extended(normalized_coordinates).x);
      });
    }
    else {
      parallel_for(size, [&](const int2 texel) {
        const float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(size);
        downsampled_kernel.store_pixel(
            texel, Color(kernel.sample_bilinear_extended(normalized_coordinates)));
      });
    }

    return downsampled_kernel;
  }

  Result downsample_kernel_gpu(const Result &kernel)
  {
    Result downsampled_kernel = this->context().create_result(kernel.type());
    const int2 size = kernel.domain().size / this->get_quality_factor();
    downsampled_kernel.allocate_texture(size);

    gpu::Shader *shader = context().get_shader(this->get_kernel_downsample_shader_name(kernel));
    GPU_shader_bind(shader);

    GPU_texture_filter_mode(kernel, true);
    kernel.bind_as_texture(shader, "input_tx");

    downsampled_kernel.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, size);

    GPU_shader_unbind();
    kernel.unbind_as_texture();
    downsampled_kernel.unbind_as_image();

    return downsampled_kernel;
  }

  const char *get_kernel_downsample_shader_name(const Result &kernel)
  {
    if (kernel.type() == ResultType::Float) {
      return "compositor_glare_kernel_downsample_float";
    }
    return "compositor_glare_kernel_downsample_color";
  }

  const Result &get_kernel_input()
  {
    switch (this->get_kernel_data_type()) {
      case KernelDataType::Float:
        return this->get_input("Float Kernel");
      case KernelDataType::Color:
        return this->get_input("Color Kernel");
    }

    return this->get_input("Float Kernel");
  }

  KernelDataType get_kernel_data_type()
  {
    const Result &input = this->get_input("Kernel Data Type");
    const MenuValue default_menu_value = MenuValue(KernelDataType::Float);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<KernelDataType>(menu_value.value);
  }

  /* ----------
   * Glare Mix.
   * ---------- */

  void execute_mix(const Result &glare_result)
  {
    Result &image_output = this->get_result("Image");
    if (!image_output.should_compute()) {
      return;
    }

    if (this->context().use_gpu()) {
      this->execute_mix_gpu(glare_result);
    }
    else {
      this->execute_mix_cpu(glare_result);
    }
  }

  void execute_mix_gpu(const Result &glare_result)
  {
    gpu::Shader *shader = context().get_shader("compositor_glare_mix");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "saturation", this->get_saturation());
    GPU_shader_uniform_3fv(shader, "tint", this->get_corrected_tint());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    GPU_texture_filter_mode(glare_result, true);
    glare_result.bind_as_texture(shader, "glare_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output_image.unbind_as_image();
    input_image.unbind_as_texture();
    glare_result.unbind_as_texture();
  }

  void execute_mix_cpu(const Result &glare_result)
  {
    const float saturation = this->get_saturation();
    const float3 tint = this->get_corrected_tint();

    const Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    parallel_for(domain.size, [&](const int2 texel) {
      /* Make sure the input is not negative
       * to avoid a subtractive effect when adding the glare. */
      float4 input_color = math::max(float4(0.0f), float4(input.load_pixel<Color>(texel)));

      float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(input.domain().size);
      float4 glare_color = glare_result.sample_bilinear_extended(normalized_coordinates);

      /* Adjust saturation of glare. */
      float4 glare_hsva;
      rgb_to_hsv_v(glare_color, glare_hsva);
      glare_hsva.y = math::clamp(glare_hsva.y * saturation, 0.0f, 1.0f);
      float4 glare_rgba;
      hsv_to_rgb_v(glare_hsva, glare_rgba);

      float3 combined_color = input_color.xyz() + glare_rgba.xyz() * tint;

      output.store_pixel(texel, Color(float4(combined_color, input_color.w)));
    });
  }

  /* Writes the given input glare by adjusting it as needed and upsampling it using bilinear
   * interpolation to match the size of the original input, allocating the glare output and writing
   * the result to it. */
  void write_glare_output(const Result &glare)
  {
    if (this->context().use_gpu()) {
      this->write_glare_output_gpu(glare);
    }
    else {
      this->write_glare_output_cpu(glare);
    }
  }

  void write_glare_output_gpu(const Result &glare)
  {
    gpu::Shader *shader = this->context().get_shader("compositor_glare_write_glare_output");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "saturation", this->get_saturation());
    GPU_shader_uniform_3fv(shader, "tint", this->get_corrected_tint());

    GPU_texture_filter_mode(glare, true);
    GPU_texture_extend_mode(glare, GPU_SAMPLER_EXTEND_MODE_EXTEND);
    glare.bind_as_texture(shader, "input_tx");

    const Result &image_input = this->get_input("Image");
    Result &output = this->get_result("Glare");
    output.allocate_texture(image_input.domain());
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, output.domain().size);

    GPU_shader_unbind();
    output.unbind_as_image();
    glare.unbind_as_texture();
  }

  void write_glare_output_cpu(const Result &glare)
  {
    const float saturation = this->get_saturation();
    const float3 tint = this->get_corrected_tint();

    const Result &image_input = this->get_input("Image");
    Result &output = this->get_result("Glare");
    output.allocate_texture(image_input.domain());

    const int2 size = output.domain().size;
    parallel_for(size, [&](const int2 texel) {
      float2 normalized_coordinates = (float2(texel) + float2(0.5f)) / float2(size);
      float4 glare_color = glare.sample_bilinear_extended(normalized_coordinates);

      /* Adjust saturation of glare. */
      float4 glare_hsva;
      rgb_to_hsv_v(glare_color, glare_hsva);
      glare_hsva.y = math::clamp(glare_hsva.y * saturation, 0.0f, 1.0f);
      float4 glare_rgba;
      hsv_to_rgb_v(glare_hsva, glare_rgba);

      float3 adjusted_glare_value = glare_rgba.xyz() * tint;
      output.store_pixel(texel, Color(float4(adjusted_glare_value, 1.0f)));
    });
  }

  /* Combine the tint, strength, and normalization scale into a single factor that can be
   * multiplied to the glare. */
  float3 get_corrected_tint()
  {
    return this->get_tint() * this->get_strength() / this->get_normalization_scale();
  }

  /* The computed glare might need to be normalized to be energy conserving or be in a reasonable
   * range, instead of doing that in a separate step as part of the glare computation, we delay the
   * normalization until the mixing step as an optimization, since we multiply by the tint and
   * strength anyways. */
  float get_normalization_scale()
  {
    switch (this->get_type()) {
      case CMP_NODE_GLARE_BLOOM:
        /* Bloom adds a number of passes equal to the chain length, if the input is constant, each
         * of those passes will hold the same constant, so we need to normalize by the chain
         * length, see the bloom code for more information. If the chain length is less than 1,
         * then no bloom will be generated, so we can return 1 in this case to avoid zero division
         * later on. */
        return math::max(1, this->compute_bloom_chain_length());
      case CMP_NODE_GLARE_SIMPLE_STAR:
      case CMP_NODE_GLARE_FOG_GLOW:
      case CMP_NODE_GLARE_STREAKS:
      case CMP_NODE_GLARE_GHOST:
      case CMP_NODE_GLARE_SUN_BEAMS:
      case CMP_NODE_GLARE_KERNEL:
        return 1.0f;
    }

    return 1.0f;
  }

  /* -------
   * Common.
   * ------- */

  CMPNodeGlareType get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_GLARE_STREAKS);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeGlareType>(menu_value.value);
  }

  float get_strength()
  {
    return math::max(0.0f, this->get_input("Strength").get_single_value_default(1.0f));
  }

  float get_saturation()
  {
    return math::max(0.0f, this->get_input("Saturation").get_single_value_default(1.0f));
  }

  float3 get_tint()
  {
    return float4(this->get_input("Tint").get_single_value_default(Color(1.0f))).xyz();
  }

  float get_size()
  {
    return math::clamp(this->get_input("Size").get_single_value_default(0.5f), 0.0f, 1.0f);
  }

  int get_number_of_iterations()
  {
    return math::clamp(this->get_input("Iterations").get_single_value_default(3), 2, 5);
  }

  float get_fade()
  {
    return math::clamp(this->get_input("Fade").get_single_value_default(0.9f), 0.75f, 1.0f);
  }

  float get_color_modulation()
  {
    return math::clamp(
        this->get_input("Color Modulation").get_single_value_default(0.25f), 0.0f, 1.0f);
  }

  float2 get_sun_position()
  {
    return this->get_input("Sun Position").get_single_value_default(float2(0.5f));
  }

  /* As a performance optimization, the operation can compute the glare on a fraction of the input
   * image size, so the input is downsampled then upsampled at the end, and this method returns the
   * size after downsampling. */
  int2 get_glare_image_size()
  {
    return math::divide_ceil(this->compute_domain().size, int2(this->get_quality_factor()));
  }

  /* The glare node can compute the glare on a fraction of the input image size to improve
   * performance. The quality values and their corresponding quality factors are as follows:
   *
   * - High Quality   => Quality Value: 0 => Quality Factor: 1.
   * - Medium Quality => Quality Value: 1 => Quality Factor: 2.
   * - Low Quality    => Quality Value: 2 => Quality Factor: 4.
   *
   * Dividing the image size by the quality factor gives the size where the glare should be
   * computed. The glare algorithm should also take the quality factor into account to compensate
   * for the reduced sized, perhaps by dividing blur radii and similar values by the quality
   * factor. */
  int get_quality_factor()
  {
    return 1 << this->get_quality();
  }

  CMPNodeGlareQuality get_quality()
  {
    const Result &input = this->get_input("Quality");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_GLARE_QUALITY_MEDIUM);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeGlareQuality>(menu_value.value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new GlareOperation(context, node);
}

}  // namespace blender::nodes::node_composite_glare_cc

static void register_node_type_cmp_glare()
{
  namespace file_ns = blender::nodes::node_composite_glare_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeGlare", CMP_NODE_GLARE);
  ntype.ui_name = "Glare";
  ntype.ui_description = "Add lens flares, fog and glows around bright parts of the image";
  ntype.enum_name_legacy = "GLARE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_glare_declare;
  ntype.initfunc = file_ns::node_composit_init_glare;
  ntype.gather_link_search_ops = file_ns::gather_link_searches;
  blender::bke::node_type_storage(
      ntype, "NodeGlare", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_glare)
