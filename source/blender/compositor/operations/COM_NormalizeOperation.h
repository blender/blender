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
 *		Dalai Felinto
 */

#ifndef __COM_NORMALIZEOPERATION_H__
#define __COM_NORMALIZEOPERATION_H__
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

/**
 * \brief base class of normalize, implementing the simple normalize
 * \ingroup operation
 */
class NormalizeOperation : public NodeOperation {
protected:
	/**
	 * \brief Cached reference to the reader
	 */
	SocketReader *m_imageReader;

	/**
	 * \brief temporarily cache of the execution storage
	 * it stores x->min and y->mult
	 */
	NodeTwoFloats *m_cachedInstance;

public:
	NormalizeOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	/**
	 * Initialize the execution
	 */
	void initExecution();

	void *initializeTileData(rcti *rect);
	void deinitializeTileData(rcti *rect, void *data);

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

};

#endif
