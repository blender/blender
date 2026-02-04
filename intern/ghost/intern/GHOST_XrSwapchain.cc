/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#include <cassert>

#include "GHOST_IXrGraphicsBinding.hh"
#include "GHOST_XrException.hh"
#include "GHOST_Xr_intern.hh"

#include "GHOST_XrSwapchain.hh"

struct OpenXRSwapchainData {
  using ImageVec = std::vector<XrSwapchainImageBaseHeader *>;

  XrSwapchain swapchain = XR_NULL_HANDLE;
  ImageVec swapchain_images;
};

static OpenXRSwapchainData::ImageVec swapchain_images_create(XrSwapchain swapchain,
                                                             GHOST_IXrGraphicsBinding &gpu_binding)
{
  std::vector<XrSwapchainImageBaseHeader *> images;
  uint32_t image_count;

  CHECK_XR(xrEnumerateSwapchainImages(swapchain, 0, &image_count, nullptr),
           "Failed to get count of swapchain images to create for the VR session.");
  images = gpu_binding.createSwapchainImages(image_count);
  CHECK_XR(xrEnumerateSwapchainImages(swapchain, images.size(), &image_count, images[0]),
           "Failed to create swapchain images for the VR session.");

  return images;
}

GHOST_XrSwapchain::GHOST_XrSwapchain(GHOST_IXrGraphicsBinding &gpu_binding,
                                     const XrSession &session,
                                     const XrViewConfigurationView &view_config)
    : oxr_(std::make_unique<OpenXRSwapchainData>())
{
  XrSwapchainCreateInfo create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  uint32_t format_count = 0;

  CHECK_XR(xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr),
           "Failed to get count of swapchain image formats.");
  std::vector<int64_t> swapchain_formats(format_count);
  CHECK_XR(xrEnumerateSwapchainFormats(
               session, swapchain_formats.size(), &format_count, swapchain_formats.data()),
           "Failed to get swapchain image formats.");
  assert(swapchain_formats.size() == format_count);

  std::optional chosen_format = gpu_binding.chooseSwapchainFormat(
      swapchain_formats, format_, is_srgb_buffer_);
  if (!chosen_format) {
    throw GHOST_XrException(
        "Error: No format matching OpenXR runtime supported swapchain formats found.");
  }
  gpu_format_ = *chosen_format;

  create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                           XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.format = *chosen_format;
  create_info.sampleCount = view_config.recommendedSwapchainSampleCount;
  create_info.width = view_config.recommendedImageRectWidth;
  create_info.height = view_config.recommendedImageRectHeight;
  create_info.faceCount = 1;
  create_info.arraySize = 1;
  create_info.mipCount = 1;

  CHECK_XR(xrCreateSwapchain(session, &create_info, &oxr_->swapchain),
           "Failed to create OpenXR swapchain.");

  image_width_ = create_info.width;
  image_height_ = create_info.height;

  oxr_->swapchain_images = swapchain_images_create(oxr_->swapchain, gpu_binding);
}

GHOST_XrSwapchain::GHOST_XrSwapchain(GHOST_XrSwapchain &&other)
    : oxr_(std::move(other.oxr_)),
      image_width_(other.image_width_),
      image_height_(other.image_height_),
      format_(other.format_),
      gpu_format_(other.gpu_format_),
      is_srgb_buffer_(other.is_srgb_buffer_)
{
  /* Prevent xrDestroySwapchain call for the moved out item. */
  other.oxr_ = nullptr;
}

GHOST_XrSwapchain::~GHOST_XrSwapchain()
{
  /* oxr_ may be nullptr after move. */
  if (oxr_ && oxr_->swapchain != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySwapchain(oxr_->swapchain));
  }
}

XrSwapchainImageBaseHeader *GHOST_XrSwapchain::acquireDrawableSwapchainImage()

{
  XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  uint32_t image_idx;

  CHECK_XR(xrAcquireSwapchainImage(oxr_->swapchain, &acquire_info, &image_idx),
           "Failed to acquire swapchain image for the VR session.");
  wait_info.timeout = XR_INFINITE_DURATION;
  CHECK_XR(xrWaitSwapchainImage(oxr_->swapchain, &wait_info),
           "Failed to acquire swapchain image for the VR session.");

  return oxr_->swapchain_images[image_idx];
}

void GHOST_XrSwapchain::updateCompositionLayerProjectViewSubImage(XrSwapchainSubImage &r_sub_image)
{
  r_sub_image.swapchain = oxr_->swapchain;
  r_sub_image.imageRect.offset = {0, 0};
  r_sub_image.imageRect.extent = {image_width_, image_height_};
}

GHOST_TXrSwapchainFormat GHOST_XrSwapchain::getFormat() const
{
  return format_;
}

int64_t GHOST_XrSwapchain::getGPUFormat() const
{
  return gpu_format_;
}

bool GHOST_XrSwapchain::isBufferSRGB() const
{
  return is_srgb_buffer_;
}

void GHOST_XrSwapchain::releaseImage()
{
  XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

  CHECK_XR(xrReleaseSwapchainImage(oxr_->swapchain, &release_info),
           "Failed to release swapchain image used to submit VR session frame.");
}
