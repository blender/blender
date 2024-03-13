/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_GlareNode.h"
#include "COM_GlareBloomOperation.h"
#include "COM_GlareFogGlowOperation.h"
#include "COM_GlareGhostOperation.h"
#include "COM_GlareSimpleStarOperation.h"
#include "COM_GlareStreaksOperation.h"
#include "COM_GlareThresholdOperation.h"
#include "COM_MixOperation.h"
#include "COM_SetValueOperation.h"

namespace blender::compositor {

GlareNode::GlareNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void GlareNode::convert_to_operations(NodeConverter &converter,
                                      const CompositorContext & /*context*/) const
{
  const bNode *node = this->get_bnode();
  const NodeGlare *glare = (const NodeGlare *)node->storage;

  GlareBaseOperation *glareoperation = nullptr;
  switch (glare->type) {
    default:
    case CMP_NODE_GLARE_GHOST:
      glareoperation = new GlareGhostOperation();
      break;
    case CMP_NODE_GLARE_STREAKS:
      glareoperation = new GlareStreaksOperation();
      break;
    case CMP_NODE_GLARE_FOG_GLOW:
      glareoperation = new GlareFogGlowOperation();
      break;
    case CMP_NODE_GLARE_SIMPLE_STAR:
      glareoperation = new GlareSimpleStarOperation();
      break;
    case CMP_NODE_GLARE_BLOOM:
      glareoperation = new GlareBloomOperation();
      break;
  }
  BLI_assert(glareoperation);
  glareoperation->set_glare_settings(glare);

  GlareThresholdOperation *threshold_operation = new GlareThresholdOperation();
  threshold_operation->set_glare_settings(glare);

  SetValueOperation *mixvalueoperation = new SetValueOperation();
  mixvalueoperation->set_value(glare->mix);

  MixGlareOperation *mixoperation = new MixGlareOperation();
  mixoperation->set_canvas_input_index(1);
  mixoperation->get_input_socket(2)->set_resize_mode(ResizeMode::FitAny);

  converter.add_operation(glareoperation);
  converter.add_operation(threshold_operation);
  converter.add_operation(mixvalueoperation);
  converter.add_operation(mixoperation);

  converter.map_input_socket(get_input_socket(0), threshold_operation->get_input_socket(0));
  converter.add_link(threshold_operation->get_output_socket(),
                     glareoperation->get_input_socket(0));

  converter.add_link(mixvalueoperation->get_output_socket(), mixoperation->get_input_socket(0));
  converter.map_input_socket(get_input_socket(0), mixoperation->get_input_socket(1));
  converter.add_link(glareoperation->get_output_socket(), mixoperation->get_input_socket(2));
  converter.map_output_socket(get_output_socket(), mixoperation->get_output_socket());
}

}  // namespace blender::compositor
