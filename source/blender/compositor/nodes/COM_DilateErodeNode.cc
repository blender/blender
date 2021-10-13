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

#include "COM_DilateErodeNode.h"
#include "COM_AntiAliasOperation.h"
#include "COM_DilateErodeOperation.h"
#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"

namespace blender::compositor {

DilateErodeNode::DilateErodeNode(bNode *editor_node) : Node(editor_node)
{
  /* initialize node data */
  NodeBlurData *data = &alpha_blur_;
  memset(data, 0, sizeof(NodeBlurData));
  data->filtertype = R_FILTER_GAUSS;

  if (editor_node->custom2 > 0) {
    data->sizex = data->sizey = editor_node->custom2;
  }
  else {
    data->sizex = data->sizey = -editor_node->custom2;
  }
}

void DilateErodeNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext &context) const
{

  bNode *editor_node = this->get_bnode();
  if (editor_node->custom1 == CMP_NODE_DILATEERODE_DISTANCE_THRESH) {
    DilateErodeThresholdOperation *operation = new DilateErodeThresholdOperation();
    operation->set_distance(editor_node->custom2);
    operation->set_inset(editor_node->custom3);
    converter.add_operation(operation);

    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));

    if (editor_node->custom3 < 2.0f) {
      AntiAliasOperation *anti_alias = new AntiAliasOperation();
      converter.add_operation(anti_alias);

      converter.add_link(operation->get_output_socket(), anti_alias->get_input_socket(0));
      converter.map_output_socket(get_output_socket(0), anti_alias->get_output_socket(0));
    }
    else {
      converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
    }
  }
  else if (editor_node->custom1 == CMP_NODE_DILATEERODE_DISTANCE) {
    if (editor_node->custom2 > 0) {
      DilateDistanceOperation *operation = new DilateDistanceOperation();
      operation->set_distance(editor_node->custom2);
      converter.add_operation(operation);

      converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
      converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
    }
    else {
      ErodeDistanceOperation *operation = new ErodeDistanceOperation();
      operation->set_distance(-editor_node->custom2);
      converter.add_operation(operation);

      converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
      converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
    }
  }
  else if (editor_node->custom1 == CMP_NODE_DILATEERODE_DISTANCE_FEATHER) {
    /* this uses a modified gaussian blur function otherwise its far too slow */
    eCompositorQuality quality = context.get_quality();

    GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
    operationx->set_data(&alpha_blur_);
    operationx->set_quality(quality);
    operationx->set_falloff(PROP_SMOOTH);
    converter.add_operation(operationx);

    converter.map_input_socket(get_input_socket(0), operationx->get_input_socket(0));
    // converter.map_input_socket(get_input_socket(1), operationx->get_input_socket(1)); // no size
    // input yet

    GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
    operationy->set_data(&alpha_blur_);
    operationy->set_quality(quality);
    operationy->set_falloff(PROP_SMOOTH);
    converter.add_operation(operationy);

    converter.add_link(operationx->get_output_socket(), operationy->get_input_socket(0));
    // converter.map_input_socket(get_input_socket(1), operationy->get_input_socket(1)); // no size
    // input yet
    converter.map_output_socket(get_output_socket(0), operationy->get_output_socket());

    converter.add_preview(operationy->get_output_socket());

    /* TODO? */
    /* see gaussian blue node for original usage */
#if 0
    if (!connected_size_socket) {
      operationx->set_size(size);
      operationy->set_size(size);
    }
#else
    operationx->set_size(1.0f);
    operationy->set_size(1.0f);
#endif
    operationx->set_subtract(editor_node->custom2 < 0);
    operationy->set_subtract(editor_node->custom2 < 0);

    if (editor_node->storage) {
      NodeDilateErode *data_storage = (NodeDilateErode *)editor_node->storage;
      operationx->set_falloff(data_storage->falloff);
      operationy->set_falloff(data_storage->falloff);
    }
  }
  else {
    if (editor_node->custom2 > 0) {
      DilateStepOperation *operation = new DilateStepOperation();
      operation->set_iterations(editor_node->custom2);
      converter.add_operation(operation);

      converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
      converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
    }
    else {
      ErodeStepOperation *operation = new ErodeStepOperation();
      operation->set_iterations(-editor_node->custom2);
      converter.add_operation(operation);

      converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
      converter.map_output_socket(get_output_socket(0), operation->get_output_socket(0));
    }
  }
}

}  // namespace blender::compositor
