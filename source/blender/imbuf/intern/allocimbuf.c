/*
 * allocimbuf.c
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

/* It's become a bit messy... Basically, only the IMB_ prefixed files
 * should remain. */

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"
#include "IMB_metadata.h"

#include "imbuf.h"

#include "MEM_CacheLimiterC-Api.h"

void imb_freemipmapImBuf(ImBuf *ibuf)
{
	int a;
	
	for(a=1; a<ibuf->miptot; a++) {
		if(ibuf->mipmap[a-1])
			IMB_freeImBuf(ibuf->mipmap[a-1]);
		ibuf->mipmap[a-1]= NULL;
	}

	ibuf->miptot= 0;
}

/* any free rect frees mipmaps to be sure, creation is in render on first request */
void imb_freerectfloatImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return;
	
	if(ibuf->rect_float && (ibuf->mall & IB_rectfloat)) {
		MEM_freeN(ibuf->rect_float);
		ibuf->rect_float=NULL;
	}

	imb_freemipmapImBuf(ibuf);
	
	ibuf->rect_float= NULL;
	ibuf->mall &= ~IB_rectfloat;
}

/* any free rect frees mipmaps to be sure, creation is in render on first request */
void imb_freerectImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return;
	
	if(ibuf->crect && ibuf->crect != ibuf->rect)
		MEM_freeN(ibuf->crect);

	if(ibuf->rect && (ibuf->mall & IB_rect))
		MEM_freeN(ibuf->rect);
	
	imb_freemipmapImBuf(ibuf);
	
	ibuf->rect= NULL;
	ibuf->crect= NULL;
	ibuf->mall &= ~IB_rect;
}

void imb_freetilesImBuf(ImBuf *ibuf)
{
	int tx, ty;

	if(ibuf==NULL) return;

	if(ibuf->tiles && (ibuf->mall & IB_tiles)) {
		for(ty=0; ty<ibuf->ytiles; ty++) {
			for(tx=0; tx<ibuf->xtiles; tx++) {
				if(ibuf->tiles[ibuf->xtiles*ty + tx]) {
					imb_tile_cache_tile_free(ibuf, tx, ty);
					MEM_freeN(ibuf->tiles[ibuf->xtiles*ty + tx]);
				}
			}
		}

		MEM_freeN(ibuf->tiles);
	}

	ibuf->tiles= NULL;
	ibuf->mall &= ~IB_tiles;
}

static void freeencodedbufferImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return;

	if(ibuf->encodedbuffer && (ibuf->mall & IB_mem))
		MEM_freeN(ibuf->encodedbuffer);

	ibuf->encodedbuffer = 0;
	ibuf->encodedbuffersize = 0;
	ibuf->encodedsize = 0;
	ibuf->mall &= ~IB_mem;
}

void IMB_freezbufImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return;

	if(ibuf->zbuf && (ibuf->mall & IB_zbuf))
		MEM_freeN(ibuf->zbuf);

	ibuf->zbuf= NULL;
	ibuf->mall &= ~IB_zbuf;
}

void IMB_freezbuffloatImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return;

	if(ibuf->zbuf_float && (ibuf->mall & IB_zbuffloat))
		MEM_freeN(ibuf->zbuf_float);

	ibuf->zbuf_float= NULL;
	ibuf->mall &= ~IB_zbuffloat;
}

void IMB_freeImBuf(ImBuf *ibuf)
{
	if(ibuf) {
		if(ibuf->refcounter > 0) {
			ibuf->refcounter--;
		}
		else {
			imb_freerectImBuf(ibuf);
			imb_freerectfloatImBuf(ibuf);
			imb_freetilesImBuf(ibuf);
			IMB_freezbufImBuf(ibuf);
			IMB_freezbuffloatImBuf(ibuf);
			freeencodedbufferImBuf(ibuf);
			IMB_cache_limiter_unmanage(ibuf);
			IMB_metadata_free(ibuf);
			MEM_freeN(ibuf);
		}
	}
}

void IMB_refImBuf(ImBuf *ibuf)
{
	ibuf->refcounter++;
}

short addzbufImBuf(ImBuf *ibuf)
{
	int size;
	
	if(ibuf==NULL) return FALSE;
	
	IMB_freezbufImBuf(ibuf);
	
	size = ibuf->x *ibuf->y *sizeof(unsigned int);
	if((ibuf->zbuf = MEM_mapallocN(size, "addzbufImBuf"))) {
		ibuf->mall |= IB_zbuf;
		ibuf->flags |= IB_zbuf;
		return TRUE;
	}
	
	return FALSE;
}

short addzbuffloatImBuf(ImBuf *ibuf)
{
	int size;
	
	if(ibuf==NULL) return FALSE;
	
	IMB_freezbuffloatImBuf(ibuf);
	
	size = ibuf->x *ibuf->y *sizeof(float);
	if((ibuf->zbuf_float = MEM_mapallocN(size, "addzbuffloatImBuf"))) {
		ibuf->mall |= IB_zbuffloat;
		ibuf->flags |= IB_zbuffloat;
		return TRUE;
	}
	
	return FALSE;
}


short imb_addencodedbufferImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return FALSE;

	freeencodedbufferImBuf(ibuf);

	if(ibuf->encodedbuffersize == 0) 
		ibuf->encodedbuffersize = 10000;

	ibuf->encodedsize = 0;

	if((ibuf->encodedbuffer = MEM_mallocN(ibuf->encodedbuffersize, "addencodedbufferImBuf"))) {
		ibuf->mall |= IB_mem;
		ibuf->flags |= IB_mem;
		return TRUE;
	}

	return FALSE;
}


short imb_enlargeencodedbufferImBuf(ImBuf *ibuf)
{
	unsigned int newsize, encodedsize;
	void *newbuffer;

	if(ibuf==NULL) return FALSE;

	if(ibuf->encodedbuffersize < ibuf->encodedsize) {
		printf("imb_enlargeencodedbufferImBuf: error in parameters\n");
		return FALSE;
	}

	newsize = 2 *ibuf->encodedbuffersize;
	if(newsize < 10000) newsize = 10000;

	newbuffer = MEM_mallocN(newsize, "enlargeencodedbufferImBuf");
	if(newbuffer == NULL) return FALSE;

	if(ibuf->encodedbuffer) {
		memcpy(newbuffer, ibuf->encodedbuffer, ibuf->encodedsize);
	} else {
		ibuf->encodedsize = 0;
	}

	encodedsize = ibuf->encodedsize;

	freeencodedbufferImBuf(ibuf);

	ibuf->encodedbuffersize = newsize;
	ibuf->encodedsize = encodedsize;
	ibuf->encodedbuffer = newbuffer;
	ibuf->mall |= IB_mem;
	ibuf->flags |= IB_mem;

	return TRUE;
}

short imb_addrectfloatImBuf(ImBuf *ibuf)
{
	int size;
	
	if(ibuf==NULL) return FALSE;
	
	imb_freerectfloatImBuf(ibuf);
	
	size = ibuf->x *ibuf->y;
	size = size *4 *sizeof(float);
	ibuf->channels= 4;
	
	if((ibuf->rect_float = MEM_mapallocN(size, "imb_addrectfloatImBuf"))) {
		ibuf->mall |= IB_rectfloat;
		ibuf->flags |= IB_rectfloat;
		return TRUE;
	}
	
	return FALSE;
}

/* question; why also add zbuf? */
short imb_addrectImBuf(ImBuf *ibuf)
{
	int size;

	if(ibuf==NULL) return FALSE;
	imb_freerectImBuf(ibuf);

	size = ibuf->x*ibuf->y;
	size = size*sizeof(unsigned int);

	if((ibuf->rect = MEM_mapallocN(size, "imb_addrectImBuf"))) {
		ibuf->mall |= IB_rect;
		ibuf->flags |= IB_rect;
		if(ibuf->depth > 32) return (addzbufImBuf(ibuf));
		else return TRUE;
	}

	return FALSE;
}

short imb_addtilesImBuf(ImBuf *ibuf)
{
	if(ibuf==NULL) return FALSE;

	if(!ibuf->tiles)
		if((ibuf->tiles = MEM_callocN(sizeof(unsigned int*)*ibuf->xtiles*ibuf->ytiles, "imb_tiles")))
			ibuf->mall |= IB_tiles;

	return (ibuf->tiles != NULL);
}

ImBuf *IMB_allocImBuf(short x, short y, uchar d, unsigned int flags, uchar bitmap) /* XXX bitmap argument is deprecated */
{
	ImBuf *ibuf;

	ibuf = MEM_callocN(sizeof(ImBuf), "ImBuf_struct");

	if(ibuf) {
		ibuf->x= x;
		ibuf->y= y;
		ibuf->depth= d;
		ibuf->ftype= TGA;
		ibuf->channels= 4;	/* float option, is set to other values when buffers get assigned */
		
		if(flags & IB_rect) {
			if(imb_addrectImBuf(ibuf)==FALSE) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
		
		if(flags & IB_rectfloat) {
			if(imb_addrectfloatImBuf(ibuf)==FALSE) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
		
		if(flags & IB_zbuf) {
			if(addzbufImBuf(ibuf)==FALSE) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
		
		if(flags & IB_zbuffloat) {
			if(addzbuffloatImBuf(ibuf)==FALSE) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
	}
	return (ibuf);
}

/* does no zbuffers? */
ImBuf *IMB_dupImBuf(ImBuf *ibuf1)
{
	ImBuf *ibuf2, tbuf;
	int flags = 0;
	int a, x, y;
	
	if(ibuf1 == NULL) return NULL;

	if(ibuf1->rect) flags |= IB_rect;
	if(ibuf1->rect_float) flags |= IB_rectfloat;

	x = ibuf1->x;
	y = ibuf1->y;
	if(ibuf1->flags & IB_fields) y *= 2;
	
	ibuf2 = IMB_allocImBuf(x, y, ibuf1->depth, flags, 0);
	if(ibuf2 == NULL) return NULL;

	if(flags & IB_rect)
		memcpy(ibuf2->rect, ibuf1->rect, x *y *sizeof(int));
	
	if(flags & IB_rectfloat)
		memcpy(ibuf2->rect_float, ibuf1->rect_float, ibuf1->channels *x *y *sizeof(float));

	if(ibuf1->encodedbuffer) {
		ibuf2->encodedbuffersize = ibuf1->encodedbuffersize;
		if(imb_addencodedbufferImBuf(ibuf2) == FALSE) {
			IMB_freeImBuf(ibuf2);
			return NULL;
		}

		memcpy(ibuf2->encodedbuffer, ibuf1->encodedbuffer, ibuf1->encodedsize);
	}

	/* silly trick to copy the entire contents of ibuf1 struct over to ibuf */
	tbuf = *ibuf1;
	
	// fix pointers 
	tbuf.rect		= ibuf2->rect;
	tbuf.rect_float = ibuf2->rect_float;
	tbuf.encodedbuffer = ibuf2->encodedbuffer;
	tbuf.zbuf= NULL;
	tbuf.zbuf_float= NULL;
	for(a=0; a<IB_MIPMAP_LEVELS; a++)
		tbuf.mipmap[a]= NULL;
	
	// set malloc flag
	tbuf.mall		= ibuf2->mall;
	tbuf.c_handle           = 0;
	tbuf.refcounter         = 0;

	// for now don't duplicate metadata
	tbuf.metadata = 0;

	*ibuf2 = tbuf;
	
	return(ibuf2);
}

/* support for cache limiting */

static void imbuf_cache_destructor(void *data)
{
	ImBuf *ibuf = (ImBuf*) data;

	imb_freerectImBuf(ibuf);
	imb_freerectfloatImBuf(ibuf);
	IMB_freezbufImBuf(ibuf);
	IMB_freezbuffloatImBuf(ibuf);
	freeencodedbufferImBuf(ibuf);

	ibuf->c_handle = 0;
}

static MEM_CacheLimiterC **get_imbuf_cache_limiter()
{
	static MEM_CacheLimiterC *c = 0;

	if(!c)
		c = new_MEM_CacheLimiter(imbuf_cache_destructor);

	return &c;
}

void IMB_free_cache_limiter()
{
	delete_MEM_CacheLimiter(*get_imbuf_cache_limiter());
	*get_imbuf_cache_limiter() = 0;
}

void IMB_cache_limiter_insert(ImBuf *i)
{
	if(!i->c_handle) {
		i->c_handle = MEM_CacheLimiter_insert(
			*get_imbuf_cache_limiter(), i);
		MEM_CacheLimiter_ref(i->c_handle);
		MEM_CacheLimiter_enforce_limits(
			*get_imbuf_cache_limiter());
		MEM_CacheLimiter_unref(i->c_handle);
	}
}

void IMB_cache_limiter_unmanage(ImBuf *i)
{
	if(i->c_handle) {
		MEM_CacheLimiter_unmanage(i->c_handle);
		i->c_handle = 0;
	}
}

void IMB_cache_limiter_touch(ImBuf *i)
{
	if(i->c_handle)
		MEM_CacheLimiter_touch(i->c_handle);
}

void IMB_cache_limiter_ref(ImBuf *i)
{
	if(i->c_handle)
		MEM_CacheLimiter_ref(i->c_handle);
}

void IMB_cache_limiter_unref(ImBuf *i)
{
	if(i->c_handle)
		MEM_CacheLimiter_unref(i->c_handle);
}

int IMB_cache_limiter_get_refcount(ImBuf *i)
{
	if(i->c_handle)
		return MEM_CacheLimiter_get_refcount(i->c_handle);

	return 0;
}

