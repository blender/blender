/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#ifndef _WIN32
#  error "GHOST_XrGraphcisBindingD3D can only be compiled on Windows platforms."
#endif

#include <algorithm>
#include <sstream>

#include "GHOST_SystemWin32.hh"
#include "GHOST_XrException.hh"
#include "GHOST_XrGraphicsBindingD3D.hh"

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

/* -------------------------------------------------------------------- */
/** \name Direct3D binding
 * \{ */

GHOST_XrGraphicsBindingD3D::GHOST_XrGraphicsBindingD3D() : GHOST_IXrGraphicsBinding()
{
  ghost_d3d_ctx_ = GHOST_SystemWin32::createOffscreenContextD3D();
}
GHOST_XrGraphicsBindingD3D::~GHOST_XrGraphicsBindingD3D()
{
  if (ghost_d3d_ctx_) {
    GHOST_SystemWin32::disposeContextD3D(ghost_d3d_ctx_);
  }
}

bool GHOST_XrGraphicsBindingD3D::checkVersionRequirements(
    GHOST_Context & /*ghost_ctx*/, /* Remember: This is the OpenGL context! */
    XrInstance instance,
    XrSystemId system_id,
    std::string *r_requirement_info) const
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

    *r_requirement_info = strstream.str();
  }

  return ghost_d3d_ctx_->device_->GetFeatureLevel() >= gpu_requirements.minFeatureLevel;
}

void GHOST_XrGraphicsBindingD3D::initFromGhostContext(
    GHOST_Context & /*ghost_ctx*/ /* Remember: This is the OpenGL context! */,
    XrInstance /*instance*/,
    XrSystemId /*system_id*/
)
{
  oxr_binding.d3d11.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
  oxr_binding.d3d11.device = ghost_d3d_ctx_->device_;
}

std::optional<int64_t> GHOST_XrGraphicsBindingD3D::chooseSwapchainFormat(
    const std::vector<int64_t> &runtime_formats,
    GHOST_TXrSwapchainFormat &r_format,
    bool &r_is_srgb_format) const
{
  std::vector<int64_t> gpu_binding_formats = {
#if 0 /* RGB10A2, RGBA16 don't seem to work with Oculus head-sets, \
       * so move them after RGBA16F for the time being. */
        DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R16G16B16A16_UNORM,
#endif
      DXGI_FORMAT_R16G16B16A16_FLOAT,
#if 1
      DXGI_FORMAT_R10G10B10A2_UNORM,
      DXGI_FORMAT_R16G16B16A16_UNORM,
#endif
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

std::vector<XrSwapchainImageBaseHeader *> GHOST_XrGraphicsBindingD3D::createSwapchainImages(
    uint32_t image_count)
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
  image_cache_.push_back(std::move(d3d_images));

  return base_images;
}

bool GHOST_XrGraphicsBindingD3D::needsUpsideDownDrawing(GHOST_Context &) const
{
  return ghost_d3d_ctx_->isUpsideDown();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OpenGL-Direct3D bridge
 * \{ */

GHOST_XrGraphicsBindingOpenGLD3D::GHOST_XrGraphicsBindingOpenGLD3D(GHOST_Context &ghost_ctx)

    : GHOST_XrGraphicsBindingD3D(), ghost_wgl_ctx_(static_cast<GHOST_ContextWGL &>(ghost_ctx))
{
}

GHOST_XrGraphicsBindingOpenGLD3D::~GHOST_XrGraphicsBindingOpenGLD3D()
{
  if (shared_resource_) {
    ghost_d3d_ctx_->disposeSharedOpenGLResource(shared_resource_);
    shared_resource_ = nullptr;
  }
}

void GHOST_XrGraphicsBindingOpenGLD3D::submitToSwapchainImage(
    XrSwapchainImageBaseHeader &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  XrSwapchainImageD3D11KHR &d3d_swapchain_image = reinterpret_cast<XrSwapchainImageD3D11KHR &>(
      swapchain_image);

#if 0
    /* Ideally we'd just create a render target view for the OpenXR swap-chain image texture and
     * blit from the OpenGL context into it. The NV_DX_interop extension doesn't want to work with
     * this though. At least not with OPTIMUS hardware. See:
     * https://github.com/mpv-player/mpv/issues/2949#issuecomment-197262807.
     */

    ID3D11RenderTargetView *rtv;
    CD3D11_RENDER_TARGET_VIEW_DESC rtv_desc(D3D11_RTV_DIMENSION_TEXTURE2D,
                                            DXGI_FORMAT_R8G8B8A8_UNORM);

    ghost_ctx_->device_->CreateRenderTargetView(d3d_swapchain_image.texture, &rtv_desc, &rtv);
    if (!shared_resource_) {
      DXGI_FORMAT format;
      ghost_format_to_dx_format(draw_info.swapchain_format, draw_info.expects_srgb_buffer, format);
      shared_resource_ = ghost_ctx_->createSharedOpenGLResource(
          draw_info.width, draw_info.height, format, rtv);
    }
    ghost_ctx_->blitFromOpenGLContext(shared_resource_, draw_info.width, draw_info.height);
#else
  if (!shared_resource_) {
    DXGI_FORMAT format;
    ghost_format_to_dx_format(draw_info.swapchain_format, draw_info.expects_srgb_buffer, format);
    shared_resource_ = ghost_d3d_ctx_->createSharedOpenGLResource(
        draw_info.width, draw_info.height, format);
  }
  ghost_d3d_ctx_->blitFromOpenGLContext(shared_resource_, draw_info.width, draw_info.height);

  ghost_d3d_ctx_->device_ctx_->OMSetRenderTargets(0, nullptr, nullptr);
  ghost_d3d_ctx_->device_ctx_->CopyResource(d3d_swapchain_image.texture,
                                            ghost_d3d_ctx_->getSharedTexture2D(shared_resource_));
#endif
}

/** \} */

#ifdef WITH_VULKAN_BACKEND

/* -------------------------------------------------------------------- */
/** \name Vulkan-Direct3D bridge
 * \{ */

GHOST_XrGraphicsBindingVulkanD3D::GHOST_XrGraphicsBindingVulkanD3D(GHOST_Context &ghost_ctx)

    : GHOST_XrGraphicsBindingD3D(), ghost_ctx_(static_cast<GHOST_ContextVK &>(ghost_ctx))
{
}

GHOST_XrGraphicsBindingVulkanD3D::~GHOST_XrGraphicsBindingVulkanD3D() {}

void GHOST_XrGraphicsBindingVulkanD3D::submitToSwapchainImage(
    XrSwapchainImageBaseHeader &swapchain_image, const GHOST_XrDrawViewInfo &draw_info)
{
  XrSwapchainImageD3D11KHR &d3d_swapchain_image = reinterpret_cast<XrSwapchainImageD3D11KHR &>(
      swapchain_image);

  VkDeviceSize component_size = 4 * sizeof(uint8_t);
  if (draw_info.swapchain_format == GHOST_kXrSwapchainFormatRGBA16F ||
      draw_info.swapchain_format == GHOST_kXrSwapchainFormatRGBA16)
  {
    component_size = 4 * sizeof(uint16_t);
  }

  ID3D11Device *d3d_device = ghost_d3d_ctx_->device_;
  ID3D11DeviceContext *d3d_device_ctx = ghost_d3d_ctx_->device_ctx_;
  DXGI_FORMAT format;
  ghost_format_to_dx_format(draw_info.swapchain_format, draw_info.expects_srgb_buffer, format);

  /* Acquire frame buffer image. */
  GHOST_VulkanOpenXRData openxr_data = {GHOST_kVulkanXRModeCPU};
  ghost_ctx_.openxr_acquire_framebuffer_image_callback_(&openxr_data);

  /* Upload the data to a D3D Texture */
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = openxr_data.extent.width;
  desc.Height = openxr_data.extent.height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_IMMUTABLE;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  D3D11_SUBRESOURCE_DATA data;
  data.pSysMem = openxr_data.cpu.image_data;
  data.SysMemPitch = component_size * openxr_data.extent.width;
  data.SysMemSlicePitch = 0;

  ID3D11Texture2D *texture = nullptr;
  d3d_device->CreateTexture2D(&desc, &data, &texture);

  /* Copy sub-resource of the uploaded texture to the swap-chain texture. */
  d3d_device_ctx->CopySubresourceRegion(
      d3d_swapchain_image.texture, 0, draw_info.ofsx, draw_info.ofsy, 0, texture, 0, nullptr);

  /* Release the texture. */
  texture->Release();

  /* Release frame buffer image. */
  ghost_ctx_.openxr_release_framebuffer_image_callback_(&openxr_data);
}

/** \} */

#endif
