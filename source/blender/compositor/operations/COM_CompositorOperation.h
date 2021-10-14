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

#include "COM_MultiThreadedOperation.h"

struct Scene;

namespace blender::compositor {

/**
 * \brief Compositor output operation
 */
class CompositorOperation : public MultiThreadedOperation {
 private:
  const struct Scene *scene_;
  /**
   * \brief Scene name, used for getting the render output, includes 'SC' prefix.
   */
  char scene_name_[MAX_ID_NAME];

  /**
   * \brief local reference to the scene
   */
  const RenderData *rd_;

  /**
   * \brief reference to the output float buffer
   */
  float *output_buffer_;

  /**
   * \brief reference to the output depth float buffer
   */
  float *depth_buffer_;

  /**
   * \brief local reference to the input image operation
   */
  SocketReader *image_input_;

  /**
   * \brief local reference to the input alpha operation
   */
  SocketReader *alpha_input_;

  /**
   * \brief local reference to the depth operation
   */
  SocketReader *depth_input_;

  /**
   * \brief Ignore any alpha input
   */
  bool use_alpha_input_;

  /**
   * \brief operation is active for calculating final compo result
   */
  bool active_;

  /**
   * \brief View name, used for multiview
   */
  const char *view_name_;

 public:
  CompositorOperation();
  bool is_active_compositor_output() const
  {
    return active_;
  }
  void execute_region(rcti *rect, unsigned int tile_number) override;
  void set_scene(const struct Scene *scene)
  {
    scene_ = scene;
  }
  void set_scene_name(const char *scene_name)
  {
    BLI_strncpy(scene_name_, scene_name, sizeof(scene_name_));
  }
  void set_view_name(const char *view_name)
  {
    view_name_ = view_name;
  }
  void set_render_data(const RenderData *rd)
  {
    rd_ = rd;
  }
  bool is_output_operation(bool /*rendering*/) const override
  {
    return this->is_active_compositor_output();
  }
  void init_execution() override;
  void deinit_execution() override;
  eCompositorPriority get_render_priority() const override
  {
    return eCompositorPriority::Medium;
  }
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void set_use_alpha_input(bool value)
  {
    use_alpha_input_ = value;
  }
  void set_active(bool active)
  {
    active_ = active;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
