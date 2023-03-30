/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_matrix_types.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** FILTER  ******************** */

namespace blender::nodes::node_composite_filter_cc {

static void cmp_node_filter_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Float>(N_("Fac"))
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .compositor_domain_priority(1);
  b.add_input<decl::Color>(N_("Image"))
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .compositor_domain_priority(0);
  b.add_output<decl::Color>(N_("Image"));
}

static void node_composit_buts_filter(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "filter_type", UI_ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

using namespace blender::realtime_compositor;

class FilterOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    GPUShader *shader = shader_manager().get(get_shader_name());
    GPU_shader_bind(shader);

    GPU_shader_uniform_mat3_as_mat4(shader, "ukernel", get_filter_kernel().ptr());

    const Result &input_image = get_input("Image");
    input_image.bind_as_texture(shader, "input_tx");

    const Result &factor = get_input("Fac");
    factor.bind_as_texture(shader, "factor_tx");

    const Domain domain = compute_domain();

    Result &output_image = get_result("Image");
    output_image.allocate_texture(domain);
    output_image.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, domain.size);

    input_image.unbind_as_texture();
    factor.unbind_as_texture();
    output_image.unbind_as_image();
    GPU_shader_unbind();
  }

  CMPNodeFilterMethod get_filter_method()
  {
    return (CMPNodeFilterMethod)bnode().custom1;
  }

  float3x3 get_filter_kernel()
  {
    /* Initialize the kernels as arrays of rows with the top row first. Edge detection kernels
     * return the kernel in the X direction, while the kernel in the Y direction will be computed
     * inside the shader by transposing the kernel in the X direction. */
    switch (get_filter_method()) {
      case CMP_NODE_FILTER_SOFT: {
        const float kernel[3][3] = {{1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f},
                                    {2.0f / 16.0f, 4.0f / 16.0f, 2.0f / 16.0f},
                                    {1.0f / 16.0f, 2.0f / 16.0f, 1.0f / 16.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SHARP_BOX: {
        const float kernel[3][3] = {
            {-1.0f, -1.0f, -1.0f}, {-1.0f, 9.0f, -1.0f}, {-1.0f, -1.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_LAPLACE: {
        const float kernel[3][3] = {{-1.0f / 8.0f, -1.0f / 8.0f, -1.0f / 8.0f},
                                    {-1.0f / 8.0f, 1.0f, -1.0f / 8.0f},
                                    {-1.0f / 8.0f, -1.0f / 8.0f, -1.0f / 8.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SOBEL: {
        const float kernel[3][3] = {{1.0f, 0.0f, -1.0f}, {2.0f, 0.0f, -2.0f}, {1.0f, 0.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_PREWITT: {
        const float kernel[3][3] = {{1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_KIRSCH: {
        const float kernel[3][3] = {
            {5.0f, -3.0f, -2.0f}, {5.0f, -3.0f, -2.0f}, {5.0f, -3.0f, -2.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SHADOW: {
        const float kernel[3][3] = {{1.0f, 2.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, -2.0f, -1.0f}};
        return float3x3(kernel);
      }
      case CMP_NODE_FILTER_SHARP_DIAMOND: {
        const float kernel[3][3] = {
            {0.0f, -1.0f, 0.0f}, {-1.0f, 5.0f, -1.0f}, {0.0f, -1.0f, 0.0f}};
        return float3x3(kernel);
      }
      default: {
        const float kernel[3][3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
        return float3x3(kernel);
      }
    }
  }

  const char *get_shader_name()
  {
    switch (get_filter_method()) {
      case CMP_NODE_FILTER_LAPLACE:
      case CMP_NODE_FILTER_SOBEL:
      case CMP_NODE_FILTER_PREWITT:
      case CMP_NODE_FILTER_KIRSCH:
        return "compositor_edge_filter";
      case CMP_NODE_FILTER_SOFT:
      case CMP_NODE_FILTER_SHARP_BOX:
      case CMP_NODE_FILTER_SHADOW:
      case CMP_NODE_FILTER_SHARP_DIAMOND:
      default:
        return "compositor_filter";
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new FilterOperation(context, node);
}

}  // namespace blender::nodes::node_composite_filter_cc

void register_node_type_cmp_filter()
{
  namespace file_ns = blender::nodes::node_composite_filter_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_FILTER, "Filter", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_filter_declare;
  ntype.draw_buttons = file_ns::node_composit_buts_filter;
  ntype.labelfunc = node_filter_label;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
