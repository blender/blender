/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/task.h>
#include <pxr/imaging/hdx/renderSetupTask.h>

#include "GPU_framebuffer.hh"
#include "GPU_texture.hh"

namespace blender::render::hydra {

/* Registers a HdxRenderTask with the render index as a task prim. */

class RenderTaskParamsDataSource final
    : public pxr::HdTypedSampledDataSource<pxr::HdxRenderTaskParams> {
 public:
  HD_DECLARE_DATASOURCE(RenderTaskParamsDataSource);

  pxr::HdxRenderTaskParams params;

  pxr::VtValue GetValue(Time /*shutterOffset*/) override
  {
    return pxr::VtValue(params);
  }
  pxr::HdxRenderTaskParams GetTypedValue(Time /*shutterOffset*/) override
  {
    return params;
  }
  bool GetContributingSampleTimesForInterval(Time /*start*/,
                                             Time /*end*/,
                                             std::vector<Time> * /*sampleTimes*/) override
  {
    return false;
  }
};

class RenderTaskDelegate {
 protected:
  pxr::HdRenderIndex *render_index_ = nullptr;
  pxr::HdRetainedSceneIndexRefPtr task_scene_index_;
  pxr::SdfPath base_id_;
  pxr::SdfPath task_id_;
  RenderTaskParamsDataSource::Handle task_params_ds_ = RenderTaskParamsDataSource::New();
  pxr::HdxRenderTaskParams &task_params_ = task_params_ds_->params;
  pxr::TfHashMap<pxr::SdfPath, pxr::HdRenderBufferDescriptor, pxr::SdfPath::Hash>
      buffer_descriptors_;

 public:
  RenderTaskDelegate(pxr::HdRenderIndex *render_index,
                     pxr::HdRetainedSceneIndexRefPtr task_scene_index,
                     pxr::SdfPath const &base_id);
  virtual ~RenderTaskDelegate() = default;

  pxr::HdTaskSharedPtr task();
  void set_camera(pxr::SdfPath const &camera_id);
  bool is_converged();
  virtual void set_viewport(pxr::GfVec4d const &viewport);
  virtual void add_aov(pxr::TfToken const &aov_key);
  virtual void read_aov(pxr::TfToken const &aov_key, void *data);
  pxr::HdRenderBuffer *get_aov_buffer(pxr::TfToken const &aov_key);
  virtual void bind();
  virtual void unbind();

 protected:
  pxr::SdfPath buffer_id(pxr::TfToken const &aov_key) const;

  void publish_task();
  void dirty_task_params();
  void publish_buffer(pxr::SdfPath const &buf_id, pxr::HdRenderBufferDescriptor const &desc);
};

class GPURenderTaskDelegate : public RenderTaskDelegate {
 private:
  gpu::FrameBuffer *framebuffer_ = nullptr;
  gpu::Texture *tex_color_ = nullptr;
  gpu::Texture *tex_depth_ = nullptr;
  unsigned int VAO_ = 0;

 public:
  using RenderTaskDelegate::RenderTaskDelegate;
  ~GPURenderTaskDelegate() override;

  void set_viewport(pxr::GfVec4d const &viewport) override;
  void add_aov(pxr::TfToken const &aov_key) override;
  void read_aov(pxr::TfToken const &aov_key, void *data) override;
  void bind() override;
  void unbind() override;
  gpu::Texture *get_aov_texture(pxr::TfToken const &aov_key);
};

}  // namespace blender::render::hydra
