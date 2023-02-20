/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include "COM_OutputFileNode.h"

namespace blender::compositor {

OutputFileNode::OutputFileNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void OutputFileNode::add_input_sockets(OutputOpenExrMultiLayerOperation &operation) const
{
  for (NodeInput *input : inputs_) {
    NodeImageMultiFileSocket *sockdata =
        (NodeImageMultiFileSocket *)input->get_bnode_socket()->storage;
    /* NOTE: layer becomes an empty placeholder if the input is not linked. */
    operation.add_layer(sockdata->layer, input->get_data_type(), input->is_linked());
  }
}

void OutputFileNode::map_input_sockets(NodeConverter &converter,
                                       OutputOpenExrMultiLayerOperation &operation) const
{
  bool preview_added = false;
  int index = 0;
  for (NodeInput *input : inputs_) {
    converter.map_input_socket(input, operation.get_input_socket(index++));

    if (!preview_added) {
      converter.add_node_input_preview(input);
      preview_added = true;
    }
  }
}

void OutputFileNode::add_preview_to_first_linked_input(NodeConverter &converter) const
{
  if (get_input_sockets().is_empty()) {
    return;
  }

  NodeInput *first_socket = this->get_input_socket(0);
  if (first_socket->is_linked()) {
    converter.add_node_input_preview(first_socket);
  }
}

void OutputFileNode::convert_to_operations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
  const NodeImageMultiFile *storage = (const NodeImageMultiFile *)this->get_bnode()->storage;
  const bool is_multiview = (context.get_render_data()->scemode & R_MULTIVIEW) != 0;

  add_preview_to_first_linked_input(converter);

  if (!context.is_rendering()) {
    /* only output files when rendering a sequence -
     * otherwise, it overwrites the output files just
     * scrubbing through the timeline when the compositor updates.
     */
    return;
  }

  if (storage->format.imtype == R_IMF_IMTYPE_MULTILAYER) {
    const bool use_half_float = (storage->format.depth == R_IMF_CHAN_DEPTH_16);
    /* Single output operation for the multi-layer file. */
    OutputOpenExrMultiLayerOperation *output_operation;

    if (is_multiview && storage->format.views_format == R_IMF_VIEWS_MULTIVIEW) {
      output_operation = new OutputOpenExrMultiLayerMultiViewOperation(context.get_scene(),
                                                                       context.get_render_data(),
                                                                       context.get_bnodetree(),
                                                                       storage->base_path,
                                                                       storage->format.exr_codec,
                                                                       use_half_float,
                                                                       context.get_view_name());
    }
    else {
      output_operation = new OutputOpenExrMultiLayerOperation(context.get_scene(),
                                                              context.get_render_data(),
                                                              context.get_bnodetree(),
                                                              storage->base_path,
                                                              storage->format.exr_codec,
                                                              use_half_float,
                                                              context.get_view_name());
    }
    converter.add_operation(output_operation);

    /* First add all inputs. Inputs are stored in a Vector and can be moved to a different
     * memory address during this time. */
    add_input_sockets(*output_operation);
    /* After adding the sockets the memory addresses will stick. */
    map_input_sockets(converter, *output_operation);
  }
  else { /* single layer format */
    for (NodeInput *input : inputs_) {
      if (input->is_linked()) {
        NodeImageMultiFileSocket *sockdata =
            (NodeImageMultiFileSocket *)input->get_bnode_socket()->storage;
        const ImageFormatData *format = (sockdata->use_node_format ? &storage->format :
                                                                     &sockdata->format);
        char path[FILE_MAX];

        /* combine file path for the input */
        if (sockdata->path[0]) {
          BLI_path_join(path, FILE_MAX, storage->base_path, sockdata->path);
        }
        else {
          BLI_strncpy(path, storage->base_path, FILE_MAX);
          BLI_path_slash_ensure(path, FILE_MAX);
        }

        NodeOperation *output_operation = nullptr;

        if (is_multiview && format->views_format == R_IMF_VIEWS_MULTIVIEW) {
          output_operation = new OutputOpenExrSingleLayerMultiViewOperation(
              context.get_scene(),
              context.get_render_data(),
              context.get_bnodetree(),
              input->get_data_type(),
              format,
              path,
              context.get_view_name(),
              sockdata->save_as_render);
        }
        else if ((!is_multiview) || (format->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
          output_operation = new OutputSingleLayerOperation(context.get_scene(),
                                                            context.get_render_data(),
                                                            context.get_bnodetree(),
                                                            input->get_data_type(),
                                                            format,
                                                            path,
                                                            context.get_view_name(),
                                                            sockdata->save_as_render);
        }
        else { /* R_IMF_VIEWS_STEREO_3D */
          output_operation = new OutputStereoOperation(context.get_scene(),
                                                       context.get_render_data(),
                                                       context.get_bnodetree(),
                                                       input->get_data_type(),
                                                       format,
                                                       path,
                                                       sockdata->layer,
                                                       context.get_view_name(),
                                                       sockdata->save_as_render);
        }

        converter.add_operation(output_operation);
        converter.map_input_socket(input, output_operation->get_input_socket(0));
      }
    }
  }
}

}  // namespace blender::compositor
