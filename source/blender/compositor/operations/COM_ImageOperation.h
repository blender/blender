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


#ifndef _COM_ImageOperation_h
#define _COM_ImageOperation_h

#include "COM_NodeOperation.h"
#include "BLI_listbase.h"
#include "BKE_image.h"
extern "C" {
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"
#  include "RE_render_ext.h"
#  include "MEM_guardedalloc.h"
}

/**
 * @brief Base class for all image operations
 */
class BaseImageOperation : public NodeOperation {
protected:
	ImBuf *m_buffer;
	Image *m_image;
	ImageUser *m_imageUser;
	float *m_imageFloatBuffer;
	unsigned int *m_imageByteBuffer;
	float *m_depthBuffer;
	int m_imageheight;
	int m_imagewidth;
	int m_framenumber;
	int m_numberOfChannels;
	
	BaseImageOperation();
	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	
	virtual ImBuf *getImBuf();

public:
	
	void initExecution();
	void deinitExecution();
	void setImage(Image *image) { this->m_image = image; }
	void setImageUser(ImageUser *imageuser) { this->m_imageUser = imageuser; }

	void setFramenumber(int framenumber) { this->m_framenumber = framenumber; }
};
class ImageOperation : public BaseImageOperation {
public:
	/**
	 * Constructor
	 */
	ImageOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};
class ImageAlphaOperation : public BaseImageOperation {
public:
	/**
	 * Constructor
	 */
	ImageAlphaOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};
class ImageDepthOperation : public BaseImageOperation {
public:
	/**
	 * Constructor
	 */
	ImageDepthOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};
#endif
