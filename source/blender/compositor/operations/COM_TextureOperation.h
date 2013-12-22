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

#include "COM_SingleThreadedNodeOperation.h"
#include "DNA_texture_types.h"
#include "BLI_listbase.h"
extern "C" {
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"
#  include "RE_render_ext.h"
#  include "MEM_guardedalloc.h"
}

/**
 * Base class for all renderlayeroperations
 *
 * @todo: rename to operation.
 */
class TextureBaseOperation : public SingleThreadedNodeOperation {
private:
	Tex *m_texture;
	const RenderData *m_rd;
	SocketReader *m_inputSize;
	SocketReader *m_inputOffset;
	struct ImagePool *m_pool;
	bool m_sceneColorManage;

protected:

	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	
	/**
	 * Constructor
	 */
	TextureBaseOperation();

	MemoryBuffer *createMemoryBuffer(rcti *rect2);
public:
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void setTexture(Tex *texture) { this->m_texture = texture; }
	void initExecution();
	void deinitExecution();
	void setRenderData(const RenderData *rd) { this->m_rd = rd; }
	void setSceneColorManage(bool sceneColorManage) { this->m_sceneColorManage = sceneColorManage; }
};

class TextureOperation : public TextureBaseOperation {
public:
	TextureOperation();

};
class TextureAlphaOperation : public TextureBaseOperation {
public:
	TextureAlphaOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

};

#endif
