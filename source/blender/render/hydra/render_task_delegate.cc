/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "render_task_delegate.hh"

#ifdef WITH_OPENGL_BACKEND
#  include "GPU_context.hh"
#  include <epoxy/gl.h>
#endif

#include <pxr/imaging/hd/legacyTaskFactory.h>
#include <pxr/imaging/hd/legacyTaskSchema.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/renderBufferSchema.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hdx/renderTask.h>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include <Eigen/Core>

#include "engine.hh"

namespace blender::render::hydra {

RenderTaskDelegate::RenderTaskDelegate(pxr::HdRenderIndex *render_index,
                                       pxr::HdRetainedSceneIndexRefPtr task_scene_index,
                                       pxr::SdfPath const &base_id)
    : render_index_(render_index),
      task_scene_index_(std::move(task_scene_index)),
      base_id_(base_id),
      task_id_(base_id.AppendElementString("task"))
{
  task_params_.enableLighting = true;
  task_params_.alphaThreshold = 0.1f;

  /* Disable this so Metal and OpenGL match in Storm render tests, only
   * the former seems to use multisample. */
  task_params_.useAovMultiSample = false;

  publish_task();

  CLOG_DEBUG(LOG_HYDRA_RENDER, "%s", task_id_.GetText());
}

void RenderTaskDelegate::publish_task()
{
  const pxr::HdRprimCollection collection(pxr::HdTokens->geometry,
                                          pxr::HdReprSelector(pxr::HdReprTokens->smoothHull));
  const pxr::TfTokenVector render_tags = {pxr::HdRenderTagTokens->geometry};

  pxr::HdContainerDataSourceHandle task_ds =
      pxr::HdLegacyTaskSchema::Builder()
          .SetFactory(
              pxr::HdRetainedTypedSampledDataSource<pxr::HdLegacyTaskFactorySharedPtr>::New(
                  pxr::HdMakeLegacyTaskFactory<pxr::HdxRenderTask>()))
          .SetParameters(task_params_ds_)
          .SetCollection(
              pxr::HdRetainedTypedSampledDataSource<pxr::HdRprimCollection>::New(collection))
          .SetRenderTags(
              pxr::HdRetainedTypedSampledDataSource<pxr::TfTokenVector>::New(render_tags))
          .Build();

  task_scene_index_->AddPrims({{task_id_,
                                pxr::HdPrimTypeTokens->task,
                                pxr::HdRetainedContainerDataSource::New(
                                    pxr::HdLegacyTaskSchema::GetSchemaToken(), task_ds)}});
}

void RenderTaskDelegate::dirty_task_params()
{
  task_scene_index_->DirtyPrims({{task_id_, pxr::HdLegacyTaskSchema::GetParametersLocator()}});
}

void RenderTaskDelegate::publish_buffer(pxr::SdfPath const &buf_id,
                                        pxr::HdRenderBufferDescriptor const &desc)
{
  pxr::HdContainerDataSourceHandle buffer_ds =
      pxr::HdRenderBufferSchema::Builder()
          .SetDimensions(pxr::HdRetainedTypedSampledDataSource<pxr::GfVec3i>::New(desc.dimensions))
          .SetFormat(pxr::HdRetainedTypedSampledDataSource<pxr::HdFormat>::New(desc.format))
          .SetMultiSampled(pxr::HdRetainedTypedSampledDataSource<bool>::New(desc.multiSampled))
          .Build();

  task_scene_index_->AddPrims({{buf_id,
                                pxr::HdPrimTypeTokens->renderBuffer,
                                pxr::HdRetainedContainerDataSource::New(
                                    pxr::HdRenderBufferSchema::GetSchemaToken(), buffer_ds)}});
}

pxr::HdTaskSharedPtr RenderTaskDelegate::task()
{
  return render_index_->GetTask(task_id_);
}

void RenderTaskDelegate::set_camera(pxr::SdfPath const &camera_id)
{
  if (task_params_.camera == camera_id) {
    return;
  }
  task_params_.camera = camera_id;
  dirty_task_params();
}

bool RenderTaskDelegate::is_converged()
{
  return static_cast<pxr::HdxRenderTask *>(task().get())->IsConverged();
}

void RenderTaskDelegate::set_viewport(pxr::GfVec4d const &viewport)
{
  if (task_params_.viewport == viewport) {
    return;
  }
  task_params_.viewport = viewport;
  dirty_task_params();

  int w = viewport[2] - viewport[0];
  int h = viewport[3] - viewport[1];
  for (auto &it : buffer_descriptors_) {
    it.second.dimensions = pxr::GfVec3i(w, h, 1);
    publish_buffer(it.first, it.second);
    task_scene_index_->DirtyPrims({{it.first, pxr::HdRenderBufferSchema::GetDimensionsLocator()}});
  }
}

void RenderTaskDelegate::add_aov(pxr::TfToken const &aov_key)
{
  pxr::SdfPath buf_id = buffer_id(aov_key);
  if (buffer_descriptors_.find(buf_id) != buffer_descriptors_.end()) {
    return;
  }
  pxr::HdAovDescriptor aov_desc = render_index_->GetRenderDelegate()->GetDefaultAovDescriptor(
      aov_key);

  if (aov_desc.format == pxr::HdFormatInvalid) {
    CLOG_ERROR(LOG_HYDRA_RENDER, "Invalid AOV: %s", aov_key.GetText());
    return;
  }
  if (!ELEM(
          pxr::HdGetComponentFormat(aov_desc.format), pxr::HdFormatFloat32, pxr::HdFormatFloat16))
  {
    CLOG_WARN(LOG_HYDRA_RENDER,
              "Unsupported data format %s for AOV %s",
              pxr::TfEnum::GetName(aov_desc.format).c_str(),
              aov_key.GetText());
    return;
  }

  int w = task_params_.viewport[2] - task_params_.viewport[0];
  int h = task_params_.viewport[3] - task_params_.viewport[1];
  pxr::HdRenderBufferDescriptor desc(
      pxr::GfVec3i(w, h, 1), aov_desc.format, aov_desc.multiSampled);
  buffer_descriptors_[buf_id] = desc;
  publish_buffer(buf_id, desc);

  pxr::HdRenderPassAovBinding binding;
  binding.aovName = aov_key;
  binding.renderBufferId = buf_id;
  binding.aovSettings = aov_desc.aovSettings;
  binding.clearValue = aov_desc.clearValue;
  task_params_.aovBindings.push_back(binding);
  dirty_task_params();

  CLOG_DEBUG(LOG_HYDRA_RENDER, "%s", aov_key.GetText());
}

void RenderTaskDelegate::read_aov(pxr::TfToken const &aov_key, void *data)
{
  pxr::HdRenderBuffer *buffer = static_cast<pxr::HdRenderBuffer *>(
      render_index_->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, buffer_id(aov_key)));
  if (!buffer) {
    return;
  }

  pxr::HdFormat format = buffer->GetFormat();
  size_t len = buffer->GetWidth() * buffer->GetHeight() * pxr::HdGetComponentCount(format);
  if (pxr::HdGetComponentFormat(format) == pxr::HdFormatFloat32) {
    void *buf_data = buffer->Map();
    memcpy(data, buf_data, len * sizeof(float));
    buffer->Unmap();
  }
  else if (pxr::HdGetComponentFormat(format) == pxr::HdFormatFloat16) {
    Eigen::half *buf_data = (Eigen::half *)buffer->Map();
    float *fdata = static_cast<float *>(data);
    for (size_t i = 0; i < len; ++i) {
      fdata[i] = buf_data[i];
    }
    buffer->Unmap();
  }
  else {
    BLI_assert_unreachable();
  }
}

pxr::HdRenderBuffer *RenderTaskDelegate::get_aov_buffer(pxr::TfToken const &aov_key)
{
  return (pxr::HdRenderBuffer *)render_index_->GetBprim(pxr::HdPrimTypeTokens->renderBuffer,
                                                        buffer_id(aov_key));
}

void RenderTaskDelegate::bind() {}

void RenderTaskDelegate::unbind() {}

pxr::SdfPath RenderTaskDelegate::buffer_id(pxr::TfToken const &aov_key) const
{
  return base_id_.AppendElementString("aov_" + aov_key.GetString());
}

GPURenderTaskDelegate::~GPURenderTaskDelegate()
{
  unbind();
  if (tex_color_) {
    GPU_texture_free(tex_color_);
  }
  if (tex_depth_) {
    GPU_texture_free(tex_depth_);
  }
}

void GPURenderTaskDelegate::set_viewport(pxr::GfVec4d const &viewport)
{
  if (task_params_.viewport == viewport) {
    return;
  }
  task_params_.viewport = viewport;
  dirty_task_params();

  if (tex_color_) {
    GPU_texture_free(tex_color_);
    tex_color_ = nullptr;
    add_aov(pxr::HdAovTokens->color);
  }
  if (tex_depth_) {
    GPU_texture_free(tex_depth_);
    tex_depth_ = nullptr;
    add_aov(pxr::HdAovTokens->depth);
  }
}

void GPURenderTaskDelegate::add_aov(pxr::TfToken const &aov_key)
{
  gpu::TextureFormat format;
  gpu::Texture **tex;
  if (aov_key == pxr::HdAovTokens->color) {
    format = gpu::TextureFormat::SFLOAT_32_32_32_32;
    tex = &tex_color_;
  }
  else if (aov_key == pxr::HdAovTokens->depth) {
    format = gpu::TextureFormat::SFLOAT_32_DEPTH;
    tex = &tex_depth_;
  }
  else {
    CLOG_ERROR(LOG_HYDRA_RENDER, "Invalid AOV: %s", aov_key.GetText());
    return;
  }

  if (*tex) {
    return;
  }

  *tex = GPU_texture_create_2d(("tex_render_hydra_" + aov_key.GetString()).c_str(),
                               task_params_.viewport[2] - task_params_.viewport[0],
                               task_params_.viewport[3] - task_params_.viewport[1],
                               1,
                               format,
                               GPU_TEXTURE_USAGE_GENERAL,
                               nullptr);

  CLOG_DEBUG(LOG_HYDRA_RENDER, "%s", aov_key.GetText());
}

void GPURenderTaskDelegate::read_aov(pxr::TfToken const &aov_key, void *data)
{
  gpu::Texture *tex = nullptr;
  int c;
  if (aov_key == pxr::HdAovTokens->color) {
    tex = tex_color_;
    c = 4;
  }
  else if (aov_key == pxr::HdAovTokens->depth) {
    tex = tex_depth_;
    c = 1;
  }
  if (!tex) {
    return;
  }

  int w = GPU_texture_width(tex), h = GPU_texture_height(tex);
  void *tex_data = GPU_texture_read(tex, GPU_DATA_FLOAT, 0);
  memcpy(data, tex_data, sizeof(float) * w * h * c);
  MEM_delete_void(tex_data);
}

void GPURenderTaskDelegate::bind()
{
  if (!framebuffer_) {
    framebuffer_ = GPU_framebuffer_create("fb_render_hydra");
  }
  GPU_framebuffer_ensure_config(
      &framebuffer_, {GPU_ATTACHMENT_TEXTURE(tex_depth_), GPU_ATTACHMENT_TEXTURE(tex_color_)});
  GPU_framebuffer_bind(framebuffer_);

  GPU_framebuffer_clear_color_depth(framebuffer_, {0.0, 0.0, 0.0, 0.0}, 1.0f);

#ifdef WITH_OPENGL_BACKEND
  /* Workaround missing/buggy VAOs in hgiGL and hdSt. For OpenGL compatibility
   * profile this is not a problem, but for core profile it is. */
  if (VAO_ == 0 && GPU_backend_get_type() == GPU_BACKEND_OPENGL) {
    glGenVertexArrays(1, &VAO_);
    glBindVertexArray(VAO_);
  }
#else
  UNUSED_VARS(VAO_);
#endif
  CLOG_DEBUG(LOG_HYDRA_RENDER, "bind");
}

void GPURenderTaskDelegate::unbind()
{
#ifdef WITH_OPENGL_BACKEND
  if (VAO_) {
    glDeleteVertexArrays(1, &VAO_);
    VAO_ = 0;
  }
#endif
  if (framebuffer_) {
    GPU_framebuffer_free(framebuffer_);
    framebuffer_ = nullptr;
  }
  CLOG_DEBUG(LOG_HYDRA_RENDER, "unbind");
}

gpu::Texture *GPURenderTaskDelegate::get_aov_texture(pxr::TfToken const &aov_key)
{
  if (aov_key == pxr::HdAovTokens->color) {
    return tex_color_;
  }
  if (aov_key == pxr::HdAovTokens->depth) {
    return tex_depth_;
  }
  return nullptr;
}

}  // namespace blender::render::hydra
