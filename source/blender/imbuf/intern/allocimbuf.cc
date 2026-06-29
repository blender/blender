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

#include "IMB_colormanagement_intern.hh"
#include "IMB_metadata.hh"

#include "imbuf.hh"

#include "MEM_guardedalloc.h"

#include "GPU_context.hh"
#include "GPU_state.hh"
#include "GPU_texture.hh"

#include "OCIO_colorspace.hh"

#include "CLG_log.h"

#include "atomic_ops.h"

namespace blender {

static CLG_LogRef LOG = {"image.buffer"};

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
  void *data = imb_alloc_pixels(x, y, channels, type_size, initialize_pixels, __func__);
  if (!data) {
    return false;
  }
  buffer.data = static_cast<decltype(BufferType::data)>(data);
  buffer.sharing_info = ImplicitSharingPtr<>(implicit_sharing::info_for_mem_free(data));
  return true;
}

uint8_t *ImBuf::byte_data_for_write()
{
  if (!this->byte_buffer.data) {
    return nullptr;
  }
  if (this->byte_buffer.sharing_info->is_mutable()) {
    this->byte_buffer.sharing_info->tag_ensured_mutable();
  }
  else {
    const size_t size = size_t(this->x) * size_t(this->y) * 4;
    uint8_t *new_data = MEM_new_array_uninitialized<uint8_t>(size, __func__);
    std::copy_n(this->byte_buffer.data, size, new_data);
    this->byte_buffer.data = new_data;
    this->byte_buffer.sharing_info = ImplicitSharingPtr<>(
        implicit_sharing::info_for_mem_free(new_data));
  }
  return const_cast<uint8_t *>(this->byte_buffer.data);
}

float *ImBuf::float_data_for_write()
{
  if (!this->float_buffer.data) {
    return nullptr;
  }
  if (this->float_buffer.sharing_info->is_mutable()) {
    this->float_buffer.sharing_info->tag_ensured_mutable();
  }
  else {
    const size_t size = size_t(this->x) * size_t(this->y) * this->channels;
    float *new_data = MEM_new_array_uninitialized<float>(size, __func__);
    std::copy_n(this->float_buffer.data, size, new_data);
    this->float_buffer.data = new_data;
    this->float_buffer.sharing_info = ImplicitSharingPtr<>(
        implicit_sharing::info_for_mem_free(new_data));
  }
  return const_cast<float *>(this->float_buffer.data);
}

void IMB_free_float_pixels(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }
  ibuf->float_buffer = {};
}

void IMB_free_byte_pixels(ImBuf *ibuf)
{
  if (ibuf == nullptr) {
    return;
  }
  ibuf->byte_buffer = {};
}

void IMB_free_all_data(ImBuf *ibuf)
{
  IMB_free_byte_pixels(ibuf);
  IMB_free_float_pixels(ibuf);
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
    MEM_delete(ibuf);
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

  IMB_freeImBuf(ibuf);

  return rval;
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
  return initialize_pixels ? MEM_new_zeroed(size, alloc_name) :
                             MEM_new_uninitialized(size, alloc_name);
}

bool IMB_alloc_float_pixels(ImBuf *ibuf, const uint channels, bool initialize_pixels)
{
  if (ibuf == nullptr) {
    return false;
  }

  if (ibuf->float_data()) {
    IMB_free_float_pixels(ibuf);
  }

  if (!imb_alloc_buffer(
          ibuf->float_buffer, ibuf->x, ibuf->y, channels, sizeof(float), initialize_pixels))
  {
    return false;
  }

  ibuf->channels = channels;

  return true;
}

bool IMB_alloc_byte_pixels(ImBuf *ibuf, bool initialize_pixels)
{
  /* Question; why also add ZBUF (when `planes > 32`)? */

  if (ibuf == nullptr) {
    return false;
  }

  ibuf->byte_buffer = {};

  if (!imb_alloc_buffer(
          ibuf->byte_buffer, ibuf->x, ibuf->y, 4, sizeof(uint8_t), initialize_pixels))
  {
    return false;
  }

  return true;
}

void ImBuf::assign_byte_data(uint8_t *data)
{
  this->byte_buffer = {};
  if (data) {
    this->byte_buffer.data = data;
    this->byte_buffer.sharing_info = ImplicitSharingPtr<>(
        implicit_sharing::info_for_mem_free(data));
  }
}

void ImBuf::assign_float_data(float *data)
{
  this->float_buffer = {};
  if (data) {
    this->float_buffer.data = data;
    this->float_buffer.sharing_info = ImplicitSharingPtr<>(
        implicit_sharing::info_for_mem_free(data));
  }
}

bool ImBuf::colorspace_is_data() const
{
  if (this->float_buffer.data) {
    return this->float_buffer.colorspace && this->float_buffer.colorspace->is_data();
  }
  return this->byte_buffer.colorspace && this->byte_buffer.colorspace->is_data();
}

void ImBuf::assign_byte_data(const uint8_t *data, ImplicitSharingPtr<> sharing_ptr)
{
  BLI_assert(data != nullptr);
  BLI_assert(sharing_ptr.get() != nullptr);
  this->byte_buffer.data = data;
  this->byte_buffer.sharing_info = std::move(sharing_ptr);
}

void ImBuf::assign_float_data(const float *data, ImplicitSharingPtr<> sharing_ptr)
{
  BLI_assert(data != nullptr);
  BLI_assert(sharing_ptr.get() != nullptr);
  this->float_buffer.data = data;
  this->float_buffer.sharing_info = std::move(sharing_ptr);
}

void IMB_ensure_host_buffer(ImBuf *ibuf)
{
  if (!ibuf || !ibuf->gpu.texture) {
    return;
  }

  /* The host buffers are already up-to-date. */
  if (!(ibuf->userflags & IB_HOST_BUFFER_INVALID)) {
    return;
  }
  ibuf->userflags &= ~IB_HOST_BUFFER_INVALID;

  const bool need_secondary_context = !GPU_context_active_get();
  if (need_secondary_context) {
    IMB_activate_gpu_context();
  }

  GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
  float *output_buffer = static_cast<float *>(
      GPU_texture_read(ibuf->gpu.texture, GPU_DATA_FLOAT, 0));
  const ColorSpace *float_colorspace = ibuf->float_buffer.colorspace;
  ibuf->assign_float_data(output_buffer);
  ibuf->float_buffer.colorspace = float_colorspace;

  if (need_secondary_context) {
    IMB_deactivate_gpu_context();
  }
}

ImBuf *IMB_allocFromBufferOwn(
    uint8_t *byte_buffer, float *float_buffer, uint w, uint h, uint channels)
{
  if (!(byte_buffer || float_buffer)) {
    return nullptr;
  }

  ImBuf *ibuf = IMB_allocImBuf(w, h, ImBufFlags::Zero);

  ibuf->channels = channels;

  if (float_buffer) {
    /* TODO(sergey): The 4 channels is the historical code. Should probably be `channels`, but
     * needs a dedicated investigation. */
    BLI_assert(MEM_allocN_len(float_buffer) == sizeof(float[4]) * w * h);
    ibuf->assign_float_data(float_buffer);
  }

  if (byte_buffer) {
    BLI_assert(MEM_allocN_len(byte_buffer) == sizeof(uint8_t[4]) * w * h);
    ibuf->assign_byte_data(byte_buffer);
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

  ibuf = IMB_allocImBuf(w, h, ImBufFlags::Zero);

  ibuf->channels = channels;

  /* NOTE: Avoid #MEM_dupalloc since the buffers might not be allocated using guarded-allocation.
   */
  if (float_buffer) {
    /* TODO(sergey): The 4 channels is the historical code. Should probably be `channels`, but
     * needs a dedicated investigation. */
    imb_alloc_buffer(ibuf->float_buffer, w, h, 4, sizeof(float), false);

    memcpy(ibuf->float_data_for_write(), float_buffer, sizeof(float[4]) * w * h);
  }

  if (byte_buffer) {
    imb_alloc_buffer(ibuf->byte_buffer, w, h, 4, sizeof(uint8_t), false);

    memcpy(ibuf->byte_data_for_write(), byte_buffer, sizeof(uint8_t[4]) * w * h);
  }

  return ibuf;
}

ImBuf *IMB_allocImBuf(uint x, uint y, ImBufFlags flags)
{
  ImBuf *ibuf = MEM_new<ImBuf>("ImBuf_struct");

  if (ibuf) {
    if (!IMB_initImBuf(ibuf, x, y, flags)) {
      IMB_freeImBuf(ibuf);
      return nullptr;
    }
  }

  return ibuf;
}

bool IMB_initImBuf(ImBuf *ibuf, uint x, uint y, ImBufFlags flags)
{
  ibuf->~ImBuf();
  new (ibuf) ImBuf();

  ibuf->x = x;
  ibuf->y = y;
  ibuf->color_mode = ImColorMode::RGBA;
  ibuf->ftype = IMB_FTYPE_PNG;
  /* float option, is set to other values when buffers get assigned. */
  ibuf->channels = 4;
  /* IMB_DPI_DEFAULT -> pixels-per-meter. */
  ibuf->ppm[0] = ibuf->ppm[1] = IMB_DPI_DEFAULT / 0.0254;

  const bool init_pixels = !flag_is_set(flags, ImBufFlags::UninitializedPixels);

  if (flag_is_set(flags, ImBufFlags::ByteData)) {
    if (!IMB_alloc_byte_pixels(ibuf, init_pixels)) {
      return false;
    }
  }

  if (flag_is_set(flags, ImBufFlags::FloatData)) {
    if (!IMB_alloc_float_pixels(ibuf, ibuf->channels, init_pixels)) {
      return false;
    }
  }

  /* assign default spaces */
  colormanage_imbuf_set_default_spaces(ibuf);

  return true;
}

ImBuf *IMB_dupImBuf(const ImBuf *ibuf1)
{
  if (ibuf1 == nullptr) {
    return nullptr;
  }

  ImBuf *ibuf2 = IMB_allocImBuf(ibuf1->x, ibuf1->y, ImBufFlags::Zero);
  if (ibuf2 == nullptr) {
    return nullptr;
  }
  ibuf2->x = ibuf1->x;
  ibuf2->y = ibuf1->y;
  ibuf2->display_size[0] = ibuf1->display_size[0];
  ibuf2->display_size[1] = ibuf1->display_size[1];
  ibuf2->data_offset[0] = ibuf1->data_offset[0];
  ibuf2->data_offset[1] = ibuf1->data_offset[1];
  ibuf2->display_offset[0] = ibuf1->display_offset[0];
  ibuf2->display_offset[1] = ibuf1->display_offset[1];
  ibuf2->color_mode = ibuf1->color_mode;
  ibuf2->channels = ibuf1->channels;
  ibuf2->flags = ibuf1->flags;
  ibuf2->byte_buffer = ibuf1->byte_buffer;
  ibuf2->float_buffer = ibuf1->float_buffer;
  /* GPU textures can not be easily copied, as it is not guaranteed that this function is called
   * from within an active GPU context. */
  ibuf2->gpu.texture = nullptr;
  ibuf2->ppm[0] = ibuf1->ppm[0];
  ibuf2->ppm[1] = ibuf1->ppm[1];
  ibuf2->dither = ibuf1->dither;
  ibuf2->userflags = ibuf1->userflags;
  ibuf2->userflags = ibuf1->userflags;
  ibuf2->metadata_ptr = ibuf1->metadata_ptr;
  ibuf2->metadata_sharing_info = ibuf1->metadata_sharing_info;
  ibuf2->ftype = ibuf1->ftype;
  ibuf2->foptions = ibuf1->foptions;
  ibuf2->filepath = ibuf1->filepath;
  ibuf2->fileframe = ibuf1->fileframe;
  ibuf2->refcounter = 0;

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

  if (ibuf->byte_data()) {
    channel_size += sizeof(char);
  }

  if (ibuf->float_data()) {
    channel_size += sizeof(float);
  }

  size += channel_size * IMB_get_pixel_count(ibuf) * size_t(ibuf->channels);

  return size;
}

ImColorMode IMB_color_mode_from_channels(const int channels)
{
  switch (channels) {
    case 1:
      return ImColorMode::BW;
    case 2:
      return ImColorMode::BW_A;
    case 3:
      return ImColorMode::RGB;
    case 4:
      return ImColorMode::RGBA;
  }
  return ImColorMode::RGBA;
}

bool IMB_chan_id_is_color(const StringRef chan_id)
{
  return chan_id == "RGB" || chan_id == "RGBA" || chan_id == "RA" || chan_id == "BA" ||
         chan_id == "GA" || chan_id == "R" || chan_id == "G" || chan_id == "B" || chan_id == "A";
}

}  // namespace blender
