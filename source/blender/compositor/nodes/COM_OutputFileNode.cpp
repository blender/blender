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

#include "COM_OutputFileNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_OutputFileMultiViewOperation.h"
#include "COM_OutputFileOperation.h"

#include "BKE_scene.h"

#include "BLI_path_util.h"

OutputFileNode::OutputFileNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void OutputFileNode::convertToOperations(NodeConverter &converter,
                                         const CompositorContext &context) const
{
  NodeImageMultiFile *storage = (NodeImageMultiFile *)this->getbNode()->storage;
  const bool is_multiview = (context.getRenderData()->scemode & R_MULTIVIEW) != 0;

  if (!context.isRendering()) {
    /* only output files when rendering a sequence -
     * otherwise, it overwrites the output files just
     * scrubbing through the timeline when the compositor updates.
     */
    return;
  }

  if (storage->format.imtype == R_IMF_IMTYPE_MULTILAYER) {
    const bool use_half_float = (storage->format.depth == R_IMF_CHAN_DEPTH_16);
    /* single output operation for the multilayer file */
    OutputOpenExrMultiLayerOperation *outputOperation;

    if (is_multiview && storage->format.views_format == R_IMF_VIEWS_MULTIVIEW) {
      outputOperation = new OutputOpenExrMultiLayerMultiViewOperation(context.getScene(),
                                                                      context.getRenderData(),
                                                                      context.getbNodeTree(),
                                                                      storage->base_path,
                                                                      storage->format.exr_codec,
                                                                      use_half_float,
                                                                      context.getViewName());
    }
    else {
      outputOperation = new OutputOpenExrMultiLayerOperation(context.getScene(),
                                                             context.getRenderData(),
                                                             context.getbNodeTree(),
                                                             storage->base_path,
                                                             storage->format.exr_codec,
                                                             use_half_float,
                                                             context.getViewName());
    }
    converter.addOperation(outputOperation);

    int num_inputs = getNumberOfInputSockets();
    bool previewAdded = false;
    for (int i = 0; i < num_inputs; i++) {
      NodeInput *input = getInputSocket(i);
      NodeImageMultiFileSocket *sockdata =
          (NodeImageMultiFileSocket *)input->getbNodeSocket()->storage;

      /* note: layer becomes an empty placeholder if the input is not linked */
      outputOperation->add_layer(sockdata->layer, input->getDataType(), input->isLinked());

      converter.mapInputSocket(input, outputOperation->getInputSocket(i));

      if (!previewAdded) {
        converter.addNodeInputPreview(input);
        previewAdded = true;
      }
    }
  }
  else { /* single layer format */
    int num_inputs = getNumberOfInputSockets();
    bool previewAdded = false;
    for (int i = 0; i < num_inputs; i++) {
      NodeInput *input = getInputSocket(i);
      if (input->isLinked()) {
        NodeImageMultiFileSocket *sockdata =
            (NodeImageMultiFileSocket *)input->getbNodeSocket()->storage;
        ImageFormatData *format = (sockdata->use_node_format ? &storage->format :
                                                               &sockdata->format);
        char path[FILE_MAX];

        /* combine file path for the input */
        BLI_join_dirfile(path, FILE_MAX, storage->base_path, sockdata->path);

        NodeOperation *outputOperation = nullptr;

        if (is_multiview && format->views_format == R_IMF_VIEWS_MULTIVIEW) {
          outputOperation = new OutputOpenExrSingleLayerMultiViewOperation(
              context.getRenderData(),
              context.getbNodeTree(),
              input->getDataType(),
              format,
              path,
              context.getViewSettings(),
              context.getDisplaySettings(),
              context.getViewName(),
              sockdata->save_as_render);
        }
        else if ((!is_multiview) || (format->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
          outputOperation = new OutputSingleLayerOperation(context.getRenderData(),
                                                           context.getbNodeTree(),
                                                           input->getDataType(),
                                                           format,
                                                           path,
                                                           context.getViewSettings(),
                                                           context.getDisplaySettings(),
                                                           context.getViewName(),
                                                           sockdata->save_as_render);
        }
        else { /* R_IMF_VIEWS_STEREO_3D */
          outputOperation = new OutputStereoOperation(context.getRenderData(),
                                                      context.getbNodeTree(),
                                                      input->getDataType(),
                                                      format,
                                                      path,
                                                      sockdata->layer,
                                                      context.getViewSettings(),
                                                      context.getDisplaySettings(),
                                                      context.getViewName(),
                                                      sockdata->save_as_render);
        }

        converter.addOperation(outputOperation);
        converter.mapInputSocket(input, outputOperation->getInputSocket(0));

        if (!previewAdded) {
          converter.addNodeInputPreview(input);
          previewAdded = true;
        }
      }
    }
  }
}
