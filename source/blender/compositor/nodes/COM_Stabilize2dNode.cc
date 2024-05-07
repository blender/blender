/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_Stabilize2dNode.h"
#include "COM_MovieClipAttributeOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetSamplerOperation.h"
#include "COM_TranslateOperation.h"

namespace blender::compositor {

Stabilize2dNode::Stabilize2dNode(bNode *editor_node) : Node(editor_node)
{
  /* pass */
}

void Stabilize2dNode::convert_to_operations(NodeConverter &converter,
                                            const CompositorContext &context) const
{
  const bNode *editor_node = this->get_bnode();
  NodeInput *image_input = this->get_input_socket(0);
  MovieClip *clip = (MovieClip *)editor_node->id;
  bool invert = (editor_node->custom2 & CMP_NODE_STABILIZE_FLAG_INVERSE) != 0;
  const PixelSampler sampler = (PixelSampler)editor_node->custom1;

  MovieClipAttributeOperation *scale_attribute = new MovieClipAttributeOperation();
  MovieClipAttributeOperation *angle_attribute = new MovieClipAttributeOperation();
  MovieClipAttributeOperation *x_attribute = new MovieClipAttributeOperation();
  MovieClipAttributeOperation *y_attribute = new MovieClipAttributeOperation();

  scale_attribute->set_attribute(MCA_SCALE);
  scale_attribute->set_framenumber(context.get_framenumber());
  scale_attribute->set_movie_clip(clip);
  scale_attribute->set_invert(invert);

  angle_attribute->set_attribute(MCA_ANGLE);
  angle_attribute->set_framenumber(context.get_framenumber());
  angle_attribute->set_movie_clip(clip);
  angle_attribute->set_invert(invert);

  x_attribute->set_attribute(MCA_X);
  x_attribute->set_framenumber(context.get_framenumber());
  x_attribute->set_movie_clip(clip);
  x_attribute->set_invert(invert);

  y_attribute->set_attribute(MCA_Y);
  y_attribute->set_framenumber(context.get_framenumber());
  y_attribute->set_movie_clip(clip);
  y_attribute->set_invert(invert);

  converter.add_operation(scale_attribute);
  converter.add_operation(angle_attribute);
  converter.add_operation(x_attribute);
  converter.add_operation(y_attribute);

  ScaleRelativeOperation *scale_operation = new ScaleRelativeOperation();
  scale_operation->set_sampler(sampler);
  RotateOperation *rotate_operation = new RotateOperation();
  rotate_operation->set_do_degree2_rad_conversion(false);
  rotate_operation->set_sampler(sampler);
  TranslateOperation *translate_operation = new TranslateCanvasOperation();

  converter.add_operation(scale_operation);
  converter.add_operation(translate_operation);
  converter.add_operation(rotate_operation);

  converter.add_link(scale_attribute->get_output_socket(), scale_operation->get_input_socket(1));
  converter.add_link(scale_attribute->get_output_socket(), scale_operation->get_input_socket(2));

  converter.add_link(angle_attribute->get_output_socket(), rotate_operation->get_input_socket(1));

  converter.add_link(x_attribute->get_output_socket(), translate_operation->get_input_socket(1));
  converter.add_link(y_attribute->get_output_socket(), translate_operation->get_input_socket(2));

  NodeOperationInput *stabilization_socket = nullptr;
  if (invert) {
    /* Translate -> Rotate -> Scale. */
    stabilization_socket = translate_operation->get_input_socket(0);
    converter.map_input_socket(image_input, translate_operation->get_input_socket(0));

    converter.add_link(translate_operation->get_output_socket(),
                       rotate_operation->get_input_socket(0));
    converter.add_link(rotate_operation->get_output_socket(),
                       scale_operation->get_input_socket(0));

    converter.map_output_socket(get_output_socket(), scale_operation->get_output_socket());
  }
  else {
    /* Scale  -> Rotate -> Translate. */
    stabilization_socket = scale_operation->get_input_socket(0);
    converter.map_input_socket(image_input, scale_operation->get_input_socket(0));

    converter.add_link(scale_operation->get_output_socket(),
                       rotate_operation->get_input_socket(0));
    converter.add_link(rotate_operation->get_output_socket(),
                       translate_operation->get_input_socket(0));

    converter.map_output_socket(get_output_socket(), translate_operation->get_output_socket());
  }

  x_attribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
  y_attribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
  scale_attribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
  angle_attribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
}

}  // namespace blender::compositor
