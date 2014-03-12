/*
 * Copyright 2014, Blender Foundation.
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
 *		Lukas Toenne
 */

#include "COM_CornerPinNode.h"
#include "COM_ExecutionSystem.h"

#include "COM_PlaneCornerPinOperation.h"

CornerPinNode::CornerPinNode(bNode *editorNode) : Node(editorNode)
{
}

void CornerPinNode::convertToOperations(ExecutionSystem *graph, CompositorContext *context)
{
	InputSocket *input_image = this->getInputSocket(0);
	/* note: socket order differs between UI node and operations:
	 * bNode uses intuitive order following top-down layout:
	 *   upper-left, upper-right, lower-left, lower-right
	 * Operations use same order as the tracking blenkernel functions expect:
	 *   lower-left, lower-right, upper-right, upper-left
	 */
	const int node_corner_index[4] = { 3, 4, 2, 1 };

	OutputSocket *output_warped_image = this->getOutputSocket(0);
	OutputSocket *output_plane = this->getOutputSocket(1);

	PlaneCornerPinWarpImageOperation *warp_image_operation = new PlaneCornerPinWarpImageOperation();
	
	input_image->relinkConnections(warp_image_operation->getInputSocket(0), 0, graph);
	for (int i = 0; i < 4; ++i) {
		int node_index = node_corner_index[i];
		getInputSocket(node_index)->relinkConnections(warp_image_operation->getInputSocket(i + 1),
		                                              node_index, graph);
	}
	output_warped_image->relinkConnections(warp_image_operation->getOutputSocket());
	
	graph->addOperation(warp_image_operation);
	
	PlaneCornerPinMaskOperation *plane_mask_operation = new PlaneCornerPinMaskOperation();
	
	/* connect mask op inputs to the same sockets as the warp image op */
	for (int i = 0; i < 4; ++i)
		addLink(graph,
		        warp_image_operation->getInputSocket(i + 1)->getConnection()->getFromSocket(),
		        plane_mask_operation->getInputSocket(i));
	output_plane->relinkConnections(plane_mask_operation->getOutputSocket());
	
	graph->addOperation(plane_mask_operation);
}
