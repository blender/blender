/* Copyright (c) 1999, Not a Number / NeoGeo b.v. 
 * $Id$
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

#ifndef IFF_H
#define IFF_H

#include <sys/types.h>
#include "util.h"
#include "externdef.h"

#define IB_rect			(1 << 0)
#define IB_planes		(1 << 1)
#define IB_cmap			(1 << 2)
#define IB_test			(1 << 7)

#define IB_fields		(1 << 11)
#define IB_yuv			(1 << 12)
#define IB_zbuf			(1 << 13)
#define IB_rgba			(1 << 14)

#define AMI             (1 << 31)
#define PNG             (1 << 30)
#define Anim            (1 << 29)
#define TGA             (1 << 28)
#define JPG             (1 << 27)
#define BMP             (1 << 26)
#ifdef WITH_QUICKTIME
#define QUICKTIME       (1 << 25)
#endif
#define RADHDR  (1<<24)

#define RAWTGA	(TGA | 1)

#define JPG_STD	(JPG | (0 << 8))
#define JPG_VID	(JPG | (1 << 8))
#define JPG_JST	(JPG | (2 << 8))
#define JPG_MAX	(JPG | (3 << 8))
#define JPG_MSK	(0xffffff00)

#define AM_ham	    (0x0800 | AMI)
#define AM_hbrite   (0x0080 | AMI)
#define AM_lace	    (0x0004 | AMI)
#define AM_hires    (0x8000 | AMI)
#define AM_hblace   (AM_hbrite | AM_lace)
#define AM_hilace   (AM_hires | AM_lace)
#define AM_hamlace  (AM_ham | AM_lace)

#define RGB888	1
#define RGB555	2
#define DYUV	3
#define CLUT8	4
#define CLUT7	5
#define CLUT4	6
#define CLUT3	7
#define RL7	8
#define RL3	9
#define MPLTE	10

#define DYUV1	0
#define C233	1
#define YUVX	2
#define HAMX	3
#define TANX	4

#define AN_c233			(Anim | C233)
#define AN_yuvx			(Anim | YUVX)
#define AN_hamx			(Anim | HAMX)
#define AN_tanx			(Anim | TANX)

#define IS_amiga(x)		(x->ftype & AMI)
#define IS_ham(x)		((x->ftype & AM_ham) == AM_ham)
#define IS_hbrite(x)	((x->ftype & AM_hbrite) == AM_hbrite)

#define IS_lace(x)		((x->ftype & AM_lace) == AM_lace)
#define IS_hires(x)		((x->ftype & AM_hires) == AM_hires)
#define IS_hblace(x)	((x->ftype & AM_hblace) == AM_hblace)
#define IS_hilace(x)	((x->ftype & AM_hilace) == AM_hilace)
#define IS_hamlace(x)	((x->ftype & AM_hamlace) == AM_hamlace)

#define IS_anim(x)		(x->ftype & Anim)
#define IS_hamx(x)		(x->ftype == AN_hamx)
#define IS_tga(x)		(x->ftype & TGA)
#define IS_png(x)               (x->ftype & PNG)
#define IS_bmp(x)               (x->ftype & BMP)
#define IS_radhdr(x)    	(x->ftype & RADHDR)
#define IS_tim(x)		(x->ftype & TIM)
#define IS_tiff(x)		(x->ftype & TIFF)
#define IS_openexr(x)           (x->ftype & OPENEXR)


#define IMAGIC 	0732
#define IS_iris(x)		(x->ftype == IMAGIC)

#define IS_jpg(x)		(x->ftype & JPG)
#define IS_stdjpg(x)	((x->ftype & JPG_MSK) == JPG_STD)
#define IS_vidjpg(x)	((x->ftype & JPG_MSK) == JPG_VID)
#define IS_jstjpg(x)	((x->ftype & JPG_MSK) == JPG_JST)
#define IS_maxjpg(x)	((x->ftype & JPG_MSK) == JPG_MAX)

#define AN_INIT an_stringdec = stringdec; an_stringenc = stringenc;

#define IB_MIPMAP_LEVELS	10

struct MEM_CacheLimiterHandle_s;

typedef struct ImBuf {
	struct ImBuf *next, *prev;	/**< allow lists of ImBufs, for caches or flipbooks */
	short	x, y;				/**< width and Height of our image buffer */
	short	skipx;				/**< Width in ints to get to the next scanline */
	unsigned char	depth;		/**< Active amount of bits/bitplanes */
	unsigned char	cbits;		/**< Amount of active bits in cmap */
	unsigned short	mincol;		/**< smallest color in colormap */
	unsigned short	maxcol;		/**< Largest color in colormap */
	int	type;					/**< 0=abgr, 1=bitplanes */
	int	ftype;					/**< File type we are going to save as */
	unsigned int	*cmap;		/**< Color map data. */
	unsigned int	*rect;		/**< pixel values stored here */
	unsigned int	**planes;	/**< bitplanes */
	int	flags;				/**< Controls which components should exist. */
	int	mall;				/**< what is malloced internal, and can be freed */
	short	xorig, yorig;		/**< Cordinates of first pixel of an image used in some formats (example: targa) */
	char	name[1023];		/**< The file name assocated with this image */
	char	namenull;		/**< Unused don't want to remove it thought messes things up */
	int	userflags;			/**< Used to set imbuf to Dirty and other stuff */
	int	*zbuf;				/**< z buffer data, original zbuffer */
	float *zbuf_float;		/**< z buffer data, camera coordinates */
	void *userdata;	
	unsigned char *encodedbuffer;     /**< Compressed image only used with png currently */
	unsigned int   encodedsize;       /**< Size of data written to encodedbuffer */
	unsigned int   encodedbuffersize; /**< Size of encodedbuffer */

	float *rect_float;		/**< floating point Rect equivilant */
	int channels;			/**< amount of channels in rect_float (0 = 4 channel default) */
	float dither;			/**< random dither value, for conversion from float -> byte rect */
	
	struct MEM_CacheLimiterHandle_s * c_handle; /**< handle for cache limiter */
	int refcounter;			/**< Refcounter for multiple users */
	int index;				/**< reference index for ImBuf lists */
	
	struct ImBuf *mipmap[IB_MIPMAP_LEVELS]; /**< MipMap levels, a series of halved images */
} ImBuf;

LIBIMPORT struct ImBuf *allocImBuf(short,short,uchar,uint,uchar);
LIBIMPORT struct ImBuf *dupImBuf(struct ImBuf *);
LIBIMPORT void freeImBuf(struct ImBuf*);

LIBIMPORT short converttocmap(struct ImBuf* ibuf);

LIBIMPORT short saveiff(struct ImBuf *,char *,int);

LIBIMPORT struct ImBuf *loadiffmem(int *,int);
LIBIMPORT struct ImBuf *loadifffile(int,int);
LIBIMPORT struct ImBuf *loadiffname(char *,int);
LIBIMPORT struct ImBuf *testiffname(char *,int);

LIBIMPORT struct ImBuf *onehalf(struct ImBuf *);
LIBIMPORT struct ImBuf *onethird(struct ImBuf *);
LIBIMPORT struct ImBuf *halflace(struct ImBuf *);
LIBIMPORT struct ImBuf *half_x(struct ImBuf *);
LIBIMPORT struct ImBuf *half_y(struct ImBuf *);
LIBIMPORT struct ImBuf *double_x(struct ImBuf *);
LIBIMPORT struct ImBuf *double_y(struct ImBuf *);
LIBIMPORT struct ImBuf *double_fast_x(struct ImBuf *);
LIBIMPORT struct ImBuf *double_fast_y(struct ImBuf *);

LIBIMPORT int ispic(char *);

LIBIMPORT void dit2(struct ImBuf *, short, short);
LIBIMPORT void dit0(struct ImBuf *, short, short);

LIBIMPORT struct ImBuf *scaleImBuf(struct ImBuf *, short, short);
LIBIMPORT struct ImBuf *scalefastImBuf(struct ImBuf *, short, short);
LIBIMPORT struct ImBuf *scalefieldImBuf(struct ImBuf *, short, short);
LIBIMPORT struct ImBuf *scalefastfieldImBuf(struct ImBuf *, short, short);

LIBIMPORT void de_interlace(struct ImBuf *ib);
LIBIMPORT void interlace(struct ImBuf *ib);
LIBIMPORT void gamwarp(struct ImBuf *ibuf, double gamma);

LIBIMPORT void IMB_rectcpy(struct ImBuf *dbuf, struct ImBuf *sbuf, 
	int destx, int desty, int srcx, int srcy, int width, int height);

LIBIMPORT void IMB_rectfill(struct ImBuf *drect, float col[4]);
LIBIMPORT void IMB_rectfill_area(struct ImBuf *ibuf, float *col, int x1, int y1, int x2, int y2);
LIBIMPORT void buf_rectfill_area(unsigned char *rect, float *rectf, int width, int height, float *col, int x1, int y1, int x2, int y2);

#endif /* IFF_H */

