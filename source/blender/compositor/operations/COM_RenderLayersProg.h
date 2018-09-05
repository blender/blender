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


#ifndef __COM_RENDERLAYERSPROG_H__
#define __COM_RENDERLAYERSPROG_H__

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "BLI_listbase.h"
#include "BKE_image.h"
extern "C" {
#  include "RE_pipeline.h"
#  include "MEM_guardedalloc.h"
}

/**
 * Base class for all renderlayeroperations
 *
 * \todo: rename to operation.
 */
class RenderLayersProg : public NodeOperation {
protected:
	/**
	 * Reference to the scene object.
	 */
	Scene *m_scene;

	/**
	 * layerId of the layer where this operation needs to get its data from
	 */
	short m_layerId;

	/**
	 * viewName of the view to use (unless another view is specified by the node
	 */
	const char *m_viewName;

	/**
	 * cached instance to the float buffer inside the layer
	 */
	float *m_inputBuffer;

	/**
	 * renderpass where this operation needs to get its data from
	 */
	std::string m_passName;

	int m_elementsize;

	/**
	 * \brief render data used for active rendering
	 */
	const RenderData *m_rd;

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
	 * Constructor
	 */
	RenderLayersProg(const char *passName, DataType type, int elementsize);
	/**
	 * setter for the scene field. Will be called from
	 * \see RenderLayerNode to set the actual scene where
	 * the data will be retrieved from.
	 */
	void setScene(Scene *scene) { this->m_scene = scene; }
	Scene *getScene() { return this->m_scene; }
	void setRenderData(const RenderData *rd) { this->m_rd = rd; }
	void setLayerId(short layerId) { this->m_layerId = layerId; }
	short getLayerId() { return this->m_layerId; }
	void setViewName(const char *viewName) { this->m_viewName = viewName; }
	const char *getViewName() { return this->m_viewName; }
	void initExecution();
	void deinitExecution();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class RenderLayersAOOperation : public RenderLayersProg {
public:
	RenderLayersAOOperation(const char *passName, DataType type, int elementsize)
	 : RenderLayersProg(passName, type, elementsize) {}
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class RenderLayersAlphaProg : public RenderLayersProg {
public:
	RenderLayersAlphaProg(const char *passName, DataType type, int elementsize)
	 : RenderLayersProg(passName, type, elementsize) {}
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class RenderLayersDepthProg : public RenderLayersProg {
public:
	RenderLayersDepthProg(const char *passName, DataType type, int elementsize)
	 : RenderLayersProg(passName, type, elementsize) {}
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

#endif
