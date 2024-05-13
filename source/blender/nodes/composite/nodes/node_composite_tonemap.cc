/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <cmath>

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "IMB_colormanagement.hh"

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_tonemap_cc {

NODE_STORAGE_FUNCS(NodeTonemap)

static void cmp_node_tonemap_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_tonemap(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTonemap *ntm = MEM_cnew<NodeTonemap>(__func__);
  ntm->type = 1;
  ntm->key = 0.18;
  ntm->offset = 1;
  ntm->gamma = 1;
  ntm->f = 0;
  ntm->m = 0; /* Actual value is set according to input. */
  /* Default a of 1 works well with natural HDR images, but not always so for CGI.
   * Maybe should use 0 or at least lower initial value instead. */
  ntm->a = 1;
  ntm->c = 0;
  node->storage = ntm;
}

static void node_composit_buts_tonemap(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "tonemap_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  if (RNA_enum_get(ptr, "tonemap_type") == 0) {
    uiItemR(col, ptr, "key", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(col, ptr, "offset", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(col, ptr, "gamma", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  }
  else {
    uiItemR(col, ptr, "intensity", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
    uiItemR(
        col, ptr, "contrast", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(
        col, ptr, "adaptation", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
    uiItemR(
        col, ptr, "correction", UI_ITEM_R_SPLIT_EMPTY_NAME | UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class ToneMapOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");
    if (input_image.is_single_value()) {
      input_image.pass_through(output_image);
      return;
    }

    switch (get_type()) {
      case CMP_NODE_TONE_MAP_SIMPLE:
        execute_simple();
        return;
      case CMP_NODE_TONE_MAP_PHOTORECEPTOR:
        execute_photoreceptor();
        return;
      default:
        BLI_assert_unreachable();
        return;
    }
  }

  /* Tone mapping based on equation (3) from Reinhard, Erik, et al. "Photographic tone reproduction
   * for digital images." Proceedings of the 29th annual conference on Computer graphics and
   * interactive techniques. 2002. */
  void execute_simple()
  {
    const float luminance_scale = compute_luminance_scale();
    const float luminance_scale_blend_factor = compute_luminance_scale_blend_factor();
    const float gamma = node_storage(bnode()).gamma;
    const float inverse_gamma = gamma != 0.0f ? 1.0f / gamma : 0.0f;

    GPUShader *shader = context().get_shader("compositor_tone_map_simple");
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

  /* Computes the scaling factor in equation (2) from Reinhard's 2002 paper. */
  float compute_luminance_scale()
  {
    const float geometric_mean = compute_geometric_mean_of_luminance();
    return geometric_mean != 0.0 ? node_storage(bnode()).key / geometric_mean : 0.0f;
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

  /* Equation (3) from Reinhard's 2002 paper blends between high luminance scaling for high
   * luminance values and low luminance scaling for low luminance values. This is done by adding 1
   * to the denominator, since for low luminance values, the denominator will be close to 1 and for
   * high luminance values, the 1 in the denominator will be relatively insignificant. But the
   * response of such function is not always ideal, so in this implementation, the 1 was exposed as
   * a parameter to the user for more flexibility. */
  float compute_luminance_scale_blend_factor()
  {
    return node_storage(bnode()).offset;
  }

  /* Tone mapping based on equation (1) and the trilinear interpolation between equations (6) and
   * (7) from Reinhard, Erik, and Kate Devlin. "Dynamic range reduction inspired by photoreceptor
   * physiology." IEEE transactions on visualization and computer graphics 11.1 (2005): 13-24. */
  void execute_photoreceptor()
  {
    const float4 global_adaptation_level = compute_global_adaptation_level();
    const float contrast = compute_contrast();
    const float intensity = compute_intensity();
    const float chromatic_adaptation = get_chromatic_adaptation();
    const float light_adaptation = get_light_adaptation();

    GPUShader *shader = context().get_shader("compositor_tone_map_photoreceptor");
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
    return sum_color(context(), input.texture()) / (input.domain().size.x * input.domain().size.y);
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
    float sum = sum_luminance(context(), input.texture(), luminance_coefficients);
    return sum / (input.domain().size.x * input.domain().size.y);
  }

  /* Computes equation (5) from Reinhard's 2005 paper. */
  float compute_intensity()
  {
    return std::exp(-node_storage(bnode()).f);
  }

  /* If the contrast is not zero, return it, otherwise, a zero contrast denote automatic derivation
   * of the contrast value based on equations (2) and (4) from Reinhard's 2005 paper. */
  float compute_contrast()
  {
    if (node_storage(bnode()).m != 0.0f) {
      return node_storage(bnode()).m;
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
        context(), input_image.texture(), luminance_coefficients);

    return sum_of_log_luminance / (input_image.domain().size.x * input_image.domain().size.y);
  }

  float compute_log_maximum_luminance()
  {
    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    const float maximum = maximum_luminance(
        context(), get_input("Image").texture(), luminance_coefficients);
    return std::log(math::max(maximum, 1e-5f));
  }

  float compute_log_minimum_luminance()
  {
    float luminance_coefficients[3];
    IMB_colormanagement_get_luminance_coefficients(luminance_coefficients);
    const float minimum = minimum_luminance(
        context(), get_input("Image").texture(), luminance_coefficients);
    return std::log(math::max(minimum, 1e-5f));
  }

  float get_chromatic_adaptation()
  {
    return node_storage(bnode()).c;
  }

  float get_light_adaptation()
  {
    return node_storage(bnode()).a;
  }

  CMPNodeToneMapType get_type()
  {
    return static_cast<CMPNodeToneMapType>(node_storage(bnode()).type);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ToneMapOperation(context, node);
}

}  // namespace blender::nodes::node_composite_tonemap_cc

void register_node_type_cmp_tonemap()
{
  namespace file_ns = blender::nodes::node_composite_tonemap_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TONEMAP, "Tonemap", NODE_CLASS_OP_COLOR);
  ntype.declare = file_ns::cmp_node_tonemap_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_tonemap;
  ntype.initfunc = file_ns::node_composit_init_tonemap;
  blender::bke::node_type_storage(&ntype, "NodeTonemap", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
