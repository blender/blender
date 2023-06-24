/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef _WIN32
// Include first to avoid "NOGDI" definition set in Cycles headers
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#endif

#include "hydra/display_driver.h"
#include "hydra/render_buffer.h"
#include "hydra/session.h"

#include <epoxy/gl.h>
#include <pxr/imaging/hgiGL/texture.h>

HDCYCLES_NAMESPACE_OPEN_SCOPE

HdCyclesDisplayDriver::HdCyclesDisplayDriver(HdCyclesSession *renderParam, Hgi *hgi)
    : _renderParam(renderParam), _hgi(hgi)
{
}

HdCyclesDisplayDriver::~HdCyclesDisplayDriver()
{
  if (texture_) {
    _hgi->DestroyTexture(&texture_);
  }

  if (gl_pbo_id_) {
    glDeleteBuffers(1, &gl_pbo_id_);
  }

  gl_context_dispose();
}

void HdCyclesDisplayDriver::gl_context_create()
{
#ifdef _WIN32
  if (!gl_context_) {
    hdc_ = GetDC(CreateWindowA("STATIC",
                               "HdCycles",
                               WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                               0,
                               0,
                               64,
                               64,
                               NULL,
                               NULL,
                               GetModuleHandle(NULL),
                               NULL));

    int pixelFormat = GetPixelFormat(wglGetCurrentDC());
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd)};
    DescribePixelFormat((HDC)hdc_, pixelFormat, sizeof(pfd), &pfd);
    SetPixelFormat((HDC)hdc_, pixelFormat, &pfd);

    TF_VERIFY(gl_context_ = wglCreateContext((HDC)hdc_));
    TF_VERIFY(wglShareLists(wglGetCurrentContext(), (HGLRC)gl_context_));
  }
  if (!gl_context_) {
    return;
  }
#endif

  if (!gl_pbo_id_) {
    glGenBuffers(1, &gl_pbo_id_);
  }
}

bool HdCyclesDisplayDriver::gl_context_enable()
{
#ifdef _WIN32
  if (!hdc_ || !gl_context_) {
    return false;
  }

  mutex_.lock();

  // Do not change context if this is called in the main thread
  if (wglGetCurrentContext() == nullptr) {
    if (!TF_VERIFY(wglMakeCurrent((HDC)hdc_, (HGLRC)gl_context_))) {
      mutex_.unlock();
      return false;
    }
  }

  return true;
#else
  return false;
#endif
}

void HdCyclesDisplayDriver::gl_context_disable()
{
#ifdef _WIN32
  if (wglGetCurrentContext() == gl_context_) {
    TF_VERIFY(wglMakeCurrent(nullptr, nullptr));
  }

  mutex_.unlock();
#endif
}

void HdCyclesDisplayDriver::gl_context_dispose()
{
#ifdef _WIN32
  if (gl_context_) {
    TF_VERIFY(wglDeleteContext((HGLRC)gl_context_));
    DestroyWindow(WindowFromDC((HDC)hdc_));
  }
#endif
}

void HdCyclesDisplayDriver::next_tile_begin() {}

bool HdCyclesDisplayDriver::update_begin(const Params &params,
                                         int texture_width,
                                         int texture_height)
{
  if (!gl_context_enable()) {
    return false;
  }

  if (gl_render_sync_) {
    glWaitSync((GLsync)gl_render_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  if (pbo_size_.x != params.full_size.x || pbo_size_.y != params.full_size.y) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_id_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER,
                 sizeof(half4) * params.full_size.x * params.full_size.y,
                 0,
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    pbo_size_ = params.full_size;
  }

  need_update_ = true;

  return true;
}

void HdCyclesDisplayDriver::update_end()
{
  gl_upload_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

  gl_context_disable();
}

void HdCyclesDisplayDriver::flush()
{
  gl_context_enable();

  if (gl_upload_sync_) {
    glWaitSync((GLsync)gl_upload_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  if (gl_render_sync_) {
    glWaitSync((GLsync)gl_render_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  gl_context_disable();
}

half4 *HdCyclesDisplayDriver::map_texture_buffer()
{
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_id_);

  const auto mapped_rgba_pixels = static_cast<half4 *>(
      glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));

  if (need_clear_ && mapped_rgba_pixels) {
    memset(mapped_rgba_pixels, 0, sizeof(half4) * pbo_size_.x * pbo_size_.y);
    need_clear_ = false;
  }

  return mapped_rgba_pixels;
}

void HdCyclesDisplayDriver::unmap_texture_buffer()
{
  glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

DisplayDriver::GraphicsInterop HdCyclesDisplayDriver::graphics_interop_get()
{
  GraphicsInterop interop_dst;
  interop_dst.buffer_width = pbo_size_.x;
  interop_dst.buffer_height = pbo_size_.y;
  interop_dst.opengl_pbo_id = gl_pbo_id_;

  interop_dst.need_clear = need_clear_;
  need_clear_ = false;

  return interop_dst;
}

void HdCyclesDisplayDriver::graphics_interop_activate()
{
  gl_context_enable();
}

void HdCyclesDisplayDriver::graphics_interop_deactivate()
{
  gl_context_disable();
}

void HdCyclesDisplayDriver::clear()
{
  need_clear_ = true;
}

void HdCyclesDisplayDriver::draw(const Params &params)
{
  const auto renderBuffer = static_cast<HdCyclesRenderBuffer *>(
      _renderParam->GetDisplayAovBinding().renderBuffer);
  if (!renderBuffer ||  // Ensure this render buffer matches the texture dimensions
      (renderBuffer->GetWidth() != params.size.x || renderBuffer->GetHeight() != params.size.y))
  {
    return;
  }

  if (!renderBuffer->IsResourceUsed()) {
    return;
  }

  gl_context_create();

  // Cycles 'DisplayDriver' only supports 'half4' format
  TF_VERIFY(renderBuffer->GetFormat() == HdFormatFloat16Vec4);

  const thread_scoped_lock lock(mutex_);

  const GfVec3i dimensions(params.size.x, params.size.y, 1);
  if (!texture_ || texture_->GetDescriptor().dimensions != dimensions) {
    if (texture_) {
      _hgi->DestroyTexture(&texture_);
    }

    HgiTextureDesc texDesc;
    texDesc.usage = 0;
    texDesc.format = HgiFormatFloat16Vec4;
    texDesc.type = HgiTextureType2D;
    texDesc.dimensions = dimensions;
    texDesc.sampleCount = HgiSampleCount1;

    texture_ = _hgi->CreateTexture(texDesc);

    renderBuffer->SetResource(VtValue(texture_));
  }

  HgiGLTexture *const texture = dynamic_cast<HgiGLTexture *>(texture_.Get());
  if (!texture || !need_update_ || pbo_size_.x != params.size.x || pbo_size_.y != params.size.y) {
    return;
  }

  if (gl_upload_sync_) {
    glWaitSync((GLsync)gl_upload_sync_, 0, GL_TIMEOUT_IGNORED);
  }

  glBindTexture(GL_TEXTURE_2D, texture->GetTextureId());
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_id_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pbo_size_.x, pbo_size_.y, GL_RGBA, GL_HALF_FLOAT, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  gl_render_sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();

  need_update_ = false;
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
