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


#ifndef _COM_TextureOperation_h
#define _COM_TextureOperation_h

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "BLI_listbase.h"
extern "C" {
	#include "RE_pipeline.h"
	#include "RE_shader_ext.h"
	#include "RE_render_ext.h"
	#include "MEM_guardedalloc.h"
}

/**
  * Base class for all renderlayeroperations
  *
  * @todo: rename to operation.
  */
class TextureBaseOperation : public NodeOperation {
private:
	Tex* texture;
	SocketReader *inputSize;
	SocketReader *inputOffset;

protected:

	/**
	  * Determine the output resolution. The resolution is retrieved from the Renderer
	  */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	
	/**
	  * Constructor
	  */
	TextureBaseOperation();

public:
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

	void setTexture(Tex* texture) {this->texture = texture;}
	void initExecution();
	void deinitExecution();
};

class TextureOperation:public TextureBaseOperation {
public:
	TextureOperation();

};
class TextureAlphaOperation:public TextureBaseOperation {
public:
	TextureAlphaOperation();
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

};

#endif
