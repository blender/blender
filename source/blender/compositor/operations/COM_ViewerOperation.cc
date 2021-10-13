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
  this->setImage(nullptr);
  this->setImageUser(nullptr);
  m_outputBuffer = nullptr;
  m_depthBuffer = nullptr;
  m_active = false;
  m_doDepthBuffer = false;
  m_viewSettings = nullptr;
  m_displaySettings = nullptr;
  m_useAlphaInput = false;

  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);

  m_imageInput = nullptr;
  m_alphaInput = nullptr;
  m_depthInput = nullptr;
  m_rd = nullptr;
  m_viewName = nullptr;
  flags.use_viewer_border = true;
  flags.is_viewer_operation = true;
}

void ViewerOperation::initExecution()
{
  /* When initializing the tree during initial load the width and height can be zero. */
  m_imageInput = getInputSocketReader(0);
  m_alphaInput = getInputSocketReader(1);
  m_depthInput = getInputSocketReader(2);
  m_doDepthBuffer = (m_depthInput != nullptr);

  if (isActiveViewerOutput() && !exec_system_->is_breaked()) {
    initImage();
  }
}

void ViewerOperation::deinitExecution()
{
  m_imageInput = nullptr;
  m_alphaInput = nullptr;
  m_depthInput = nullptr;
  m_outputBuffer = nullptr;
}

void ViewerOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  float *buffer = m_outputBuffer;
  float *depthbuffer = m_depthBuffer;
  if (!buffer) {
    return;
  }
  const int x1 = rect->xmin;
  const int y1 = rect->ymin;
  const int x2 = rect->xmax;
  const int y2 = rect->ymax;
  const int offsetadd = (this->getWidth() - (x2 - x1));
  const int offsetadd4 = offsetadd * 4;
  int offset = (y1 * this->getWidth() + x1);
  int offset4 = offset * 4;
  float alpha[4], depth[4];
  int x;
  int y;
  bool breaked = false;

  for (y = y1; y < y2 && (!breaked); y++) {
    for (x = x1; x < x2; x++) {
      m_imageInput->readSampled(&(buffer[offset4]), x, y, PixelSampler::Nearest);
      if (m_useAlphaInput) {
        m_alphaInput->readSampled(alpha, x, y, PixelSampler::Nearest);
        buffer[offset4 + 3] = alpha[0];
      }
      m_depthInput->readSampled(depth, x, y, PixelSampler::Nearest);
      depthbuffer[offset] = depth[0];

      offset++;
      offset4 += 4;
    }
    if (isBraked()) {
      breaked = true;
    }
    offset += offsetadd;
    offset4 += offsetadd4;
  }
  updateImage(rect);
}

void ViewerOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  const int sceneRenderWidth = m_rd->xsch * m_rd->size / 100;
  const int sceneRenderHeight = m_rd->ysch * m_rd->size / 100;

  rcti local_preferred = preferred_area;
  local_preferred.xmax = local_preferred.xmin + sceneRenderWidth;
  local_preferred.ymax = local_preferred.ymin + sceneRenderHeight;

  NodeOperation::determine_canvas(local_preferred, r_area);
}

void ViewerOperation::initImage()
{
  Image *ima = m_image;
  ImageUser iuser = *m_imageUser;
  void *lock;
  ImBuf *ibuf;

  /* make sure the image has the correct number of views */
  if (ima && BKE_scene_multiview_is_render_view_first(m_rd, m_viewName)) {
    BKE_image_ensure_viewer_views(m_rd, ima, m_imageUser);
  }

  BLI_thread_lock(LOCK_DRAW_IMAGE);

  /* local changes to the original ImageUser */
  iuser.multi_index = BKE_scene_multiview_view_id_get(m_rd, m_viewName);
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

  display_width_ = getWidth() + padding_x;
  display_height_ = getHeight() + padding_y;
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
    ImageTile *tile = BKE_image_get_tile(ima, 0);
    tile->ok = IMA_OK_LOADED;

    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  }

  if (m_doDepthBuffer) {
    addzbuffloatImBuf(ibuf);
  }

  /* now we combine the input with ibuf */
  m_outputBuffer = ibuf->rect_float;

  /* needed for display buffer update */
  m_ibuf = ibuf;

  if (m_doDepthBuffer) {
    m_depthBuffer = ibuf->zbuf_float;
  }

  BKE_image_release_ibuf(m_image, m_ibuf, lock);

  BLI_thread_unlock(LOCK_DRAW_IMAGE);
}

void ViewerOperation::updateImage(const rcti *rect)
{
  if (exec_system_->is_breaked()) {
    return;
  }

  float *buffer = m_outputBuffer;
  IMB_partial_display_buffer_update(m_ibuf,
                                    buffer,
                                    nullptr,
                                    display_width_,
                                    0,
                                    0,
                                    m_viewSettings,
                                    m_displaySettings,
                                    rect->xmin,
                                    rect->ymin,
                                    rect->xmax,
                                    rect->ymax);
  m_image->gpuflag |= IMA_GPU_REFRESH;
  this->updateDraw();
}

eCompositorPriority ViewerOperation::getRenderPriority() const
{
  if (this->isActiveViewerOutput()) {
    return eCompositorPriority::High;
  }

  return eCompositorPriority::Low;
}

void ViewerOperation::update_memory_buffer_partial(MemoryBuffer *UNUSED(output),
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  if (!m_outputBuffer) {
    return;
  }

  const int offset_x = area.xmin + (canvas_.xmin > 0 ? canvas_.xmin * 2 : 0);
  const int offset_y = area.ymin + (canvas_.ymin > 0 ? canvas_.ymin * 2 : 0);
  MemoryBuffer output_buffer(
      m_outputBuffer, COM_DATA_TYPE_COLOR_CHANNELS, display_width_, display_height_);
  const MemoryBuffer *input_image = inputs[0];
  output_buffer.copy_from(input_image, area, offset_x, offset_y);
  if (m_useAlphaInput) {
    const MemoryBuffer *input_alpha = inputs[1];
    output_buffer.copy_from(
        input_alpha, area, 0, COM_DATA_TYPE_VALUE_CHANNELS, offset_x, offset_y, 3);
  }

  if (m_depthBuffer) {
    MemoryBuffer depth_buffer(
        m_depthBuffer, COM_DATA_TYPE_VALUE_CHANNELS, display_width_, display_height_);
    const MemoryBuffer *input_depth = inputs[2];
    depth_buffer.copy_from(input_depth, area, offset_x, offset_y);
  }

  rcti display_area;
  BLI_rcti_init(&display_area,
                offset_x,
                offset_x + BLI_rcti_size_x(&area),
                offset_y,
                offset_y + BLI_rcti_size_y(&area));
  updateImage(&display_area);
}

void ViewerOperation::clear_display_buffer()
{
  BLI_assert(isActiveViewerOutput());
  if (exec_system_->is_breaked()) {
    return;
  }

  initImage();
  if (m_outputBuffer == nullptr) {
    return;
  }

  size_t buf_bytes = (size_t)m_ibuf->y * m_ibuf->x * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float);
  if (buf_bytes > 0) {
    memset(m_outputBuffer, 0, buf_bytes);
    rcti display_area;
    BLI_rcti_init(&display_area, 0, m_ibuf->x, 0, m_ibuf->y);
    updateImage(&display_area);
  }
}

}  // namespace blender::compositor
