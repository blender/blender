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

#ifndef _COM_ScreenLensDistortionOperation_h
#define _COM_ScreenLensDistortionOperation_h
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

class ScreenLensDistortionOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputProgram;
	
	NodeLensDist *m_data;
	
	float m_dispersion;
	float m_distortion;
	bool m_valuesAvailable;
	float m_kr, m_kg, m_kb;
	float m_kr4, m_kg4, m_kb4;
	float m_maxk;
	float m_drg;
	float m_dgb;
	float m_sc, m_cx, m_cy;
public:
	ScreenLensDistortionOperation();
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	void *initializeTileData(rcti *rect);
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	void setData(NodeLensDist *data) { this->m_data = data; }
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	/**
	 * @brief Set the distortion and dispersion and precalc some values
	 * @param distortion
	 * @param dispersion
	 */
	void setDistortionAndDispersion(float distortion, float dispersion) {
		this->m_distortion = distortion;
		this->m_dispersion = dispersion;
		updateVariables(distortion, dispersion);
		this->m_valuesAvailable = true;
	}

private:
	void determineUV(float result[4], float x, float y) const;
	void determineUV(float result[4], float x, float y, float distortion, float dispersion);
	void updateDispersionAndDistortion();
	void updateVariables(float distortion, float dispersion);

};
#endif
