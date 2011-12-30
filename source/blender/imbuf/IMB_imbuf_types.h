/*
 * IMB_imbuf_types.h (mar-2001 nzc)
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
 *
 *
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
/**
 * \file IMB_imbuf_types.h
 * \ingroup imbuf
 * \brief Contains defines and structs used throughout the imbuf module.
 * \todo Clean up includes.
 */

#ifndef IMB_IMBUF_TYPES_H
#define IMB_IMBUF_TYPES_H

struct ImMetaData;

#define IB_MIPMAP_LEVELS	20
#define IB_FILENAME_SIZE	1023

/**
 * \ingroup imbuf
 * This is the abstraction of an image.  ImBuf is the basic type used for all
 * imbuf operations.
 *
 * REMINDER: if any changes take place, they need to be carried over
 * to source/blender/blenpluginapi/iff.h too, OTHERWISE PLUGINS WON'T
 * WORK CORRECTLY!
 *
 * Also; add new variables to the end to save pain!
 *
 * Also, that iff.h needs to be in the final release "plugins/include" dir, too!
 */
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
	unsigned int *rect;		/* pixel values stored here */
	float *rect_float;		/* floating point Rect equivalent
							Linear RGB color space - may need gamma correction to 
							sRGB when generating 8bit representations */

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
	short profile;				/* color space/profile preset that the byte rect buffer represents */

	/* mipmapping */
	struct ImBuf *mipmap[IB_MIPMAP_LEVELS]; /* MipMap levels, a series of halved images */
	int miptot, miplevel;

	/* externally used data */
	int index;						/* reference index for ImBuf lists */
	int	userflags;					/* used to set imbuf to dirty and other stuff */
	struct ImMetaData *metadata;	/* image metadata */
	void *userdata;					/* temporary storage, only used by baking at the moment */

	/* file information */
	int	ftype;							/* file type we are going to save as */
	char name[IB_FILENAME_SIZE];		/* filename associated with this image */
	char cachename[IB_FILENAME_SIZE];	/* full filename used for reading from cache */

	/* memory cache limiter */
	struct MEM_CacheLimiterHandle_s *c_handle; /* handle for cache limiter */
	int refcounter; /* reference counter for multiple users */

	/* some parameters to pass along for packing images */
	unsigned char *encodedbuffer;     /* Compressed image only used with png currently */
	unsigned int   encodedsize;       /* Size of data written to encodedbuffer */
	unsigned int   encodedbuffersize; /* Size of encodedbuffer */
} ImBuf;

/* Moved from BKE_bmfont_types.h because it is a userflag bit mask. */
/**
 * \brief userflags: Flags used internally by blender for imagebuffers
 */

#define IB_BITMAPFONT		(1 << 0)	/* this image is a font */
#define IB_BITMAPDIRTY		(1 << 1)	/* image needs to be saved is not the same as filename */
#define IB_MIPMAP_INVALID	(1 << 2)	/* image mipmaps are invalid, need recreate */
#define IB_RECT_INVALID		(1 << 3)    /* float buffer changed, needs recreation of byte rect */

/**
 * \name Imbuf Component flags
 * \brief These flags determine the components of an ImBuf struct.
 */
/**@{*/
/** \brief Flag defining the components of the ImBuf struct. */

#define IB_rect				(1 << 0)
#define IB_test				(1 << 1)
#define IB_fields			(1 << 2)
#define IB_zbuf				(1 << 3)
#define IB_mem				(1 << 4)
#define IB_rectfloat		(1 << 5)
#define IB_zbuffloat		(1 << 6)
#define IB_multilayer		(1 << 7)
#define IB_metadata			(1 << 8)
#define IB_animdeinterlace	(1 << 9)
#define IB_tiles			(1 << 10)
#define IB_tilecache		(1 << 11)
#define IB_premul			(1 << 12)
#define IB_cm_predivide		(1 << 13)

/*
 * The bit flag is stored in the ImBuf.ftype variable.
 * Note that the lower 10 bits is used for storing custom flags 
 */
#define PNG				(1 << 30)
#define TGA				(1 << 28)
#define JPG				(1 << 27)
#define BMP				(1 << 26)

#ifdef WITH_QUICKTIME
#define QUICKTIME		(1 << 25)
#endif

#ifdef WITH_HDR
#define RADHDR			(1 << 24)
#endif
#ifdef WITH_TIFF
#define TIF				(1 << 23)
#define TIF_16BIT		(1 << 8 )
#endif

#define OPENEXR			(1 << 22)
#define OPENEXR_HALF	(1 << 8 )
#define OPENEXR_COMPRESS (7)	

#ifdef WITH_CINEON
#define CINEON			(1 << 21)
#define DPX				(1 << 20)
#endif

#ifdef WITH_DDS
#define DDS				(1 << 19)
#endif

#ifdef WITH_OPENJPEG
#define JP2				(1 << 18)
#define JP2_12BIT		(1 << 17)
#define JP2_16BIT		(1 << 16)
#define JP2_YCC			(1 << 15)
#define JP2_CINE		(1 << 14)
#define JP2_CINE_48FPS	(1 << 13) 
#endif

#define RAWTGA	        (TGA | 1)

#define JPG_STD	        (JPG | (0 << 8))
#define JPG_VID	        (JPG | (1 << 8))
#define JPG_JST	        (JPG | (2 << 8))
#define JPG_MAX	        (JPG | (3 << 8))
#define JPG_MSK	        (0xffffff00)

#define IMAGIC			0732

/**
 * \name Imbuf preset profile tags
 * \brief Some predefined color space profiles that 8 bit imbufs can represent
 */
#define IB_PROFILE_NONE			0
#define IB_PROFILE_LINEAR_RGB	1
#define IB_PROFILE_SRGB			2
#define IB_PROFILE_CUSTOM		3

extern const char *imb_ext_image[];
extern const char *imb_ext_image_qt[];
extern const char *imb_ext_movie[];
extern const char *imb_ext_audio[];

#endif
