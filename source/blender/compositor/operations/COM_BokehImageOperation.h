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

#ifndef _COM_BokehImageOperation_h
#define _COM_BokehImageOperation_h
#include "COM_NodeOperation.h"

/**
 * @brief The BokehImageOperation class is an operation that creates an image useful to mimic the internals
 *of a camera.
 *
 * features:
 *  - number of flaps
 *  - angle offset of the flaps
 *  - rounding of the flaps (also used to make a circular lens)
 *  - simulate catadioptric
 *  - simulate lensshift
 *
 * Per pixel the algorithm determines the edge of the bokeh on the same line as the center of the image and the pixel
 * is evaluating.
 *
 * The edge is detected by finding the closest point on the direct line between the two nearest flap-corners.
 * this edge is interpolated with a full circle. 
 * Result of this edge detection is stored as the distance between the center of the image and the edge.
 *
 * catadioptric lenses are simulated to interpolate between the center of the image and the distance of the edge.
 * We now have three distances:
 *  - distance between the center of the image and the pixel to be evaluated
 *  - distance between the center of the image and the outer-edge
 *  - distance between the center of the image and the inner-edge
 *
 * With a simple compare it can be detected if the evaluated pixel is between the outer and inner edge.
 */
class BokehImageOperation : public NodeOperation {
private:
	/**
	 * @brief Settings of the bokeh image
	 */
	NodeBokehImage *m_data;

	/**
	 * @brief precalced center of the image
	 */
	float m_center[2];
	
	/**
	 * @brief 1.0-rounding
	 */
	float m_inverseRounding;
	
	/**
	 * @brief distance of a full circle lens
	 */
	float m_circularDistance;
	
	/**
	 * @brief radius when the first flap starts
	 */
	float m_flapRad;
	
	/**
	 * @brief radians of a single flap
	 */
	float m_flapRadAdd;
	
	/**
	 * @brief should the m_data field by deleted when this operation is finished
	 */
	bool m_deleteData;

	/**
	 * @brief determine the coordinate of a flap cornder
	 *
	 * @param r result in bokehimage space are stored [x,y]
	 * @param flapNumber the flap number to calculate
	 * @param distance the lens distance is used to simulate lens shifts
	 */
	void detemineStartPointOfFlap(float r[2], int flapNumber, float distance);
	
	/**
	 * @brief Determine if a coordinate is inside the bokeh image
	 *
	 * @param distance the distance that will be used. This parameter is modified a bit to mimic lens shifts
	 * @param x the x coordinate of the pixel to evaluate
	 * @param y the y coordinate of the pixel to evaluate
	 * @return float range 0..1 0 is completely outside
	 */
	float isInsideBokeh(float distance, float x, float y);
public:
	BokehImageOperation();

	/**
	 * @brief the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
	
	/**
	 * @brief Initialize the execution
	 */
	void initExecution();
	
	/**
	 * @brief Deinitialize the execution
	 */
	void deinitExecution();
	
	/**
	 * @brief determine the resolution of this operation. currently fixed at [COM_BLUR_BOKEH_PIXELS, COM_BLUR_BOKEH_PIXELS]
	 * @param resolution
	 * @param preferredResolution
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);

	/**
	 * @brief set the node data
	 * @param data
	 */
	void setData(NodeBokehImage *data) { this->m_data = data; }
	
	/**
	 * @brief deleteDataOnFinish
	 *
	 * There are cases that the compositor uses this operation on its own (see defocus node)
	 * the deleteDataOnFinish must only be called when the data has been created by the compositor.
	 *It should not be called when the data has been created by the node-editor/user.
	 */
	void deleteDataOnFinish() { this->m_deleteData = true; }
};
#endif
