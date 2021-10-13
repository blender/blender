/*
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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_Enums.h"

#include "DNA_color_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

struct bNodeInstanceHash;

namespace blender::compositor {

/**
 * \brief Overall context of the compositor
 */
class CompositorContext {
 private:
  /**
   * \brief The rendering field describes if we are rendering (F12) or if we are editing (Node
   * editor) This field is initialized in ExecutionSystem and must only be read from that point
   * on. \see ExecutionSystem
   */
  bool rendering_;

  /**
   * \brief The quality of the composite.
   * This field is initialized in ExecutionSystem and must only be read from that point on.
   * \see ExecutionSystem
   */
  eCompositorQuality quality_;

  Scene *scene_;

  /**
   * \brief Reference to the render data that is being composited.
   * This field is initialized in ExecutionSystem and must only be read from that point on.
   * \see ExecutionSystem
   */
  RenderData *rd_;

  /**
   * \brief reference to the bNodeTree
   * This field is initialized in ExecutionSystem and must only be read from that point on.
   * \see ExecutionSystem
   */
  bNodeTree *bnodetree_;

  /**
   * \brief Preview image hash table
   * This field is initialized in ExecutionSystem and must only be read from that point on.
   */
  bNodeInstanceHash *previews_;

  /**
   * \brief does this system have active opencl devices?
   */
  bool hasActiveOpenCLDevices_;

  /**
   * \brief Skip slow nodes
   */
  bool fastCalculation_;

  /* \brief color management settings */
  const ColorManagedViewSettings *viewSettings_;
  const ColorManagedDisplaySettings *displaySettings_;

  /**
   * \brief active rendering view name
   */
  const char *viewName_;

 public:
  /**
   * \brief constructor initializes the context with default values.
   */
  CompositorContext();

  /**
   * \brief set the rendering field of the context
   */
  void setRendering(bool rendering)
  {
    rendering_ = rendering;
  }

  /**
   * \brief get the rendering field of the context
   */
  bool isRendering() const
  {
    return rendering_;
  }

  /**
   * \brief set the scene of the context
   */
  void setRenderData(RenderData *rd)
  {
    rd_ = rd;
  }

  /**
   * \brief set the bnodetree of the context
   */
  void setbNodeTree(bNodeTree *bnodetree)
  {
    bnodetree_ = bnodetree;
  }

  /**
   * \brief get the bnodetree of the context
   */
  const bNodeTree *getbNodeTree() const
  {
    return bnodetree_;
  }

  /**
   * \brief get the scene of the context
   */
  const RenderData *getRenderData() const
  {
    return rd_;
  }

  void setScene(Scene *scene)
  {
    scene_ = scene;
  }
  Scene *getScene() const
  {
    return scene_;
  }

  /**
   * \brief set the preview image hash table
   */
  void setPreviewHash(bNodeInstanceHash *previews)
  {
    previews_ = previews;
  }

  /**
   * \brief get the preview image hash table
   */
  bNodeInstanceHash *getPreviewHash() const
  {
    return previews_;
  }

  /**
   * \brief set view settings of color management
   */
  void setViewSettings(const ColorManagedViewSettings *viewSettings)
  {
    viewSettings_ = viewSettings;
  }

  /**
   * \brief get view settings of color management
   */
  const ColorManagedViewSettings *getViewSettings() const
  {
    return viewSettings_;
  }

  /**
   * \brief set display settings of color management
   */
  void setDisplaySettings(const ColorManagedDisplaySettings *displaySettings)
  {
    displaySettings_ = displaySettings;
  }

  /**
   * \brief get display settings of color management
   */
  const ColorManagedDisplaySettings *getDisplaySettings() const
  {
    return displaySettings_;
  }

  /**
   * \brief set the quality
   */
  void setQuality(eCompositorQuality quality)
  {
    quality_ = quality;
  }

  /**
   * \brief get the quality
   */
  eCompositorQuality getQuality() const
  {
    return quality_;
  }

  /**
   * \brief get the current frame-number of the scene in this context
   */
  int getFramenumber() const;

  /**
   * \brief has this system active openclDevices?
   */
  bool getHasActiveOpenCLDevices() const
  {
    return hasActiveOpenCLDevices_;
  }

  /**
   * \brief set has this system active openclDevices?
   */
  void setHasActiveOpenCLDevices(bool hasAvtiveOpenCLDevices)
  {
    hasActiveOpenCLDevices_ = hasAvtiveOpenCLDevices;
  }

  /** Whether it has a view with a specific name and not the default one. */
  bool has_explicit_view() const
  {
    return viewName_ && viewName_[0] != '\0';
  }

  /**
   * \brief get the active rendering view
   */
  const char *getViewName() const
  {
    return viewName_;
  }

  /**
   * \brief set the active rendering view
   */
  void setViewName(const char *viewName)
  {
    viewName_ = viewName;
  }

  int getChunksize() const
  {
    return this->getbNodeTree()->chunksize;
  }

  void setFastCalculation(bool fastCalculation)
  {
    fastCalculation_ = fastCalculation;
  }
  bool isFastCalculation() const
  {
    return fastCalculation_;
  }
  bool isGroupnodeBufferEnabled() const
  {
    return (this->getbNodeTree()->flag & NTREE_COM_GROUPNODE_BUFFER) != 0;
  }

  /**
   * \brief Get the render percentage as a factor.
   * The compositor uses a factor i.o. a percentage.
   */
  float getRenderPercentageAsFactor() const
  {
    return rd_->size * 0.01f;
  }

  Size2f get_render_size() const;

  /**
   * Get active execution model.
   */
  eExecutionModel get_execution_model() const;
};

}  // namespace blender::compositor
