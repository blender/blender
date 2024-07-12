/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_FileOutputNode.h"

#include "BLI_string.h"

namespace blender::compositor {

FileOutputNode::FileOutputNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

static DataType get_input_data_type(NodeInput *input)
{
  const DataType data_type = input->get_data_type();
  /* Incoming inputs to vector sockets can be 4D, so we declare them as 4-channel color inputs to
   * avoid loss of fourth channel due to implicit conversion. The operation will look into the
   * is_4d_vector meta data member of the input to check if it should be written as 4D or 3D, where
   * the last channel will be ignored in the 3D case. */
  if (data_type == DataType::Vector) {
    return DataType::Color;
  }
  return data_type;
}

void FileOutputNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  for (NodeInput *input : inputs_) {
    if (input->is_linked()) {
      converter.add_node_input_preview(input);
      break;
    }
  }

  if (!context.is_rendering()) {
    return;
  }

  Vector<FileOutputInput> inputs;
  for (NodeInput *input : inputs_) {
    auto *storage = static_cast<NodeImageMultiFileSocket *>(input->get_bnode_socket()->storage);
    inputs.append(FileOutputInput(storage, get_input_data_type(input), input->get_data_type()));
  }

  auto *storage = static_cast<const NodeImageMultiFile *>(this->get_bnode()->storage);
  auto *output_operation = new FileOutputOperation(&context, storage, inputs);
  converter.add_operation(output_operation);

  for (int i = 0; i < inputs_.size(); i++) {
    converter.map_input_socket(inputs_[i], output_operation->get_input_socket(i));
  }
}

}  // namespace blender::compositor
