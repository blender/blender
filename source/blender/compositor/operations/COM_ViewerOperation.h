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
#include "COM_NodeOperation.h"
#include "DNA_image_types.h"

namespace blender::compositor {

class ViewerOperation : public NodeOperation {
 private:
  float *m_outputBuffer;
  float *m_depthBuffer;
  Image *m_image;
  ImageUser *m_imageUser;
  bool m_active;
  float m_centerX;
  float m_centerY;
  ChunkOrdering m_chunkOrder;
  bool m_doDepthBuffer;
  ImBuf *m_ibuf;
  bool m_useAlphaInput;
  const RenderData *m_rd;
  const char *m_viewName;

  const ColorManagedViewSettings *m_viewSettings;
  const ColorManagedDisplaySettings *m_displaySettings;

  SocketReader *m_imageInput;
  SocketReader *m_alphaInput;
  SocketReader *m_depthInput;

 public:
  ViewerOperation();
  void initExecution() override;
  void deinitExecution() override;
  void executeRegion(rcti *rect, unsigned int tileNumber) override;
  void determineResolution(unsigned int resolution[2],
                           unsigned int preferredResolution[2]) override;
  bool isOutputOperation(bool /*rendering*/) const override
  {
    if (G.background) {
      return false;
    }
    return isActiveViewerOutput();
  }
  void setImage(Image *image)
  {
    this->m_image = image;
  }
  void setImageUser(ImageUser *imageUser)
  {
    this->m_imageUser = imageUser;
  }
  bool isActiveViewerOutput() const override
  {
    return this->m_active;
  }
  void setActive(bool active)
  {
    this->m_active = active;
  }
  void setCenterX(float centerX)
  {
    this->m_centerX = centerX;
  }
  void setCenterY(float centerY)
  {
    this->m_centerY = centerY;
  }
  void setChunkOrder(ChunkOrdering tileOrder)
  {
    this->m_chunkOrder = tileOrder;
  }
  float getCenterX() const
  {
    return this->m_centerX;
  }
  float getCenterY() const
  {
    return this->m_centerY;
  }
  ChunkOrdering getChunkOrder() const
  {
    return this->m_chunkOrder;
  }
  eCompositorPriority getRenderPriority() const override;
  void setUseAlphaInput(bool value)
  {
    this->m_useAlphaInput = value;
  }
  void setRenderData(const RenderData *rd)
  {
    this->m_rd = rd;
  }
  void setViewName(const char *viewName)
  {
    this->m_viewName = viewName;
  }

  void setViewSettings(const ColorManagedViewSettings *viewSettings)
  {
    this->m_viewSettings = viewSettings;
  }
  void setDisplaySettings(const ColorManagedDisplaySettings *displaySettings)
  {
    this->m_displaySettings = displaySettings;
  }

 private:
  void updateImage(rcti *rect);
  void initImage();
};

}  // namespace blender::compositor
