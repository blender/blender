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

#include "COM_BokehImageOperation.h"
#include "BLI_math.h"

BokehImageOperation::BokehImageOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_COLOR);
	this->m_deleteData = false;
}
void BokehImageOperation::initExecution()
{
	this->m_centerX = getWidth() / 2;
	this->m_centerY = getHeight() / 2;
	this->m_center[0] = this->m_centerX;
	this->m_center[1] = this->m_centerY;
	this->m_inverseRounding = 1.0f - this->m_data->rounding;
	this->m_circularDistance = getWidth() / 2;
	this->m_flapRad = (float)(M_PI * 2) / this->m_data->flaps;
	this->m_flapRadAdd = (this->m_data->angle / 360.0f) * (float)(M_PI * 2.0);
	while (this->m_flapRadAdd < 0.0f) {
		this->m_flapRadAdd += (float)(M_PI * 2.0);
	}
	while (this->m_flapRadAdd > (float)M_PI) {
		this->m_flapRadAdd -= (float)(M_PI * 2.0);
	}
}
void BokehImageOperation::detemineStartPointOfFlap(float r[2], int flapNumber, float distance)
{
	r[0] = sinf(this->m_flapRad * flapNumber + this->m_flapRadAdd) * distance + this->m_centerX;
	r[1] = cosf(this->m_flapRad * flapNumber + this->m_flapRadAdd) * distance + this->m_centerY;
}
float BokehImageOperation::isInsideBokeh(float distance, float x, float y)
{
	float insideBokeh = 0.0f;
	const float deltaX = x - this->m_centerX;
	const float deltaY = y - this->m_centerY;
	float closestPoint[2];
	float lineP1[2];
	float lineP2[2];
	float point[2];
	point[0] = x;
	point[1] = y;

	const float distanceToCenter = len_v2v2(point, this->m_center);
	const float bearing = (atan2f(deltaX, deltaY) + (float)(M_PI * 2.0));
	int flapNumber = (int)((bearing - this->m_flapRadAdd) / this->m_flapRad);

	detemineStartPointOfFlap(lineP1, flapNumber, distance);
	detemineStartPointOfFlap(lineP2, flapNumber + 1, distance);
	closest_to_line_v2(closestPoint, point, lineP1, lineP2);

	const float distanceLineToCenter = len_v2v2(this->m_center, closestPoint);
	const float distanceRoundingToCenter = this->m_inverseRounding * distanceLineToCenter + this->m_data->rounding * distance;

	const float catadioptricDistanceToCenter = distanceRoundingToCenter * this->m_data->catadioptric;
	if (distanceRoundingToCenter >= distanceToCenter && catadioptricDistanceToCenter <= distanceToCenter) {
		if (distanceRoundingToCenter - distanceToCenter < 1.0f) {
			insideBokeh = (distanceRoundingToCenter - distanceToCenter);
		}
		else if (this->m_data->catadioptric != 0.0f && distanceToCenter - catadioptricDistanceToCenter < 1.0f) {
			insideBokeh = (distanceToCenter - catadioptricDistanceToCenter);
		}
		else {
			insideBokeh = 1.0f;
		}
	}
	return insideBokeh;
}
void BokehImageOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	float shift = this->m_data->lensshift;
	float shift2 = shift / 2.0f;
	float distance = this->m_circularDistance;
	float insideBokehMax = isInsideBokeh(distance, x, y);
	float insideBokehMed = isInsideBokeh(distance - fabsf(shift2 * distance), x, y);
	float insideBokehMin = isInsideBokeh(distance - fabsf(shift * distance), x, y);
	if (shift < 0) {
		color[0] = insideBokehMax;
		color[1] = insideBokehMed;
		color[2] = insideBokehMin;
	}
	else {
		color[0] = insideBokehMin;
		color[1] = insideBokehMed;
		color[2] = insideBokehMax;
	}
	color[3] = (insideBokehMax + insideBokehMed + insideBokehMin) / 3.0f;
}

void BokehImageOperation::deinitExecution()
{
	if (this->m_deleteData) {
		if (this->m_data) {
			delete this->m_data;
			this->m_data = NULL;
		}
	}
}

void BokehImageOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	resolution[0] = 512;
	resolution[1] = 512;
}
