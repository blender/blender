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
  char sceneName_[MAX_ID_NAME];

  /**
   * \brief local reference to the scene
   */
  const RenderData *rd_;

  /**
   * \brief reference to the output float buffer
   */
  float *outputBuffer_;

  /**
   * \brief reference to the output depth float buffer
   */
  float *depthBuffer_;

  /**
   * \brief local reference to the input image operation
   */
  SocketReader *imageInput_;

  /**
   * \brief local reference to the input alpha operation
   */
  SocketReader *alphaInput_;

  /**
   * \brief local reference to the depth operation
   */
  SocketReader *depthInput_;

  /**
   * \brief Ignore any alpha input
   */
  bool useAlphaInput_;

  /**
   * \brief operation is active for calculating final compo result
   */
  bool active_;

  /**
   * \brief View name, used for multiview
   */
  const char *viewName_;

 public:
  CompositorOperation();
  bool isActiveCompositorOutput() const
  {
    return active_;
  }
  void executeRegion(rcti *rect, unsigned int tileNumber) override;
  void setScene(const struct Scene *scene)
  {
    scene_ = scene;
  }
  void setSceneName(const char *sceneName)
  {
    BLI_strncpy(sceneName_, sceneName, sizeof(sceneName_));
  }
  void setViewName(const char *viewName)
  {
    viewName_ = viewName;
  }
  void setRenderData(const RenderData *rd)
  {
    rd_ = rd;
  }
  bool isOutputOperation(bool /*rendering*/) const override
  {
    return this->isActiveCompositorOutput();
  }
  void initExecution() override;
  void deinitExecution() override;
  eCompositorPriority getRenderPriority() const override
  {
    return eCompositorPriority::Medium;
  }
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void setUseAlphaInput(bool value)
  {
    useAlphaInput_ = value;
  }
  void setActive(bool active)
  {
    active_ = active;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
