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

#include "COM_ViewerOperation.h"
#include "BKE_image.h"
#include "BKE_scene.h"
#include "COM_ExecutionSystem.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

static int MAX_VIEWER_TRANSLATION_PADDING = 12000;

ViewerOperation::ViewerOperation()
{
  this->set_image(nullptr);
  this->set_image_user(nullptr);
  output_buffer_ = nullptr;
  depth_buffer_ = nullptr;
  active_ = false;
  do_depth_buffer_ = false;
  view_settings_ = nullptr;
  display_settings_ = nullptr;
  use_alpha_input_ = false;

  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);

  image_input_ = nullptr;
  alpha_input_ = nullptr;
  depth_input_ = nullptr;
  rd_ = nullptr;
  view_name_ = nullptr;
  flags_.use_viewer_border = true;
  flags_.is_viewer_operation = true;
}

void ViewerOperation::init_execution()
{
  /* When initializing the tree during initial load the width and height can be zero. */
  image_input_ = get_input_socket_reader(0);
  alpha_input_ = get_input_socket_reader(1);
  depth_input_ = get_input_socket_reader(2);
  do_depth_buffer_ = (depth_input_ != nullptr);

  if (is_active_viewer_output() && !exec_system_->is_breaked()) {
    init_image();
  }
}

void ViewerOperation::deinit_execution()
{
  image_input_ = nullptr;
  alpha_input_ = nullptr;
  depth_input_ = nullptr;
  output_buffer_ = nullptr;
}

void ViewerOperation::execute_region(rcti *rect, unsigned int /*tile_number*/)
{
  float *buffer = output_buffer_;
  float *depthbuffer = depth_buffer_;
  if (!buffer) {
    return;
  }
  const int x1 = rect->xmin;
  const int y1 = rect->ymin;
  const int x2 = rect->xmax;
  const int y2 = rect->ymax;
  const int offsetadd = (this->get_width() - (x2 - x1));
  const int offsetadd4 = offsetadd * 4;
  int offset = (y1 * this->get_width() + x1);
  int offset4 = offset * 4;
  float alpha[4], depth[4];
  int x;
  int y;
  bool breaked = false;

  for (y = y1; y < y2 && (!breaked); y++) {
    for (x = x1; x < x2; x++) {
      image_input_->read_sampled(&(buffer[offset4]), x, y, PixelSampler::Nearest);
      if (use_alpha_input_) {
        alpha_input_->read_sampled(alpha, x, y, PixelSampler::Nearest);
        buffer[offset4 + 3] = alpha[0];
      }
      depth_input_->read_sampled(depth, x, y, PixelSampler::Nearest);
      depthbuffer[offset] = depth[0];

      offset++;
      offset4 += 4;
    }
    if (is_braked()) {
      breaked = true;
    }
    offset += offsetadd;
    offset4 += offsetadd4;
  }
  update_image(rect);
}

void ViewerOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  const int scene_render_width = rd_->xsch * rd_->size / 100;
  const int scene_render_height = rd_->ysch * rd_->size / 100;

  rcti local_preferred = preferred_area;
  local_preferred.xmax = local_preferred.xmin + scene_render_width;
  local_preferred.ymax = local_preferred.ymin + scene_render_height;

  NodeOperation::determine_canvas(local_preferred, r_area);
}

void ViewerOperation::init_image()
{
  Image *ima = image_;
  ImageUser iuser = *image_user_;
  void *lock;
  ImBuf *ibuf;

  /* make sure the image has the correct number of views */
  if (ima && BKE_scene_multiview_is_render_view_first(rd_, view_name_)) {
    BKE_image_ensure_viewer_views(rd_, ima, image_user_);
  }

  BLI_thread_lock(LOCK_DRAW_IMAGE);

  /* local changes to the original ImageUser */
  iuser.multi_index = BKE_scene_multiview_view_id_get(rd_, view_name_);
  ibuf = BKE_image_acquire_ibuf(ima, &iuser, &lock);

  if (!ibuf) {
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
    return;
  }

  int padding_x = abs(canvas_.xmin) * 2;
  int padding_y = abs(canvas_.ymin) * 2;
  if (padding_x > MAX_VIEWER_TRANSLATION_PADDING) {
    padding_x = MAX_VIEWER_TRANSLATION_PADDING;
  }
  if (padding_y > MAX_VIEWER_TRANSLATION_PADDING) {
    padding_y = MAX_VIEWER_TRANSLATION_PADDING;
  }

  display_width_ = get_width() + padding_x;
  display_height_ = get_height() + padding_y;
  if (ibuf->x != display_width_ || ibuf->y != display_height_) {
    imb_freerectImBuf(ibuf);
    imb_freerectfloatImBuf(ibuf);
    IMB_freezbuffloatImBuf(ibuf);
    ibuf->x = display_width_;
    ibuf->y = display_height_;
    /* zero size can happen if no image buffers exist to define a sensible resolution */
    if (ibuf->x > 0 && ibuf->y > 0) {
      imb_addrectfloatImBuf(ibuf);
    }

    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  }

  if (do_depth_buffer_) {
    addzbuffloatImBuf(ibuf);
  }

  /* now we combine the input with ibuf */
  output_buffer_ = ibuf->rect_float;

  /* needed for display buffer update */
  ibuf_ = ibuf;

  if (do_depth_buffer_) {
    depth_buffer_ = ibuf->zbuf_float;
  }

  BKE_image_release_ibuf(image_, ibuf_, lock);

  BLI_thread_unlock(LOCK_DRAW_IMAGE);
}

void ViewerOperation::update_image(const rcti *rect)
{
  if (exec_system_->is_breaked()) {
    return;
  }

  float *buffer = output_buffer_;
  IMB_partial_display_buffer_update(ibuf_,
                                    buffer,
                                    nullptr,
                                    display_width_,
                                    0,
                                    0,
                                    view_settings_,
                                    display_settings_,
                                    rect->xmin,
                                    rect->ymin,
                                    rect->xmax,
                                    rect->ymax);

  /* This could be improved to use partial updates. For now disabled as the full frame compositor
   * would not use partial frames anymore and the image engine requires more testing.*/
  BKE_image_partial_update_mark_full_update(image_);
  this->update_draw();
}

eCompositorPriority ViewerOperation::get_render_priority() const
{
  if (this->is_active_viewer_output()) {
    return eCompositorPriority::High;
  }

  return eCompositorPriority::Low;
}

void ViewerOperation::update_memory_buffer_partial(MemoryBuffer *UNUSED(output),
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  if (!output_buffer_) {
    return;
  }

  const int offset_x = area.xmin + (canvas_.xmin > 0 ? canvas_.xmin * 2 : 0);
  const int offset_y = area.ymin + (canvas_.ymin > 0 ? canvas_.ymin * 2 : 0);
  MemoryBuffer output_buffer(
      output_buffer_, COM_DATA_TYPE_COLOR_CHANNELS, display_width_, display_height_);
  const MemoryBuffer *input_image = inputs[0];
  output_buffer.copy_from(input_image, area, offset_x, offset_y);
  if (use_alpha_input_) {
    const MemoryBuffer *input_alpha = inputs[1];
    output_buffer.copy_from(
        input_alpha, area, 0, COM_DATA_TYPE_VALUE_CHANNELS, offset_x, offset_y, 3);
  }

  if (depth_buffer_) {
    MemoryBuffer depth_buffer(
        depth_buffer_, COM_DATA_TYPE_VALUE_CHANNELS, display_width_, display_height_);
    const MemoryBuffer *input_depth = inputs[2];
    depth_buffer.copy_from(input_depth, area, offset_x, offset_y);
  }

  rcti display_area;
  BLI_rcti_init(&display_area,
                offset_x,
                offset_x + BLI_rcti_size_x(&area),
                offset_y,
                offset_y + BLI_rcti_size_y(&area));
  update_image(&display_area);
}

void ViewerOperation::clear_display_buffer()
{
  BLI_assert(is_active_viewer_output());
  if (exec_system_->is_breaked()) {
    return;
  }

  init_image();
  if (output_buffer_ == nullptr) {
    return;
  }

  size_t buf_bytes = (size_t)ibuf_->y * ibuf_->x * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float);
  if (buf_bytes > 0) {
    memset(output_buffer_, 0, buf_bytes);
    rcti display_area;
    BLI_rcti_init(&display_area, 0, ibuf_->x, 0, ibuf_->y);
    update_image(&display_area);
  }
}

}  // namespace blender::compositor
