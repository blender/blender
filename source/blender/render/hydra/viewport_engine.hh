/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/imaging/hd/renderBuffer.h>

#include "GPU_batch.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "engine.hh"

namespace blender::render::hydra {

class DrawTexture {
 private:
  GPUTexture *texture_ = nullptr;
  GPUBatch *batch_;

 public:
  DrawTexture();
  ~DrawTexture();

  void write_data(int width, int height, const void *data);
  void draw(GPUShader *shader, const pxr::GfVec4d &viewport, GPUTexture *tex = nullptr);
  GPUTexture *texture() const;

 private:
};

class ViewportEngine : public Engine {
 private:
  double time_begin_;
  DrawTexture draw_texture_;

 public:
  using Engine::Engine;

  void render() override;
  void render(bContext *context);

 protected:
  void notify_status(float progress, const std::string &title, const std::string &info) override;
};

}  // namespace blender::render::hydra
