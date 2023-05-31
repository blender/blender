/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_BlurNode.h"
#include "COM_FastGaussianBlurOperation.h"
#include "COM_GammaCorrectOperation.h"
#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"
#include "COM_GaussianBokehBlurOperation.h"
#include "COM_GaussianXBlurOperation.h"
#include "COM_GaussianYBlurOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"

namespace blender::compositor {

BlurNode::BlurNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void BlurNode::convert_to_operations(NodeConverter &converter,
                                     const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  const NodeBlurData *data = (const NodeBlurData *)editor_node->storage;
  NodeInput *input_size_socket = this->get_input_socket(1);
  bool connected_size_socket = input_size_socket->is_linked();

  const float size = this->get_input_socket(1)->get_editor_value_float();
  const bool extend_bounds = (editor_node->custom1 & CMP_NODEFLAG_BLUR_EXTEND_BOUNDS) != 0;

  eCompositorQuality quality = context.get_quality();
  NodeOperation *input_operation = nullptr, *output_operation = nullptr;

  if (data->filtertype == R_FILTER_FAST_GAUSS) {
    FastGaussianBlurOperation *operationfgb = new FastGaussianBlurOperation();
    operationfgb->set_data(data);
    operationfgb->set_extend_bounds(extend_bounds);
    converter.add_operation(operationfgb);

    converter.map_input_socket(get_input_socket(1), operationfgb->get_input_socket(1));

    input_operation = operationfgb;
    output_operation = operationfgb;
  }
  else if (editor_node->custom1 & CMP_NODEFLAG_BLUR_VARIABLE_SIZE) {
    MathAddOperation *clamp = new MathAddOperation();
    SetValueOperation *zero = new SetValueOperation();
    zero->set_value(0.0f);
    clamp->set_use_clamp(true);

    converter.add_operation(clamp);
    converter.add_operation(zero);
    converter.map_input_socket(get_input_socket(1), clamp->get_input_socket(0));
    converter.add_link(zero->get_output_socket(), clamp->get_input_socket(1));

    GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
    operationx->set_data(data);
    operationx->set_quality(quality);
    operationx->set_size(1.0f);
    operationx->set_falloff(PROP_SMOOTH);
    operationx->set_subtract(false);
    operationx->set_extend_bounds(extend_bounds);

    converter.add_operation(operationx);
    converter.add_link(clamp->get_output_socket(), operationx->get_input_socket(0));

    GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
    operationy->set_data(data);
    operationy->set_quality(quality);
    operationy->set_size(1.0f);
    operationy->set_falloff(PROP_SMOOTH);
    operationy->set_subtract(false);
    operationy->set_extend_bounds(extend_bounds);

    converter.add_operation(operationy);
    converter.add_link(operationx->get_output_socket(), operationy->get_input_socket(0));

    GaussianBlurReferenceOperation *operation = new GaussianBlurReferenceOperation();
    operation->set_data(data);
    operation->set_quality(quality);
    operation->set_extend_bounds(extend_bounds);

    converter.add_operation(operation);
    converter.add_link(operationy->get_output_socket(), operation->get_input_socket(1));

    output_operation = operation;
    input_operation = operation;
  }
  else if (!data->bokeh) {
    GaussianXBlurOperation *operationx = new GaussianXBlurOperation();
    operationx->set_data(data);
    operationx->set_quality(quality);
    operationx->check_opencl();
    operationx->set_extend_bounds(extend_bounds);

    converter.add_operation(operationx);
    converter.map_input_socket(get_input_socket(1), operationx->get_input_socket(1));

    GaussianYBlurOperation *operationy = new GaussianYBlurOperation();
    operationy->set_data(data);
    operationy->set_quality(quality);
    operationy->check_opencl();
    operationy->set_extend_bounds(extend_bounds);

    converter.add_operation(operationy);
    converter.map_input_socket(get_input_socket(1), operationy->get_input_socket(1));
    converter.add_link(operationx->get_output_socket(), operationy->get_input_socket(0));

    if (!connected_size_socket) {
      operationx->set_size(size);
      operationy->set_size(size);
    }

    input_operation = operationx;
    output_operation = operationy;
  }
  else {
    GaussianBokehBlurOperation *operation = new GaussianBokehBlurOperation();
    operation->set_data(data);
    operation->set_quality(quality);
    operation->set_extend_bounds(extend_bounds);

    converter.add_operation(operation);
    converter.map_input_socket(get_input_socket(1), operation->get_input_socket(1));

    if (!connected_size_socket) {
      operation->set_size(size);
    }

    input_operation = operation;
    output_operation = operation;
  }

  if (data->gamma) {
    GammaCorrectOperation *correct = new GammaCorrectOperation();
    GammaUncorrectOperation *inverse = new GammaUncorrectOperation();
    converter.add_operation(correct);
    converter.add_operation(inverse);

    converter.map_input_socket(get_input_socket(0), correct->get_input_socket(0));
    converter.add_link(correct->get_output_socket(), input_operation->get_input_socket(0));
    converter.add_link(output_operation->get_output_socket(), inverse->get_input_socket(0));
    converter.map_output_socket(get_output_socket(), inverse->get_output_socket());

    converter.add_preview(inverse->get_output_socket());
  }
  else {
    converter.map_input_socket(get_input_socket(0), input_operation->get_input_socket(0));
    converter.map_output_socket(get_output_socket(), output_operation->get_output_socket());

    converter.add_preview(output_operation->get_output_socket());
  }
}

}  // namespace blender::compositor
