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

#ifndef _COM_SocketProxyNode_h_
#define _COM_SocketProxyNode_h_

#include "COM_Node.h"

/**
 * @brief SocketProxyNode
 * @ingroup Node
 */
class SocketProxyNode : public Node {
public:
	SocketProxyNode(bNode *editorNode, bNodeSocket *editorInput, bNodeSocket *editorOutput, bool use_conversion);
	void convertToOperations(NodeConverter &converter, const CompositorContext &context) const;
	
	bool getUseConversion() const { return m_use_conversion; }
	void setUseConversion(bool use_conversion) { m_use_conversion = use_conversion; }
	
private:
	/** If true, the proxy will convert input and output data to/from the proxy socket types. */
	bool m_use_conversion;
};


class SocketBufferNode : public Node {
public:
	SocketBufferNode(bNode *editorNode, bNodeSocket *editorInput, bNodeSocket *editorOutput);
	void convertToOperations(NodeConverter &converter, const CompositorContext &context) const;
};

#endif
