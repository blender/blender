/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "BKE_context.hh"

#include "BLI_math_vector_types.hh"

#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "COM_node_operation.hh"

#include "NOD_node_extra_info.hh"

#include "SEQ_time.hh"

#include "UI_resources.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_strip_info_cc {

static void cmp_node_strip_info_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Int>("Start Frame");
  b.add_output<decl::Int>("End Frame");
  b.add_output<decl::Vector>("Location").dimensions(2);
  b.add_output<decl::Float>("Rotation");
  b.add_output<decl::Vector>("Scale").dimensions(2);
}

using namespace blender::compositor;

class StripInfoOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Strip *strip = this->context().get_strip();
    if (strip == nullptr) {
      this->execute_invalid();
      return;
    }

    Result &start_frame_result = this->get_result("Start Frame");
    if (start_frame_result.should_compute()) {
      start_frame_result.allocate_single_value();
      start_frame_result.set_single_value(
          seq::time_left_handle_frame_get(&context().get_scene(), strip));
    }

    Result &end_frame_result = this->get_result("End Frame");
    if (end_frame_result.should_compute()) {
      end_frame_result.allocate_single_value();
      end_frame_result.set_single_value(
          seq::time_right_handle_frame_get(&context().get_scene(), strip));
    }

    Result &location_result = this->get_result("Location");
    if (location_result.should_compute()) {
      location_result.allocate_single_value();
      location_result.set_single_value(
          float2(strip->data->transform->xofs, strip->data->transform->yofs));
    }

    Result &rotation_result = this->get_result("Rotation");
    if (rotation_result.should_compute()) {
      rotation_result.allocate_single_value();
      rotation_result.set_single_value(strip->data->transform->rotation);
    }

    Result &scale_result = this->get_result("Scale");
    if (scale_result.should_compute()) {
      scale_result.allocate_single_value();
      scale_result.set_single_value(
          float2(strip->data->transform->scale_x, strip->data->transform->scale_y));
    }
  }

  void execute_invalid()
  {
    Result &start_frame_result = this->get_result("Start Frame");
    if (start_frame_result.should_compute()) {
      start_frame_result.allocate_invalid();
    }

    Result &end_frame_result = this->get_result("End Frame");
    if (end_frame_result.should_compute()) {
      end_frame_result.allocate_invalid();
    }

    Result &location_result = this->get_result("Location");
    if (location_result.should_compute()) {
      location_result.allocate_invalid();
    }

    Result &rotation_result = this->get_result("Rotation");
    if (rotation_result.should_compute()) {
      rotation_result.allocate_invalid();
    }

    Result &scale_result = this->get_result("Scale");
    if (scale_result.should_compute()) {
      scale_result.allocate_invalid();
    }
  }
};

static void node_extra_info(NodeExtraInfoParams &parameters)
{
  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SEQUENCER) {
    NodeExtraInfoRow row;
    row.text = RPT_("Node Unsupported");
    row.tooltip = TIP_("The Strip Info node is only supported for sequencer compositing");
    row.icon = ICON_ERROR;
    parameters.rows.append(std::move(row));
  }
}

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new StripInfoOperation(context, node);
}

}  // namespace blender::nodes::node_composite_strip_info_cc

static void register_node_type_cmp_strip_info()
{
  namespace file_ns = blender::nodes::node_composite_strip_info_cc;

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeSequencerStripInfo");
  ntype.ui_name = "Sequencer Strip Info";
  ntype.ui_description = "Returns information about the active strip of the modifier";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_strip_info_declare;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;
  ntype.get_extra_info = file_ns::node_extra_info;

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node_type_cmp_strip_info)
