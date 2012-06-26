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

#ifndef _COM_Converter_h
#define _COM_Converter_h

#include "DNA_node_types.h"
#include "COM_Node.h"

/**
 * @brief Conversion methods for the compositor
 */
class Converter {
public:
	/**
	 * @brief Convert/wraps a bNode in its Node instance.
	 *
	 * For all nodetypes a wrapper class is created.
	 * Muted nodes are wrapped with MuteNode.
	 *
	 * @note When adding a new node to blender, this method needs to be changed to return the correct Node instance.
	 *
	 * @see Node
	 * @see MuteNode
	 */
	static Node *convert(bNode *b_node);
	
	/**
	 * @brief This method will add a datetype conversion rule when the to-socket does not support the from-socket actual data type.
	 *
	 * @note this method is called when conversion is needed.
	 *
	 * @param connection the SocketConnection what needs conversion
	 * @param system the ExecutionSystem to add the conversion to.
	 * @see SocketConnection - a link between two sockets
	 */
	static void convertDataType(SocketConnection *connection, ExecutionSystem *system);
	
	/**
	 * @brief This method will add a resolution rule based on the settings of the InputSocket.
	 *
	 * @note Conversion logic is implemented in this method
	 * @see InputSocketResizeMode for the possible conversions.

	 * @param connection the SocketConnection what needs conversion
	 * @param system the ExecutionSystem to add the conversion to.
	 * @see SocketConnection - a link between two sockets
	 */
	static void convertResolution(SocketConnection *connection, ExecutionSystem *system);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:Converter")
#endif
};
#endif
