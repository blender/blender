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

#ifndef _COM_CompositorContext_h
#define _COM_CompositorContext_h

#include <vector>
#include "BKE_text.h"
#include <string>
#include "DNA_node_types.h"
#include "BLI_rect.h"
#include "DNA_scene_types.h"
#include "COM_defines.h"

/**
  * @brief Overall context of the compositor
  */
class CompositorContext {
private:
	/**
	  * @brief The rendering field describes if we are rendering (F12) or if we are editing (Node editor)
	  * This field is initialized in ExecutionSystem and must only be read from that point on.
	  * @see ExecutionSystem
	  */
	bool rendering;

	/**
	  * @brief The quality of the composite.
	  * This field is initialized in ExecutionSystem and must only be read from that point on.
	  * @see ExecutionSystem
	  */
	CompositorQuality quality;

	/**
	  * @brief Reference to the scene that is being composited.
	  * This field is initialized in ExecutionSystem and must only be read from that point on.
	  * @see ExecutionSystem
	  */
	Scene *scene;

	/**
	  * @brief reference to the bNodeTree
	  * This field is initialized in ExecutionSystem and must only be read from that point on.
	  * @see ExecutionSystem
	  */
	bNodeTree *bnodetree;
	
	/**
	 * @brief activegNode the group node that is currently being edited.
	 */
	bNode *activegNode;

	/**
	  * @brief does this system have active opencl devices?
	  */
	bool hasActiveOpenCLDevices;

public:
	/**
	  * @brief constructor initializes the context with default values.
	  */
	CompositorContext();

	/**
	  * @brief set the rendering field of the context
	  */
	void setRendering(bool rendering) { this->rendering = rendering; }

	/**
	  * @brief get the rendering field of the context
	  */
	bool isRendering() const {return this->rendering;}

	/**
	  * @brief set the scene of the context
	  */
	void setScene(Scene *scene) {this->scene = scene;}

	/**
	  * @brief set the bnodetree of the context
	  */
	void setbNodeTree(bNodeTree *bnodetree) {this->bnodetree = bnodetree;}

	/**
	  * @brief get the bnodetree of the context
	  */
	const bNodeTree * getbNodeTree() const {return this->bnodetree;}

	/**
	  * @brief set the active groupnode of the context
	  */
	void setActivegNode(bNode *gnode) {this->activegNode = gnode;}

	/**
	  * @brief get the active groupnode of the context
	  */
	const bNode * getActivegNode() const {return this->activegNode;}

	/**
	  * @brief get the scene of the context
	  */
	const Scene *getScene() const {return this->scene;}

	/**
	  * @brief set the quality
	  */
	void setQuality(CompositorQuality quality) {
		this->quality = quality;
	}

	/**
	  * @brief get the quality
	  */
	const CompositorQuality getQuality() const {
		return quality;
	}

	/**
	  * @brief get the current framenumber of the scene in this context
	  */
	const int getFramenumber() const;

	/**
	  * @brief has this system active openclDevices?
	  */
	const bool getHasActiveOpenCLDevices() const {
		return this->hasActiveOpenCLDevices;
	}

	/**
	  * @brief set has this system active openclDevices?
	  */
	void setHasActiveOpenCLDevices(bool hasAvtiveOpenCLDevices) {
		this->hasActiveOpenCLDevices = hasAvtiveOpenCLDevices;
	}
	
	int getChunksize() {return this->getbNodeTree()->chunksize;}
	
	const int isColorManaged() const;
};


#endif
