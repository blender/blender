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

#include "COM_OutputFileNode.h"
#include "COM_OutputFileOperation.h"
#include "COM_ExecutionSystem.h"
#include "BLI_path_util.h"
#include "BKE_utildefines.h"

OutputFileNode::OutputFileNode(bNode *editorNode): Node(editorNode)
{
}

void OutputFileNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	NodeImageMultiFile *storage = (NodeImageMultiFile*)this->getbNode()->storage;
	
	if (!context->isRendering()) {
		/* XXX TODO as in previous implementation?
		 * add dummy operations and exit, to prevent file writing on each compo update.
		 */
	}
	
	if (storage->format.imtype==R_IMF_IMTYPE_MULTILAYER) {
		/* single output operation for the multilayer file */
		OutputOpenExrMultiLayerOperation *outputOperation = new OutputOpenExrMultiLayerOperation(
				context->getScene(), context->getbNodeTree(), storage->base_path, storage->format.exr_codec);
		
		int num_inputs = getNumberOfInputSockets();
		bool hasConnections = false;
		for (int i=0; i < num_inputs; ++i) {
			InputSocket *input = getInputSocket(i);
			if (input->isConnected()) {
				hasConnections = true;
				NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)input->getbNodeSocket()->storage;
				
				outputOperation->add_layer(sockdata->path, input->getDataType());
				
				input->relinkConnections(outputOperation->getInputSocket(i));
			}
		}
		if (hasConnections) addPreviewOperation(graph, outputOperation->getInputSocket(0), 5);
		
		graph->addOperation(outputOperation);
	}
	else {	/* single layer format */
		int num_inputs = getNumberOfInputSockets();
		bool previewAdded = false;
		for (int i=0; i < num_inputs; ++i) {
			InputSocket *input = getInputSocket(i);
			if (input->isConnected()) {
				NodeImageMultiFileSocket *sockdata = (NodeImageMultiFileSocket *)input->getbNodeSocket()->storage;
				ImageFormatData *format = (sockdata->use_node_format ? &storage->format : &sockdata->format);
				char path[FILE_MAX];
				
				/* combine file path for the input */
				BLI_join_dirfile(path, FILE_MAX, storage->base_path, sockdata->path);
				
				OutputSingleLayerOperation *outputOperation = new OutputSingleLayerOperation(
						context->getScene(), context->getbNodeTree(), input->getActualDataType(), format, path);
				input->relinkConnections(outputOperation->getInputSocket(0));
				graph->addOperation(outputOperation);
				if (!previewAdded) {
					addPreviewOperation(graph, outputOperation->getInputSocket(0), 5);
					previewAdded = true;
				}
			}
		}
	}
}

