/**
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
 * $Id$ 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef IMB_IMBUF_TYPES_H
#define IMB_IMBUF_TYPES_H

#include <stdio.h>        /* for size_t */
#include "DNA_listBase.h" /* for ListBase */
struct _AviMovie;
struct Mdec;

/**
 * \brief The basic imbuf type
 * \ingroup imbuf
 * This is the abstraction of an image.  ImBuf is the basic type used for all
 * imbuf operations.
 *
 */
typedef struct ImBuf{
	short	x;		/**< Width in pixels */
	short	y;		/**< Height in scanlines */
	short	skipx;		/**< Width in ints to get to the next scanline */
	unsigned char	depth;		/**< Active amount of bits/bitplanes */
	unsigned char	cbits;		/**< Amount of active bits in cmap */
	unsigned short	mincol;		/**< First active color?*/
	unsigned short	maxcol;		/**< Last active color?*/
	int	type;		/**< 0=abgr, 1=bitplanes */
	int	ftype;		/**< File type */
	unsigned int	*cmap;		/**< Color map data. */
	unsigned int	*rect;		/**< databuffer */
	unsigned int	**planes;	/**< bitplanes */
	int	flags;		/**< Controls which components should exist. */
	int	mall;		/**< what is malloced internal, and can be freed */
	short	xorig;		/**< X origin.  What is this relative to? */
	short	yorig;		/**< Y origin.  What is this relative to? */
	char	name[1023];		/**< The file name */
	char	namenull;		/**< What does this do?*/
	int	userflags;		/**< What does this do? Holds an enum ImBuf_userflagsMask?*/
	int	*zbuf;		/**< A z buffer */
	void *userdata;		/**< What does this do?*/
	unsigned char *encodedbuffer; 		/** What is an encoded buffer? */
	unsigned int   encodedsize;		/** What is an encoded buffer? */
	unsigned int   encodedbuffersize;		/** What is an encoded buffer? */
} ImBuf;

/* Moved from BKE_bmfont_types.h because it is a userflag bit mask. */
/**
 * \brief Flags for the user?
 */
typedef enum {
	IB_BITMAPFONT = 1 << 0,
	IB_BITMAPDIRTY = 1 << 1
} ImBuf_userflagsMask;


/* From iff.h. This was once moved away by Frank, now Nzc moves it
 * back. Such is the way it is... It is a long list of defines, and
 * there are a few external defines in the back. Most of the stuff is
 * probably imbuf_intern only. This will need to be merged later
 * on. */

/**
 * \brief Rectangle flag
 */
#define IB_rect			(1 << 0)
/**
 * \brief Bitmap Planes flag
 */
#define IB_planes		(1 << 1)
/**
 * \brief Color map flag
 */
#define IB_cmap			(1 << 2)

/**
 * \brief Vertex flag
 */
#define IB_vert			(1 << 4)
/**
 * \brief Free Memory flag
 */
#define IB_freem		(1 << 6)
/**
 * \brief Test flag
 */
#define IB_test			(1 << 7)

/**
 * \brief True Type object??
 */
#define IB_ttob			(1 << 8)
#define IB_subdlta		(1 << 9)
/**
 * \brief Video fields flag
 */
#define IB_fields		(1 << 11)
/**
 * \brief Zbuffer flag
 */
#define IB_zbuf			(1 << 13)

/**
 * \brief Memory flag?
 */
#define IB_mem			(1 << 14)

/**
 * \brief .ami (amiga) filetype
 */
#define AMI	    (1 << 31)
/**
 * \brief .png filetype
 */
#define PNG	    (1 << 30)
/**
 * \brief .??? (Anim) filetype
 */
#define Anim	(1 << 29)
/**
 * \brief .tga (targa) filetype
 */
#define TGA	    (1 << 28)
/**
 * \brief .jpg (JPEG) filetype
 */
#define JPG		(1 << 27)
/**
 * \brief .bmp filetype
 */
#define BMP		(1 << 26)
#ifdef WITH_QUICKTIME
/**
 * \brief .mov? (Quicktime) filetype
 */
#define QUICKTIME	(1 << 25)
#endif
#ifdef WITH_FREEIMAGE
/**
 * \brief .??? (Freeimage) filetype
 */
#define FREEIMAGE	(1 << 24)
#endif
#ifdef WITH_IMAGEMAGICK
/**
 * \brief .im? (ImageMagick) filetype
 */
#define IMAGEMAGICK	(1 << 23)
#endif

/**
 * \brief tga of type "raw"
 */
#define RAWTGA	(TGA | 1)

/**
 * \brief jpg of type "standard"?
 */
#define JPG_STD	(JPG | (0 << 8))
/**
 * \brief jpg of type "video"?
 */
#define JPG_VID	(JPG | (1 << 8))
/**
 * \brief jpg of type "jst"?
 */
#define JPG_JST	(JPG | (2 << 8))
/**
 * \brief jpg of type "max"?
 */
#define JPG_MAX	(JPG | (3 << 8))
/**
 * \brief Masks off the last two bytes.
 */
#define JPG_MSK	(0xffffff00)

/**
 * \brief .ham? Anim filetype
 */
#define AM_ham	    (0x0800 | AMI)
/**
 * \brief .??? Anim filetype
 */
#define AM_hbrite   (0x0080 | AMI)

/**
 * \brief c233 type for Anim filetype
 */
#define C233	1
/**
 * \brief c233 type for Anim filetype
 */
#define YUVX	2
/**
 * \brief c233 type for Anim filetype
 */
#define HAMX	3
/**
 * \brief c233 type for Anim filetype
 */
#define TANX	4

/**
 * \brief Anim file of type c233
 */
#define AN_c233			(Anim | C233)
/**
 * \brief Anim file of type YUVX
 */
#define AN_yuvx			(Anim | YUVX)
/**
 * \brief Anim file of type HAMX
 */
#define AN_hamx			(Anim | HAMX)
/**
 * \brief Anim file of type TANX
 */
#define AN_tanx			(Anim | TANX)

/**
 * \brief Tests if an ImBuf is an Amiga file.
 * \param x Must be an ImBuf*
 */
#define IS_amiga(x)		(x->ftype & AMI)
/**
 * \brief Tests if an ImBuf is a ham file.
 * \param x Must be an ImBuf*
 */
#define IS_ham(x)		((x->ftype & AM_ham) == AM_ham)
/**
 * \brief Tests if an ImBuf is an hbrite file. 
 * \param x Must be an ImBuf*
 */
#define IS_hbrite(x)	((x->ftype & AM_hbrite) == AM_hbrite)

/**
 * \brief Tests if an ImBuf is an Anim file.
 * \param x Must be an ImBuf*
 */
#define IS_anim(x)		(x->ftype & Anim)
/**
 * \brief Tests if an ImBuf is an Anim hamx.
 * \param x Must be an ImBuf*
 */
#define IS_hamx(x)		(x->ftype == AN_hamx)
/**
 * \brief Tests if an ImBuf is a tga file.
 * \param x Must be an ImBuf*
 */
#define IS_tga(x)		(x->ftype & TGA)
/**
 * \brief Tests if an ImBuf is a png file.
 * \param x Must be an ImBuf*
 */
#define IS_png(x)		(x->ftype & PNG)
/**
 * \brief Tests if an ImBuf is a bmp file.
 * \param x Must be an ImBuf*
 */
#define IS_bmp(x)		(x->ftype & BMP)

/**
 * \brief Iris filetype.
 */
#define IMAGIC 	0732
/**
 * \brief Tests if an ImBuf is an Iris file.
 * \param x Must be an ImBuf*
 */
#define IS_iris(x)		(x->ftype == IMAGIC)

/**
 * \brief Tests if an ImBuf is a JPEG file.
 * \param x Must be an ImBuf*
 */
#define IS_jpg(x)		(x->ftype & JPG)
/**
 * \brief Tests if an ImBuf is a standard JPEG file.
 * \param x Must be an ImBuf*
 */
#define IS_stdjpg(x)	((x->ftype & JPG_MSK) == JPG_STD)
/**
 * \brief Tests if an ImBuf is a video JPEG file.
 * \param x Must be an ImBuf*
 */
#define IS_vidjpg(x)	((x->ftype & JPG_MSK) == JPG_VID)
/**
 * \brief Tests if an ImBuf is a jst JPEG file.
 * \param x Must be an ImBuf*
 */
#define IS_jstjpg(x)	((x->ftype & JPG_MSK) == JPG_JST)
/**
 * \brief Tests if an ImBuf is a max JPEG file.
 * \param x Must be an ImBuf*
 */
#define IS_maxjpg(x)	((x->ftype & JPG_MSK) == JPG_MAX)

#endif


