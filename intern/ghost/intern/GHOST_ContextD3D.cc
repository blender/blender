/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * For testing purposes, it can be useful to do some DirectX only drawing. A patch for this can be
 * found here: https://developer.blender.org/P1284
 */

#include <iostream>
#include <string>

#include <epoxy/wgl.h>

#include "GHOST_ContextD3D.hh"
#include "GHOST_ContextWGL.hh" /* For shared drawing */

HMODULE GHOST_ContextD3D::s_d3d_lib = nullptr;
PFN_D3D11_CREATE_DEVICE GHOST_ContextD3D::s_D3D11CreateDeviceFn = nullptr;

GHOST_ContextD3D::GHOST_ContextD3D(const GHOST_ContextParams &context_params, HWND hWnd)
    : GHOST_Context(context_params), h_wnd_(hWnd)
{
}

GHOST_ContextD3D::~GHOST_ContextD3D()
{
  device_->Release();
  device_ctx_->ClearState();
  device_ctx_->Release();
}

GHOST_TSuccess GHOST_ContextD3D::swapBufferRelease()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::activateDrawingContext()
{
  active_context_ = this;
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::releaseDrawingContext()
{
  active_context_ = nullptr;
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::setupD3DLib()
{
  if (s_d3d_lib == nullptr) {
    s_d3d_lib = LoadLibraryA("d3d11.dll");

    WIN32_CHK(s_d3d_lib != nullptr);

    if (s_d3d_lib == nullptr) {
      fprintf(stderr, "LoadLibrary(\"d3d11.dll\") failed!\n");
      return GHOST_kFailure;
    }
  }

  if (s_D3D11CreateDeviceFn == nullptr) {
    s_D3D11CreateDeviceFn = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(s_d3d_lib,
                                                                    "D3D11CreateDevice");

    WIN32_CHK(s_D3D11CreateDeviceFn != nullptr);

    if (s_D3D11CreateDeviceFn == nullptr) {
      fprintf(stderr, "GetProcAddress(s_d3d_lib, \"D3D11CreateDevice\") failed!\n");
      return GHOST_kFailure;
    }
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::initializeDrawingContext()
{
  if (setupD3DLib() == GHOST_kFailure) {
    return GHOST_kFailure;
  }

  HRESULT hres = s_D3D11CreateDeviceFn(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      /* For debugging you may want to pass D3D11_CREATE_DEVICE_DEBUG here, but that requires
       * additional setup, see
       * https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-layers#debug-layer.
       */
      0,
      nullptr,
      0,
      D3D11_SDK_VERSION,
      &device_,
      nullptr,
      &device_ctx_);

  WIN32_CHK(hres == S_OK);

  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::releaseNativeHandles()
{
  return GHOST_kFailure;
}

class GHOST_SharedOpenGLResource {
  struct SharedData {
    HANDLE device;
    GLuint fbo;
    HANDLE render_target{nullptr};
  } shared_;

  enum RenderTarget { TARGET_RENDERBUF, TARGET_TEX2D };

 public:
  GHOST_SharedOpenGLResource(ID3D11Device *device,
                             ID3D11DeviceContext *device_ctx,
                             unsigned int width,
                             unsigned int height,
                             DXGI_FORMAT format,
                             ID3D11RenderTargetView *render_target = nullptr)
      : device_(device), device_ctx_(device_ctx), cur_width_(width), cur_height_(height)
  {
    if (!render_target) {
      D3D11_TEXTURE2D_DESC texDesc{};
      D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
      ID3D11Texture2D *tex;

      texDesc.Width = width;
      texDesc.Height = height;
      texDesc.Format = format;
      texDesc.SampleDesc.Count = 1;
      texDesc.ArraySize = 1;
      texDesc.MipLevels = 1;
      texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

      device->CreateTexture2D(&texDesc, nullptr, &tex);
      if (!tex) {
        /* If texture creation fails, we just return and leave the render target unset. So it needs
         * to be nullptr-checked before use. */
        fprintf(stderr, "Error creating texture for shared DirectX-OpenGL resource\n");
        return;
      }

      renderTargetViewDesc.Format = texDesc.Format;
      renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      renderTargetViewDesc.Texture2D.MipSlice = 0;

      device->CreateRenderTargetView(tex, &renderTargetViewDesc, &render_target);

      tex->Release();
    }

    render_target_ = render_target;
    if (render_target_) {
      ID3D11Resource *backbuffer_res = nullptr;
      render_target_->GetResource(&backbuffer_res);
      if (backbuffer_res) {
        backbuffer_res->QueryInterface<ID3D11Texture2D>(&render_target_tex_);
        backbuffer_res->Release();
      }
    }

    if (!render_target_ || !render_target_tex_) {
      fprintf(stderr, "Error creating render target for shared DirectX-OpenGL resource\n");
      return;
    }
  }

  ~GHOST_SharedOpenGLResource()
  {
    if (render_target_tex_) {
      render_target_tex_->Release();
    }
    if (render_target_) {
      render_target_->Release();
    }

    if (is_initialized_) {
#if 0 /* TODO: Causes an access violation since Blender 3.4 (a296b8f694d1). */
        if (shared_.render_target
#  if 1
          /* TODO: #wglDXUnregisterObjectNV() causes an access violation on AMD when the shared
           * resource is a GL texture. Since there is currently no good alternative, just skip
           * unregistering the shared resource. */
          && !use_gl_texture2d_
#  endif
      ) {
        wglDXUnregisterObjectNV(shared_.device, shared_.render_target);
      }
      if (shared_.device) {
        wglDXCloseDeviceNV(shared_.device);
      }
      glDeleteFramebuffers(1, &shared_.fbo);
      if (use_gl_texture2d_) {
        glDeleteTextures(1, &gl_render_target_);
      }
      else {
        glDeleteRenderbuffers(1, &gl_render_target_);
      }
#else
      glDeleteFramebuffers(1, &shared_.fbo);
      if (use_gl_texture2d_) {
        glDeleteTextures(1, &gl_render_target_);
      }
#endif
    }
  }

  /* Returns true if the shared object was successfully registered, false otherwise. */
  bool reregisterSharedObject(RenderTarget target)
  {
    if (shared_.render_target) {
      wglDXUnregisterObjectNV(shared_.device, shared_.render_target);
    }

    if (!render_target_tex_) {
      return false;
    }

    if (target == TARGET_TEX2D) {
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   cur_width_,
                   cur_height_,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   nullptr);
    }

    shared_.render_target = wglDXRegisterObjectNV(shared_.device,
                                                  render_target_tex_,
                                                  gl_render_target_,
                                                  (target == TARGET_TEX2D) ? GL_TEXTURE_2D :
                                                                             GL_RENDERBUFFER,
                                                  WGL_ACCESS_READ_WRITE_NV);
    if (!shared_.render_target) {
      fprintf(stderr, "Error registering shared object using wglDXRegisterObjectNV()\n");
      return false;
    }

    return true;
  }

  GHOST_TSuccess initialize()
  {
    shared_.device = wglDXOpenDeviceNV(device_);
    if (shared_.device == nullptr) {
      fprintf(stderr, "Error opening shared device using wglDXOpenDeviceNV()\n");
      return GHOST_kFailure;
    }

    /* Build the render-buffer. */
    glGenRenderbuffers(1, &gl_render_target_);
    glBindRenderbuffer(GL_RENDERBUFFER, gl_render_target_);

    if (!reregisterSharedObject(TARGET_RENDERBUF)) {
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      if (gl_render_target_) {
        glDeleteRenderbuffers(1, &gl_render_target_);
      }
      /* Fall back to texture 2d. */
      use_gl_texture2d_ = true;
      glGenTextures(1, &gl_render_target_);
      glBindTexture(GL_TEXTURE_2D, gl_render_target_);

      reregisterSharedObject(TARGET_TEX2D);
    }

    /* Build the frame-buffer. */
    glGenFramebuffers(1, &shared_.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shared_.fbo);
    if (use_gl_texture2d_) {
      glFramebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl_render_target_, 0);
    }
    else {
      glFramebufferRenderbuffer(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl_render_target_);
    }
    is_initialized_ = true;

    return GHOST_kSuccess;
  }

  void ensureUpdated(unsigned int width, unsigned int height)
  {
    if (is_initialized_ == false) {
      initialize();
    }

    if ((cur_width_ != width) || (cur_height_ != height)) {
      cur_width_ = width;
      cur_height_ = height;
      reregisterSharedObject(use_gl_texture2d_ ? TARGET_TEX2D : TARGET_RENDERBUF);
    }
  }

  GHOST_TSuccess blit(unsigned int width, unsigned int height)
  {
    GLint fbo;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

    if (!render_target_ || !render_target_tex_) {
      return GHOST_kFailure;
    }

    ensureUpdated(width, height);

#ifdef NDEBUG
    const float clear_col[] = {0.8f, 0.5f, 1.0f, 1.0f};
    device_ctx_->ClearRenderTargetView(render_target_, clear_col);
#endif
    device_ctx_->OMSetRenderTargets(1, &render_target_, nullptr);

    beginGLOnly();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, shared_.fbo);
    GLenum err = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (err != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(
          stderr, "Error: Framebuffer for shared DirectX-OpenGL resource incomplete %u\n", err);
      return GHOST_kFailure;
    }

    /* No glBlitNamedFramebuffer, gotta be 3.3 compatible. */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    endGLOnly();

    return GHOST_kSuccess;
  }

  ID3D11RenderTargetView *render_target_{nullptr};
  ID3D11Texture2D *render_target_tex_{nullptr};

 private:
  void beginGLOnly()
  {
    wglDXLockObjectsNV(shared_.device, 1, &shared_.render_target);
  }
  void endGLOnly()
  {
    wglDXUnlockObjectsNV(shared_.device, 1, &shared_.render_target);
  }

  ID3D11Device *device_;
  ID3D11DeviceContext *device_ctx_;
  GLuint gl_render_target_;
  unsigned int cur_width_, cur_height_;
  bool is_initialized_{false};
  bool use_gl_texture2d_{false};
};

GHOST_SharedOpenGLResource *GHOST_ContextD3D::createSharedOpenGLResource(
    unsigned int width,
    unsigned int height,
    DXGI_FORMAT format,
    ID3D11RenderTargetView *render_target)
{
  if (!(WGL_NV_DX_interop && WGL_NV_DX_interop2)) {
    fprintf(stderr,
            "Error: Can't render OpenGL framebuffer using Direct3D. NV_DX_interop extension not "
            "available.");
    return nullptr;
  }
  GHOST_SharedOpenGLResource *shared_res = new GHOST_SharedOpenGLResource(
      device_, device_ctx_, width, height, format, render_target);

  return shared_res;
}
GHOST_SharedOpenGLResource *GHOST_ContextD3D::createSharedOpenGLResource(unsigned int width,
                                                                         unsigned int height,
                                                                         DXGI_FORMAT format)
{
  return createSharedOpenGLResource(width, height, format, nullptr);
}

void GHOST_ContextD3D::disposeSharedOpenGLResource(GHOST_SharedOpenGLResource *shared_res)
{
  delete shared_res;
}

GHOST_TSuccess GHOST_ContextD3D::blitFromOpenGLContext(GHOST_SharedOpenGLResource *shared_res,
                                                       unsigned int width,
                                                       unsigned int height)
{
  return shared_res->blit(width, height);
}

ID3D11Texture2D *GHOST_ContextD3D::getSharedTexture2D(GHOST_SharedOpenGLResource *shared_res)
{
  return shared_res->render_target_tex_;
}
