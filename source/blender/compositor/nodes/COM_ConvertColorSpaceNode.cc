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
 * Copyright 2021, Blender Foundation.
 */

#include "COM_ConvertColorSpaceNode.h"

#include "BKE_node.h"

#include "BLI_utildefines.h"

#include "COM_ConvertColorSpaceOperation.h"
#include "COM_ConvertOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_ImageOperation.h"
#include "COM_MultilayerImageOperation.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"compositor"};

namespace blender::compositor {

ConvertColorSpaceNode::ConvertColorSpaceNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void ConvertColorSpaceNode::convert_to_operations(NodeConverter &converter,
                                                  const CompositorContext &UNUSED(context)) const
{
  bNode *b_node = get_bnode();

  NodeInput *inputSocketImage = this->get_input_socket(0);
  NodeOutput *outputSocketImage = this->get_output_socket(0);

  NodeConvertColorSpace *settings = static_cast<NodeConvertColorSpace *>(b_node->storage);

  if (!performs_conversion(*settings)) {
    converter.map_output_socket(get_output_socket(0),
                                converter.add_input_proxy(get_input_socket(0), false));
    return;
  }

  ConvertColorSpaceOperation *operation = new ConvertColorSpaceOperation();
  operation->set_settings((NodeConvertColorSpace *)b_node->storage);
  converter.add_operation(operation);

  converter.map_input_socket(inputSocketImage, operation->get_input_socket(0));
  converter.map_output_socket(outputSocketImage, operation->get_output_socket());
}

bool ConvertColorSpaceNode::performs_conversion(NodeConvertColorSpace &settings) const
{
  bNode *b_node = get_bnode();

  if (IMB_colormanagement_space_name_is_data(settings.from_color_space)) {
    CLOG_INFO(&LOG,
              2,
              "Color space conversion bypassed for node: %s. From color space is data: %s.",
              b_node->name,
              settings.from_color_space);
    return false;
  }

  if (IMB_colormanagement_space_name_is_data(settings.to_color_space)) {
    CLOG_INFO(&LOG,
              2,
              "Color space conversion bypassed for node: %s. To color space is data: %s.",
              b_node->name,
              settings.to_color_space);
    return false;
  }

  if (STREQLEN(
          settings.from_color_space, settings.to_color_space, sizeof(settings.from_color_space))) {
    CLOG_INFO(&LOG,
              2,
              "Color space conversion bypassed for node: %s. To and from are the same: %s.",
              b_node->name,
              settings.from_color_space);
    return false;
  }
  return true;
}

}  // namespace blender::compositor
