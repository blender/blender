/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "decoder.h"
#include "common.h"

int decodeFromFile(const char *filename, ktxTexture2 **newTex)
{
  ktx_error_code_e result = ktxTexture2_CreateFromNamedFile(
      filename, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, newTex);
  if (result != KTX_SUCCESS) {
    return result;
  }

  /* Because KHR_texture_basisu requires ETC1S or UASTC encoding,
   * a transcode is always necessary for glTF usage. */

  if (ktxTexture2_NeedsTranscoding(*newTex)) {
    result = ktxTexture2_TranscodeBasis(*newTex, KTX_TTF_RGBA32, 0);
    if (result != KTX_SUCCESS) {
      ktxTexture2_Destroy(*newTex);
      *newTex = nullptr;
      return result;
    }
  }

  /* When transcoded, the texture is in RGBA8
   * In glTF KHR_texture_basisu context, images are always transcoded
   * Adding a sanity check here, in case the glTF extension specification is not followed
   * correctly */
  if ((*newTex)->vkFormat != VK_FORMAT_R8G8B8A8_UNORM &&
      (*newTex)->vkFormat != VK_FORMAT_R8G8B8A8_SRGB)
  {
    ktxTexture2_Destroy(*newTex);
    *newTex = nullptr;
    return KTX_UNSUPPORTED_FEATURE;
  }

  return KTX_SUCCESS;
}

int getImageInfo(ktxTexture2 *texture, unsigned int *width, unsigned int *height, size_t *dataSize)
{
  if (texture == nullptr) {
    return KTX_INVALID_VALUE;
  }

  *width = texture->baseWidth;
  *height = texture->baseHeight;
  *dataSize = ktxTexture_GetImageSize(ktxTexture(texture), 0);

  return KTX_SUCCESS;
}

int getImageDataFloat(ktxTexture2 *texture, float *destination, size_t destinationSize)
{
  if (texture == nullptr) {
    return KTX_INVALID_VALUE;
  }

  /* In glTF KHR_texture_basisu context, images are always transcoded
   * So we should have a RGBA8 image here
   * Adding a sanity check here, in case the glTF extension specification is not followed
   * correctly */
  if (texture->vkFormat != VK_FORMAT_R8G8B8A8_UNORM &&
      texture->vkFormat != VK_FORMAT_R8G8B8A8_SRGB)
  {
    return KTX_UNSUPPORTED_FEATURE;
  }

  const ktx_size_t dataSize = ktxTexture_GetImageSize(ktxTexture(texture), 0);
  if (destinationSize < dataSize * sizeof(float)) {
    return KTX_INVALID_VALUE;
  }

  /* Retrieve the offset of the highest resolution mip level, first array layer, and first face */
  ktx_size_t offset;
  if (ktxTexture_GetImageOffset(ktxTexture(texture), 0, 0, 0, &offset) != KTX_SUCCESS) {
    return KTX_INVALID_OPERATION;
  }
  const ktx_uint8_t *src = ktxTexture_GetData(ktxTexture(texture)) + offset;
  const ktx_uint32_t rowPitch = ktxTexture_GetRowPitch(ktxTexture(texture), 0);
  const ktx_uint32_t height = texture->baseHeight;

  /* KTX stores rows top-to-bottom; Blender's `Image.pixels` expects bottom-to-top. */
  for (ktx_uint32_t y = 0; y < height; y++) {
    const ktx_uint8_t *srcRow = src + (ktx_size_t)y * rowPitch;
    float *dstRow = destination + (ktx_size_t)(height - 1 - y) * rowPitch;
    for (ktx_uint32_t x = 0; x < rowPitch; x++) {
      dstRow[x] = srcRow[x] / 255.0f;
    }
  }

  return KTX_SUCCESS;
}

void destroyTexture(ktxTexture2 *texture)
{
  if (texture != nullptr) {
    ktxTexture2_Destroy(texture);
  }
}
