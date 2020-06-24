/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup GHOST
 */

#include <cassert>

#include "GHOST_C-api.h"

#include "GHOST_IXrGraphicsBinding.h"
#include "GHOST_XrException.h"
#include "GHOST_XrSession.h"
#include "GHOST_Xr_intern.h"

#include "GHOST_XrSwapchain.h"

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
    : m_oxr(new OpenXRSwapchainData())
{
  XrSwapchainCreateInfo create_info = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
  uint32_t format_count = 0;
  int64_t chosen_format;

  CHECK_XR(xrEnumerateSwapchainFormats(session, 0, &format_count, nullptr),
           "Failed to get count of swapchain image formats.");
  std::vector<int64_t> swapchain_formats(format_count);
  CHECK_XR(xrEnumerateSwapchainFormats(
               session, swapchain_formats.size(), &format_count, swapchain_formats.data()),
           "Failed to get swapchain image formats.");
  assert(swapchain_formats.size() == format_count);

  if (!gpu_binding.chooseSwapchainFormat(swapchain_formats, chosen_format, m_is_srgb_buffer)) {
    throw GHOST_XrException(
        "Error: No format matching OpenXR runtime supported swapchain formats found.");
  }

  create_info.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT |
                           XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
  create_info.format = chosen_format;
  create_info.sampleCount = view_config.recommendedSwapchainSampleCount;
  create_info.width = view_config.recommendedImageRectWidth;
  create_info.height = view_config.recommendedImageRectHeight;
  create_info.faceCount = 1;
  create_info.arraySize = 1;
  create_info.mipCount = 1;

  CHECK_XR(xrCreateSwapchain(session, &create_info, &m_oxr->swapchain),
           "Failed to create OpenXR swapchain.");

  m_image_width = create_info.width;
  m_image_height = create_info.height;

  m_oxr->swapchain_images = swapchain_images_create(m_oxr->swapchain, gpu_binding);
}

GHOST_XrSwapchain::GHOST_XrSwapchain(GHOST_XrSwapchain &&other)
    : m_oxr(std::move(other.m_oxr)),
      m_image_width(other.m_image_width),
      m_image_height(other.m_image_height)
{
  /* Prevent xrDestroySwapchain call for the moved out item. */
  other.m_oxr = nullptr;
}

GHOST_XrSwapchain::~GHOST_XrSwapchain()
{
  /* m_oxr may be NULL after move. */
  if (m_oxr && m_oxr->swapchain != XR_NULL_HANDLE) {
    CHECK_XR_ASSERT(xrDestroySwapchain(m_oxr->swapchain));
  }
}

XrSwapchainImageBaseHeader *GHOST_XrSwapchain::acquireDrawableSwapchainImage()

{
  XrSwapchainImageAcquireInfo acquire_info = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
  XrSwapchainImageWaitInfo wait_info = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
  uint32_t image_idx;

  CHECK_XR(xrAcquireSwapchainImage(m_oxr->swapchain, &acquire_info, &image_idx),
           "Failed to acquire swapchain image for the VR session.");
  wait_info.timeout = XR_INFINITE_DURATION;
  CHECK_XR(xrWaitSwapchainImage(m_oxr->swapchain, &wait_info),
           "Failed to acquire swapchain image for the VR session.");

  return m_oxr->swapchain_images[image_idx];
}

void GHOST_XrSwapchain::updateCompositionLayerProjectViewSubImage(XrSwapchainSubImage &r_sub_image)
{
  r_sub_image.swapchain = m_oxr->swapchain;
  r_sub_image.imageRect.offset = {0, 0};
  r_sub_image.imageRect.extent = {m_image_width, m_image_height};
}

bool GHOST_XrSwapchain::isBufferSRGB()
{
  return m_is_srgb_buffer;
}

void GHOST_XrSwapchain::releaseImage()
{
  XrSwapchainImageReleaseInfo release_info = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};

  CHECK_XR(xrReleaseSwapchainImage(m_oxr->swapchain, &release_info),
           "Failed to release swapchain image used to submit VR session frame.");
}
