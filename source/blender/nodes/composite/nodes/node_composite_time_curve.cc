/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_base.hh"

#include "BKE_colortools.hh"
#include "BKE_node.hh"

#include "COM_node_operation.hh"
#include "COM_result.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_time_curves_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Int>("Start Frame").default_value(1);
  b.add_input<decl::Int>("End Frame").default_value(250);

  b.add_output<decl::Float>("Factor", "Fac");
}

static void node_init(bNodeTree * /*ntree*/, bNode *node)
{
  node->storage = BKE_curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
}

using namespace blender::compositor;

class TimeCurveOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &result = this->get_result("Fac");
    result.allocate_single_value();

    CurveMapping *curve_mapping = const_cast<CurveMapping *>(this->get_curve_mapping());
    BKE_curvemapping_init(curve_mapping);
    const float time = BKE_curvemapping_evaluateF(
        curve_mapping, 0, this->compute_normalized_time());
    result.set_single_value(math::clamp(time, 0.0f, 1.0f));
  }

  float compute_normalized_time()
  {
    const int frame_number = this->context().get_frame_number();
    if (frame_number < this->get_start_frame()) {
      return 0.0f;
    }
    if (frame_number > this->get_end_frame()) {
      return 1.0f;
    }
    if (this->get_start_frame() == this->get_end_frame()) {
      return 0.0f;
    }
    return float(frame_number - this->get_start_frame()) /
           float(this->get_end_frame() - this->get_start_frame());
  }

  int get_start_frame()
  {
    return this->get_input("Start Frame").get_single_value_default<int>();
  }

  int get_end_frame()
  {
    return this->get_input("End Frame").get_single_value_default<int>();
  }

  const CurveMapping *get_curve_mapping()
  {
    return static_cast<const CurveMapping *>(node().storage);
  }
};

static NodeOperation *get_compositor_operation(Context &context, const bNode &node)
{
  return new TimeCurveOperation(context, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTime", CMP_NODE_TIME);
  ntype.ui_name = "Time Curve";
  ntype.ui_description =
      "Generate a factor value (from 0.0 to 1.0) between scene start and end time, using a curve "
      "mapping";
  ntype.enum_name_legacy = "TIME";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 200, 140, 320);
  ntype.initfunc = node_init;
  bke::node_type_storage(ntype, "CurveMapping", node_free_curves, node_copy_curves);
  ntype.get_compositor_operation = get_compositor_operation;

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_composite_time_curves_cc
