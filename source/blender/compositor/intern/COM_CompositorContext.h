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
#include "DNA_color_types.h"
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
	bool m_rendering;

	/**
	 * @brief The quality of the composite.
	 * This field is initialized in ExecutionSystem and must only be read from that point on.
	 * @see ExecutionSystem
	 */
	CompositorQuality m_quality;

	Scene *m_scene;

	/**
	 * @brief Reference to the render data that is being composited.
	 * This field is initialized in ExecutionSystem and must only be read from that point on.
	 * @see ExecutionSystem
	 */
	RenderData *m_rd;

	/**
	 * @brief reference to the bNodeTree
	 * This field is initialized in ExecutionSystem and must only be read from that point on.
	 * @see ExecutionSystem
	 */
	bNodeTree *m_bnodetree;

	/**
	 * @brief Preview image hash table
	 * This field is initialized in ExecutionSystem and must only be read from that point on.
	 */
	bNodeInstanceHash *m_previews;

	/**
	 * @brief does this system have active opencl devices?
	 */
	bool m_hasActiveOpenCLDevices;

	/**
	 * @brief Skip slow nodes
	 */
	bool m_fastCalculation;

	/* @brief color management settings */
	const ColorManagedViewSettings *m_viewSettings;
	const ColorManagedDisplaySettings *m_displaySettings;

public:
	/**
	 * @brief constructor initializes the context with default values.
	 */
	CompositorContext();

	/**
	 * @brief set the rendering field of the context
	 */
	void setRendering(bool rendering) { this->m_rendering = rendering; }

	/**
	 * @brief get the rendering field of the context
	 */
	bool isRendering() const { return this->m_rendering; }

	/**
	 * @brief set the scene of the context
	 */
	void setRenderData(RenderData *rd) { this->m_rd = rd; }

	/**
	 * @brief set the bnodetree of the context
	 */
	void setbNodeTree(bNodeTree *bnodetree) { this->m_bnodetree = bnodetree; }

	/**
	 * @brief get the bnodetree of the context
	 */
	const bNodeTree *getbNodeTree() const { return this->m_bnodetree; }

	/**
	 * @brief get the scene of the context
	 */
	const RenderData *getRenderData() const { return this->m_rd; }
	
	void setScene(Scene *scene) { m_scene = scene; }
	Scene *getScene() const { return m_scene; }

	/**
	 * @brief set the preview image hash table
	 */
	void setPreviewHash(bNodeInstanceHash *previews) { this->m_previews = previews; }

	/**
	 * @brief get the preview image hash table
	 */
	bNodeInstanceHash *getPreviewHash() const { return this->m_previews; }

	/**
	 * @brief set view settings of color color management
	 */
	void setViewSettings(const ColorManagedViewSettings *viewSettings) { this->m_viewSettings = viewSettings; }

	/**
	 * @brief get view settings of color color management
	 */
	const ColorManagedViewSettings *getViewSettings() const { return this->m_viewSettings; }

	/**
	 * @brief set display settings of color color management
	 */
	void setDisplaySettings(const ColorManagedDisplaySettings *displaySettings) { this->m_displaySettings = displaySettings; }

	/**
	 * @brief get display settings of color color management
	 */
	const ColorManagedDisplaySettings *getDisplaySettings() const { return this->m_displaySettings; }

	/**
	 * @brief set the quality
	 */
	void setQuality(CompositorQuality quality) { this->m_quality = quality; }

	/**
	 * @brief get the quality
	 */
	const CompositorQuality getQuality() const { return this->m_quality; }

	/**
	 * @brief get the current framenumber of the scene in this context
	 */
	const int getFramenumber() const;

	/**
	 * @brief has this system active openclDevices?
	 */
	const bool getHasActiveOpenCLDevices() const { return this->m_hasActiveOpenCLDevices; }

	/**
	 * @brief set has this system active openclDevices?
	 */
	void setHasActiveOpenCLDevices(bool hasAvtiveOpenCLDevices) { this->m_hasActiveOpenCLDevices = hasAvtiveOpenCLDevices; }
	
	int getChunksize() { return this->getbNodeTree()->chunksize; }
	
	void setFastCalculation(bool fastCalculation) {this->m_fastCalculation = fastCalculation;}
	bool isFastCalculation() {return this->m_fastCalculation;}
	inline bool isGroupnodeBufferEnabled() {return this->getbNodeTree()->flag & NTREE_COM_GROUPNODE_BUFFER;}
};


#endif
