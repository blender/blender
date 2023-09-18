/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <algorithm>
#include <list>
#include <sstream>

#if defined(WITH_GHOST_X11)
#  include "GHOST_ContextEGL.hh"
#  include "GHOST_ContextGLX.hh"
#  include "GHOST_SystemX11.hh"
#endif
#if defined(WITH_GHOST_WAYLAND)
#  include "GHOST_ContextEGL.hh"
#  include "GHOST_SystemWayland.hh"
#endif
#if defined(WIN32)
#  include "GHOST_ContextD3D.hh"
#  include "GHOST_ContextWGL.hh"
#  include "GHOST_SystemWin32.hh"
#endif
#include "GHOST_C-api.h"
#include "GHOST_XrException.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_IXrGraphicsBinding.hh"

static std::optional<int64_t> choose_swapchain_format_from_candidates(
    const std::vector<int64_t> &gpu_binding_formats, const std::vector<int64_t> &runtime_formats)
{
  if (gpu_binding_formats.empty()) {
    return std::nullopt;
  }

  auto res = std::find_first_of(gpu_binding_formats.begin(),
                                gpu_binding_formats.end(),
                                runtime_formats.begin(),
                                runtime_formats.end());
  if (res == gpu_binding_formats.end()) {
    return std::nullopt;
  }

  return *res;
}

class GHOST_XrGraphicsBindingOpenGL : public GHOST_IXrGraphicsBinding {
 public:
  ~GHOST_XrGraphicsBindingOpenGL()
  {
    if (m_fbo != 0) {
      glDeleteFramebuffers(1, &m_fbo);
    }
  }

  bool checkVersionRequirements(GHOST_Context &ghost_ctx,
                                XrInstance instance,
                                XrSystemId system_id,
                                std::string *r_requirement_info) const override
  {
    int gl_major_version, gl_minor_version;
#if defined(WIN32)
    GHOST_ContextWGL &ctx_gl = static_cast<GHOST_ContextWGL &>(ghost_ctx);
    gl_major_version = ctx_gl.m_contextMajorVersion;
    gl_minor_version = ctx_gl.m_contextMinorVersion;
#elif defined(WITH_GHOST_X11) || defined(WITH_GHOST_WAYLAND)
    if (dynamic_cast<GHOST_ContextEGL *>(&ghost_ctx)) {
      GHOST_ContextEGL &ctx_gl = static_cast<GHOST_ContextEGL &>(ghost_ctx);
      gl_major_version = ctx_gl.m_contextMajorVersion;
      gl_minor_version = ctx_gl.m_contextMinorVersion;
    }
#  if defined(WITH_GHOST_X11)
    else {
      GHOST_ContextGLX &ctx_gl = static_cast<GHOST_ContextGLX &>(ghost_ctx);
      gl_major_version = ctx_gl.m_contextMajorVersion;
      gl_minor_version = ctx_gl.m_contextMinorVersion;
    }
#  endif
#endif
    static PFN_xrGetOpenGLGraphicsRequirementsKHR s_xrGetOpenGLGraphicsRequirementsKHR_fn =
        nullptr;
    // static XrInstance s_instance = XR_NULL_HANDLE;
    XrGraphicsRequirementsOpenGLKHR gpu_requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    const XrVersion gl_version = XR_MAKE_VERSION(gl_major_version, gl_minor_version, 0);

    /* Although it would seem reasonable that the PROC address would not change if the instance was
     * the same, in testing, repeated calls to #xrGetInstanceProcAddress() with the same instance
     * can still result in changes so the workaround is to simply set the function pointer every
     * time (trivializing its 'static' designation). */
    // if (instance != s_instance) {
    // s_instance = instance;
    s_xrGetOpenGLGraphicsRequirementsKHR_fn = nullptr;
    //}
    if (!s_xrGetOpenGLGraphicsRequirementsKHR_fn &&
        XR_FAILED(
            xrGetInstanceProcAddr(instance,
                                  "xrGetOpenGLGraphicsRequirementsKHR",
                                  (PFN_xrVoidFunction *)&s_xrGetOpenGLGraphicsRequirementsKHR_fn)))
    {
      s_xrGetOpenGLGraphicsRequirementsKHR_fn = nullptr;
      return false;
    }

    s_xrGetOpenGLGraphicsRequirementsKHR_fn(instance, system_id, &gpu_requirements);

    if (r_requirement_info) {
      std::ostringstream strstream;
      strstream << "Min OpenGL version "
                << XR_VERSION_MAJOR(gpu_requirements.minApiVersionSupported) << "."
                << XR_VERSION_MINOR(gpu_requirements.minApiVersionSupported) << std::endl;
      strstream << "Max OpenGL version "
                << XR_VERSION_MAJOR(gpu_requirements.maxApiVersionSupported) << "."
                << XR_VERSION_MINOR(gpu_requirements.maxApiVersionSupported) << std::endl;

      *r_requirement_info = strstream.str();
    }

    return (gl_version >= gpu_requirements.minApiVersionSupported) &&
           (gl_version <= gpu_requirements.maxApiVersionSupported);
  }

  void initFromGhostContext(GHOST_Context &ghost_ctx) override
  {
#if defined(WITH_GHOST_X11) || defined(WITH_GHOST_WAYLAND)
    /* WAYLAND/X11 may be dynamically selected at load time but both may also be
     * supported at compile time individually.
     * Without `is_ctx_egl` & `is_wayland` preprocessor checks become an unmanageable soup. */
    const bool is_ctx_egl = dynamic_cast<GHOST_ContextEGL *>(&ghost_ctx) != nullptr;
    if (is_ctx_egl) {
      GHOST_ContextEGL &ctx_egl = static_cast<GHOST_ContextEGL &>(ghost_ctx);
      const bool is_wayland = (
#  if defined(WITH_GHOST_WAYLAND)
          dynamic_cast<const GHOST_SystemWayland *const>(ctx_egl.m_system) != nullptr
#  else
          false
#  endif
      );

      if (is_wayland) {
#  if defined(WITH_GHOST_WAYLAND)
        /* #GHOST_SystemWayland */
        oxr_binding.wl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR;
        oxr_binding.wl.display = (wl_display *)ctx_egl.m_nativeDisplay;
#  else
        GHOST_ASSERT(false, "Unexpected State: logical error, unreachable!");
#  endif /* !WITH_GHOST_WAYLAND */
      }
      else { /* `!is_wayland` */
#  if defined(WITH_GHOST_X11)
        /* #GHOST_SystemX11. */
        oxr_binding.egl.type = XR_TYPE_GRAPHICS_BINDING_EGL_MNDX;
#    if XR_CURRENT_API_VERSION >= XR_MAKE_VERSION(1, 0, 29)
        oxr_binding.egl.getProcAddress = reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(
            eglGetProcAddress);
#    else
        oxr_binding.egl.getProcAddress = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(
            eglGetProcAddress);
#    endif
        oxr_binding.egl.display = ctx_egl.getDisplay();
        oxr_binding.egl.config = ctx_egl.getConfig();
        oxr_binding.egl.context = ctx_egl.getContext();
#  else
        GHOST_ASSERT(false, "Unexpected State: built with only WAYLAND and no System found!");
#  endif /* !WITH_GHOST_X11 */
      }
    }
    else { /* `!is_ctx_egl` */
#  if defined(WITH_GHOST_X11)
      GHOST_ContextGLX &ctx_glx = static_cast<GHOST_ContextGLX &>(ghost_ctx);
      XVisualInfo *visual_info = glXGetVisualFromFBConfig(ctx_glx.m_display, ctx_glx.m_fbconfig);

      oxr_binding.glx.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
      oxr_binding.glx.xDisplay = ctx_glx.m_display;
      oxr_binding.glx.glxFBConfig = ctx_glx.m_fbconfig;
      oxr_binding.glx.glxDrawable = ctx_glx.m_window;
      oxr_binding.glx.glxContext = ctx_glx.m_context;
      oxr_binding.glx.visualid = visual_info->visualid;

      XFree(visual_info);
#  else
      GHOST_ASSERT(false, "Unexpected State: built without X11 and no EGL context is available!");
#  endif /* !WITH_GHOST_X11 */
    }
#elif defined(WIN32)
    GHOST_ContextWGL &ctx_wgl = static_cast<GHOST_ContextWGL &>(ghost_ctx);

    oxr_binding.wgl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
    oxr_binding.wgl.hDC = ctx_wgl.m_hDC;
    oxr_binding.wgl.hGLRC = ctx_wgl.m_hGLRC;
#endif /* WIN32 */

    /* Generate a frame-buffer to use for blitting into the texture. */
    glGenFramebuffers(1, &m_fbo);
  }

  std::optional<int64_t> chooseSwapchainFormat(const std::vector<int64_t> &runtime_formats,
                                               GHOST_TXrSwapchainFormat &r_format,
                                               bool &r_is_srgb_format) const override
  {
    std::vector<int64_t> gpu_binding_formats = {
#if 0 /* RGB10A2, RGBA16 don't seem to work with Oculus head-sets, \
       * so move them after RGBA16F for the time being. */
        GL_RGB10_A2,
        GL_RGBA16,
#endif
      GL_RGBA16F,
#if 1
      GL_RGB10_A2,
      GL_RGBA16,
#endif
      GL_RGBA8,
      GL_SRGB8_ALPHA8,
    };

    std::optional result = choose_swapchain_format_from_candidates(gpu_binding_formats,
                                                                   runtime_formats);
    if (result) {
      switch (*result) {
        case GL_RGB10_A2:
          r_format = GHOST_kXrSwapchainFormatRGB10_A2;
          break;
        case GL_RGBA16:
          r_format = GHOST_kXrSwapchainFormatRGBA16;
          break;
        case GL_RGBA16F:
          r_format = GHOST_kXrSwapchainFormatRGBA16F;
          break;
        case GL_RGBA8:
        case GL_SRGB8_ALPHA8:
          r_format = GHOST_kXrSwapchainFormatRGBA8;
          break;
      }
      r_is_srgb_format = (*result == GL_SRGB8_ALPHA8);
    }
    else {
      r_format = GHOST_kXrSwapchainFormatRGBA8;
      r_is_srgb_format = false;
    }

    return result;
  }

  std::vector<XrSwapchainImageBaseHeader *> createSwapchainImages(uint32_t image_count) override
  {
    std::vector<XrSwapchainImageOpenGLKHR> ogl_images(image_count);
    std::vector<XrSwapchainImageBaseHeader *> base_images;

    /* Need to return vector of base header pointers, so of a different type. Need to build a new
     * list with this type, and keep the initial one alive. */
    for (XrSwapchainImageOpenGLKHR &image : ogl_images) {
      image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
      base_images.push_back(reinterpret_cast<XrSwapchainImageBaseHeader *>(&image));
    }

    /* Keep alive. */
    m_image_cache.push_back(std::move(ogl_images));

    return base_images;
  }

  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override
  {
    XrSwapchainImageOpenGLKHR &ogl_swapchain_image = reinterpret_cast<XrSwapchainImageOpenGLKHR &>(
        swapchain_image);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    glFramebufferTexture2D(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ogl_swapchain_image.image, 0);

    glBlitFramebuffer(draw_info.ofsx,
                      draw_info.ofsy,
                      draw_info.ofsx + draw_info.width,
                      draw_info.ofsy + draw_info.height,
                      draw_info.ofsx,
                      draw_info.ofsy,
                      draw_info.ofsx + draw_info.width,
                      draw_info.ofsy + draw_info.height,
                      GL_COLOR_BUFFER_BIT,
                      GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  bool needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const override
  {
    return ghost_ctx.isUpsideDown();
  }

 private:
  std::list<std::vector<XrSwapchainImageOpenGLKHR>> m_image_cache;
  GLuint m_fbo = 0;
};

#ifdef WIN32
static void ghost_format_to_dx_format(GHOST_TXrSwapchainFormat ghost_format,
                                      bool expects_srgb_buffer,
                                      DXGI_FORMAT &r_dx_format)
{
  r_dx_format = DXGI_FORMAT_UNKNOWN;

  switch (ghost_format) {
    case GHOST_kXrSwapchainFormatRGBA8:
      r_dx_format = expects_srgb_buffer ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB :
                                          DXGI_FORMAT_R8G8B8A8_UNORM;
      break;
    case GHOST_kXrSwapchainFormatRGBA16:
      r_dx_format = DXGI_FORMAT_R16G16B16A16_UNORM;
      break;
    case GHOST_kXrSwapchainFormatRGBA16F:
      r_dx_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      break;
    case GHOST_kXrSwapchainFormatRGB10_A2:
      r_dx_format = DXGI_FORMAT_R10G10B10A2_UNORM;
      break;
  }

  if (r_dx_format == DXGI_FORMAT_UNKNOWN) {
    throw GHOST_XrException("No supported DirectX swapchain format found.");
  }
}

class GHOST_XrGraphicsBindingD3D : public GHOST_IXrGraphicsBinding {
 public:
  GHOST_XrGraphicsBindingD3D(GHOST_Context &ghost_ctx)
      : GHOST_IXrGraphicsBinding(), m_ghost_wgl_ctx(static_cast<GHOST_ContextWGL &>(ghost_ctx))
  {
    m_ghost_d3d_ctx = GHOST_SystemWin32::createOffscreenContextD3D();
  }
  ~GHOST_XrGraphicsBindingD3D()
  {
    if (m_shared_resource) {
      m_ghost_d3d_ctx->disposeSharedOpenGLResource(m_shared_resource);
    }
    if (m_ghost_d3d_ctx) {
      GHOST_SystemWin32::disposeContextD3D(m_ghost_d3d_ctx);
    }
  }

  bool checkVersionRequirements(
      GHOST_Context & /*ghost_ctx*/, /* Remember: This is the OpenGL context! */
      XrInstance instance,
      XrSystemId system_id,
      std::string *r_requirement_info) const override
  {
    static PFN_xrGetD3D11GraphicsRequirementsKHR s_xrGetD3D11GraphicsRequirementsKHR_fn = nullptr;
    // static XrInstance s_instance = XR_NULL_HANDLE;
    XrGraphicsRequirementsD3D11KHR gpu_requirements = {XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR};

    /* Although it would seem reasonable that the PROC address would not change if the instance was
     * the same, in testing, repeated calls to #xrGetInstanceProcAddress() with the same instance
     * can still result in changes so the workaround is to simply set the function pointer every
     * time (trivializing its 'static' designation). */
    // if (instance != s_instance) {
    // s_instance = instance;
    s_xrGetD3D11GraphicsRequirementsKHR_fn = nullptr;
    //}
    if (!s_xrGetD3D11GraphicsRequirementsKHR_fn &&
        XR_FAILED(
            xrGetInstanceProcAddr(instance,
                                  "xrGetD3D11GraphicsRequirementsKHR",
                                  (PFN_xrVoidFunction *)&s_xrGetD3D11GraphicsRequirementsKHR_fn)))
    {
      s_xrGetD3D11GraphicsRequirementsKHR_fn = nullptr;
      return false;
    }

    s_xrGetD3D11GraphicsRequirementsKHR_fn(instance, system_id, &gpu_requirements);

    if (r_requirement_info) {
      std::ostringstream strstream;
      strstream << "Minimum DirectX 11 Feature Level " << gpu_requirements.minFeatureLevel
                << std::endl;

      *r_requirement_info = std::move(strstream.str());
    }

    return m_ghost_d3d_ctx->m_device->GetFeatureLevel() >= gpu_requirements.minFeatureLevel;
  }

  void initFromGhostContext(
      GHOST_Context & /*ghost_ctx*/ /* Remember: This is the OpenGL context! */
      ) override
  {
    oxr_binding.d3d11.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
    oxr_binding.d3d11.device = m_ghost_d3d_ctx->m_device;
  }

  std::optional<int64_t> chooseSwapchainFormat(const std::vector<int64_t> &runtime_formats,
                                               GHOST_TXrSwapchainFormat &r_format,
                                               bool &r_is_srgb_format) const override
  {
    std::vector<int64_t> gpu_binding_formats = {
#  if 0 /* RGB10A2, RGBA16 don't seem to work with Oculus head-sets, \
         * so move them after RGBA16F for the time being. */
        DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R16G16B16A16_UNORM,
#  endif
      DXGI_FORMAT_R16G16B16A16_FLOAT,
#  if 1
      DXGI_FORMAT_R10G10B10A2_UNORM,
      DXGI_FORMAT_R16G16B16A16_UNORM,
#  endif
      DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
    };

    std::optional result = choose_swapchain_format_from_candidates(gpu_binding_formats,
                                                                   runtime_formats);
    if (result) {
      switch (*result) {
        case DXGI_FORMAT_R10G10B10A2_UNORM:
          r_format = GHOST_kXrSwapchainFormatRGB10_A2;
          break;
        case DXGI_FORMAT_R16G16B16A16_UNORM:
          r_format = GHOST_kXrSwapchainFormatRGBA16;
          break;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
          r_format = GHOST_kXrSwapchainFormatRGBA16F;
          break;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
          r_format = GHOST_kXrSwapchainFormatRGBA8;
          break;
      }
      r_is_srgb_format = (*result == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
    }
    else {
      r_format = GHOST_kXrSwapchainFormatRGBA8;
      r_is_srgb_format = false;
    }

    return result;
  }

  std::vector<XrSwapchainImageBaseHeader *> createSwapchainImages(uint32_t image_count) override
  {
    std::vector<XrSwapchainImageD3D11KHR> d3d_images(image_count);
    std::vector<XrSwapchainImageBaseHeader *> base_images;

    /* Need to return vector of base header pointers, so of a different type. Need to build a new
     * list with this type, and keep the initial one alive. */
    for (XrSwapchainImageD3D11KHR &image : d3d_images) {
      image.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
      base_images.push_back(reinterpret_cast<XrSwapchainImageBaseHeader *>(&image));
    }

    /* Keep alive. */
    m_image_cache.push_back(std::move(d3d_images));

    return base_images;
  }

  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override
  {
    XrSwapchainImageD3D11KHR &d3d_swapchain_image = reinterpret_cast<XrSwapchainImageD3D11KHR &>(
        swapchain_image);

#  if 0
    /* Ideally we'd just create a render target view for the OpenXR swap-chain image texture and
     * blit from the OpenGL context into it. The NV_DX_interop extension doesn't want to work with
     * this though. At least not with Optimus hardware. See:
     * https://github.com/mpv-player/mpv/issues/2949#issuecomment-197262807.
     */

    ID3D11RenderTargetView *rtv;
    CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D,
                                            DXGI_FORMAT_R8G8B8A8_UNORM);

    m_ghost_ctx->m_device->CreateRenderTargetView(d3d_swapchain_image.texture, &rtv_desc, &rtv);
    if (!m_shared_resource) {
      DXGI_FORMAT format;
      ghost_format_to_dx_format(draw_info.swapchain_format, draw_info.expects_srgb_buffer, format);
      m_shared_resource = m_ghost_ctx->createSharedOpenGLResource(
          draw_info.width, draw_info.height, format, rtv);
    }
    m_ghost_ctx->blitFromOpenGLContext(m_shared_resource, draw_info.width, draw_info.height);
#  else
    if (!m_shared_resource) {
      DXGI_FORMAT format;
      ghost_format_to_dx_format(draw_info.swapchain_format, draw_info.expects_srgb_buffer, format);
      m_shared_resource = m_ghost_d3d_ctx->createSharedOpenGLResource(
          draw_info.width, draw_info.height, format);
    }
    m_ghost_d3d_ctx->blitFromOpenGLContext(m_shared_resource, draw_info.width, draw_info.height);

    m_ghost_d3d_ctx->m_device_ctx->OMSetRenderTargets(0, nullptr, nullptr);
    m_ghost_d3d_ctx->m_device_ctx->CopyResource(
        d3d_swapchain_image.texture, m_ghost_d3d_ctx->getSharedTexture2D(m_shared_resource));
#  endif
  }

  bool needsUpsideDownDrawing(GHOST_Context &) const
  {
    return m_ghost_d3d_ctx->isUpsideDown();
  }

 private:
  /** Primary OpenGL context for Blender to use for drawing. */
  GHOST_ContextWGL &m_ghost_wgl_ctx;
  /** Secondary DirectX 11 context to share with OpenGL context. */
  GHOST_ContextD3D *m_ghost_d3d_ctx = nullptr;
  /** Handle to shared resource object. */
  GHOST_SharedOpenGLResource *m_shared_resource = nullptr;

  std::list<std::vector<XrSwapchainImageD3D11KHR>> m_image_cache;
};
#endif  // WIN32

std::unique_ptr<GHOST_IXrGraphicsBinding> GHOST_XrGraphicsBindingCreateFromType(
    GHOST_TXrGraphicsBinding type, GHOST_Context &context)
{
  switch (type) {
    case GHOST_kXrGraphicsOpenGL:
      return std::make_unique<GHOST_XrGraphicsBindingOpenGL>();
#ifdef WIN32
    case GHOST_kXrGraphicsD3D11:
      return std::make_unique<GHOST_XrGraphicsBindingD3D>(context);
#endif
    default:
      return nullptr;
  }

  (void)context; /* Might be unused. */
}
