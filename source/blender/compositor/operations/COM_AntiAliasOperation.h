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

#ifndef __COM_ANTIALIASOPERATION_H__
#define __COM_ANTIALIASOPERATION_H__
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

/**
 * \brief AntiAlias operations
 * it only supports anti aliasing on BW buffers.
 * \ingroup operation
 */
class AntiAliasOperation : public NodeOperation {
protected:
	/**
	 * \brief Cached reference to the reader
	 */
	SocketReader *m_valueReader;
public:
	AntiAliasOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	/**
	 * Initialize the execution
	 */
	void initExecution();

	void *initializeTileData(rcti *rect);

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};
#endif
