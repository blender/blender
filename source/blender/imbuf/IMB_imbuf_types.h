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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

#ifndef __IMB_IMBUF_TYPES_H__
#define __IMB_IMBUF_TYPES_H__

#include "DNA_vec_types.h" /* for rcti */

/** \file
 * \ingroup imbuf
 * \brief Contains defines and structs used throughout the imbuf module.
 * \todo Clean up includes.
 *
 * Types needed for using the image buffer.
 *
 * Imbuf is external code, slightly adapted to live in the Blender
 * context. It requires an external jpeg module, and the avi-module
 * (also external code) in order to function correctly.
 *
 * This file contains types and some constants that go with them. Most
 * are self-explanatory (e.g. IS_amiga tests whether the buffer
 * contains an Amiga-format file).
 */

#define IMB_MIPMAP_LEVELS 20
#define IMB_FILENAME_SIZE 1024

typedef struct DDSData {
  /** DDS fourcc info */
  unsigned int fourcc;
  /** The number of mipmaps in the dds file */
  unsigned int nummipmaps;
  /** The compressed image data */
  unsigned char *data;
  /** The size of the compressed data */
  unsigned int size;
} DDSData;

/**
 * \ingroup imbuf
 * This is the abstraction of an image.  ImBuf is the basic type used for all
 * imbuf operations.
 *
 * Also; add new variables to the end to save pain!
 */

/* Warning: Keep explicit value assignments here,
 * this file is included in areas where not all format defines are set
 * (e.g. intern/dds only get WITH_DDS, even if TIFF, HDR etc are also defined).
 * See T46524. */

/** #ImBuf.ftype flag, main image types. */
enum eImbTypes {
  IMB_FTYPE_PNG = 1,
  IMB_FTYPE_TGA = 2,
  IMB_FTYPE_JPG = 3,
  IMB_FTYPE_BMP = 4,
  IMB_FTYPE_OPENEXR = 5,
  IMB_FTYPE_IMAGIC = 6,
#ifdef WITH_OPENIMAGEIO
  IMB_FTYPE_PSD = 7,
#endif
#ifdef WITH_OPENJPEG
  IMB_FTYPE_JP2 = 8,
#endif
#ifdef WITH_HDR
  IMB_FTYPE_RADHDR = 9,
#endif
#ifdef WITH_TIFF
  IMB_FTYPE_TIF = 10,
#endif
#ifdef WITH_CINEON
  IMB_FTYPE_CINEON = 11,
  IMB_FTYPE_DPX = 12,
#endif

#ifdef WITH_DDS
  IMB_FTYPE_DDS = 13,
#endif
};

/* ibuf->foptions flag, type specific options.
 * Some formats include compression rations on some bits */

#define OPENEXR_HALF (1 << 8)
/* careful changing this, it's used in DNA as well */
#define OPENEXR_COMPRESS (15)

#ifdef WITH_CINEON
#  define CINEON_LOG (1 << 8)
#  define CINEON_16BIT (1 << 7)
#  define CINEON_12BIT (1 << 6)
#  define CINEON_10BIT (1 << 5)
#endif

#ifdef WITH_OPENJPEG
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

#ifdef WITH_TIFF
#  define TIF_16BIT (1 << 8)
#  define TIF_COMPRESS_NONE (1 << 7)
#  define TIF_COMPRESS_DEFLATE (1 << 6)
#  define TIF_COMPRESS_LZW (1 << 5)
#  define TIF_COMPRESS_PACKBITS (1 << 4)
#endif

typedef struct ImbFormatOptions {
  short flag;
  /** quality serves dual purpose as quality number for jpeg or compression amount for png */
  char quality;
} ImbFormatOptions;

typedef struct ImBuf {
  struct ImBuf *next, *prev; /**< allow lists of ImBufs, for caches or flipbooks */

  /* dimensions */
  /** Width and Height of our image buffer.
   * Should be 'unsigned int' since most formats use this.
   * but this is problematic with texture math in imagetexture.c
   * avoid problems and use int. - campbell */
  int x, y;

  /** Active amount of bits/bitplanes */
  unsigned char planes;
  /** Number of channels in `rect_float` (0 = 4 channel default) */
  int channels;

  /* flags */
  /** Controls which components should exist. */
  int flags;
  /** what is malloced internal, and can be freed */
  int mall;

  /* pixels */

  /** Image pixel buffer (8bit representation):
   * - color space defaults to `sRGB`.
   * - alpha defaults to 'straight'.
   */
  unsigned int *rect;
  /** Image pixel buffer (float representation):
   * - color space defaults to 'linear' (`rec709`).
   * - alpha defaults to 'premul'.
   * \note May need gamma correction to `sRGB` when generating 8bit representations.
   * \note Formats that support higher more than 8 but channels load as floats.
   */
  float *rect_float;

  /* resolution - pixels per meter */
  double ppm[2];

  /* tiled pixel storage */
  int tilex, tiley;
  int xtiles, ytiles;
  unsigned int **tiles;

  /* zbuffer */
  /** z buffer data, original zbuffer */
  int *zbuf;
  /** z buffer data, camera coordinates */
  float *zbuf_float;

  /* parameters used by conversion between byte and float */
  /** random dither value, for conversion from float -> byte rect */
  float dither;

  /* mipmapping */
  /** MipMap levels, a series of halved images */
  struct ImBuf *mipmap[IMB_MIPMAP_LEVELS];
  int miptot, miplevel;

  /* externally used data */
  /** reference index for ImBuf lists */
  int index;
  /** used to set imbuf to dirty and other stuff */
  int userflags;
  /** image metadata */
  struct IDProperty *metadata;
  /** temporary storage */
  void *userdata;

  /* file information */
  /** file type we are going to save as */
  enum eImbTypes ftype;
  /** file format specific flags */
  ImbFormatOptions foptions;
  /** filename associated with this image */
  char name[IMB_FILENAME_SIZE];
  /** full filename used for reading from cache */
  char cachename[IMB_FILENAME_SIZE];

  /* memory cache limiter */
  /** handle for cache limiter */
  struct MEM_CacheLimiterHandle_s *c_handle;
  /** reference counter for multiple users */
  int refcounter;

  /* some parameters to pass along for packing images */
  /** Compressed image only used with png and exr currently */
  unsigned char *encodedbuffer;
  /** Size of data written to encodedbuffer */
  unsigned int encodedsize;
  /** Size of encodedbuffer */
  unsigned int encodedbuffersize;

  /* color management */
  /** color space of byte buffer */
  struct ColorSpace *rect_colorspace;
  /** color space of float buffer, used by sequencer only */
  struct ColorSpace *float_colorspace;
  /** array of per-display display buffers dirty flags */
  unsigned int *display_buffer_flags;
  /** cache used by color management */
  struct ColormanageCache *colormanage_cache;
  int colormanage_flag;
  rcti invalid_rect;

  /* information for compressed textures */
  struct DDSData dds_data;
} ImBuf;

/**
 * \brief userflags: Flags used internally by blender for imagebuffers
 */

enum {
  /** image needs to be saved is not the same as filename */
  IB_BITMAPDIRTY = (1 << 1),
  /** image mipmaps are invalid, need recreate */
  IB_MIPMAP_INVALID = (1 << 2),
  /** float buffer changed, needs recreation of byte rect */
  IB_RECT_INVALID = (1 << 3),
  /** either float or byte buffer changed, need to re-calculate display buffers */
  IB_DISPLAY_BUFFER_INVALID = (1 << 4),
  /** image buffer is persistent in the memory and should never be removed from the cache */
  IB_PERSISTENT = (1 << 5),
};

/**
 * \name Imbuf Component flags
 * \brief These flags determine the components of an ImBuf struct.
 *
 * \{ */

enum {
  IB_rect = 1 << 0,
  IB_test = 1 << 1,
  IB_zbuf = 1 << 3,
  IB_mem = 1 << 4,
  IB_rectfloat = 1 << 5,
  IB_zbuffloat = 1 << 6,
  IB_multilayer = 1 << 7,
  IB_metadata = 1 << 8,
  IB_animdeinterlace = 1 << 9,
  IB_tiles = 1 << 10,
  IB_tilecache = 1 << 11,
  /** indicates whether image on disk have premul alpha */
  IB_alphamode_premul = 1 << 12,
  /** if this flag is set, alpha mode would be guessed from file */
  IB_alphamode_detect = 1 << 13,
  /* alpha channel is unrelated to RGB and should not affect it */
  IB_alphamode_channel_packed = 1 << 14,
  /** ignore alpha on load and substitute it with 1.0f */
  IB_alphamode_ignore = 1 << 15,
  IB_thumbnail = 1 << 16,
  IB_multiview = 1 << 17,
};

/** \} */

/**
 * \name Imbuf preset profile tags
 * \brief Some predefined color space profiles that 8 bit imbufs can represent
 *
 * \{ */
#define IB_PROFILE_NONE 0
#define IB_PROFILE_LINEAR_RGB 1
#define IB_PROFILE_SRGB 2
#define IB_PROFILE_CUSTOM 3

/** \} */

/* dds */
#ifdef WITH_DDS
#  ifndef DDS_MAKEFOURCC
#    define DDS_MAKEFOURCC(ch0, ch1, ch2, ch3) \
      ((unsigned long)(unsigned char)(ch0) | ((unsigned long)(unsigned char)(ch1) << 8) | \
       ((unsigned long)(unsigned char)(ch2) << 16) | ((unsigned long)(unsigned char)(ch3) << 24))
#  endif /* DDS_MAKEFOURCC */

/*
 * FOURCC codes for DX compressed-texture pixel formats
 */

#  define FOURCC_DDS (DDS_MAKEFOURCC('D', 'D', 'S', ' '))
#  define FOURCC_DXT1 (DDS_MAKEFOURCC('D', 'X', 'T', '1'))
#  define FOURCC_DXT2 (DDS_MAKEFOURCC('D', 'X', 'T', '2'))
#  define FOURCC_DXT3 (DDS_MAKEFOURCC('D', 'X', 'T', '3'))
#  define FOURCC_DXT4 (DDS_MAKEFOURCC('D', 'X', 'T', '4'))
#  define FOURCC_DXT5 (DDS_MAKEFOURCC('D', 'X', 'T', '5'))

#endif /* DDS */
extern const char *imb_ext_image[];
extern const char *imb_ext_movie[];
extern const char *imb_ext_audio[];

/* image formats that can only be loaded via filepath */
extern const char *imb_ext_image_filepath_only[];

/**
 * \name Imbuf Color Management Flag
 * \brief Used with #ImBuf.colormanage_flag
 *
 * \{ */

enum {
  IMB_COLORMANAGE_IS_DATA = (1 << 0),
};

/** \} */

#endif /* __IMB_IMBUF_TYPES_H__ */
