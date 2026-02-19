/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <limits>

#include "BLI_index_range.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_task.hh"

#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "GPU_shader.hh"

#include "COM_algorithm_jump_flooding.hh"
#include "COM_algorithm_morphological_distance.hh"
#include "COM_algorithm_morphological_distance_feather.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_dilate_erode_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_DILATE_ERODE_STEP, "STEP", 0, N_("Steps"), ""},
    {CMP_NODE_DILATE_ERODE_DISTANCE_THRESHOLD, "THRESHOLD", 0, N_("Threshold"), ""},
    {CMP_NODE_DILATE_ERODE_DISTANCE, "DISTANCE", 0, N_("Distance"), ""},
    {CMP_NODE_DILATE_ERODE_DISTANCE_FEATHER, "FEATHER", 0, N_("Feather"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
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

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused but kept for forward compatibility. */
  NodeDilateErode *data = MEM_new<NodeDilateErode>(__func__);
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
    const int2 transposed_domain = int2(domain.data_size.y, domain.data_size.x);

    Result horizontal_pass_result = context().create_result(ResultType::Float);
    horizontal_pass_result.allocate_texture(transposed_domain);
    horizontal_pass_result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.data_size);

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
    const int2 transposed_domain = int2(domain.data_size.y, domain.data_size.x);

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
    compute_dispatch_threads_at_least(shader, int2(domain.data_size.y, domain.data_size.x));

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
    const int2 image_size = int2(output.domain().data_size.y, output.domain().data_size.x);

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
    Result masked_pixels = this->context().create_result(ResultType::Int2, ResultPrecision::Half);
    Result unmasked_pixels = this->context().create_result(ResultType::Int2,
                                                           ResultPrecision::Half);
    this->compute_distance_threshold_seeds(masked_pixels, unmasked_pixels);

    Result flooded_masked_pixels = this->context().create_result(ResultType::Int2,
                                                                 ResultPrecision::Half);
    Result flooded_unmasked_pixels = this->context().create_result(ResultType::Int2,
                                                                   ResultPrecision::Half);
    jump_flooding(this->context(), masked_pixels, flooded_masked_pixels);
    masked_pixels.release();
    jump_flooding(this->context(), unmasked_pixels, flooded_unmasked_pixels);
    unmasked_pixels.release();

    this->compute_distance_threshold(flooded_masked_pixels, flooded_unmasked_pixels);
    flooded_masked_pixels.release();
    flooded_unmasked_pixels.release();
  }

  /* Compute an image that marks both masked and unmasked pixels as seed pixels for the jump
   * flooding algorithm. */
  void compute_distance_threshold_seeds(Result &masked_pixels, Result &unmasked_pixels)
  {
    if (this->context().use_gpu()) {
      this->compute_distance_threshold_seeds_gpu(masked_pixels, unmasked_pixels);
      return;
    }

    this->compute_distance_threshold_seeds_cpu(masked_pixels, unmasked_pixels);
  }

  void compute_distance_threshold_seeds_gpu(Result &masked_pixels, Result &unmasked_pixels)
  {
    gpu::Shader *shader = this->context().get_shader(
        "compositor_morphological_distance_threshold_seeds", ResultPrecision::Half);
    GPU_shader_bind(shader);

    const Result &mask = this->get_input("Mask");
    mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = mask.domain();
    masked_pixels.allocate_texture(domain);
    masked_pixels.bind_as_image(shader, "masked_pixels_img");
    unmasked_pixels.allocate_texture(domain);
    unmasked_pixels.bind_as_image(shader, "unmasked_pixels_img");

    compute_dispatch_threads_at_least(shader, domain.data_size);

    mask.unbind_as_texture();
    masked_pixels.unbind_as_image();
    unmasked_pixels.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_distance_threshold_seeds_cpu(Result &masked_pixels, Result &unmasked_pixels)
  {
    const Result &mask = this->get_input("Mask");

    const Domain domain = mask.domain();
    masked_pixels.allocate_texture(domain);
    unmasked_pixels.allocate_texture(domain);

    parallel_for(domain.data_size, [&](const int2 texel) {
      const bool is_masked = mask.load_pixel<float>(texel) > 0.5f;

      const int2 masked_jump_flooding_value = initialize_jump_flooding_value(texel, is_masked);
      masked_pixels.store_pixel(texel, masked_jump_flooding_value);

      const int2 unmasked_jump_flooding_value = initialize_jump_flooding_value(texel, !is_masked);
      unmasked_pixels.store_pixel(texel, unmasked_jump_flooding_value);
    });
  }

  void compute_distance_threshold(const Result &flooded_masked_pixels,
                                  const Result &flooded_unmasked_pixels)
  {
    if (this->context().use_gpu()) {
      this->compute_distance_threshold_gpu(flooded_masked_pixels, flooded_unmasked_pixels);
      return;
    }

    this->compute_distance_threshold_cpu(flooded_masked_pixels, flooded_unmasked_pixels);
  }

  void compute_distance_threshold_gpu(const Result &flooded_masked_pixels,
                                      const Result &flooded_unmasked_pixels)
  {
    gpu::Shader *shader = this->context().get_shader(
        "compositor_morphological_distance_threshold");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "distance_offset", this->get_size());
    GPU_shader_uniform_1f(shader, "falloff_size", this->get_falloff_size());

    const Result &input_mask = this->get_input("Mask");
    input_mask.bind_as_texture(shader, "mask_tx");

    flooded_masked_pixels.bind_as_texture(shader, "flooded_masked_pixels_tx");
    flooded_unmasked_pixels.bind_as_texture(shader, "flooded_unmasked_pixels_tx");

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Mask");
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.data_size);

    GPU_shader_unbind();
    output.unbind_as_image();
    input_mask.unbind_as_texture();
  }

  void compute_distance_threshold_cpu(const Result &flooded_masked_pixels,
                                      const Result &flooded_unmasked_pixels)
  {
    const Result &mask = this->get_input("Mask");

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Mask");
    output.allocate_texture(domain);

    const float falloff_size = this->get_falloff_size();
    const int distance_offset = this->get_size();

    parallel_for(domain.data_size, [&](const int2 texel) {
      const bool is_masked = mask.load_pixel<float>(texel) > 0.5f;
      const int2 closest_masked_texel = flooded_masked_pixels.load_pixel<int2>(texel);
      const int2 closest_unmasked_texel = flooded_unmasked_pixels.load_pixel<int2>(texel);
      const int2 closest_different_texel = is_masked ? closest_unmasked_texel :
                                                       closest_masked_texel;
      const float distance_to_different = math::distance(float2(texel),
                                                         float2(closest_different_texel));
      const float signed_distance = is_masked ? distance_to_different : -distance_to_different;
      const float value = math::clamp(
          (signed_distance + distance_offset) / falloff_size, 0.0f, 1.0f);

      output.store_pixel(texel, value);
    });
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
    return this->get_input("Size").get_single_value_default<int>();
  }

  float get_falloff_size()
  {
    return math::max(0.0f, this->get_input("Falloff Size").get_single_value_default<float>());
  }

  CMPNodeDilateErodeMethod get_type()
  {
    return CMPNodeDilateErodeMethod(
        this->get_input("Type").get_single_value_default<MenuValue>().value);
  }

  int get_falloff()
  {
    return this->get_input("Falloff").get_single_value_default<MenuValue>().value;
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new DilateErodeOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeDilateErode", CMP_NODE_DILATEERODE);
  ntype.ui_name = "Dilate/Erode";
  ntype.ui_description = "Expand and shrink masks";
  ntype.enum_name_legacy = "DILATEERODE";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  bke::node_type_storage(
      ntype, "NodeDilateErode", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_dilate_erode_cc
