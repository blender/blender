/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include <limits>

#include "BLI_math_base.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_numbers.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_types.hh"

#include "RNA_types.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "COM_algorithm_summed_area_table.hh"
#include "COM_algorithm_symmetric_separable_blur.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_kuwahara_cc {

static const EnumPropertyItem type_items[] = {
    {CMP_NODE_KUWAHARA_CLASSIC,
     "CLASSIC",
     0,
     N_("Classic"),
     N_("Fast but less accurate variation")},
    {CMP_NODE_KUWAHARA_ANISOTROPIC,
     "ANISOTROPIC",
     0,
     N_("Anisotropic"),
     N_("Accurate but slower variation")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void cmp_node_kuwahara_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Float>("Size")
      .default_value(6.0f)
      .min(0.0f)
      .description("The size of the filter in pixels")
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Menu>("Type")
      .default_value(CMP_NODE_KUWAHARA_ANISOTROPIC)
      .static_items(type_items)
      .optional_label();

  b.add_input<decl::Int>("Uniformity")
      .default_value(4)
      .min(0)
      .usage_by_single_menu(CMP_NODE_KUWAHARA_ANISOTROPIC)
      .description(
          "Controls the uniformity of the direction of the filter. Higher values produces more "
          "uniform directions");
  b.add_input<decl::Float>("Sharpness")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .usage_by_single_menu(CMP_NODE_KUWAHARA_ANISOTROPIC)
      .description(
          "Controls the sharpness of the filter. 0 means completely smooth while 1 means "
          "completely sharp");
  b.add_input<decl::Float>("Eccentricity")
      .default_value(1.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(2.0f)
      .usage_by_single_menu(CMP_NODE_KUWAHARA_ANISOTROPIC)
      .description(
          "Controls how directional the filter is. 0 means the filter is completely "
          "omnidirectional while 2 means it is maximally directed along the edges of the image");
  b.add_input<decl::Bool>("High Precision")
      .default_value(false)
      .usage_by_single_menu(CMP_NODE_KUWAHARA_CLASSIC)
      .description(
          "Uses a more precise but slower method. Use if the output contains undesirable noise.");
}

static void node_composit_init_kuwahara(bNodeTree * /*ntree*/, bNode *node)
{
  /* Unused, kept for forward compatibility. */
  NodeKuwaharaData *data = MEM_callocN<NodeKuwaharaData>(__func__);
  node->storage = data;
}

using namespace blender::compositor;

class ConvertKuwaharaOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Result &input = this->get_input("Image");
    if (input.is_single_value()) {
      Result &output = this->get_result("Image");
      output.share_data(input);
      return;
    }

    if (this->get_type() == CMP_NODE_KUWAHARA_ANISOTROPIC) {
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
    if (!this->get_high_precision() &&
        (!size_input.is_single_value() || size_input.get_single_value<float>() > 5.0f))
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
    gpu::Shader *shader = context().get_shader(get_classic_convolution_shader_name());
    GPU_shader_bind(shader);

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1i(shader, "size", int(size_input.get_single_value<float>()));
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
    gpu::Shader *shader = context().get_shader(get_classic_summed_area_table_shader_name());
    GPU_shader_bind(shader);

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1i(shader, "size", int(size_input.get_single_value<float>()));
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
      int radius = math::max(0, int(size_input.load_pixel<float, true>(texel)));

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
              float4 color = float4(input->load_pixel_zero<Color>(texel + int2(i, j) * sign));
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

      output.store_pixel(texel, Color(mean_color_of_chosen_quadrant));
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
    Result smoothed_structure_tensor = context().create_result(ResultType::Float4);
    symmetric_separable_blur(
        context(), structure_tensor, smoothed_structure_tensor, float2(this->get_uniformity()));
    structure_tensor.release();

    if (this->context().use_gpu()) {
      this->execute_anisotropic_gpu(smoothed_structure_tensor);
    }
    else {
      this->execute_anisotropic_cpu(smoothed_structure_tensor);
    }

    smoothed_structure_tensor.release();
  }

  void execute_anisotropic_gpu(const Result &structure_tensor)
  {
    gpu::Shader *shader = context().get_shader(get_anisotropic_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_1f(shader, "eccentricity", this->compute_eccentricity());
    GPU_shader_uniform_1f(shader, "sharpness", this->compute_sharpness());

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result &size_input = get_input("Size");
    if (size_input.is_single_value()) {
      GPU_shader_uniform_1f(shader, "size", size_input.get_single_value<float>());
    }
    else {
      size_input.bind_as_texture(shader, "size_tx");
    }

    structure_tensor.bind_as_texture(shader, "structure_tensor_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    structure_tensor.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  const char *get_anisotropic_shader_name()
  {
    if (is_constant_size()) {
      return "compositor_kuwahara_anisotropic_constant_size";
    }
    return "compositor_kuwahara_anisotropic_variable_size";
  }

  void execute_anisotropic_cpu(const Result &structure_tensor)
  {
    const float eccentricity = this->compute_eccentricity();
    const float sharpness = this->compute_sharpness();

    Result &input = this->get_input("Image");
    Result &size = this->get_input("Size");

    const Domain domain = this->compute_domain();
    Result &output = this->get_result("Image");
    output.allocate_texture(domain);

    /* An implementation of the Anisotropic Kuwahara filter described in the paper:
     *
     *   Kyprianidis, Jan Eric, Henry Kang, and Jurgen Dollner. "Image and video abstraction by
     *   anisotropic Kuwahara filtering." 2009.
     *
     * But with the polynomial weighting functions described in the paper:
     *
     *   Kyprianidis, Jan Eric, et al. "Anisotropic Kuwahara Filtering with Polynomial Weighting
     *   Functions." 2010.
     *
     * And the sector weight function described in the paper:
     *
     *  Kyprianidis, Jan Eric. "Image and video abstraction by multi-scale anisotropic Kuwahara
     *  filtering." 2011.
     */

    parallel_for(domain.size, [&](const int2 texel) {
      /* The structure tensor is encoded in a float4 using a column major storage order, as can be
       * seen in the compute_structure_tensor_cpu method. */
      float4 encoded_structure_tensor = structure_tensor.load_pixel<float4>(texel);
      float dxdx = encoded_structure_tensor.x;
      float dxdy = encoded_structure_tensor.y;
      float dydy = encoded_structure_tensor.w;

      /* Compute the first and second eigenvalues of the structure tensor using the equations in
       * section "3.1 Orientation and Anisotropy Estimation" of the paper. */
      float eigenvalue_first_term = (dxdx + dydy) / 2.0f;
      float eigenvalue_square_root_term = math::sqrt(math::square(dxdx - dydy) +
                                                     4.0f * math::square(dxdy)) /
                                          2.0f;
      float first_eigenvalue = eigenvalue_first_term + eigenvalue_square_root_term;
      float second_eigenvalue = eigenvalue_first_term - eigenvalue_square_root_term;

      /* Compute the normalized eigenvector of the structure tensor oriented in direction of the
       * minimum rate of change using the equations in section "3.1 Orientation and Anisotropy
       * Estimation" of the paper. */
      float2 eigenvector = float2(first_eigenvalue - dxdx, -dxdy);
      float eigenvector_length = math::length(eigenvector);
      float2 unit_eigenvector = eigenvector_length != 0.0f ? eigenvector / eigenvector_length :
                                                             float2(1.0f);

      /* Compute the amount of anisotropy using equations in section "3.1 Orientation and
       * Anisotropy Estimation" of the paper. The anisotropy ranges from 0 to 1, where 0
       * corresponds to isotropic and 1 corresponds to entirely anisotropic regions. */
      float eigenvalue_sum = first_eigenvalue + second_eigenvalue;
      float eigenvalue_difference = first_eigenvalue - second_eigenvalue;
      float anisotropy = eigenvalue_sum > 0.0f ? eigenvalue_difference / eigenvalue_sum : 0.0f;

      float radius = math::max(0.0f, size.load_pixel<float, true>(texel));
      if (radius == 0.0f) {
        output.store_pixel(texel, input.load_pixel<Color>(texel));
        return;
      }

      /* Compute the width and height of an ellipse that is more width-elongated for high
       * anisotropy and more circular for low anisotropy, controlled using the eccentricity factor.
       * Since the anisotropy is in the [0, 1] range, the width factor tends to 1 as the
       * eccentricity tends to infinity and tends to infinity when the eccentricity tends to zero.
       * This is based on the equations in section "3.2. Anisotropic Kuwahara Filtering" of the
       * paper. */
      float ellipse_width_factor = (eccentricity + anisotropy) / eccentricity;
      float ellipse_width = ellipse_width_factor * radius;
      float ellipse_height = radius / ellipse_width_factor;

      /* Compute the cosine and sine of the angle that the eigenvector makes with the x axis. Since
       * the eigenvector is normalized, its x and y components are the cosine and sine of the angle
       * it makes with the x axis. */
      float cosine = unit_eigenvector.x;
      float sine = unit_eigenvector.y;

      /* Compute an inverse transformation matrix that represents an ellipse of the given width and
       * height and makes and an angle with the x axis of the given cosine and sine. This is an
       * inverse matrix, so it transforms the ellipse into a disk of unit radius. */
      float2x2 inverse_ellipse_matrix = float2x2(
          float2(cosine / ellipse_width, -sine / ellipse_height),
          float2(sine / ellipse_width, cosine / ellipse_height));

      /* Compute the bounding box of a zero centered ellipse whose major axis is aligned with the
       * eigenvector and has the given width and height. This is based on the equations described
       * in:
       *
       *   https://iquilezles.org/articles/ellipses/
       *
       * Notice that we only compute the upper bound, the lower bound is just negative that since
       * the ellipse is zero centered. Also notice that we take the ceiling of the bounding box,
       * just to ensure the filter window is at least 1x1. */
      float2 ellipse_major_axis = ellipse_width * unit_eigenvector;
      float2 ellipse_minor_axis = ellipse_height * float2(unit_eigenvector.y, unit_eigenvector.x) *
                                  float2(-1, 1);
      int2 ellipse_bounds = int2(math::ceil(
          math::sqrt(math::square(ellipse_major_axis) + math::square(ellipse_minor_axis))));

      /* Compute the overlap polynomial parameters for 8-sector ellipse based on the equations in
       * section "3 Alternative Weighting Functions" of the polynomial weights paper. More on this
       * later in the code. */
      const int number_of_sectors = 8;
      float sector_center_overlap_parameter = 2.0f / radius;
      float sector_envelope_angle = ((3.0f / 2.0f) * math::numbers::pi_v<float>) /
                                    number_of_sectors;
      float cross_sector_overlap_parameter = (sector_center_overlap_parameter +
                                              math::cos(sector_envelope_angle)) /
                                             math::square(math::sin(sector_envelope_angle));

      /* We need to compute the weighted mean of color and squared color of each of the 8 sectors
       * of the ellipse, so we declare arrays for accumulating those and initialize them in the
       * next code section. */
      float4 weighted_mean_of_squared_color_of_sectors[8];
      float4 weighted_mean_of_color_of_sectors[8];
      float sum_of_weights_of_sectors[8];

      /* The center pixel (0, 0) is exempt from the main loop below for reasons that are explained
       * in the first if statement in the loop, so we need to accumulate its color, squared color,
       * and weight separately first. Luckily, the zero coordinates of the center pixel zeros out
       * most of the complex computations below, and it can easily be shown that the weight for the
       * center pixel in all sectors is simply (1 / number_of_sectors). */
      float4 center_color = float4(input.load_pixel<Color>(texel));
      float4 center_color_squared = center_color * center_color;
      float center_weight = 1.0f / number_of_sectors;
      float4 weighted_center_color = center_color * center_weight;
      float4 weighted_center_color_squared = center_color_squared * center_weight;
      for (int i = 0; i < number_of_sectors; i++) {
        weighted_mean_of_squared_color_of_sectors[i] = weighted_center_color_squared;
        weighted_mean_of_color_of_sectors[i] = weighted_center_color;
        sum_of_weights_of_sectors[i] = center_weight;
      }

      /* Loop over the window of pixels inside the bounding box of the ellipse. However, we utilize
       * the fact that ellipses are mirror symmetric along the horizontal axis, so we reduce the
       * window to only the upper two quadrants, and compute each two mirrored pixels at the same
       * time using the same weight as an optimization. */
      for (int j = 0; j <= ellipse_bounds.y; j++) {
        for (int i = -ellipse_bounds.x; i <= ellipse_bounds.x; i++) {
          /* Since we compute each two mirrored pixels at the same time, we need to also exempt the
           * pixels whose x coordinates are negative and their y coordinates are zero, that's
           * because those are mirrored versions of the pixels whose x coordinates are positive and
           * their y coordinates are zero, and we don't want to compute and accumulate them twice.
           * Moreover, we also need to exempt the center pixel with zero coordinates for the same
           * reason, however, since the mirror of the center pixel is itself, it need to be
           * accumulated separately, hence why we did that in the code section just before this
           * loop. */
          if (j == 0 && i <= 0) {
            continue;
          }

          /* Map the pixels of the ellipse into a unit disk, exempting any points that are not part
           * of the ellipse or disk. */
          float2 disk_point = inverse_ellipse_matrix * float2(i, j);
          float disk_point_length_squared = math::dot(disk_point, disk_point);
          if (disk_point_length_squared > 1.0f) {
            continue;
          }

          /* While each pixel belongs to a single sector in the ellipse, we expand the definition
           * of a sector a bit to also overlap with other sectors as illustrated in Figure 8 of the
           * polynomial weights paper. So each pixel may contribute to multiple sectors, and thus
           * we compute its weight in each of the 8 sectors. */
          float sector_weights[8];

          /* We evaluate the weighting polynomial at each of the 8 sectors by rotating the disk
           * point by 45 degrees and evaluating the weighting polynomial at each incremental
           * rotation. To avoid potentially expensive rotations, we utilize the fact that rotations
           * by 90 degrees are simply swapping of the coordinates and negating the x component. We
           * also note that since the y term of the weighting polynomial is squared, it is not
           * affected by the sign and can be computed once for the x and once for the y
           * coordinates. So we compute every other even-indexed 4 weights by successive 90 degree
           * rotations as discussed. */
          float2 polynomial = sector_center_overlap_parameter -
                              cross_sector_overlap_parameter * math::square(disk_point);
          sector_weights[0] = math::square(math::max(0.0f, disk_point.y + polynomial.x));
          sector_weights[2] = math::square(math::max(0.0f, -disk_point.x + polynomial.y));
          sector_weights[4] = math::square(math::max(0.0f, -disk_point.y + polynomial.x));
          sector_weights[6] = math::square(math::max(0.0f, disk_point.x + polynomial.y));

          /* Then we rotate the disk point by 45 degrees, which is a simple expression involving a
           * constant as can be demonstrated by applying a 45 degree rotation matrix. */
          float2 rotated_disk_point = (1.0f / math::numbers::sqrt2) *
                                      float2(disk_point.x - disk_point.y,
                                             disk_point.x + disk_point.y);

          /* Finally, we compute every other odd-index 4 weights starting from the 45 degrees
           * rotated disk point. */
          float2 rotated_polynomial = sector_center_overlap_parameter -
                                      cross_sector_overlap_parameter *
                                          math::square(rotated_disk_point);
          sector_weights[1] = math::square(
              math::max(0.0f, rotated_disk_point.y + rotated_polynomial.x));
          sector_weights[3] = math::square(
              math::max(0.0f, -rotated_disk_point.x + rotated_polynomial.y));
          sector_weights[5] = math::square(
              math::max(0.0f, -rotated_disk_point.y + rotated_polynomial.x));
          sector_weights[7] = math::square(
              math::max(0.0f, rotated_disk_point.x + rotated_polynomial.y));

          /* We compute a radial Gaussian weighting component such that pixels further away from
           * the sector center gets attenuated, and we also divide by the sum of sector weights to
           * normalize them, since the radial weight will eventually be multiplied to the sector
           * weight below. */
          float sector_weights_sum = sector_weights[0] + sector_weights[1] + sector_weights[2] +
                                     sector_weights[3] + sector_weights[4] + sector_weights[5] +
                                     sector_weights[6] + sector_weights[7];
          float radial_gaussian_weight = math::exp(-math::numbers::pi *
                                                   disk_point_length_squared) /
                                         sector_weights_sum;

          /* Load the color of the pixel and its mirrored pixel and compute their square. */
          float4 upper_color = float4(input.load_pixel_extended<Color>(texel + int2(i, j)));
          float4 lower_color = float4(input.load_pixel_extended<Color>(texel - int2(i, j)));
          float4 upper_color_squared = upper_color * upper_color;
          float4 lower_color_squared = lower_color * lower_color;

          for (int k = 0; k < number_of_sectors; k++) {
            float weight = sector_weights[k] * radial_gaussian_weight;

            /* Accumulate the pixel to each of the sectors multiplied by the sector weight. */
            int upper_index = k;
            sum_of_weights_of_sectors[upper_index] += weight;
            weighted_mean_of_color_of_sectors[upper_index] += upper_color * weight;
            weighted_mean_of_squared_color_of_sectors[upper_index] += upper_color_squared * weight;

            /* Accumulate the mirrored pixel to each of the sectors multiplied by the sector
             * weight. */
            int lower_index = (k + number_of_sectors / 2) % number_of_sectors;
            sum_of_weights_of_sectors[lower_index] += weight;
            weighted_mean_of_color_of_sectors[lower_index] += lower_color * weight;
            weighted_mean_of_squared_color_of_sectors[lower_index] += lower_color_squared * weight;
          }
        }
      }

      /* Compute the weighted sum of mean of sectors, such that sectors with lower standard
       * deviation gets more significant weight than sectors with higher standard deviation. */
      float sum_of_weights = 0.0f;
      float4 weighted_sum = float4(0.0f);
      for (int i = 0; i < number_of_sectors; i++) {
        weighted_mean_of_color_of_sectors[i] /= sum_of_weights_of_sectors[i];
        weighted_mean_of_squared_color_of_sectors[i] /= sum_of_weights_of_sectors[i];

        float4 color_mean = weighted_mean_of_color_of_sectors[i];
        float4 squared_color_mean = weighted_mean_of_squared_color_of_sectors[i];
        float4 color_variance = math::abs(squared_color_mean - color_mean * color_mean);

        float standard_deviation = math::dot(math::sqrt(color_variance.xyz()), float3(1.0f));

        /* Compute the sector weight based on the weight function introduced in section "3.3.1
         * Single-scale Filtering" of the multi-scale paper. Use a threshold of 0.02 to avoid zero
         * division and avoid artifacts in homogeneous regions as demonstrated in the paper. */
        float weight = 1.0f / math::pow(math::max(0.02f, standard_deviation), sharpness);

        sum_of_weights += weight;
        weighted_sum += color_mean * weight;
      }

      /* Fall back to the original color if all sector weights are zero due to very high standard
       * deviation and sharpness. */
      if (sum_of_weights == 0.0f) {
        weighted_sum = center_color;
      }
      else {
        weighted_sum /= sum_of_weights;
      }

      output.store_pixel(texel, Color(weighted_sum));
    });
  }

  Result compute_structure_tensor()
  {
    if (this->context().use_gpu()) {
      return this->compute_structure_tensor_gpu();
    }
    return this->compute_structure_tensor_cpu();
  }

  Result compute_structure_tensor_gpu()
  {
    gpu::Shader *shader = context().get_shader(
        "compositor_kuwahara_anisotropic_compute_structure_tensor");
    GPU_shader_bind(shader);

    Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    const Domain domain = compute_domain();
    Result structure_tensor = context().create_result(ResultType::Float4);
    structure_tensor.allocate_texture(domain);
    structure_tensor.bind_as_image(shader, "structure_tensor_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    structure_tensor.unbind_as_image();
    GPU_shader_unbind();

    return structure_tensor;
  }

  Result compute_structure_tensor_cpu()
  {
    const Result &input = this->get_input("Image");

    const Domain domain = this->compute_domain();
    Result structure_tensor_image = context().create_result(ResultType::Float4);
    structure_tensor_image.allocate_texture(domain);

    /* Computes the structure tensor of the image using a Dirac delta window function as described
     * in section "3.2 Local Structure Estimation" of the paper:
     *
     *   Kyprianidis, Jan Eric. "Image and video abstraction by multi-scale anisotropic Kuwahara
     *   filtering." 2011.
     *
     * The structure tensor should then be smoothed using a Gaussian function to eliminate high
     * frequency details. */
    parallel_for(domain.size, [&](const int2 texel) {
      /* The weight kernels of the filter optimized for rotational symmetry described in section
       * "3.2.1 Gradient Calculation". */
      const float corner_weight = 0.182f;
      const float center_weight = 1.0f - 2.0f * corner_weight;

      float3 x_partial_derivative =
          float4(input.load_pixel_extended<Color>(texel + int2(-1, 1))).xyz() * -corner_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(-1, 0))).xyz() * -center_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(-1, -1))).xyz() * -corner_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(1, 1))).xyz() * corner_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(1, 0))).xyz() * center_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(1, -1))).xyz() * corner_weight;

      float3 y_partial_derivative =
          float4(input.load_pixel_extended<Color>(texel + int2(-1, 1))).xyz() * corner_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(0, 1))).xyz() * center_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(1, 1))).xyz() * corner_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(-1, -1))).xyz() * -corner_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(0, -1))).xyz() * -center_weight +
          float4(input.load_pixel_extended<Color>(texel + int2(1, -1))).xyz() * -corner_weight;

      float dxdx = math::dot(x_partial_derivative, x_partial_derivative);
      float dxdy = math::dot(x_partial_derivative, y_partial_derivative);
      float dydy = math::dot(y_partial_derivative, y_partial_derivative);

      /* We encode the structure tensor in a float4 using a column major storage order. */
      float4 structure_tensor = float4(dxdx, dxdy, dxdy, dydy);

      structure_tensor_image.store_pixel(texel, structure_tensor);
    });

    return structure_tensor_image;
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
  float compute_sharpness()
  {
    const float sharpness_factor = this->get_sharpness();
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
  float compute_eccentricity()
  {
    return 1.0f / math::max(0.01f, this->get_eccentricity());
  }

  int get_high_precision()
  {
    return this->get_input("High Precision").get_single_value_default(false);
  }

  int get_uniformity()
  {
    return math::max(0, this->get_input("Uniformity").get_single_value_default(4));
  }

  float get_sharpness()
  {
    return math::clamp(this->get_input("Sharpness").get_single_value_default(1.0f), 0.0f, 1.0f);
  }

  float get_eccentricity()
  {
    return math::clamp(this->get_input("Eccentricity").get_single_value_default(1.0f), 0.0f, 2.0f);
  }

  CMPNodeKuwahara get_type()
  {
    const Result &input = this->get_input("Type");
    const MenuValue default_menu_value = MenuValue(CMP_NODE_KUWAHARA_ANISOTROPIC);
    const MenuValue menu_value = input.get_single_value_default(default_menu_value);
    return static_cast<CMPNodeKuwahara>(menu_value.value);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new ConvertKuwaharaOperation(context, node);
}

}  // namespace blender::nodes::node_composite_kuwahara_cc

static void register_node_type_cmp_kuwahara()
{
  namespace file_ns = blender::nodes::node_composite_kuwahara_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeKuwahara", CMP_NODE_KUWAHARA);
  ntype.ui_name = "Kuwahara";
  ntype.ui_description =
      "Apply smoothing filter that preserves edges, for stylized and painterly effects";
  ntype.enum_name_legacy = "KUWAHARA";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_kuwahara_declare;
  ntype.initfunc = file_ns::node_composit_init_kuwahara;
  blender::bke::node_type_storage(
      ntype, "NodeKuwaharaData", node_free_standard_storage, node_copy_standard_storage);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_type_size(ntype, 150, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_kuwahara)
