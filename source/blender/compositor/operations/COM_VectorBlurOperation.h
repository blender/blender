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

#ifndef __COM_VECTORBLUROPERATION_H__
#define __COM_VECTORBLUROPERATION_H__
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"
#include "COM_QualityStepHelper.h"

class VectorBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	/**
	 * \brief Cached reference to the inputProgram
	 */
	SocketReader *m_inputImageProgram;
	SocketReader *m_inputSpeedProgram;
	SocketReader *m_inputZProgram;

	/**
	 * \brief settings of the glare node.
	 */
	NodeBlurData *m_settings;

	float *m_cachedInstance;

public:
	VectorBlurOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	/**
	 * Initialize the execution
	 */
	void initExecution();

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void *initializeTileData(rcti *rect);

	void setVectorBlurSettings(NodeBlurData *settings) { this->m_settings = settings; }
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
protected:

	void generateVectorBlur(float *data, MemoryBuffer *inputImage, MemoryBuffer *inputSpeed, MemoryBuffer *inputZ);


};
#endif
