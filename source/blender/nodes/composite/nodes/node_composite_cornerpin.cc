/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_geom.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "BKE_tracking.h"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_cornerpin_cc {

static void cmp_node_cornerpin_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_input<decl::Vector>("Upper Left")
      .default_value({0.0f, 1.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Vector>("Upper Right")
      .default_value({1.0f, 1.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Vector>("Lower Left")
      .default_value({0.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_input<decl::Vector>("Lower Right")
      .default_value({1.0f, 0.0f, 0.0f})
      .min(0.0f)
      .max(1.0f)
      .compositor_expects_single_value();
  b.add_output<decl::Color>("Image");
  b.add_output<decl::Float>("Plane");
}

using namespace blender::realtime_compositor;

class CornerPinOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const float3x3 homography_matrix = compute_homography_matrix();

    Result &input_image = get_input("Image");
    Result &output_image = get_result("Image");
    Result &output_mask = get_result("Plane");
    if (input_image.is_single_value() || homography_matrix == float3x3::identity()) {
      if (output_image.should_compute()) {
        input_image.pass_through(output_image);
      }
      if (output_mask.should_compute()) {
        output_mask.allocate_single_value();
        output_mask.set_float_value(1.0f);
      }
      return;
    }

    Result plane_mask = compute_plane_mask(homography_matrix);
    Result anti_aliased_plane_mask = context().create_result(ResultType::Float);
    smaa(context(), plane_mask, anti_aliased_plane_mask);
    plane_mask.release();

    if (output_image.should_compute()) {
      compute_plane(homography_matrix, anti_aliased_plane_mask);
    }

    if (output_mask.should_compute()) {
      output_mask.steal_data(anti_aliased_plane_mask);
    }
    else {
      anti_aliased_plane_mask.release();
    }
  }

  void compute_plane(const float3x3 &homography_matrix, Result &plane_mask)
  {
    if (this->context().use_gpu()) {
      this->compute_plane_gpu(homography_matrix, plane_mask);
    }
    else {
      this->compute_plane_cpu(homography_matrix, plane_mask);
    }
  }

  void compute_plane_gpu(const float3x3 &homography_matrix, Result &plane_mask)
  {
    GPUShader *shader = context().get_shader("compositor_plane_deform");
    GPU_shader_bind(shader);

    GPU_shader_uniform_mat3_as_mat4(shader, "homography_matrix", homography_matrix.ptr());

    Result &input_image = get_input("Image");
    GPU_texture_mipmap_mode(input_image, true, true);
    GPU_texture_anisotropic_filter(input_image, true);
    GPU_texture_extend_mode(input_image, GPU_SAMPLER_EXTEND_MODE_EXTEND);
    input_image.bind_as_texture(shader, "input_tx");

    plane_mask.bind_as_texture(shader, "mask_tx");

    const Domain domain = compute_domain();
    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    plane_mask.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_plane_cpu(const float3x3 &homography_matrix, Result &plane_mask)
  {
    Result &input = get_input("Image");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      float3 transformed_coordinates = float3x3(homography_matrix) * float3(coordinates, 1.0f);
      /* Point is at infinity and will be zero when sampled, so early exit. */
      if (transformed_coordinates.z == 0.0f) {
        output.store_pixel(texel, float4(0.0f));
        return;
      }
      float2 projected_coordinates = transformed_coordinates.xy() / transformed_coordinates.z;

      /* The derivatives of the projected coordinates with respect to x and y are the first and
       * second columns respectively, divided by the z projection factor as can be shown by
       * differentiating the above matrix multiplication with respect to x and y. Divide by the
       * output size since sample_ewa assumes derivatives with respect to texel coordinates. */
      float2 x_gradient = (homography_matrix[0].xy() / transformed_coordinates.z) / size.x;
      float2 y_gradient = (homography_matrix[1].xy() / transformed_coordinates.z) / size.y;

      float4 sampled_color = input.sample_ewa_extended(
          projected_coordinates, x_gradient, y_gradient);

      /* Premultiply the mask value as an alpha. */
      float4 plane_color = sampled_color * plane_mask.load_pixel(texel).x;

      output.store_pixel(texel, plane_color);
    });
  }

  Result compute_plane_mask(const float3x3 &homography_matrix)
  {
    if (this->context().use_gpu()) {
      return this->compute_plane_mask_gpu(homography_matrix);
    }

    return this->compute_plane_mask_cpu(homography_matrix);
  }

  Result compute_plane_mask_gpu(const float3x3 &homography_matrix)
  {
    GPUShader *shader = context().get_shader("compositor_plane_deform_mask");
    GPU_shader_bind(shader);

    GPU_shader_uniform_mat3_as_mat4(shader, "homography_matrix", homography_matrix.ptr());

    const Domain domain = compute_domain();
    Result plane_mask = context().create_result(ResultType::Float);
    plane_mask.allocate_texture(domain);
    plane_mask.bind_as_image(shader, "mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    plane_mask.unbind_as_image();
    GPU_shader_unbind();

    return plane_mask;
  }

  Result compute_plane_mask_cpu(const float3x3 &homography_matrix)
  {
    const Domain domain = compute_domain();
    Result plane_mask = context().create_result(ResultType::Float);
    plane_mask.allocate_texture(domain);

    const int2 size = domain.size;
    parallel_for(size, [&](const int2 texel) {
      float2 coordinates = (float2(texel) + float2(0.5f)) / float2(size);

      float3 transformed_coordinates = float3x3(homography_matrix) * float3(coordinates, 1.0f);
      /* Point is at infinity and will be zero when sampled, so early exit. */
      if (transformed_coordinates.z == 0.0f) {
        plane_mask.store_pixel(texel, float4(0.0f));
        return;
      }
      float2 projected_coordinates = transformed_coordinates.xy() / transformed_coordinates.z;

      bool is_inside_plane = projected_coordinates.x >= 0.0f && projected_coordinates.y >= 0.0f &&
                             projected_coordinates.x <= 1.0f && projected_coordinates.y <= 1.0f;
      float mask_value = is_inside_plane ? 1.0f : 0.0f;

      plane_mask.store_pixel(texel, float4(mask_value));
    });

    return plane_mask;
  }

  float3x3 compute_homography_matrix()
  {
    float2 lower_left = get_input("Lower Left").get_vector_value_default(float4(0.0f)).xy();
    float2 lower_right = get_input("Lower Right").get_vector_value_default(float4(0.0f)).xy();
    float2 upper_right = get_input("Upper Right").get_vector_value_default(float4(0.0f)).xy();
    float2 upper_left = get_input("Upper Left").get_vector_value_default(float4(0.0f)).xy();

    /* The inputs are invalid because the plane is not convex, fallback to an identity operation in
     * that case. */
    if (!is_quad_convex_v2(lower_left, lower_right, upper_right, upper_left)) {
      return float3x3::identity();
    }

    /* Compute a 2D projection matrix that projects from the corners of the image in normalized
     * coordinates into the corners of the input plane. */
    float3x3 homography_matrix;
    float corners[4][2] = {{lower_left.x, lower_left.y},
                           {lower_right.x, lower_right.y},
                           {upper_right.x, upper_right.y},
                           {upper_left.x, upper_left.y}};
    float identity_corners[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    BKE_tracking_homography_between_two_quads(corners, identity_corners, homography_matrix.ptr());
    return homography_matrix;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new CornerPinOperation(context, node);
}

}  // namespace blender::nodes::node_composite_cornerpin_cc

void register_node_type_cmp_cornerpin()
{
  namespace file_ns = blender::nodes::node_composite_cornerpin_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_CORNERPIN, "Corner Pin", NODE_CLASS_DISTORT);
  ntype.declare = file_ns::cmp_node_cornerpin_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}
