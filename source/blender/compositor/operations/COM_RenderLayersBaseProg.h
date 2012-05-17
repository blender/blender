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
class RenderLayersBaseProg : public NodeOperation {
private:
	/**
	  * Reference to the scene object.
	  */
	Scene* scene;
	
	/**
	  * layerId of the layer where this operation needs to get its data from
	  */
	short layerId;
	
	/**
	  * cached instance to the float buffer inside the layer
	  */
	float* inputBuffer;
	
	/**
	  * renderpass where this operation needs to get its data from
	  */
	int renderpass;
	
	int elementsize;
	
protected:
	/**
	  * Constructor
	  */
	RenderLayersBaseProg(int renderpass, int elementsize);
	
	/**
	  * Determine the output resolution. The resolution is retrieved from the Renderer
	  */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	
	/**
	  * retrieve the reference to the float buffer of the renderer.
	  */
	inline float* getInputBuffer() {return this->inputBuffer;}

public:
	/**
	  * setter for the scene field. Will be called from
	  * @see RenderLayerNode to set the actual scene where
	  * the data will be retrieved from.
	  */
	void setScene(Scene* scene) {this->scene = scene;}
	Scene* getScene() {return this->scene;}
	void setLayerId(short layerId) {this->layerId = layerId;}
	short getLayerId() {return this->layerId;}
	void initExecution();
	void deinitExecution();
	void executePixel(float* output, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);

};

#endif
