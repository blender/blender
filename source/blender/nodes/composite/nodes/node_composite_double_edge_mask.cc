/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "COM_algorithm_jump_flooding.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Double Edge Mask ******************** */

namespace blender::nodes::node_composite_double_edge_mask_cc {

static void cmp_node_double_edge_mask_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>("Inner Mask")
      .default_value(0.8f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(1);
  b.add_input<decl::Float>("Outer Mask")
      .default_value(0.8f)
      .min(0.0f)
      .max(1.0f)
      .compositor_domain_priority(0);
  b.add_output<decl::Float>("Mask");
}

static void node_composit_buts_double_edge_mask(uiLayout *layout,
                                                bContext * /*C*/,
                                                PointerRNA *ptr)
{
  uiLayout *col;

  col = uiLayoutColumn(layout, false);

  uiItemL(col, IFACE_("Inner Edge:"), ICON_NONE);
  uiItemR(col, ptr, "inner_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
  uiItemL(col, IFACE_("Buffer Edge:"), ICON_NONE);
  uiItemR(col, ptr, "edge_mode", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

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
    Result inner_boundary = context().create_temporary_result(ResultType::Int2,
                                                              ResultPrecision::Half);
    Result outer_boundary = context().create_temporary_result(ResultType::Int2,
                                                              ResultPrecision::Half);
    compute_boundary(inner_boundary, outer_boundary);

    /* Compute a jump flooding table for each mask boundary to get a distance transform to each of
     * the boundaries. */
    Result flooded_inner_boundary = context().create_temporary_result(ResultType::Int2,
                                                                      ResultPrecision::Half);
    Result flooded_outer_boundary = context().create_temporary_result(ResultType::Int2,
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
    GPUShader *shader = context().get_shader("compositor_double_edge_mask_compute_boundary",
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

  void compute_gradient(Result &flooded_inner_boundary, Result &flooded_outer_boundary)
  {
    GPUShader *shader = context().get_shader("compositor_double_edge_mask_compute_gradient");
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

  /* If false, only edges of the inner mask that lie inside the outer mask will be considered. If
   * true, all edges of the inner mask will be considered. */
  bool include_all_inner_edges()
  {
    return !bool(bnode().custom1);
  }

  /* If true, the edges of the image that intersects the outer mask will be considered edges o the
   * outer mask. If false, the outer mask will be considered open-ended. */
  bool include_edges_of_image()
  {
    return bool(bnode().custom2);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new DoubleEdgeMaskOperation(context, node);
}

}  // namespace blender::nodes::node_composite_double_edge_mask_cc

void register_node_type_cmp_doubleedgemask()
{
  namespace file_ns = blender::nodes::node_composite_double_edge_mask_cc;

  static blender::bke::bNodeType ntype; /* Allocate a node type data structure. */

  cmp_node_type_base(&ntype, CMP_NODE_DOUBLEEDGEMASK, "Double Edge Mask", NODE_CLASS_MATTE);
  ntype.declare = file_ns::cmp_node_double_edge_mask_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_double_edge_mask;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
