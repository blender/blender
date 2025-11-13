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
#  if defined(WITH_OPENGL_BACKEND)
#    include "GHOST_ContextEGL.hh"
#    include "GHOST_ContextGLX.hh"
#  endif
#  include "GHOST_SystemX11.hh"
#endif
#if defined(WITH_GHOST_WAYLAND)
#  if defined(WITH_OPENGL_BACKEND)
#    include "GHOST_ContextEGL.hh"
#  endif
#  include "GHOST_SystemWayland.hh"
#endif
#if defined(WIN32)
#  include "GHOST_ContextD3D.hh"
#  include "GHOST_ContextWGL.hh"
#  include "GHOST_SystemWin32.hh"
#  include "GHOST_XrGraphicsBindingD3D.hh"
#endif
#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_XrGraphicsBindingVulkan.hh"
#endif
#ifdef WITH_METAL_BACKEND
#  include "GHOST_XrGraphicsBindingMetal.hh"
#endif

#include "GHOST_C-api.h"
#include "GHOST_XrException.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_IXrGraphicsBinding.hh"

#if defined(WITH_OPENGL_BACKEND)
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
    if (fbo_ != 0) {
      glDeleteFramebuffers(1, &fbo_);
    }
  }

  bool checkVersionRequirements(GHOST_Context &ghost_ctx,
                                XrInstance instance,
                                XrSystemId system_id,
                                std::string *r_requirement_info) const override
  {
    int gl_major_version, gl_minor_version;
#  if defined(WIN32)
    GHOST_ContextWGL &ctx_gl = static_cast<GHOST_ContextWGL &>(ghost_ctx);
    gl_major_version = ctx_gl.context_major_version_;
    gl_minor_version = ctx_gl.context_minor_version_;
#  elif defined(WITH_GHOST_X11) || defined(WITH_GHOST_WAYLAND)
    if (dynamic_cast<GHOST_ContextEGL *>(&ghost_ctx)) {
      GHOST_ContextEGL &ctx_gl = static_cast<GHOST_ContextEGL &>(ghost_ctx);
      gl_major_version = ctx_gl.context_major_version_;
      gl_minor_version = ctx_gl.context_minor_version_;
    }
#    if defined(WITH_GHOST_X11)
    else {
      GHOST_ContextGLX &ctx_gl = static_cast<GHOST_ContextGLX &>(ghost_ctx);
      gl_major_version = ctx_gl.context_major_version_;
      gl_minor_version = ctx_gl.context_minor_version_;
    }
#    endif
#  endif
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

  void initFromGhostContext(GHOST_Context &ghost_ctx,
                            XrInstance /*instance*/,
                            XrSystemId /*system_id*/) override
  {
#  if defined(WITH_GHOST_X11) || defined(WITH_GHOST_WAYLAND)
    /* WAYLAND/X11 may be dynamically selected at load time but both may also be
     * supported at compile time individually.
     * Without `is_ctx_egl` & `is_wayland` preprocessor checks become an unmanageable soup. */
    const bool is_ctx_egl = dynamic_cast<GHOST_ContextEGL *>(&ghost_ctx) != nullptr;
    if (is_ctx_egl) {
      GHOST_ContextEGL &ctx_egl = static_cast<GHOST_ContextEGL &>(ghost_ctx);
      const bool is_wayland = (
#    if defined(WITH_GHOST_WAYLAND)
          dynamic_cast<const GHOST_SystemWayland *const>(ctx_egl.system_) != nullptr
#    else
          false
#    endif
      );

      if (is_wayland) {
#    if defined(WITH_GHOST_WAYLAND)
        /* #GHOST_SystemWayland */
        oxr_binding.wl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WAYLAND_KHR;
        oxr_binding.wl.display = (wl_display *)ctx_egl.native_display_;
#    else
        GHOST_ASSERT(false, "Unexpected State: logical error, unreachable!");
#    endif /* !WITH_GHOST_WAYLAND */
      }
      else { /* `!is_wayland` */
#    if defined(WITH_GHOST_X11)
        /* #GHOST_SystemX11. */
        oxr_binding.egl.type = XR_TYPE_GRAPHICS_BINDING_EGL_MNDX;
#      if XR_CURRENT_API_VERSION >= XR_MAKE_VERSION(1, 0, 29)
        oxr_binding.egl.getProcAddress = reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(
            eglGetProcAddress);
#      else
        oxr_binding.egl.getProcAddress = reinterpret_cast<PFNEGLGETPROCADDRESSPROC>(
            eglGetProcAddress);
#      endif
        oxr_binding.egl.display = ctx_egl.getDisplay();
        oxr_binding.egl.config = ctx_egl.getConfig();
        oxr_binding.egl.context = ctx_egl.getContext();
#    else
        GHOST_ASSERT(false, "Unexpected State: built with only WAYLAND and no System found!");
#    endif /* !WITH_GHOST_X11 */
      }
    }
    else { /* `!is_ctx_egl` */
#    if defined(WITH_GHOST_X11)
      GHOST_ContextGLX &ctx_glx = static_cast<GHOST_ContextGLX &>(ghost_ctx);
      XVisualInfo *visual_info = glXGetVisualFromFBConfig(ctx_glx.display_, ctx_glx.fbconfig_);

      oxr_binding.glx.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
      oxr_binding.glx.xDisplay = ctx_glx.display_;
      oxr_binding.glx.glxFBConfig = ctx_glx.fbconfig_;
      oxr_binding.glx.glxDrawable = ctx_glx.window_;
      oxr_binding.glx.glxContext = ctx_glx.context_;
      oxr_binding.glx.visualid = visual_info->visualid;

      XFree(visual_info);
#    else
      GHOST_ASSERT(false, "Unexpected State: built without X11 and no EGL context is available!");
#    endif /* !WITH_GHOST_X11 */
    }
#  elif defined(WIN32)
    GHOST_ContextWGL &ctx_wgl = static_cast<GHOST_ContextWGL &>(ghost_ctx);

    oxr_binding.wgl.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
    oxr_binding.wgl.hDC = ctx_wgl.h_DC_;
    oxr_binding.wgl.hGLRC = ctx_wgl.h_GLRC_;
#  endif /* WIN32 */

    /* Generate a frame-buffer to use for blitting into the texture. */
    glGenFramebuffers(1, &fbo_);
  }

  std::optional<int64_t> chooseSwapchainFormat(const std::vector<int64_t> &runtime_formats,
                                               GHOST_TXrSwapchainFormat &r_format,
                                               bool &r_is_srgb_format) const override
  {
    std::vector<int64_t> gpu_binding_formats = {
#  if 0 /* RGB10A2, RGBA16 don't seem to work with Oculus head-sets, \
         * so move them after RGBA16F for the time being. */
        GL_RGB10_A2,
        GL_RGBA16,
#  endif
        GL_RGBA16F,
#  if 1
        GL_RGB10_A2,
        GL_RGBA16,
#  endif
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
    image_cache_.push_back(std::move(ogl_images));

    return base_images;
  }

  void submitToSwapchainBegin() override {}
  void submitToSwapchainImage(XrSwapchainImageBaseHeader &swapchain_image,
                              const GHOST_XrDrawViewInfo &draw_info) override
  {
    XrSwapchainImageOpenGLKHR &ogl_swapchain_image = reinterpret_cast<XrSwapchainImageOpenGLKHR &>(
        swapchain_image);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);

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
  void submitToSwapchainEnd() override {}

  bool needsUpsideDownDrawing(GHOST_Context &ghost_ctx) const override
  {
    return ghost_ctx.isUpsideDown();
  }

 private:
  std::list<std::vector<XrSwapchainImageOpenGLKHR>> image_cache_;
  GLuint fbo_ = 0;
};
#endif

std::unique_ptr<GHOST_IXrGraphicsBinding> GHOST_XrGraphicsBindingCreateFromType(
    GHOST_TXrGraphicsBinding type, GHOST_Context &context)
{
  switch (type) {
#ifdef WITH_OPENGL_BACKEND
    case GHOST_kXrGraphicsOpenGL:
      return std::make_unique<GHOST_XrGraphicsBindingOpenGL>();
#endif
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kXrGraphicsVulkan:
      return std::make_unique<GHOST_XrGraphicsBindingVulkan>(context);
#endif
#ifdef WITH_METAL_BACKEND
    case GHOST_kXrGraphicsMetal:
      return std::make_unique<GHOST_XrGraphicsBindingMetal>(context);
#endif
#ifdef WIN32
#  ifdef WITH_OPENGL_BACKEND
    case GHOST_kXrGraphicsOpenGLD3D11:
      return std::make_unique<GHOST_XrGraphicsBindingOpenGLD3D>(context);
#  endif
#  ifdef WITH_VULKAN_BACKEND
    case GHOST_kXrGraphicsVulkanD3D11:
      return std::make_unique<GHOST_XrGraphicsBindingVulkanD3D>(context);
#  endif
#endif
    default:
      return nullptr;
  }

  (void)context; /* Might be unused. */
}
