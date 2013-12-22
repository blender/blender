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
#include "COM_ConvertOperation.h"
#include "BKE_node.h"
#include "BLI_utildefines.h"

#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"

ImageNode::ImageNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */

}
NodeOperation *ImageNode::doMultilayerCheck(ExecutionSystem *system, RenderLayer *rl, Image *image, ImageUser *user, int framenumber, int outputsocketIndex, int passindex, DataType datatype)
{
	OutputSocket *outputSocket = this->getOutputSocket(outputsocketIndex);
	MultilayerBaseOperation *operation = NULL;
	switch (datatype) {
		case COM_DT_VALUE:
			operation = new MultilayerValueOperation(passindex);
			break;
		case COM_DT_VECTOR:
			operation = new MultilayerVectorOperation(passindex);
			break;
		case COM_DT_COLOR:
			operation = new MultilayerColorOperation(passindex);
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

void ImageNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	/// Image output
	OutputSocket *outputImage = this->getOutputSocket(0);
	bNode *editorNode = this->getbNode();
	Image *image = (Image *)editorNode->id;
	ImageUser *imageuser = (ImageUser *)editorNode->storage;
	int framenumber = context->getFramenumber();
	int numberOfOutputs = this->getNumberOfOutputSockets();
	bool outputStraightAlpha = (editorNode->custom1 & CMP_NODE_IMAGE_USE_STRAIGHT_OUTPUT) != 0;
	BKE_image_user_frame_calc(imageuser, context->getFramenumber(), 0);

	/* force a load, we assume iuser index will be set OK anyway */
	if (image && image->type == IMA_TYPE_MULTILAYER) {
		bool is_multilayer_ok = false;
		ImBuf *ibuf = BKE_image_acquire_ibuf(image, imageuser, NULL);
		if (image->rr) {
			RenderLayer *rl = (RenderLayer *)BLI_findlink(&image->rr->layers, imageuser->layer);
			if (rl) {
				OutputSocket *socket;
				int index;

				is_multilayer_ok = true;

				for (index = 0; index < numberOfOutputs; index++) {
					NodeOperation *operation = NULL;
					socket = this->getOutputSocket(index);
					if (socket->isConnected() || index == 0) {
						bNodeSocket *bnodeSocket = socket->getbNodeSocket();
						/* Passes in the file can differ from passes stored in sockets (#36755).
						 * Look up the correct file pass using the socket identifier instead.
						 */
#if 0
						NodeImageLayer *storage = (NodeImageLayer *)bnodeSocket->storage;*/
						int passindex = storage->pass_index;*/
						RenderPass *rpass = (RenderPass *)BLI_findlink(&rl->passes, passindex);
#endif
						int passindex;
						RenderPass *rpass;
						for (rpass = (RenderPass *)rl->passes.first, passindex = 0; rpass; rpass = rpass->next, ++passindex)
							if (STREQ(rpass->name, bnodeSocket->identifier))
								break;
						if (rpass) {
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
									/* dummy operation is added below */
									break;
							}

							if (index == 0 && operation) {
								addPreviewOperation(graph, context, operation->getOutputSocket());
							}
						}
					}

					/* incase we can't load the layer */
					if (operation == NULL) {
						convertToOperations_invalid_index(graph, index);
					}
				}
			}
		}
		BKE_image_release_ibuf(image, ibuf, NULL);

		/* without this, multilayer that fail to load will crash blender [#32490] */
		if (is_multilayer_ok == false) {
			int index;
			vector<OutputSocket *> &outputsockets = this->getOutputSockets();
			for (index = 0; index < outputsockets.size(); index++) {
				const float warning_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
				SetColorOperation *operation = new SetColorOperation();
				operation->setChannels(warning_color);

				/* link the operation */
				this->getOutputSocket(index)->relinkConnections(operation->getOutputSocket());
				graph->addOperation(operation);
			}
		}
	}
	else {
		if (numberOfOutputs >  0) {
			ImageOperation *operation = new ImageOperation();
			if (outputImage->isConnected()) {
				if (outputStraightAlpha) {
					NodeOperation *alphaConvertOperation = new ConvertPremulToStraightOperation();
					addLink(graph, operation->getOutputSocket(0), alphaConvertOperation->getInputSocket(0));
					outputImage->relinkConnections(alphaConvertOperation->getOutputSocket());
					graph->addOperation(alphaConvertOperation);
				}
				else {
					outputImage->relinkConnections(operation->getOutputSocket());
				}
			}
			operation->setImage(image);
			operation->setImageUser(imageuser);
			operation->setFramenumber(framenumber);
			graph->addOperation(operation);
			addPreviewOperation(graph, context, operation->getOutputSocket());
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
		if (numberOfOutputs > 3) {
			/* happens when unlinking image datablock from multilayer node */
			for (int i = 3; i < numberOfOutputs; i++) {
				OutputSocket *output = this->getOutputSocket(i);
				NodeOperation *operation = NULL;
				switch (output->getDataType()) {
					case COM_DT_VALUE:
					{
						SetValueOperation *valueoperation = new SetValueOperation();
						valueoperation->setValue(0.0f);
						operation = valueoperation;
						break;
					}
					case COM_DT_VECTOR:
					{
						SetVectorOperation *vectoroperation = new SetVectorOperation();
						vectoroperation->setX(0.0f);
						vectoroperation->setY(0.0f);
						vectoroperation->setW(0.0f);
						operation = vectoroperation;
						break;
					}
					case COM_DT_COLOR:
					{
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
					output->relinkConnections(operation->getOutputSocket());
					graph->addOperation(operation);
				}
			}
		}
	}
}

