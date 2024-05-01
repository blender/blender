/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TranslateNode.h"

#include "COM_TranslateOperation.h"

namespace blender::compositor {

TranslateNode::TranslateNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void TranslateNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext & /*context*/) const
{
  const bNode *bnode = this->get_bnode();
  const NodeTranslateData *data = (const NodeTranslateData *)bnode->storage;

  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_xsocket = this->get_input_socket(1);
  NodeInput *input_ysocket = this->get_input_socket(2);
  NodeOutput *output_socket = this->get_output_socket(0);

  TranslateCanvasOperation *operation = new TranslateCanvasOperation();
  operation->set_wrapping(data->wrap_axis);
  operation->set_is_relative(data->relative);

  switch (data->interpolation) {
    case CMP_NODE_INTERPOLATION_NEAREST:
      operation->set_sampler(PixelSampler::Nearest);
      break;
    case CMP_NODE_INTERPOLATION_BILINEAR:
      operation->set_sampler(PixelSampler::Bilinear);
      break;
    case CMP_NODE_INTERPOLATION_BICUBIC:
      operation->set_sampler(PixelSampler::Bicubic);
      break;
  }

  converter.add_operation(operation);
  converter.map_input_socket(input_xsocket, operation->get_input_socket(1));
  converter.map_input_socket(input_ysocket, operation->get_input_socket(2));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
  converter.map_input_socket(input_socket, operation->get_input_socket(0));
}

}  // namespace blender::compositor
