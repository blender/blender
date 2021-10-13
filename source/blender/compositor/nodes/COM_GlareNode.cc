/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2011, Blender Foundation.
 */

#include "COM_GlareNode.h"
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
  bNode *node = this->get_bnode();
  NodeGlare *glare = (NodeGlare *)node->storage;

  GlareBaseOperation *glareoperation = nullptr;
  switch (glare->type) {
    default:
    case 3:
      glareoperation = new GlareGhostOperation();
      break;
    case 2: /* Streaks. */
      glareoperation = new GlareStreaksOperation();
      break;
    case 1: /* Fog glow. */
      glareoperation = new GlareFogGlowOperation();
      break;
    case 0: /* Simple star. */
      glareoperation = new GlareSimpleStarOperation();
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
