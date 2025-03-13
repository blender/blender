/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_noise.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* Distortion can't be exactly -1.0 as it will cause infinite pincushion distortion. */
#define MINIMUM_DISTORTION -0.999f
/* Arbitrary scaling factor for the dispersion input in projector distortion mode. */
#define PROJECTOR_DISPERSION_SCALE 5.0f
/* Arbitrary scaling factor for the dispersion input in screen distortion mode. */
#define SCREEN_DISPERSION_SCALE 4.0f
/* Arbitrary scaling factor for the distortion input. */
#define DISTORTION_SCALE 4.0f

namespace blender::nodes::node_composite_lensdist_cc {

NODE_STORAGE_FUNCS(NodeLensDist)

static void cmp_node_lensdist_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Distortion")
      .default_value(0.0f)
      .min(MINIMUM_DISTORTION)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Float>("Dispersion")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_lensdist(bNodeTree * /*ntree*/, bNode *node)
{
  NodeLensDist *nld = MEM_callocN<NodeLensDist>(__func__);
  nld->jit = nld->proj = nld->fit = 0;
  node->storage = nld;
}

static void node_composit_buts_lensdist(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "use_projector", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);

  col = uiLayoutColumn(col, false);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_projector") == false);
  uiItemR(col, ptr, "use_jitter", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
  uiItemR(col, ptr, "use_fit", UI_ITEM_R_SPLIT_EMPTY_NAME, std::nullopt, ICON_NONE);
}

using namespace blender::compositor;

/* --------------------------------------------------------------------
 * Screen Lens Distortion
 */

/* A model that approximates lens distortion parameterized by a distortion parameter and dependent
 * on the squared distance to the center of the image. The distorted pixel is then computed as the
 * scalar multiplication of the pixel coordinates with the value returned by this model. See the
 * compute_distorted_uv function for more details. */
static float compute_distortion_scale(const float distortion, const float distance_squared)
{
  return 1.0f / (1.0f + math::sqrt(math::max(0.0f, 1.0f - distortion * distance_squared)));
}

/* A vectorized version of compute_distortion_scale that is applied on the chromatic distortion
 * parameters passed to the shader. */
static float3 compute_chromatic_distortion_scale(const float3 &chromatic_distortion,
                                                 const float distance_squared)
{
  return 1.0f / (1.0f + math::sqrt(math::max(float3(0.0f),
                                             1.0f - chromatic_distortion * distance_squared)));
}

/* Compute the image coordinates after distortion by the given distortion scale computed by the
 * compute_distortion_scale function. Note that the function expects centered normalized UV
 * coordinates but outputs non-centered image coordinates. */
static float2 compute_distorted_uv(const float2 &uv, const float uv_scale, const int2 &size)
{
  return (uv * uv_scale + 0.5f) * float2(size);
}

/* Compute the number of integration steps that should be used to approximate the distorted pixel
 * using a heuristic, see the compute_number_of_steps function for more details. The numbers of
 * steps is proportional to the number of pixels spanned by the distortion amount. For jitter
 * distortion, the square root of the distortion amount plus 1 is used with a minimum of 2 steps.
 * For non-jitter distortion, the distortion amount plus 1 is used as the number of steps */
static int compute_number_of_integration_steps_heuristic(const float distortion,
                                                         const bool use_jitter)
{
  if (use_jitter) {
    return distortion < 4.0f ? 2 : int(math::sqrt(distortion + 1.0f));
  }
  return int(distortion + 1.0f);
}

/* Compute the number of integration steps that should be used to compute each channel of the
 * distorted pixel. Each of the channels are distorted by their respective chromatic distortion
 * amount, then the amount of distortion between each two consecutive channels is computed, this
 * amount is then used to heuristically infer the number of needed integration steps, see the
 * integrate_distortion function for more information. */
static int3 compute_number_of_integration_steps(const float3 &chromatic_distortion,
                                                const int2 &size,
                                                const float2 &uv,
                                                const float distance_squared,
                                                const bool use_jitter)
{
  /* Distort each channel by its respective chromatic distortion amount. */
  float3 distortion_scale = compute_chromatic_distortion_scale(chromatic_distortion,
                                                               distance_squared);
  float2 distorted_uv_red = compute_distorted_uv(uv, distortion_scale.x, size);
  float2 distorted_uv_green = compute_distorted_uv(uv, distortion_scale.y, size);
  float2 distorted_uv_blue = compute_distorted_uv(uv, distortion_scale.z, size);

  /* Infer the number of needed integration steps to compute the distorted red channel starting
   * from the green channel. */
  float distortion_red = math::distance(distorted_uv_red, distorted_uv_green);
  int steps_red = compute_number_of_integration_steps_heuristic(distortion_red, use_jitter);

  /* Infer the number of needed integration steps to compute the distorted blue channel starting
   * from the green channel. */
  float distortion_blue = math::distance(distorted_uv_green, distorted_uv_blue);
  int steps_blue = compute_number_of_integration_steps_heuristic(distortion_blue, use_jitter);

  /* The number of integration steps used to compute the green channel is the sum of both the red
   * and the blue channel steps because it is computed once with each of them. */
  return int3(steps_red, steps_red + steps_blue, steps_blue);
}

/* Returns a random jitter amount, which is essentially a random value in the [0, 1] range. If
 * jitter is not enabled, return a constant 0.5 value instead. */
static float get_jitter(const int2 &texel, const int seed, const bool use_jitter)
{
  if (use_jitter) {
    return noise::hash_to_float(texel.x, texel.y, seed);
  }
  return 0.5f;
}

/* Each color channel may have a different distortion with the guarantee that the red will have the
 * lowest distortion while the blue will have the highest one. If each channel is distorted
 * independently, the image will look disintegrated, with each channel seemingly merely shifted.
 * Consequently, the distorted pixels needs to be computed by integrating along the path of change
 * of distortion starting from one channel to another. For instance, to compute the distorted red
 * from the distorted green, we accumulate the color of the distorted pixel starting from the
 * distortion of the red, taking small steps until we reach the distortion of the green. The pixel
 * color is weighted such that it is maximum at the start distortion and zero at the end distortion
 * in an arithmetic progression. The integration steps can be augmented with random values to
 * simulate lens jitter. Finally, it should be noted that this function integrates both the start
 * and end channels in reverse directions for more efficient computation. */
static float3 integrate_distortion(const int2 &texel,
                                   const Result &input,
                                   const int2 &size,
                                   const float3 &chromatic_distortion,
                                   const int start,
                                   const int end,
                                   const float distance_squared,
                                   const float2 &uv,
                                   const int steps,
                                   const bool use_jitter)
{
  float3 accumulated_color = float3(0.0f);
  float distortion_amount = chromatic_distortion[end] - chromatic_distortion[start];
  for (int i = 0; i < steps; i++) {
    /* The increment will be in the [0, 1) range across iterations. Include the start channel in
     * the jitter seed to make sure each channel gets a different jitter. */
    float increment = (i + get_jitter(texel, start * steps + i, use_jitter)) / steps;
    float distortion = chromatic_distortion[start] + increment * distortion_amount;
    float distortion_scale = compute_distortion_scale(distortion, distance_squared);

    /* Sample the color at the distorted coordinates and accumulate it weighted by the increment
     * value for both the start and end channels. */
    float2 distorted_uv = compute_distorted_uv(uv, distortion_scale, size);
    float4 color = input.sample_bilinear_zero(distorted_uv / float2(size));
    accumulated_color[start] += (1.0f - increment) * color[start];
    accumulated_color[end] += increment * color[end];
  }
  return accumulated_color;
}

static void screen_lens_distortion(const int2 texel,
                                   const Result &input,
                                   Result &output,
                                   const int2 &size,
                                   const float3 &chromatic_distortion,
                                   const float scale,
                                   const bool use_jitter)
{
  /* Compute the UV image coordinates in the range [-1, 1] as well as the squared distance to the
   * center of the image, which is at (0, 0) in the UV coordinates. */
  float2 center = float2(size) / 2.0f;
  float2 uv = scale * (float2(texel) + float2(0.5f) - center) / center;
  float distance_squared = math::dot(uv, uv);

  /* If any of the color channels will get distorted outside of the screen beyond what is possible,
   * write a zero transparent color and return. */
  float3 distortion_bounds = chromatic_distortion * distance_squared;
  if (distortion_bounds.x > 1.0f || distortion_bounds.y > 1.0f || distortion_bounds.z > 1.0f) {
    output.store_pixel(texel, float4(0.0f));
    return;
  }

  /* Compute the number of integration steps that should be used to compute each channel of the
   * distorted pixel. */
  int3 number_of_steps = compute_number_of_integration_steps(
      chromatic_distortion, size, uv, distance_squared, use_jitter);

  /* Integrate the distortion of the red and green, then the green and blue channels. That means
   * the green will be integrated twice, but this is accounted for in the number of steps which the
   * color will later be divided by. See the compute_number_of_integration_steps function for more
   * details. */
  float3 color = float3(0.0f);
  color += integrate_distortion(texel,
                                input,
                                size,
                                chromatic_distortion,
                                0,
                                1,
                                distance_squared,
                                uv,
                                number_of_steps.x,
                                use_jitter);
  color += integrate_distortion(texel,
                                input,
                                size,
                                chromatic_distortion,
                                1,
                                2,
                                distance_squared,
                                uv,
                                number_of_steps.z,
                                use_jitter);

  /* The integration above performed weighted accumulation, and thus the color needs to be divided
   * by the sum of the weights. Assuming no jitter, the weights are generated as an arithmetic
   * progression starting from (0.5 / n) to ((n - 0.5) / n) for n terms. The sum of an arithmetic
   * progression can be computed as (n * (start + end) / 2), which when subsisting the start and
   * end reduces to (n / 2). So the color should be multiplied by 2 / n. The jitter sequence
   * approximately sums to the same value because it is a uniform random value whose mean value is
   * 0.5, so the expression doesn't change regardless of jitter. */
  color *= 2.0f / float3(number_of_steps);

  output.store_pixel(texel, float4(color, 1.0f));
}

class LensDistortionOperation : public NodeOperation {
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

    if (get_is_projector()) {
      execute_projector_distortion();
    }
    else {
      execute_screen_distortion();
    }
  }

  void execute_projector_distortion()
  {
    if (this->context().use_gpu()) {
      this->execute_projector_distortion_gpu();
    }
    else {
      this->execute_projector_distortion_cpu();
    }
  }

  void execute_projector_distortion_gpu()
  {
    GPUShader *shader = context().get_shader("compositor_projector_lens_distortion");
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image, true);
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();

    const float dispersion = (get_dispersion() * PROJECTOR_DISPERSION_SCALE) / domain.size.x;
    GPU_shader_uniform_1f(shader, "dispersion", dispersion);

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void execute_projector_distortion_cpu()
  {
    const Domain domain = compute_domain();
    const float dispersion = (get_dispersion() * PROJECTOR_DISPERSION_SCALE) / domain.size.x;

    const Result &input = get_input("Image");

    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      /* Get the normalized coordinates of the pixel centers. */
      float2 normalized_texel = (float2(texel) + float2(0.5f)) / float2(size);

      /* Sample the red and blue channels shifted by the dispersion amount. */
      const float red = input.sample_bilinear_zero(normalized_texel + float2(dispersion, 0.0f)).x;
      const float green = input.load_pixel<float4>(texel).y;
      const float blue = input.sample_bilinear_zero(normalized_texel - float2(dispersion, 0.0f)).z;

      output.store_pixel(texel, float4(red, green, blue, 1.0f));
    });
  }

  void execute_screen_distortion()
  {
    if (this->context().use_gpu()) {
      this->execute_screen_distortion_gpu();
    }
    else {
      this->execute_screen_distortion_cpu();
    }
  }

  void execute_screen_distortion_gpu()
  {
    GPUShader *shader = context().get_shader(get_screen_distortion_shader());
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    GPU_texture_filter_mode(input_image, true);
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_CLAMP_TO_BORDER);
    input_image.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();

    const float3 chromatic_distortion = compute_chromatic_distortion();
    GPU_shader_uniform_3fv(shader, "chromatic_distortion", chromatic_distortion);

    GPU_shader_uniform_1f(shader, "scale", compute_scale());

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_screen_distortion_shader()
  {
    if (get_use_jitter()) {
      return "compositor_screen_lens_distortion_jitter";
    }
    return "compositor_screen_lens_distortion";
  }

  void execute_screen_distortion_cpu()
  {
    const float scale = this->compute_scale();
    const bool use_jitter = this->get_use_jitter();
    const float3 chromatic_distortion = this->compute_chromatic_distortion();

    const Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      screen_lens_distortion(texel, input, output, size, chromatic_distortion, scale, use_jitter);
    });
  }

  float get_distortion()
  {
    const Result &input = get_input("Distortion");
    return clamp_f(input.get_single_value_default(0.0f), MINIMUM_DISTORTION, 1.0f);
  }

  float get_dispersion()
  {
    const Result &input = get_input("Dispersion");
    return clamp_f(input.get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  /* Get the distortion amount for each channel. The green channel has a distortion amount that
   * matches that specified in the node inputs, while the red and blue channels have higher and
   * lower distortion amounts respectively based on the dispersion value. */
  float3 compute_chromatic_distortion()
  {
    const float green_distortion = get_distortion();
    const float dispersion = get_dispersion() / SCREEN_DISPERSION_SCALE;
    const float red_distortion = clamp_f(green_distortion + dispersion, MINIMUM_DISTORTION, 1.0f);
    const float blue_distortion = clamp_f(green_distortion - dispersion, MINIMUM_DISTORTION, 1.0f);
    return float3(red_distortion, green_distortion, blue_distortion) * DISTORTION_SCALE;
  }

  /* The distortion model will distort the image in such a way that the result will no longer
   * fit the domain of the original image, so we scale the image to account for that. If get_is_fit
   * is false, then the scaling factor will be such that the furthest pixels horizontally and
   * vertically are at the boundary of the image. Otherwise, if get_is_fit is true, the scaling
   * factor will be such that the furthest pixels diagonally are at the corner of the image. */
  float compute_scale()
  {
    const float3 distortion = compute_chromatic_distortion() / DISTORTION_SCALE;
    const float maximum_distortion = max_fff(distortion[0], distortion[1], distortion[2]);

    if (get_is_fit() && (maximum_distortion > 0.0f)) {
      return 1.0f / (1.0f + 2.0f * maximum_distortion);
    }
    return 1.0f / (1.0f + maximum_distortion);
  }

  bool get_is_projector()
  {
    return node_storage(bnode()).proj;
  }

  bool get_use_jitter()
  {
    return node_storage(bnode()).jit;
  }

  bool get_is_fit()
  {
    return node_storage(bnode()).fit;
  }

  /* Returns true if the operation does nothing and the input can be passed through. */
  bool is_identity()
  {
    /* The input is a single value and the operation does nothing. */
    if (get_input("Image").is_single_value()) {
      return true;
    }

    /* Projector have zero dispersion and does nothing. */
    if (get_is_projector() && get_dispersion() == 0.0f) {
      return true;
    }

    /* Both distortion and dispersion are zero and the operation does nothing. Jittering has an
     * effect regardless, so its gets an exemption. */
    if (!get_use_jitter() && get_distortion() == 0.0f && get_dispersion() == 0.0f) {
      return true;
    }

    return false;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new LensDistortionOperation(context, node);
}

}  // namespace blender::nodes::node_composite_lensdist_cc

void register_node_type_cmp_lensdist()
{
  namespace file_ns = blender::nodes::node_composite_lensdist_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeLensdist", CMP_NODE_LENSDIST);
  ntype.ui_name = "Lens Distortion";
  ntype.ui_description = "Simulate distortion and dispersion from camera lenses";
  ntype.enum_name_legacy = "LENSDIST";
  ntype.nclass = NODE_CLASS_DISTORT;
  ntype.declare = file_ns::cmp_node_lensdist_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_lensdist;
  ntype.initfunc = file_ns::node_composit_init_lensdist;
  blender::bke::node_type_storage(
      ntype, "NodeLensDist", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
