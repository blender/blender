/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

/* It's become a bit messy... Basically, only the IMB_ prefixed files
 * should remain. */

#include <algorithm>
#include <cstddef>

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "IMB_allocimbuf.hh"
#include "IMB_colormanagement_intern.hh"
#include "IMB_filetype.hh"
#include "IMB_metadata.hh"

#include "imbuf.hh"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"

#include "GPU_texture.hh"

#include "CLG_log.h"

#include "atomic_ops.h"

static CLG_LogRef LOG = {"image.buffer"};

/* Free the specified buffer storage, freeing memory when needed and restoring the state of the
 * buffer to its defaults. */
template<class BufferType> static void imb_free_buffer(BufferType &buffer)
{
  if (buffer.data) {
    switch (buffer.ownership) {
      case IB_DO_NOT_TAKE_OWNERSHIP:
        break;

      case IB_TAKE_OWNERSHIP:
        MEM_freeN(buffer.data);
        break;
    }
  }

  /* Reset buffer to defaults. */
  buffer.data = nullptr;
  buffer.ownership = IB_DO_NOT_TAKE_OWNERSHIP;
}

/* Free the specified DDS buffer storage, freeing memory when needed and restoring the state of the
 * buffer to its defaults. */
static void imb_free_dds_buffer(DDSData &dds_data)
{
  if (dds_data.data) {
    switch (dds_data.ownership) {
      case IB_DO_NOT_TAKE_OWNERSHIP:
        break;

      case IB_TAKE_OWNERSHIP:
        /* dds_data.data is allocated by DirectDrawSurface::readData(), so don't use MEM_freeN! */
        free(dds_data.data);
        break;
    }
  }

  /* Reset buffer to defaults. */
  dds_data.data = nullptr;
  dds_data.ownership = IB_DO_NOT_TAKE_OWNERSHIP;
}

/* Allocate pixel storage of the given buffer. The buffer owns the allocated memory.
 * Returns true of allocation succeeded, false otherwise. */
template<class BufferType>
bool imb_alloc_buffer(BufferType &buffer,
                      const uint x,
                      const uint y,
                      const uint channels,
                      const size_t type_size,
                      bool initialize_pixels)
{
  buffer.data = static_cast<decltype(BufferType::data)>(
      imb_alloc_pixels(x, y, channels, type_size, initialize_pixels, __func__));
  if (!buffer.data) {
    return false;
  }

  buffer.ownership = IB_TAKE_OWNERSHIP;

  return true;
}

/* Make the buffer available for modification.
 * Is achieved by ensuring that the buffer is the only owner of its data. */
template<class BufferType> void imb_make_writeable_buffer(BufferType &buffer)
{
  if (!buffer.data) {
    return;
  }

  switch (buffer.ownership) {
    case IB_DO_NOT_TAKE_OWNERSHIP:
      buffer.data = static_cast<decltype(BufferType::data)>(MEM_dupallocN(buffer.data));
      buffer.ownership = IB_TAKE_OWNERSHIP;

    case IB_TAKE_OWNERSHIP:
      break;
  }
}

template<class BufferType>
auto imb_steal_buffer_data(BufferType &buffer) -> decltype(BufferType::data)
{
  if (!buffer.data) {
    return nullptr;
  }

  switch (buffer.ownership) {
    case IB_DO_NOT_TAKE_OWNERSHIP:
      BLI_assert_msg(false, "Unexpected behavior: stealing non-owned data pointer");
      return nullptr;

    case IB_TAKE_OWNERSHIP: {
      decltype(BufferType::data) data = buffer.data;

      buffer.data = nullptr;
      buffer.ownership = IB_DO_NOT_TAKE_OWNERSHIP;

      return data;
    }
  }

  BLI_assert_unreachable();

  return nullptr;
}

void IMB_free_float_pixels(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }
  imb_free_buffer(ibuf->float_buffer);
  ibuf->flags &= ~IB_float_data;
}

void IMB_free_byte_pixels(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }
  imb_free_buffer(ibuf->byte_buffer);
  ibuf->flags &= ~IB_byte_data;
}

static void free_encoded_data(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }

  imb_free_buffer(ibuf->encoded_buffer);

  ibuf->encoded_buffer_size = 0;
  ibuf->encoded_size = 0;

  ibuf->flags &= ~IB_mem;
}

void IMB_free_all_data(ImBuf *ibuf)
{
  IMB_free_byte_pixels(ibuf);
  IMB_free_float_pixels(ibuf);
  free_encoded_data(ibuf);
}

void IMB_free_gpu_textures(ImBuf *ibuf)
{
  if (!ibuf || !ibuf->gpu.texture) {
    return;
  }

  GPU_texture_free(ibuf->gpu.texture);
  ibuf->gpu.texture = nullptr;
}

void IMB_freeImBuf(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }

  bool needs_free = atomic_sub_and_fetch_int32(&ibuf->refcounter, 1) < 0;
  if (needs_free) {
    /* Include this check here as the path may be manipulated after creation. */
    BLI_assert_msg(!(ibuf->filepath[0] == '/' && ibuf->filepath[1] == '/'),
                   "'.blend' relative \"//\" must not be used in ImBuf!");

    IMB_free_all_data(ibuf);
    IMB_free_gpu_textures(ibuf);
    IMB_metadata_free(ibuf->metadata);
    colormanage_cache_free(ibuf);
    imb_free_dds_buffer(ibuf->dds_data);
    MEM_freeN(ibuf);
  }
}

void IMB_refImBuf(ImBuf *ibuf)
{
  atomic_add_and_fetch_int32(&ibuf->refcounter, 1);
}

ImBuf *IMB_makeSingleUser(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return nullptr;
  }

  const bool is_single = (atomic_load_int32(&ibuf->refcounter) == 0);
  if (is_single) {
    return ibuf;
  }

  ImBuf *rval = IMB_dupImBuf(ibuf);

  IMB_metadata_copy(rval, ibuf);

  IMB_freeImBuf(ibuf);

  return rval;
}

bool imb_addencodedbufferImBuf(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return false;
  }

  free_encoded_data(ibuf);

  if (ibuf->encoded_buffer_size == 0) {
    ibuf->encoded_buffer_size = 10000;
  }

  ibuf->encoded_size = 0;

  if (!imb_alloc_buffer(
          ibuf->encoded_buffer, ibuf->encoded_buffer_size, 1, 1, sizeof(uint8_t), true))
  {
    return false;
  }

  ibuf->flags |= IB_mem;

  return true;
}

bool imb_enlargeencodedbufferImBuf(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return false;
  }

  if (ibuf->encoded_buffer_size < ibuf->encoded_size) {
    CLOG_ERROR(&LOG, "%s: error in parameters\n", __func__);
    return false;
  }

  uint newsize = 2 * ibuf->encoded_buffer_size;
  newsize = std::max<uint>(newsize, 10000);

  ImBufByteBuffer new_buffer;
  if (!imb_alloc_buffer(new_buffer, newsize, 1, 1, sizeof(uint8_t), true)) {
    return false;
  }

  if (ibuf->encoded_buffer.data) {
    memcpy(new_buffer.data, ibuf->encoded_buffer.data, ibuf->encoded_size);
  }
  else {
    ibuf->encoded_size = 0;
  }

  imb_free_buffer(ibuf->encoded_buffer);

  ibuf->encoded_buffer = new_buffer;
  ibuf->encoded_buffer_size = newsize;
  ibuf->flags |= IB_mem;

  return true;
}

void *imb_alloc_pixels(
    uint x, uint y, uint channels, size_t typesize, bool initialize_pixels, const char *alloc_name)
{
  /* Protect against buffer overflow vulnerabilities from files specifying
   * a width and height that overflow and alloc too little memory. */
  if (!(uint64_t(x) * uint64_t(y) < (SIZE_MAX / (channels * typesize)))) {
    return nullptr;
  }

  size_t size = size_t(x) * size_t(y) * size_t(channels) * typesize;
  return initialize_pixels ? MEM_callocN(size, alloc_name) : MEM_mallocN(size, alloc_name);
}

bool IMB_alloc_float_pixels(ImBuf *ibuf, const uint channels, bool initialize_pixels)
{
  if (ibuf == nullptr) {
    return false;
  }

  if (ibuf->float_buffer.data) {
    IMB_free_float_pixels(ibuf);
  }

  if (!imb_alloc_buffer(
          ibuf->float_buffer, ibuf->x, ibuf->y, channels, sizeof(float), initialize_pixels))
  {
    return false;
  }

  ibuf->channels = channels;
  ibuf->flags |= IB_float_data;

  return true;
}

bool IMB_alloc_byte_pixels(ImBuf *ibuf, bool initialize_pixels)
{
  /* Question; why also add ZBUF (when `planes > 32`)? */

  if (ibuf == nullptr) {
    return false;
  }

  imb_free_buffer(ibuf->byte_buffer);

  if (!imb_alloc_buffer(
          ibuf->byte_buffer, ibuf->x, ibuf->y, 4, sizeof(uint8_t), initialize_pixels))
  {
    return false;
  }

  ibuf->flags |= IB_byte_data;

  return true;
}

uint8_t *IMB_steal_byte_buffer(ImBuf *ibuf)
{
  uint8_t *data = imb_steal_buffer_data(ibuf->byte_buffer);
  ibuf->flags &= ~IB_byte_data;
  return data;
}

float *IMB_steal_float_buffer(ImBuf *ibuf)
{
  float *data = imb_steal_buffer_data(ibuf->float_buffer);
  ibuf->flags &= ~IB_float_data;
  return data;
}

uint8_t *IMB_steal_encoded_buffer(ImBuf *ibuf)
{
  uint8_t *data = imb_steal_buffer_data(ibuf->encoded_buffer);

  ibuf->encoded_size = 0;
  ibuf->encoded_buffer_size = 0;

  ibuf->flags &= ~IB_mem;

  return data;
}

void IMB_make_writable_byte_buffer(ImBuf *ibuf)
{
  imb_make_writeable_buffer(ibuf->byte_buffer);
}

void IMB_make_writable_float_buffer(ImBuf *ibuf)
{
  imb_make_writeable_buffer(ibuf->float_buffer);
}

void IMB_assign_byte_buffer(ImBuf *ibuf, uint8_t *buffer_data, const ImBufOwnership ownership)
{
  imb_free_buffer(ibuf->byte_buffer);
  ibuf->flags &= ~IB_byte_data;

  if (buffer_data) {
    ibuf->byte_buffer.data = buffer_data;
    ibuf->byte_buffer.ownership = ownership;

    ibuf->flags |= IB_byte_data;
  }
}

void IMB_assign_float_buffer(ImBuf *ibuf, float *buffer_data, const ImBufOwnership ownership)
{
  imb_free_buffer(ibuf->float_buffer);
  ibuf->flags &= ~IB_float_data;

  if (buffer_data) {
    ibuf->float_buffer.data = buffer_data;
    ibuf->float_buffer.ownership = ownership;

    ibuf->flags |= IB_float_data;
  }
}

void IMB_assign_byte_buffer(ImBuf *ibuf,
                            const ImBufByteBuffer &buffer,
                            const ImBufOwnership ownership)
{
  IMB_assign_byte_buffer(ibuf, buffer.data, ownership);
  ibuf->byte_buffer.colorspace = buffer.colorspace;
}

void IMB_assign_float_buffer(ImBuf *ibuf,
                             const ImBufFloatBuffer &buffer,
                             const ImBufOwnership ownership)
{
  IMB_assign_float_buffer(ibuf, buffer.data, ownership);
  ibuf->float_buffer.colorspace = buffer.colorspace;
}

void IMB_assign_dds_data(ImBuf *ibuf, const DDSData &data, const ImBufOwnership ownership)
{
  BLI_assert(ibuf->ftype == IMB_FTYPE_DDS);

  imb_free_dds_buffer(ibuf->dds_data);

  ibuf->dds_data = data;
  ibuf->dds_data.ownership = ownership;
}

ImBuf *IMB_allocFromBufferOwn(
    uint8_t *byte_buffer, float *float_buffer, uint w, uint h, uint channels)
{
  if (!(byte_buffer || float_buffer)) {
    return nullptr;
  }

  ImBuf *ibuf = IMB_allocImBuf(w, h, 32, 0);

  ibuf->channels = channels;

  if (float_buffer) {
    /* TODO(sergey): The 4 channels is the historical code. Should probably be `channels`, but
     * needs a dedicated investigation. */
    BLI_assert(MEM_allocN_len(float_buffer) == sizeof(float[4]) * w * h);
    IMB_assign_float_buffer(ibuf, float_buffer, IB_TAKE_OWNERSHIP);
  }

  if (byte_buffer) {
    BLI_assert(MEM_allocN_len(byte_buffer) == sizeof(uint8_t[4]) * w * h);
    IMB_assign_byte_buffer(ibuf, byte_buffer, IB_TAKE_OWNERSHIP);
  }

  return ibuf;
}

ImBuf *IMB_allocFromBuffer(
    const uint8_t *byte_buffer, const float *float_buffer, uint w, uint h, uint channels)
{
  ImBuf *ibuf = nullptr;

  if (!(byte_buffer || float_buffer)) {
    return nullptr;
  }

  ibuf = IMB_allocImBuf(w, h, 32, 0);

  ibuf->channels = channels;

  /* NOTE: Avoid #MEM_dupallocN since the buffers might not be allocated using guarded-allocation.
   */
  if (float_buffer) {
    /* TODO(sergey): The 4 channels is the historical code. Should probably be `channels`, but
     * needs a dedicated investigation. */
    imb_alloc_buffer(ibuf->float_buffer, w, h, 4, sizeof(float), false);

    memcpy(ibuf->float_buffer.data, float_buffer, sizeof(float[4]) * w * h);
  }

  if (byte_buffer) {
    imb_alloc_buffer(ibuf->byte_buffer, w, h, 4, sizeof(uint8_t), false);

    memcpy(ibuf->byte_buffer.data, byte_buffer, sizeof(uint8_t[4]) * w * h);
  }

  return ibuf;
}

ImBuf *IMB_allocImBuf(uint x, uint y, uchar planes, uint flags)
{
  ImBuf *ibuf = MEM_callocN<ImBuf>("ImBuf_struct");

  if (ibuf) {
    if (!IMB_initImBuf(ibuf, x, y, planes, flags)) {
      IMB_freeImBuf(ibuf);
      return nullptr;
    }
  }

  return ibuf;
}

bool IMB_initImBuf(ImBuf *ibuf, uint x, uint y, uchar planes, uint flags)
{
  *ibuf = ImBuf{};

  ibuf->x = x;
  ibuf->y = y;
  ibuf->planes = planes;
  ibuf->ftype = IMB_FTYPE_PNG;
  /* The '15' means, set compression to low ratio but not time consuming. */
  ibuf->foptions.quality = 15;
  /* float option, is set to other values when buffers get assigned. */
  ibuf->channels = 4;
  /* IMB_DPI_DEFAULT -> pixels-per-meter. */
  ibuf->ppm[0] = ibuf->ppm[1] = IMB_DPI_DEFAULT / 0.0254;

  const bool init_pixels = (flags & IB_uninitialized_pixels) == 0;

  if (flags & IB_byte_data) {
    if (IMB_alloc_byte_pixels(ibuf, init_pixels) == false) {
      return false;
    }
  }

  if (flags & IB_float_data) {
    if (IMB_alloc_float_pixels(ibuf, ibuf->channels, init_pixels) == false) {
      return false;
    }
  }

  /* assign default spaces */
  colormanage_imbuf_set_default_spaces(ibuf);

  return true;
}

ImBuf *IMB_dupImBuf(const ImBuf *ibuf1)
{
  ImBuf *ibuf2, tbuf;
  int flags = IB_uninitialized_pixels;
  int x, y;

  if (ibuf1 == nullptr) {
    return nullptr;
  }

  if (ibuf1->byte_buffer.data) {
    flags |= IB_byte_data;
  }

  x = ibuf1->x;
  y = ibuf1->y;

  ibuf2 = IMB_allocImBuf(x, y, ibuf1->planes, flags);
  if (ibuf2 == nullptr) {
    return nullptr;
  }

  if (flags & IB_byte_data) {
    memcpy(ibuf2->byte_buffer.data, ibuf1->byte_buffer.data, size_t(x) * y * 4 * sizeof(uint8_t));
  }

  if (ibuf1->float_buffer.data) {
    /* Ensure the correct number of channels are being allocated for the new #ImBuf. Some
     * compositing scenarios might end up with >4 channels and we want to duplicate them properly.
     */
    if (IMB_alloc_float_pixels(ibuf2, ibuf1->channels, false) == false) {
      IMB_freeImBuf(ibuf2);
      return nullptr;
    }

    memcpy(ibuf2->float_buffer.data,
           ibuf1->float_buffer.data,
           size_t(ibuf2->channels) * x * y * sizeof(float));
  }

  if (ibuf1->encoded_buffer.data) {
    ibuf2->encoded_buffer_size = ibuf1->encoded_buffer_size;
    if (imb_addencodedbufferImBuf(ibuf2) == false) {
      IMB_freeImBuf(ibuf2);
      return nullptr;
    }

    memcpy(ibuf2->encoded_buffer.data, ibuf1->encoded_buffer.data, ibuf1->encoded_size);
  }

  ibuf2->byte_buffer.colorspace = ibuf1->byte_buffer.colorspace;
  ibuf2->float_buffer.colorspace = ibuf1->float_buffer.colorspace;

  /* silly trick to copy the entire contents of ibuf1 struct over to ibuf */
  tbuf = *ibuf1;

  /* fix pointers */
  tbuf.byte_buffer = ibuf2->byte_buffer;
  tbuf.float_buffer = ibuf2->float_buffer;
  tbuf.encoded_buffer = ibuf2->encoded_buffer;
  tbuf.dds_data.data = nullptr;

  /* Set `malloc` flag. */
  tbuf.refcounter = 0;

  /* for now don't duplicate metadata */
  tbuf.metadata = nullptr;

  tbuf.display_buffer_flags = nullptr;
  tbuf.colormanage_cache = nullptr;

  /* GPU textures can not be easily copied, as it is not guaranteed that this function is called
   * from within an active GPU context. */
  tbuf.gpu.texture = nullptr;

  *ibuf2 = tbuf;

  return ibuf2;
}

size_t IMB_get_pixel_count(const ImBuf *ibuf)
{
  return size_t(ibuf->x) * size_t(ibuf->y);
}

size_t IMB_get_size_in_memory(const ImBuf *ibuf)
{
  size_t size = 0, channel_size = 0;

  size += sizeof(ImBuf);

  if (ibuf->byte_buffer.data) {
    channel_size += sizeof(char);
  }

  if (ibuf->float_buffer.data) {
    channel_size += sizeof(float);
  }

  size += channel_size * IMB_get_pixel_count(ibuf) * size_t(ibuf->channels);

  return size;
}
