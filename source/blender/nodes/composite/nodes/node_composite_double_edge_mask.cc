/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "COM_algorithm_jump_flooding.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_double_edge_mask_cc {

static void cmp_node_double_edge_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Outer Mask")
      .default_value(0.8f)
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Float>("Inner Mask")
      .default_value(0.8f)
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Dynamic);
  b.add_input<decl::Bool>("Image Edges")
      .default_value(false)
      .description(
          "The edges of the image that intersects the outer mask will be considered edges of the "
          "outer mask. Otherwise, the outer mask will be considered open-ended");
  b.add_input<decl::Bool>("Only Inside Outer")
      .default_value(false)
      .description(
          "Only edges of the inner mask that lie inside the outer mask will be considered. "
          "Otherwise, all edges of the inner mask will be considered");

  b.add_output<decl::Float>("Mask").structure_type(StructureType::Dynamic);
}

using namespace blender::compositor;

class DoubleEdgeMaskOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &inner_mask = get_input("Inner Mask");
    Result &outer_mask = get_input("Outer Mask");
    Result &output = get_result("Mask");
    if (inner_mask.is_single_value() || outer_mask.is_single_value()) {
      output.allocate_invalid();
      return;
    }

    /* Compute an image that marks the boundary pixels of the masks as seed pixels in the format
     * expected by the jump flooding algorithm. */
    Result inner_boundary = context().create_result(ResultType::Int2, ResultPrecision::Half);
    Result outer_boundary = context().create_result(ResultType::Int2, ResultPrecision::Half);
    compute_boundary(inner_boundary, outer_boundary);

    /* Compute a jump flooding table for each mask boundary to get a distance transform to each of
     * the boundaries. */
    Result flooded_inner_boundary = context().create_result(ResultType::Int2,
                                                            ResultPrecision::Half);
    Result flooded_outer_boundary = context().create_result(ResultType::Int2,
                                                            ResultPrecision::Half);
    jump_flooding(context(), inner_boundary, flooded_inner_boundary);
    jump_flooding(context(), outer_boundary, flooded_outer_boundary);
    inner_boundary.release();
    outer_boundary.release();

    /* Compute the gradient based on the jump flooding table. */
    compute_gradient(flooded_inner_boundary, flooded_outer_boundary);
    flooded_inner_boundary.release();
    flooded_outer_boundary.release();
  }

  void compute_boundary(Result &inner_boundary, Result &outer_boundary)
  {
    if (this->context().use_gpu()) {
      this->compute_boundary_gpu(inner_boundary, outer_boundary);
    }
    else {
      this->compute_boundary_cpu(inner_boundary, outer_boundary);
    }
  }

  void compute_boundary_gpu(Result &inner_boundary, Result &outer_boundary)
  {
    gpu::Shader *shader = context().get_shader("compositor_double_edge_mask_compute_boundary",
                                               ResultPrecision::Half);
    GPU_shader_bind(shader);

    GPU_shader_uniform_1b(shader, "include_all_inner_edges", include_all_inner_edges());
    GPU_shader_uniform_1b(shader, "include_edges_of_image", include_edges_of_image());

    const Result &inner_mask = get_input("Inner Mask");
    inner_mask.bind_as_texture(shader, "inner_mask_tx");

    const Result &outer_mask = get_input("Outer Mask");
    outer_mask.bind_as_texture(shader, "outer_mask_tx");

    const Domain domain = compute_domain();

    inner_boundary.allocate_texture(domain);
    inner_boundary.bind_as_image(shader, "inner_boundary_img");

    outer_boundary.allocate_texture(domain);
    outer_boundary.bind_as_image(shader, "outer_boundary_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    inner_mask.unbind_as_texture();
    outer_mask.unbind_as_texture();
    inner_boundary.unbind_as_image();
    outer_boundary.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_boundary_cpu(Result &inner_boundary, Result &outer_boundary)
  {
    const bool include_all_inner_edges = this->include_all_inner_edges();
    const bool include_edges_of_image = this->include_edges_of_image();

    const Result &inner_mask = get_input("Inner Mask");
    const Result &outer_mask = get_input("Outer Mask");

    const Domain domain = compute_domain();
    inner_boundary.allocate_texture(domain);
    outer_boundary.allocate_texture(domain);

    /* The Double Edge Mask operation uses a jump flood algorithm to compute a distance transform
     * to the boundary of the inner and outer masks. The algorithm expects an input image whose
     * values are those returned by the initialize_jump_flooding_value function, given the texel
     * location and a boolean specifying if the pixel is a boundary one.
     *
     * Technically, we needn't restrict the output to just the boundary pixels, since the algorithm
     * can still operate if the interior of the masks was also included. However, the algorithm
     * operates more accurately when the number of pixels to be flooded is minimum. */
    parallel_for(domain.size, [&](const int2 texel) {
      /* Identify if any of the 8 neighbors around the center pixel are not masked. */
      bool has_inner_non_masked_neighbors = false;
      bool has_outer_non_masked_neighbors = false;
      for (int j = -1; j <= 1; j++) {
        for (int i = -1; i <= 1; i++) {
          int2 offset = int2(i, j);

          /* Exempt the center pixel. */
          if (offset == int2(0)) {
            continue;
          }

          if (inner_mask.load_pixel_extended<float>(texel + offset) == 0.0f) {
            has_inner_non_masked_neighbors = true;
          }

          /* If the user specified include_edges_of_image to be true, then we assume the outer mask
           * is bounded by the image boundary, otherwise, we assume the outer mask is open-ended.
           * This is practically implemented by falling back to 0.0f or 1.0f for out of bound
           * pixels. */
          float boundary_fallback = include_edges_of_image ? 0.0f : 1.0f;
          if (outer_mask.load_pixel_fallback(texel + offset, boundary_fallback) == 0.0f) {
            has_outer_non_masked_neighbors = true;
          }

          /* Both are true, no need to continue. */
          if (has_inner_non_masked_neighbors && has_outer_non_masked_neighbors) {
            break;
          }
        }
      }

      bool is_inner_masked = inner_mask.load_pixel<float>(texel) > 0.0f;
      bool is_outer_masked = outer_mask.load_pixel<float>(texel) > 0.0f;

      /* The pixels at the boundary are those that are masked and have non masked neighbors. The
       * inner boundary has a specialization, if include_all_inner_edges is false, only inner
       * boundaries that lie inside the outer mask will be considered a boundary. The outer
       * boundary is only considered if it is not inside the inner mask. */
      bool is_inner_boundary = is_inner_masked && has_inner_non_masked_neighbors &&
                               (is_outer_masked || include_all_inner_edges);
      bool is_outer_boundary = is_outer_masked && !is_inner_masked &&
                               has_outer_non_masked_neighbors;

      /* Encode the boundary information in the format expected by the jump flooding algorithm. */
      int2 inner_jump_flooding_value = initialize_jump_flooding_value(texel, is_inner_boundary);
      int2 outer_jump_flooding_value = initialize_jump_flooding_value(texel, is_outer_boundary);

      inner_boundary.store_pixel(texel, inner_jump_flooding_value);
      outer_boundary.store_pixel(texel, outer_jump_flooding_value);
    });
  }

  void compute_gradient(const Result &flooded_inner_boundary, const Result &flooded_outer_boundary)
  {
    if (this->context().use_gpu()) {
      this->compute_gradient_gpu(flooded_inner_boundary, flooded_outer_boundary);
    }
    else {
      this->compute_gradient_cpu(flooded_inner_boundary, flooded_outer_boundary);
    }
  }

  void compute_gradient_gpu(const Result &flooded_inner_boundary,
                            const Result &flooded_outer_boundary)
  {
    gpu::Shader *shader = context().get_shader("compositor_double_edge_mask_compute_gradient");
    GPU_shader_bind(shader);

    const Result &inner_mask = get_input("Inner Mask");
    inner_mask.bind_as_texture(shader, "inner_mask_tx");

    const Result &outer_mask = get_input("Outer Mask");
    outer_mask.bind_as_texture(shader, "outer_mask_tx");

    flooded_inner_boundary.bind_as_texture(shader, "flooded_inner_boundary_tx");
    flooded_outer_boundary.bind_as_texture(shader, "flooded_outer_boundary_tx");

    const Domain domain = compute_domain();
    Result &output = get_result("Mask");
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    inner_mask.unbind_as_texture();
    outer_mask.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  void compute_gradient_cpu(const Result &flooded_inner_boundary,
                            const Result &flooded_outer_boundary)
  {
    const Result &inner_mask_input = get_input("Inner Mask");
    const Result &outer_mask_input = get_input("Outer Mask");

    const Domain domain = compute_domain();
    Result &output = get_result("Mask");
    output.allocate_texture(domain);

    /* Computes a linear gradient from the outer mask boundary to the inner mask boundary, starting
     * from 0 and ending at 1. This is computed using the equation:
     *
     *   Gradient = O / (O + I)
     *
     * Where O is the distance to the outer boundary and I is the distance to the inner boundary.
     * This can be viewed as computing the ratio between the distance to the outer boundary to the
     * distance between the outer and inner boundaries as can be seen in the following illustration
     * where the $ sign designates a pixel between both boundaries.
     *
     *                   |    O         I    |
     *   Outer Boundary  |---------$---------|  Inner Boundary
     *                   |                   |
     */
    parallel_for(domain.size, [&](const int2 texel) {
      /* Pixels inside the inner mask are always 1.0. */
      float inner_mask = inner_mask_input.load_pixel<float>(texel);
      if (inner_mask != 0.0f) {
        output.store_pixel(texel, 1.0f);
        return;
      }

      /* Pixels outside the outer mask are always 0.0. */
      float outer_mask = outer_mask_input.load_pixel<float>(texel);
      if (outer_mask == 0.0f) {
        output.store_pixel(texel, 0.0f);
        return;
      }

      /* Compute the distances to the inner and outer boundaries from the jump flooding tables. */
      int2 inner_boundary_texel = flooded_inner_boundary.load_pixel<int2>(texel);
      int2 outer_boundary_texel = flooded_outer_boundary.load_pixel<int2>(texel);
      float distance_to_inner = math::distance(float2(texel), float2(inner_boundary_texel));
      float distance_to_outer = math::distance(float2(texel), float2(outer_boundary_texel));

      float gradient = distance_to_outer / (distance_to_outer + distance_to_inner);

      output.store_pixel(texel, gradient);
    });
  }

  bool include_all_inner_edges()
  {
    return !this->get_input("Only Inside Outer").get_single_value_default(false);
  }

  bool include_edges_of_image()
  {
    return this->get_input("Image Edges").get_single_value_default(false);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DoubleEdgeMaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_double_edge_mask_cc

static void register_node_type_cmp_doubleedgemask()
{
  namespace file_ns = blender::nodes::node_composite_double_edge_mask_cc;

  static blender::bke::bNodeType ntype; /* Allocate a node type data structure. */

  cmp_node_type_base(&ntype, "CompositorNodeDoubleEdgeMask", CMP_NODE_DOUBLEEDGEMASK);
  ntype.ui_name = "Double Edge Mask";
  ntype.ui_description = "Create a gradient between two masks";
  ntype.enum_name_legacy = "DOUBLEEDGEMASK";
  ntype.nclass = NODE_CLASS_MATTE;
  ntype.declare = file_ns::cmp_node_double_edge_mask_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_type_size(ntype, 145, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_doubleedgemask)
