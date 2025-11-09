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

#include "DNA_vec_types.h" /* for rcti */

#include "IMB_imbuf_enums.h"

struct ColormanageCache;
struct ExrHandle;
namespace blender::gpu {
class Texture;
}
struct IDProperty;

namespace blender::ocio {
class ColorSpace;
}
using ColorSpace = blender::ocio::ColorSpace;

#define IMB_FILEPATH_SIZE 1024

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

struct ImbFormatOptions {
  short flag;
  /** Quality serves dual purpose as quality number for JPEG or compression amount for PNG. */
  char quality;
};

/* -------------------------------------------------------------------- */
/** \name ImBuf Component flags
 * \brief These flags determine the components of an ImBuf struct.
 * \{ */

enum eImBufFlags {
  /** Image has byte data (unsigned 0..1 range in a byte, always 4 channels). */
  IB_byte_data = 1 << 0,
  IB_test = 1 << 1,
  IB_mem = 1 << 4,
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
};

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
   * The ImBuf takes ownership of the buffer data, and will use MEM_freeN() to free this memory
   * when the ImBuf needs to free the data.
   */
  IB_TAKE_OWNERSHIP = 1,
};

struct DDSData {
  /** DDS fourcc info */
  unsigned int fourcc;
  /** The number of mipmaps in the dds file */
  unsigned int nummipmaps;
  /** The compressed image data */
  unsigned char *data;
  /** The size of the compressed data */
  unsigned int size;
  /** Who owns the data buffer. */
  ImBufOwnership ownership;
};

/* Different storage specialization.
 *
 * NOTE: Avoid direct assignments and allocations, use the buffer utilities from the IMB_imbuf.hh
 * instead.
 *
 * Accessing the data pointer directly is fine and is an expected way of accessing it. */

struct ImBufByteBuffer {
  uint8_t *data;
  ImBufOwnership ownership;

  const ColorSpace *colorspace;
};

struct ImBufFloatBuffer {
  float *data;
  ImBufOwnership ownership;

  const ColorSpace *colorspace;
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
  blender::gpu::Texture *texture;
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
  int x, y;

  /** Active amount of bits/bit-planes. */
  unsigned char planes;
  /** Number of channels in `rect_float` (0 = 4 channel default) */
  int channels;

  /* flags */
  /** Controls which components should exist. */
  int flags;

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
  double ppm[2];

  /** Amount of dithering to apply, when converting float -> byte. */
  float dither;

  /* externally used data */
  /** reference index for ImBuf lists */
  int index;
  /** used to set imbuf to dirty and other stuff */
  int userflags;
  /** image metadata */
  IDProperty *metadata;
  /** OpenEXR handle. */
  ExrHandle *exrhandle;

  /* file information */
  /** file type we are going to save as */
  enum eImbFileType ftype;
  /** file format specific flags */
  ImbFormatOptions foptions;
  /** The absolute file path associated with this image. */
  char filepath[IMB_FILEPATH_SIZE];
  /** For movie files, the frame number loaded from the file. */
  int fileframe;

  /** reference counter for multiple users */
  int32_t refcounter;

  /* some parameters to pass along for packing images */
  /** Compressed image only used with PNG and EXR currently. */
  ImBufByteBuffer encoded_buffer;
  /** Size of data written to `encoded_buffer`. */
  unsigned int encoded_size;
  /** Size of `encoded_buffer` */
  unsigned int encoded_buffer_size;

  /* color management */
  /** array of per-display display buffers dirty flags */
  unsigned int *display_buffer_flags;
  /** cache used by color management */
  ColormanageCache *colormanage_cache;
  int colormanage_flag;
  rcti invalid_rect;

  /** Information for compressed textures. */
  DDSData dds_data;
};

/**
 * \brief userflags: Flags used internally by blender for image-buffers.
 */
enum {
  /** image needs to be saved is not the same as filename */
  IB_BITMAPDIRTY = (1 << 1),
  /** float buffer changed, needs recreation of byte rect */
  IB_RECT_INVALID = (1 << 3),
  /** either float or byte buffer changed, need to re-calculate display buffers */
  IB_DISPLAY_BUFFER_INVALID = (1 << 4),
  /** image buffer is persistent in the memory and should never be removed from the cache */
  IB_PERSISTENT = (1 << 5),
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

/* dds */
#ifndef DDS_MAKEFOURCC
#  define DDS_MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((unsigned long)(unsigned char)(ch0) | ((unsigned long)(unsigned char)(ch1) << 8) | \
     ((unsigned long)(unsigned char)(ch2) << 16) | ((unsigned long)(unsigned char)(ch3) << 24))
#endif /* DDS_MAKEFOURCC */

/*
 * FOURCC codes for DX compressed-texture pixel formats.
 */

#define FOURCC_DDS (DDS_MAKEFOURCC('D', 'D', 'S', ' '))
#define FOURCC_DX10 (DDS_MAKEFOURCC('D', 'X', '1', '0'))
#define FOURCC_DXT1 (DDS_MAKEFOURCC('D', 'X', 'T', '1'))
#define FOURCC_DXT2 (DDS_MAKEFOURCC('D', 'X', 'T', '2'))
#define FOURCC_DXT3 (DDS_MAKEFOURCC('D', 'X', 'T', '3'))
#define FOURCC_DXT4 (DDS_MAKEFOURCC('D', 'X', 'T', '4'))
#define FOURCC_DXT5 (DDS_MAKEFOURCC('D', 'X', 'T', '5'))

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
