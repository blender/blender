/*
 * Copyright 2012, Blender Foundation.
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

#include "COM_Node.h"

/**
 * \brief KeyingNode
 * \ingroup Node
 */
class KeyingNode : public Node {
protected:
	NodeOperationOutput *setupPreBlur(NodeConverter &converter, NodeInput *inputImage, int size) const;
	NodeOperationOutput *setupPostBlur(NodeConverter &converter, NodeOperationOutput *postBlurInput, int size) const;
	NodeOperationOutput *setupDilateErode(NodeConverter &converter, NodeOperationOutput *dilateErodeInput, int distance) const;
	NodeOperationOutput *setupFeather(NodeConverter &converter, const CompositorContext &context, NodeOperationOutput *featherInput,
	                           int falloff, int distance) const;
	NodeOperationOutput *setupDespill(NodeConverter &converter, NodeOperationOutput *despillInput, NodeInput *inputSrceen,
	                           float factor, float colorBalance) const;
	NodeOperationOutput *setupClip(NodeConverter &converter, NodeOperationOutput *clipInput, int kernelRadius, float kernelTolerance,
	                        float clipBlack, float clipWhite, bool edgeMatte) const;
public:
	KeyingNode(bNode *editorNode);
	void convertToOperations(NodeConverter &converter, const CompositorContext &context) const;

};
