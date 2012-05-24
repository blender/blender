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
 */

#include "COM_MovieClipNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_MovieClipOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_ConvertColorProfileOperation.h"

extern "C" {
	#include "DNA_movieclip_types.h"
	#include "BKE_movieclip.h"
	#include "BKE_tracking.h"
	#include "IMB_imbuf.h"
}

MovieClipNode::MovieClipNode(bNode *editorNode): Node(editorNode)
{
}

void MovieClipNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context)
{
	OutputSocket *outputMovieClip = this->getOutputSocket(0);
	OutputSocket *offsetXMovieClip = this->getOutputSocket(1);
	OutputSocket *offsetYMovieClip = this->getOutputSocket(2);
	OutputSocket *scaleMovieClip = this->getOutputSocket(3);
	OutputSocket *angleMovieClip = this->getOutputSocket(4);
	
	bNode *editorNode = this->getbNode();
	MovieClip *movieClip = (MovieClip*)editorNode->id;
	MovieClipUser *movieClipUser = (MovieClipUser*)editorNode->storage;
	
	ImBuf * ibuf = NULL;
	if (movieClip) {
		ibuf = BKE_movieclip_get_ibuf(movieClip, movieClipUser);
	}
	
	// always connect the output image
	MovieClipOperation *operation = new MovieClipOperation();
	
	if (ibuf && context->isColorManaged() && ibuf->profile == IB_PROFILE_NONE) {
		ConvertColorProfileOperation *converter = new ConvertColorProfileOperation();
		converter->setFromColorProfile(IB_PROFILE_LINEAR_RGB);
		converter->setToColorProfile(IB_PROFILE_SRGB);
		addLink(graph, operation->getOutputSocket(), converter->getInputSocket(0));
		addPreviewOperation(graph, converter->getOutputSocket(), 9);
		if (outputMovieClip->isConnected()) {
			outputMovieClip->relinkConnections(converter->getOutputSocket());
		}
		graph->addOperation(converter);
		if (ibuf) {
			converter->setPredivided(ibuf->flags & IB_cm_predivide);
		}
	}
	else {
		addPreviewOperation(graph, operation->getOutputSocket(), 9);
		if (outputMovieClip->isConnected()) {
			outputMovieClip->relinkConnections(operation->getOutputSocket());
		}
	}
	operation->setMovieClip(movieClip);
	operation->setMovieClipUser(movieClipUser);
	operation->setFramenumber(context->getFramenumber());
	graph->addOperation(operation);

	MovieTrackingStabilization *stab = &movieClip->tracking.stabilization;
	float loc[2], scale, angle;
	loc[0] = 0.0f;
	loc[1] = 0.0f;
	scale = 1.0f;
	angle = 0.0f;

	if (ibuf) {
		if (stab->flag&TRACKING_2D_STABILIZATION) {
			BKE_tracking_stabilization_data(&movieClip->tracking, context->getFramenumber(), ibuf->x, ibuf->y, loc, &scale, &angle);
		}
	}
	
	if (offsetXMovieClip->isConnected()) {
		SetValueOperation * operationSetValue = new SetValueOperation();
		operationSetValue->setValue(loc[0]);
		offsetXMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}
	if (offsetYMovieClip->isConnected()) {
		SetValueOperation * operationSetValue = new SetValueOperation();
		operationSetValue->setValue(loc[1]);
		offsetYMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}
	if (scaleMovieClip->isConnected()) {
		SetValueOperation * operationSetValue = new SetValueOperation();
		operationSetValue->setValue(scale);
		scaleMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}
	if (angleMovieClip->isConnected()) {
		SetValueOperation * operationSetValue = new SetValueOperation();
		operationSetValue->setValue(angle);
		angleMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}

	if (ibuf) {
		IMB_freeImBuf(ibuf);
	}
}
