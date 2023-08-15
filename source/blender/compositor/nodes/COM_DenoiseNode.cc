/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "COM_DenoiseNode.h"
#include "COM_DenoiseOperation.h"

namespace blender::compositor {

DenoiseNode::DenoiseNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DenoiseNode::convert_to_operations(NodeConverter &converter,
                                        const CompositorContext & /*context*/) const
{
  if (!COM_is_denoise_supported()) {
    converter.map_output_socket(get_output_socket(0),
                                converter.add_input_proxy(get_input_socket(0), false));
    return;
  }

  const bNode *node = this->get_bnode();
  const NodeDenoise *denoise = (const NodeDenoise *)node->storage;

  DenoiseOperation *operation = new DenoiseOperation();
  converter.add_operation(operation);
  operation->set_denoise_settings(denoise);

  converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
  if (denoise && denoise->prefilter == CMP_NODE_DENOISE_PREFILTER_ACCURATE) {
    {
      DenoisePrefilterOperation *normal_prefilter = new DenoisePrefilterOperation(
          DataType::Vector);
      normal_prefilter->set_image_name("normal");
      converter.add_operation(normal_prefilter);
      converter.map_input_socket(get_input_socket(1), normal_prefilter->get_input_socket(0));
      converter.add_link(normal_prefilter->get_output_socket(), operation->get_input_socket(1));
    }
    {
      DenoisePrefilterOperation *albedo_prefilter = new DenoisePrefilterOperation(DataType::Color);
      albedo_prefilter->set_image_name("albedo");
      converter.add_operation(albedo_prefilter);
      converter.map_input_socket(get_input_socket(2), albedo_prefilter->get_input_socket(0));
      converter.add_link(albedo_prefilter->get_output_socket(), operation->get_input_socket(2));
    }
  }
  else {
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));
    converter.map_input_socket(get_input_socket(2), operation->get_input_socket(2));
  }

  converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
}

}  // namespace blender::compositor
