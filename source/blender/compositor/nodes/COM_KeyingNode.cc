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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_KeyingNode.h"

#include "COM_KeyingBlurOperation.h"
#include "COM_KeyingClipOperation.h"
#include "COM_KeyingDespillOperation.h"
#include "COM_KeyingOperation.h"

#include "COM_MathBaseOperation.h"

#include "COM_ConvertOperation.h"
#include "COM_SetValueOperation.h"

#include "COM_DilateErodeOperation.h"

#include "COM_SetAlphaMultiplyOperation.h"

#include "COM_GaussianAlphaXBlurOperation.h"
#include "COM_GaussianAlphaYBlurOperation.h"

namespace blender::compositor {

KeyingNode::KeyingNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

NodeOperationOutput *KeyingNode::setup_pre_blur(NodeConverter &converter,
                                                NodeInput *input_image,
                                                int size) const
{
  ConvertRGBToYCCOperation *convertRGBToYCCOperation = new ConvertRGBToYCCOperation();
  convertRGBToYCCOperation->set_mode(BLI_YCC_ITU_BT709);
  converter.add_operation(convertRGBToYCCOperation);

  converter.map_input_socket(input_image, convertRGBToYCCOperation->get_input_socket(0));

  CombineChannelsOperation *combine_operation = new CombineChannelsOperation();
  converter.add_operation(combine_operation);

  for (int channel = 0; channel < 4; channel++) {
    SeparateChannelOperation *separate_operation = new SeparateChannelOperation();
    separate_operation->set_channel(channel);
    converter.add_operation(separate_operation);

    converter.add_link(convertRGBToYCCOperation->get_output_socket(0),
                       separate_operation->get_input_socket(0));

    if (ELEM(channel, 0, 3)) {
      converter.add_link(separate_operation->get_output_socket(0),
                         combine_operation->get_input_socket(channel));
    }
    else {
      KeyingBlurOperation *blur_xoperation = new KeyingBlurOperation();
      blur_xoperation->set_size(size);
      blur_xoperation->set_axis(KeyingBlurOperation::BLUR_AXIS_X);
      converter.add_operation(blur_xoperation);

      KeyingBlurOperation *blur_yoperation = new KeyingBlurOperation();
      blur_yoperation->set_size(size);
      blur_yoperation->set_axis(KeyingBlurOperation::BLUR_AXIS_Y);
      converter.add_operation(blur_yoperation);

      converter.add_link(separate_operation->get_output_socket(),
                         blur_xoperation->get_input_socket(0));
      converter.add_link(blur_xoperation->get_output_socket(),
                         blur_yoperation->get_input_socket(0));
      converter.add_link(blur_yoperation->get_output_socket(0),
                         combine_operation->get_input_socket(channel));
    }
  }

  ConvertYCCToRGBOperation *convertYCCToRGBOperation = new ConvertYCCToRGBOperation();
  convertYCCToRGBOperation->set_mode(BLI_YCC_ITU_BT709);
  converter.add_operation(convertYCCToRGBOperation);

  converter.add_link(combine_operation->get_output_socket(0),
                     convertYCCToRGBOperation->get_input_socket(0));

  return convertYCCToRGBOperation->get_output_socket(0);
}

NodeOperationOutput *KeyingNode::setup_post_blur(NodeConverter &converter,
                                                 NodeOperationOutput *post_blur_input,
                                                 int size) const
{
  KeyingBlurOperation *blur_xoperation = new KeyingBlurOperation();
  blur_xoperation->set_size(size);
  blur_xoperation->set_axis(KeyingBlurOperation::BLUR_AXIS_X);
  converter.add_operation(blur_xoperation);

  KeyingBlurOperation *blur_yoperation = new KeyingBlurOperation();
  blur_yoperation->set_size(size);
  blur_yoperation->set_axis(KeyingBlurOperation::BLUR_AXIS_Y);
  converter.add_operation(blur_yoperation);

  converter.add_link(post_blur_input, blur_xoperation->get_input_socket(0));
  converter.add_link(blur_xoperation->get_output_socket(), blur_yoperation->get_input_socket(0));

  return blur_yoperation->get_output_socket();
}

NodeOperationOutput *KeyingNode::setup_dilate_erode(NodeConverter &converter,
                                                    NodeOperationOutput *dilate_erode_input,
                                                    int distance) const
{
  DilateDistanceOperation *dilate_erode_operation;
  if (distance > 0) {
    dilate_erode_operation = new DilateDistanceOperation();
    dilate_erode_operation->set_distance(distance);
  }
  else {
    dilate_erode_operation = new ErodeDistanceOperation();
    dilate_erode_operation->set_distance(-distance);
  }
  converter.add_operation(dilate_erode_operation);

  converter.add_link(dilate_erode_input, dilate_erode_operation->get_input_socket(0));

  return dilate_erode_operation->get_output_socket(0);
}

NodeOperationOutput *KeyingNode::setup_feather(NodeConverter &converter,
                                               const CompositorContext &context,
                                               NodeOperationOutput *feather_input,
                                               int falloff,
                                               int distance) const
{
  /* this uses a modified gaussian blur function otherwise its far too slow */
  eCompositorQuality quality = context.get_quality();

  /* initialize node data */
  NodeBlurData data;
  memset(&data, 0, sizeof(NodeBlurData));
  data.filtertype = R_FILTER_GAUSS;
  if (distance > 0) {
    data.sizex = data.sizey = distance;
  }
  else {
    data.sizex = data.sizey = -distance;
  }

  GaussianAlphaXBlurOperation *operationx = new GaussianAlphaXBlurOperation();
  operationx->set_data(&data);
  operationx->set_quality(quality);
  operationx->set_size(1.0f);
  operationx->set_subtract(distance < 0);
  operationx->set_falloff(falloff);
  converter.add_operation(operationx);

  GaussianAlphaYBlurOperation *operationy = new GaussianAlphaYBlurOperation();
  operationy->set_data(&data);
  operationy->set_quality(quality);
  operationy->set_size(1.0f);
  operationy->set_subtract(distance < 0);
  operationy->set_falloff(falloff);
  converter.add_operation(operationy);

  converter.add_link(feather_input, operationx->get_input_socket(0));
  converter.add_link(operationx->get_output_socket(), operationy->get_input_socket(0));

  return operationy->get_output_socket();
}

NodeOperationOutput *KeyingNode::setup_despill(NodeConverter &converter,
                                               NodeOperationOutput *despill_input,
                                               NodeInput *input_screen,
                                               float factor,
                                               float color_balance) const
{
  KeyingDespillOperation *despill_operation = new KeyingDespillOperation();
  despill_operation->set_despill_factor(factor);
  despill_operation->set_color_balance(color_balance);
  converter.add_operation(despill_operation);

  converter.add_link(despill_input, despill_operation->get_input_socket(0));
  converter.map_input_socket(input_screen, despill_operation->get_input_socket(1));

  return despill_operation->get_output_socket(0);
}

NodeOperationOutput *KeyingNode::setup_clip(NodeConverter &converter,
                                            NodeOperationOutput *clip_input,
                                            int kernel_radius,
                                            float kernel_tolerance,
                                            float clip_black,
                                            float clip_white,
                                            bool edge_matte) const
{
  KeyingClipOperation *clip_operation = new KeyingClipOperation();
  clip_operation->set_kernel_radius(kernel_radius);
  clip_operation->set_kernel_tolerance(kernel_tolerance);
  clip_operation->set_clip_black(clip_black);
  clip_operation->set_clip_white(clip_white);
  clip_operation->set_is_edge_matte(edge_matte);
  converter.add_operation(clip_operation);

  converter.add_link(clip_input, clip_operation->get_input_socket(0));

  return clip_operation->get_output_socket(0);
}

void KeyingNode::convert_to_operations(NodeConverter &converter,
                                       const CompositorContext &context) const
{
  bNode *editor_node = this->get_bnode();
  NodeKeyingData *keying_data = (NodeKeyingData *)editor_node->storage;

  NodeInput *input_image = this->get_input_socket(0);
  NodeInput *input_screen = this->get_input_socket(1);
  NodeInput *input_garbage_matte = this->get_input_socket(2);
  NodeInput *input_core_matte = this->get_input_socket(3);
  NodeOutput *output_image = this->get_output_socket(0);
  NodeOutput *output_matte = this->get_output_socket(1);
  NodeOutput *output_edges = this->get_output_socket(2);
  NodeOperationOutput *postprocessed_matte = nullptr, *postprocessed_image = nullptr,
                      *edges_matte = nullptr;

  /* keying operation */
  KeyingOperation *keying_operation = new KeyingOperation();
  keying_operation->set_screen_balance(keying_data->screen_balance);
  converter.add_operation(keying_operation);

  converter.map_input_socket(input_screen, keying_operation->get_input_socket(1));

  if (keying_data->blur_pre) {
    /* Chroma pre-blur operation for input of keying operation. */
    NodeOperationOutput *pre_blurred_image = setup_pre_blur(
        converter, input_image, keying_data->blur_pre);
    converter.add_link(pre_blurred_image, keying_operation->get_input_socket(0));
  }
  else {
    converter.map_input_socket(input_image, keying_operation->get_input_socket(0));
  }

  postprocessed_matte = keying_operation->get_output_socket();

  /* black / white clipping */
  if (keying_data->clip_black > 0.0f || keying_data->clip_white < 1.0f) {
    postprocessed_matte = setup_clip(converter,
                                     postprocessed_matte,
                                     keying_data->edge_kernel_radius,
                                     keying_data->edge_kernel_tolerance,
                                     keying_data->clip_black,
                                     keying_data->clip_white,
                                     false);
  }

  /* output edge matte */
  edges_matte = setup_clip(converter,
                           postprocessed_matte,
                           keying_data->edge_kernel_radius,
                           keying_data->edge_kernel_tolerance,
                           keying_data->clip_black,
                           keying_data->clip_white,
                           true);

  /* apply garbage matte */
  if (input_garbage_matte->is_linked()) {
    SetValueOperation *value_operation = new SetValueOperation();
    value_operation->set_value(1.0f);
    converter.add_operation(value_operation);

    MathSubtractOperation *subtract_operation = new MathSubtractOperation();
    converter.add_operation(subtract_operation);

    MathMinimumOperation *min_operation = new MathMinimumOperation();
    converter.add_operation(min_operation);

    converter.add_link(value_operation->get_output_socket(),
                       subtract_operation->get_input_socket(0));
    converter.map_input_socket(input_garbage_matte, subtract_operation->get_input_socket(1));

    converter.add_link(subtract_operation->get_output_socket(),
                       min_operation->get_input_socket(0));
    converter.add_link(postprocessed_matte, min_operation->get_input_socket(1));

    postprocessed_matte = min_operation->get_output_socket();
  }

  /* apply core matte */
  if (input_core_matte->is_linked()) {
    MathMaximumOperation *max_operation = new MathMaximumOperation();
    converter.add_operation(max_operation);

    converter.map_input_socket(input_core_matte, max_operation->get_input_socket(0));
    converter.add_link(postprocessed_matte, max_operation->get_input_socket(1));

    postprocessed_matte = max_operation->get_output_socket();
  }

  /* apply blur on matte if needed */
  if (keying_data->blur_post) {
    postprocessed_matte = setup_post_blur(converter, postprocessed_matte, keying_data->blur_post);
  }

  /* matte dilate/erode */
  if (keying_data->dilate_distance != 0) {
    postprocessed_matte = setup_dilate_erode(
        converter, postprocessed_matte, keying_data->dilate_distance);
  }

  /* matte feather */
  if (keying_data->feather_distance != 0) {
    postprocessed_matte = setup_feather(converter,
                                        context,
                                        postprocessed_matte,
                                        keying_data->feather_falloff,
                                        keying_data->feather_distance);
  }

  /* set alpha channel to output image */
  SetAlphaMultiplyOperation *alpha_operation = new SetAlphaMultiplyOperation();
  converter.add_operation(alpha_operation);

  converter.map_input_socket(input_image, alpha_operation->get_input_socket(0));
  converter.add_link(postprocessed_matte, alpha_operation->get_input_socket(1));

  postprocessed_image = alpha_operation->get_output_socket();

  /* despill output image */
  if (keying_data->despill_factor > 0.0f) {
    postprocessed_image = setup_despill(converter,
                                        postprocessed_image,
                                        input_screen,
                                        keying_data->despill_factor,
                                        keying_data->despill_balance);
  }

  /* connect result to output sockets */
  converter.map_output_socket(output_image, postprocessed_image);
  converter.map_output_socket(output_matte, postprocessed_matte);

  if (edges_matte) {
    converter.map_output_socket(output_edges, edges_matte);
  }
}

}  // namespace blender::compositor
