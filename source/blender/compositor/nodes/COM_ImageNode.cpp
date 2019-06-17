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

#include "COM_ImageNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_ImageOperation.h"
#include "COM_MultilayerImageOperation.h"
#include "COM_ConvertOperation.h"
#include "BKE_node.h"
#include "BLI_utildefines.h"

#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SeparateColorNode.h"

ImageNode::ImageNode(bNode *editorNode) : Node(editorNode)
{
  /* pass */
}
NodeOperation *ImageNode::doMultilayerCheck(NodeConverter &converter,
                                            RenderLayer *rl,
                                            Image *image,
                                            ImageUser *user,
                                            int framenumber,
                                            int outputsocketIndex,
                                            int passindex,
                                            int view,
                                            DataType datatype) const
{
  NodeOutput *outputSocket = this->getOutputSocket(outputsocketIndex);
  MultilayerBaseOperation *operation = NULL;
  switch (datatype) {
    case COM_DT_VALUE:
      operation = new MultilayerValueOperation(passindex, view);
      break;
    case COM_DT_VECTOR:
      operation = new MultilayerVectorOperation(passindex, view);
      break;
    case COM_DT_COLOR:
      operation = new MultilayerColorOperation(passindex, view);
      break;
    default:
      break;
  }
  operation->setImage(image);
  operation->setRenderLayer(rl);
  operation->setImageUser(user);
  operation->setFramenumber(framenumber);

  converter.addOperation(operation);
  converter.mapOutputSocket(outputSocket, operation->getOutputSocket());

  return operation;
}

void ImageNode::convertToOperations(NodeConverter &converter,
                                    const CompositorContext &context) const
{
  /// Image output
  NodeOutput *outputImage = this->getOutputSocket(0);
  bNode *editorNode = this->getbNode();
  Image *image = (Image *)editorNode->id;
  ImageUser *imageuser = (ImageUser *)editorNode->storage;
  int framenumber = context.getFramenumber();
  int numberOfOutputs = this->getNumberOfOutputSockets();
  bool outputStraightAlpha = (editorNode->custom1 & CMP_NODE_IMAGE_USE_STRAIGHT_OUTPUT) != 0;
  BKE_image_user_frame_calc(image, imageuser, context.getFramenumber());
  /* force a load, we assume iuser index will be set OK anyway */
  if (image && image->type == IMA_TYPE_MULTILAYER) {
    bool is_multilayer_ok = false;
    ImBuf *ibuf = BKE_image_acquire_ibuf(image, imageuser, NULL);
    if (image->rr) {
      RenderLayer *rl = (RenderLayer *)BLI_findlink(&image->rr->layers, imageuser->layer);
      if (rl) {
        NodeOutput *socket;
        int index;

        is_multilayer_ok = true;

        for (index = 0; index < numberOfOutputs; index++) {
          NodeOperation *operation = NULL;
          socket = this->getOutputSocket(index);
          bNodeSocket *bnodeSocket = socket->getbNodeSocket();
          NodeImageLayer *storage = (NodeImageLayer *)bnodeSocket->storage;
          RenderPass *rpass = (RenderPass *)BLI_findstring(
              &rl->passes, storage->pass_name, offsetof(RenderPass, name));
          int view = 0;

          if (STREQ(storage->pass_name, RE_PASSNAME_COMBINED) &&
              STREQ(bnodeSocket->name, "Alpha")) {
            /* Alpha output is already handled with the associated combined output. */
            continue;
          }

          /* returns the image view to use for the current active view */
          if (BLI_listbase_count_at_most(&image->rr->views, 2) > 1) {
            const int view_image = imageuser->view;
            const bool is_allview = (view_image == 0); /* if view selected == All (0) */

            if (is_allview) {
              /* heuristic to match image name with scene names
               * check if the view name exists in the image */
              view = BLI_findstringindex(
                  &image->rr->views, context.getViewName(), offsetof(RenderView, name));
              if (view == -1) {
                view = 0;
              }
            }
            else {
              view = view_image - 1;
            }
          }

          if (rpass) {
            int passindex = BLI_findindex(&rl->passes, rpass);
            switch (rpass->channels) {
              case 1:
                operation = doMultilayerCheck(converter,
                                              rl,
                                              image,
                                              imageuser,
                                              framenumber,
                                              index,
                                              passindex,
                                              view,
                                              COM_DT_VALUE);
                break;
                /* using image operations for both 3 and 4 channels (RGB and RGBA respectively) */
                /* XXX any way to detect actual vector images? */
              case 3:
                operation = doMultilayerCheck(converter,
                                              rl,
                                              image,
                                              imageuser,
                                              framenumber,
                                              index,
                                              passindex,
                                              view,
                                              COM_DT_VECTOR);
                break;
              case 4:
                operation = doMultilayerCheck(converter,
                                              rl,
                                              image,
                                              imageuser,
                                              framenumber,
                                              index,
                                              passindex,
                                              view,
                                              COM_DT_COLOR);
                break;
              default:
                /* dummy operation is added below */
                break;
            }
            if (index == 0 && operation) {
              converter.addPreview(operation->getOutputSocket());
            }
            if (STREQ(rpass->name, RE_PASSNAME_COMBINED)) {
              for (int alphaIndex = 0; alphaIndex < numberOfOutputs; alphaIndex++) {
                NodeOutput *alphaSocket = this->getOutputSocket(alphaIndex);
                bNodeSocket *bnodeAlphaSocket = alphaSocket->getbNodeSocket();
                if (!STREQ(bnodeAlphaSocket->name, "Alpha")) {
                  continue;
                }
                NodeImageLayer *alphaStorage = (NodeImageLayer *)bnodeSocket->storage;
                if (!STREQ(alphaStorage->pass_name, RE_PASSNAME_COMBINED)) {
                  continue;
                }
                SeparateChannelOperation *separate_operation;
                separate_operation = new SeparateChannelOperation();
                separate_operation->setChannel(3);
                converter.addOperation(separate_operation);
                converter.addLink(operation->getOutputSocket(),
                                  separate_operation->getInputSocket(0));
                converter.mapOutputSocket(alphaSocket, separate_operation->getOutputSocket());
                break;
              }
            }
          }

          /* incase we can't load the layer */
          if (operation == NULL) {
            converter.setInvalidOutput(getOutputSocket(index));
          }
        }
      }
    }
    BKE_image_release_ibuf(image, ibuf, NULL);

    /* without this, multilayer that fail to load will crash blender [#32490] */
    if (is_multilayer_ok == false) {
      for (int i = 0; i < getNumberOfOutputSockets(); ++i) {
        converter.setInvalidOutput(getOutputSocket(i));
      }
    }
  }
  else {
    if (numberOfOutputs > 0) {
      ImageOperation *operation = new ImageOperation();
      operation->setImage(image);
      operation->setImageUser(imageuser);
      operation->setFramenumber(framenumber);
      operation->setRenderData(context.getRenderData());
      operation->setViewName(context.getViewName());
      converter.addOperation(operation);

      if (outputStraightAlpha) {
        NodeOperation *alphaConvertOperation = new ConvertPremulToStraightOperation();

        converter.addOperation(alphaConvertOperation);
        converter.mapOutputSocket(outputImage, alphaConvertOperation->getOutputSocket());
        converter.addLink(operation->getOutputSocket(0), alphaConvertOperation->getInputSocket(0));
      }
      else {
        converter.mapOutputSocket(outputImage, operation->getOutputSocket());
      }

      converter.addPreview(operation->getOutputSocket());
    }

    if (numberOfOutputs > 1) {
      NodeOutput *alphaImage = this->getOutputSocket(1);
      ImageAlphaOperation *alphaOperation = new ImageAlphaOperation();
      alphaOperation->setImage(image);
      alphaOperation->setImageUser(imageuser);
      alphaOperation->setFramenumber(framenumber);
      alphaOperation->setRenderData(context.getRenderData());
      alphaOperation->setViewName(context.getViewName());
      converter.addOperation(alphaOperation);

      converter.mapOutputSocket(alphaImage, alphaOperation->getOutputSocket());
    }
    if (numberOfOutputs > 2) {
      NodeOutput *depthImage = this->getOutputSocket(2);
      ImageDepthOperation *depthOperation = new ImageDepthOperation();
      depthOperation->setImage(image);
      depthOperation->setImageUser(imageuser);
      depthOperation->setFramenumber(framenumber);
      depthOperation->setRenderData(context.getRenderData());
      depthOperation->setViewName(context.getViewName());
      converter.addOperation(depthOperation);

      converter.mapOutputSocket(depthImage, depthOperation->getOutputSocket());
    }
    if (numberOfOutputs > 3) {
      /* happens when unlinking image datablock from multilayer node */
      for (int i = 3; i < numberOfOutputs; i++) {
        NodeOutput *output = this->getOutputSocket(i);
        NodeOperation *operation = NULL;
        switch (output->getDataType()) {
          case COM_DT_VALUE: {
            SetValueOperation *valueoperation = new SetValueOperation();
            valueoperation->setValue(0.0f);
            operation = valueoperation;
            break;
          }
          case COM_DT_VECTOR: {
            SetVectorOperation *vectoroperation = new SetVectorOperation();
            vectoroperation->setX(0.0f);
            vectoroperation->setY(0.0f);
            vectoroperation->setW(0.0f);
            operation = vectoroperation;
            break;
          }
          case COM_DT_COLOR: {
            SetColorOperation *coloroperation = new SetColorOperation();
            coloroperation->setChannel1(0.0f);
            coloroperation->setChannel2(0.0f);
            coloroperation->setChannel3(0.0f);
            coloroperation->setChannel4(0.0f);
            operation = coloroperation;
            break;
          }
        }

        if (operation) {
          /* not supporting multiview for this generic case */
          converter.addOperation(operation);
          converter.mapOutputSocket(output, operation->getOutputSocket());
        }
      }
    }
  }
}
