/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <limits>

#include "BLI_assert.h"
#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "GPU_shader.hh"

#include "COM_algorithm_morphological_distance.hh"
#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_dilate_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_DILATE_ERODE_STEP, "STEP", 0, N_("Steps"), ""},
    {CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD, "THRESHOLD", 0, N_("Threshold"), ""},
    {CMP_NODE_DILATE_ERODE_DISTANCE, "DISTANCE", 0, N_("Distance"), ""},
    {CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER, "FEATHER", 0, N_("Feather"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_dilate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Mask").default_value(0.0f).min(0.0f).max(1.0f).structure_type(
      StructureType::Dynamic);
  b.add_input<decl::Int>("Size").default_value(0).description(
      "The size of dilation/erosion in pixels. Positive values dilates and negative values "
      "erodes");
  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_DILATE_ERODE_STEP)
      .static_items(type_items)
      .optional_label();
  b.add_input<decl::Float>("Falloff Size")
      .default_value(0.0f)
      .min(0.0f)
      .usage_by_menu("Type", CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD)
      .description(
          "The size of the falloff from the edges in pixels. If less than two pixels, the edges "
          "will be anti-aliased");
  b.add_input<decl::Menu>("Falloff")
      .default_value(PROP_SMOOTH)
      .static_items(rna_enum_proportional_falloff_curve_only_items)
      .optional_label()
      .usage_by_menu("Type", CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER)
      .translation_context(BLT_I18NCONTEXT_ID_CURVE_LEGACY);

  b.add_output<decl::Float>("Mask").structure_type(StructureType::Dynamic);
}

static void node_composit_init_dilateerode(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused but kept for forward compatibility. */
  NodeDilateErode *data = MEM_callocN<NodeDilateErode>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class DilateErodeOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Mask");
    Result &output = this->get_result("Mask");

    if (this->is_identity()) {
      output.share_data(input);
      return;
    }

    switch (this->get_type()) {
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
    }

    output.share_data(input);
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
    gpu::Shader *shader = context().get_shader(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "radius", this->get_structuring_element_size() / 2);

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

    Result horizontal_pass_result = context().create_result(ResultType::Float);
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

    Result horizontal_pass_result = context().create_result(ResultType::Float);
    horizontal_pass_result.allocate_texture(transposed_domain);

    if (this->is_dilation()) {
      this->execute_step_pass_cpu<true>(input, horizontal_pass_result);
    }
    else {
      this->execute_step_pass_cpu<false>(input, horizontal_pass_result);
    }

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
    gpu::Shader *shader = context().get_shader(get_morphological_step_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "radius", this->get_structuring_element_size() / 2);

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

  const char *get_morphological_step_shader_name()
  {
    if (this->is_dilation()) {
      return "compositor_morphological_step_dilate";
    }
    return "compositor_morphological_step_erode";
  }

  void execute_step_vertical_pass_cpu(Result &horizontal_pass_result)
  {
    const Domain domain = compute_domain();
    Result &output_mask = get_result("Mask");
    output_mask.allocate_texture(domain);

    if (this->is_dilation()) {
      this->execute_step_pass_cpu<true>(horizontal_pass_result, output_mask);
    }
    else {
      this->execute_step_pass_cpu<false>(horizontal_pass_result, output_mask);
    }
  }

  /* Apply a van Herk/Gil-Werman algorithm on the input based on:
   *
   *   Domanski, Luke, Pascal Vallotton, and Dadong Wang. "Parallel van Herk/Gil-Werman image
   *   morphology on GPUs using CUDA." GTC 2009 Conference posters. 2009.
   *
   * The output is written transposed for more efficient execution, see the horizontal pass method
   * for more information. The template argument IsDilate decides if dilation or erosion will be
   * performed. */
  template<bool IsDilate> void execute_step_pass_cpu(const Result &input, Result &output)
  {
    const float limit = IsDilate ? std::numeric_limits<float>::lowest() :
                                   std::numeric_limits<float>::max();
    const auto morphology_operator = [](const float a, const float b) {
      if constexpr (IsDilate) {
        return math::max(a, b);
      }
      else {
        return math::min(a, b);
      }
    };

    /* Notice that the domain is transposed, see the note on the horizontal pass method for more
     * information on the reasoning behind this. */
    const int2 image_size = int2(output.domain().size.y, output.domain().size.x);

    /* We process rows in tiles whose size is the same as the structuring element size. So we
     * compute the number of tiles using ceiling division, noting that the last tile might not be
     * complete. */
    const int size = this->get_structuring_element_size();
    const int tiles_count = int(math::ceil(float(image_size.x) / size));

    /* Process along rows in parallel. */
    threading::parallel_for(IndexRange(image_size.y), 1, [&](const IndexRange sub_y_range) {
      Array<float> prefix_table(size);
      Array<float> suffix_table(size);
      for (const int64_t y : sub_y_range) {
        for (const int64_t tile_index : IndexRange(tiles_count)) {
          const int64_t tile_start = tile_index * size;
          /* Compute the x texel location of the pixel at the center of the tile. Noting that the
           * size of the structuring element is guaranteed to be odd. */
          const int64_t tile_center = tile_start + size / 2;

          float prefix_value = limit;
          float suffix_value = limit;
          /* Starting from the pixel at the center of the tile, recursively compute the prefix
           * table to the right and the suffix table to the left by applying the morphology
           * operator. */
          for (const int64_t i : IndexRange(size)) {
            const float right_value = input.load_pixel_fallback(int2(tile_center + i, y), limit);
            prefix_value = morphology_operator(prefix_value, right_value);
            prefix_table[i] = prefix_value;

            /* Note that we access pixels increasingly to the left, so invert the suffix table when
             * writing to it. */
            const float left_value = input.load_pixel_fallback(int2(tile_center - i, y), limit);
            suffix_value = morphology_operator(suffix_value, left_value);
            suffix_table[size - 1 - i] = suffix_value;
          }

          const IndexRange tile_range = IndexRange(tile_start, size);
          const IndexRange safe_tile_range = tile_range.intersect(IndexRange(image_size.x));
          /* For each pixel in the tile, write the result of applying the morphology operator on
           * the prefix and suffix values. */
          for (const int64_t x : safe_tile_range) {
            /* Compute the local table index, since the prefix and suffix tables are local to each
             * tile. */
            const int64_t table_index = x - tile_start;
            const float prefix_value = prefix_table[table_index];
            const float suffix_value = suffix_table[table_index];

            const float value = morphology_operator(prefix_value, suffix_value);

            /* Write the value using the transposed texel. See the horizontal pass method for more
             * information on the rational behind this. */
            output.store_pixel(int2(y, x), value);
          }
        }
      }
    });
  }

  /* --------------------------------
   * Distance Morphological Operator.
   * -------------------------------- */

  void execute_distance()
  {
    morphological_distance(context(), get_input("Mask"), get_result("Mask"), this->get_size());
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

    /* For configurations where there is little user-specified falloff size, anti-alias the result
     * for smoother edges. */
    Result &output = this->get_result("Mask");
    if (this->get_falloff_size() < 2.0f) {
      smaa(this->context(), output_mask, output);
      output_mask.release();
    }
    else {
      output.steal_data(output_mask);
    }
  }

  void execute_distance_threshold_gpu(Result &output)
  {
    gpu::Shader *shader = context().get_shader("compositor_morphological_distance_threshold");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "inset", math::max(this->get_falloff_size(), 10e-6f));
    GPU_shader_uniform_1i(shader, "radius", get_morphological_distance_threshold_radius());
    GPU_shader_uniform_1i(shader, "distance", this->get_size());

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

    const int2 image_size = input.domain().size;

    const float inset = math::max(this->get_falloff_size(), 10e-6f);
    const int radius = this->get_morphological_distance_threshold_radius();
    const int distance = this->get_size();

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
       * hard-coded at 0.5. The pixels with values larger than the threshold are said to be
       * masked. */
      bool is_center_masked = input.load_pixel<float>(texel) > 0.5f;

      /* Since the distance search window is limited to the given radius, the maximum possible
       * squared distance to the center is double the squared radius. */
      int minimum_squared_distance = radius * radius * 2;

      /* Compute the start and end bounds of the window such that no out-of-bounds processing
       * happen in the loops. */
      const int2 start = math::max(texel - radius, int2(0)) - texel;
      const int2 end = math::min(texel + radius + 1, image_size) - texel;

      /* Find the squared distance to the nearest different pixel in the search window of the given
       * radius. */
      for (int y = start.y; y < end.y; y++) {
        const int yy = y * y;
        for (int x = start.x; x < end.x; x++) {
          bool is_sample_masked = input.load_pixel<float>(texel + int2(x, y)) > 0.5f;
          if (is_center_masked != is_sample_masked) {
            minimum_squared_distance = math::min(minimum_squared_distance, x * x + yy);
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

      output.store_pixel(texel, value);
    });
  }

  /* See the discussion in the implementation for more information. */
  int get_morphological_distance_threshold_radius()
  {
    return int(math::ceil(this->get_falloff_size())) + math::abs(this->get_size());
  }

  /* ----------------------------------------
   * Distance Feather Morphological Operator.
   * ---------------------------------------- */

  void execute_distance_feather()
  {
    morphological_distance_feather(
        context(), get_input("Mask"), get_result("Mask"), this->get_size(), this->get_falloff());
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

    if (this->get_type() == CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD &&
        this->get_falloff_size() != 0.0f)
    {
      return false;
    }

    if (this->get_size() == 0) {
      return true;
    }

    return false;
  }

  /* Gets the size of the structuring element. See the get_size method for more information. */
  int get_structuring_element_size()
  {
    return math::abs(this->get_size()) * 2 + 1;
  }

  /* Returns true if dilation should be performed, as opposed to erosion. See the get_size()
   * method for more information. */
  bool is_dilation()
  {
    return this->get_size() > 0;
  }

  /* The signed radius of the structuring element, that is, half the structuring element size. The
   * sign indicates either dilation or erosion, where negative values means erosion. */
  int get_size()
  {
    return this->get_input("Size").get_single_value_default(0);
  }

  float get_falloff_size()
  {
    return math::max(0.0f, this->get_input("Falloff Size").get_single_value_default(0.0f));
  }

  CMPNodeDilateErodeMethod get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_DILATE_ERODE_STEP);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeDilateErodeMethod>(menu_value.value);
  }

  int get_falloff()
  {
    const Result &input = this->get_input("Falloff");
    const MenuValue default_menu_value = MenuValue(PROP_SMOOTH);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return menu_value.value;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DilateErodeOperation(context, node);
}

}  // namespace blender::nodes::node_composite_dilate_cc

static void register_node_type_cmp_dilateerode()
{
  namespace file_ns = blender::nodes::node_composite_dilate_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDilateErode", CMP_NODE_DILATEERODE);
  ntype.ui_name = "Dilate/Erode";
  ntype.ui_description = "Expand and shrink masks";
  ntype.enum_name_legacy = "DILATEERODE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_dilate_declare;
  ntype.initfunc = file_ns::node_composit_init_dilateerode;
  blender::bke::node_type_storage(
      ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_dilateerode)
