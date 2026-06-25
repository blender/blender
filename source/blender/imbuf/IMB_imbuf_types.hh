/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 *
 * Image buffer types.
 */

#include "BLI_assert.hh"
#include "BLI_enum_flags.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_mutex.hh"
#include "BLI_string_ref.hh"

#include "DNA_image_enums.h"
#include "IMB_imbuf_enums.h"

#include <string>

namespace blender {

namespace gpu {
class Texture;
}
struct IDProperty;

namespace ocio {
class ColorSpace;
}
using ColorSpace = ocio::ColorSpace;

/**
 * \ingroup imbuf
 * This is the abstraction of an image. ImBuf is the basic type used for all imbuf operations.
 *
 * Also; add new variables to the end to save pain!
 */

/**
 * #ImBuf::foptions.flag, type specific options.
 * Some formats include compression rations on some bits.
 */

#define OPENEXR_HALF (1 << 8)
#define OPENEXR_MULTIPART (1 << 9)
/* Lowest bits of foptions.flag / exr_codec contain actual codec enum. */
#define OPENEXR_CODEC_MASK (0xF)

#ifdef WITH_IMAGE_CINEON
#  define CINEON_LOG (1 << 8)
#  define CINEON_16BIT (1 << 7)
#  define CINEON_12BIT (1 << 6)
#  define CINEON_10BIT (1 << 5)
#endif

#ifdef WITH_IMAGE_OPENJPEG
#  define JP2_12BIT (1 << 9)
#  define JP2_16BIT (1 << 8)
#  define JP2_YCC (1 << 7)
#  define JP2_CINE (1 << 6)
#  define JP2_CINE_48FPS (1 << 5)
#  define JP2_JP2 (1 << 4)
#  define JP2_J2K (1 << 3)
#endif

#define PNG_16BIT (1 << 10)

#define RAWTGA 1

#define TIF_16BIT (1 << 8)
#define TIF_COMPRESS_NONE (1 << 7)
#define TIF_COMPRESS_DEFLATE (1 << 6)
#define TIF_COMPRESS_LZW (1 << 5)
#define TIF_COMPRESS_PACKBITS (1 << 4)

#define AVIF_10BIT (1 << 8)
#define AVIF_12BIT (1 << 9)

#define DDS_COMPRESSED_DXT1 (1 << 8)
#define DDS_COMPRESSED_DXT3 (1 << 9)
#define DDS_COMPRESSED_DXT5 (1 << 10)

struct ImbFormatOptions {
  short flag = 0;
  /** Quality for JPEG, WebP, AVIF. */
  char quality = 90;
  /* Compression amount for PNG.
   * Default to low compression ratio that is not time consuming. */
  char compress = 15;
};

/* -------------------------------------------------------------------- */
/** \name ImBuf buffer storage
 * \{ */

/* Different storage specialization.
 *
 * NOTE: Avoid direct access. Use the buffer utilities from the IMB_imbuf.hh  instead
 */

struct ImBufByteBuffer {
  const uint8_t *data = nullptr;
  ImplicitSharingPtr<> sharing_info;

  const ColorSpace *colorspace = nullptr;
};

struct ImBufFloatBuffer {
  const float *data = nullptr;
  ImplicitSharingPtr<> sharing_info;

  const ColorSpace *colorspace = nullptr;
};

enum ImBufGPUFlag : int {
  /** Mipmap chain has been generated for the GPU texture. */
  IMB_GPU_MIPMAP_COMPLETE = (1 << 0),
  /** Disable mipmap updates, primarily used for texture painting. */
  IMB_GPU_DISABLE_MIPMAP_UPDATE = (1 << 1),
  /** GPU texture failed to be loaded onto the GPU, to distinguish a null
   * texture between not yet loaded and failed to load. */
  IMB_GPU_LOAD_FAILED = (1 << 2),
};
ENUM_OPERATORS(ImBufGPUFlag)

struct ImBufGPU {
  /**
   * Texture which corresponds to the state of the ImBuf on the GPU.
   *
   * Allocation is supposed to happen outside of the ImBuf module from a proper GPU context.
   * De-referencing the ImBuf or its GPU texture can happen from any state.
   *
   * TODO(@sergey): This should become a list of textures, to support having high-res ImBuf on GPU
   * without hitting hardware limitations.
   */
  gpu::Texture *texture = nullptr;

  /** Last used timestamp for garbage collection */
  int64_t lastused = 0;

  /** GPU buffer flags. */
  ImBufGPUFlag flag = ImBufGPUFlag(0);

  /** Mutex guarding access to #texture, #lastused, and #flag. */
  blender::Mutex mutex;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Buffer
 * \{ */

struct ImBuf {
  /* dimensions */
  /** Width and Height of our image buffer.
   * Should be 'unsigned int' since most formats use this.
   * but this is problematic with texture math in `imagetexture.c`
   * avoid problems and use int. - campbell */
  int x = 0;
  int y = 0;

  /**
   * Stores the Data and Display Window information. Those are only initialized if the image buffer
   * has the #ImBufFlags::HasDisplayWindow flag active, otherwise, they should be ignored as the
   * image has no display window.
   *
   * The data size is already stored in the x and y members. The data_offset member stores the
   * offset from the display window to the data window, if positive, then only part of the display
   * window has data, while if negative, it means the image has over-scan.
   * The display_offset member is the offset from the origin,
   * can be interpreted as a global translation.
   */
  int display_size[2];
  int data_offset[2];
  int display_offset[2];

  /**
   * Number of channels in `float_buffer` (0 = 4 channel default).
   * Note that `byte_buffer` always has 4 channels.
   */
  int channels = 0;

  /**
   * How to interpret pixel color values in the data that is present.
   * For example, byte buffer always contains 4 channels, but if code
   * knows that the alpha channel is fully opaque, it should set color mode
   * to RGB.
   */
  ImColorMode color_mode = ImColorMode::RGBA;

  ImBufFlags flags = ImBufFlags::Zero;

  /* pixels */

  /**
   * Image pixel buffer (8bit representation):
   * - color space defaults to `sRGB`.
   * - alpha defaults to 'straight'.
   */
  ImBufByteBuffer byte_buffer;

  /**
   * Image pixel buffer (float representation):
   * - color space defaults to `linear` (`rec709`).
   * - alpha defaults to `premul`.
   * \note May need gamma correction to `sRGB` when generating 8bit representations.
   * \note Formats that support higher more than 8 but channels load as floats.
   */
  ImBufFloatBuffer float_buffer;

  /** Image buffer on the GPU. */
  ImBufGPU gpu;

  /** Resolution in pixels per meter. Multiply by `0.0254` for DPI. */
  double ppm[2] = {0.0, 0.0};

  /** Amount of dithering to apply, when converting float -> byte. */
  float dither = 0.0f;

  /** Last used timestamp for garbage collection. */
  int64_t lastused = 0;
  /** used to set imbuf to dirty and other stuff */
  int userflags = 0;

  /** Image Metadata */
  IDProperty *metadata_ptr = nullptr;
  /** Implicit-sharing owner for #metadata_ptr. */
  ImplicitSharingPtr<> metadata_sharing_info;

  /* file information */
  /** file type we are going to save as */
  eImbFileType ftype = IMB_FTYPE_NONE;
  /** file format specific flags */
  ImbFormatOptions foptions;
  /** The absolute file path associated with this image. */
  std::string filepath;
  /** For movie files and image sequences, the frame number loaded from the file. */
  int fileframe = 0;

  /** reference counter for multiple users */
  int32_t refcounter = 0;

  const uint8_t *byte_data() const;
  uint8_t *byte_data_for_write();

  const float *float_data() const;
  float *float_data_for_write();

  /** Take sole ownership of a buffer allocated with the guarded allocator. */
  void assign_byte_data(uint8_t *data);
  void assign_float_data(float *data);

  /** Share ownership with the implicit sharing referenced by the pointer. */
  void assign_byte_data(const uint8_t *data, ImplicitSharingPtr<> sharing_ptr);
  void assign_float_data(const float *data, ImplicitSharingPtr<> sharing_ptr);

  /** Metadata access, should go through these methods instead of direct access. */
  const IDProperty *metadata() const;
  IDProperty *metadata_for_write();
  void assign_metadata(const IDProperty *metadata, ImplicitSharingPtr<> sharing_info);

  [[nodiscard]] bool colorspace_is_data() const;

  [[nodiscard]] bool can_contain_alpha() const
  {
    return color_mode == ImColorMode::RGBA || color_mode == ImColorMode::BW_A;
  }

  [[nodiscard]] int color_mode_channels_get() const
  {
    switch (this->color_mode) {
      case ImColorMode::BW:
        return 1;
      case ImColorMode::BW_A:
        return 2;
      case ImColorMode::RGB:
        return 3;
      case ImColorMode::RGBA:
        return 4;
    }
    BLI_assert_unreachable();
    return 0;
  }
};

/** Return default color mode for the give number of channels. */
[[nodiscard]] ImColorMode IMB_color_mode_from_channels(int channels);

/** Test if channel names indicate colors or data. */
[[nodiscard]] bool IMB_chan_id_is_color(StringRef chan_id);

/**
 * \brief userflags: Flags used internally by blender for image-buffers.
 */
enum {
  /** image needs to be saved is not the same as filename */
  IB_BITMAPDIRTY = (1 << 1),
  /** float buffer changed, needs recreation of byte rect */
  IB_RECT_INVALID = (1 << 3),
  /** either float or byte buffer changed */
  IB_DISPLAY_BUFFER_INVALID = (1 << 4),
  /** image buffer is persistent in the memory and should never be removed from the cache */
  IB_PERSISTENT = (1 << 5),
  /** The image buffer is backed by a GPU texture storage but the host buffers either do not exist
   * or are out-dated and needs to read from the GPU texture. */
  IB_HOST_BUFFER_INVALID = (1 << 6),
};

/** \} */

/**
 * Known image extensions, in most cases these match values
 * for images which Blender creates, there are some exceptions to this.
 *
 * See #BKE_image_path_ext_from_imformat which also stores known extensions.
 */
extern const char *imb_ext_image[];
extern const char *imb_ext_movie[];
extern const char *imb_ext_audio[];

inline const uint8_t *ImBuf::byte_data() const
{
  return this->byte_buffer.data;
}

inline const float *ImBuf::float_data() const
{
  return this->float_buffer.data;
}

}  // namespace blender
