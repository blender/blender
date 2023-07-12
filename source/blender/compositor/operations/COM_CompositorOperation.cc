/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CompositorOperation.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_scene.h"

#include "RE_pipeline.h"

namespace blender::compositor {

CompositorOperation::CompositorOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Value);

  this->set_render_data(nullptr);
  output_buffer_ = nullptr;
  image_input_ = nullptr;
  alpha_input_ = nullptr;

  use_alpha_input_ = false;
  active_ = false;

  scene_ = nullptr;
  scene_name_[0] = '\0';
  view_name_ = nullptr;

  flags_.use_render_border = true;
}

void CompositorOperation::init_execution()
{
  if (!active_) {
    return;
  }

  /* When initializing the tree during initial load the width and height can be zero. */
  image_input_ = get_input_socket_reader(0);
  alpha_input_ = get_input_socket_reader(1);
  if (this->get_width() * this->get_height() != 0) {
    output_buffer_ = (float *)MEM_callocN(
        sizeof(float[4]) * this->get_width() * this->get_height(), "CompositorOperation");
  }
}

void CompositorOperation::deinit_execution()
{
  if (!active_) {
    return;
  }

  if (!is_braked()) {
    Render *re = RE_GetSceneRender(scene_);
    RenderResult *rr = RE_AcquireResultWrite(re);

    if (rr) {
      RenderView *rv = RE_RenderViewGetByName(rr, view_name_);
      ImBuf *ibuf = RE_RenderViewEnsureImBuf(rr, rv);

      IMB_assign_float_buffer(ibuf, output_buffer_, IB_TAKE_OWNERSHIP);

      rr->have_combined = true;
    }
    else {
      if (output_buffer_) {
        MEM_freeN(output_buffer_);
      }
    }

    if (re) {
      RE_ReleaseResult(re);
      re = nullptr;
    }

    Image *image = BKE_image_ensure_viewer(G.main, IMA_TYPE_R_RESULT, "Render Result");
    BKE_image_partial_update_mark_full_update(image);
    BLI_thread_lock(LOCK_DRAW_IMAGE);
    BKE_image_signal(G.main, image, nullptr, IMA_SIGNAL_FREE);
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
  }
  else {
    if (output_buffer_) {
      MEM_freeN(output_buffer_);
    }
  }

  output_buffer_ = nullptr;
  image_input_ = nullptr;
  alpha_input_ = nullptr;
}

void CompositorOperation::execute_region(rcti *rect, uint /*tile_number*/)
{
  float color[8]; /* 7 is enough. */
  float *buffer = output_buffer_;

  if (!buffer) {
    return;
  }
  int x1 = rect->xmin;
  int y1 = rect->ymin;
  int x2 = rect->xmax;
  int y2 = rect->ymax;
  int offset = (y1 * this->get_width() + x1);
  int add = (this->get_width() - (x2 - x1));
  int offset4 = offset * COM_DATA_TYPE_COLOR_CHANNELS;
  int x;
  int y;
  bool breaked = false;
  int dx = 0, dy = 0;

#if 0
  const RenderData *rd = rd_;

  if (rd->mode & R_BORDER && rd->mode & R_CROP) {
    /**
     * When using cropped render result, need to re-position area of interest,
     * so it'll match bounds of render border within frame. By default, canvas
     * will be centered between full frame and cropped frame, so we use such
     * scheme to map cropped coordinates to full-frame coordinates
     *
     * ^ Y
     * |                      Width
     * +------------------------------------------------+
     * |                                                |
     * |                                                |
     * |  Centered canvas, we map coordinate from it    |
     * |              +------------------+              |
     * |              |                  |              |  H
     * |              |                  |              |  e
     * |  +------------------+ . Center  |              |  i
     * |  |           |      |           |              |  g
     * |  |           |      |           |              |  h
     * |  |....dx.... +------|-----------+              |  t
     * |  |           . dy   |                          |
     * |  +------------------+                          |
     * |  Render border, we map coordinates to it       |
     * |                                                |    X
     * +------------------------------------------------+---->
     *                      Full frame
     */

    int full_width, full_height;
    BKE_render_resolution(rd, false, &full_width, &full_height);

    dx = rd->border.xmin * full_width - (full_width - this->get_width()) / 2.0f;
    dy = rd->border.ymin * full_height - (full_height - this->get_height()) / 2.0f;
  }
#endif

  for (y = y1; y < y2 && (!breaked); y++) {
    for (x = x1; x < x2 && (!breaked); x++) {
      int input_x = x + dx, input_y = y + dy;

      image_input_->read_sampled(color, input_x, input_y, PixelSampler::Nearest);
      if (use_alpha_input_) {
        alpha_input_->read_sampled(&(color[3]), input_x, input_y, PixelSampler::Nearest);
      }

      copy_v4_v4(buffer + offset4, color);

      offset4 += COM_DATA_TYPE_COLOR_CHANNELS;
      offset++;
      if (is_braked()) {
        breaked = true;
      }
    }
    offset += add;
    offset4 += add * COM_DATA_TYPE_COLOR_CHANNELS;
  }
}

void CompositorOperation::update_memory_buffer_partial(MemoryBuffer * /*output*/,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> inputs)
{
  if (!output_buffer_) {
    return;
  }
  MemoryBuffer output_buf(output_buffer_, COM_DATA_TYPE_COLOR_CHANNELS, get_width(), get_height());
  output_buf.copy_from(inputs[0], area);
  if (use_alpha_input_) {
    output_buf.copy_from(inputs[1], area, 0, COM_DATA_TYPE_VALUE_CHANNELS, 3);
  }
}

void CompositorOperation::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  int width, height;
  BKE_render_resolution(rd_, false, &width, &height);

  /* Check actual render resolution with cropping it may differ with cropped border.rendering
   * Fix for #31777 Border Crop gives black (easy). */
  Render *re = RE_GetSceneRender(scene_);
  if (re) {
    RenderResult *rr = RE_AcquireResultRead(re);
    if (rr) {
      width = rr->rectx;
      height = rr->recty;
    }
    RE_ReleaseResult(re);
  }

  rcti local_preferred;
  BLI_rcti_init(&local_preferred, 0, width, 0, height);

  switch (execution_model_) {
    case eExecutionModel::Tiled:
      NodeOperation::determine_canvas(local_preferred, r_area);
      r_area = local_preferred;
      break;
    case eExecutionModel::FullFrame:
      set_determined_canvas_modifier([&](rcti &canvas) { canvas = local_preferred; });
      NodeOperation::determine_canvas(local_preferred, r_area);
      break;
  }
}

}  // namespace blender::compositor
