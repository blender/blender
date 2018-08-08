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
 * Contributor: Peter Schlaile
 *		Jeroen Bakker
 *		Monique Dewanchand
 */

#ifndef __COM_INPAINTOPERATION_H__
#define __COM_INPAINTOPERATION_H__
#include "COM_NodeOperation.h"

class InpaintSimpleOperation : public NodeOperation {
protected:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputImageProgram;

	int m_iterations;

	float *m_cached_buffer;
	bool m_cached_buffer_ready;

	int *m_pixelorder;
	int m_area_size;
	short *m_manhatten_distance;
public:
	InpaintSimpleOperation();

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

	void setIterations(int iterations) { this->m_iterations = iterations; }

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

private:
	void calc_manhatten_distance();
	void clamp_xy(int &x, int &y);
	float *get_pixel(int x, int y);
	int mdist(int x, int y);
	bool next_pixel(int &x, int &y, int &curr, int iters);
	void pix_step(int x, int y);
};


#endif
