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
#include "COM_node_operation.hh"
#include "COM_symmetric_separable_blur_weights.hh"
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
    if (input.is_single_value() || get_distance() == 0) {
      input.pass_through(output);
      return;
    }

    /* Compute an image that marks the boundary pixels of the inpainting region as seed pixels in
     * the format expected by the jump flooding algorithm. The inpainting region is the region
     * composed of pixels that are not opaque. */
    Result inpainting_boundary = compute_inpainting_boundary();

    /* Compute a jump flooding table to get the closest boundary pixel to each pixel. */
    Result flooded_boundary = Result::Temporary(ResultType::Color, texture_pool());
    jump_flooding(context(), inpainting_boundary, flooded_boundary);
    inpainting_boundary.release();

    /* Fill the inpainting region based on the jump flooding table. */
    compute_inpainting_region(flooded_boundary);
    flooded_boundary.release();
  }

  Result compute_inpainting_boundary()
  {
    GPUShader *shader = shader_manager().get("compositor_inpaint_compute_boundary");
    GPU_shader_bind(shader);

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    Result inpainting_boundary = Result::Temporary(ResultType::Color, texture_pool());
    const Domain domain = compute_domain();
    inpainting_boundary.allocate_texture(domain);
    inpainting_boundary.bind_as_image(shader, "boundary_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    inpainting_boundary.unbind_as_image();
    GPU_shader_unbind();

    return inpainting_boundary;
  }

  void compute_inpainting_region(Result &flooded_boundary)
  {
    GPUShader *shader = shader_manager().get("compositor_inpaint_compute_region");
    GPU_shader_bind(shader);

    GPU_shader_uniform_1i(shader, "distance", get_distance());

    const Result &input = get_input("Image");
    input.bind_as_texture(shader, "input_tx");

    flooded_boundary.bind_as_texture(shader, "flooded_boundary_tx");

    /* The lateral blur in the shader is proportional to the distance each pixel makes with the
     * inpainting boundary. So the maximum possible blur radius is the user supplied distance. */
    const float max_radius = float(get_distance());
    const SymmetricSeparableBlurWeights &gaussian_weights =
        context().cache_manager().symmetric_separable_blur_weights.get(R_FILTER_GAUSS, max_radius);
    gaussian_weights.bind_as_texture(shader, "gaussian_weights_tx");

    const Domain domain = compute_domain();
    Result &output = get_result("Image");
    output.allocate_texture(domain);
    output.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input.unbind_as_texture();
    gaussian_weights.unbind_as_texture();
    output.unbind_as_image();
    GPU_shader_unbind();
  }

  int get_distance()
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

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_INPAINT, "Inpaint", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_inpaint_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_inpaint;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
