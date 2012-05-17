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

BokehImageOperation::BokehImageOperation(): NodeOperation() {
	this->addOutputSocket(COM_DT_COLOR);
	this->deleteData = false;
}
void BokehImageOperation::initExecution() {
	this->centerX = getWidth() / 2;
	this->centerY = getHeight() / 2;
	this->center[0] = this->centerX;
	this->center[1] = this->centerY;
	this->inverseRounding = 1.0-this->data->rounding;
	this->circularDistance = getWidth()/2;
	this->flapRad = (M_PI*2)/this->data->flaps;
	this->flapRadAdd = (this->data->angle/360.0)*M_PI*2;
	if (this->flapRadAdd>M_PI) {
		this->flapRadAdd-=M_PI*2;
	}
}
void BokehImageOperation::detemineStartPointOfFlap(float r[2], int flapNumber, float distance) {
	r[0] = sin(flapRad*flapNumber + flapRadAdd)*distance+centerX;
	r[1] = cos(flapRad*flapNumber + flapRadAdd)*distance+centerY;
}
float BokehImageOperation::isInsideBokeh(float distance, float x, float y) {
	float insideBokeh = 0.0;
	const float deltaX = x - centerX;
	const float deltaY = y - centerY;
	float closestPoint[2];
	float lineP1[2];
	float lineP2[2];
	float point[2];
	point[0] = x;
	point[1] = y;

	const float distanceToCenter = len_v2v2(point, center);
	const float bearing = (atan2f(deltaX, deltaY) + (M_PI*2));
	int flapNumber = (int)((bearing-flapRadAdd)/flapRad);

	detemineStartPointOfFlap(lineP1, flapNumber, distance);
	detemineStartPointOfFlap(lineP2, flapNumber+1, distance);
	closest_to_line_v2(closestPoint, point, lineP1, lineP2);

	const float distanceLineToCenter = len_v2v2(center, closestPoint);
	const float distanceRoundingToCenter = inverseRounding*distanceLineToCenter+this->data->rounding*distance;

	const float catadioptricDistanceToCenter = distanceRoundingToCenter*this->data->catadioptric;
	if (distanceRoundingToCenter>=distanceToCenter && catadioptricDistanceToCenter<=distanceToCenter) {
		if (distanceRoundingToCenter-distanceToCenter<1.0) {
			insideBokeh = (distanceRoundingToCenter-distanceToCenter);
		}
		else if (this->data->catadioptric != 0.0 && distanceToCenter-catadioptricDistanceToCenter<1.0) {
			insideBokeh = (distanceToCenter-catadioptricDistanceToCenter);
		}
		else {
			insideBokeh = 1.0;
		}
	}
	return insideBokeh;
}
void BokehImageOperation::executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	float shift = this->data->lensshift;
	float shift2 = shift/2.0f;
	float distance = this->circularDistance;
	float insideBokehMax = isInsideBokeh(distance, x, y);
	float insideBokehMed = isInsideBokeh(distance-fabs(shift2*distance), x, y);
	float insideBokehMin = isInsideBokeh(distance-fabs(shift*distance), x, y);
	if (shift<0) {
		color[0] = insideBokehMax;
		color[1] = insideBokehMed;
		color[2] = insideBokehMin;
	}
	else {
		color[0] = insideBokehMin;
		color[1] = insideBokehMed;
		color[2] = insideBokehMax;
	}
	color[3] = 1.0f;
}

void BokehImageOperation::deinitExecution() {
	if (deleteData) {
		if (data) {
			delete data;
			data = NULL;
		}
	}
}

void BokehImageOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
	resolution[0] = 512;
	resolution[1] = 512;
}
