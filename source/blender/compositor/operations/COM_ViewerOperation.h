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
  float *outputBuffer_;
  float *depthBuffer_;

  Image *image_;
  ImageUser *imageUser_;
  bool active_;
  float centerX_;
  float centerY_;
  ChunkOrdering chunkOrder_;
  bool doDepthBuffer_;
  ImBuf *ibuf_;
  bool useAlphaInput_;
  const RenderData *rd_;
  const char *viewName_;

  const ColorManagedViewSettings *viewSettings_;
  const ColorManagedDisplaySettings *displaySettings_;

  SocketReader *imageInput_;
  SocketReader *alphaInput_;
  SocketReader *depthInput_;

  int display_width_;
  int display_height_;

 public:
  ViewerOperation();
  void initExecution() override;
  void deinitExecution() override;
  void executeRegion(rcti *rect, unsigned int tileNumber) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  bool isOutputOperation(bool /*rendering*/) const override
  {
    if (G.background) {
      return false;
    }
    return isActiveViewerOutput();
  }
  void setImage(Image *image)
  {
    image_ = image;
  }
  void setImageUser(ImageUser *imageUser)
  {
    imageUser_ = imageUser;
  }
  bool isActiveViewerOutput() const override
  {
    return active_;
  }
  void setActive(bool active)
  {
    active_ = active;
  }
  void setCenterX(float centerX)
  {
    centerX_ = centerX;
  }
  void setCenterY(float centerY)
  {
    centerY_ = centerY;
  }
  void setChunkOrder(ChunkOrdering tileOrder)
  {
    chunkOrder_ = tileOrder;
  }
  float getCenterX() const
  {
    return centerX_;
  }
  float getCenterY() const
  {
    return centerY_;
  }
  ChunkOrdering getChunkOrder() const
  {
    return chunkOrder_;
  }
  eCompositorPriority getRenderPriority() const override;
  void setUseAlphaInput(bool value)
  {
    useAlphaInput_ = value;
  }
  void setRenderData(const RenderData *rd)
  {
    rd_ = rd;
  }
  void setViewName(const char *viewName)
  {
    viewName_ = viewName;
  }

  void setViewSettings(const ColorManagedViewSettings *viewSettings)
  {
    viewSettings_ = viewSettings;
  }
  void setDisplaySettings(const ColorManagedDisplaySettings *displaySettings)
  {
    displaySettings_ = displaySettings;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

  void clear_display_buffer();

 private:
  void updateImage(const rcti *rect);
  void initImage();
};

}  // namespace blender::compositor
