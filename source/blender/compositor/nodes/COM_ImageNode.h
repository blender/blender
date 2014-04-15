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

#include "COM_defines.h"
#include "COM_Node.h"
#include "DNA_node_types.h"
#include "DNA_image_types.h"
extern "C" {
#  include "RE_engine.h"
}

/**
 * @brief ImageNode
 * @ingroup Node
 */
class ImageNode : public Node {
private:
	NodeOperation *doMultilayerCheck(NodeConverter &converter, RenderLayer *rl, Image *image, ImageUser *user,
	                                 int framenumber, int outputsocketIndex, int passindex, DataType datatype) const;
public:
	ImageNode(bNode *editorNode);
	void convertToOperations(NodeConverter &converter, const CompositorContext &context) const;

};
