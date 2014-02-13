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

#define MAX_GAUSSTAB_RADIUS 30000

class BlurBaseOperation : public NodeOperation, public QualityStepHelper {
private:

protected:

	BlurBaseOperation(DataType data_type);
	float *make_gausstab(float rad, int size);
	float *make_dist_fac_inverse(float rad, int size, int falloff);

	void updateSize();

	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputProgram;
	SocketReader *m_inputSize;
	NodeBlurData *m_data;

	float m_size;
	bool m_deleteData;
	bool m_sizeavailable;

public:
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	void setData(NodeBlurData *data) { this->m_data = data; }

	void deleteDataWhenFinished() { this->m_deleteData = true; }

	void setSize(float size) { this->m_size = size; this->m_sizeavailable = true; }
};
#endif
