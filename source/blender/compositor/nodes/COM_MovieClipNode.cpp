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
#  include "DNA_movieclip_types.h"
#  include "BKE_movieclip.h"
#  include "BKE_tracking.h"
#  include "IMB_imbuf.h"
}

MovieClipNode::MovieClipNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void MovieClipNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	OutputSocket *outputMovieClip = this->getOutputSocket(0);
	OutputSocket *alphaMovieClip = this->getOutputSocket(1);
	OutputSocket *offsetXMovieClip = this->getOutputSocket(2);
	OutputSocket *offsetYMovieClip = this->getOutputSocket(3);
	OutputSocket *scaleMovieClip = this->getOutputSocket(4);
	OutputSocket *angleMovieClip = this->getOutputSocket(5);
	
	bNode *editorNode = this->getbNode();
	MovieClip *movieClip = (MovieClip *)editorNode->id;
	MovieClipUser *movieClipUser = (MovieClipUser *)editorNode->storage;
	bool cacheFrame = !context->isRendering();

	ImBuf *ibuf = NULL;
	if (movieClip) {
		if (cacheFrame)
			ibuf = BKE_movieclip_get_ibuf(movieClip, movieClipUser);
		else
			ibuf = BKE_movieclip_get_ibuf_flag(movieClip, movieClipUser, movieClip->flag, MOVIECLIP_CACHE_SKIP);
	}
	
	// always connect the output image
	MovieClipOperation *operation = new MovieClipOperation();
	
	addPreviewOperation(graph, context, operation->getOutputSocket());
	if (outputMovieClip->isConnected()) {
		outputMovieClip->relinkConnections(operation->getOutputSocket());
	}

	operation->setMovieClip(movieClip);
	operation->setMovieClipUser(movieClipUser);
	operation->setFramenumber(context->getFramenumber());
	operation->setCacheFrame(cacheFrame);
	graph->addOperation(operation);

	if (alphaMovieClip->isConnected()) {
		MovieClipAlphaOperation *alphaOperation = new MovieClipAlphaOperation();
		alphaOperation->setMovieClip(movieClip);
		alphaOperation->setMovieClipUser(movieClipUser);
		alphaOperation->setFramenumber(context->getFramenumber());
		alphaOperation->setCacheFrame(cacheFrame);
		alphaMovieClip->relinkConnections(alphaOperation->getOutputSocket());
		graph->addOperation(alphaOperation);
	}

	MovieTrackingStabilization *stab = &movieClip->tracking.stabilization;
	float loc[2], scale, angle;
	loc[0] = 0.0f;
	loc[1] = 0.0f;
	scale = 1.0f;
	angle = 0.0f;

	if (ibuf) {
		if (stab->flag & TRACKING_2D_STABILIZATION) {
			int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(movieClip, context->getFramenumber());

			BKE_tracking_stabilization_data_get(&movieClip->tracking, clip_framenr, ibuf->x, ibuf->y, loc, &scale, &angle);
		}
	}

	if (offsetXMovieClip->isConnected()) {
		SetValueOperation *operationSetValue = new SetValueOperation();
		operationSetValue->setValue(loc[0]);
		offsetXMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}
	if (offsetYMovieClip->isConnected()) {
		SetValueOperation *operationSetValue = new SetValueOperation();
		operationSetValue->setValue(loc[1]);
		offsetYMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}
	if (scaleMovieClip->isConnected()) {
		SetValueOperation *operationSetValue = new SetValueOperation();
		operationSetValue->setValue(scale);
		scaleMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}
	if (angleMovieClip->isConnected()) {
		SetValueOperation *operationSetValue = new SetValueOperation();
		operationSetValue->setValue(angle);
		angleMovieClip->relinkConnections(operationSetValue->getOutputSocket());
		graph->addOperation(operationSetValue);
	}

	if (ibuf) {
		IMB_freeImBuf(ibuf);
	}
}
