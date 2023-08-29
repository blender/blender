/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TranslateNode.h"

#include "COM_TranslateOperation.h"
#include "COM_WrapOperation.h"
#include "COM_WriteBufferOperation.h"

namespace blender::compositor {

TranslateNode::TranslateNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void TranslateNode::convert_to_operations(NodeConverter &converter,
                                          const CompositorContext &context) const
{
  const bNode *bnode = this->get_bnode();
  const NodeTranslateData *data = (const NodeTranslateData *)bnode->storage;

  NodeInput *input_socket = this->get_input_socket(0);
  NodeInput *input_xsocket = this->get_input_socket(1);
  NodeInput *input_ysocket = this->get_input_socket(2);
  NodeOutput *output_socket = this->get_output_socket(0);

  TranslateOperation *operation = context.get_execution_model() == eExecutionModel::Tiled ?
                                      new TranslateOperation() :
                                      new TranslateCanvasOperation();
  operation->set_wrapping(data->wrap_axis);
  if (data->relative) {
    const RenderData *rd = context.get_render_data();
    const float render_size_factor = context.get_render_percentage_as_factor();
    float fx = rd->xsch * render_size_factor;
    float fy = rd->ysch * render_size_factor;

    operation->setFactorXY(fx, fy);
  }

  converter.add_operation(operation);
  converter.map_input_socket(input_xsocket, operation->get_input_socket(1));
  converter.map_input_socket(input_ysocket, operation->get_input_socket(2));
  converter.map_output_socket(output_socket, operation->get_output_socket(0));
  if (data->wrap_axis && context.get_execution_model() != eExecutionModel::FullFrame) {
    /* TODO: To be removed with tiled implementation. */
    WriteBufferOperation *write_operation = new WriteBufferOperation(DataType::Color);
    WrapOperation *wrap_operation = new WrapOperation(DataType::Color);
    wrap_operation->set_memory_proxy(write_operation->get_memory_proxy());
    wrap_operation->set_wrapping(data->wrap_axis);

    converter.add_operation(write_operation);
    converter.add_operation(wrap_operation);
    converter.map_input_socket(input_socket, write_operation->get_input_socket(0));
    converter.add_link(wrap_operation->get_output_socket(), operation->get_input_socket(0));
  }
  else {
    converter.map_input_socket(input_socket, operation->get_input_socket(0));
  }
}

}  // namespace blender::compositor
