/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "DNA_scene_types.h"

#include "COM_algorithm_jump_flooding.hh"
#include "COM_algorithm_symmetric_separable_blur_variable_size.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** Inpaint/ ******************** */

namespace blender::nodes::node_composite_inpaint_cc {

static void cmp_node_inpaint_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>("Image");
}

static void node_composit_buts_inpaint(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "distance", UI_ITEM_R_SPLIT_EMPTY_NAME, nullptr, ICON_NONE);
}

using namespace blender::realtime_compositor;

class InpaintOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &input = get_input("Image");
    Result &output = get_result("Image");
    if (input.is_single_value() || get_max_distance() == 0) {
      input.pass_through(output);
      return;
    }

    Result inpainting_boundary = compute_inpainting_boundary();

    /* Compute a jump flooding table to get the closest boundary pixel to each pixel. */
    Result flooded_boundary = context().create_temporary_result(ResultType::Int2,
                                                                ResultPrecision::Half);
    jump_flooding(context(), inpainting_boundary, flooded_boundary);
    inpainting_boundary.release();

    Result filled_region = context().create_temporary_result(ResultType::Color);
    Result distance_to_boundary = context().create_temporary_result(ResultType::Float,
                                                                    ResultPrecision::Half);
    Result smoothing_radius = context().create_temporary_result(ResultType::Float,
                                                                ResultPrecision::Half);
    fill_inpainting_region(
        flooded_boundary, filled_region, distance_to_boundary, smoothing_radius);
    flooded_boundary.release();

    Result smoothed_region = context().create_temporary_result(ResultType::Color);
    symmetric_separable_blur_variable_size(context(),
                                           filled_region,
                                           smoothed_region,
                                           smoothing_radius,
                                           R_FILTER_GAUSS,
                                           get_max_distance());
    filled_region.release();
    smoothing_radius.release();

    compute_inpainting_region(smoothed_region, distance_to_boundary);
    distance_to_boundary.release();
    smoothed_region.release();
  }

  /* Compute an image that marks the boundary pixels of the inpainting region as seed pixels for
   * the jump flooding algorithm. The inpainting region is the region composed of pixels that are
   * not opaque. */
  Result compute_inpainting_boundary()
  {
    GPUShader *shader = context().get_shader("compositor_inpaint_compute_boundary",
                                             ResultPrecision::Half);
    GPU_shader_bind(shader);

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result inpainting_boundary = context().create_temporary_result(ResultType::Int2,
                                                                   ResultPrecision::Half);
    const Domain domain = compute_domain();
    inpainting_boundary.allocate_texture(domain);
    inpainting_boundary.bind_as_image(shader, "boundary_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    inpainting_boundary.unbind_as_image();
    GPU_shader_unbind();

    return inpainting_boundary;
  }

  /* Fill the inpainting region based on the jump flooding table and write the distance to the
   * closest boundary pixel to an intermediate buffer. */
  void fill_inpainting_region(Result &flooded_boundary,
                              Result &filled_region,
                              Result &distance_to_boundary,
                              Result &smoothing_radius)
  {
    GPUShader *shader = context().get_shader("compositor_inpaint_fill_region");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "max_distance", get_max_distance());

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    flooded_boundary.bind_as_texture(shader, "flooded_boundary_tx");

    const Domain domain = compute_domain();
    filled_region.allocate_texture(domain);
    filled_region.bind_as_image(shader, "filled_region_img");

    distance_to_boundary.allocate_texture(domain);
    distance_to_boundary.bind_as_image(shader, "distance_to_boundary_img");

    smoothing_radius.allocate_texture(domain);
    smoothing_radius.bind_as_image(shader, "smoothing_radius_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    flooded_boundary.unbind_as_texture();
    filled_region.unbind_as_image();
    distance_to_boundary.unbind_as_image();
    smoothing_radius.unbind_as_image();
    GPU_shader_unbind();
  }

  /* Compute the inpainting region by mixing the smoothed inpainted region with the original input
   * up to the inpainting distance. */
  void compute_inpainting_region(Result &inpainted_region, Result &distance_to_boundary)
  {
    GPUShader *shader = context().get_shader("compositor_inpaint_compute_region");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "max_distance", get_max_distance());

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    inpainted_region.bind_as_texture(shader, "inpainted_region_tx");
    distance_to_boundary.bind_as_texture(shader, "distance_to_boundary_tx");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    inpainted_region.unbind_as_texture();
    distance_to_boundary.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  int get_max_distance()
  {
    return bnode().custom2;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new InpaintOperation(context, node);
}

}  // namespace blender::nodes::node_composite_inpaint_cc

void register_node_type_cmp_inpaint()
{
  namespace file_ns = blender::nodes::node_composite_inpaint_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_INPAINT, "Inpaint", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_inpaint_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_inpaint;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::nodeRegisterType(&ntype);
}
