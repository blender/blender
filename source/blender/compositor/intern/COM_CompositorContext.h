/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  bool fast_calculation_;

  /**
   * \brief active rendering view name
   */
  const char *view_name_;

 public:
  /**
   * \brief constructor initializes the context with default values.
   */
  CompositorContext();

  /**
   * \brief set the rendering field of the context
   */
  void set_rendering(bool rendering)
  {
    rendering_ = rendering;
  }

  /**
   * \brief get the rendering field of the context
   */
  bool is_rendering() const
  {
    return rendering_;
  }

  /**
   * \brief set the scene of the context
   */
  void set_render_data(RenderData *rd)
  {
    rd_ = rd;
  }

  /**
   * \brief set the bnodetree of the context
   */
  void set_bnodetree(bNodeTree *bnodetree)
  {
    bnodetree_ = bnodetree;
  }

  /**
   * \brief get the bnodetree of the context
   */
  const bNodeTree *get_bnodetree() const
  {
    return bnodetree_;
  }

  /**
   * \brief get the scene of the context
   */
  const RenderData *get_render_data() const
  {
    return rd_;
  }

  void set_scene(Scene *scene)
  {
    scene_ = scene;
  }
  Scene *get_scene() const
  {
    return scene_;
  }

  /**
   * \brief set the preview image hash table
   */
  void set_preview_hash(bNodeInstanceHash *previews)
  {
    previews_ = previews;
  }

  /**
   * \brief get the preview image hash table
   */
  bNodeInstanceHash *get_preview_hash() const
  {
    return previews_;
  }

  /**
   * \brief set the quality
   */
  void set_quality(eCompositorQuality quality)
  {
    quality_ = quality;
  }

  /**
   * \brief get the quality
   */
  eCompositorQuality get_quality() const
  {
    return quality_;
  }

  /**
   * \brief get the current frame-number of the scene in this context
   */
  int get_framenumber() const;

  /**
   * \brief has this system active opencl_devices?
   */
  bool get_has_active_opencl_devices() const
  {
    return hasActiveOpenCLDevices_;
  }

  /**
   * \brief set has this system active opencl_devices?
   */
  void setHasActiveOpenCLDevices(bool hasAvtiveOpenCLDevices)
  {
    hasActiveOpenCLDevices_ = hasAvtiveOpenCLDevices;
  }

  /** Whether it has a view with a specific name and not the default one. */
  bool has_explicit_view() const
  {
    return view_name_ && view_name_[0] != '\0';
  }

  /**
   * \brief get the active rendering view
   */
  const char *get_view_name() const
  {
    return view_name_;
  }

  /**
   * \brief set the active rendering view
   */
  void set_view_name(const char *view_name)
  {
    view_name_ = view_name;
  }

  int get_chunksize() const
  {
    return this->get_bnodetree()->chunksize;
  }

  void set_fast_calculation(bool fast_calculation)
  {
    fast_calculation_ = fast_calculation;
  }
  bool is_fast_calculation() const
  {
    return fast_calculation_;
  }
  bool is_groupnode_buffer_enabled() const
  {
    return (this->get_bnodetree()->flag & NTREE_COM_GROUPNODE_BUFFER) != 0;
  }

  /**
   * \brief Get the render percentage as a factor.
   * The compositor uses a factor i.o. a percentage.
   */
  float get_render_percentage_as_factor() const
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
