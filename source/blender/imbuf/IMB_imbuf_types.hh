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

#include "IMB_imbuf_enums.h"

#include <string>

namespace blender {

struct ExrHandle;
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
/** \name ImBuf Component flags
 * \brief These flags determine the components of an ImBuf struct.
 * \{ */

enum eImBufFlags {
  /** Image has byte data (unsigned 0..1 range in a byte, always 4 channels). */
  IB_byte_data = 1 << 0,
  IB_test = 1 << 1,
  /** Image has float data (usually 1..4 channels, 32 bit float per channel). */
  IB_float_data = 1 << 5,
  IB_multilayer = 1 << 7,
  IB_metadata = 1 << 8,
  IB_animdeinterlace = 1 << 9,
  /** Do not clear image pixel buffer to zero. Without this flag, allocating
   * a new ImBuf does clear the pixel data to zero (transparent black). If
   * whole pixel data is overwritten after allocation, then this flag can be
   * faster since it avoids a memory clear. */
  IB_uninitialized_pixels = 1 << 10,

  /** Indicates whether image on disk have pre-multiplied alpha. */
  IB_alphamode_premul = 1 << 12,
  /** If this flag is set, alpha mode would be guessed from file. */
  IB_alphamode_detect = 1 << 13,
  /** Alpha channel is unrelated to RGB and should not affect it. */
  IB_alphamode_channel_packed = 1 << 14,
  /** Ignore alpha on load and substitute it with 1.0f. */
  IB_alphamode_ignore = 1 << 15,
  IB_thumbnail = 1 << 16,
  /**
   * The image contains display window information. See ImbBuf.display_size and other members for
   * more information. */
  IB_has_display_window = 1 << 17,

  /** Perform no color space conversions when reading, leave the image in the file colorspace. */
  IB_no_colorspace_convert = 1 << 18,
};
ENUM_OPERATORS(eImBufFlags);

/** \} */

/* -------------------------------------------------------------------- */
/** \name ImBuf buffer storage
 * \{ */

/**
 * Specialization of an ownership whenever a bare pointer is provided to the ImBuf buffers
 * assignment API.
 */
enum ImBufOwnership {
  /**
   * The ImBuf simply shares pointer with data owned by someone else, and will not perform any
   * memory management when the ImBuf frees the buffer.
   */
  IB_DO_NOT_TAKE_OWNERSHIP = 0,

  /**
   * The ImBuf takes ownership of the buffer data, and will use MEM_delete() to free this memory
   * when the ImBuf needs to free the data.
   */
  IB_TAKE_OWNERSHIP = 1,
};

/* Different storage specialization.
 *
 * NOTE: Avoid direct access. Use the buffer utilities from the IMB_imbuf.hh  instead
 */

struct ImBufByteBuffer {
  uint8_t *data = nullptr;
  ImBufOwnership ownership = IB_DO_NOT_TAKE_OWNERSHIP;

  const ColorSpace *colorspace = nullptr;
};

struct ImBufFloatBuffer {
  float *data = nullptr;
  ImBufOwnership ownership = IB_DO_NOT_TAKE_OWNERSHIP;

  const ColorSpace *colorspace = nullptr;
};

struct ImBufGPU {
  /**
   * Texture which corresponds to the state of the ImBug on the GPU.
   *
   * Allocation is supposed to happen outside of the ImBug module from a proper GPU context.
   * De-referencing the ImBuf or its GPU texture can happen from any state.
   *
   * TODO(@sergey): This should become a list of textures, to support having high-res ImBuf on GPU
   * without hitting hardware limitations.
   */
  gpu::Texture *texture = nullptr;
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
   * has the IB_has_display_window flag active, otherwise, they should be ignored as the image has
   * no display window.
   *
   * The data size is already stored in the x and y members. The data_offset member stores the
   * offset from the display window to the data window, if positive, then only part of the display
   * window has data, while if negative, it means the image has over-scan.
   * The display_offset member is the offset from the origin,
   * can can be interpreted as a global translation.
   */
  int display_size[2];
  int data_offset[2];
  int display_offset[2];

  /** Active amount of bits/bit-planes. */
  unsigned char planes = 0;
  /** Number of channels in `rect_float` (0 = 4 channel default) */
  int channels = 0;

  /* flags */
  /** Controls which components should exist. */
  int flags = 0;

  /* pixels */

  /**
   * Image pixel buffer (8bit representation):
   * - color space defaults to `sRGB`.
   * - alpha defaults to 'straight'.
   */
  ImBufByteBuffer byte_buffer;

  /**
   * Image pixel buffer (float representation):
   * - color space defaults to 'linear' (`rec709`).
   * - alpha defaults to 'premul'.
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

  /* externally used data */
  /** reference index for ImBuf lists */
  int index = 0;
  /** used to set imbuf to dirty and other stuff */
  int userflags = 0;
  /** image metadata */
  IDProperty *metadata = nullptr;
  /** OpenEXR handle. */
  ExrHandle *exrhandle = nullptr;

  /* file information */
  /** file type we are going to save as */
  eImbFileType ftype = IMB_FTYPE_NONE;
  /** file format specific flags */
  ImbFormatOptions foptions;
  /** The absolute file path associated with this image. */
  std::string filepath;
  /** For movie files, the frame number loaded from the file. */
  int fileframe = 0;

  /** reference counter for multiple users */
  int32_t refcounter = 0;

  /* color management */
  int colormanage_flag = 0;

  const uint8_t *byte_data() const;
  uint8_t *byte_data_for_write();

  const float *float_data() const;
  float *float_data_for_write();
};

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

/* -------------------------------------------------------------------- */
/** \name ImBuf Preset Profile Tags
 *
 * \brief Some predefined color space profiles that 8 bit imbufs can represent.
 * \{ */

#define IB_PROFILE_NONE 0
#define IB_PROFILE_LINEAR_RGB 1
#define IB_PROFILE_SRGB 2
#define IB_PROFILE_CUSTOM 3

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

/* -------------------------------------------------------------------- */
/** \name ImBuf Color Management Flag
 *
 * \brief Used with #ImBuf.colormanage_flag
 * \{ */

enum {
  IMB_COLORMANAGE_IS_DATA = (1 << 0),
};

/** \} */

inline const uint8_t *ImBuf::byte_data() const
{
  return this->byte_buffer.data;
}

inline uint8_t *ImBuf::byte_data_for_write()
{
  return this->byte_buffer.data;
}

inline const float *ImBuf::float_data() const
{
  return this->float_buffer.data;
}

inline float *ImBuf::float_data_for_write()
{
  return this->float_buffer.data;
}

}  // namespace blender
