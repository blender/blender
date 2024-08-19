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

#include "BKE_global.h"
#include "BLI_rect.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_image_types.h"

namespace blender::compositor {

class ViewerOperation : public MultiThreadedOperation {
 private:
  /* TODO(manzanilla): To be removed together with tiled implementation. */
  float *output_buffer_;
  float *depth_buffer_;

  Image *image_;
  ImageUser *image_user_;
  bool active_;
  float center_x_;
  float center_y_;
  ChunkOrdering chunk_order_;
  bool do_depth_buffer_;
  ImBuf *ibuf_;
  bool use_alpha_input_;
  const RenderData *rd_;
  const char *view_name_; 

  const ColorManagedViewSettings *view_settings_;
  const ColorManagedDisplaySettings *display_settings_;

  SocketReader *image_input_;
  SocketReader *alpha_input_;
  SocketReader *depth_input_;

  int display_width_;
  int display_height_;

 public:
  ViewerOperation();
  void init_execution() override;
  void deinit_execution() override;
  void execute_region(rcti *rect, unsigned int tile_number) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  bool is_output_operation(bool /*rendering*/) const override
  {
    // ETH ASL: Allow background rendering
    // if (G.background) {
    //   return false;
    // }
    return is_active_viewer_output();
  }
  void set_image(Image *image)
  {
    image_ = image;
  }
  void set_image_user(ImageUser *image_user)
  {
    image_user_ = image_user;
  }
  bool is_active_viewer_output() const override
  {
    return active_;
  }
  void set_active(bool active)
  {
    active_ = active;
  }
  void setCenterX(float centerX)
  {
    center_x_ = centerX;
  }
  void setCenterY(float centerY)
  {
    center_y_ = centerY;
  }
  void set_chunk_order(ChunkOrdering tile_order)
  {
    chunk_order_ = tile_order;
  }
  float getCenterX() const
  {
    return center_x_;
  }
  float getCenterY() const
  {
    return center_y_;
  }
  ChunkOrdering get_chunk_order() const
  {
    return chunk_order_;
  }
  eCompositorPriority get_render_priority() const override;
  void set_use_alpha_input(bool value)
  {
    use_alpha_input_ = value;
  }
  void set_render_data(const RenderData *rd)
  {
    rd_ = rd;
  }
  void set_view_name(const char *view_name)
  {
    view_name_ = view_name;
  }

  void set_view_settings(const ColorManagedViewSettings *view_settings)
  {
    view_settings_ = view_settings;
  }
  void set_display_settings(const ColorManagedDisplaySettings *display_settings)
  {
    display_settings_ = display_settings;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

  void clear_display_buffer();

 private:
  void update_image(const rcti *rect);
  void init_image();
};

}  // namespace blender::compositor
