/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <limits>

#include "BLI_math_base.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_summed_area_table.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"

#include "node_composite_util.hh"

/* **************** Kuwahara ******************** */

namespace blender::nodes::node_composite_kuwahara_cc {

NODE_STORAGE_FUNCS(NodeKuwaharaData)

static void cmp_node_kuwahara_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Float>("Size").default_value(6.0f).compositor_domain_priority(1);
  b.add_output<decl::Color>("Image");
}

static void node_composit_init_kuwahara(bNodeTree * /*ntree*/, bNode *node)
{
  NodeKuwaharaData *data = MEM_cnew<NodeKuwaharaData>(__func__);
  node->storage = data;

  /* Set defaults. */
  data->uniformity = 4;
  data->eccentricity = 1.0;
  data->sharpness = 0.5;
}

static void node_composit_buts_kuwahara(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemR(col, ptr, "variation", UI_ITEM_NONE, nullptr, ICON_NONE);

  const int variation = RNA_enum_get(ptr, "variation");

  if (variation == CMP_NODE_KUWAHARA_CLASSIC) {
    uiItemR(col, ptr, "use_high_precision", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else if (variation == CMP_NODE_KUWAHARA_ANISOTROPIC) {
    uiItemR(col, ptr, "uniformity", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "sharpness", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemR(col, ptr, "eccentricity", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
}

using namespace blender::realtime_compositor;

class ConvertKuwaharaOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    if (get_input("Image").is_single_value()) {
      get_input("Image").pass_through(get_result("Image"));
      return;
    }

    if (node_storage(bnode()).variation == CMP_NODE_KUWAHARA_ANISOTROPIC) {
      execute_anisotropic();
    }
    else {
      execute_classic();
    }
  }

  void execute_classic()
  {
    /* For high radii, we accelerate the filter using a summed area table, making the filter
     * execute in constant time as opposed to having quadratic complexity. Except if high precision
     * is enabled, since summed area tables are less precise. */
    Result &size_input = get_input("Size");
    if (!node_storage(bnode()).high_precision &&
        (!size_input.is_single_value() || size_input.get_float_value() > 5.0f))
    {
      this->execute_classic_summed_area_table();
    }
    else {
      this->execute_classic_convolution();
    }
  }

  void execute_classic_convolution()
  {
    if (this->context().use_gpu()) {
      this->execute_classic_convolution_gpu();
    }
    else {
      this->execute_classic_convolution_cpu();
    }
  }

  void execute_classic_convolution_gpu()
  {
    GPUShader *shader = context().get_shader(get_classic_convolution_shader_name());
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1i(shader, "size", int(size_input.get_float_value()));
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_classic_convolution_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_classic_convolution_constant_size";
    }
    return "compositor_kuwahara_classic_convolution_variable_size";
  }

  void execute_classic_convolution_cpu()
  {
    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    this->compute_classic<false>(
        &this->get_input("Image"), nullptr, nullptr, this->get_input("Size"), output, domain.size);
  }

  void execute_classic_summed_area_table()
  {
    Result table = this->context().create_result(ResultType::Color, ResultPrecision::Full);
    summed_area_table(this->context(), this->get_input("Image"), table);

    Result squared_table = this->context().create_result(ResultType::Color, ResultPrecision::Full);
    summed_area_table(this->context(),
                      this->get_input("Image"),
                      squared_table,
                      SummedAreaTableOperation::Square);

    if (this->context().use_gpu()) {
      this->execute_classic_summed_area_table_gpu(table, squared_table);
    }
    else {
      this->execute_classic_summed_area_table_cpu(table, squared_table);
    }

    table.release();
    squared_table.release();
  }

  void execute_classic_summed_area_table_gpu(const Result &table, const Result &squared_table)
  {
    GPUShader *shader = context().get_shader(get_classic_summed_area_table_shader_name());
    GPU_shader_bind(shader);

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1i(shader, "size", int(size_input.get_float_value()));
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    table.bind_as_texture(shader, "table_tx");
    squared_table.bind_as_texture(shader, "squared_table_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    table.unbind_as_texture();
    squared_table.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_classic_summed_area_table_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_classic_summed_area_table_constant_size";
    }
    return "compositor_kuwahara_classic_summed_area_table_variable_size";
  }

  void execute_classic_summed_area_table_cpu(const Result &table, const Result &squared_table)
  {
    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    this->compute_classic<true>(
        nullptr, &table, &squared_table, this->get_input("Size"), output, domain.size);
  }

  /* If UseSummedAreaTable is true, then `table` and `squared_table` should be provided while
   * `input` should be nullptr, otherwise, `input` should be provided while `table` and
   * `squared_table` should be nullptr. */
  template<bool UseSummedAreaTable>
  void compute_classic(const Result *input,
                       const Result *table,
                       const Result *squared_table,
                       const Result &size_input,
                       Result &output,
                       const int2 size)
  {
    parallel_for(size, [&](const int2 texel) {
      int radius = math::max(0, int(size_input.load_pixel(texel).x));

      float4 mean_of_squared_color_of_quadrants[4] = {
          float4(0.0f), float4(0.0f), float4(0.0f), float4(0.0f)};
      float4 mean_of_color_of_quadrants[4] = {
          float4(0.0f), float4(0.0f), float4(0.0f), float4(0.0f)};

      /* Compute the above statistics for each of the quadrants around the current pixel. */
      for (int q = 0; q < 4; q++) {
        /* A fancy expression to compute the sign of the quadrant q. */
        int2 sign = int2((q % 2) * 2 - 1, ((q / 2) * 2 - 1));

        int2 lower_bound = texel - int2(sign.x > 0 ? 0 : radius, sign.y > 0 ? 0 : radius);
        int2 upper_bound = texel + int2(sign.x < 0 ? 0 : radius, sign.y < 0 ? 0 : radius);

        /* Limit the quadrants to the image bounds. */
        int2 image_bound = size - int2(1);
        int2 corrected_lower_bound = math::min(image_bound, math::max(int2(0), lower_bound));
        int2 corrected_upper_bound = math::min(image_bound, math::max(int2(0), upper_bound));
        int2 region_size = corrected_upper_bound - corrected_lower_bound + int2(1);
        int quadrant_pixel_count = region_size.x * region_size.y;

        if constexpr (UseSummedAreaTable) {
          mean_of_color_of_quadrants[q] = summed_area_table_sum(*table, lower_bound, upper_bound);
          mean_of_squared_color_of_quadrants[q] = summed_area_table_sum(
              *squared_table, lower_bound, upper_bound);
        }
        else {
          for (int j = 0; j <= radius; j++) {
            for (int i = 0; i <= radius; i++) {
              float4 color = input->load_pixel_fallback(texel + int2(i, j) * sign, float4(0.0f));
              mean_of_color_of_quadrants[q] += color;
              mean_of_squared_color_of_quadrants[q] += color * color;
            }
          }
        }

        mean_of_color_of_quadrants[q] /= quadrant_pixel_count;
        mean_of_squared_color_of_quadrants[q] /= quadrant_pixel_count;
      }

      /* Find the quadrant which has the minimum variance. */
      float minimum_variance = std::numeric_limits<float>::max();
      float4 mean_color_of_chosen_quadrant = mean_of_color_of_quadrants[0];
      for (int q = 0; q < 4; q++) {
        float4 color_mean = mean_of_color_of_quadrants[q];
        float4 squared_color_mean = mean_of_squared_color_of_quadrants[q];
        float4 color_variance = squared_color_mean - color_mean * color_mean;

        float variance = math::dot(color_variance.xyz(), float3(1.0f));
        if (variance < minimum_variance) {
          minimum_variance = variance;
          mean_color_of_chosen_quadrant = color_mean;
        }
      }

      output.store_pixel(texel, mean_color_of_chosen_quadrant);
    });
  }

  /* An implementation of the Anisotropic Kuwahara filter described in the paper:
   *
   *   Kyprianidis, Jan Eric, Henry Kang, and Jurgen Dollner. "Image and video abstraction by
   *   anisotropic Kuwahara filtering." 2009.
   */
  void execute_anisotropic()
  {
    Result structure_tensor = compute_structure_tensor();
    Result smoothed_structure_tensor = context().create_result(ResultType::Color);
    symmetric_separable_blur(context(),
                             structure_tensor,
                             smoothed_structure_tensor,
                             float2(node_storage(bnode()).uniformity));
    structure_tensor.release();

    GPUShader *shader = context().get_shader(get_anisotropic_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "eccentricity", get_eccentricity());
    GPU_shader_uniform_1f(shader, "sharpness", get_sharpness());

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1f(shader, "size", size_input.get_float_value());
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    smoothed_structure_tensor.bind_as_texture(shader, "structure_tensor_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    smoothed_structure_tensor.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();

    smoothed_structure_tensor.release();
  }

  Result compute_structure_tensor()
  {
    GPUShader *shader = context().get_shader(
        "compositor_kuwahara_anisotropic_compute_structure_tensor");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result structure_tensor = context().create_result(ResultType::Color);
    structure_tensor.allocate_texture(domain);
    structure_tensor.bind_as_image(shader, "structure_tensor_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    structure_tensor.unbind_as_image();
    GPU_shader_unbind();

    return structure_tensor;
  }

  const char *get_anisotropic_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_anisotropic_constant_size";
    }
    return "compositor_kuwahara_anisotropic_variable_size";
  }

  bool is_constant_size()
  {
    return get_input("Size").is_single_value();
  }

  /* The sharpness controls the sharpness of the transitions between the kuwahara sectors, which
   * is controlled by the weighting function pow(standard_deviation, -sharpness) as can be seen
   * in the shader. The transition is completely smooth when the sharpness is zero and completely
   * sharp when it is infinity. But realistically, the sharpness doesn't change much beyond the
   * value of 16 due to its exponential nature, so we just assume a maximum sharpness of 16.
   *
   * The stored sharpness is in the range [0, 1], so we multiply by 16 to get it in the range
   * [0, 16], however, we also square it before multiplication to slow down the rate of change
   * near zero to counter its exponential nature for more intuitive user control. */
  float get_sharpness()
  {
    const float sharpness_factor = node_storage(bnode()).sharpness;
    return sharpness_factor * sharpness_factor * 16.0f;
  }

  /* The eccentricity controls how much the image anisotropy affects the eccentricity of the
   * kuwahara sectors, which is controlled by the following factor that gets multiplied to the
   * radius to get the ellipse width and divides the radius to get the ellipse height:
   *
   *   (eccentricity + anisotropy) / eccentricity
   *
   * Since the anisotropy is in the [0, 1] range, the factor tends to 1 as the eccentricity tends
   * to infinity and tends to infinity when the eccentricity tends to zero. The stored
   * eccentricity is in the range [0, 2], we map that to the range [infinity, 0.5] by taking the
   * reciprocal, satisfying the aforementioned limits. The upper limit doubles the computed
   * default eccentricity, which users can use to enhance the directionality of the filter.
   * Instead of actual infinity, we just use an eccentricity of 1 / 0.01 since the result is very
   * similar to that of infinity. */
  float get_eccentricity()
  {
    return 1.0f / math::max(0.01f, node_storage(bnode()).eccentricity);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ConvertKuwaharaOperation(context, node);
}

}  // namespace blender::nodes::node_composite_kuwahara_cc

void register_node_type_cmp_kuwahara()
{
  namespace file_ns = blender::nodes::node_composite_kuwahara_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_KUWAHARA, "Kuwahara", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_kuwahara_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_kuwahara;
  ntype.initfunc = file_ns::node_composit_init_kuwahara;
  blender::bke::node_type_storage(
      &ntype, "NodeKuwaharaData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
