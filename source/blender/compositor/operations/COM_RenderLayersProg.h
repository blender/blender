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


#ifndef _COM_RenderLayersBaseProg_h
#define _COM_RenderLayersBaseProg_h

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "BLI_listbase.h"
#include "BKE_image.h"
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
class RenderLayersBaseProg : public NodeOperation {
private:
	/**
	 * Reference to the scene object.
	 */
	Scene *m_scene;
	
	/**
	 * layerId of the layer where this operation needs to get its data from
	 */
	short m_layerId;
	
	/**
	 * cached instance to the float buffer inside the layer
	 */
	float *m_inputBuffer;
	
	/**
	 * renderpass where this operation needs to get its data from
	 */
	int m_renderpass;
	
	int m_elementsize;

	/**
	 * @brief render data used for active rendering
	 */
	const RenderData *m_rd;

protected:
	/**
	 * Constructor
	 */
	RenderLayersBaseProg(int renderpass, int elementsize);
	
	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	
	/**
	 * retrieve the reference to the float buffer of the renderer.
	 */
	inline float *getInputBuffer() { return this->m_inputBuffer; }

	void doInterpolation(float output[4], float x, float y, PixelSampler sampler);
public:
	/**
	 * setter for the scene field. Will be called from
	 * @see RenderLayerNode to set the actual scene where
	 * the data will be retrieved from.
	 */
	void setScene(Scene *scene) { this->m_scene = scene; }
	Scene *getScene() { return this->m_scene; }
	void setRenderData(const RenderData *rd) { this->m_rd = rd; }
	void setLayerId(short layerId) { this->m_layerId = layerId; }
	short getLayerId() { return this->m_layerId; }
	void initExecution();
	void deinitExecution();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class RenderLayersAOOperation : public RenderLayersBaseProg {
public:
	RenderLayersAOOperation();
};

class RenderLayersAlphaProg : public RenderLayersBaseProg {
public:
	RenderLayersAlphaProg();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class RenderLayersColorOperation : public RenderLayersBaseProg {
public:
	RenderLayersColorOperation();
};

class RenderLayersCyclesOperation : public RenderLayersBaseProg {
public:
	RenderLayersCyclesOperation(int pass);
};

class RenderLayersDepthProg : public RenderLayersBaseProg {
public:
	RenderLayersDepthProg();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class RenderLayersDiffuseOperation : public RenderLayersBaseProg {
public:
	RenderLayersDiffuseOperation();
};

class RenderLayersEmitOperation : public RenderLayersBaseProg {
public:
	RenderLayersEmitOperation();
};

class RenderLayersEnvironmentOperation : public RenderLayersBaseProg {
public:
	RenderLayersEnvironmentOperation();
};

/// @todo rename to image operation
class RenderLayersColorProg : public RenderLayersBaseProg {
public:
	RenderLayersColorProg();
};

class RenderLayersIndirectOperation : public RenderLayersBaseProg {
public:
	RenderLayersIndirectOperation();
};

class RenderLayersMaterialIndexOperation : public RenderLayersBaseProg {
public:
	RenderLayersMaterialIndexOperation();
};

class RenderLayersMistOperation : public RenderLayersBaseProg {
public:
	RenderLayersMistOperation();
};

class RenderLayersNormalOperation : public RenderLayersBaseProg {
public:
	RenderLayersNormalOperation();
};

class RenderLayersObjectIndexOperation : public RenderLayersBaseProg {
public:
	RenderLayersObjectIndexOperation();
};

class RenderLayersReflectionOperation : public RenderLayersBaseProg {
public:
	RenderLayersReflectionOperation();
};

class RenderLayersRefractionOperation : public RenderLayersBaseProg {
public:
	RenderLayersRefractionOperation();
};

class RenderLayersShadowOperation : public RenderLayersBaseProg {
public:
	RenderLayersShadowOperation();
};

class RenderLayersSpecularOperation : public RenderLayersBaseProg {
public:
	RenderLayersSpecularOperation();
};

class RenderLayersSpeedOperation : public RenderLayersBaseProg {
public:
	RenderLayersSpeedOperation();
};

class RenderLayersUVOperation : public RenderLayersBaseProg {
public:
	RenderLayersUVOperation();
};

#endif
