/* SPDX-FileCopyrightText: 2020-2023 Blender Foundation
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

HMODULE GHOST_ContextD3D::s_d3d_lib = NULL;
PFN_D3D11_CREATE_DEVICE GHOST_ContextD3D::s_D3D11CreateDeviceFn = NULL;

GHOST_ContextD3D::GHOST_ContextD3D(bool stereoVisual, HWND hWnd)
    : GHOST_Context(stereoVisual), m_hWnd(hWnd)
{
}

GHOST_ContextD3D::~GHOST_ContextD3D()
{
  m_device->Release();
  m_device_ctx->ClearState();
  m_device_ctx->Release();
}

GHOST_TSuccess GHOST_ContextD3D::swapBuffers()
{
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextD3D::activateDrawingContext()
{
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::releaseDrawingContext()
{
  return GHOST_kFailure;
}

GHOST_TSuccess GHOST_ContextD3D::setupD3DLib()
{
  if (s_d3d_lib == NULL) {
    s_d3d_lib = LoadLibraryA("d3d11.dll");

    WIN32_CHK(s_d3d_lib != NULL);

    if (s_d3d_lib == NULL) {
      fprintf(stderr, "LoadLibrary(\"d3d11.dll\") failed!\n");
      return GHOST_kFailure;
    }
  }

  if (s_D3D11CreateDeviceFn == NULL) {
    s_D3D11CreateDeviceFn = (PFN_D3D11_CREATE_DEVICE)GetProcAddress(s_d3d_lib,
                                                                    "D3D11CreateDevice");

    WIN32_CHK(s_D3D11CreateDeviceFn != NULL);

    if (s_D3D11CreateDeviceFn == NULL) {
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
      NULL,
      D3D_DRIVER_TYPE_HARDWARE,
      NULL,
      /* For debugging you may want to pass D3D11_CREATE_DEVICE_DEBUG here, but that requires
       * additional setup, see
       * https://docs.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-layers#debug-layer.
       */
      0,
      NULL,
      0,
      D3D11_SDK_VERSION,
      &m_device,
      NULL,
      &m_device_ctx);

  WIN32_CHK(hres == S_OK);

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
  } m_shared;

  enum RenderTarget { TARGET_RENDERBUF, TARGET_TEX2D };

 public:
  GHOST_SharedOpenGLResource(ID3D11Device *device,
                             ID3D11DeviceContext *device_ctx,
                             unsigned int width,
                             unsigned int height,
                             DXGI_FORMAT format,
                             ID3D11RenderTargetView *render_target = nullptr)
      : m_device(device), m_device_ctx(device_ctx), m_cur_width(width), m_cur_height(height)
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

      device->CreateTexture2D(&texDesc, NULL, &tex);
      if (!tex) {
        /* If texture creation fails, we just return and leave the render target unset. So it needs
         * to be NULL-checked before use. */
        fprintf(stderr, "Error creating texture for shared DirectX-OpenGL resource\n");
        return;
      }

      renderTargetViewDesc.Format = texDesc.Format;
      renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
      renderTargetViewDesc.Texture2D.MipSlice = 0;

      device->CreateRenderTargetView(tex, &renderTargetViewDesc, &render_target);

      tex->Release();
    }

    m_render_target = render_target;
    if (m_render_target) {
      ID3D11Resource *backbuffer_res = nullptr;
      m_render_target->GetResource(&backbuffer_res);
      if (backbuffer_res) {
        backbuffer_res->QueryInterface<ID3D11Texture2D>(&m_render_target_tex);
        backbuffer_res->Release();
      }
    }

    if (!m_render_target || !m_render_target_tex) {
      fprintf(stderr, "Error creating render target for shared DirectX-OpenGL resource\n");
      return;
    }
  }

  ~GHOST_SharedOpenGLResource()
  {
    if (m_render_target_tex) {
      m_render_target_tex->Release();
    }
    if (m_render_target) {
      m_render_target->Release();
    }

    if (m_is_initialized) {
#if 0 /* TODO: Causes an access violation since Blender 3.4 (a296b8f694d1). */
        if (m_shared.render_target
#  if 1
          /* TODO: #wglDXUnregisterObjectNV() causes an access violation on AMD when the shared
           * resource is a GL texture. Since there is currently no good alternative, just skip
           * unregistering the shared resource. */
          && !m_use_gl_texture2d
#  endif
      ) {
        wglDXUnregisterObjectNV(m_shared.device, m_shared.render_target);
      }
      if (m_shared.device) {
        wglDXCloseDeviceNV(m_shared.device);
      }
      glDeleteFramebuffers(1, &m_shared.fbo);
      if (m_use_gl_texture2d) {
        glDeleteTextures(1, &m_gl_render_target);
      }
      else {
        glDeleteRenderbuffers(1, &m_gl_render_target);
      }
#else
      glDeleteFramebuffers(1, &m_shared.fbo);
      if (m_use_gl_texture2d) {
        glDeleteTextures(1, &m_gl_render_target);
      }
#endif
    }
  }

  /* Returns true if the shared object was successfully registered, false otherwise. */
  bool reregisterSharedObject(RenderTarget target)
  {
    if (m_shared.render_target) {
      wglDXUnregisterObjectNV(m_shared.device, m_shared.render_target);
    }

    if (!m_render_target_tex) {
      return false;
    }

    if (target == TARGET_TEX2D) {
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   GL_RGBA8,
                   m_cur_width,
                   m_cur_height,
                   0,
                   GL_RGBA,
                   GL_UNSIGNED_BYTE,
                   nullptr);
    }

    m_shared.render_target = wglDXRegisterObjectNV(m_shared.device,
                                                   m_render_target_tex,
                                                   m_gl_render_target,
                                                   (target == TARGET_TEX2D) ? GL_TEXTURE_2D :
                                                                              GL_RENDERBUFFER,
                                                   WGL_ACCESS_READ_WRITE_NV);
    if (!m_shared.render_target) {
      fprintf(stderr, "Error registering shared object using wglDXRegisterObjectNV()\n");
      return false;
    }

    return true;
  }

  GHOST_TSuccess initialize()
  {
    m_shared.device = wglDXOpenDeviceNV(m_device);
    if (m_shared.device == NULL) {
      fprintf(stderr, "Error opening shared device using wglDXOpenDeviceNV()\n");
      return GHOST_kFailure;
    }

    /* Build the renderbuffer. */
    glGenRenderbuffers(1, &m_gl_render_target);
    glBindRenderbuffer(GL_RENDERBUFFER, m_gl_render_target);

    if (!reregisterSharedObject(TARGET_RENDERBUF)) {
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      if (m_gl_render_target) {
        glDeleteRenderbuffers(1, &m_gl_render_target);
      }
      /* Fall back to texture 2d. */
      m_use_gl_texture2d = true;
      glGenTextures(1, &m_gl_render_target);
      glBindTexture(GL_TEXTURE_2D, m_gl_render_target);

      reregisterSharedObject(TARGET_TEX2D);
    }

    /* Build the framebuffer */
    glGenFramebuffers(1, &m_shared.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shared.fbo);
    if (m_use_gl_texture2d) {
      glFramebufferTexture2D(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gl_render_target, 0);
    }
    else {
      glFramebufferRenderbuffer(
          GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_gl_render_target);
    }
    m_is_initialized = true;

    return GHOST_kSuccess;
  }

  void ensureUpdated(unsigned int width, unsigned int height)
  {
    if (m_is_initialized == false) {
      initialize();
    }

    if ((m_cur_width != width) || (m_cur_height != height)) {
      m_cur_width = width;
      m_cur_height = height;
      reregisterSharedObject(m_use_gl_texture2d ? TARGET_TEX2D : TARGET_RENDERBUF);
    }
  }

  GHOST_TSuccess blit(unsigned int width, unsigned int height)
  {
    GLint fbo;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);

    if (!m_render_target || !m_render_target_tex) {
      return GHOST_kFailure;
    }

    ensureUpdated(width, height);

#ifdef NDEBUG
    const float clear_col[] = {0.8f, 0.5f, 1.0f, 1.0f};
    m_device_ctx->ClearRenderTargetView(m_render_target, clear_col);
#endif
    m_device_ctx->OMSetRenderTargets(1, &m_render_target, nullptr);

    beginGLOnly();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_shared.fbo);
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

  ID3D11RenderTargetView *m_render_target{nullptr};
  ID3D11Texture2D *m_render_target_tex{nullptr};

 private:
  void beginGLOnly()
  {
    wglDXLockObjectsNV(m_shared.device, 1, &m_shared.render_target);
  }
  void endGLOnly()
  {
    wglDXUnlockObjectsNV(m_shared.device, 1, &m_shared.render_target);
  }

  ID3D11Device *m_device;
  ID3D11DeviceContext *m_device_ctx;
  GLuint m_gl_render_target;
  unsigned int m_cur_width, m_cur_height;
  bool m_is_initialized{false};
  bool m_use_gl_texture2d{false};
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
      m_device, m_device_ctx, width, height, format, render_target);

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
  return shared_res->m_render_target_tex;
}
