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
    Result anti_aliased_plane_mask = context().create_temporary_result(ResultType::Float);
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
    GPUShader *shader = context().get_shader("compositor_plane_deform");
    GPU_shader_bind(shader);

    GPU_shader_uniform_mat3_as_mat4(shader, "homography_matrix", homography_matrix.ptr());

    Result &input_image = get_input("Image");
    GPU_texture_mipmap_mode(input_image.texture(), true, true);
    GPU_texture_anisotropic_filter(input_image.texture(), true);
    GPU_texture_extend_mode(input_image.texture(), GPU_SAMPLER_EXTEND_MODE_EXTEND);
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

  Result compute_plane_mask(const float3x3 &homography_matrix)
  {
    GPUShader *shader = context().get_shader("compositor_plane_deform_mask");
    GPU_shader_bind(shader);

    GPU_shader_uniform_mat3_as_mat4(shader, "homography_matrix", homography_matrix.ptr());

    const Domain domain = compute_domain();
    Result plane_mask = context().create_temporary_result(ResultType::Float);
    plane_mask.allocate_texture(domain);
    plane_mask.bind_as_image(shader, "mask_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    plane_mask.unbind_as_image();
    GPU_shader_unbind();

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

  blender::bke::nodeRegisterType(&ntype);
}
