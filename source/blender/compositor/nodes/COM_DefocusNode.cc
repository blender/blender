/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DefocusNode.h"
#include "COM_BokehImageOperation.h"
#include "COM_ConvertDepthToRadiusOperation.h"
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
  Scene *scene = node->id ? (Scene *)node->id : context.get_scene();
  Object *camob = scene ? scene->camera : nullptr;

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
    radius_op->set_camera_object(camob);
    radius_op->setf_stop(data->fstop);
    radius_op->set_max_radius(data->maxblur);
    converter.add_operation(radius_op);

    converter.map_input_socket(get_input_socket(1), radius_op->get_input_socket(0));

    FastGaussianBlurValueOperation *blur = new FastGaussianBlurValueOperation();
    /* maintain close pixels so far Z values don't bleed into the foreground */
    blur->set_overlay(FAST_GAUSS_OVERLAY_MIN);
    converter.add_operation(blur);

    converter.add_link(radius_op->get_output_socket(0), blur->get_input_socket(0));
    radius_op->set_post_blur(blur);

    radius_operation = blur;
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
  bokeh->delete_data_on_finish();
  converter.add_operation(bokeh);

#ifdef COM_DEFOCUS_SEARCH
  InverseSearchRadiusOperation *search = new InverseSearchRadiusOperation();
  search->set_max_blur(data->maxblur);
  converter.add_operation(search);

  converter.add_link(radius_operation->get_output_socket(0), search->get_input_socket(0));
#endif

  VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
  if (data->preview) {
    operation->set_quality(eCompositorQuality::Low);
  }
  else {
    operation->set_quality(context.get_quality());
  }
  operation->set_max_blur(data->maxblur);
  operation->set_threshold(data->bthresh);
  converter.add_operation(operation);

  converter.add_link(bokeh->get_output_socket(), operation->get_input_socket(1));
  converter.add_link(radius_operation->get_output_socket(), operation->get_input_socket(2));
#ifdef COM_DEFOCUS_SEARCH
  converter.add_link(search->get_output_socket(), operation->get_input_socket(3));
#endif

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

}  // namespace blender::compositor
