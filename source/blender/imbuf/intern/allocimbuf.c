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

/** \file
 * \ingroup imbuf
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

#include "BLI_utildefines.h"
#include "BLI_threads.h"

static SpinLock refcounter_spin;

void imb_refcounter_lock_init(void)
{
  BLI_spin_init(&refcounter_spin);
}

void imb_refcounter_lock_exit(void)
{
  BLI_spin_end(&refcounter_spin);
}

#ifdef WIN32
static SpinLock mmap_spin;

void imb_mmap_lock_init(void)
{
  BLI_spin_init(&mmap_spin);
}

void imb_mmap_lock_exit(void)
{
  BLI_spin_end(&mmap_spin);
}

void imb_mmap_lock(void)
{
  BLI_spin_lock(&mmap_spin);
}

void imb_mmap_unlock(void)
{
  BLI_spin_unlock(&mmap_spin);
}
#endif

void imb_freemipmapImBuf(ImBuf *ibuf)
{
  int a;

  /* Do not trust ibuf->miptot, in some cases IMB_remakemipmap can leave unfreed unused levels,
   * leading to memory leaks... */
  for (a = 0; a < IMB_MIPMAP_LEVELS; a++) {
    if (ibuf->mipmap[a] != NULL) {
      IMB_freeImBuf(ibuf->mipmap[a]);
      ibuf->mipmap[a] = NULL;
    }
  }

  ibuf->miptot = 0;
}

/* any free rect frees mipmaps to be sure, creation is in render on first request */
void imb_freerectfloatImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return;
  }

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
  if (ibuf == NULL) {
    return;
  }

  if (ibuf->rect && (ibuf->mall & IB_rect)) {
    MEM_freeN(ibuf->rect);
  }
  ibuf->rect = NULL;

  imb_freemipmapImBuf(ibuf);

  ibuf->mall &= ~IB_rect;
}

void imb_freetilesImBuf(ImBuf *ibuf)
{
  int tx, ty;

  if (ibuf == NULL) {
    return;
  }

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
  if (ibuf == NULL) {
    return;
  }

  if (ibuf->encodedbuffer && (ibuf->mall & IB_mem)) {
    MEM_freeN(ibuf->encodedbuffer);
  }

  ibuf->encodedbuffer = NULL;
  ibuf->encodedbuffersize = 0;
  ibuf->encodedsize = 0;
  ibuf->mall &= ~IB_mem;
}

void IMB_freezbufImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return;
  }

  if (ibuf->zbuf && (ibuf->mall & IB_zbuf)) {
    MEM_freeN(ibuf->zbuf);
  }

  ibuf->zbuf = NULL;
  ibuf->mall &= ~IB_zbuf;
}

void IMB_freezbuffloatImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return;
  }

  if (ibuf->zbuf_float && (ibuf->mall & IB_zbuffloat)) {
    MEM_freeN(ibuf->zbuf_float);
  }

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
      IMB_metadata_free(ibuf->metadata);
      colormanage_cache_free(ibuf);

      if (ibuf->dds_data.data != NULL) {
        /* dds_data.data is allocated by DirectDrawSurface::readData(), so don't use MEM_freeN! */
        free(ibuf->dds_data.data);
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

  if (ibuf) {
    bool is_single;
    BLI_spin_lock(&refcounter_spin);
    is_single = (ibuf->refcounter == 0);
    BLI_spin_unlock(&refcounter_spin);
    if (is_single) {
      return ibuf;
    }
  }
  else {
    return NULL;
  }

  rval = IMB_dupImBuf(ibuf);

  IMB_metadata_copy(rval, ibuf);

  IMB_freeImBuf(ibuf);

  return rval;
}

bool addzbufImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return false;
  }

  IMB_freezbufImBuf(ibuf);

  if ((ibuf->zbuf = imb_alloc_pixels(ibuf->x, ibuf->y, 1, sizeof(unsigned int), __func__))) {
    ibuf->mall |= IB_zbuf;
    ibuf->flags |= IB_zbuf;
    return true;
  }

  return false;
}

bool addzbuffloatImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return false;
  }

  IMB_freezbuffloatImBuf(ibuf);

  if ((ibuf->zbuf_float = imb_alloc_pixels(ibuf->x, ibuf->y, 1, sizeof(float), __func__))) {
    ibuf->mall |= IB_zbuffloat;
    ibuf->flags |= IB_zbuffloat;
    return true;
  }

  return false;
}

bool imb_addencodedbufferImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return false;
  }

  freeencodedbufferImBuf(ibuf);

  if (ibuf->encodedbuffersize == 0) {
    ibuf->encodedbuffersize = 10000;
  }

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

  if (ibuf == NULL) {
    return false;
  }

  if (ibuf->encodedbuffersize < ibuf->encodedsize) {
    printf("%s: error in parameters\n", __func__);
    return false;
  }

  newsize = 2 * ibuf->encodedbuffersize;
  if (newsize < 10000) {
    newsize = 10000;
  }

  newbuffer = MEM_mallocN(newsize, __func__);
  if (newbuffer == NULL) {
    return false;
  }

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

void *imb_alloc_pixels(
    unsigned int x, unsigned int y, unsigned int channels, size_t typesize, const char *name)
{
  /* Protect against buffer overflow vulnerabilities from files specifying
   * a width and height that overflow and alloc too little memory. */
  if (!((uint64_t)x * (uint64_t)y < (SIZE_MAX / (channels * typesize)))) {
    return NULL;
  }

  size_t size = (size_t)x * (size_t)y * (size_t)channels * typesize;
  return MEM_mapallocN(size, name);
}

bool imb_addrectfloatImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return false;
  }

  if (ibuf->rect_float) {
    imb_freerectfloatImBuf(ibuf); /* frees mipmap too, hrm */
  }

  ibuf->channels = 4;
  if ((ibuf->rect_float = imb_alloc_pixels(ibuf->x, ibuf->y, 4, sizeof(float), __func__))) {
    ibuf->mall |= IB_rectfloat;
    ibuf->flags |= IB_rectfloat;
    return true;
  }

  return false;
}

/* question; why also add zbuf? */
bool imb_addrectImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return false;
  }

  /* Don't call imb_freerectImBuf, it frees mipmaps,
   * this call is used only too give float buffers display. */
  if (ibuf->rect && (ibuf->mall & IB_rect)) {
    MEM_freeN(ibuf->rect);
  }
  ibuf->rect = NULL;

  if ((ibuf->rect = imb_alloc_pixels(ibuf->x, ibuf->y, 4, sizeof(unsigned char), __func__))) {
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

struct ImBuf *IMB_allocFromBuffer(const unsigned int *rect,
                                  const float *rectf,
                                  unsigned int w,
                                  unsigned int h)
{
  ImBuf *ibuf = NULL;

  if (!(rect || rectf)) {
    return NULL;
  }

  ibuf = IMB_allocImBuf(w, h, 32, 0);

  if (rectf) {
    ibuf->rect_float = MEM_dupallocN(rectf);
    ibuf->flags |= IB_rectfloat;
    ibuf->mall |= IB_rectfloat;
  }
  if (rect) {
    ibuf->rect = MEM_dupallocN(rect);
    ibuf->flags |= IB_rect;
    ibuf->mall |= IB_rect;
  }

  return ibuf;
}

bool imb_addtilesImBuf(ImBuf *ibuf)
{
  if (ibuf == NULL) {
    return false;
  }

  if (!ibuf->tiles) {
    if ((ibuf->tiles = MEM_callocN(sizeof(unsigned int *) * ibuf->xtiles * ibuf->ytiles,
                                   "imb_tiles"))) {
      ibuf->mall |= IB_tiles;
    }
  }

  return (ibuf->tiles != NULL);
}

ImBuf *IMB_allocImBuf(unsigned int x, unsigned int y, uchar planes, unsigned int flags)
{
  ImBuf *ibuf;

  ibuf = MEM_mallocN(sizeof(ImBuf), "ImBuf_struct");

  if (ibuf) {
    if (!IMB_initImBuf(ibuf, x, y, planes, flags)) {
      IMB_freeImBuf(ibuf);
      return NULL;
    }
  }

  return (ibuf);
}

bool IMB_initImBuf(
    struct ImBuf *ibuf, unsigned int x, unsigned int y, unsigned char planes, unsigned int flags)
{
  memset(ibuf, 0, sizeof(ImBuf));

  ibuf->x = x;
  ibuf->y = y;
  ibuf->planes = planes;
  ibuf->ftype = IMB_FTYPE_PNG;
  ibuf->foptions.quality =
      15;             /* the 15 means, set compression to low ratio but not time consuming */
  ibuf->channels = 4; /* float option, is set to other values when buffers get assigned */
  ibuf->ppm[0] = ibuf->ppm[1] = IMB_DPI_DEFAULT /
                                0.0254f; /* IMB_DPI_DEFAULT -> pixels-per-meter */

  if (flags & IB_rect) {
    if (imb_addrectImBuf(ibuf) == false) {
      return false;
    }
  }

  if (flags & IB_rectfloat) {
    if (imb_addrectfloatImBuf(ibuf) == false) {
      return false;
    }
  }

  if (flags & IB_zbuf) {
    if (addzbufImBuf(ibuf) == false) {
      return false;
    }
  }

  if (flags & IB_zbuffloat) {
    if (addzbuffloatImBuf(ibuf) == false) {
      return false;
    }
  }

  /* assign default spaces */
  colormanage_imbuf_set_default_spaces(ibuf);

  return true;
}

/* does no zbuffers? */
ImBuf *IMB_dupImBuf(const ImBuf *ibuf1)
{
  ImBuf *ibuf2, tbuf;
  int flags = 0;
  int a, x, y;

  if (ibuf1 == NULL) {
    return NULL;
  }

  if (ibuf1->rect) {
    flags |= IB_rect;
  }
  if (ibuf1->rect_float) {
    flags |= IB_rectfloat;
  }
  if (ibuf1->zbuf) {
    flags |= IB_zbuf;
  }
  if (ibuf1->zbuf_float) {
    flags |= IB_zbuffloat;
  }

  x = ibuf1->x;
  y = ibuf1->y;

  ibuf2 = IMB_allocImBuf(x, y, ibuf1->planes, flags);
  if (ibuf2 == NULL) {
    return NULL;
  }

  if (flags & IB_rect) {
    memcpy(ibuf2->rect, ibuf1->rect, ((size_t)x) * y * sizeof(int));
  }

  if (flags & IB_rectfloat) {
    memcpy(
        ibuf2->rect_float, ibuf1->rect_float, ((size_t)ibuf1->channels) * x * y * sizeof(float));
  }

  if (flags & IB_zbuf) {
    memcpy(ibuf2->zbuf, ibuf1->zbuf, ((size_t)x) * y * sizeof(int));
  }

  if (flags & IB_zbuffloat) {
    memcpy(ibuf2->zbuf_float, ibuf1->zbuf_float, ((size_t)x) * y * sizeof(float));
  }

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
  tbuf.rect = ibuf2->rect;
  tbuf.rect_float = ibuf2->rect_float;
  tbuf.encodedbuffer = ibuf2->encodedbuffer;
  tbuf.zbuf = ibuf2->zbuf;
  tbuf.zbuf_float = ibuf2->zbuf_float;
  for (a = 0; a < IMB_MIPMAP_LEVELS; a++) {
    tbuf.mipmap[a] = NULL;
  }
  tbuf.dds_data.data = NULL;

  /* set malloc flag */
  tbuf.mall = ibuf2->mall;
  tbuf.c_handle = NULL;
  tbuf.refcounter = 0;

  /* for now don't duplicate metadata */
  tbuf.metadata = NULL;

  tbuf.display_buffer_flags = NULL;
  tbuf.colormanage_cache = NULL;

  *ibuf2 = tbuf;

  return (ibuf2);
}

size_t IMB_get_size_in_memory(ImBuf *ibuf)
{
  int a;
  size_t size = 0, channel_size = 0;

  size += sizeof(ImBuf);

  if (ibuf->rect)
    channel_size += sizeof(char);

  if (ibuf->rect_float)
    channel_size += sizeof(float);

  size += channel_size * ibuf->x * ibuf->y * ibuf->channels;

  if (ibuf->miptot) {
    for (a = 0; a < ibuf->miptot; a++) {
      if (ibuf->mipmap[a])
        size += IMB_get_size_in_memory(ibuf->mipmap[a]);
    }
  }

  if (ibuf->tiles) {
    size += sizeof(unsigned int) * ibuf->ytiles * ibuf->xtiles;
  }

  return size;
}

#if 0 /* remove? - campbell */
/* support for cache limiting */

static void imbuf_cache_destructor(void *data)
{
  ImBuf *ibuf = (ImBuf *)data;

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

  if (!c) {
    c = new_MEM_CacheLimiter(imbuf_cache_destructor, NULL);
  }

  return &c;
}
#endif
