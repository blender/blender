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

#include "COM_Stabilize2dNode.h"
#include "COM_MovieClipAttributeOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetSamplerOperation.h"
#include "COM_TranslateOperation.h"

namespace blender::compositor {

Stabilize2dNode::Stabilize2dNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}

void Stabilize2dNode::convertToOperations(NodeConverter &converter,
                                          const CompositorContext &context) const
{
  bNode *editorNode = this->getbNode();
  NodeInput *imageInput = this->getInputSocket(0);
  MovieClip *clip = (MovieClip *)editorNode->id;
  bool invert = (editorNode->custom2 & CMP_NODEFLAG_STABILIZE_INVERSE) != 0;
  const PixelSampler sampler = (PixelSampler)editorNode->custom1;

  MovieClipAttributeOperation *scaleAttribute = new MovieClipAttributeOperation();
  MovieClipAttributeOperation *angleAttribute = new MovieClipAttributeOperation();
  MovieClipAttributeOperation *xAttribute = new MovieClipAttributeOperation();
  MovieClipAttributeOperation *yAttribute = new MovieClipAttributeOperation();

  scaleAttribute->setAttribute(MCA_SCALE);
  scaleAttribute->setFramenumber(context.getFramenumber());
  scaleAttribute->setMovieClip(clip);
  scaleAttribute->setInvert(invert);

  angleAttribute->setAttribute(MCA_ANGLE);
  angleAttribute->setFramenumber(context.getFramenumber());
  angleAttribute->setMovieClip(clip);
  angleAttribute->setInvert(invert);

  xAttribute->setAttribute(MCA_X);
  xAttribute->setFramenumber(context.getFramenumber());
  xAttribute->setMovieClip(clip);
  xAttribute->setInvert(invert);

  yAttribute->setAttribute(MCA_Y);
  yAttribute->setFramenumber(context.getFramenumber());
  yAttribute->setMovieClip(clip);
  yAttribute->setInvert(invert);

  converter.addOperation(scaleAttribute);
  converter.addOperation(angleAttribute);
  converter.addOperation(xAttribute);
  converter.addOperation(yAttribute);

  switch (context.get_execution_model()) {
    case eExecutionModel::Tiled: {
      ScaleRelativeOperation *scaleOperation = new ScaleRelativeOperation();
      scaleOperation->setSampler(sampler);
      RotateOperation *rotateOperation = new RotateOperation();
      rotateOperation->setDoDegree2RadConversion(false);
      TranslateOperation *translateOperation = new TranslateOperation();
      SetSamplerOperation *psoperation = new SetSamplerOperation();
      psoperation->setSampler(sampler);

      converter.addOperation(scaleOperation);
      converter.addOperation(translateOperation);
      converter.addOperation(rotateOperation);
      converter.addOperation(psoperation);

      converter.addLink(scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(1));
      converter.addLink(scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(2));

      converter.addLink(angleAttribute->getOutputSocket(), rotateOperation->getInputSocket(1));

      converter.addLink(xAttribute->getOutputSocket(), translateOperation->getInputSocket(1));
      converter.addLink(yAttribute->getOutputSocket(), translateOperation->getInputSocket(2));

      converter.mapOutputSocket(getOutputSocket(), psoperation->getOutputSocket());

      if (invert) {
        /* Translate -> Rotate -> Scale. */
        converter.mapInputSocket(imageInput, translateOperation->getInputSocket(0));

        converter.addLink(translateOperation->getOutputSocket(),
                          rotateOperation->getInputSocket(0));
        converter.addLink(rotateOperation->getOutputSocket(), scaleOperation->getInputSocket(0));

        converter.addLink(scaleOperation->getOutputSocket(), psoperation->getInputSocket(0));
      }
      else {
        /* Scale  -> Rotate -> Translate. */
        converter.mapInputSocket(imageInput, scaleOperation->getInputSocket(0));

        converter.addLink(scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
        converter.addLink(rotateOperation->getOutputSocket(),
                          translateOperation->getInputSocket(0));

        converter.addLink(translateOperation->getOutputSocket(), psoperation->getInputSocket(0));
      }
      break;
    }
    case eExecutionModel::FullFrame: {
      ScaleRelativeOperation *scaleOperation = new ScaleRelativeOperation();
      scaleOperation->setSampler(sampler);
      RotateOperation *rotateOperation = new RotateOperation();
      rotateOperation->setDoDegree2RadConversion(false);
      rotateOperation->set_sampler(sampler);
      TranslateOperation *translateOperation = new TranslateCanvasOperation();

      converter.addOperation(scaleOperation);
      converter.addOperation(translateOperation);
      converter.addOperation(rotateOperation);

      converter.addLink(scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(1));
      converter.addLink(scaleAttribute->getOutputSocket(), scaleOperation->getInputSocket(2));

      converter.addLink(angleAttribute->getOutputSocket(), rotateOperation->getInputSocket(1));

      converter.addLink(xAttribute->getOutputSocket(), translateOperation->getInputSocket(1));
      converter.addLink(yAttribute->getOutputSocket(), translateOperation->getInputSocket(2));

      NodeOperationInput *stabilization_socket = nullptr;
      if (invert) {
        /* Translate -> Rotate -> Scale. */
        stabilization_socket = translateOperation->getInputSocket(0);
        converter.mapInputSocket(imageInput, translateOperation->getInputSocket(0));

        converter.addLink(translateOperation->getOutputSocket(),
                          rotateOperation->getInputSocket(0));
        converter.addLink(rotateOperation->getOutputSocket(), scaleOperation->getInputSocket(0));

        converter.mapOutputSocket(getOutputSocket(), scaleOperation->getOutputSocket());
      }
      else {
        /* Scale  -> Rotate -> Translate. */
        stabilization_socket = scaleOperation->getInputSocket(0);
        converter.mapInputSocket(imageInput, scaleOperation->getInputSocket(0));

        converter.addLink(scaleOperation->getOutputSocket(), rotateOperation->getInputSocket(0));
        converter.addLink(rotateOperation->getOutputSocket(),
                          translateOperation->getInputSocket(0));

        converter.mapOutputSocket(getOutputSocket(), translateOperation->getOutputSocket());
      }

      xAttribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
      yAttribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
      scaleAttribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
      angleAttribute->set_socket_input_resolution_for_stabilization(stabilization_socket);
      break;
    }
  }
}

}  // namespace blender::compositor
