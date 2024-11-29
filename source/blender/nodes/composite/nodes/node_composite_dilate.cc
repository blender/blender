/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <limits>

#include "BLI_assert.h"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "COM_algorithm_morphological_distance.hh"
#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Dilate/Erode ******************** */

namespace blender::nodes::node_composite_dilate_cc {

NODE_STORAGE_FUNCS(NodeDilateErode)

static void cmp_node_dilate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Mask").default_value(0.0f).min(0.0f).max(1.0f);
  b.add_output<decl::Float>("Mask");
}

static void node_composit_init_dilateerode(bNodeTree * /*ntree*/, bNode *node)
{
  NodeDilateErode *data = MEM_cnew<NodeDilateErode>(__func__);
  data->falloff = PROP_SMOOTH;
  node->storage = data;
}

static void node_composit_buts_dilateerode(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "mode", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
  switch (RNA_enum_get(ptr, "mode")) {
    case CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD:
      uiItemR(layout, ptr, "edge", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
    case CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER:
      uiItemR(layout, ptr, "falloff", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
      break;
  }
}

using namespace blender::realtime_compositor;

class DilateErodeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (is_identity()) {
      get_input("Mask").pass_through(get_result("Mask"));
      return;
    }

    switch (get_method()) {
      case CMP_NODE_DILATE_ERODE_STEP:
        execute_step();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE:
        execute_distance();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD:
        execute_distance_threshold();
        return;
      case CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER:
        execute_distance_feather();
        return;
      default:
        BLI_assert_unreachable();
        return;
    }
  }

  /* ----------------------------
   * Step Morphological Operator.
   * ---------------------------- */

  void execute_step()
  {
    Result horizontal_pass_result = execute_step_horizontal_pass();
    execute_step_vertical_pass(horizontal_pass_result);
    horizontal_pass_result.release();
  }

  Result execute_step_horizontal_pass()
  {
    if (this->context().use_gpu()) {
      return this->execute_step_horizontal_pass_gpu();
    }
    return this->execute_step_horizontal_pass_cpu();
  }

  Result execute_step_horizontal_pass_gpu()
  {
    GPUShader *shader = context().get_shader(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    /* We allocate an output image of a transposed size, that is, with a height equivalent to the
     * width of the input and vice versa. This is done as a performance optimization. The shader
     * will process the image horizontally and write it to the intermediate output transposed. Then
     * the vertical pass will execute the same horizontal pass shader, but since its input is
     * transposed, it will effectively do a vertical pass and write to the output transposed,
     * effectively undoing the transposition in the horizontal pass. This is done to improve
     * spatial cache locality in the shader and to avoid having two separate shaders for each of
     * the passes. */
    const Domain domain = compute_domain();
    const int2 transposed_domain = int2(domain.size.y, domain.size.x);

    Result horizontal_pass_result = context().create_result(ResultType::Color);
    horizontal_pass_result.allocate_texture(transposed_domain);
    horizontal_pass_result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    input_mask.unbind_as_texture();
    horizontal_pass_result.unbind_as_image();

    return horizontal_pass_result;
  }

  Result execute_step_horizontal_pass_cpu()
  {
    const Result &input = get_input("Mask");

    /* We allocate an output image of a transposed size, that is, with a height equivalent to the
     * width of the input and vice versa. This is done as a performance optimization. The shader
     * will process the image horizontally and write it to the intermediate output transposed. Then
     * the vertical pass will execute the same horizontal pass shader, but since its input is
     * transposed, it will effectively do a vertical pass and write to the output transposed,
     * effectively undoing the transposition in the horizontal pass. This is done to improve
     * spatial cache locality in the shader and to avoid having two separate shaders for each of
     * the passes. */
    const Domain domain = compute_domain();
    const int2 transposed_domain = int2(domain.size.y, domain.size.x);

    Result horizontal_pass_result = context().create_result(ResultType::Color);
    horizontal_pass_result.allocate_texture(transposed_domain);

    this->execute_step_pass_cpu(input, horizontal_pass_result);

    return horizontal_pass_result;
  }

  void execute_step_vertical_pass(Result &horizontal_pass_result)
  {
    if (this->context().use_gpu()) {
      this->execute_step_vertical_pass_gpu(horizontal_pass_result);
    }
    else {
      this->execute_step_vertical_pass_cpu(horizontal_pass_result);
    }
  }

  void execute_step_vertical_pass_gpu(Result &horizontal_pass_result)
  {
    GPUShader *shader = context().get_shader(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    /* Pass the absolute value of the distance. We have specialized shaders for each sign. */
    GPU_shader_uniform_1i(shader, "radius", math::abs(get_distance()));

    horizontal_pass_result.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);
    output_mask.bind_as_image(shader, "output_img");

    /* Notice that the domain is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    compute_dispatch_threads_at_least(shader, int2(domain.size.y, domain.size.x));

    GPU_shader_unbind();
    horizontal_pass_result.unbind_as_texture();
    output_mask.unbind_as_image();
  }

  void execute_step_vertical_pass_cpu(Result &horizontal_pass_result)
  {
    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);

    this->execute_step_pass_cpu(horizontal_pass_result, output_mask);
  }

  void execute_step_pass_cpu(const Result &input, Result &output)
  {
    /* We have specialized code for each sign, so use the absolute value. */
    const int radius = math::abs(this->get_distance());

    /* Notice that the size is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    const int2 size = int2(output.domain().size.y, output.domain().size.x);
    if (this->get_distance() > 0) {
      parallel_for(size, [&](const int2 texel) {
        /* Find the maximum value in the window of the given radius around the pixel. This
         * is essentially a morphological dilate operator with a square structuring element. */
        const float limit = std::numeric_limits<float>::lowest();
        float value = limit;
        for (int i = -radius; i <= radius; i++) {
          value = math::max(value, input.load_pixel_fallback(texel + int2(i, 0), float4(limit)).x);
        }

        /* Write the value using the transposed texel. See the horizontal pass method
         * for more information on the rational behind this. */
        output.store_pixel(int2(texel.y, texel.x), float4(value));
      });
    }
    else {
      parallel_for(size, [&](const int2 texel) {
        /* Find the minimum value in the window of the given radius around the pixel. This
         * is essentially a morphological erode operator with a square structuring element. */
        const float limit = std::numeric_limits<float>::max();
        float value = limit;
        for (int i = -radius; i <= radius; i++) {
          value = math::min(value, input.load_pixel_fallback(texel + int2(i, 0), float4(limit)).x);
        }

        /* Write the value using the transposed texel. See the horizontal pass method
         * for more information on the rational behind this. */
        output.store_pixel(int2(texel.y, texel.x), float4(value));
      });
    }
  }

  const char *get_morphological_step_shader_name()
  {
    if (get_distance() > 0) {
      return "compositor_morphological_step_dilate";
    }
    return "compositor_morphological_step_erode";
  }

  /* --------------------------------
   * Distance Morphological Operator.
   * -------------------------------- */

  void execute_distance()
  {
    morphological_distance(context(), get_input("Mask"), get_result("Mask"), get_distance());
  }

  /* ------------------------------------------
   * Distance Threshold Morphological Operator.
   * ------------------------------------------ */

  void execute_distance_threshold()
  {
    Result output_mask = context().create_result(ResultType::Float);

    if (this->context().use_gpu()) {
      this->execute_distance_threshold_gpu(output_mask);
    }
    else {
      this->execute_distance_threshold_cpu(output_mask);
    }

    /* For configurations where there is little user-specified inset, anti-alias the result for
     * smoother edges. */
    Result &output = this->get_result("Mask");
    if (this->get_inset() < 2.0f) {
      smaa(this->context(), output_mask, output);
      output_mask.release();
    }
    else {
      output.steal_data(output_mask);
    }
  }

  void execute_distance_threshold_gpu(Result &output)
  {
    GPUShader *shader = context().get_shader("compositor_morphological_distance_threshold");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "inset", math::max(this->get_inset(), 10e-6f));
    GPU_shader_uniform_1i(shader, "radius", get_morphological_distance_threshold_radius());
    GPU_shader_uniform_1i(shader, "distance", get_distance());

    const Result &input_mask = get_input("Mask");
    input_mask.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    GPU_shader_unbind();
    output.unbind_as_image();
    input_mask.unbind_as_texture();
  }

  void execute_distance_threshold_cpu(Result &output)
  {
    const Result &input = get_input("Mask");

    const Domain domain = compute_domain();
    output.allocate_texture(domain);

    const float inset = math::max(this->get_inset(), 10e-6f);
    const int radius = this->get_morphological_distance_threshold_radius();
    const int distance = this->get_distance();

    /* The Morphological Distance Threshold operation is effectively three consecutive operations
     * implemented as a single operation. The three operations are as follows:
     *
     * .-----------.   .--------------.   .----------------.
     * | Threshold |-->| Dilate/Erode |-->| Distance Inset |
     * '-----------'   '--------------'   '----------------'
     *
     * The threshold operation just converts the input into a binary image, where the pixel is 1 if
     * it is larger than 0.5 and 0 otherwise. Pixels that are 1 in the output of the threshold
     * operation are said to be masked. The dilate/erode operation is a dilate or erode
     * morphological operation with a circular structuring element depending on the sign of the
     * distance, where it is a dilate operation if the distance is positive and an erode operation
     * otherwise. This is equivalent to the Morphological Distance operation, see its
     * implementation for more information. Finally, the distance inset is an operation that
     * converts the binary image into a narrow band distance field. That is, pixels that are
     * unmasked will remain 0, while pixels that are masked will start from zero at the boundary of
     * the masked region and linearly increase until reaching 1 in the span of a number pixels
     * given by the inset value.
     *
     * As a performance optimization, the dilate/erode operation is omitted and its effective
     * result is achieved by slightly adjusting the distance inset operation. The base distance
     * inset operation works by computing the signed distance from the current center pixel to the
     * nearest pixel with a different value. Since our image is a binary image, that means that if
     * the pixel is masked, we compute the signed distance to the nearest unmasked pixel, and if
     * the pixel unmasked, we compute the signed distance to the nearest masked pixel. The distance
     * is positive if the pixel is masked and negative otherwise. The distance is then normalized
     * by dividing by the given inset value and clamped to the [0, 1] range. Since distances larger
     * than the inset value are eventually clamped, the distance search window is limited to a
     * radius equivalent to the inset value.
     *
     * To archive the effective result of the omitted dilate/erode operation, we adjust the
     * distance inset operation as follows. First, we increase the radius of the distance search
     * window by the radius of the dilate/erode operation. Then we adjust the resulting narrow band
     * signed distance field as follows.
     *
     * For the erode case, we merely subtract the erode distance, which makes the outermost erode
     * distance number of pixels zero due to clamping, consequently achieving the result of the
     * erode, while retaining the needed inset because we increased the distance search window by
     * the same amount we subtracted.
     *
     * Similarly, for the dilate case, we add the dilate distance, which makes the dilate distance
     * number of pixels just outside of the masked region positive and part of the narrow band
     * distance field, consequently achieving the result of the dilate, while at the same time, the
     * innermost dilate distance number of pixels become 1 due to clamping, retaining the needed
     * inset because we increased the distance search window by the same amount we added.
     *
     * Since the erode/dilate distance is already signed appropriately as described before, we just
     * add it in both cases. */
    parallel_for(domain.size, [&](const int2 texel) {
      /* Apply a threshold operation on the center pixel, where the threshold is currently
       * hard-coded at 0.5. The pixels with values larger than the threshold are said to be masked.
       */
      bool is_center_masked = input.load_pixel(texel).x > 0.5f;

      /* Since the distance search window will access pixels outside of the bounds of the image, we
       * use a texture loader with a fallback value. And since we don't want those values to affect
       * the result, the fallback value is chosen such that the inner condition fails, which is
       * when the sampled pixel and the center pixel are the same, so choose a fallback that will
       * be considered masked if the center pixel is masked and unmasked otherwise. */
      float4 fallback = float4(is_center_masked ? 1.0f : 0.0f);

      /* Since the distance search window is limited to the given radius, the maximum possible
       * squared distance to the center is double the squared radius. */
      int minimum_squared_distance = radius * radius * 2;

      /* Find the squared distance to the nearest different pixel in the search window of the given
       * radius. */
      for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
          bool is_sample_masked = input.load_pixel_fallback(texel + int2(x, y), fallback).x > 0.5f;
          if (is_center_masked != is_sample_masked) {
            minimum_squared_distance = math::min(minimum_squared_distance, x * x + y * y);
          }
        }
      }

      /* Compute the actual distance from the squared distance and assign it an appropriate sign
       * depending on whether it lies in a masked region or not. */
      float signed_minimum_distance = math::sqrt(float(minimum_squared_distance)) *
                                      (is_center_masked ? 1.0f : -1.0f);

      /* Add the erode/dilate distance and divide by the inset amount as described in the
       * discussion, then clamp to the [0, 1] range. */
      float value = math::clamp((signed_minimum_distance + distance) / inset, 0.0f, 1.0f);

      output.store_pixel(texel, float4(value));
    });
  }

  /* See the discussion in the implementation for more information. */
  int get_morphological_distance_threshold_radius()
  {
    return int(math::ceil(get_inset())) + math::abs(get_distance());
  }

  /* ----------------------------------------
   * Distance Feather Morphological Operator.
   * ---------------------------------------- */

  void execute_distance_feather()
  {
    morphological_distance_feather(context(),
                                   get_input("Mask"),
                                   get_result("Mask"),
                                   get_distance(),
                                   node_storage(bnode()).falloff);
  }

  /* ---------------
   * Common Methods.
   * --------------- */

  bool is_identity()
  {
    const Result &input = get_input("Mask");
    if (input.is_single_value()) {
      return true;
    }

    if (get_method() == CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD && get_inset() != 0.0f) {
      return false;
    }

    if (get_distance() == 0) {
      return true;
    }

    return false;
  }

  int get_distance()
  {
    return bnode().custom2;
  }

  float get_inset()
  {
    return bnode().custom3;
  }

  CMPNodeDilateErodeMethod get_method()
  {
    return static_cast<CMPNodeDilateErodeMethod>(bnode().custom1);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DilateErodeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_dilate_cc

void register_node_type_cmp_dilateerode()
{
  namespace file_ns = blender::nodes::node_composite_dilate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_DILATEERODE, "Dilate/Erode", NODE_CLASS_OP_FILTER);
  ntype.draw_buttons = file_ns::node_composit_buts_dilateerode;
  ntype.declare = file_ns::cmp_node_dilate_declare;
  ntype.initfunc = file_ns::node_composit_init_dilateerode;
  blender::bke::node_type_storage(
      &ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
