/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "encoder.h"
#include "common.h"

#include <algorithm>
#include <cstring>
#include <thread>
#include <vector>

int encodeToFile(const char *filename,
                 const unsigned char *pixels,
                 unsigned int width,
                 unsigned int height,
                 unsigned int channels,
                 int quality,
                 int is_data,
                 int use_uastc,
                 int compress)
{
  if (pixels == nullptr || (channels != 3 && channels != 4)) {
    return KTX_INVALID_VALUE;
  }

  // KHR_texture_basisu requires width and height to be multiples of 4
  if (width % 4 != 0 || height % 4 != 0) {
    return KTX_INVALID_VALUE;
  }

  /* KTX2 stores rows top-to-bottom; `pixels` follows Blender's bottom-to-top convention, so
   * flip while copying into a KTX-ready buffer. */
  const size_t row_size = size_t(width) * channels;
  std::vector<unsigned char> flipped(row_size * height);
  for (unsigned int y = 0; y < height; y++) {
    memcpy(flipped.data() + size_t(y) * row_size,
           pixels + size_t(height - 1 - y) * row_size,
           row_size);
  }

  ktxTextureCreateInfo create_info = {};
  if (channels == 3) {
    create_info.vkFormat = is_data ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R8G8B8_SRGB;
  }
  else {
    create_info.vkFormat = is_data ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
  }
  create_info.baseWidth = width;
  create_info.baseHeight = height;
  create_info.baseDepth = 1;
  create_info.numDimensions = 2;
  create_info.numLevels = 1;
  create_info.numLayers = 1;
  create_info.numFaces = 1;
  create_info.isArray = KTX_FALSE;
  create_info.generateMipmaps = KTX_FALSE;

  ktxTexture2 *texture = nullptr;
  ktx_error_code_e result = ktxTexture2_Create(
      &create_info, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &texture);
  if (result != KTX_SUCCESS) {
    return result;
  }

  result = ktxTexture_SetImageFromMemory(ktxTexture(texture),
                                         /*level*/ 0,
                                         /*layer*/ 0,
                                         /*faceSlice*/ 0,
                                         flipped.data(),
                                         flipped.size());
  if (result != KTX_SUCCESS) {
    ktxTexture_Destroy(ktxTexture(texture));
    return result;
  }

  ktxBasisParams params = {};
  params.structSize = sizeof(params);
  params.threadCount = std::max(1u, std::thread::hardware_concurrency());

  if (use_uastc) {
    /* UASTC: higher quality than ETC1S, larger output.
     * Map `quality` [0, 100] to UASTC levels [0, KTX_PACK_UASTC_MAX_LEVEL]. */
    params.uastc = KTX_TRUE;
    const int level = int(float(quality) / 100.0f * float(KTX_PACK_UASTC_MAX_LEVEL) + 0.5f);
    params.uastcFlags = ktx_pack_uastc_flags(std::clamp(level, 0, int(KTX_PACK_UASTC_MAX_LEVEL)));
  }
  else {
    /* ETC1S: smaller output, lower quality than UASTC.
     * Map `quality` [0, 100] to qualityLevel [1, 255]. */
    params.compressionLevel = KTX_ETC1S_DEFAULT_COMPRESSION_LEVEL;
    params.qualityLevel = uint32_t(
        std::clamp(int(float(quality) / 100.0f * 254.0f + 1.5f), 1, 255));
  }

  result = ktxTexture2_CompressBasisEx(texture, &params);
  if (result != KTX_SUCCESS) {
    ktxTexture_Destroy(ktxTexture(texture));
    return result;
  }

  /* `ktxTexture2_Create()` always writes BT709 primaries, regardless of the source
   * VkFormat. KHR_texture_basisu requires UNSPECIFIED primaries for non-color data (normal
   * maps, etc.), so force it back for `is_data` textures. */
  if (is_data) {
    ktxTexture2_SetPrimaries(texture, KHR_DF_PRIMARIES_UNSPECIFIED);
  }

  /* Zstd supercompression can only be layered on top of UASTC: ETC1S already sets its own
   * `supercompressionScheme` (KTX_SS_BASIS_LZ), and `ktxTexture2_DeflateZstd()` rejects any
   * texture whose scheme isn't already KTX_SS_NONE. */
  if (use_uastc && compress > 0) {
    const ktx_uint32_t zstd_level = ktx_uint32_t(std::clamp(compress, 1, 22));
    result = ktxTexture2_DeflateZstd(texture, zstd_level);
    if (result != KTX_SUCCESS) {
      ktxTexture_Destroy(ktxTexture(texture));
      return result;
    }
  }

  result = ktxTexture_WriteToNamedFile(ktxTexture(texture), filename);
  ktxTexture_Destroy(ktxTexture(texture));

  return result;
}
