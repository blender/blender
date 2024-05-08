/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_scene_types.h"

#include "BKE_camera.h"

#include "COM_BokehImageOperation.h"
#include "COM_ConvertDepthToRadiusOperation.h"
#include "COM_DefocusNode.h"
#include "COM_FastGaussianBlurOperation.h"
#include "COM_GammaCorrectOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"

namespace blender::compositor {

DefocusNode::DefocusNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void DefocusNode::convert_to_operations(NodeConverter &converter,
                                        const CompositorContext &context) const
{
  const bNode *node = this->get_bnode();
  const NodeDefocus *data = (const NodeDefocus *)node->storage;

  NodeOperation *radius_operation;
  if (data->no_zbuf) {
    MathMultiplyOperation *multiply = new MathMultiplyOperation();
    SetValueOperation *multiplier = new SetValueOperation();
    multiplier->set_value(data->scale);
    SetValueOperation *max_radius = new SetValueOperation();
    max_radius->set_value(data->maxblur);
    MathMinimumOperation *minimize = new MathMinimumOperation();

    converter.add_operation(multiply);
    converter.add_operation(multiplier);
    converter.add_operation(max_radius);
    converter.add_operation(minimize);

    converter.map_input_socket(get_input_socket(1), multiply->get_input_socket(0));
    converter.add_link(multiplier->get_output_socket(), multiply->get_input_socket(1));
    converter.add_link(multiply->get_output_socket(), minimize->get_input_socket(0));
    converter.add_link(max_radius->get_output_socket(), minimize->get_input_socket(1));

    radius_operation = minimize;
  }
  else {
    ConvertDepthToRadiusOperation *radius_op = new ConvertDepthToRadiusOperation();
    radius_op->set_data(data);
    radius_op->set_scene(get_scene(context));
    converter.add_operation(radius_op);
    converter.map_input_socket(get_input_socket(1), radius_op->get_input_socket(0));
    converter.map_input_socket(get_input_socket(0), radius_op->get_input_socket(1));

    GaussianXBlurOperation *blur_x_operation = new GaussianXBlurOperation();
    converter.add_operation(blur_x_operation);
    converter.add_link(radius_op->get_output_socket(), blur_x_operation->get_input_socket(0));

    GaussianYBlurOperation *blur_y_operation = new GaussianYBlurOperation();
    converter.add_operation(blur_y_operation);
    converter.add_link(blur_x_operation->get_output_socket(),
                       blur_y_operation->get_input_socket(0));

    MathMinimumOperation *minimum_operation = new MathMinimumOperation();
    converter.add_operation(minimum_operation);
    converter.add_link(blur_y_operation->get_output_socket(),
                       minimum_operation->get_input_socket(0));
    converter.add_link(radius_op->get_output_socket(), minimum_operation->get_input_socket(1));

    radius_op->set_blur_x_operation(blur_x_operation);
    radius_op->set_blur_y_operation(blur_y_operation);

    radius_operation = minimum_operation;
  }

  NodeBokehImage *bokehdata = new NodeBokehImage();
  bokehdata->angle = data->rotation;
  bokehdata->rounding = 0.0f;
  bokehdata->flaps = data->bktype;
  if (data->bktype < 3) {
    bokehdata->flaps = 5;
    bokehdata->rounding = 1.0f;
  }
  bokehdata->catadioptric = 0.0f;
  bokehdata->lensshift = 0.0f;

  BokehImageOperation *bokeh = new BokehImageOperation();
  bokeh->set_data(bokehdata);
  bokeh->set_resolution(math::ceil(data->maxblur) * 2 + 1);
  bokeh->delete_data_on_finish();
  converter.add_operation(bokeh);

  SetValueOperation *bounding_box_operation = new SetValueOperation();
  bounding_box_operation->set_value(1.0f);
  converter.add_operation(bounding_box_operation);

  VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
  operation->set_max_blur(data->maxblur);
  operation->set_threshold(0.0f);
  converter.add_operation(operation);

  converter.add_link(bokeh->get_output_socket(), operation->get_input_socket(1));
  converter.add_link(radius_operation->get_output_socket(), operation->get_input_socket(2));
  converter.add_link(bounding_box_operation->get_output_socket(), operation->get_input_socket(3));

  if (data->gamco) {
    GammaCorrectOperation *correct = new GammaCorrectOperation();
    converter.add_operation(correct);
    GammaUncorrectOperation *inverse = new GammaUncorrectOperation();
    converter.add_operation(inverse);

    converter.map_input_socket(get_input_socket(0), correct->get_input_socket(0));
    converter.add_link(correct->get_output_socket(), operation->get_input_socket(0));
    converter.add_link(operation->get_output_socket(), inverse->get_input_socket(0));
    converter.map_output_socket(get_output_socket(), inverse->get_output_socket());
  }
  else {
    converter.map_input_socket(get_input_socket(0), operation->get_input_socket(0));
    converter.map_output_socket(get_output_socket(), operation->get_output_socket());
  }
}

const Scene *DefocusNode::get_scene(const CompositorContext &context) const
{
  return get_bnode()->id ? reinterpret_cast<Scene *>(get_bnode()->id) : context.get_scene();
}

}  // namespace blender::compositor
