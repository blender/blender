/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/hdx/renderSetupTask.h>

#include "GPU_framebuffer.h"
#include "GPU_texture.h"

namespace blender::render::hydra {

/* Delegate to create a render task with given camera, viewport and AOVs. */

class RenderTaskDelegate : public pxr::HdSceneDelegate {
 protected:
  pxr::SdfPath task_id_;
  pxr::HdxRenderTaskParams task_params_;
  pxr::TfHashMap<pxr::SdfPath, pxr::HdRenderBufferDescriptor, pxr::SdfPath::Hash>
      buffer_descriptors_;

 public:
  RenderTaskDelegate(pxr::HdRenderIndex *parent_index, pxr::SdfPath const &delegate_id);
  ~RenderTaskDelegate() override = default;

  /* Delegate methods */
  pxr::VtValue Get(pxr::SdfPath const &id, pxr::TfToken const &key) override;
  pxr::TfTokenVector GetTaskRenderTags(pxr::SdfPath const &id) override;
  pxr::HdRenderBufferDescriptor GetRenderBufferDescriptor(pxr::SdfPath const &id) override;

  pxr::HdTaskSharedPtr task();
  void set_camera(pxr::SdfPath const &camera_id);
  bool is_converged();
  virtual void set_viewport(pxr::GfVec4d const &viewport);
  virtual void add_aov(pxr::TfToken const &aov_key);
  virtual void read_aov(pxr::TfToken const &aov_key, void *data);
  virtual void read_aov(pxr::TfToken const &aov_key, GPUTexture *texture);
  virtual void bind();
  virtual void unbind();

 protected:
  pxr::SdfPath buffer_id(pxr::TfToken const &aov_key) const;
};

class GPURenderTaskDelegate : public RenderTaskDelegate {
 private:
  GPUFrameBuffer *framebuffer_ = nullptr;
  GPUTexture *tex_color_ = nullptr;
  GPUTexture *tex_depth_ = nullptr;
  unsigned int VAO_ = 0;

 public:
  using RenderTaskDelegate::RenderTaskDelegate;
  ~GPURenderTaskDelegate() override;

  void set_viewport(pxr::GfVec4d const &viewport) override;
  void add_aov(pxr::TfToken const &aov_key) override;
  void read_aov(pxr::TfToken const &aov_key, void *data) override;
  void read_aov(pxr::TfToken const &aov_key, GPUTexture *texture) override;
  void bind() override;
  void unbind() override;
  GPUTexture *aov_texture(pxr::TfToken const &aov_key);
};

}  // namespace blender::render::hydra
