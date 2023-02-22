/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_matrix.hh"

#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Pixelate ******************** */

namespace blender::nodes::node_composite_pixelate_cc {

static void cmp_node_pixelate_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Color>(N_("Color"));
  b.add_output<decl::Color>(N_("Color"));
}

using namespace blender::realtime_compositor;

class PixelateOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    /* It might seems strange that the input is passed through without any processing, but note
     * that the actual processing happens inside the domain realization input processor of the
     * input. Indeed, the pixelate node merely realizes its input on a smaller-sized domain that
     * matches its apparent size, that is, its size after the domain transformation. The pixelate
     * node has no effect if the input is scaled-up. See the compute_domain method for more
     * information. */
    Result &result = get_result("Color");
    get_input("Color").pass_through(result);

    result.get_realization_options().interpolation = Interpolation::Nearest;
  }

  /* Compute a smaller-sized domain that matches the apparent size of the input while having a unit
   * scale transformation, see the execute method for more information. */
  Domain compute_domain() override
  {
    Domain domain = get_input("Color").domain();

    /* Get the scaling component of the domain transformation, but make sure it doesn't exceed 1,
     * because pixelation should only happen if the input is scaled down. */
    const float2 scale = math::min(float2(1.0f), math::to_scale(float2x2(domain.transformation)));

    /* Multiply the size of the domain by its scale to match its apparent size, but make sure it is
     * at least 1 pixel in both axis. */
    domain.size = math::max(int2(float2(domain.size) * scale), int2(1));

    /* Reset the scale of the transformation by transforming it with the inverse of the scale. */
    domain.transformation *= math::from_scale<float3x3>(math::safe_divide(float2(1.0f), scale));

    return domain;
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new PixelateOperation(context, node);
}

}  // namespace blender::nodes::node_composite_pixelate_cc

void register_node_type_cmp_pixelate()
{
  namespace file_ns = blender::nodes::node_composite_pixelate_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_PIXELATE, "Pixelate", NODE_CLASS_OP_FILTER);
  ntype.declare = file_ns::cmp_node_pixelate_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}
