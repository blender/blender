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


#ifndef __COM_MULTILAYERIMAGEOPERATION_H__
#define __COM_MULTILAYERIMAGEOPERATION_H__

#include "COM_ImageOperation.h"

class MultilayerBaseOperation : public BaseImageOperation {
private:
	int m_passId;
	int m_view;
	RenderLayer *m_renderlayer;
protected:
	ImBuf *getImBuf();
public:
	/**
	 * Constructor
	 */
	MultilayerBaseOperation(int passindex, int view);
	void setRenderLayer(RenderLayer *renderlayer) { this->m_renderlayer = renderlayer; }
};

class MultilayerColorOperation : public MultilayerBaseOperation {
public:
	MultilayerColorOperation(int passindex, int view) : MultilayerBaseOperation(passindex, view) {
		this->addOutputSocket(COM_DT_COLOR);
	}
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MultilayerValueOperation : public MultilayerBaseOperation {
public:
	MultilayerValueOperation(int passindex, int view) : MultilayerBaseOperation(passindex, view) {
		this->addOutputSocket(COM_DT_VALUE);
	}
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MultilayerVectorOperation : public MultilayerBaseOperation {
public:
	MultilayerVectorOperation(int passindex, int view) : MultilayerBaseOperation(passindex, view) {
		this->addOutputSocket(COM_DT_VECTOR);
	}
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

#endif
