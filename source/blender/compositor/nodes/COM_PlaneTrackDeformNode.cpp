/*
 * Copyright 2013, Blender Foundation.
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
 *		Sergey Sharybin
 */

#include "COM_PlaneTrackDeformNode.h"
#include "COM_ExecutionSystem.h"

#include "COM_PlaneTrackMaskOperation.h"
#include "COM_PlaneTrackWarpImageOperation.h"

extern "C" {
#  include "BKE_node.h"
#  include "BKE_movieclip.h"
#  include "BKE_tracking.h"
}

PlaneTrackDeformNode::PlaneTrackDeformNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void PlaneTrackDeformNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *input_image = this->getInputSocket(0);

	OutputSocket *output_warped_image = this->getOutputSocket(0);
	OutputSocket *output_plane = this->getOutputSocket(1);

	bNode *editorNode = this->getbNode();
	MovieClip *clip = (MovieClip *) editorNode->id;

	NodePlaneTrackDeformData *data = (NodePlaneTrackDeformData *) editorNode->storage;

	int frame_number = context->getFramenumber();

	if (output_warped_image->isConnected()) {
		PlaneTrackWarpImageOperation *warp_image_operation = new PlaneTrackWarpImageOperation();

		warp_image_operation->setMovieClip(clip);
		warp_image_operation->setTrackingObject(data->tracking_object);
		warp_image_operation->setPlaneTrackName(data->plane_track_name);
		warp_image_operation->setFramenumber(frame_number);

		input_image->relinkConnections(warp_image_operation->getInputSocket(0), 0, graph);
		output_warped_image->relinkConnections(warp_image_operation->getOutputSocket());

		graph->addOperation(warp_image_operation);
	}

	if (output_plane->isConnected()) {
		PlaneTrackMaskOperation *plane_mask_operation = new PlaneTrackMaskOperation();

		plane_mask_operation->setMovieClip(clip);
		plane_mask_operation->setTrackingObject(data->tracking_object);
		plane_mask_operation->setPlaneTrackName(data->plane_track_name);
		plane_mask_operation->setFramenumber(frame_number);

		output_plane->relinkConnections(plane_mask_operation->getOutputSocket());

		graph->addOperation(plane_mask_operation);
	}
}
