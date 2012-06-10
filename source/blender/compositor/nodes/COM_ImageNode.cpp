/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 *		Lukas Tönne
 */

#include "COM_ImageNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_ImageOperation.h"
#include "COM_MultilayerImageOperation.h"
#include "BKE_node.h"
#include "BLI_utildefines.h"

ImageNode::ImageNode(bNode *editorNode): Node(editorNode)
{
}
NodeOperation *ImageNode::doMultilayerCheck(ExecutionSystem *system, RenderLayer *rl, Image *image, ImageUser *user, int framenumber, int outputsocketIndex, int pass, DataType datatype)
{
	OutputSocket *outputSocket = this->getOutputSocket(outputsocketIndex);
	MultilayerBaseOperation * operation = NULL;
	switch (datatype) {
	case COM_DT_VALUE:
		operation = new MultilayerValueOperation(pass);
		break;
	case COM_DT_VECTOR:
		operation = new MultilayerVectorOperation(pass);
		break;
	case COM_DT_COLOR:
		operation = new MultilayerColorOperation(pass);
		break;
	default:
		break;
	}
	operation->setImage(image);
	operation->setRenderLayer(rl);
	operation->setImageUser(user);
	operation->setFramenumber(framenumber);
	outputSocket->relinkConnections(operation->getOutputSocket());
	system->addOperation(operation);
	return operation;
}

void ImageNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	/// Image output
	OutputSocket *outputImage = this->getOutputSocket(0);
	bNode *editorNode = this->getbNode();
	Image *image = (Image*)editorNode->id;
	ImageUser *imageuser = (ImageUser*)editorNode->storage;
	int framenumber = context->getFramenumber();
	int numberOfOutputs = this->getNumberOfOutputSockets();
	BKE_image_user_frame_calc(imageuser, context->getFramenumber(), 0);

	/* force a load, we assume iuser index will be set OK anyway */
	if (image && image->type==IMA_TYPE_MULTILAYER) {
		BKE_image_get_ibuf(image, imageuser);
		if (image->rr) {
			RenderLayer *rl = (RenderLayer*)BLI_findlink(&image->rr->layers, imageuser->layer);
			if (rl) {
				OutputSocket * socket;
				int index;
				for (index = 0 ; index < numberOfOutputs ; index ++) {
					socket = this->getOutputSocket(index);
					if (socket->isConnected() || index == 0) {
						bNodeSocket *bnodeSocket = socket->getbNodeSocket();
						NodeImageLayer *storage = (NodeImageLayer*)bnodeSocket->storage;
						int passindex = storage->pass_index;
						
						RenderPass *rpass = (RenderPass *)BLI_findlink(&rl->passes, passindex);
						if (rpass) {
							NodeOperation * operation = NULL;
							imageuser->pass = passindex;
							switch (rpass->channels) {
							case 1:
								operation = doMultilayerCheck(graph, rl, image, imageuser, framenumber, index, passindex, COM_DT_VALUE);
								break;
							/* using image operations for both 3 and 4 channels (RGB and RGBA respectively) */
							/* XXX any way to detect actual vector images? */
							case 3:
								operation = doMultilayerCheck(graph, rl, image, imageuser, framenumber, index, passindex, COM_DT_VECTOR);
								break;
							case 4:
								operation = doMultilayerCheck(graph, rl, image, imageuser, framenumber, index, passindex, COM_DT_COLOR);
								break;
							
							default:
							/* XXX add a dummy operation? */
							break;
							}
							if (index == 0 && operation) {
								addPreviewOperation(graph, operation->getOutputSocket());
							}
						}
					}
				}
			}
		}
	}
	else {
		if (numberOfOutputs >  0) {
			ImageOperation *operation = new ImageOperation();
			if (outputImage->isConnected()) {
				outputImage->relinkConnections(operation->getOutputSocket());
			}
			operation->setImage(image);
			operation->setImageUser(imageuser);
			operation->setFramenumber(framenumber);
			graph->addOperation(operation);
			addPreviewOperation(graph, operation->getOutputSocket());
		}
		
		if (numberOfOutputs > 1) {
			OutputSocket *alphaImage = this->getOutputSocket(1);
			if (alphaImage->isConnected()) {
				ImageAlphaOperation *alphaOperation = new ImageAlphaOperation();
				alphaOperation->setImage(image);
				alphaOperation->setImageUser(imageuser);
				alphaOperation->setFramenumber(framenumber);
				alphaImage->relinkConnections(alphaOperation->getOutputSocket());
				graph->addOperation(alphaOperation);
			}
		}
		if (numberOfOutputs > 2) {
			OutputSocket *depthImage = this->getOutputSocket(2);
			if (depthImage->isConnected()) {
				ImageDepthOperation *depthOperation = new ImageDepthOperation();
				depthOperation->setImage(image);
				depthOperation->setImageUser(imageuser);
				depthOperation->setFramenumber(framenumber);
				depthImage->relinkConnections(depthOperation->getOutputSocket());
				graph->addOperation(depthOperation);
			}
		}
	}
}

