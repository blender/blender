/* Copyright (c) 1999, Not a Number / NeoGeo b.v. 
 * 
 * All rights reserved.
 * 
 * Contact:      info@blender.org   
 * Information:  http://www.blender.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __IFF_H__
#define __IFF_H__

/** \file blender/blenpluginapi/iff.h
 *  \ingroup blpluginapi
 */

#include <sys/types.h>
#include "util.h"
#include "externdef.h"

struct ImMetaData;

#define IB_MIPMAP_LEVELS	20
#define IB_FILENAME_SIZE	1023

typedef struct ImBuf {
	struct ImBuf *next, *prev;	/**< allow lists of ImBufs, for caches or flipbooks */
	short	x, y;				/**< width and Height of our image buffer */
	unsigned char	depth;		/**< Active amount of bits/bitplanes */
	unsigned int	*rect;		/**< pixel values stored here */
	unsigned int	*crect;		/**< color corrected pixel values stored here */
	int	flags;				/**< Controls which components should exist. */
	int	mall;				/**< what is malloced internal, and can be freed */
	int	*zbuf;				/**< z buffer data, original zbuffer */
	float *zbuf_float;		/**< z buffer data, camera coordinates */
	void *userdata;			/**< temporary storage, only used by baking at the moment */
	unsigned char *encodedbuffer;     /**< Compressed image only used with png currently */
	unsigned int   encodedsize;       /**< Size of data written to encodedbuffer */
	unsigned int   encodedbuffersize; /**< Size of encodedbuffer */

	float *rect_float;		/** < floating point Rect equivalent
							 * Linear RGB color space - may need gamma correction to
							 * sRGB when generating 8bit representations */
	int channels;			/**< amount of channels in rect_float (0 = 4 channel default) */
	float dither;			/**< random dither value, for conversion from float -> byte rect */
	short profile;			/** color space/profile preset that the byte rect buffer represents */
	char profile_filename[1024];		/** to be implemented properly, specific filename for custom profiles */

	/* mipmapping */
	struct ImBuf *mipmap[IB_MIPMAP_LEVELS]; /**< MipMap levels, a series of halved images */
	int miplevels;

	/* externally used flags */
	int index;				/* reference index for ImBuf lists */
	int	userflags;			/* used to set imbuf to dirty and other stuff */
	struct ImMetaData *metadata;

	/* file information */
	int	ftype;						/* file type we are going to save as */
	char name[IB_FILENAME_SIZE];	/* filename associated with this image */

	/* memory cache limiter */
	struct MEM_CacheLimiterHandle_s *c_handle; /* handle for cache limiter */
	int refcounter; /* reference counter for multiple users */
} ImBuf;

LIBIMPORT struct ImBuf *allocImBuf(short, short, uchar, uint);
LIBIMPORT struct ImBuf *dupImBuf(struct ImBuf *);
LIBIMPORT void freeImBuf(struct ImBuf*);

LIBIMPORT short saveiff(struct ImBuf *, char *, int);

LIBIMPORT struct ImBuf *loadifffile(int, int);
LIBIMPORT struct ImBuf *loadiffname(char *, int);
LIBIMPORT struct ImBuf *testiffname(char *, int);

LIBIMPORT struct ImBuf *onehalf(struct ImBuf *);
LIBIMPORT struct ImBuf *half_x(struct ImBuf *);
LIBIMPORT struct ImBuf *half_y(struct ImBuf *);
LIBIMPORT struct ImBuf *double_x(struct ImBuf *);
LIBIMPORT struct ImBuf *double_y(struct ImBuf *);
LIBIMPORT struct ImBuf *double_fast_x(struct ImBuf *);
LIBIMPORT struct ImBuf *double_fast_y(struct ImBuf *);

LIBIMPORT int ispic(char *);

LIBIMPORT struct ImBuf *scaleImBuf(struct ImBuf *, short, short);
LIBIMPORT struct ImBuf *scalefastImBuf(struct ImBuf *, short, short);

LIBIMPORT void de_interlace(struct ImBuf *ib);
LIBIMPORT void interlace(struct ImBuf *ib);

LIBIMPORT void IMB_rectcpy(struct ImBuf *dbuf, struct ImBuf *sbuf, 
	int destx, int desty, int srcx, int srcy, int width, int height);

LIBIMPORT void IMB_rectfill(struct ImBuf *drect, const float col[4]);
LIBIMPORT void IMB_rectfill_area(struct ImBuf *ibuf, float *col, int x1, int y1, int x2, int y2);
LIBIMPORT void buf_rectfill_area(unsigned char *rect, float *rectf, int width, int height, const float col[4], int x1, int y1, int x2, int y2);
LIBIMPORT void IMB_rectfill_alpha(struct ImBuf *drect, const float value);

#endif /* __IFF_H__ */

