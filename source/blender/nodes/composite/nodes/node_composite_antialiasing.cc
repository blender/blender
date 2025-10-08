/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "UI_resources.hh"

#include "COM_algorithm_smaa.hh"
#include "COM_node_operation.hh"

#include "node_composite_util.hh"

/* **************** Anti-Aliasing (SMAA 1x) ******************** */

namespace blender::nodes::node_composite_antialiasing_cc {

static void cmp_node_antialiasing_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Color>("Image")
      .default_value({1.0f, 1.0f, 1.0f, 1.0f})
      .hide_value()
      .structure_type(StructureType::Dynamic);
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic).align_with_previous();

  b.add_input<decl::Float>("Threshold")
      .default_value(0.2f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description(
          "Specifies the threshold or sensitivity to edges. Lowering this value you will be able "
          "to detect more edges at the expense of performance");
  b.add_input<decl::Float>("Contrast Limit")
      .default_value(2.0f)
      .min(0.0f)
      .description(
          "If there is an neighbor edge that has a Contrast Limit times bigger contrast than "
          "current edge, current edge will be discarded. This allows to eliminate spurious "
          "crossing edges");
  b.add_input<decl::Float>("Corner Rounding")
      .default_value(0.25f)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .description("Specifies how much sharp corners will be rounded");
}

using namespace blender::compositor;

class AntiAliasingOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    smaa(this->context(),
         this->get_input("Image"),
         this->get_result("Image"),
         this->get_threshold(),
         this->get_local_contrast_adaptation_factor(),
         this->get_corner_rounding());
  }

  /* We encode the threshold in the [0, 1] range, while the SMAA algorithm expects it in the
   * [0, 0.5] range. */
  float get_threshold()
  {
    return math::clamp(this->get_input("Threshold").get_single_value_default(0.2f), 0.0f, 1.0f) /
           2.0f;
  }

  float get_local_contrast_adaptation_factor()
  {
    return math::max(0.0f, this->get_input("Contrast Limit").get_single_value_default(2.0f));
  }

  /* We encode the corner rounding factor in the float [0, 1] range, while the SMAA algorithm
   * expects it in the integer [0, 100] range. */
  int get_corner_rounding()
  {
    return int(math::clamp(this->get_input("Corner Rounding").get_single_value_default(0.25f),
                           0.0f,
                           1.0f) *
               100.0f);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new AntiAliasingOperation(context, node);
}

}  // namespace blender::nodes::node_composite_antialiasing_cc

static void register_node_type_cmp_antialiasing()
{
  namespace file_ns = blender::nodes::node_composite_antialiasing_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeAntiAliasing", CMP_NODE_ANTIALIASING);
  ntype.ui_name = "Anti-Aliasing";
  ntype.ui_description = "Smooth away jagged edges";
  ntype.enum_name_legacy = "ANTIALIASING";
  ntype.nclass = NODE_CLASS_OP_FILTER;
  ntype.declare = file_ns::cmp_node_antialiasing_declare;
  ntype.flag |= NODE_PREVIEW;
  blender::bke::node_type_size(ntype, 175, 140, 200);
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_antialiasing)
