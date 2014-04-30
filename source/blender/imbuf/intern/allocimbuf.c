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

/** \file blender/imbuf/intern/allocimbuf.c
 *  \ingroup imbuf
 */


/* It's become a bit messy... Basically, only the IMB_ prefixed files
 * should remain. */

#include <stddef.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "IMB_allocimbuf.h"
#include "IMB_filetype.h"
#include "IMB_metadata.h"
#include "IMB_colormanagement_intern.h"

#include "imbuf.h"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"
#include "BLI_utildefines.h"

static SpinLock refcounter_spin;

void imb_refcounter_lock_init(void)
{
	BLI_spin_init(&refcounter_spin);
}

void imb_refcounter_lock_exit(void)
{
	BLI_spin_end(&refcounter_spin);
}

void imb_freemipmapImBuf(ImBuf *ibuf)
{
	int a;
	
	for (a = 1; a < ibuf->miptot; a++) {
		if (ibuf->mipmap[a - 1])
			IMB_freeImBuf(ibuf->mipmap[a - 1]);
		ibuf->mipmap[a - 1] = NULL;
	}

	ibuf->miptot = 0;
}

/* any free rect frees mipmaps to be sure, creation is in render on first request */
void imb_freerectfloatImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return;
	
	if (ibuf->rect_float && (ibuf->mall & IB_rectfloat)) {
		MEM_freeN(ibuf->rect_float);
		ibuf->rect_float = NULL;
	}

	imb_freemipmapImBuf(ibuf);
	
	ibuf->rect_float = NULL;
	ibuf->mall &= ~IB_rectfloat;
}

/* any free rect frees mipmaps to be sure, creation is in render on first request */
void imb_freerectImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return;

	if (ibuf->rect && (ibuf->mall & IB_rect))
		MEM_freeN(ibuf->rect);
	ibuf->rect = NULL;
	
	imb_freemipmapImBuf(ibuf);

	ibuf->mall &= ~IB_rect;
}

void imb_freetilesImBuf(ImBuf *ibuf)
{
	int tx, ty;

	if (ibuf == NULL) return;

	if (ibuf->tiles && (ibuf->mall & IB_tiles)) {
		for (ty = 0; ty < ibuf->ytiles; ty++) {
			for (tx = 0; tx < ibuf->xtiles; tx++) {
				if (ibuf->tiles[ibuf->xtiles * ty + tx]) {
					imb_tile_cache_tile_free(ibuf, tx, ty);
					MEM_freeN(ibuf->tiles[ibuf->xtiles * ty + tx]);
				}
			}
		}

		MEM_freeN(ibuf->tiles);
	}

	ibuf->tiles = NULL;
	ibuf->mall &= ~IB_tiles;
}

static void freeencodedbufferImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return;

	if (ibuf->encodedbuffer && (ibuf->mall & IB_mem))
		MEM_freeN(ibuf->encodedbuffer);

	ibuf->encodedbuffer = NULL;
	ibuf->encodedbuffersize = 0;
	ibuf->encodedsize = 0;
	ibuf->mall &= ~IB_mem;
}

void IMB_freezbufImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return;

	if (ibuf->zbuf && (ibuf->mall & IB_zbuf))
		MEM_freeN(ibuf->zbuf);

	ibuf->zbuf = NULL;
	ibuf->mall &= ~IB_zbuf;
}

void IMB_freezbuffloatImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return;

	if (ibuf->zbuf_float && (ibuf->mall & IB_zbuffloat))
		MEM_freeN(ibuf->zbuf_float);

	ibuf->zbuf_float = NULL;
	ibuf->mall &= ~IB_zbuffloat;
}

void IMB_freeImBuf(ImBuf *ibuf)
{
	if (ibuf) {
		bool needs_free = false;

		BLI_spin_lock(&refcounter_spin);
		if (ibuf->refcounter > 0) {
			ibuf->refcounter--;
		}
		else {
			needs_free = true;
		}
		BLI_spin_unlock(&refcounter_spin);

		if (needs_free) {
			imb_freerectImBuf(ibuf);
			imb_freerectfloatImBuf(ibuf);
			imb_freetilesImBuf(ibuf);
			IMB_freezbufImBuf(ibuf);
			IMB_freezbuffloatImBuf(ibuf);
			freeencodedbufferImBuf(ibuf);
			IMB_metadata_free(ibuf);
			colormanage_cache_free(ibuf);

			if (ibuf->dds_data.data != NULL) {
				free(ibuf->dds_data.data); /* dds_data.data is allocated by DirectDrawSurface::readData(), so don't use MEM_freeN! */
			}
			MEM_freeN(ibuf);
		}
	}
}

void IMB_refImBuf(ImBuf *ibuf)
{
	BLI_spin_lock(&refcounter_spin);
	ibuf->refcounter++;
	BLI_spin_unlock(&refcounter_spin);
}

ImBuf *IMB_makeSingleUser(ImBuf *ibuf)
{
	ImBuf *rval;

	if (!ibuf || ibuf->refcounter == 0) { return ibuf; }

	rval = IMB_dupImBuf(ibuf);

	IMB_freeImBuf(ibuf);

	return rval;
}

bool addzbufImBuf(ImBuf *ibuf)
{
	size_t size;
	
	if (ibuf == NULL) return false;
	
	IMB_freezbufImBuf(ibuf);
	
	size = (size_t)ibuf->x * (size_t)ibuf->y * sizeof(unsigned int);

	if ((ibuf->zbuf = MEM_mapallocN(size, __func__))) {
		ibuf->mall |= IB_zbuf;
		ibuf->flags |= IB_zbuf;
		return true;
	}
	
	return false;
}

bool addzbuffloatImBuf(ImBuf *ibuf)
{
	size_t size;
	
	if (ibuf == NULL) return false;
	
	IMB_freezbuffloatImBuf(ibuf);
	
	size = (size_t)ibuf->x * (size_t)ibuf->y * sizeof(float);

	if ((ibuf->zbuf_float = MEM_mapallocN(size, __func__))) {
		ibuf->mall |= IB_zbuffloat;
		ibuf->flags |= IB_zbuffloat;
		return true;
	}
	
	return false;
}


bool imb_addencodedbufferImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return false;

	freeencodedbufferImBuf(ibuf);

	if (ibuf->encodedbuffersize == 0)
		ibuf->encodedbuffersize = 10000;

	ibuf->encodedsize = 0;

	if ((ibuf->encodedbuffer = MEM_mallocN(ibuf->encodedbuffersize, __func__))) {
		ibuf->mall |= IB_mem;
		ibuf->flags |= IB_mem;
		return true;
	}

	return false;
}


bool imb_enlargeencodedbufferImBuf(ImBuf *ibuf)
{
	unsigned int newsize, encodedsize;
	void *newbuffer;

	if (ibuf == NULL) return false;

	if (ibuf->encodedbuffersize < ibuf->encodedsize) {
		printf("%s: error in parameters\n", __func__);
		return false;
	}

	newsize = 2 * ibuf->encodedbuffersize;
	if (newsize < 10000) newsize = 10000;

	newbuffer = MEM_mallocN(newsize, __func__);
	if (newbuffer == NULL) return false;

	if (ibuf->encodedbuffer) {
		memcpy(newbuffer, ibuf->encodedbuffer, ibuf->encodedsize);
	}
	else {
		ibuf->encodedsize = 0;
	}

	encodedsize = ibuf->encodedsize;

	freeencodedbufferImBuf(ibuf);

	ibuf->encodedbuffersize = newsize;
	ibuf->encodedsize = encodedsize;
	ibuf->encodedbuffer = newbuffer;
	ibuf->mall |= IB_mem;
	ibuf->flags |= IB_mem;

	return true;
}

bool imb_addrectfloatImBuf(ImBuf *ibuf)
{
	size_t size;
	
	if (ibuf == NULL) return false;
	
	if (ibuf->rect_float)
		imb_freerectfloatImBuf(ibuf);  /* frees mipmap too, hrm */
	
	size = (size_t)ibuf->x * (size_t)ibuf->y * sizeof(float[4]);

	ibuf->channels = 4;
	if ((ibuf->rect_float = MEM_mapallocN(size, __func__))) {
		ibuf->mall |= IB_rectfloat;
		ibuf->flags |= IB_rectfloat;
		return true;
	}
	
	return false;
}

/* question; why also add zbuf? */
bool imb_addrectImBuf(ImBuf *ibuf)
{
	size_t size;

	if (ibuf == NULL) return false;
	
	/* don't call imb_freerectImBuf, it frees mipmaps, this call is used only too give float buffers display */
	if (ibuf->rect && (ibuf->mall & IB_rect))
		MEM_freeN(ibuf->rect);
	ibuf->rect = NULL;
	
	size = (size_t)ibuf->x * (size_t)ibuf->y * sizeof(unsigned int);

	if ((ibuf->rect = MEM_mapallocN(size, __func__))) {
		ibuf->mall |= IB_rect;
		ibuf->flags |= IB_rect;
		if (ibuf->planes > 32) {
			return (addzbufImBuf(ibuf));
		}
		else {
			return true;
		}
	}

	return false;
}

bool imb_addtilesImBuf(ImBuf *ibuf)
{
	if (ibuf == NULL) return false;

	if (!ibuf->tiles)
		if ((ibuf->tiles = MEM_callocN(sizeof(unsigned int *) * ibuf->xtiles * ibuf->ytiles, "imb_tiles")))
			ibuf->mall |= IB_tiles;

	return (ibuf->tiles != NULL);
}

ImBuf *IMB_allocImBuf(unsigned int x, unsigned int y, uchar planes, unsigned int flags)
{
	ImBuf *ibuf;

	ibuf = MEM_callocN(sizeof(ImBuf), "ImBuf_struct");

	if (ibuf) {
		ibuf->x = x;
		ibuf->y = y;
		ibuf->planes = planes;
		ibuf->ftype = PNG | 15; /* the 15 means, set compression to low ratio but not time consuming */
		ibuf->channels = 4;  /* float option, is set to other values when buffers get assigned */
		ibuf->ppm[0] = ibuf->ppm[1] = IMB_DPI_DEFAULT / 0.0254f; /* IMB_DPI_DEFAULT -> pixels-per-meter */

		if (flags & IB_rect) {
			if (imb_addrectImBuf(ibuf) == false) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
		
		if (flags & IB_rectfloat) {
			if (imb_addrectfloatImBuf(ibuf) == false) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
		
		if (flags & IB_zbuf) {
			if (addzbufImBuf(ibuf) == false) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}
		
		if (flags & IB_zbuffloat) {
			if (addzbuffloatImBuf(ibuf) == false) {
				IMB_freeImBuf(ibuf);
				return NULL;
			}
		}

		/* assign default spaces */
		colormanage_imbuf_set_default_spaces(ibuf);
	}
	return (ibuf);
}

/* does no zbuffers? */
ImBuf *IMB_dupImBuf(ImBuf *ibuf1)
{
	ImBuf *ibuf2, tbuf;
	int flags = 0;
	int a, x, y;
	
	if (ibuf1 == NULL) return NULL;

	if (ibuf1->rect) flags |= IB_rect;
	if (ibuf1->rect_float) flags |= IB_rectfloat;

	x = ibuf1->x;
	y = ibuf1->y;
	if (ibuf1->flags & IB_fields) y *= 2;
	
	ibuf2 = IMB_allocImBuf(x, y, ibuf1->planes, flags);
	if (ibuf2 == NULL) return NULL;

	if (flags & IB_rect)
		memcpy(ibuf2->rect, ibuf1->rect, x * y * sizeof(int));
	
	if (flags & IB_rectfloat)
		memcpy(ibuf2->rect_float, ibuf1->rect_float, ibuf1->channels * x * y * sizeof(float));

	if (ibuf1->encodedbuffer) {
		ibuf2->encodedbuffersize = ibuf1->encodedbuffersize;
		if (imb_addencodedbufferImBuf(ibuf2) == false) {
			IMB_freeImBuf(ibuf2);
			return NULL;
		}

		memcpy(ibuf2->encodedbuffer, ibuf1->encodedbuffer, ibuf1->encodedsize);
	}

	/* silly trick to copy the entire contents of ibuf1 struct over to ibuf */
	tbuf = *ibuf1;
	
	/* fix pointers */
	tbuf.rect          = ibuf2->rect;
	tbuf.rect_float    = ibuf2->rect_float;
	tbuf.encodedbuffer = ibuf2->encodedbuffer;
	tbuf.zbuf          = NULL;
	tbuf.zbuf_float    = NULL;
	for (a = 0; a < IB_MIPMAP_LEVELS; a++)
		tbuf.mipmap[a] = NULL;
	tbuf.dds_data.data = NULL;
	
	/* set malloc flag */
	tbuf.mall               = ibuf2->mall;
	tbuf.c_handle           = NULL;
	tbuf.refcounter         = 0;

	/* for now don't duplicate metadata */
	tbuf.metadata = NULL;

	tbuf.display_buffer_flags = NULL;
	tbuf.colormanage_cache = NULL;

	*ibuf2 = tbuf;

	return(ibuf2);
}

#if 0 /* remove? - campbell */
/* support for cache limiting */

static void imbuf_cache_destructor(void *data)
{
	ImBuf *ibuf = (ImBuf *) data;

	imb_freerectImBuf(ibuf);
	imb_freerectfloatImBuf(ibuf);
	IMB_freezbufImBuf(ibuf);
	IMB_freezbuffloatImBuf(ibuf);
	freeencodedbufferImBuf(ibuf);

	ibuf->c_handle = NULL;
}


static MEM_CacheLimiterC **get_imbuf_cache_limiter(void)
{
	static MEM_CacheLimiterC *c = NULL;

	if (!c)
		c = new_MEM_CacheLimiter(imbuf_cache_destructor, NULL);

	return &c;
}
#endif
