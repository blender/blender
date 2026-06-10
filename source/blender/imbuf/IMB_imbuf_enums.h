/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_enum_flags.hh"

/** \file
 * \ingroup imbuf
 */

namespace blender {

#define IM_MAX_SPACE 64

enum class ImBufFlags {
  Zero = 0,

  /**
   * Flag for image creation & IO functions: create or prefer byte data
   * (0..1 range in a byte, always 4 channels).
   */
  ByteData = 1 << 0,
  Test = 1 << 1,
  /**
   * Flag for image creation & IO functions: create or prefer float data
   * (usually 1..4 channels, 32-bit float per channel).
   */
  FloatData = 1 << 5,
  /**
   * Multi-layer EXR handling.
   *
   * As a load flag, this indicates the caller supports multi-layer EXR.
   * For such files, the MultiLayer flag will be set on the ImBuf and no
   * pixels will be loaded yet, only metadata. The caller can then load
   * the layers as needed using the IMB_exr API.
   */
  MultiLayer = 1 << 7,
  Metadata = 1 << 8,
  Deinterlace = 1 << 9,
  /** Do not clear image pixel buffer to zero. Without this flag, allocating
   * a new ImBuf does clear the pixel data to zero (transparent black). If
   * whole pixel data is overwritten after allocation, then this flag can be
   * faster since it avoids a memory clear. */
  UninitializedPixels = 1 << 10,

  /** Indicates whether image on disk have pre-multiplied alpha. */
  AlphaPremul = 1 << 12,
  /** If this flag is set, alpha mode would be guessed from file. */
  AlphaDetect = 1 << 13,
  /** Alpha channel is unrelated to RGB and should not affect it. */
  AlphaChannelPacked = 1 << 14,
  /** Ignore alpha on load and substitute it with 1.0f. */
  AlphaIgnore = 1 << 15,
  Thumbnail = 1 << 16,
  /**
   * The image contains display window information. See ImbBuf.display_size and other members for
   * more information. */
  HasDisplayWindow = 1 << 17,

  /** Perform no color space conversions when reading, leave the image in the file colorspace. */
  NoColorspaceConvert = 1 << 18,
};
ENUM_OPERATORS(ImBufFlags);

/** #ImBuf.ftype: main image types. */
enum eImbFileType : int8_t {
  IMB_FTYPE_NONE = 0,
  IMB_FTYPE_PNG = 1,
  IMB_FTYPE_TGA = 2,
  IMB_FTYPE_JPG = 3,
  IMB_FTYPE_BMP = 4,
  IMB_FTYPE_OPENEXR = 5,
  IMB_FTYPE_IRIS = 6,
  IMB_FTYPE_PSD = 7,
#ifdef WITH_IMAGE_OPENJPEG
  IMB_FTYPE_JP2 = 8,
#endif
  IMB_FTYPE_RADHDR = 9,
  IMB_FTYPE_TIF = 10,
#ifdef WITH_IMAGE_CINEON
  IMB_FTYPE_CINEON = 11,
  IMB_FTYPE_DPX = 12,
#endif

  IMB_FTYPE_DDS = 13,
#ifdef WITH_IMAGE_WEBP
  IMB_FTYPE_WEBP = 14,
#endif
  IMB_FTYPE_AVIF = 15,
};
#define IMB_FTYPE_LAST IMB_FTYPE_AVIF

/** Flags for #ImFileType.capability_read and #ImFileType.capability_write. */
enum class eImFileTypeCapability : uint8_t {
  Zero = 0,
  File = (1 << 0),
  Memory = (1 << 1),
};
ENUM_OPERATORS(eImFileTypeCapability);

/** NOTE: Keep in sync with #MovieClipProxy.build_size_flag */
enum IMB_Proxy_Size {
  IMB_PROXY_NONE = 0,
  IMB_PROXY_25 = 1,
  IMB_PROXY_50 = 2,
  IMB_PROXY_75 = 4,
  IMB_PROXY_100 = 8,
  IMB_PROXY_MAX_SLOT = 4,
};
ENUM_OPERATORS(IMB_Proxy_Size);

}  // namespace blender
