/* Copyright (c) 1999, Not a Number / NeoGeo b.v. 
 * $Id$
 * 
 * All rights reserved.
 * 
 * Contact:      blender@blender.nl   
 * Information:  http://www.blender.nl
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

#define IB_rect			(1 << 0)
#define IB_planes		(1 << 1)
#define IB_cmap			(1 << 2)
#define IB_test			(1 << 7)

#define IB_fields		(1 << 11)
#define IB_yuv			(1 << 12)
#define IB_zbuf			(1 << 13)
#define IB_rgba			(1 << 14)

#define AMI	    (1 << 31)
#define Anim	(1 << 29)
#define TGA	    (1 << 28)
#define JPG		(1 << 27)

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
#define IS_tim(x)		(x->ftype & TIM)

#define IMAGIC 	0732
#define IS_iris(x)		(x->ftype == IMAGIC)

#define IS_jpg(x)		(x->ftype & JPG)
#define IS_stdjpg(x)	((x->ftype & JPG_MSK) == JPG_STD)
#define IS_vidjpg(x)	((x->ftype & JPG_MSK) == JPG_VID)
#define IS_jstjpg(x)	((x->ftype & JPG_MSK) == JPG_JST)
#define IS_maxjpg(x)	((x->ftype & JPG_MSK) == JPG_MAX)

#define AN_INIT an_stringdec = stringdec; an_stringenc = stringenc;

typedef struct ImBuf{
	short	x,y;		/* breedte in pixels, hoogte in scanlines */
	short	skipx;		/* breedte in ints om bij volgende scanline te komen */
	uchar	depth;		/* actieve aantal bits/bitplanes */
	uchar	cbits;		/* aantal active bits in cmap */
	ushort	mincol;
	ushort	maxcol;
	int	type;		/* 0=abgr, 1=bitplanes */
	int	ftype;
	uint	*cmap;		/* int array van kleuren */
	uint	*rect;		/* databuffer */
	uint	**planes;	/* bitplanes */
	uchar	*chardata;	/* voor cdi-compressie */
	int	flags;
	int	mall;		/* wat is er intern gemalloced en mag weer vrijgegeven worden */
	short	xorig, yorig;
	char	name[127];
	char	namenull;
	int	userflags;
	int	*zbuf;
	void	*userdata;
} ImBuf;

extern struct ImBuf *allocImBuf(short,short,uchar,uint,uchar);
extern struct ImBuf *dupImBuf(struct ImBuf *);
extern void freeImBuf(struct ImBuf*);

extern short converttocmap(struct ImBuf* ibuf);

extern short saveiff(struct ImBuf *,char *,int);

extern struct ImBuf *loadiffmem(int *,int);
extern struct ImBuf *loadifffile(int,int);
extern struct ImBuf *loadiffname(char *,int);
extern struct ImBuf *testiffname(char *,int);

extern struct ImBuf *onehalf(struct ImBuf *);
extern struct ImBuf *onethird(struct ImBuf *);
extern struct ImBuf *halflace(struct ImBuf *);
extern struct ImBuf *half_x(struct ImBuf *);
extern struct ImBuf *half_y(struct ImBuf *);
extern struct ImBuf *double_x(struct ImBuf *);
extern struct ImBuf *double_y(struct ImBuf *);
extern struct ImBuf *double_fast_x(struct ImBuf *);
extern struct ImBuf *double_fast_y(struct ImBuf *);

extern int ispic(char *);

extern void dit2(struct ImBuf *, short, short);
extern void dit0(struct ImBuf *, short, short);

extern struct ImBuf *scaleImBuf(struct ImBuf *, short, short);
extern struct ImBuf *scalefastImBuf(struct ImBuf *, short, short);
extern struct ImBuf *scalefieldImBuf(struct ImBuf *, short, short);
extern struct ImBuf *scalefastfieldImBuf(struct ImBuf *, short, short);

extern void de_interlace(struct ImBuf *ib);

extern void rectop(struct ImBuf *dbuf,
				   struct ImBuf *sbuf,
				   int destx,
				   int desty,
				   int srcx,
				   int srcy,
				   int width,
				   int height,
				   void (*operation)(unsigned int *, unsigned int*, int, int),
				   int value);

#endif /* IFF_H */
