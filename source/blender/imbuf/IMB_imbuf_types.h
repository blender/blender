/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __IMB_IMBUF_TYPES_H__
#define __IMB_IMBUF_TYPES_H__

#include "DNA_vec_types.h"  /* for rcti */

/**
 * \file IMB_imbuf_types.h
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

#define IMB_MIPMAP_LEVELS	20
#define IMB_FILENAME_SIZE	1024

typedef struct DDSData {
	unsigned int fourcc; /* DDS fourcc info */
	unsigned int nummipmaps; /* The number of mipmaps in the dds file */
	unsigned char *data; /* The compressed image data */
	unsigned int size; /* The size of the compressed data */
} DDSData;

/**
 * \ingroup imbuf
 * This is the abstraction of an image.  ImBuf is the basic type used for all
 * imbuf operations.
 *
 * Also; add new variables to the end to save pain!
 *
 */

/* ibuf->ftype flag, main image types */
/* Warning: Keep explicit value assignements here, this file is included in areas where not all format defines
 *          are set (e.g. intern/dds only get WITH_DDS, even if TIFF, HDR etc are also defined). See T46524. */
enum eImbTypes {
	IMB_FTYPE_PNG       = 1,
	IMB_FTYPE_TGA       = 2,
	IMB_FTYPE_JPG       = 3,
	IMB_FTYPE_BMP       = 4,
	IMB_FTYPE_OPENEXR   = 5,
	IMB_FTYPE_IMAGIC    = 6,
#ifdef WITH_OPENIMAGEIO
	IMB_FTYPE_PSD       = 7,
#endif
#ifdef WITH_OPENJPEG
	IMB_FTYPE_JP2       = 8,
#endif
#ifdef WITH_HDR
	IMB_FTYPE_RADHDR    = 9,
#endif
#ifdef WITH_TIFF
	IMB_FTYPE_TIF       = 10,
#endif
#ifdef WITH_CINEON
	IMB_FTYPE_CINEON    = 11,
	IMB_FTYPE_DPX       = 12,
#endif

#ifdef WITH_DDS
	IMB_FTYPE_DDS       = 13,
#endif
};

/* ibuf->foptions flag, type specific options.
 * Some formats include compression rations on some bits */

#define OPENEXR_HALF	(1 << 8 )
/* careful changing this, it's used in DNA as well */
#define OPENEXR_COMPRESS (15)

#ifdef WITH_CINEON
#define CINEON_LOG		(1 << 8)
#define CINEON_16BIT	(1 << 7)
#define CINEON_12BIT	(1 << 6)
#define CINEON_10BIT	(1 << 5)
#endif

#ifdef WITH_OPENJPEG
#define JP2_12BIT		(1 << 9)
#define JP2_16BIT		(1 << 8)
#define JP2_YCC			(1 << 7)
#define JP2_CINE		(1 << 6)
#define JP2_CINE_48FPS	(1 << 5)
#define JP2_JP2	(1 << 4)
#define JP2_J2K	(1 << 3)
#endif

#define PNG_16BIT			(1 << 10)

#define RAWTGA	        1

#ifdef WITH_TIFF
#define TIF_16BIT			(1 << 8)
#define TIF_COMPRESS_NONE		(1 << 7)
#define TIF_COMPRESS_DEFLATE		(1 << 6)
#define TIF_COMPRESS_LZW		(1 << 5)
#define TIF_COMPRESS_PACKBITS		(1 << 4)
#endif

typedef struct ImbFormatOptions {
	short flag;
	char quality; /* quality serves dual purpose as quality number for jpeg or compression amount for png */
} ImbFormatOptions;

typedef struct ImBuf {
	struct ImBuf *next, *prev;	/**< allow lists of ImBufs, for caches or flipbooks */

	/* dimensions */
	int x, y;				/* width and Height of our image buffer.
							 * Should be 'unsigned int' since most formats use this.
							 * but this is problematic with texture math in imagetexture.c
							 * avoid problems and use int. - campbell */

	unsigned char planes;	/* Active amount of bits/bitplanes */
	int channels;			/* amount of channels in rect_float (0 = 4 channel default) */

	/* flags */
	int	flags;				/* Controls which components should exist. */
	int	mall;				/* what is malloced internal, and can be freed */

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
	int	*zbuf;				/* z buffer data, original zbuffer */
	float *zbuf_float;		/* z buffer data, camera coordinates */

	/* parameters used by conversion between byte and float */
	float dither;				/* random dither value, for conversion from float -> byte rect */

	/* mipmapping */
	struct ImBuf *mipmap[IMB_MIPMAP_LEVELS]; /* MipMap levels, a series of halved images */
	int miptot, miplevel;

	/* externally used data */
	int index;						/* reference index for ImBuf lists */
	int	userflags;					/* used to set imbuf to dirty and other stuff */
	struct IDProperty *metadata;	/* image metadata */
	void *userdata;					/* temporary storage */

	/* file information */
	enum eImbTypes	ftype;				/* file type we are going to save as */
	ImbFormatOptions foptions;			/* file format specific flags */
	char name[IMB_FILENAME_SIZE];		/* filename associated with this image */
	char cachename[IMB_FILENAME_SIZE];	/* full filename used for reading from cache */

	/* memory cache limiter */
	struct MEM_CacheLimiterHandle_s *c_handle; /* handle for cache limiter */
	int refcounter; /* reference counter for multiple users */

	/* some parameters to pass along for packing images */
	unsigned char *encodedbuffer;     /* Compressed image only used with png currently */
	unsigned int   encodedsize;       /* Size of data written to encodedbuffer */
	unsigned int   encodedbuffersize; /* Size of encodedbuffer */

	/* color management */
	struct ColorSpace *rect_colorspace;          /* color space of byte buffer */
	struct ColorSpace *float_colorspace;         /* color space of float buffer, used by sequencer only */
	unsigned int *display_buffer_flags;          /* array of per-display display buffers dirty flags */
	struct ColormanageCache *colormanage_cache;  /* cache used by color management */
	int colormanage_flag;
	rcti invalid_rect;

	/* information for compressed textures */
	struct DDSData dds_data;
} ImBuf;

/* Moved from BKE_bmfont_types.h because it is a userflag bit mask. */
/**
 * \brief userflags: Flags used internally by blender for imagebuffers
 */

#define IB_BITMAPFONT			(1 << 0)	/* this image is a font */
#define IB_BITMAPDIRTY			(1 << 1)	/* image needs to be saved is not the same as filename */
#define IB_MIPMAP_INVALID		(1 << 2)	/* image mipmaps are invalid, need recreate */
#define IB_RECT_INVALID			(1 << 3)	/* float buffer changed, needs recreation of byte rect */
#define IB_DISPLAY_BUFFER_INVALID	(1 << 4)	/* either float or byte buffer changed, need to re-calculate display buffers */
#define IB_PERSISTENT				(1 << 5)	/* image buffer is persistent in the memory and should never be removed from the cache */

/**
 * \name Imbuf Component flags
 * \brief These flags determine the components of an ImBuf struct.
 *
 * \{ */

#define IB_rect				(1 << 0)
#define IB_test				(1 << 1)
#define IB_zbuf				(1 << 3)
#define IB_mem				(1 << 4)
#define IB_rectfloat		(1 << 5)
#define IB_zbuffloat		(1 << 6)
#define IB_multilayer		(1 << 7)
#define IB_metadata			(1 << 8)
#define IB_animdeinterlace	(1 << 9)
#define IB_tiles			(1 << 10)
#define IB_tilecache		(1 << 11)
#define IB_alphamode_premul	(1 << 12)  /* indicates whether image on disk have premul alpha */
#define IB_alphamode_detect	(1 << 13)  /* if this flag is set, alpha mode would be guessed from file */
#define IB_ignore_alpha		(1 << 14)  /* ignore alpha on load and substitude it with 1.0f */
#define IB_thumbnail		(1 << 15)
#define IB_multiview		(1 << 16)

/** \} */

/**
 * \name Imbuf preset profile tags
 * \brief Some predefined color space profiles that 8 bit imbufs can represent
 *
 * \{ */
#define IB_PROFILE_NONE			0
#define IB_PROFILE_LINEAR_RGB	1
#define IB_PROFILE_SRGB			2
#define IB_PROFILE_CUSTOM		3

/** \} */

/* dds */
#ifdef WITH_DDS
#ifndef DDS_MAKEFOURCC
#define DDS_MAKEFOURCC(ch0, ch1, ch2, ch3)\
	((unsigned long)(unsigned char)(ch0) | \
	((unsigned long)(unsigned char)(ch1) << 8) | \
	((unsigned long)(unsigned char)(ch2) << 16) | \
	((unsigned long)(unsigned char)(ch3) << 24))
#endif  /* DDS_MAKEFOURCC */

/*
 * FOURCC codes for DX compressed-texture pixel formats
 */

#define FOURCC_DDS   (DDS_MAKEFOURCC('D','D','S',' '))
#define FOURCC_DXT1  (DDS_MAKEFOURCC('D','X','T','1'))
#define FOURCC_DXT2  (DDS_MAKEFOURCC('D','X','T','2'))
#define FOURCC_DXT3  (DDS_MAKEFOURCC('D','X','T','3'))
#define FOURCC_DXT4  (DDS_MAKEFOURCC('D','X','T','4'))
#define FOURCC_DXT5  (DDS_MAKEFOURCC('D','X','T','5'))

#endif  /* DDS */
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
	IMB_COLORMANAGE_IS_DATA = (1 << 0)
};

/** \} */

#endif  /* __IMB_IMBUF_TYPES_H__ */
