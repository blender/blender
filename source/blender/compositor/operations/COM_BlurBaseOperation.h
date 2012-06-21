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

#ifndef _COM_BlurBaseOperation_h
#define _COM_BlurBaseOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

class BlurBaseOperation : public NodeOperation, public QualityStepHelper {
private:

protected:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *inputProgram;
	SocketReader *inputSize;
	NodeBlurData *data;
	BlurBaseOperation(DataType data_type);
	float *make_gausstab(int rad);
	float *make_dist_fac_inverse(int rad, int falloff);
	float size;
	bool deleteData;
	bool sizeavailable;
	void updateSize(MemoryBuffer **memoryBuffers);
public:
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	void setData(NodeBlurData *data) { this->data = data; }

	void deleteDataWhenFinished() { this->deleteData = true; }

	void setSize(float size) { this->size = size; sizeavailable = true; }
};
#endif
