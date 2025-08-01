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
  blender::gpu::Texture *texture_ = nullptr;
  gpu::Batch *batch_;

 public:
  DrawTexture();
  ~DrawTexture();

  void create_from_buffer(pxr::HdRenderBuffer *buffer);
  void draw(GPUShader *shader, const pxr::GfVec4d &viewport, blender::gpu::Texture *tex = nullptr);
  blender::gpu::Texture *texture() const;

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
  void notify_status(float progress, const std::string &info, const std::string &status) override;
};

}  // namespace blender::render::hydra
