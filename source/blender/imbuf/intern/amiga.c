/**
 * amiga.c
 *
 * $Id$
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

#ifdef _WIN32
#include <io.h>
#define open _open
#define read _read
#define close _close
#define write _write
#endif
#include "imbuf.h"
#include "imbuf_patch.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_global.h"

#include "IMB_cmap.h"
#include "IMB_allocimbuf.h"
#include "IMB_bitplanes.h"
#include "IMB_amiga.h"

/* actually hard coded endianness */
#define GET_BIG_LONG(x) (((uchar *) (x))[0] << 24 | ((uchar *) (x))[1] << 16 | ((uchar *) (x))[2] << 8 | ((uchar *) (x))[3])
#define GET_LITTLE_LONG(x) (((uchar *) (x))[3] << 24 | ((uchar *) (x))[2] << 16 | ((uchar *) (x))[1] << 8 | ((uchar *) (x))[0])
#define SWAP_L(x) (((x << 24) & 0xff000000) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) | ((x >> 24) & 0xff))
#define SWAP_S(x) (((x << 8) & 0xff00) | ((x >> 8) & 0xff))

/* more endianness... should move to a separate file... */
#if defined(__sgi) || defined (__sparc) || defined (__sparc__) || defined (__PPC__) || defined (__ppc__) || defined (__hppa__) || defined (__BIG_ENDIAN__)
#define GET_ID GET_BIG_LONG
#define LITTLE_LONG SWAP_LONG
#else
#define GET_ID GET_LITTLE_LONG
#define LITTLE_LONG ENDIAN_NOP
#endif

static uchar *decodebodyscanl(uchar *body, short bytes, uchar **list, short d)
{
	for (;d>0;d--){
		uchar *point;
		short todo;
		uchar i,j;

		point = *(list++);
		todo=bytes;
		while (todo>0){
			i = *body++;

			if (i & 128){			/* fill */
				if (i==128) continue;	/* nop  */

				i=257-i;
				todo-=i;
				j = *(body++);
				do{
					*(point++) = j;
					i--;
				}while (i);
			} else{				/* copy */
				i++;
				todo-=i;

				do{
					*(point++) = *(body++);
					i--;
				}while (i);
			}
		}
		if (todo) return (0);
	}
	return(body);
}


static uchar *decodebodyh(struct ImBuf *ibuf, uchar *body)
{
	if (ibuf->y==1) {
		body=decodebodyscanl(body, WIDTHB(ibuf->x), (uchar **)ibuf->planes, ibuf->depth);
	}
	else {
		unsigned int **list;
		short skipx,i,bytes,y;

		list = imb_copyplanelist(ibuf);
		if (list == 0) return (0);

		y=ibuf->y;
		bytes = WIDTHB(ibuf->x);
		skipx = ibuf->skipx;

		for (;y>0;y--){
			body=decodebodyscanl(body, bytes, (uchar **)list, ibuf->depth);
			if (body == 0) return (0);

			for (i=ibuf->depth-1;i>=0;i--){
				list[i] += skipx;
			}
		}
		free(list);
	}
	return(body);
}


static uchar *decodebodykolum(uchar *body, short bytes, uchar **list, short d, int next)
{
	for (;d>0;d--){
		uchar *point;
		short todo;
		uchar i,j;

		point = *(list++);
		todo=bytes;
		while (todo>0){
			i = *body++;

			if (i & 128){			/* fill */
				if (i==128) continue;	/* nop  */

				i=257-i;
				todo-=i;
				j = *body++;
				do{
					*point = j;
					point += next;
					i--;
				}while (i);
			}
			else{				/* copy */
				i++;
				todo-=i;

				do{
					*point = *body++;
					point += next;
					i--;
				}while (i);
			}
		}
		if (todo) return (0);
	}
	return(body);
}


static uchar *decodebodyv(struct ImBuf *ibuf, uchar *body)
{
	uchar **list;
	int skipx, i, bytes, times;

	list = (uchar **)imb_copyplanelist(ibuf);
	if (list == 0) return (0);

	bytes = ibuf->y;
	times = WIDTHB(ibuf->x);
	skipx = ibuf->skipx << 2;

	for (;times>0;times--){
		body=decodebodykolum(body,bytes,list,ibuf->depth,skipx);
		if (body == 0) return (0);

		for (i=ibuf->depth-1;i>=0;i--){
			list[i] += 1;
		}
	}
	free(list);
	return(body);
}

static uchar *makebody(uchar **planes, short bytes, short depth, uchar *buf)
{
	uchar *bitplstart,*temp;

	register uchar last,this,*bitpl;
	register short todo;
	register int copy;

	bytes--;
	for (;depth>0;depth--){
		bitpl = *(planes++);
		bitplstart = bitpl;
		todo = bytes;
		last = *bitpl++;
		this = *bitpl++;
		copy = last^this;
		while (todo>0){

			if (copy){
				do{
					last = this;
					this = *bitpl++;
					if (last == this){
						if (this == bitpl[-3]){		/* three identical ones? */
							todo -= 1;		/* set todo */
							break;
						}
					}
				}while (--todo != 0);

				copy=bitpl-bitplstart;
				copy -= 1;
				if (todo) copy -= 2;

				temp = bitpl;
				bitpl = bitplstart;

				while (copy){
					last = copy;
					if (copy>MAXDAT) last = MAXDAT;
					copy -= last;
					*buf++ = last-1;
					do{
						*buf++ = *bitpl++;
					}while(--last != 0);
				}
				bitplstart = bitpl;
				bitpl = temp;
				last = this;

				copy = FALSE;
			}
			else{
				while (*bitpl++ == this){	/* search for first different bye */
					if (--todo == 0) break;	/* or end of line */
				}
				bitpl -= 1;
				copy = bitpl-bitplstart;
				bitplstart = bitpl;
				todo -= 1;
				this = *bitpl++;

				while (copy){
					if (copy>MAXRUN){
						*buf++ = -(MAXRUN-1);
						*buf++ = last;
						copy -= MAXRUN;
					}
					else{
						*buf++ = -(copy-1);
						*buf++ = last;
						break;
					}
				}
				copy=TRUE;
			}
		}
	}
	return (buf);
}


short imb_encodebodyh(struct ImBuf *ibuf, int file)
{
	uchar *buf, *endbuf, *max;
	int size, line, ok = TRUE;
	unsigned int **list;
	short skipx,i,y;

	line = WIDTHB(ibuf->x) * ibuf->depth;
	line += (line >> 6) + 10;
	size = 16 * line;
	if (size < 16384) size = 16384;
	
	buf = (uchar *) malloc(size);
	if (buf == 0) return (0);

	max = buf + size - line;
	
	list = imb_copyplanelist(ibuf);
	if (list == 0){
		free(buf);
		return (0);
	}

	y=ibuf->y;
	skipx = ibuf->skipx;
	endbuf = buf;
	
	for (y=ibuf->y;y>0;y--){
		endbuf = makebody((uchar **)list, WIDTHB(ibuf->x), ibuf->depth, endbuf);
		if (endbuf==0){
			ok = -20;
			break;
		}
		if (endbuf >= max || y == 1){ 
			size = endbuf-buf;
			if (write(file,buf,size)!=size) ok = -19;
			endbuf = buf;
		}
		for (i=ibuf->depth-1;i>=0;i--){
			list[i] += skipx;
		}
		if (ok != TRUE) break;
	}
	free(list);

	free(buf);
	return(ok);
}


short imb_encodebodyv(struct ImBuf *ibuf, int file)
{
	struct ImBuf *ibufv;
	uchar *buf,*endbuf;
	short x,offset;

	buf = (uchar *) malloc((ibuf->y + (ibuf->y >> 6) + 10) * ibuf->depth);
	if (buf == 0) return (0);

	ibufv=IMB_allocImBuf((ibuf->y)<<3,1, ibuf->depth, 0, 1);
	if (ibufv == 0){
		free(buf);
		return (0);
	}

	offset=0;

	for(x = WIDTHB(ibuf->x);x>0;x--){
		register short i;

		for(i = ibuf->depth-1 ;i>=0;i--){
			register uchar *p1,*p2;
			register int skipx;
			register short y;

			skipx = (ibuf->skipx)*sizeof(int *);
			p1=(uchar *)ibuf->planes[i];
			p2=(uchar *)ibufv->planes[i];
			p1 += offset;

			for (y=ibuf->y;y>0;y--){
				*(p2++) = *p1;
				p1 += skipx;
			}
		}
		offset += 1;

		endbuf=makebody((uchar **)ibufv->planes, ibuf->y, ibuf->depth, buf);
		if (endbuf==0) return (-20);
		if (write(file,buf,endbuf-buf)!=endbuf-buf) return (-19);
	}
	free(buf);
	IMB_freeImBuf(ibufv);
	return (TRUE);
}

static uchar *readbody(struct ImBuf *ibuf, uchar *body)
{
	int skipbuf,skipbdy,depth,y,offset = 0;

	skipbuf = ibuf->skipx;
	skipbdy = WIDTHB(ibuf->x);

	for (y = ibuf->y; y> 0; y--){
		for( depth = 0; depth < ibuf->depth; depth ++){
			memcpy(ibuf->planes[depth] + offset, body, skipbdy);
			body += skipbdy;
		}
		offset += skipbuf;
	}
	return body;
}

struct ImBuf *imb_loadamiga(int *iffmem,int flags)
{
	int chunk,totlen,len,*cmap=0,cmaplen =0,*mem,ftype=0;
	uchar *body=0;
	struct BitMapHeader bmhd;
	struct ImBuf *ibuf=0;

	mem = iffmem;
	bmhd.w = 0;

	if (GET_ID(mem) != FORM) return (0);
	if (GET_ID(mem+2) != ILBM) return (0);
	totlen= (GET_BIG_LONG(mem+1) + 1) & ~1;
	mem += 3;
	totlen -= 4;


	while(totlen > 0){
		chunk = GET_ID(mem);
		len= (GET_BIG_LONG(mem+1) + 1) & ~1;
		mem += 2;

		totlen -= len+8;

		switch (chunk){
		case BMHD:
			memcpy(&bmhd, mem, sizeof(struct BitMapHeader));

			bmhd.w = BIG_SHORT(bmhd.w);
			bmhd.h = BIG_SHORT(bmhd.h);
			bmhd.x = BIG_SHORT(bmhd.x);
			bmhd.y = BIG_SHORT(bmhd.y);
			bmhd.transparentColor = BIG_SHORT(bmhd.transparentColor);
			bmhd.pageWidth = BIG_SHORT(bmhd.pageWidth);
			bmhd.pageHeight = BIG_SHORT(bmhd.pageHeight);
			
			break;
		case BODY:
			body = (uchar *)mem;
			break;
		case CMAP:
			cmap = mem;
			cmaplen = len/3;
			break;
		case CAMG:
			ftype = GET_BIG_LONG(mem);
			break;
		}
		mem = (int *)((uchar *)mem +len);
		if (body) break;
	}
	if (bmhd.w == 0) return (0);
	if (body == 0) return (0);
	
	if (flags & IB_test) ibuf = IMB_allocImBuf(bmhd.w, bmhd.h, bmhd.nPlanes, 0, 0);
	else ibuf = IMB_allocImBuf(bmhd.w, bmhd.h, bmhd.nPlanes + (bmhd.masking & 1),0,1);

	if (ibuf == 0) return (0);

	ibuf->ftype = (ftype | AMI);
	ibuf->profile = IB_PROFILE_SRGB;
	
	if (cmap){
		ibuf->mincol = 0;
		ibuf->maxcol = cmaplen;
		imb_addcmapImBuf(ibuf);
		imb_makecolarray(ibuf, (uchar *)cmap, 0);
	}

	if (flags & IB_test){
		if (flags & IB_freem) free(iffmem);
		return(ibuf);
	}
	
	switch (bmhd.compression){
	case 0:
		body= readbody(ibuf, body);
		break;
	case 1:
		body= decodebodyh(ibuf,body);
		break;
	case 2:
		body= decodebodyv(ibuf,body);
		ibuf->type |= IB_subdlta;
		break;
	}

	if (flags & IB_freem) free(iffmem);

	if (body == 0){
		free (ibuf);
		return(0);
	}
	
	/* forget stencil */
	ibuf->depth = bmhd.nPlanes;
	
	if (flags & IB_rect){
		imb_addrectImBuf(ibuf);
		imb_bptolong(ibuf);
		imb_freeplanesImBuf(ibuf);
		if (ibuf->cmap){
			if ((flags & IB_cmap) == 0) IMB_applycmap(ibuf);
		} else if (ibuf->depth == 18){
			int i,col;
			unsigned int *rect;

			rect = ibuf->rect;
			for(i=ibuf->x * ibuf->y ; i>0 ; i--){
				col = *rect;
				col = ((col & 0x3f000) << 6) + ((col & 0xfc0) << 4) + ((col & 0x3f) << 2);
				col += (col & 0xc0c0c0) >> 6;
				*rect++ = col;
			}
			ibuf->depth = 24;
		} else if (ibuf->depth <= 8) { /* no colormap and no 24 bits: b&w */
			uchar *rect;
			int size, shift;

			if (ibuf->depth < 8){
				rect = (uchar *) ibuf->rect;
				rect += 3;
				shift = 8 - ibuf->depth;
				for (size = ibuf->x * ibuf->y; size > 0; size --){
					rect[0] <<= shift;
					rect += 4;
				}
			}
			rect = (uchar *) ibuf->rect;
			for (size = ibuf->x * ibuf->y; size > 0; size --){
				rect[1] = rect[2] = rect[3];
				rect += 4;
			}
			ibuf->depth = 8;
		}
	}

	if ((flags & IB_ttob) == 0) IMB_flipy(ibuf);

	if (ibuf) {
		if (ibuf->rect) 
			if (ENDIAN_ORDER == B_ENDIAN) IMB_convert_rgba_to_abgr(ibuf);
	}
	
	return (ibuf);
}
