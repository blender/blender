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
 *		Lukas Tönne
 */


#ifndef _COM_MultilayerImageOperation_h
#define _COM_MultilayerImageOperation_h

#include "COM_ImageOperation.h"

class MultilayerBaseOperation : public BaseImageOperation {
private:
	int passId;
	RenderLayer *renderlayer;
protected:
	ImBuf *getImBuf();
public:
	/**
	 * Constructor
	 */
	MultilayerBaseOperation(int pass);
	void setRenderLayer(RenderLayer *renderlayer) { this->renderlayer = renderlayer; }
};

class MultilayerColorOperation : public MultilayerBaseOperation {
public:
	MultilayerColorOperation(int pass) : MultilayerBaseOperation(pass) {
		this->addOutputSocket(COM_DT_COLOR);
	}
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
};

class MultilayerValueOperation : public MultilayerBaseOperation {
public:
	MultilayerValueOperation(int pass) : MultilayerBaseOperation(pass) {
		this->addOutputSocket(COM_DT_VALUE);
	}
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
};

class MultilayerVectorOperation : public MultilayerBaseOperation {
public:
	MultilayerVectorOperation(int pass) : MultilayerBaseOperation(pass) {
		this->addOutputSocket(COM_DT_VECTOR);
	}
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
};

#endif
