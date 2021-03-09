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

#include "COM_ImageOperation.h"

class MultilayerBaseOperation : public BaseImageOperation {
 private:
  int m_passId;
  int m_view;

 protected:
  RenderLayer *m_renderLayer;
  RenderPass *m_renderPass;
  ImBuf *getImBuf() override;

 public:
  /**
   * Constructor
   */
  MultilayerBaseOperation(RenderLayer *render_layer, RenderPass *render_pass, int view);
};

class MultilayerColorOperation : public MultilayerBaseOperation {
 public:
  MultilayerColorOperation(RenderLayer *render_layer, RenderPass *render_pass, int view)
      : MultilayerBaseOperation(render_layer, render_pass, view)
  {
    this->addOutputSocket(COM_DT_COLOR);
  }
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
  std::unique_ptr<MetaData> getMetaData() const override;
};

class MultilayerValueOperation : public MultilayerBaseOperation {
 public:
  MultilayerValueOperation(RenderLayer *render_layer, RenderPass *render_pass, int view)
      : MultilayerBaseOperation(render_layer, render_pass, view)
  {
    this->addOutputSocket(COM_DT_VALUE);
  }
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};

class MultilayerVectorOperation : public MultilayerBaseOperation {
 public:
  MultilayerVectorOperation(RenderLayer *render_layer, RenderPass *render_pass, int view)
      : MultilayerBaseOperation(render_layer, render_pass, view)
  {
    this->addOutputSocket(COM_DT_VECTOR);
  }
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
};
