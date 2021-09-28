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
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"
#include "PIL_time.h"
#include "WM_api.h"
#include "WM_types.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

ViewerOperation::ViewerOperation()
{
  this->setImage(nullptr);
  this->setImageUser(nullptr);
  this->m_outputBuffer = nullptr;
  this->m_depthBuffer = nullptr;
  this->m_active = false;
  this->m_doDepthBuffer = false;
  this->m_viewSettings = nullptr;
  this->m_displaySettings = nullptr;
  this->m_useAlphaInput = false;

  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);

  this->m_imageInput = nullptr;
  this->m_alphaInput = nullptr;
  this->m_depthInput = nullptr;
  this->m_rd = nullptr;
  this->m_viewName = nullptr;
  flags.use_viewer_border = true;
  flags.is_viewer_operation = true;
}

void ViewerOperation::initExecution()
{
  /* When initializing the tree during initial load the width and height can be zero. */
  this->m_imageInput = getInputSocketReader(0);
  this->m_alphaInput = getInputSocketReader(1);
  this->m_depthInput = getInputSocketReader(2);
  this->m_doDepthBuffer = (this->m_depthInput != nullptr);

  if (isActiveViewerOutput()) {
    initImage();
  }
}

void ViewerOperation::deinitExecution()
{
  this->m_imageInput = nullptr;
  this->m_alphaInput = nullptr;
  this->m_depthInput = nullptr;
  this->m_outputBuffer = nullptr;
}

void ViewerOperation::executeRegion(rcti *rect, unsigned int /*tileNumber*/)
{
  float *buffer = this->m_outputBuffer;
  float *depthbuffer = this->m_depthBuffer;
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
      this->m_imageInput->readSampled(&(buffer[offset4]), x, y, PixelSampler::Nearest);
      if (this->m_useAlphaInput) {
        this->m_alphaInput->readSampled(alpha, x, y, PixelSampler::Nearest);
        buffer[offset4 + 3] = alpha[0];
      }
      this->m_depthInput->readSampled(depth, x, y, PixelSampler::Nearest);
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
  const int sceneRenderWidth = this->m_rd->xsch * this->m_rd->size / 100;
  const int sceneRenderHeight = this->m_rd->ysch * this->m_rd->size / 100;

  rcti local_preferred = preferred_area;
  local_preferred.xmax = local_preferred.xmin + sceneRenderWidth;
  local_preferred.ymax = local_preferred.ymin + sceneRenderHeight;
  NodeOperation::determine_canvas(local_preferred, r_area);
}

void ViewerOperation::initImage()
{
  Image *ima = this->m_image;
  ImageUser iuser = *this->m_imageUser;
  void *lock;
  ImBuf *ibuf;

  /* make sure the image has the correct number of views */
  if (ima && BKE_scene_multiview_is_render_view_first(this->m_rd, this->m_viewName)) {
    BKE_image_ensure_viewer_views(this->m_rd, ima, this->m_imageUser);
  }

  BLI_thread_lock(LOCK_DRAW_IMAGE);

  /* local changes to the original ImageUser */
  iuser.multi_index = BKE_scene_multiview_view_id_get(this->m_rd, this->m_viewName);
  ibuf = BKE_image_acquire_ibuf(ima, &iuser, &lock);

  if (!ibuf) {
    BLI_thread_unlock(LOCK_DRAW_IMAGE);
    return;
  }
  if (ibuf->x != (int)getWidth() || ibuf->y != (int)getHeight()) {

    imb_freerectImBuf(ibuf);
    imb_freerectfloatImBuf(ibuf);
    IMB_freezbuffloatImBuf(ibuf);
    ibuf->x = getWidth();
    ibuf->y = getHeight();
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
  this->m_outputBuffer = ibuf->rect_float;

  /* needed for display buffer update */
  this->m_ibuf = ibuf;

  if (m_doDepthBuffer) {
    this->m_depthBuffer = ibuf->zbuf_float;
  }

  BKE_image_release_ibuf(this->m_image, this->m_ibuf, lock);

  BLI_thread_unlock(LOCK_DRAW_IMAGE);
}

void ViewerOperation::updateImage(const rcti *rect)
{
  float *buffer = m_outputBuffer;
  IMB_partial_display_buffer_update(this->m_ibuf,
                                    buffer,
                                    nullptr,
                                    getWidth(),
                                    0,
                                    0,
                                    this->m_viewSettings,
                                    this->m_displaySettings,
                                    rect->xmin,
                                    rect->ymin,
                                    rect->xmax,
                                    rect->ymax);
  this->m_image->gpuflag |= IMA_GPU_REFRESH;
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

  MemoryBuffer output_buffer(
      m_outputBuffer, COM_DATA_TYPE_COLOR_CHANNELS, getWidth(), getHeight());
  const MemoryBuffer *input_image = inputs[0];
  output_buffer.copy_from(input_image, area);
  if (this->m_useAlphaInput) {
    const MemoryBuffer *input_alpha = inputs[1];
    output_buffer.copy_from(input_alpha, area, 0, COM_DATA_TYPE_VALUE_CHANNELS, 3);
  }

  if (m_depthBuffer) {
    MemoryBuffer depth_buffer(
        m_depthBuffer, COM_DATA_TYPE_VALUE_CHANNELS, getWidth(), getHeight());
    const MemoryBuffer *input_depth = inputs[2];
    depth_buffer.copy_from(input_depth, area);
  }

  updateImage(&area);
}

void ViewerOperation::clear_display_buffer()
{
  BLI_assert(isActiveViewerOutput());
  initImage();
  size_t buf_bytes = (size_t)m_ibuf->y * m_ibuf->x * COM_DATA_TYPE_COLOR_CHANNELS * sizeof(float);
  if (buf_bytes > 0) {
    memset(m_outputBuffer, 0, buf_bytes);
    rcti display_area;
    BLI_rcti_init(&display_area, 0, m_ibuf->x, 0, m_ibuf->y);
    updateImage(&display_area);
  }
}

}  // namespace blender::compositor
