/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"

#include "UI_resources.hh"

#include "COM_bokeh_kernel.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Bokeh image Tools  ******************** */

namespace blender::nodes::node_composite_bokehimage_cc {

static void cmp_node_bokehimage_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Flaps").default_value(5).min(3).max(24).description(
      "The number of flaps in the bokeh");
  b.add_input<decl::Float>("Angle")
      .default_value(0.0f)
      .subtype(PROP_ANGLE)
      .description("The angle of the bokeh");
  b.add_input<decl::Float>("Roundness")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Specifies how round the bokeh is, maximum roundness produces a circular bokeh");
  b.add_input<decl::Float>("Catadioptric Size")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Specifies the size of the catadioptric iris, zero means no iris");
  b.add_input<decl::Float>("Color Shift")
      .default_value(0.0f)
      .subtype(PROP_FACTOR)
      .min(-1.0f)
      .max(1.0f)
      .description(
          "Specifies the amount of color shifting. 1 means maximum shifting towards blue while -1 "
          "means maximum shifting toward red");

  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
}

using namespace blender::compositor;

class BokehImageOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Domain domain = this->compute_domain();

    const Result &bokeh_kernel = this->context().cache_manager().bokeh_kernels.get(
        this->context(),
        domain.size,
        this->get_flaps(),
        this->get_angle(),
        this->get_roundness(),
        this->get_catadioptric_size(),
        this->get_color_shift());

    Result &output = this->get_result("Image");
    output.wrap_external(bokeh_kernel);
  }

  Domain compute_domain() override
  {
    return Domain(int2(512));
  }

  int get_flaps()
  {
    return math::clamp(this->get_input("Flaps").get_single_value_default(5), 3, 24);
  }

  float get_angle()
  {
    return this->get_input("Angle").get_single_value_default(0.0f);
  }

  float get_roundness()
  {
    return math::clamp(this->get_input("Roundness").get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  float get_catadioptric_size()
  {
    return math::clamp(
        this->get_input("Catadioptric Size").get_single_value_default(0.0f), 0.0f, 1.0f);
  }

  float get_color_shift()
  {
    return math::clamp(this->get_input("Color Shift").get_single_value_default(0.0f), -1.0f, 1.0f);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new BokehImageOperation(context, node);
}

}  // namespace blender::nodes::node_composite_bokehimage_cc

static void register_node_type_cmp_bokehimage()
{
  namespace file_ns = blender::nodes::node_composite_bokehimage_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeBokehImage", CMP_NODE_BOKEHIMAGE);
  ntype.ui_name = "Bokeh Image";
  ntype.ui_description = "Generate image with bokeh shape for use with the Bokeh Blur filter node";
  ntype.enum_name_legacy = "BOKEHIMAGE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_bokehimage_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  blender::bke::node_type_size(ntype, 160, 140, NODE_DEFAULT_MAX_WIDTH);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_bokehimage)
